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

#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>

#include "../platform.h"
#include "../util.h"

// Maximum length of any ZIP file comment (bounds search for ECDR)
#define MAX_GLOBAL_COMMENT_LEN  (1 << 16)

// PKZIP-defined signatures for various ZIP headers
#define CENTRAL_DIRECTORY_MAGIC 0x02014b50
#define LOCAL_FILE_HEADER_MAGIC 0x04034b50
#define DATA_DESCRIPTOR_MAGIC   0x08074b50

// Offset in CD entry to Local Header offset (used by repack)
#define CDE_OFFSET_TO_LH        (4+2+2+2+2+2+2+4+4+4+2+2+2+2+2+4)

// Maximum block size for repacker
#define BUFFER_SIZE             4096

struct segment
{
  Uint32 offset;
  Uint32 length;
};

enum zip_method
{
  ZIP_METHOD_STORE     = 0,
  ZIP_METHOD_DEFLATE   = 8,
  ZIP_METHOD_DEFLATE64 = 9,
};

// The goal of the entry abstraction is to take just enough pertinent
// information from the LFH in order to rewrite it, should the file data
// be modified.
//
// We don't attempt to preserve file comments or so-called "extra field"
// data, because it was felt that this information could become inconsistent
// with respect to the written data.
//
// Additionally, the abstraction has been minimally extended with segment
// information to allow unchanged files to be moved inside the on-disk file.

struct zip_entry
{
  struct zip_entry *next;

  Uint32 crc32;
  Uint32 uncompressed_size;

  enum zip_method method;
  char *file_name;
  time_t datetime;

  struct segment cd_entry;
  struct segment local_header;
  struct segment file_data;
  struct segment data_descriptor;
};

// Construction of the CD segment is done by walking each entry and either
// copying its cd_entry (CDE) or generating a new one.
//
// The ECDR is wholly static apart from the CD offset/length (which must be
// computed at runtime) and the file comment, which is stored here.

struct zip_file
{
  struct zip_entry *entries;
  Uint16 comment_length;
  char *comment;

  struct segment ecdr;
};

// Contains the file abstraction and (of course) the open file handle.

struct zip_handle
{
  struct zip_file file;
  char *path;
  FILE *f;
};

// DIR abstraction for walking "directories" in ZIP files.

struct zip_dir
{
  struct zip_file *file;

  struct zip_entry *last_match;
  char *path;
};

static const unsigned char ecdr_sig[] = { 'P', 'K', 0x5, 0x6 };

static Sint64 fgetuw(FILE *f)
{
  Uint16 d = 0;
  int i, j;

  for(i = 0, j = 0; i < (int)sizeof(Uint16); i++, j += 8)
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

  for(i = 0, j = 0; i < (int)sizeof(Uint32); i++, j += 8)
  {
    int r = fgetc(f);
    if(r < 0)
      return r;
    d |= r << j;
  }

  return d;
}

static Sint64 fputuw(Uint16 d, FILE *f)
{
  int i;

  for(i = 0; i < (int)sizeof(Uint16); i++)
  {
    int r = fputc(d & 0xff, f);
    if(r < 0)
      return r;
    d >>= 8;
  }

  return d;
}

static Sint64 fputud(Uint32 d, FILE *f)
{
  int i;

  for(i = 0; i < (int)sizeof(Uint32); i++)
  {
    int r = fputc(d & 0xff, f);
    if(r < 0)
      return r;
    d >>= 8;
  }

  return d;
}

static bool zgetuw(FILE *f, Sint64 *d, enum zip_error *err, enum zip_error code)
{
  *d = fgetuw(f);

  if(*d < 0)
  {
    *err = code;
    return false;
  }

  return true;
}

static bool zgetud(FILE *f, Sint64 *d, enum zip_error *err, enum zip_error code)
{
  *d = fgetud(f);

  if(*d < 0)
  {
    *err = code;
    return false;
  }

  return true;
}

static bool zputuw(FILE *f, Uint16 d, enum zip_error *err, enum zip_error code)
{
  if(fputuw(d, f) < 0)
  {
    *err = code;
    return false;
  }

  return true;
}

static bool zputud(FILE *f, Uint32 d, enum zip_error *err, enum zip_error code)
{
  if(fputud(d, f) < 0)
  {
    *err = code;
    return false;
  }

  return true;
}

