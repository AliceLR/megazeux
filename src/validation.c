/* MegaZeux
 *
 * Copyright (C) 2012 Alice Lauren Rowan <petrifiedrowan@gmail.com>
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

/*********************************
 * LEGACY FILE FORMAT VALIDATION *
 *********************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

#include "validation.h"
#include "world.h"
#include "error.h"
#include "window.h"
#include "const.h"
#include "util.h"
#include "legacy_rasm.h"

/****************************
 * LEGACY WORLD FORMAT INFO *
 ****************************/

/* world:
 * 25 name
 * 1  protected?
 * 3  magic
 *
 * 4129 block 1
 * 3584 char set
 *  455 charid table
 *   90 status counters
 *
 *   72 block 2
 *   24 global info settings
 *   48 palette
 *
 *@4230:
 * ???? block 3 -- board and robot data, and custom sfx
 *    4 global robot offset
 *+   1 number of boards?  if that byte was actually 0:
 *    {
 *      2 sfx size
 *+    50 sfx strings up to 68 chars long each
 *      1 number of boards
 *    }
 *+  25 times the number of boards -- board names
 *    8 times the number of boards -- size, offset
 */

/* save (2.84):
 *    5 save magic
 *    2 world magic
 *    1 current board
 *
 * (block 1)
 *
 *+  73 save block 1
 *   71 keys, potion timers, save player position data, misc
 *+   2 real_mod_playing, up to 514
 *
 * (block 2)
 *
 *+7508 save block 2
 *   24 intensity, faded, coords, player under
 *+   4 counters, up to 2^31 * counter size
 *+   4 strings, up to 2^31 * max string size
 * 4612 sprites
 *(4096   data
 *    4   info
 *  512   collision list)
 *   12 misc settings
 *+   2 fread filename, up to 514
 *    4 fread pos
 *+   2 fwrite filename, up to 514
 *    4 fwrite pos
 *    2 screen mode
 *  768 smzx palette
 *    4 commands
 *+   8 vlayer size, w, h; then data: vlayer size (up to 2^31) * 2
 *
 * (block 3)
 */

#ifndef WORLD_GLOBAL_OFFSET_OFFSET
#define WORLD_GLOBAL_OFFSET_OFFSET 4230
#define WORLD_BLOCK_1_SIZE 4129
#define WORLD_BLOCK_2_SIZE 72
#endif

int suppress_errors = -1;



/* error messages */
void val_error(enum val_error error_id, int value)
{
  val_error_str(error_id, value, NULL);
}

