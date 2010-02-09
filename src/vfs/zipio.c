/* MegaZeux
 *
 * Copyright (C) 2009 Alistair Strachan <alistair@devzero.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "zipio.h"

#include <assert.h>
#include <errno.h>

#include "../platform.h"
#include "../util.h"

#define MAX_GLOBAL_COMMENT_LEN  (1 << 16)
#define ECDR_STATIC_LENGTH      (4 + 2 + 2 + 2 + 2 + 4 + 4 + 2)

#define CENTRAL_DIRECTORY_MAGIC 0x02014b50

struct zip_segment
{
  Uint32 offset;
  Uint32 length;
};

struct zip_dirent {
  Uint16 compression_method;
  Uint16 file_name_length;
  Uint16 extra_field_length;
  Uint16 file_comment_length;
  Uint32 crc32;
  Uint32 compressed_size;
  Uint32 uncompressed_size;

  struct zip_segment segment;
  time_t datetime;
  char *file_name;
};

struct zip_handle
{
  struct zip_dirent **entries;
  struct zip_segment cd;
  struct zip_segment ecdr;

  Uint16 num_entries;
  FILE *f;
};

static Sint64 fgetuw(FILE *f)
{
  Uint16 d = 0;
  int i, j;

  for(i = 0, j = 0; i < 2; i++, j += 8)
  {
    int r = fgetc(f);
    if(r < 0)
      return r;
    d |= r << j;
  }

  return d;
}

static Sint64 fgetud(FILE *f)
{
  Uint32 d = 0;
  int i, j;

  for(i = 0, j = 0; i < 4; i++, j += 8)
  {
    int r = fgetc(f);
    if(r < 0)
      return r;
    d |= r << j;
  }

  return d;
}

static bool zgetuw(FILE *f, Sint64 *d, zipio_error_t *err, zipio_error_t code)
{
  *d = fgetuw(f);

  if(*d < 0)
  {
    *err = code;
    return false;
  }

  return true;
}

static bool zgetud(FILE *f, Sint64 *d, zipio_error_t *err, zipio_error_t code)
{
  *d = fgetud(f);

  if(*d < 0)
  {
    *err = code;
    return false;
  }

  return true;
}

static time_t zip_dos_to_time_t(Uint16 time, Uint16 date)
{
  struct tm tm;

  tm.tm_sec  = ((time & 0x1F)   << 1);      // seconds (0-58)
  tm.tm_min  = ((time & 0x7E0)  >> 5);      // minutes (0-59)
  tm.tm_hour = ((time & 0xF800) >> 11);     // hours   (0-23)
  tm.tm_mday = ((date & 0x1F)   >> 0) - 1;  // day     (1-31)
  tm.tm_mon  = ((date & 0x1E0)  >> 5) - 1;  // month   (1-12)
  tm.tm_year = ((date & 0xFE00) >> 9) + 80; // year    (since 1980)

  return mktime(&tm);
}

const char *zipio_strerror(zipio_error_t err)
{
  switch(err)
  {
    case ZIPIO_SUCCESS:
      return "Success";
    case ZIPIO_OPEN_FAILED:
      return "Could not open file";
    case ZIPIO_CLOSE_FAILED:
      return "Could not close file";
    case ZIPIO_TELL_FAILED:
      return "Could not get current file position";
    case ZIPIO_SEEK_FAILED:
      return "Could not seek to requested file position";
    case ZIPIO_READ_FAILED:
      return "Failure reading from file";
    case ZIPIO_NO_CENTRAL_DIRECTORY:
      return "Failed to locate central directory record";
    case ZIPIO_NO_END_CENTRAL_DIRECTORY:
      return "Failed to locate end central directory record";
    case ZIPIO_CORRUPT_CENTRAL_DIRECTORY:
      return "Central directory is corrupt";
    case ZIPIO_CORRUPT_DATETIME:
      return "Date/Time specifier is corrupt";
    case ZIPIO_UNSUPPORTED_FEATURE:
      return "Unsupported extended ZIP feature (encryption, etc.)";
    case ZIPIO_UNSUPPORTED_MULTIPLE_DISKS:
      return "Multiple volume ZIPs are unsupported";
    case ZIPIO_UNSUPPORTED_ZIP64:
      return "No support for ZIP64 entries";
    default:
      return "Unknown error";
  }
}

zipio_error_t zipio_open(const char *filename, zip_handle_t **_z)
{
  static const unsigned char ecdr_sig[] = { 'P', 'K', 0x5, 0x6 };
  long size, start, cur_ecdr_offset = 0;
  zipio_error_t err = ZIPIO_SUCCESS;
  Uint32 zip_comment_length;
  int ecdr_sig_off;
  zip_handle_t *z;
  Sint64 d;
  int i;

  assert(filename != NULL);

  *_z = malloc(sizeof(zip_handle_t));
  z = *_z;

  assert(*_z != NULL);

  // FIXME: Should replace with nested VFS stream calls
  z->f = fopen(filename, "r+b");
  if(!z->f)
  {
    err = ZIPIO_OPEN_FAILED;
    goto err_free;
  }

  size = ftell_and_rewind(z->f);
  if(size < 0)
  {
    err = ZIPIO_TELL_FAILED;
    goto err_close;
  }

  // Variable length trailer limited to MAX_GLOBAL_COMMENT_LEN bytes
  start = MAX(0, size - MAX_GLOBAL_COMMENT_LEN);
  if(fseek(z->f, start, SEEK_SET))
  {
    err = ZIPIO_SEEK_FAILED;
    goto err_close;
  }

  // Find End Central Directory Record (by signature)
  while(true)
  {
    // Because we might have scanned too far back into the file, it's
    // possible we'll encounter ECDRs that are actually nested (stored) ZIP
    // files themselves and which do not reflect this ZIP's ECDR.
    //
    // So, if we locate an ECDR, keep looking for another until we
    // reach the end of the file.

    ecdr_sig_off = 0;
    while(true)
    {
      // May "fail" for unexpected reasons or simply because we got to EOF
      d = fgetc(z->f);
      if(d < 0)
        break;

      // Does the current byte look like our magic (so far?)
      if((unsigned char)d == ecdr_sig[ecdr_sig_off])
      {
        ecdr_sig_off++;
        if(ecdr_sig_off == sizeof(ecdr_sig))
        {
          cur_ecdr_offset = ftell(z->f);
          if(cur_ecdr_offset < 0)
          {
            err = ZIPIO_READ_FAILED;
            goto err_close;
          }
          break;
        }
      }

      // We may want to restart, but if we got another 'P' we've made progress
      else if((unsigned char)d == ecdr_sig[0])
        ecdr_sig_off = 1;

      // Otherwise, no progress has been made; discard all bytes read so far
      else
        ecdr_sig_off = 0;
    }

    // I/O error or we reached end of ZIP
    if(d < 0)
    {
      // Found at least one ECDR
      if(cur_ecdr_offset)
        break;

      // Failed to locate End Central Directory Record
      err = ZIPIO_NO_END_CENTRAL_DIRECTORY;
      goto err_close;
    }
  }

  // Seek back to found location of ECDR
  if(fseek(z->f, cur_ecdr_offset, SEEK_SET))
  {
    err = ZIPIO_SEEK_FAILED;
    goto err_close;
  }

  // "number of this disk"
  if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;

  // We can't support spanned ZIPs yet (ever?)
  if(d > 0)
  {
    err = ZIPIO_UNSUPPORTED_MULTIPLE_DISKS;
    goto err_close;
  }

  // "number of the disk with the start of the central directory"
  if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;

  // We can't support spanned ZIPs yet (ever?)
  if(d > 0)
  {
    err = ZIPIO_UNSUPPORTED_MULTIPLE_DISKS;
    goto err_close;
  }

  // Skip "total number of entries in the central directory on this disk"
  // Can skip because we must be using non-spanning ZIP
  if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;

  // "total number of entries in the central directory"
  if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;
  z->num_entries = (Uint16)d;

  // "size of the central directory"
  if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;
  z->cd.length = (Uint32)d;

  // "offset of start of central directory with respect to
  //  the starting disk number"
  if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;
  z->cd.offset = (Uint32)d;

  // ".ZIP file comment length"
  if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;
  zip_comment_length = (Uint32)d;

  d = ftell(z->f);
  if(d < 0)
  {
    err = ZIPIO_READ_FAILED;
    goto err_close;
  }

  // Can now compute ECDR offset/length reliably
  z->ecdr.offset = d - ECDR_STATIC_LENGTH;
  z->ecdr.length = ECDR_STATIC_LENGTH + zip_comment_length;

  debug("Valid ZIP ECDR found at offset 0x%x; length is 0x%x.\n",
   z->ecdr.offset, z->ecdr.length);

  debug("ZIP CD found at offset 0x%x; length is 0x%x. %u entries.\n",
   z->cd.offset, z->cd.length, z->num_entries);

  // Move to supposed Central Directory
  if(fseek(z->f, z->cd.offset, SEEK_SET))
  {
    err = ZIPIO_SEEK_FAILED;
    goto err_close;
  }

  z->entries = calloc(z->num_entries, sizeof(struct zip_dirent *));

  for(i = 0; i < z->num_entries; i++)
  {
    struct zip_dirent *entry;
    Uint16 time, date;

    // Keep a short hand for this entry, and pass it along
    entry = malloc(sizeof(struct zip_dirent));
    entry->file_name = NULL;
    z->entries[i] = entry;

    // "central file header signature"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // Validate CDR magic
    if((Uint32)d != CENTRAL_DIRECTORY_MAGIC)
    {
      err = ZIPIO_NO_CENTRAL_DIRECTORY;
      goto err_free_entries;
    }

    // Skip "version made by"
    // Can skip because we don't care and will write something different
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // Skip "version needed to extract"
    // Can skip because this field's semantics are regularly ignored by
    // vendors, and it seems most people just write 1.0 here.
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // "general purpose bit flag"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // We don't support Encryption (0), Delayed CRC (3), Method 8 (4),
    // Patched Data (5), Strong Encryption (6), Unused (7-10),
    // Enhanced Compression (12), Central Direction Encryption (13),
    // PKZIP Reserved (14-15)
    if(d & 0xF7F9)
    {
      err = ZIPIO_UNSUPPORTED_FEATURE;
      goto err_free_entries;
    }

    // "compression method"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->compression_method = d;

    // "time"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    time = d;

    // "date"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    date = d;

    // "CRC-32"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->crc32 = d;

    // "compressed size"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->compressed_size = d;

    // FIXME: No ZIP64 support
    if(d == 0xffffffff)
    {
      err = ZIPIO_UNSUPPORTED_ZIP64;
      goto err_free_entries;
    }

    // "uncompressed size"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->uncompressed_size = d;

    // FIXME: No ZIP64 support
    if(d == 0xffffffff)
    {
      err = ZIPIO_UNSUPPORTED_ZIP64;
      goto err_free_entries;
    }

    // "file name length"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->file_name_length = d;

    // "extra field length"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->extra_field_length = d;

    // FIXME: Hack
    if(d > 0)
      warn("Extra field of %ub skipped.\n", entry->extra_field_length);

    // "file comment length"
    if(!zgetuw(z->f, &d, &err, ZIPIO_UNSUPPORTED_MULTIPLE_DISKS))
      goto err_free_entries;
    entry->file_comment_length = d;

    // FIXME: Hack
    if(d > 0)
      warn("File comment of %ub skipped.\n", entry->file_comment_length);

    // "disk number start"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // Cannot support this file starting in any other archive
    if(d != 0)
    {
      err = ZIPIO_UNSUPPORTED_MULTIPLE_DISKS;
      goto err_free_entries;
    }

    // "internal file attributes"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // FIXME: Hack
    if(d & 0x2)
    {
      warn("Support for variable record preamble not implemented.\n");
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // Skip "external file attributes"
    // Can be skipped because this only provides MSDOS attributes (??)
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // "relative offset of local header"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->segment.offset = d;

    // FIXME: No ZIP64 support
    if(d == 0xffffffff)
    {
      err = ZIPIO_UNSUPPORTED_ZIP64;
      goto err_free_entries;
    }

    entry->file_name = malloc(entry->file_name_length + 1);
    entry->file_name[entry->file_name_length] = 0;

    // "file name"
    if(fread(entry->file_name, entry->file_name_length, 1, z->f) != 1)
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // Skip "extra field"
    if(fseek(z->f, entry->extra_field_length, SEEK_CUR))
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // Skip "file comment"
    if(fseek(z->f, entry->file_comment_length, SEEK_CUR))
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // Check we've not exceeded this segment's bounds
    if(ftell(z->f) > z->cd.offset + z->cd.length)
    {
      err = ZIPIO_CORRUPT_CENTRAL_DIRECTORY;
      goto err_free_entries;
    }

    // Convert to standard C89/C99 time_t
    entry->datetime = zip_dos_to_time_t(time, date);
    if(entry->datetime < 0)
    {
      err = ZIPIO_CORRUPT_DATETIME;
      goto err_free_entries;
    }

    debug("Found file '%s' OK.\n", entry->file_name);
    debug("Compression Method:  %u\n", entry->compression_method);
    debug("Time/Date:           %s", ctime(&entry->datetime));
    debug("CRC-32:              0x%x\n", entry->crc32);
    debug("Compressed Size:     %u\n", entry->compressed_size);
    debug("Uncompressed Size:   %u\n", entry->uncompressed_size);
    debug("File Name Length:    %u\n", entry->file_name_length);
    debug("Extra Field Length:  %u\n", entry->extra_field_length);
    debug("File Comment Length: %u\n", entry->file_comment_length);
    debug("File data offset:    0x%x\n", entry->segment.offset);
  }

  return ZIPIO_SUCCESS;

err_close:
  fclose(z->f);
err_free:
  free(z);
  *_z = NULL;
  return err;

err_free_entries:
  for(i = 0; i < z->num_entries; i++)
  {
    struct zip_dirent *entry = z->entries[i];
    if(entry)
    {
      free(entry->file_name);
      free(entry);
    }
  }
  free(z->entries);
  goto err_close;
}

zipio_error_t zipio_close(zip_handle_t *z)
{
  zipio_error_t err = ZIPIO_SUCCESS;

  assert(z != NULL);

  if(fclose(z->f))
    err = ZIPIO_CLOSE_FAILED;
  free(z);

  return err;
}

#ifdef TEST

int main(int argc, char *argv[])
{
  zipio_error_t err = ZIPIO_SUCCESS;
  zip_handle_t *z;

  if(argc != 2)
  {
    info("usage: %s [file.zip]\n", argv[0]);
    goto err_out;
  }

  err = zipio_open(argv[1], &z);
  if(err)
    goto err_out;

  err = zipio_close(z);

err_out:
  if(err)
    warn("%s\n", zipio_strerror(err));
  return err;
}

#endif // TEST