static time_t dos_to_time_t(Uint16 time, Uint16 date)
{
  struct tm tm;

  memset(&tm, 0, sizeof(struct tm));

  tm.tm_sec  = ((time & 0x1F)   << 1);      // seconds (0-58)
  tm.tm_min  = ((time & 0x7E0)  >> 5);      // minutes (0-59)
  tm.tm_hour = ((time & 0xF800) >> 11);     // hours   (0-23)
  tm.tm_mday = ((date & 0x1F)   >> 0) - 1;  // day     (1-31)
  tm.tm_mon  = ((date & 0x1E0)  >> 5) - 1;  // month   (1-12)
  tm.tm_year = ((date & 0xFE00) >> 9) + 80; // year    (since 1980)

  return mktime(&tm);
}

const char *zipio_strerror(enum zip_error err)
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
    case ZIPIO_WRITE_FAILED:
      return "Failure writing to file";
    case ZIPIO_TRUNCATE_FAILED:
      return "Failed to truncate file on close";
    case ZIPIO_NO_CENTRAL_DIRECTORY:
      return "Failed to locate central directory record";
    case ZIPIO_NO_END_CENTRAL_DIRECTORY:
      return "Failed to locate end central directory record";
    case ZIPIO_CORRUPT_CENTRAL_DIRECTORY:
      return "Central directory is corrupt";
    case ZIPIO_CORRUPT_LOCAL_FILE_HEADER:
      return "Local file header is corrupt";
    case ZIPIO_CORRUPT_DATETIME:
      return "Date/Time specifier is corrupt";
    case ZIPIO_UNSUPPORTED_FEATURE:
      return "Unsupported extended ZIP feature (encryption, etc.)";
    case ZIPIO_UNSUPPORTED_MULTIPLE_DISKS:
      return "Multiple volume ZIPs are unsupported";
    case ZIPIO_UNSUPPORTED_ZIP64:
      return "No support for ZIP64 entries";
    case ZIPIO_UNSUPPORTED_COMPRESSION_METHOD:
      return "Unsupported ZIP compression method";
    case ZIPIO_PATH_NOT_FOUND:
      return "Requested path not found within ZIP";
    default:
      return "Unknown error";
  }
}

static void free_entry(struct zip_entry *entry)
{
  free(entry->file_name);
  free(entry);
}

static void free_entries(struct zip_handle *z)
{
  struct zip_entry *entry, *next_entry;

  for(entry = z->file.entries; entry; entry = next_entry)
  {
    next_entry = entry->next;
    free_entry(entry);
  }
  z->file.entries = NULL;
}

static Uint32 compute_terminal_offset(struct zip_entry *entry)
{
  if(entry->data_descriptor.offset > 0)
    return entry->data_descriptor.offset + entry->data_descriptor.length;
  else if(entry->file_data.offset > 0)
    return entry->file_data.offset + entry->file_data.length;
  else
    return entry->local_header.offset + entry->local_header.length;
}

static int compare_offset(const void *_a, const void *_b)
{
  const struct zip_entry **a = (const struct zip_entry **)_a;
  const struct zip_entry **b = (const struct zip_entry **)_b;
  return (int)(*a)->local_header.offset - (int)(*b)->local_header.offset;
}

enum zip_error zipio_open(const char *filename, struct zip_handle **_z)
{
  long pos, size, start, cur_ecdr_offset = 0;
  struct zip_entry *entry, *last_entry;
  enum zip_error err = ZIPIO_SUCCESS;
  Uint32 entry_terminal_offset = 0;
  struct zip_handle *z;
  size_t filename_len;
  Uint16 num_entries;
  struct segment cd;
  int ecdr_sig_off;
  Sint64 d;
  int i;

  assert(filename != NULL);

  *_z = ccalloc(1, sizeof(struct zip_handle));
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
            err = ZIPIO_TELL_FAILED;
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
  z->file.ecdr.offset = cur_ecdr_offset - sizeof(ecdr_sig);

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
  num_entries = d;

  // "size of the central directory"
  if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;
  cd.length = d;

  // "offset of start of central directory with respect to
  //  the starting disk number"
  if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;
  cd.offset = d;

  // ".ZIP file comment length"
  if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
    goto err_close;
  z->file.comment_length = d;

  // ".ZIP file comment"
  if(z->file.comment_length > 0)
  {
    z->file.comment = cmalloc(z->file.comment_length);
    if(fread(z->file.comment, z->file.comment_length, 1, z->f) != 1)
    {
      err = ZIPIO_READ_FAILED;
      goto err_close;
    }
  }

  // Compute ECDR length
  d = ftell(z->f);
  if(d < 0)
  {
    err = ZIPIO_TELL_FAILED;
    goto err_close;
  }
  z->file.ecdr.length = d - z->file.ecdr.offset;

