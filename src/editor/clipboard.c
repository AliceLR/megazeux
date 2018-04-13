/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017 Ian Burgmyer <spectere@gmail.com>
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

// OS-specific clipboard functions.

#include "clipboard.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#if defined(CONFIG_SDL)
#include "SDL.h"
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
#include "clipboard_sdl2.h"
#endif

#if defined(CONFIG_X11) && !SDL_VERSION_ATLEAST(2,0,0)
#include "clipboard_x11.h"
#endif

#if defined(SDL_VIDEO_DRIVER_QUARTZ)
#include "clipboard_carbon.h"
#endif

#ifdef __WIN32__
#include "clipboard_win32.h"
#endif

static void (* clipboard_set)(char **buffer, int lines, int total_length) = NULL;
static char* (* clipboard_get)(void) = NULL;

void clipboard_init(void)
{
  // Pick the ideal clipboard for the given system.
#ifdef __WIN32__
  // MSVC flips out if you use SDL2, but MZX's Win32 clipboard implementation
  // is solid, so we'll just use that instead.
  clipboard_set = clipboard_set_win32;
  clipboard_get = clipboard_get_win32;
  debug("clipboard: using win32 handler\n");
#elif SDL_VERSION_ATLEAST(2,0,0)
  // Most platforms should use the SDL2 clipboard handler.
  clipboard_set = clipboard_set_sdl2;
  clipboard_get = clipboard_get_sdl2;
  debug("clipboard: using sdl2 handler\n");
#elif defined(CONFIG_X11)
  // X11 on SDL1.
  clipboard_set = clipboard_set_x11;
  clipboard_get = clipboard_get_x11;
  debug("clipboard: using x11 handler\n");
#elif defined(SDL_VIDEO_DRIVER_QUARTZ)
  // OS X on SDL1 (this will probably only be used on PPC).
  clipboard_set = clipboard_set_carbon;
  clipboard_get = clipboard_get_carbon;
  debug("clipboard: using carbon handler\n");
#else
  debug("clipboard: using null handler\n");
#endif
}

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  if(clipboard_set)
    clipboard_set(buffer, lines, total_length);
}

char *get_clipboard_buffer(void)
{
  if(clipboard_get)
    return clipboard_get();
  else
    return NULL;
}
