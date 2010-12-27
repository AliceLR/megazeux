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

#include <string.h>

#ifdef __WIN32__
#include <strings.h>
#endif

#include "world.h"
#include "util.h"

#define error(...) \
  { \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr); \
  }

#define WORLD_VERSION_HI ((WORLD_VERSION >> 8) & 0xff)
#define WORLD_VERSION_LO (WORLD_VERSION & 0xff)

#define WORLD_VERSION_PREV_HI ((WORLD_VERSION_PREV >> 8) & 0xff)
#define WORLD_VERSION_PREV_LO (WORLD_VERSION_PREV & 0xff)

#if WORLD_VERSION == 0x0253

#define WORLD_GLOBAL_OFFSET_OFFSET 4230
#define MAX_BOARDS                 250

enum status
{
  SUCCESS = 0,
  READ_ERROR,
  WRITE_ERROR,
  SEEK_ERROR,
};

static struct board_entry
{
  unsigned int size;
  unsigned int offset;
}
board_list[MAX_BOARDS];

static unsigned short fgetus(FILE *fp)
{
  return (fgetc(fp) << 0) | (fgetc(fp) << 8);
}

static unsigned int fgetud(FILE *fp)
{
  return (fgetc(fp) << 0)  | (fgetc(fp) << 8) |
         (fgetc(fp) << 16) | (fgetc(fp) << 24);
}

static void fputus(unsigned short src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc(src >> 8, fp);
}

static void fputud(unsigned int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc((src >> 8) & 0xFF, fp);
  fputc((src >> 16) & 0xFF, fp);
  fputc((src >> 24) & 0xFF, fp);
}

static enum status convert_283_to_282_board(FILE *fp, int *delta)
{
  char *buffer, board_mod[MAX_PATH];
  int i, byte, skip_rle_blocks = 6;
  unsigned short board_mod_len;
  long file_pos, copy_len;

  memset(board_mod, 0, MAX_PATH);

  if(fgetc(fp) < 0)
    return READ_ERROR;

  byte = fgetc(fp);
  if(byte < 0)
    return READ_ERROR;

  if(byte)
  {
    if(fseek(fp, -1, SEEK_CUR) != 0)
      return SEEK_ERROR;
  }
  else
  {
    if(fgetc(fp) < 0)
      return READ_ERROR;
    skip_rle_blocks += 2;
  }

  for(i = 0; i < skip_rle_blocks; i++)
  {
    unsigned short w = fgetus(fp);
    unsigned short h = fgetus(fp);
    int pos = 0;

    while(pos < w * h)
    {
      unsigned char c = (unsigned char)fgetc(fp);
      if(!(c & 0x80))
        pos++;
      else
      {
        c &= ~0x80;
        pos += c;
        fgetc(fp);
      }
    }
  }

  /* Read in the new format board MOD */
  board_mod_len = fgetus(fp);
  if(board_mod_len > 0)
  {
    if(fread(board_mod, 1, board_mod_len, fp) != board_mod_len)
      return READ_ERROR;
  }

  /* <=2.82's board format is almost entirely different for
   * the next ~200 bytes. Read the entire file in as-is,
   * then re-sequence it and write back out.
   */
  file_pos = ftell(fp);
  if(file_pos < 0)
    return SEEK_ERROR;

  if(fseek(fp, 0, SEEK_END) != 0)
    return SEEK_ERROR;

  copy_len = ftell(fp);
  if(copy_len < 0)
    return SEEK_ERROR;

  if(fseek(fp, file_pos, SEEK_SET) != 0)
    return SEEK_ERROR;

  copy_len -= file_pos;
  buffer = malloc(copy_len);

  if(fread(buffer, 1, copy_len, fp) != (size_t)copy_len)
    return READ_ERROR;

  /* Roll back to before the board MOD was read in */
  if(fseek(fp, file_pos - (board_mod_len + 2), SEEK_SET) != 0)
    return SEEK_ERROR;

  /* The file's original contents are buffered, now
   * do the resequencing..
   */

  /* Write out the board MOD in the legacy format */
  board_mod[13] = 0;
  if(fwrite(board_mod, 1, 13, fp) != 13)
    return WRITE_ERROR;

  /* Next 22 bytes are unchanged between 2.82 & 2.83 */
  if(fwrite(&buffer[0], 1, 22, fp) != 22)
    return WRITE_ERROR;

  /* Fabricate last_key, num_input, input_size, input_string. */
  for(i = 0; i < 1 + 2 + 1 + 81; i++)
    if(fputc(0, fp) == EOF)
      return WRITE_ERROR;

  /* Fabricate player_last_dir */
  if(fputc(0x10, fp) == EOF)
    return WRITE_ERROR;

  /* Fabricate bottom_mesg, b_mesg_timer */
  for(i = 0; i < 81 + 1; i++)
    if(fputc(0, fp) == EOF)
      return WRITE_ERROR;