  debug("Valid ZIP ECDR found at offset 0x%x; length is %u.\n",
   z->file.ecdr.offset, z->file.ecdr.length);

  debug("ZIP CD found at offset 0x%x; length is %u. %u entries.\n",
   cd.offset, cd.length, num_entries);

  // Move to supposed Central Directory
  if(fseek(z->f, cd.offset, SEEK_SET))
  {
    err = ZIPIO_SEEK_FAILED;
    goto err_close;
  }

  for(i = 0; i < num_entries; i++)
  {
    Uint16 file_name_length, extra_field_length, file_comment_length;
    struct zip_entry *new_entry;
    Uint16 time, date;

    new_entry = ccalloc(1, sizeof(struct zip_entry));

    if(z->file.entries)
    {
      entry->next = new_entry;
      entry = entry->next;
    }
    else
      z->file.entries = entry = new_entry;

    // compute CD entry offset
    pos = ftell(z->f);
    if(pos < 0)
    {
      err = ZIPIO_TELL_FAILED;
      goto err_free_entries;
    }
    entry->cd_entry.offset = pos;

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

    // We don't support Encryption (0), Method 8 (4), Patched Data (5),
    // Strong Encryption (6), Unused (7-10), Enhanced Compression (12),
    // Central Direction Encryption (13), PKZIP Reserved (14-15)
    if(d & 0xF7F1)
    {
      err = ZIPIO_UNSUPPORTED_FEATURE;
      goto err_free_entries;
    }

    // "compression method"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->method = d;

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
    entry->file_data.length = d;

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
    file_name_length = d;

    // File name cannot be NULL
    if(file_name_length == 0)
    {
      err = ZIPIO_CORRUPT_CENTRAL_DIRECTORY;
      goto err_free_entries;
    }

    // "extra field length"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    extra_field_length = d;

    // "file comment length"
    if(!zgetuw(z->f, &d, &err, ZIPIO_UNSUPPORTED_MULTIPLE_DISKS))
      goto err_free_entries;
    file_comment_length = d;

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

    // Don't support variable length preamble (yet)
    if(d & 0x2)
    {
      err = ZIPIO_UNSUPPORTED_FEATURE;
      goto err_free_entries;
    }

    // Skip "external file attributes"
    // Can be skipped because this only provides MSDOS attributes
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // "relative offset of local header"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    entry->local_header.offset = d;

    // FIXME: No ZIP64 support
    if(d == 0xffffffff)
    {
      err = ZIPIO_UNSUPPORTED_ZIP64;
      goto err_free_entries;
    }

    // Add NULL terminator to file_name (file may have none)
    entry->file_name = cmalloc(file_name_length + 1);
    entry->file_name[file_name_length] = 0;

    // "file name"
    if(fread(entry->file_name, file_name_length, 1, z->f) != 1)
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // Skip "extra field"
    // We can skip this because MZX never uses the extra data
    if(fseek(z->f, extra_field_length, SEEK_CUR))
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // Skip "file comment"
    // We can skip this because MZX never uses the file comments
    if(fseek(z->f, file_comment_length, SEEK_CUR))
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    pos = ftell(z->f);
    if(pos < 0)
    {
      err = ZIPIO_TELL_FAILED;
      goto err_free_entries;
    }

    // Check we've not exceeded this segment's bounds
    if(pos > cd.offset + cd.length)
    {
      err = ZIPIO_CORRUPT_CENTRAL_DIRECTORY;
      goto err_free_entries;
    }

    // Compute CD entry length
    entry->cd_entry.length = pos - entry->cd_entry.offset;

    // Convert to standard C89/C99 time_t
    entry->datetime = dos_to_time_t(time, date);
    if(entry->datetime < 0)
    {
      err = ZIPIO_CORRUPT_DATETIME;
      goto err_free_entries;
    }
  }

  // Convert entry list to array and qsort() it
  // Don't bother sorting if there are no entries yet, or if there's only one
  if(z->file.entries && z->file.entries->next)
  {
    struct zip_entry **earray =
     cmalloc(num_entries * sizeof(struct zip_entry *));

    for(i = 0, entry = z->file.entries; i < num_entries; i++)
    {
      earray[i] = entry;
      entry = entry->next;
    }

    qsort(earray, num_entries, sizeof(struct zip_entry *), compare_offset);

    z->file.entries = earray[0];
    for(i = 0; i < num_entries - 1; i++)
      earray[i]->next = earray[i + 1];
    earray[i]->next = NULL;

    free(earray);
  }

