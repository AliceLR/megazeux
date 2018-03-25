/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2017 Alice Rowan (petrifiedrowan@gmail.com)
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
// Error code

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "platform.h"
#include "helpsys.h"
#include "error.h"
#include "window.h"
#include "graphics.h"
#include "event.h"
#include "world.h"

// Error type names by type code:
static const char *const error_type_names[] =
{
  " WARNING: ",		// 0
  " ERROR: ",		// 1
  " FATAL ERROR: ",	// 2
  " CRITICAL ERROR: ",	// 3
};


//Call for an error OR a warning. Type=0 for a warning, 1 for a recoverable
//error, 2 for a fatal error. Options are (bits set in options and returned
//as action) FAIL=1, RETRY=2, EXIT TO DOS=4, OK=8, HELP=16 (OK is for usually
//only for warnings) Code is a specialized error code for debugging purposes.
//Type of 3 for a critical error.

// SUPPRESS=32 should only be used in conjunction with error_message().

int error(const char *string, unsigned int type, unsigned int options,
 unsigned int code)
{
  const char *type_name;
  int t1 = 9, ret = 0;
  char temp[5];
  int x;

  // Find the name of this error type.
  if(type >= sizeof(error_type_names) / sizeof(*error_type_names))
    type = 0;

  type_name = error_type_names[type];

  // If graphics couldn't initialize, print the error to stderr and abort.
  if(!has_video_initialized())
  {
    int scode = code ? (int)code : -1;

    fprintf(stderr, "%s%s\n", type_name, string);

    // Attempt to automatically handle this error if possible.
    if(options & ERROR_OPT_EXIT) exit(scode);
    if(options & ERROR_OPT_SUPPRESS) return ERROR_OPT_SUPPRESS;
    if(options & ERROR_OPT_OK) return ERROR_OPT_OK;
    if(options & ERROR_OPT_FAIL) return ERROR_OPT_FAIL;
    exit(scode);
  }

  // Window
  set_context(code >> 8);
  m_hide();
  save_screen();

  dialog_fadein();

  draw_window_box(1, 10, 78, 14, 76, 64, 72, 1, 1);
  // Add title and error name
  x = 40 - (int)strlen(type_name)/2;
  write_string(type_name, x, 10, 78, 0);

  write_string(string, 40 - ((Uint32)strlen(string) / 2), 11, 79, 0);

  // Add options
  write_string("Press", 4, 13, 78, 0);

  if(options & ERROR_OPT_FAIL)
  {
    write_string(", F for Fail", t1, 13, 78, 0);
    t1 += 12;
  }
  if(options & ERROR_OPT_RETRY)
  {
    write_string(", R to Retry", t1, 13, 78, 0);
    t1 += 12;
  }
  if(options & ERROR_OPT_EXIT)
  {
    write_string(", E to Exit", t1, 13, 78, 0);
    t1 += 11;
  }
  if(options & ERROR_OPT_OK)
  {
    write_string(", O for OK", t1, 13, 78, 0);
    t1 += 10;
  }
  if(options & ERROR_OPT_SUPPRESS)
  {
    write_string(", S for Suppress", t1, 13, 78, 0);
    t1 += 16;
  }

  draw_char('.', 78, t1, 13);
  draw_char(':', 78, 9, 13);

  // Add code if not 0

  if(code != 0)
  {
    write_string(" Debug code:0000h ", 30, 14, 64, 0);
    sprintf(temp, "%x", code);
    write_string(temp, 46 - (Uint32)strlen(temp), 14, 64, 0);
  }

  update_screen();

  // Get key
  do
  {
    wait_event(0);
    t1 = get_key(keycode_internal_wrt_numlock);

    //Exit event--mimic Escape
    if(get_exit_status())
      t1 = IKEY_ESCAPE;

    //Process
    switch(t1)
    {
      case IKEY_f:
        fail:
        if(!(options & ERROR_OPT_FAIL)) break;
        ret = ERROR_OPT_FAIL;
        break;
      case IKEY_r:
        retry:
        if(!(options & ERROR_OPT_RETRY)) break;
        ret = ERROR_OPT_RETRY;
        break;
      case IKEY_e:
        exit:
        if(!(options & ERROR_OPT_EXIT)) break;
        ret = ERROR_OPT_EXIT;
        break;
      case IKEY_o:
        ok:
        if(!(options & ERROR_OPT_OK)) break;
        ret = ERROR_OPT_OK;
        break;
      case IKEY_s:
        if(!(options & ERROR_OPT_SUPPRESS)) break;
        ret = ERROR_OPT_SUPPRESS;
        break;
      case IKEY_h:
        if(!(options & ERROR_OPT_HELP)) break;
        // Call help
        break;
      case IKEY_ESCAPE:
        // Escape. Order of options this applies to-
        // Fail, Ok, Retry, Exit.
        if(options & ERROR_OPT_FAIL ) goto fail;
        if(options & ERROR_OPT_OK   ) goto ok;
        if(options & ERROR_OPT_RETRY) goto retry;
        goto exit;
      case IKEY_RETURN:
        // Enter. Order of options this applies to-
        // OK, Retry, Fail, Exit.
        if(options & ERROR_OPT_OK   ) goto ok;
        if(options & ERROR_OPT_RETRY) goto retry;
        if(options & ERROR_OPT_FAIL ) goto fail;
        goto exit;
    }
  } while(ret == 0);

  pop_context();

  // Restore screen and exit appropriately
  dialog_fadeout();

  restore_screen();
  m_show();
  if(ret == ERROR_OPT_EXIT) // Exit the program
  {
    platform_quit();
    exit(-1);
  }

  return ret;
}