  /* Fabricate lazwall_start */
  if(fputc(7, fp) == EOF)
    return WRITE_ERROR;

  /* Fabricate b_mesg_row */
  if(fputc(24, fp) == EOF)
    return WRITE_ERROR;

  /* Fabricate b_mesg_col */
  if(fputc((unsigned char)-1, fp) == EOF)
    return WRITE_ERROR;

  /* Fabricate scroll_x, scroll_y */
  for(i = 0; i < 1 + 1; i++)
    fputus(0, fp);

  /* Fabricate locked_x, locked_y */
  for(i = 0; i < 1 + 1; i++)
    fputus((unsigned short)-1, fp);

  /* Next 3 bytes are unchanged. */
  if(fwrite(&buffer[22], 1, 3, fp) != 3)
    return WRITE_ERROR;

  /* Fabricate volume */
  if(fputc(255, fp) == EOF)
    return WRITE_ERROR;

  /* Fabricate volume_inc */
  if(fputc(0, fp) == EOF)
    return WRITE_ERROR;

  /* Fabricate volume_target */
  if(fputc(255, fp) == EOF)
    return WRITE_ERROR;

  /* Finally, write out rest of buffer */
  if(fwrite(&buffer[25], 1, copy_len - 25, fp) != (size_t)copy_len - 25)
    return WRITE_ERROR;

  /* The world is now some bytes longer/shorter */
  if(delta)
    *delta = (13 - board_mod_len - 2) + 182;
  return SUCCESS;
}

static enum status convert_283_to_282(FILE *fp)
{
  int num_boards, i, total_delta = 0;
  unsigned int global_robot_offset;
  long board_pointer_pos;

  if(fseek(fp, WORLD_GLOBAL_OFFSET_OFFSET, SEEK_SET) != 0)
    return SEEK_ERROR;

  global_robot_offset = fgetud(fp);

  num_boards = fgetc(fp);
  if(num_boards < 0)
    return READ_ERROR;

  if(num_boards == 0)
  {
    unsigned short sfx_len = fgetus(fp);
    if(fseek(fp, sfx_len, SEEK_CUR) != 0)
      return SEEK_ERROR;

    num_boards = fgetc(fp);
    if(num_boards < 0)
      return READ_ERROR;
  }

  if(fseek(fp, num_boards * BOARD_NAME_SIZE, SEEK_CUR) != 0)
    return SEEK_ERROR;

  board_pointer_pos = ftell(fp);
  if(board_pointer_pos < 0)
    return READ_ERROR;

  for(i = 0; i < num_boards; i++)
  {
    board_list[i].size = fgetud(fp);
    board_list[i].offset = fgetud(fp);
  }

  for(i = 0; i < num_boards; i++)
  {
    struct board_entry *board = &board_list[i];
    enum status ret;
    int delta;

    if(board->size == 0)
      continue;

    board->offset += total_delta;
    if(fseek(fp, board->offset, SEEK_SET) != 0)
      return SEEK_ERROR;

    ret = convert_283_to_282_board(fp, &delta);
    if(ret != SUCCESS)
      return ret;

    board->size += delta;
    total_delta += delta;
  }

  if(fseek(fp, WORLD_GLOBAL_OFFSET_OFFSET, SEEK_SET) != 0)
    return SEEK_ERROR;

  global_robot_offset += total_delta;
  fputud(global_robot_offset, fp);

  if(fseek(fp, board_pointer_pos, SEEK_SET) != 0)
    return SEEK_ERROR;

  for(i = 0; i < num_boards; i++)
  {
    fputud(board_list[i].size, fp);
    fputud(board_list[i].offset, fp);
  }

  return SUCCESS;
}

#endif // WORLD_VERSION == 0x0253

int main(int argc, char *argv[])
{
  int world, byte;
  long ext_pos;
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

  fp = fopen_unsafe(argv[1], "r+b");
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

  if(fseek(fp, -2, SEEK_CUR) != 0)
    goto err_seek;

  byte = fputc(WORLD_VERSION_PREV_HI, fp);
  if(byte == EOF)
    goto err_write;

  byte = fputc(WORLD_VERSION_PREV_LO, fp);
  if(byte == EOF)
    goto err_write;

#if WORLD_VERSION == 0x0253
  {
    enum status ret;

    if(world)
      ret = convert_283_to_282(fp);
    else
      ret = convert_283_to_282_board(fp, NULL);

    switch(ret)
    {
      case SEEK_ERROR:  goto err_seek;
      case READ_ERROR:  goto err_read;
      case WRITE_ERROR: goto err_write;
      case SUCCESS:     break;
    }
  }
#endif

  fprintf(stdout, "File '%s' successfully downgraded from %d.%d to %d.%d.\n",
   argv[1], WORLD_VERSION_HI, WORLD_VERSION_LO,
   WORLD_VERSION_PREV_HI, WORLD_VERSION_PREV_LO);

exit_close:
  fclose(fp);
exit_out:
  return 0;

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
