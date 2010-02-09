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

#define MAX_GLOBAL_COMMENT_LEN (1 << 16)

struct zip_handle
{
  Uint16 num_entries;
  Uint32 cdr_offset;
  FILE *f;
};

static int64_t fgetuw(FILE *f)
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

static int64_t fgetud(FILE *f)
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
    case ZIPIO_NO_END_CENTRAL_DIRECTORY:
      return "Failed to locate end central directory record";
    case ZIPIO_UNSUPPORTED_MULTIPLE_DISKS:
      return "Multiple volume ZIPs are unsupported";
    default:
      return "Unknown error";
  }
}

zipio_error_t zipio_open(const char *filename, zip_handle_t **_z)
{
  static const unsigned char end_cdr_sig[] = { 'P', 'K', 0x5, 0x6 };
  zipio_error_t err = ZIPIO_SUCCESS;
  int end_cdr_sig_off;
  long size, start;
  zip_handle_t *z;
  int64_t d;

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
  end_cdr_sig_off = 0;
  while(true)
  {
    // May "fail" for unexpected reasons or simply because we got to EOF
    d = fgetc(z->f);
    if(d < 0)
      break;

    // Does the current byte look like our magic (so far?)
    if((unsigned char)d == end_cdr_sig[end_cdr_sig_off])
    {
      end_cdr_sig_off++;
      if(end_cdr_sig_off == sizeof(end_cdr_sig))
        break;
    }
    else
      end_cdr_sig_off = 0;
  }

  // Failed to locate End Central Directory Record
  if(d < 0)
  {
    err = ZIPIO_NO_END_CENTRAL_DIRECTORY;
    goto err_close;
  }

  // "number of this disk"
  d = fgetuw(z->f);
  if(d < 0)
  {
    err = ZIPIO_READ_FAILED;
    goto err_close;
  }
  else if(d > 0)
  {
    err = ZIPIO_UNSUPPORTED_MULTIPLE_DISKS;
    goto err_close;
  }

  // "number of the disk with the start of the central directory"
  d = fgetuw(z->f);
  if(d < 0)
  {
    err = ZIPIO_READ_FAILED;
    goto err_close;
  }
  else if(d > 0)
  {
    err = ZIPIO_UNSUPPORTED_MULTIPLE_DISKS;
    goto err_close;
  }

  // Skip "total number of entries in the central directory on this disk"
  // Can skip because we must be using non-spanning ZIP
  d = fgetuw(z->f);
  if(d < 0)
  {
    err = ZIPIO_READ_FAILED;
    goto err_close;
  }

  // "total number of entries in the central directory"
  d = fgetuw(z->f);
  if(d < 0)
  {
    err = ZIPIO_READ_FAILED;
    goto err_close;
  }
  z->num_entries = (Uint16)d;

  // "size of the central directory offset of start of central
  //  directory with respect to the starting disk number"
  d = fgetud(z->f);
  if(d < 0)
  {
    err = ZIPIO_READ_FAILED;
    goto err_close;
  }
  z->cdr_offset = (Uint32)d;

  debug("ZIP contains %u file(s). CDR offset is %u.\n",
   z->num_entries, z->cdr_offset);

  return ZIPIO_SUCCESS;

err_close:
  fclose(z->f);
err_free:
  free(z);
  *_z = NULL;
  return err;
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