void val_error_str(enum val_error error_id, int value, char *string)
{
  char error_mesg[80];
  int hi = (value & 0xFF00) >> 8, lo = (value & 0xFF);
  int opts = ERROR_OPT_OK;
  int severity = 1;
  int code = 0;

  switch (error_id)
  {
    case FILE_DOES_NOT_EXIST:
    {
      sprintf(error_mesg, "File doesn't exist!");
      code = 0x0D01;
      break;
    }
    case SAVE_FILE_INVALID:
    {
      sprintf(error_mesg, "File is not a valid .SAV file or is corrupt");
      code = 0x2101;
      break;
    }
    case SAVE_VERSION_OLD:
    {
      snprintf(error_mesg, 80,
       ".SAV files from older versions of MZX (%d.%d) are not supported", hi, lo);
      code = 0x2101;
      break;
    }
    case SAVE_VERSION_TOO_RECENT:
    {
      snprintf(error_mesg, 80,
       ".SAV files from newer versions of MZX (%d.%d) are not supported", hi, lo);
      code = 0x2101;
      break;
    }
    case WORLD_FILE_INVALID:
    {
      sprintf(error_mesg, "File is not a valid world file or is corrupt");
      code = 0x0D02;
      break;
    }
    case WORLD_FILE_VERSION_OLD:
    {
      snprintf(error_mesg, 80,
       "World is from old version (%d.%d); use converter", hi, lo);
      code = 0x0D02;
      break;
    }
    case WORLD_FILE_VERSION_TOO_RECENT:
    {
      snprintf(error_mesg, 80,
       "World is from a more recent version (%d.%d)", hi, lo);
      code = 0x0D02;
      break;
    }
    case WORLD_PASSWORD_PROTECTED:
    {
      sprintf(error_mesg, "This world may be password protected.");
      code = 0x0D02;
      break;
    }
    case WORLD_LOCKED:
    {
      sprintf(error_mesg, "Cannot load password protected world.");
      code = 0x0D02;
      break;
    }
    case WORLD_BOARD_MISSING:
    {
      snprintf(error_mesg, 80,
       "Board @ %Xh could not be found", value);
      code = 0x0D03;
      break;
    }
    case WORLD_BOARD_CORRUPT:
    {
      snprintf(error_mesg, 80,
       "Board @ %Xh is irrecoverably truncated or corrupt", value);
      code = 0x0D03;
      break;
    }
    case WORLD_BOARD_TRUNCATED_SAFE:
    {
      snprintf(error_mesg, 80,
       "Board @ %Xh is truncated, but could be partially recovered", value);
      code = 0x0D03;
      break;
    }
    case WORLD_ROBOT_MISSING:
    {
      snprintf(error_mesg, 80,
       "Robot @ %Xh could not be found", value);
      code = 0x0D03;
      break;
    }
    case BOARD_FILE_INVALID:
    {
      sprintf(error_mesg, "File is not a board file or is corrupt");
      code = 0x4040;
      break;
    }
    case BOARD_ROBOT_CORRUPT:
    case BOARD_SCROLL_CORRUPT:
    case BOARD_SENSOR_CORRUPT:
    {
      snprintf(error_mesg, 80,
       "Robot @ %Xh is truncated or corrupt", value);
      code = 0x0D03;
      break;
    }
    case MZM_FILE_INVALID:
    {
      sprintf(error_mesg, "File is not an MZM or is corrupt");
      code = 0x6660;
      break;
    }
    case MZM_FILE_FROM_SAVEGAME:
    {
      sprintf(error_mesg, "Runtime robots incompatible with editor -- robots will be dummied out");
      code = 0x6661;
      break;
    }
    case MZM_FILE_VERSION_TOO_RECENT:
    {
      snprintf(error_mesg, 80,
       "MZM file of a more recent version (%d.%d) -- robots will be dummied out", hi, lo);
      code = 0x6661;
      break;
    }
    case LOAD_BC_CORRUPT:
    {
      snprintf(error_mesg, 80,
       "Bytecode file '%s' failed validation check", string);
      // This can easily result in hard-to-escape loops
      opts |= ERROR_OPT_EXIT;
      code = 0xD0D0;
      break;
    }
  }

  if(severity > suppress_errors)
    error(error_mesg, 1, opts, code);
}

FILE * val_fopen(const char *filename)
{
  struct stat stat_result;
  int stat_op_result = stat(filename, &stat_result);
  FILE *f;

  if(stat_op_result)
    return NULL;

  if(!S_ISREG(stat_result.st_mode))
    return NULL;

  if(!(f = fopen_unsafe(filename, "rb")))
    return NULL;

  return f;
}

void set_validation_suppression(int level)
{
  suppress_errors = level;
}


/* This is a lot like try_load_world but much more thorough, and doesn't
 * pass data through or leave a file open.  This needs to be done before
 * any data is ever loaded, so that Megazeux can cleanly abort if there
 * is an issue.
 */