  // Now the entries are sorted, we can perform some checks on their offsets

  last_entry = NULL;
  for(entry = z->file.entries; entry; entry = entry->next)
  {
    if(last_entry)
    {
      if(entry->local_header.offset <= last_entry->local_header.offset)
      {
        err = ZIPIO_CORRUPT_CENTRAL_DIRECTORY;
        goto err_free_entries;
      }
    }

    if(entry->local_header.offset >= cd.offset ||
       entry->file_data.offset    >= cd.offset)
    {
      err = ZIPIO_CORRUPT_CENTRAL_DIRECTORY;
      goto err_free_entries;
    }

    last_entry = entry;
  }

  // Sanity check Local File Header (LFH) for each of the CD entries

  for(entry = z->file.entries; entry; entry = entry->next)
  {
    Uint16 file_name_length, extra_field_length, time, date;
    bool data_descriptor = false;
    time_t datetime;
    char *file_name;

    // Move to this entry's LFH
    if(fseek(z->f, entry->local_header.offset, SEEK_SET))
    {
      err = ZIPIO_SEEK_FAILED;
      goto err_free_entries;
    }

    // "local file header signature"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // Invalid LFH header; offset must be corrupt
    if(d != LOCAL_FILE_HEADER_MAGIC)
    {
      err = ZIPIO_CORRUPT_LOCAL_FILE_HEADER;
      goto err_free_entries;
    }

    // Skip "version needed to extract"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // "general purpose bit flag"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // We don't support Encryption (0), Method 8 (4), Patched Data (5),
    // Strong Encryption (6), Unused (7-10), Enhanced Compression (12),
    // Central Direction Encryption (13), PKZIP Reserved (14-15)
    if(d & 0x77F1)
    {
      err = ZIPIO_UNSUPPORTED_FEATURE;
      goto err_free_entries;
    }

    if(d & 8000)
      warn("LFH GPBF bit 15 set (PKWARE reserved)!\n");

    // "crc-32", "compressed size" and "uncompressed size" fields are
    // deferred to a data descriptor extension
    if(d & 0x8)
      data_descriptor = true;

    // "compression method"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // Method doesn't match CD's record; take LFH's
    if(d != entry->method)
    {
      warn("LFH method overrides differing CD method!\n");
      entry->method = d;
    }

    // Can only support Stored and Deflated right now
    if(entry->method != ZIP_METHOD_STORE &&
       entry->method != ZIP_METHOD_DEFLATE &&
       entry->method != ZIP_METHOD_DEFLATE64)
    {
      err = ZIPIO_UNSUPPORTED_COMPRESSION_METHOD;
      goto err_free_entries;
    }

    // Skip "last mod file time"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    time = d;

    // Skip "last mod file date"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    date = d;

    // "crc-32"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // If CRC is zero but we expected it to be deferred, we tolerate any
    // mismatch. Otherwise, inherit the LFH's CRC and warn the user about
    // the inconsistency.
    if(!(d == 0 && data_descriptor) && d != entry->crc32)
    {
      warn("LFH crc-32 overrides differing CD crc-32!\n");
      entry->crc32 = d;
    }

    // "compressed size"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // Same check as "crc-32" but for "compressed size"
    if(!(d == 0 && data_descriptor) && d != entry->file_data.length)
    {
      warn("LFH compressed size overrides differing CD compressed size!\n");
      entry->file_data.length = d;
    }

    // "uncompressed size"
    if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;

    // Same check as "crc-32" but for "uncompressed size"
    if(!(d == 0 && data_descriptor) && d != entry->uncompressed_size)
    {
      warn("LFH uncompressed size overrides differing CD uncompressed size!\n");
      entry->uncompressed_size = d;
    }

    // "file name length"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    file_name_length = d;

    // File name cannot be NULL
    if(file_name_length == 0)
    {
      err = ZIPIO_CORRUPT_LOCAL_FILE_HEADER;
      goto err_free_entries;
    }

    // "extra field length"
    if(!zgetuw(z->f, &d, &err, ZIPIO_READ_FAILED))
      goto err_free_entries;
    extra_field_length = d;

    // Add NULL terminator to file_name (file may have none)
    file_name = cmalloc(file_name_length + 1);
    file_name[file_name_length] = 0;

    // "file name" (again)
    if(fread(file_name, file_name_length, 1, z->f) != 1)
    {
      free(file_name);
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // The "file name" here should always match the CD. If it doesn't, inherit
    // the LFH's "file name" and warn the user about the inconsistency.
    if(strcmp(entry->file_name, file_name) != 0)
    {
      char *old_name = entry->file_name;
      entry->file_name = file_name;
      file_name = old_name;
      warn("LFH file name overrides differing CD file name!\n");
    }

    // May be the CD or LFH filename, depending only on above condition
    free(file_name);

    // Skip "extra field" (again)
    // We can skip this because MZX never uses the extra data
    if(fseek(z->f, extra_field_length, SEEK_CUR))
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // We can now compute the length of the LFH..
    // ..and the offset of the file data
    pos = ftell(z->f);
    if(pos < 0)
    {
      err = ZIPIO_TELL_FAILED;
      goto err_free_entries;
    }
    entry->local_header.length = pos - entry->local_header.offset;
    entry->file_data.offset = pos;

    // Skip file data (if any)
    if(fseek(z->f, entry->file_data.length, SEEK_CUR))
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_entries;
    }

    // If bit 3 of the "general purpose bit flag" was set, we should read
    // and validate the a data descriptor.
    if(data_descriptor)
    {
      // Compute the offset of the Data Descriptor (also used for rollback)
      pos = ftell(z->f);
      if(pos < 0)
      {
        err = ZIPIO_TELL_FAILED;
        goto err_free_entries;
      }
      entry->data_descriptor.offset = pos;

      // Read "Data descriptor" signature (if present)
      if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
        goto err_free_entries;

      if(d != DATA_DESCRIPTOR_MAGIC)
      {
        // Found no signature, so roll back and assume this was actually
        // the crc-32 (NOTE: this aspect of ZIP is horrible indeed).
        if(fseek(z->f, pos, SEEK_SET))
        {
          err = ZIPIO_SEEK_FAILED;
          goto err_free_entries;
        }
      }

      // "crc-32" (again)
      if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
        goto err_free_entries;

      // Data descriptor crc-32 is considered superior to all others
      if(d != entry->crc32)
      {
        warn("Data descriptor crc-32 overrides differing CD crc-32!\n");
        entry->crc32 = d;
      }

      // "compressed size" (again)
      if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
        goto err_free_entries;

      // Same check as for "crc-32"
      if(d != entry->file_data.length)
      {
        warn("Data descriptor compressed size overrides "
          "differing CD compressed size!\n");
        entry->file_data.length = d;
      }

      // "uncompressed size" (again)
      if(!zgetud(z->f, &d, &err, ZIPIO_READ_FAILED))
        goto err_free_entries;

      // Same check as for "crc-32"
      if(d != entry->uncompressed_size)
      {
        warn("Data descriptor uncompressed size overrides "
        "differing CD uncompressed size!\n");
        entry->uncompressed_size = d;
      }

      // Finally, compute length of data descriptor
      pos = ftell(z->f);
      if(pos < 0)
      {
        err = ZIPIO_TELL_FAILED;
        goto err_free_entries;
      }
      entry->data_descriptor.length = pos - entry->data_descriptor.offset;
    }

    // Convert to standard C89/C99 time_t
    datetime = dos_to_time_t(time, date);
    if(datetime < 0)
    {
      err = ZIPIO_CORRUPT_DATETIME;
      goto err_free_entries;
    }

    // The datetime should always match the CD. If it doesn't, inherit
    // the LFH's datetime and warn the user about the inconsistency.
    if(datetime != entry->datetime)
    {
      warn("LFH time/date overrides differing CD time/date!\n");
      entry->datetime = datetime;
    }

    debug("LFH -- Parsed file '%s' OK.\n", entry->file_name);
    debug("LFH offset/length:   0x%x (%u)\n",
     entry->local_header.offset, entry->local_header.length);
    debug("Compression Method:  %s\n", (entry->method) ? "Deflate" : "Store");
    debug("Time/Date:           %s", ctime(&entry->datetime));
    debug("CRC-32:              0x%x\n", entry->crc32);
    debug("Compressed Size:     %u\n", entry->file_data.length);
    debug("Uncompressed Size:   %u\n", entry->uncompressed_size);
  }

