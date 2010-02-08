/**
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
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 *
 * This work is in the public domain.
 */

#include <strings.h>
#include <string.h>

#include "world.h"

#define error(x...) \
  { \
    fprintf(stderr, x); \
    fflush(stderr); \
  }

#define WORLD_VERSION_HI ((WORLD_VERSION >> 8) & 0xff)
#define WORLD_VERSION_LO (WORLD_VERSION & 0xff)

#define WORLD_VERSION_PREV_HI ((WORLD_VERSION_PREV >> 8) & 0xff)
#define WORLD_VERSION_PREV_LO (WORLD_VERSION_PREV & 0xff)

int main(int argc, char *argv[])
{
  int world, byte;
  ssize_t ext_pos;
  FILE *fp;

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
    world = false;
  }
  else if(!strcasecmp(argv[1] + ext_pos, ".mzx"))
  {
    world = true;
  }
  else
  {
    error("Unknown extension '%s'.\n", argv[1] + ext_pos);
    goto exit_out;
  }

  fp = fopen(argv[1], "r+");
  if(!fp)
  {
    error("Could not open '%s' for read/write.\n", argv[1]);
    goto exit_out;
  }

  /* World files need some crap at the beginning skipped. Also, world files
   * can theoretically be encrypted, though practically this will not happen
   * with modern worlds. Just in case, check for it and abort.
   *
   * Board files just need the 0xFF byte at the start skipping.
   */
  if(world)
  {
    char name[BOARD_NAME_SIZE];
    size_t fret;

    // Last selected board name; junked, we simply don't care
    fret = fread(name, BOARD_NAME_SIZE, 1, fp);
    if(fret != 1)
      goto err_read;

    // Check protection isn't enabled
    byte = fgetc(fp);
    if(byte < 0)
      goto err_read;
    else if(byte != 0)
    {
      error("Protected worlds are not supported.\n");
      goto exit_close;
    }
  }
  else
  {
    byte = fgetc(fp);
    if(byte < 0)
      goto err_read;
    else if(byte != 0xff)
      error("Board file is corrupt or unsupported.\n");
  }

  // Validate version is current

  byte = fgetc(fp);
  if(byte < 0)
    goto err_read;
  else if(byte != 'M')
  {
    error("World file is corrupt or unsupported.\n");
    goto exit_close;
  }

  byte = fgetc(fp);
  if(byte < 0)
    goto err_read;
  else if(byte != WORLD_VERSION_HI)
  {
    error("This tool only supports worlds or boards from %d.%d.\n",
     WORLD_VERSION_HI, WORLD_VERSION_LO);
    goto exit_close;
  }

  byte = fgetc(fp);
  if(byte < 0)
    goto err_read;
  else if(byte != WORLD_VERSION_LO)
  {
    error("This tool only supports worlds or boards from %d.%d.\n",
     WORLD_VERSION_HI, WORLD_VERSION_LO);
    goto exit_close;
  }

  // Re-write current magic with previous version's magic

  fseek(fp, -2, SEEK_CUR);

  byte = fputc(WORLD_VERSION_PREV_HI, fp);
  if(byte == EOF)
    goto err_write;

  byte = fputc(WORLD_VERSION_PREV_LO, fp);
  if(byte == EOF)
    goto err_write;

  fprintf(stdout, "File '%s' successfully downgraded from %d.%d to %d.%d.\n",
   argv[1], WORLD_VERSION_HI, WORLD_VERSION_LO,
   WORLD_VERSION_PREV_HI, WORLD_VERSION_PREV_LO);

exit_close:
  fclose(fp);
exit_out:
  return 0;

err_read:
  error("Read error, aborting.\n");
  goto exit_close;

err_write:
  error("Write error, aborting.\n");
  goto exit_close;
}
