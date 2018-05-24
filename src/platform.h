/* MegaZeux
 *
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

#ifndef __PLATFORM_H
#define __PLATFORM_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform_endian.h"

#ifdef CONFIG_SDL

#include "SDL.h"

#else // !CONFIG_SDL

#include <inttypes.h>

typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;

#if defined(CONFIG_WII) || defined(CONFIG_NDS) || defined(CONFIG_3DS)
int real_main(int argc, char *argv[]);
#define main real_main
#endif // CONFIG_WII || CONFIG_NDS || CONFIG_3DS

#endif // CONFIG_SDL

// Need threads and mutexes for DNS lookups.
// Otherwise only need mutexes for audio, but the Wii port
// uses them for events too

#if defined(CONFIG_AUDIO) || defined(CONFIG_NETWORK) || defined(CONFIG_WII)

#ifdef CONFIG_PTHREAD
#include "thread_pthread.h"
#elif defined(CONFIG_WII)
#include "../arch/wii/thread.h"
#elif defined(CONFIG_3DS)
#include "../arch/3ds/thread.h"
#elif defined(CONFIG_SDL)
#include "thread_sdl.h"
#else
#error Provide a valid thread/mutex implementation for this platform!
#endif

#endif // defined(CONFIG_AUDIO) || defined(CONFIG_NETWORK) || defined(CONFIG_WII)

CORE_LIBSPEC void delay(Uint32 ms);
CORE_LIBSPEC Uint32 get_ticks(void);
CORE_LIBSPEC bool platform_init(void);
CORE_LIBSPEC void platform_quit(void);

__M_END_DECLS

#endif // __PLATFORM_H