  // Try to detect overlapping and padded file segments where possible

  if(z->file.entries && z->file.entries->local_header.offset != 0)
  {
    warn("Detected %u bytes junk between start of file and first entry!\n",
     z->file.entries->local_header.offset);
  }

  for(entry = z->file.entries; entry; entry = entry->next)
  {
    if(entry_terminal_offset > 0)
    {
      if(entry_terminal_offset > entry->local_header.offset)
      {
        err = ZIPIO_CORRUPT_CENTRAL_DIRECTORY;
        goto err_free_entries;
      }

      if(entry_terminal_offset < entry->local_header.offset)
      {
        warn("Detected %u bytes junk between ZIP entries!\n",
         entry->local_header.offset - entry_terminal_offset);
      }
    }

    entry_terminal_offset = compute_terminal_offset(entry);
  }

  if(entry_terminal_offset < cd.offset)
  {
    warn("Detected %u bytes junk between final ZIP entry and CD!\n",
     cd.offset - entry_terminal_offset);
  }

  if(cd.offset + cd.length < z->file.ecdr.offset)
  {
    warn("Detected %u bytes junk between CD and ECDR!\n",
     z->file.ecdr.offset - (cd.offset + cd.length));
  }

  // Compare with file size for contiguity
  pos = ftell_and_rewind(z->f);
  if(pos < 0)
  {
    err = ZIPIO_TELL_FAILED;
    goto err_free_entries;
  }

