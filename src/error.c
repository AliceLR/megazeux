/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

// Error type names by type code:
static const char *const error_type_names[] =
{
  " WARNING: ",		// 0
  " ERROR: ",		// 1
  " FATAL ERROR: ",	// 2
  " CRITICAL ERROR: ",	// 3
};


// Call for an error OR a warning. Type=0 for a warning, 1 for a recoverable
// error, 2 for a fatal error. Options are (bits set in options and returned
// as action) FAIL=1, RETRY=2, EXIT TO DOS=4, OK=8 (OK is for usually only
// for warnings) Type = 3 for a critical error

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
    fprintf(stderr, "%s%s\n", type_name, string);
    exit(-1);
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
    wait_event();
    t1 = get_key(keycode_internal);

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
