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

#include <SDL.h>

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
void delay(uint32_t ms)
{
  emscripten_sleep(ms);
}
#else
void delay(uint32_t ms)
{
  SDL_Delay(ms);
}
#endif

uint64_t get_ticks(void)
{
  return SDL_GetTicks();
}

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/**
 * Set DPI awareness for Windows to avoid bad window scaling and various
 * fullscreen-related bugs. This function was added in Windows Vista so it
 * needs to be dynamically loaded.
 */
static void set_dpi_aware(void)
{
  BOOL (*_SetProcessDPIAware)(void) = NULL;
  void *handle;

  handle = SDL_LoadObject("User32.dll");
  if(handle)
  {
    void **dest = (void **)&_SetProcessDPIAware;
    *dest = SDL_LoadFunction(handle, "SetProcessDPIAware");

    if(_SetProcessDPIAware && !_SetProcessDPIAware())
    {
      warn("failed to SetProcessDPIAware!\n");
    }
    else

    if(!_SetProcessDPIAware)
      debug("couldn't load SetProcessDPIAware.\n");

    SDL_UnloadObject(handle);
  }
  else
    debug("couldn't load User32.dll: %s\n", SDL_GetError());
}
#endif

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

#ifdef __WIN32__
  set_dpi_aware();
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
/**
 * TODO: Don't enable SDL_TEXTINPUT events on Android for now. They seem to
 * cause more issues than they solve. Enabling these on Android also seems
 * to assume that the onscreen keyboard should be opened, which usually
 * covers whatever part of the MZX screen is being typed into.
 */
#if !defined(ANDROID)
  SDL_StartTextInput();
#endif
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
