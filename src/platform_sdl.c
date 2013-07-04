/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2008 Simon Parzer <simon.parzer@gmail.com>
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

#include "compat.h"

#ifdef CONFIG_PSP
#define _COMPILING_NEWLIB
#endif

#include "platform.h"
#include "util.h"

#include "SDL.h"

#ifdef CONFIG_PSP
#include <pspsdk.h>
#include <psppower.h>
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
#endif

#ifdef CONFIG_GP2X
#include <unistd.h> //for chdir, execl
#endif

void delay(Uint32 ms)
{
  SDL_Delay(ms);
}

Uint32 get_ticks(void)
{
  return SDL_GetTicks();
}

bool platform_init(void)
{
  Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;

#ifdef CONFIG_PSP
  scePowerSetClockFrequency(333, 333, 166);
#endif

#ifdef DEBUG
  flags |= SDL_INIT_NOPARACHUTE;
#endif

#ifdef CONFIG_AUDIO
  flags |= SDL_INIT_AUDIO;
#endif

  if(SDL_Init(flags) < 0)
  {
    // try again without joystick support
    flags &= ~SDL_INIT_JOYSTICK;

    if(SDL_Init(flags) < 0)
    {
      warn("Failed to initialize SDL: %s\n", SDL_GetError());
      return false;
    }
  }

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_StartTextInput();
#else
  SDL_EnableUNICODE(1);
#endif
  return true;
}

void platform_quit(void)
{
  SDL_Quit();

#ifdef CONFIG_GP2X
  chdir("/usr/gp2x");
  execl("/usr/gp2x/gp2xmenu", "/usr/gp2x/gp2xmenu", NULL);
#endif
}

#ifdef CONFIG_PSP
int _isatty(int fd)
{
  return isatty(fd);
}
#endif