  if(z->file.ecdr.offset + z->file.ecdr.length < pos)
  {
    warn("Detected %lu bytes junk between ECDR and EOF!\n",
     pos - (z->file.ecdr.offset + z->file.ecdr.length));
  }

  // Keep a record of the original filename for truncate-on-close
  filename_len = strlen(filename);
  z->path = cmalloc(filename_len + 1);
  strncpy(z->path, filename, filename_len + 1);

  return ZIPIO_SUCCESS;

err_close:
  fclose(z->f);
err_free:
  free(z);
  *_z = NULL;
  return err;

err_free_entries:
  free_entries(z);
  goto err_close;
}

static void cde_rewrite_lh_offset(char *cde, Uint32 lh_offset)
{
  int i;

  for(i = 0; i < (int)sizeof(Uint32); i++)
  {
    *(cde + CDE_OFFSET_TO_LH + i) = lh_offset & 0xff;
    lh_offset >>= 8;
  }
}

static enum zip_error zipio_stash_cd(struct zip_handle *z, Uint16 *num_entries,
 char ***_cd)
{
  enum zip_error err = ZIPIO_SUCCESS;
  struct zip_entry *entry;
  char **cd;
  Uint16 i;

  for(entry = z->file.entries; entry; entry = entry->next)
    (*num_entries)++;

  cd = cmalloc(sizeof(char *) * (*num_entries));

  for(entry = z->file.entries, i = 0; entry; entry = entry->next, i++)
  {
    cd[i] = cmalloc(entry->cd_entry.length);

    if(fseek(z->f, entry->cd_entry.offset, SEEK_SET))
    {
      err = ZIPIO_SEEK_FAILED;
      goto err_free_cd;
    }

    if(fread(cd[i], entry->cd_entry.length, 1, z->f) != 1)
    {
      err = ZIPIO_READ_FAILED;
      goto err_free_cd;
    }
  }

  *_cd = cd;
  return err;

err_free_cd:
  free(cd);
  *_cd = NULL;
  return err;
}

static enum zip_error zipio_rebuild_cd(struct zip_handle *z, Uint32 cd_offset)
{
  struct zip_entry *entry;
  Uint16 i, num_entries;
  Uint32 cd_length = 0;
  enum zip_error err;
  char **cd;
  long pos;

  // Store the existing (valid) CD entries on the heap temporarily
  err = zipio_stash_cd(z, &num_entries, &cd);
  if(err)
    goto err_out;

  // FIXME: If this function is consolidated, the call below isn't required
  // Scan back to the original start of the CD
  if(fseek(z->f, cd_offset, SEEK_SET))
  {
    err = ZIPIO_SEEK_FAILED;
    goto err_free_cd;
  }

  // Re-write the CD using the collated CDEs
  for(entry = z->file.entries, i = 0; entry; entry = entry->next, i++)
  {
    // Update CDE with new offset
    pos = ftell(z->f);
    if(pos < 0)
    {
      err = ZIPIO_TELL_FAILED;
      goto err_free_cd;
    }
    entry->cd_entry.offset = pos;

    // Unconditionally rewrite the CDE's LH offset
    cde_rewrite_lh_offset(cd[i], entry->local_header.offset);

    // Write back original CDE
    if(fwrite(cd[i], entry->cd_entry.length, 1, z->f) != 1)
    {
      err = ZIPIO_WRITE_FAILED;
      goto err_free_cd;
    }

    // Must account the CD length for the ECDR writeout
    cd_length += entry->cd_entry.length;
  }

  z->file.ecdr.offset = pos;

  // Write out ECDR signature
  if(fwrite(ecdr_sig, sizeof(ecdr_sig), 1, z->f) != 1)
  {
    err = ZIPIO_WRITE_FAILED;
    goto err_free_cd;
  }

