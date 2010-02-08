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

// Call for an error OR a warning. Type=0 for a warning, 1 for a recoverable
// error, 2 for a fatal error. Options are (bits set in options and returned
// as action) FAIL=1, RETRY=2, EXIT TO DOS=4, OK=8 (OK is for usually only
// for warnings) Type = 3 for a critical error

int error(const char *string, char type, char options, unsigned int code)
{
  int t1 = 9, ret = 0;
  int fade_status = get_fade_status();
  char temp[5];

  // Window
  set_context(code >> 8);
  m_hide();
  save_screen();

  if(fade_status)
  {
    clear_screen(32, 7);
    insta_fadein();
  }

  draw_window_box(1, 10, 78, 14, 76, 64, 72, 1, 1);
  // Add title and error name

  switch(type)
  {
    case 0:
      write_string(" WARNING: ", 35, 10, 78, 0);
      break;
    case 1:
      write_string(" ERROR: ", 36, 10, 78, 0);
      break;
    case 2:
      write_string(" FATAL ERROR: ",33,10,78, 0);
      break;
    case 3:
      write_string(" CRITICAL ERROR: ",32,10,78, 0);
      break;
  }

  write_string(string, 40 - (strlen(string) / 2), 11, 79, 0);

  // Add options
  write_string("Press", 4, 13, 78, 0);

  if(options & 1)
  {
    write_string(", F for Fail", t1, 13, 78, 0);
    t1 += 12;
  }
  if(options & 2)
  {
    write_string(", R to Retry", t1, 13, 78, 0);
    t1 += 12;
  }
  if(options & 4)
  {
    write_string(", E to Exit to Dos", t1, 13, 78, 0);
    t1 += 18;
  }
  if(options & 8)
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
    write_string(temp, 46 - strlen(temp), 14, 64, 0);
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
        if(!(options & 1)) break;
        ret = 1;
        break;
      case IKEY_r:
        retry:
        if(!(options & 2)) break;
        ret = 2;
        break;
      case IKEY_e:
        exit:
        if(!(options & 4)) break;
        ret = 4;
        break;
      case IKEY_o:
        ok:
        if(!(options & 8)) break;
        ret = 8;
        break;
      case IKEY_h:
        if(!(options & 16)) break;
        // Call help
        break;
      case IKEY_ESCAPE:
        // Escape. Order of options this applies to-
        // Fail, Ok, Retry, Exit.
        if(options & 1) goto fail;
        if(options & 8) goto ok;
        if(options & 2) goto retry;
        goto exit;
      case IKEY_RETURN:
        // Enter. Order of options this applies to-
        // OK, Retry, Fail, Exit.
        if(options & 8) goto ok;
        if(options & 2) goto retry;
        if(options & 1) goto fail;
        goto exit;
    }
  } while(ret == 0);

  pop_context();

  // Restore screen and exit appropriately
  if(fade_status)
    insta_fadeout();

  restore_screen();
  m_show();
  if(ret == 4) // Exit the program
  {
    platform_quit();
    exit(-1);
  }

  return ret;
}
