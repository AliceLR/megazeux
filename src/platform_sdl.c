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

#include "SDLmzx.h"
#include "platform.h"
#include "util.h"

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

/* Verify that SDL's endianness matches what platform_endian.h claims... */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN && PLATFORM_BYTE_ORDER != PLATFORM_BIG_ENDIAN
#error Endian mismatch: SDL detected big, but MZX detected little. Report this!
#endif
#if SDL_BYTEORDER == SDL_LIL_ENDIAN && PLATFORM_BYTE_ORDER != PLATFORM_LIL_ENDIAN
#error Endian mismatch: SDL detected little, but MZX detected big. Report this!
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
  // SDL_GetTicks returns a 64-bit value in SDL3, but
  // SDL_GetTicks64 had to be used prior to that.
#if SDL_VERSION_ATLEAST(2,0,18) && !SDL_VERSION_ATLEAST(3,0,0)
  return SDL_GetTicks64();
#else
  return SDL_GetTicks();
#endif
}

#ifdef __WIN32__
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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
    union dso_fn_ptr_ptr sym_ptr = { &_SetProcessDPIAware };
    dso_fn **dest = sym_ptr.value;

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

static inline boolean sdl_init(Uint32 flags)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  return SDL_Init(flags);
#else
  return SDL_Init(flags) >= 0;
#endif
}

boolean platform_init(void)
{
  Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;

#if SDL_VERSION_ATLEAST(2,0,0)
  flags |= SDL_INIT_GAMEPAD;
#endif

#ifdef CONFIG_PSP
  scePowerSetClockFrequency(333, 333, 166);
#endif

#ifdef CONFIG_WII
  if(!fatInitDefault())
    return false;
#endif

#if defined(DEBUG) && !SDL_VERSION_ATLEAST(3,0,0)
  // Removed in SDL 3.
  flags |= SDL_INIT_NOPARACHUTE;
#endif

#ifdef CONFIG_AUDIO
  flags |= SDL_INIT_AUDIO;
#endif

#ifdef __WIN32__
  set_dpi_aware();
#endif

  if(!sdl_init(flags))
  {
    debug("Failed to initialize SDL; attempting with joystick support disabled: %s\n", SDL_GetError());

    // try again without joystick support
    flags &= ~SDL_INIT_JOYSTICK;
#if SDL_VERSION_ATLEAST(2,0,0)
    flags &= ~SDL_INIT_GAMEPAD;
#endif

    if(!sdl_init(flags))
    {
      warn("Failed to initialize SDL: %s\n", SDL_GetError());
      return false;
    }
  }

#if SDL_VERSION_ATLEAST(2,0,0)
  /* Most platforms want text input events always on so they can generate
   * convenient unicode text values, but in Android this causes some problems:
   *
   * - On older versions the navigation bar will ALWAYS display, regardless
   *   of whether or not there's an attached keyboard.
   * - Holding the space key no longer works, breaking built-in shooting (as
   *   recent as Android 10).
   * - The onscreen keyboard Android pops up can be moved but not collapsed.
   *
   * TODO: Instead, enable text input on demand at text prompts.
   * TODO: this probably redundant with behavior already in SDL.
   */
  if(!SDL_HasScreenKeyboardSupport())
    SDL_StartTextInput(NULL); // FIXME
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