enum val_result validate_world_file(const char *filename,
 int savegame, int *end_of_global_offset, int decrypt_attempted)
{
  enum val_result result = VAL_SUCCESS;

  FILE *f;
  char magic[15];
  int num_boards;
  int board_name_offset;
  int v, i;

  /* TEST 1:  Make sure it's a readable file */
  if(!(f = val_fopen(filename)))
  {
    val_error(FILE_DOES_NOT_EXIST, 0);
    result = VAL_MISSING;
    goto err_out;
  }

  /* TEST 2:  Is it a save file? */
  if(savegame)
  {
    int screen_mode, num_counters, num_strings, len;

    if(fread(magic, 5, 1, f) != 1)
      goto err_invalid;

    v = save_magic(magic);
    if(!v)
      goto err_invalid;

    else if (v > WORLD_VERSION)
    {
      val_error(SAVE_VERSION_TOO_RECENT, v);
      result = VAL_VERSION;
      goto err_close;
    }
    else if (v < WORLD_VERSION)
    {
      val_error(SAVE_VERSION_OLD, v);
      result = VAL_VERSION;
      goto err_close;
    }

    /* TEST 3:  Check for truncation, savegame style, hope this
     * doesn't explode :erm:
     */
    if(
     fseek(f, 8, SEEK_SET) ||
     fseek(f, WORLD_BLOCK_1_SIZE, SEEK_CUR) ||
     fseek(f, 71, SEEK_CUR) ||
     fseek(f, (len = fgetw(f)), SEEK_CUR) ||
     (len < 0) ||
     fseek(f, WORLD_BLOCK_2_SIZE, SEEK_CUR) ||
     fseek(f, 24, SEEK_CUR))
    {
      debug("pre-counters\n");
      goto err_invalid;
    }

    //do counters - vvvvnnnn(name)
    num_counters = fgetd(f);
    if(num_counters < 0)
    {
      debug("counter num\n");
      goto err_invalid;
    }

    for(i = 0; i < num_counters; i++)
    {
      if(
       fseek(f, 4, SEEK_CUR) || //value
       fseek(f, (len = fgetd(f)), SEEK_CUR) ||
       (len < 0))
      {
        debug("counters\n");
        goto err_invalid;
      }
    }

    //do strings-   nnnnllll(name)(value)
    num_strings = fgetd(f);
    if(num_strings < 0)
    {
      debug("string num\n");
      goto err_invalid;
    }

    for(i = 0; i < num_strings; i++)
    {
      int name_length = fgetd(f);
      int value_length = fgetd(f);
      if(
       (name_length < 0) ||
       (value_length < 0) ||
       fseek(f, name_length, SEEK_CUR) ||
       fseek(f, value_length, SEEK_CUR))
      {
        debug("strings\n");
        goto err_invalid;
      }
    }

    if(
     fseek(f, 4612, SEEK_CUR) || //sprites
     fseek(f, 12, SEEK_CUR) || //misc
     fseek(f, fgetw(f), SEEK_CUR) || //fread_open
     fseek(f, 4, SEEK_CUR) || //fread_pos
     fseek(f, fgetw(f), SEEK_CUR) || //fwrite_open
     fseek(f, 4, SEEK_CUR)) //fwrite_pos
    {
      debug("post strings\n");
      goto err_invalid;
    }

    screen_mode = fgetw(f);
    if((screen_mode > 3) || (screen_mode > 1 &&
     fseek(f, 768, SEEK_CUR))) //smzx palette
    {
      debug("smzx palette\n");
      goto err_invalid;
    }

    if(
     fseek(f, 4, SEEK_CUR) || //commands
     ((len = fgetd(f)) < 0) || //vlayer size
     fseek(f, 4, SEEK_CUR) || // width & height
     fseek(f, len, SEEK_CUR) || //chars
     fseek(f, len, SEEK_CUR)) //colors
    {
      debug("vlayer\n");
      goto err_invalid;
    }

    /* now we should be at the global robot pointer! */
  }

  else /* !savegame */
  {
    int protection_method;

    /* TEST 3:  Check for truncation */
    if(fseek(f, WORLD_GLOBAL_OFFSET_OFFSET, SEEK_SET))
      goto err_invalid;

    fseek(f, BOARD_NAME_SIZE, SEEK_SET);

    /* TEST 4:  If we think it's locked, try to decrypt it. */
    protection_method = fgetc(f);
    if(protection_method > 0)
    {
      if(protection_method > 3)
        goto err_invalid;

      if(decrypt_attempted) // In the unlikely event that this will happen again
        goto err_invalid;

      val_error(WORLD_PASSWORD_PROTECTED, 0);

      if(!confirm(NULL, "Would you like to decrypt it?"))
      {
        result = VAL_NEED_UNLOCK;
        goto err_close;
      }
      else
      {
        val_error(WORLD_LOCKED, 0);
        result = VAL_ABORTED;
        goto err_close;
      }
    }

    /* TEST 5:  Make sure the magic is awwrightttt~ */
    fread(magic, 1, 3, f);

    v = world_magic(magic);
    if(v == 0)
      goto err_invalid;

    else if (v < 0x0205)
    {
      val_error(WORLD_FILE_VERSION_OLD, v);
      result = VAL_VERSION;
      goto err_close;
    }
    else if (v > WORLD_VERSION)
    {
      val_error(WORLD_FILE_VERSION_TOO_RECENT, v);
      result = VAL_VERSION;
      goto err_close;
    }

    /* TEST 6:  Attempt to eliminate invalid files by
     * testing the palette for impossible values.
     */
    fseek(f, WORLD_GLOBAL_OFFSET_OFFSET - 48, SEEK_SET);
    for(i = 0; i<48; i++)
    {
      int val = fgetc(f);
      if((val < 0) || (val > 63))
        goto err_invalid;
    }

    /* now we should be at the global robot pointer! */
  }

  /* TEST 7:  Either branch should be at the global robot pointer now.
   * Test for valid SFX structure, if applicable, and board information.
   */
  fseek(f, 4, SEEK_CUR);

  // Do the sfx
  num_boards = fgetc(f);
  if(num_boards == 0)
  {
    int sfx_size = fgetw(f);
    int sfx_off = ftell(f);

    for (i = 0; i < NUM_SFX; i++)
    {
      if(fseek(f, fgetc(f), SEEK_CUR))
        break;
    }

    if((i != NUM_SFX) || ((ftell(f) - sfx_off) != sfx_size))
      goto err_invalid;

    num_boards = fgetc(f);
  }
  if(num_boards == 0)
    goto err_invalid;

  board_name_offset = ftell(f);

  //Make sure board name and pointer data exists
  if(
   fseek(f, num_boards * BOARD_NAME_SIZE, SEEK_CUR) ||
   fseek(f, num_boards * 8, SEEK_CUR) ||
   ((ftell(f) - board_name_offset) != num_boards * (BOARD_NAME_SIZE + 8)))
    goto err_invalid;

  /* If any of the pointers are less than this pos we probably
   * aren't dealing with a valid world, but it's not our job
   * to figure that out right now, so we'll pass it back.
   */
  if(end_of_global_offset)
    *end_of_global_offset = ftell(f);

  //todo: maybe have a complete fail when N number of pointers fail?

  goto err_close;

err_invalid:
  result = VAL_INVALID;
  if(savegame)
    val_error(SAVE_FILE_INVALID, 0);
  else
    val_error(WORLD_FILE_INVALID, 0);
err_close:
  fclose(f);
err_out:
  return result;
}


