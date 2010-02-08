/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#include "platform.h"

#include "SDL.h"

#ifdef PSP_BUILD
#include <pspsdk.h>
#include <psppower.h>
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
#endif

#ifdef CONFIG_NDS
#include <fat.h>
#include "render_nds.h"
#include "ram.h"
#include "malloc.h"
#include "exception.h"
#include "memory_warning_pcx.h"
#endif

void delay(Uint32 ms)
{
  SDL_Delay(ms);
}

Uint32 get_ticks(void)
{
  return SDL_GetTicks();
}

// TODO: Return success/failure
int platform_init(void)
{
  Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;

#if defined(__WIN32__) && defined(DEBUG)
  freopen("CON", "wb", stdout);
#endif

#ifdef CONFIG_PSP
  scePowerSetClockFrequency(333, 333, 166);
#endif

#ifdef CONFIG_NDS
  powerON(POWER_ALL);
  //setMzxExceptionHandler();
  fatInitDefault();
  // If the "extra RAM" is missing, warn the user
  if(!nds_ram_init(DETECT_RAM))
    warning_screen((u8*)memory_warning_pcx);
  nds_ext_lock();
#endif

#ifdef DEBUG
  flags |= SDL_INIT_NOPARACHUTE;
#endif

#ifdef CONFIG_AUDIO
  flags |= SDL_INIT_AUDIO;
#endif

  SDL_Init(flags);

  SDL_EnableUNICODE(1);

  return 1;
}

void platform_quit(void)
{
  SDL_Quit();

#ifdef CONFIG_NDS
  nds_ext_unlock();
#endif
}