static char suppress_errors[NUM_ERROR_CODES];
static int error_count = 0;

// Wrapper for error().
int error_message(enum error_code id, int parameter, const char *string)
{
  char error_mesg[80];
  char version_string[16];
  int hi = (parameter & 0xFF00) >> 8;
  int lo = (parameter & 0xFF);
  int opts = ERROR_OPT_OK | ERROR_OPT_SUPPRESS;
  int severity = 1; // ERROR
  int code = id;
  int result;

  get_version_string(version_string, parameter);
  error_count++;

  switch (id)
  {
    case E_FILE_DOES_NOT_EXIST:
      sprintf(error_mesg, "File doesn't exist");
      break;

    case E_SAVE_FILE_INVALID:
      sprintf(error_mesg, "File is not a valid .SAV file or is corrupt");
      code = 0x2101;
      break;

    case E_SAVE_VERSION_OLD:
      sprintf(error_mesg,
       ".SAV files from older versions of MZX (%s) are not supported",
       version_string);
      code = 0x2101;
      break;

    case E_SAVE_VERSION_TOO_RECENT:
      sprintf(error_mesg,
       ".SAV files from newer versions of MZX (%s) are not supported",
       version_string);
      code = 0x2101;
      break;

    case E_WORLD_FILE_INVALID:
      sprintf(error_mesg, "File is not a valid world file or is corrupt");
      code = 0x0D02;
      break;

    case E_WORLD_FILE_VERSION_OLD:
      sprintf(error_mesg,
       "World is from old version (%s); use converter",
       version_string);
      code = 0x0D02;
      break;

    case E_WORLD_FILE_VERSION_TOO_RECENT:
      sprintf(error_mesg,
       "World is from a more recent version (%s)",
       version_string);
      code = 0x0D02;
      break;

    case E_WORLD_PASSWORD_PROTECTED:
      sprintf(error_mesg, "This world may be password protected");
      code = 0x0D02;
      break;

    case E_WORLD_DECRYPT_WRITE_PROTECTED:
      sprintf(error_mesg, "Cannot decrypt write-protected world; check permissions");
      code = 0x0DD5;
      break;

    case E_WORLD_LOCKED:
      sprintf(error_mesg, "Cannot load password protected world");
      code = 0x0D02;
      break;

    case E_WORLD_IO_POST_VALIDATION:
      sprintf(error_mesg, "Post validation IO error occurred");
      code = 0x0D01;
      break;

    case E_WORLD_IO_SAVING:
      sprintf(error_mesg, "Error saving; file/directory may be write protected");
      code = 0x0D01;
      break;

    case E_WORLD_BOARD_MISSING:
      sprintf(error_mesg,
       "Board @ %Xh could not be found", parameter);
      code = 0x0D03;
      break;

    case E_WORLD_BOARD_CORRUPT:
      sprintf(error_mesg,
       "Board @ %Xh is irrecoverably truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_WORLD_BOARD_TRUNCATED_SAFE:
      sprintf(error_mesg,
       "Board @ %Xh is truncated, but could be partially recovered", parameter);
      code = 0x0D03;
      break;

    case E_WORLD_ROBOT_MISSING:
      sprintf(error_mesg, "Robot @ %Xh could not be found", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_FILE_INVALID:
      sprintf(error_mesg, "File is not a board file or is corrupt");
      code = 0x4040;
      break;

    case E_BOARD_FILE_FUTURE_VERSION:
      sprintf(error_mesg, "Board file is from a future version (%s)",
       version_string);
      code = 0x4041;
      break;

    case E_BOARD_ROBOT_CORRUPT:
      sprintf(error_mesg, "Robot @ %Xh is truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_SCROLL_CORRUPT:
      sprintf(error_mesg, "Scroll @ %Xh is truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_SENSOR_CORRUPT:
      sprintf(error_mesg, "Sensor @ %Xh is truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_SUMMARY:
      sprintf(error_mesg, "Board @ %Xh summary", parameter);
      code = 0x4041;
      break;

    case E_MZM_DOES_NOT_EXIST:
      sprintf(error_mesg, "MZM doesn't exist");
      code = 0x0D01;
      break;

    case E_MZM_FILE_INVALID:
      sprintf(error_mesg, "File is not an MZM or is corrupt");
      code = 0x6660;
      break;

    case E_MZM_FILE_FROM_SAVEGAME:
      sprintf(error_mesg, "MZM contains runtime robots; dummying out");
      code = 0x6661;
      break;

    case E_MZM_FILE_VERSION_TOO_RECENT:
      sprintf(error_mesg,
       "MZM from newer version (%s); dummying out robots",
       version_string);
      code = 0x6661;
      break;

    case E_MZM_ROBOT_CORRUPT:
      sprintf(error_mesg, "MZM is missing robots or contains corrupt robots");
      code = 0x6662;
      break;

    case E_LOAD_BC_CORRUPT:
      sprintf(error_mesg, "Bytecode file failed validation check");
      code = 0xD0D0;
      break;
    
    case E_NO_LAYER_RENDERER:
      sprintf(error_mesg, "Current renderer lacks advanced graphical features; features disabled");
      code = 0x2563;
      break;

    case E_ZIP_BOARD_CORRUPT:
      sprintf(error_mesg, "Board # %d is corrupt", lo);
      code = 0x9000;
      break;

    case E_ZIP_BOARD_MISSING_DATA:
      sprintf(error_mesg, "Board # %d is missing data:", lo);
      code = 0x9001;
      break;

    case E_ZIP_ROBOT_CORRUPT:
      sprintf(error_mesg, "Robot # %d on board # %d is corrupt", lo, hi);
      code = 0x9002;
      break;

    case E_ZIP_SCROLL_CORRUPT:
      sprintf(error_mesg, "Scroll # %d on board # %d is corrupt", lo, hi);
      code = 0x9003;
      break;

    case E_ZIP_SENSOR_CORRUPT:
      sprintf(error_mesg, "Sensor # %d on board # %d is corrupt", lo, hi);
      code = 0x9004;
      break;

    case E_ZIP_ROBOT_MISSING_FROM_BOARD:
      sprintf(error_mesg, "Robot # %d does not exist on board # %d", lo, hi);
      code = 0x9005;
      break;

    case E_ZIP_ROBOT_MISSING_FROM_DATA:
      sprintf(error_mesg,
       "Robot # %d exists on board # %d, but was not found", lo, hi);
      code = 0x9006;
      break;

    case E_ZIP_ROBOT_DUPLICATED:
      sprintf(error_mesg,
       "Robot # %d contains duplicates on board # %d", lo, hi);
      code = 0x9007;
      break;

#ifdef CONFIG_DEBYTECODE
    case E_DBC_WORLD_OVERWRITE_OLD:
      sprintf(error_mesg,
       "Save would overwrite older version (%s); aborted",
       version_string);
      code = 0x0fac;
      break;

    case E_DBC_SAVE_ROBOT_UNSUPPORTED:
      sprintf(error_mesg,
       "SAVE_ROBOT and SAVE_BC are no longer supported");
      code = 0x0fac;
      break;
#endif

    case E_ZIP:
      code = 0xEEEE;
      /* fallthrough */

    case E_DEFAULT:
    default:
      snprintf(error_mesg, 79, "%s", string);
      string = NULL;
      break;
  }

  // This needs to happen after the switch, unfortunately, since we need to
  // check the options. Suppress errors that allow OK only.
  if(suppress_errors[id] && (opts & ERROR_OPT_OK))
    return ERROR_OPT_OK;

  if(string)
  {
    int offset = strlen(error_mesg);
    int left = 79 - offset;
    if(left > 0)
    {
      snprintf(error_mesg + offset, left, ": %s", string);
    }
  }

  result = error(error_mesg, severity, opts, code);

  if(result == ERROR_OPT_SUPPRESS)
  {
    suppress_errors[id] = 1;
    return ERROR_OPT_OK;
  }

  return result;
}



int get_and_reset_error_count(void)
{
  int count = error_count;
  error_count = 0;
  return count;
}

void set_error_suppression(enum error_code id, int value)
{
  suppress_errors[id] = (char)value;
}

void reset_error_suppression(void)
{
  int i;
  for (i = 0; i < NUM_ERROR_CODES; i++)
    suppress_errors[i] = 0;
}