// This is part of robot validation, don't give any messages.
/*enum val_result validate_legacy_bytecode(char *bc, int program_length)
{
  int i = 1;
  int cur_command_start, cur_command_length, cur_param_length;

  if(!bc)
    goto err_invalid;

  // First -- fix the odd robots that appear in old MZX games,
  // such as Catacombs of Zeux.
  if((program_length == 2) || (bc[0] != 0xFF))
  {
    bc[0] = 0xFF;
    bc[1] = 0x0;
  }

  if(bc[0] != 0xFF)
    goto err_invalid;

  // One iteration should be a single command.
  while(1)
  {
    cur_command_length = bc[i];
    i++;
    if(cur_command_length == 0)
      break;

    cur_command_start = i;

    if((i + cur_command_length) > program_length)
      goto err_invalid;

    if(bc[i + cur_command_length] != cur_command_length)
      goto err_invalid;

    i++;

    while(i < cur_command_start + cur_command_length)
    {
      cur_param_length = bc[i];
      if(cur_param_length == 0)
        cur_param_length = 2;

      i += cur_param_length + 1;
    }
    i++;

    if(i != cur_command_start + cur_command_length + 1)
      goto err_invalid;

    if(i > program_length)
      goto err_invalid;
  }

  if(i < program_length)
  {
    debug("Robot checked for %i but program length is %i; extra wiped\n",
     program_length, i);
    memset(bc + i, '\0', program_length-i);
  }

  if(i > program_length)
    goto err_invalid;

  return VAL_SUCCESS;

err_invalid:
  {
    int n;
    char hex_seg[4];
    char *err_mesg = cmalloc(sizeof(char) * ((cur_command_length + 2) * 3 + 2));
    err_mesg[0] = 0;

    for(n = cur_command_start - 1;
     n < (cur_command_start + cur_command_length + 1) &&
     n < program_length;
     n++)
    {
      snprintf(hex_seg, 4, "%X ", bc[n]);
      strcat(err_mesg, hex_seg);
    }

    debug("Prog len: %i    i: %i   bc[0]: %i   bc[1]: %i\n",
     program_length, i, bc[0], bc[1]);

    debug("Bytecode: %s\n\n", err_mesg);

    free(err_mesg);
  }

  return VAL_INVALID;
}*/
