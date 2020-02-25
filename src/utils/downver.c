/* MegaZeux
 *
 * This is a very trivial tool to downgrade a world or board file to the
 * previous binary version. It allows a world or board created in the current
 * version of MegaZeux to be loadable in the previous version (i.e. before the
 * world format change). Of course, the downgraded world may not work, but it
 * should be loadable by the editor.
 *
 * This tool is a good insurance policy for brand new releases, to prevent
 * the case where people start work in a newer version of MegaZeux, only
 * for there to be a critical bug discovered, and for the user to be "locked
 * in" to the buggy version.
 *
 * Primarily, this tool is intended for DoZs where a new version may be used,
 * but a serious bug cannot reasonably be corrected before the end of the
 * competition.
 *
 * Since the tool will change with every release, it is hard coded to downgrade
 * only to one previous version, from one current version. This is intentional,
 * to make the tool easy to use for newbies (simply drag and drop).
 *
 * Copyright (C) 2008-2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdio.h>
#include <string.h>

#ifdef __WIN32__
#include <strings.h>
#endif

#include "../compat.h"

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

#include "../const.h"
#include "../memfile.h"
#include "../util.h"
#include "../world.h"
#include "../world_format.h"
#include "../zip.h"

#define DOWNVER_VERSION "2.92"
#define DOWNVER_EXT ".291"

#define MZX_VERSION_HI ((MZX_VERSION >> 8) & 0xff)
#define MZX_VERSION_LO (MZX_VERSION & 0xff)

#define MZX_VERSION_PREV_HI ((MZX_VERSION_PREV >> 8) & 0xff)
#define MZX_VERSION_PREV_LO (MZX_VERSION_PREV & 0xff)

enum status
{
  SUCCESS = 0,
  ARCHIVE_ERROR,
  READ_ERROR,
  WRITE_ERROR,
  SEEK_ERROR,
};

// MegaZeux's obtuse architecture requires this for the time being.
// This function is used in out_of_memory_check (util.c), and check alloc
// is required by zip.c (as it should be).

int error(const char *message, unsigned int a, unsigned int b, unsigned int c)
{
  fprintf(stderr, "%s\n", message);
  exit(-1);
}

#define error(...) \
  { \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr); \
  }

static inline void save_prop_p(int ident, struct memfile *prop,
 struct memfile *mf)
{
  save_prop_s(ident, prop->start, (prop->end - prop->start), 1, mf);
}

static enum zip_error zip_duplicate_file(struct zip_archive *dest,
 struct zip_archive *src, void (*handler)(struct memfile *, struct memfile *))
{
  enum zip_error result;
  unsigned int method;
  char name[9];
  void *buffer;
  size_t actual_size;

  result = zip_get_next_name(src, name, 9);
  if(result)
    return result;

  result = zip_get_next_method(src, &method);
  if(result)
    return result;

  result = zip_get_next_uncompressed_size(src, &actual_size);
  if(result)
    return result;

  buffer = malloc(actual_size);

  result = zip_read_file(src, buffer, actual_size, &actual_size);
  if(result)
    goto err_free;

  if(handler)
  {
    struct memfile mf_in;
    struct memfile mf_out;
    void *buffer2 = malloc(actual_size);

    mfopen(buffer, actual_size, &mf_in);
    mfopen(buffer2, actual_size, &mf_out);

    handler(&mf_out, &mf_in);

    // Free the old buffer, then replace it with the new buffer.
    free(buffer);
    mfsync(&buffer, &actual_size, &mf_out);
  }

  result = zip_write_file(dest, name, buffer, actual_size, (int)method);

err_free:
  free(buffer);
  return result;
}

static void convert_292_to_291_world_info(struct memfile *dest,
 struct memfile *src)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, src))
  {
    switch(ident)
    {
      case WPROP_WORLD_VERSION:
      case WPROP_FILE_VERSION:
        // Replace the version number
        save_prop_w(ident, MZX_VERSION_PREV, dest);
        break;

      default:
        save_prop_p(ident, &prop, dest);
        break;
    }
  }

  save_prop_eof(dest);
  mfresize(mftell(dest), dest);
}

static void convert_292_to_291_board_info(struct memfile *dest,
 struct memfile *src)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, src))
  {
    switch(ident)
    {
      case BPROP_FILE_VERSION:
        // Replace the version number
        save_prop_w(ident, MZX_VERSION_PREV, dest);
        break;

      default:
        save_prop_p(ident, &prop, dest);
        break;
    }
  }

  save_prop_eof(dest);
  mfresize(mftell(dest), dest);
}

static enum status convert_292_to_291(FILE *out, FILE *in)
{
  struct zip_archive *inZ = zip_open_fp_read(in);
  struct zip_archive *outZ = zip_open_fp_write(out);
  enum zip_error err = ZIP_SUCCESS;
  unsigned int file_id;
  unsigned int board_id;
  unsigned int robot_id;

  if(!inZ || !outZ)
  {
    zip_close(inZ, NULL);
    zip_close(outZ, NULL);
    return ARCHIVE_ERROR;
  }

  assign_fprops(inZ, 0);

  while(ZIP_SUCCESS == zip_get_next_prop(inZ, &file_id, &board_id, &robot_id))
  {
    switch(file_id)
    {
      case FPROP_WORLD_INFO:
        err = zip_duplicate_file(outZ, inZ, convert_292_to_291_world_info);
        break;

      case FPROP_BOARD_INFO:
        err = zip_duplicate_file(outZ, inZ, convert_292_to_291_board_info);
        break;

      default:
        err = zip_duplicate_file(outZ, inZ, NULL);
        break;
    }

    if(err != ZIP_SUCCESS)
    {
      // Skip files that don't work
      zip_skip_file(inZ);
      err = ZIP_SUCCESS;
    }
  }

  zip_close(inZ, NULL);
  zip_close(outZ, NULL);
  return SUCCESS;
}

int main(int argc, char *argv[])
{
  char fname[MAX_PATH + 4];
  enum status ret;
  long ext_pos;
  int world;
  int byte;
  FILE *in;
  FILE *out;

  if(strcmp(VERSION, DOWNVER_VERSION) < 0)
  {
    error("[ERROR] Update downver for " VERSION "!\n");
    return 1;
  }

  if(argc <= 1)
  {
    error("Usage: %s [mzx/mzb file]\n", argv[0]);
    goto exit_out;
  }

  ext_pos = strlen(argv[1]) - 4;
  if(ext_pos < 0)
  {
    error("File '%s' of an unknown type.\n", argv[1]);
    goto exit_out;
  }

  if(!strcasecmp(argv[1] + ext_pos, ".mzb"))
  {
    snprintf(fname, sizeof(fname), "%.*s" DOWNVER_EXT ".mzb",
     (int)ext_pos, argv[1]);
    fname[sizeof(fname) - 1] = '\0';
    world = false;
  }
  else if(!strcasecmp(argv[1] + ext_pos, ".mzx"))
  {
    snprintf(fname, sizeof(fname), "%.*s" DOWNVER_EXT ".mzx",
     (int)ext_pos, argv[1]);
    fname[sizeof(fname) - 1] = '\0';
    world = true;
  }
  else
  {
    error("Unknown extension '%s'.\n", argv[1] + ext_pos);
    goto exit_out;
  }

#ifdef CONFIG_PLEDGE_UTILS
#ifdef PLEDGE_HAS_UNVEIL
  if(unveil(argv[1], "r") || unveil(fname, "cw") || unveil(NULL, NULL))
  {
    error("[ERROR] Failed unveil!\n");
    return 1;
  }
#endif

  if(pledge(PROMISES, ""))
  {
    error("[ERROR] Failed pledge!\n");
    return 1;
  }
#endif

  in = fopen_unsafe(argv[1], "rb");
  if(!in)
  {
    error("Could not open '%s' for read.\n", argv[1]);
    goto exit_out;
  }

  out = fopen_unsafe(fname, "wb");
  if(!out)
  {
    error("Could not open '%s' for write.\n", fname);
    fclose(in);
    goto exit_out;
  }

  /* World files need some crap at the beginning skipped. Also, world files
   * can theoretically be encrypted, though practically this will not happen
   * with modern worlds. Just in case, check for it and abort.
   *
   * Board files just need the 0xFF byte at the start skipped.
   */
  if(world)
  {
    char name[BOARD_NAME_SIZE];
    size_t len;

    // Duplicate the world name
    len = fread(name, BOARD_NAME_SIZE, 1, in);
    if(len != 1)
      goto err_read;

    len = fwrite(name, BOARD_NAME_SIZE, 1, out);
    if(len != 1)
      goto err_write;

    // Check protection isn't enabled
    byte = fgetc(in);
    if(byte < 0)
    {
      goto err_read;
    }
    else
    if(byte != 0)
    {
      error("Protected worlds are not supported.\n");
      goto exit_close;
    }
    fputc(0, out);
  }
  else
  {
    byte = fgetc(in);
    if(byte < 0)
    {
      goto err_read;
    }
    else
    if(byte != 0xFF)
    {
      error("Board file is corrupt or unsupported.\n");
      goto exit_close;
    }
    fputc(0xFF, out);
  }

  // Validate version is current

  byte = fgetc(in);
  if(byte < 0)
  {
    goto err_read;
  }
  else
  if(byte != 'M')
  {
    error("World file is corrupt or unsupported.\n");
    goto exit_close;
  }

  byte = fgetc(in);
  if(byte < 0)
  {
    goto err_read;
  }
  else
  if(byte != MZX_VERSION_HI)
  {
    error("This tool only supports worlds or boards from %d.%d.\n",
     MZX_VERSION_HI, MZX_VERSION_LO);
    goto exit_close;
  }

  byte = fgetc(in);
  if(byte < 0)
  {
    goto err_read;
  }
  else
  if(byte != MZX_VERSION_LO)
  {
    error("This tool only supports worlds or boards from %d.%d.\n",
     MZX_VERSION_HI, MZX_VERSION_LO);
    goto exit_close;
  }

  // Write previous version's magic

  fputc('M', out);

  byte = fputc(MZX_VERSION_PREV_HI, out);
  if(byte == EOF)
    goto err_write;

  byte = fputc(MZX_VERSION_PREV_LO, out);
  if(byte == EOF)
    goto err_write;

  // Worlds and boards are the same from here out.
  // Conversion closes the file pointers, so NULL them.

  ret = convert_292_to_291(out, in);
  out = NULL;
  in = NULL;

  switch(ret)
  {
    case ARCHIVE_ERROR: goto err_zip;
    case SEEK_ERROR:    goto err_seek;
    case READ_ERROR:    goto err_read;
    case WRITE_ERROR:   goto err_write;
    case SUCCESS:       break;
  }

  fprintf(stdout,
   "File '%s' successfully downgraded from %d.%d to %d.%d.\n"
   "Saved to '%s'.\n",
   argv[1], MZX_VERSION_HI, MZX_VERSION_LO,
   MZX_VERSION_PREV_HI, MZX_VERSION_PREV_LO, fname);

exit_close:
  if(out)
    fclose(out);
  if(in)
    fclose(in);

exit_out:
  return 0;

err_zip:
  error("Error opening world archive, aborting.\n")
  goto exit_close;

err_seek:
  error("Seek error, aborting.\n");
  goto exit_close;

err_read:
  error("Read error, aborting.\n");
  goto exit_close;

err_write:
  error("Write error, aborting.\n");
  goto exit_close;
}
