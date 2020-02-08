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
#include "platform.h"
#include "util.h"

#include "SDL.h"

#ifdef CONFIG_PSP
#include <psppower.h>
#endif

#ifdef CONFIG_GP2X
#include <unistd.h> //for chdir, execl
#endif

#ifdef CONFIG_WII
#include <sys/iosupport.h>
#include <fat.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
void delay(Uint32 ms)
{
  emscripten_sleep(ms);
}
#else
void delay(Uint32 ms)
{
  SDL_Delay(ms);
}
#endif

Uint32 get_ticks(void)
{
  return SDL_GetTicks();
}

boolean platform_init(void)
{
  Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;

#if SDL_VERSION_ATLEAST(2,0,0)
  flags |= SDL_INIT_GAMECONTROLLER;
#endif

#ifdef CONFIG_PSP
  scePowerSetClockFrequency(333, 333, 166);
#endif

#ifdef CONFIG_WII
  if(!fatInitDefault())
    return false;
#endif

#ifdef DEBUG
  flags |= SDL_INIT_NOPARACHUTE;
#endif

#ifdef CONFIG_AUDIO
  flags |= SDL_INIT_AUDIO;
#endif

  if(SDL_Init(flags) < 0)
  {
    debug("Failed to initialize SDL; attempting with joystick support disabled: %s\n", SDL_GetError());

    // try again without joystick support
    flags &= ~SDL_INIT_JOYSTICK;
#if SDL_VERSION_ATLEAST(2,0,0)
    flags &= ~SDL_INIT_GAMECONTROLLER;
#endif

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

#ifdef CONFIG_WII
  {
    int i;

    for(i = 0; i < STD_MAX; i++)
      if(devoptab_list[i] && devoptab_list[i]->chdir_r)
        fatUnmount(devoptab_list[i]->name);
  }
#endif

#ifdef CONFIG_GP2X
  chdir("/usr/gp2x");
  execl("/usr/gp2x/gp2xmenu", "/usr/gp2x/gp2xmenu", NULL);
#endif
}