  // "number of this disk" (0)
  if(!zputuw(z->f, 0, &err, ZIPIO_WRITE_FAILED))
    goto err_free_cd;

  // "number of the disk with the start of the central directory" (0)
  if(!zputuw(z->f, 0, &err, ZIPIO_WRITE_FAILED))
    goto err_free_cd;

  // "total number of entries in the central directory on this disk"
  if(!zputuw(z->f, num_entries, &err, ZIPIO_WRITE_FAILED))
    goto err_free_cd;

  // "total number of entries in the central directory"
  if(!zputuw(z->f, num_entries, &err, ZIPIO_WRITE_FAILED))
    goto err_free_cd;

  // "size of the central directory"
  if(!zputud(z->f, cd_length, &err, ZIPIO_WRITE_FAILED))
    goto err_free_cd;

  // "offset of start of central directory with respect to
  //  the starting disk number"
  if(!zputud(z->f, cd_offset, &err, ZIPIO_WRITE_FAILED))
    goto err_free_cd;

  // ".ZIP file comment length"
  if(!zputuw(z->f, z->file.comment_length, &err, ZIPIO_WRITE_FAILED))
    goto err_free_cd;

  // ".ZIP file comment" (skip if none)
  if(z->file.comment_length)
    if(fwrite(z->file.comment, z->file.comment_length, 1, z->f) != 1)
      err = ZIPIO_WRITE_FAILED;

  // Compute new ECDR length (necessary for truncate-on-close)
  pos = ftell(z->f);
  if(pos < 0)
  {
    err = ZIPIO_TELL_FAILED;
    goto err_free_cd;
  }
  z->file.ecdr.length = pos - z->file.ecdr.offset;

err_free_cd:
  free(cd);
err_out:
  return err;
}

static enum zip_error zipio_sync(struct zip_handle *z)
{
  enum zip_error err = ZIPIO_SUCCESS;
  struct zip_entry *entry;
  Uint32 file_offset = 0;
  char *buffer;

  buffer = cmalloc(BUFFER_SIZE);

  for(entry = z->file.entries; entry; entry = entry->next)
  {
    Uint32 move_length, pos = 0;
    long src_pos, dest_pos;

    // This file doesn't need to be moved
    if(entry->local_header.offset == file_offset)
    {
      file_offset = compute_terminal_offset(entry);
      continue;
    }

    // Can only move data in one direction (closer to start) at the moment
    assert(entry->local_header.offset > file_offset);

    // Compute the length of the moved block
    move_length = compute_terminal_offset(entry) - entry->local_header.offset;

    // Initial file cursor positions
    src_pos = (long)entry->local_header.offset;
    dest_pos = (long)file_offset;

    // Move file block-wise to conserve RAM
    while(pos != move_length)
    {
      Uint32 block_length = MIN(BUFFER_SIZE, move_length - pos);

      if(fseek(z->f, src_pos, SEEK_SET))
      {
        err = ZIPIO_SEEK_FAILED;
        goto err_free_buffer;
      }

      if(fread(buffer, block_length, 1, z->f) != 1)
      {
        err = ZIPIO_READ_FAILED;
        goto err_free_buffer;
      }

      if(fseek(z->f, dest_pos, SEEK_SET))
      {
        err = ZIPIO_SEEK_FAILED;
        goto err_free_buffer;
      }

      if(fwrite(buffer, block_length, 1, z->f) != 1)
      {
        err = ZIPIO_WRITE_FAILED;
        goto err_free_buffer;
      }

      pos += block_length;
      src_pos += block_length;
      dest_pos += block_length;
    }

    // Fix segment offsets wrt to new destination
    entry->local_header.offset -= src_pos - dest_pos;
    entry->file_data.offset -= src_pos - dest_pos;
    entry->data_descriptor.offset -= src_pos - dest_pos;

    // Compute offset for next file (if any)
    file_offset += move_length;
  }

  err = zipio_rebuild_cd(z, file_offset);

err_free_buffer:
  free(buffer);
  return err;
}

enum zip_error zipio_unlink(struct zip_handle *z, const char *pathname)
{
  struct zip_entry *entry, *last_entry = NULL;
  enum zip_error err = ZIPIO_SUCCESS;

  assert(z != NULL);
  assert(z->f != NULL);
  assert(pathname != NULL);

  for(entry = z->file.entries; entry; last_entry = entry, entry = entry->next)
    if(strcmp(entry->file_name, pathname) == 0)
      break;

  if(!entry)
  {
    err = ZIPIO_PATH_NOT_FOUND;
    goto err_out;
  }

  if(!last_entry)
    z->file.entries = entry->next;
  else
    last_entry->next = entry->next;

  free_entry(entry);

err_out:
  return err;
}

enum zip_error zipio_close(struct zip_handle *z)
{
  enum zip_error err = ZIPIO_SUCCESS;
  long length;

  assert(z != NULL);

  // Obtain original file length
  length = ftell_and_rewind(z->f);
  if(length < 0)
    err = ZIPIO_TELL_FAILED;

  if(fclose(z->f))
    err = ZIPIO_CLOSE_FAILED;

  if(length >= 0 && z->file.ecdr.offset + z->file.ecdr.length < length)
    if(truncate(z->path, z->file.ecdr.offset + z->file.ecdr.length) < 0)
      err = ZIPIO_TRUNCATE_FAILED;

  free(z->file.comment);
  free_entries(z);
  free(z->path);
  free(z);

  return err;
}

// Since we know the ZIP's original filename (stored in the handle) we
// expect all opendir()s to be relative to this. The following examples
// are both valid.

// test.zip/fileA		(ZIP opened as "test.zip")
// /path/to/test.zip/fileA	(ZIP opened as "/path/to/test.zip")

// If we get something else we'll fail.

enum zip_error zipio_opendir(struct zip_handle *z, const char *name, struct zip_dir **_d)
{
  struct zip_dir *d;
  size_t name_len;

  assert(z != NULL);
  assert(name != NULL);
  assert(_d != NULL);

  // Paths must be relative to original ZIP open name
  if(strncmp(z->path, name, strlen(z->path)))
    return ZIPIO_PATH_NOT_FOUND;

  *_d = ccalloc(1, sizeof(struct zip_dir));
  d = *_d;

  // Skip past the base path
  name += strlen(z->path);

  // If there's anything left, we have a specific search path..
  name_len = strlen(name);
  if(name_len)
  {
    // Should always be the case..
    if(*name == '/')
      name++;

    // Store a stripped version of the path on the zip_dir
    d->path = cmalloc(name_len);
    strncpy(d->path, name, name_len - 1);
    d->path[name_len - 1] = 0;
  }

  d->file = &z->file;
  return ZIPIO_SUCCESS;
}

enum zip_error zipio_closedir(struct zip_dir *d)
{
  assert(d != NULL);
  free(d);
  return ZIPIO_SUCCESS;
}

// FIXME: Linear search.. could be improved

enum zip_error zipio_readdir(struct zip_dir *d, char *path)
{
  struct zip_entry *entry;

  assert(d != NULL);
  assert(path != NULL);

  if(!d->last_match)
    entry = d->file->entries;
  else
    entry = d->last_match->next;

  if(d->path)
  {
    while(entry)
    {
      if(!strncasecmp(entry->file_name, d->path, strlen(d->path)))
        break;
      entry = entry->next;
    }
  }

  if(!entry)
    return ZIPIO_PATH_NOT_FOUND;

  strncpy(path, entry->file_name, MAX_PATH - 1);
  path[MAX_PATH - 1] = 0;

  d->last_match = entry;
  return ZIPIO_SUCCESS;
}

#ifdef TEST

int main(int argc, char *argv[])
{
  static const char unlink_path[] = "test";
  enum zip_error err = ZIPIO_SUCCESS;
  struct zip_handle *z;
  struct zip_dir *d;

  if(argc != 2)
  {
    info("usage: %s [file.zip]\n", argv[0]);
    goto err_out;
  }

  err = zipio_open(argv[1], &z);
  if(err)
    goto err_out;

  err = zipio_opendir(z, argv[1], &d);
  if(err)
    goto err_out;

  while(true)
  {
    char entry[MAX_PATH];
    err = zipio_readdir(d, entry);
    if(err)
      break;
    info(" %s\n", entry);
  }

  err = zipio_closedir(d);
  if(err)
    goto err_out;

  err = zipio_unlink(z, unlink_path);
  switch(err)
  {
    case ZIPIO_SUCCESS:
      break;

    case ZIPIO_PATH_NOT_FOUND:
      warn("Path '%s' not found in ZIP.\n", unlink_path);
      err = ZIPIO_SUCCESS;
      break;

    default:
      goto err_out;
  }

  err = zipio_sync(z);
  if(err)
    goto err_out;

  err = zipio_close(z);

err_out:
  if(err)
    warn("%s\n", zipio_strerror(err));
  return err;
}

#endif // TEST
