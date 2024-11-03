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

#ifndef CONFIG_SDL

#if defined(CONFIG_WII) || defined(CONFIG_NDS) || defined(CONFIG_3DS) || defined(CONFIG_DREAMCAST)
int real_main(int argc, char *argv[]);
#define main real_main
#endif // CONFIG_WII || CONFIG_NDS || CONFIG_3DS

#endif // !CONFIG_SDL

/**
 * Audio, networking, and other misc. optional features require threading
 * support. Include a platform-specific thread header here if it's available.
 * If not, a dummy implementation will be included.
 *
 * Most of the dummy functions will emit errors when used.
 * If this happens, implement proper threading functions for that platform.
 */
#ifdef CONFIG_PTHREAD
#include "thread_pthread.h"
#elif defined(CONFIG_WII)
#include "../arch/wii/thread.h"
#elif defined(CONFIG_3DS)
#include "../arch/3ds/thread.h"
#elif defined(CONFIG_DJGPP)
#include "../arch/djgpp/thread.h"
#elif defined(CONFIG_DREAMCAST)
#include "../arch/dreamcast/thread.h"
#elif defined(CONFIG_SDL) && !defined(SKIP_SDL)
#include "thread_sdl.h"
#elif defined(_WIN32) /* Fallback, prefer SDL when possible. */
#include "thread_win32.h"
#else
#if defined(CONFIG_NDS)
#define THREAD_DUMMY_ALLOW_MUTEX
#endif
#include "thread_dummy.h"
#endif

#include <stdint.h>
#include <time.h>

/* Use as (dso_fn_ptr *) to store a loaded void(*)(void) to a function pointer. */
typedef void (*dso_fn_ptr)(void);
struct dso_library;

/* Initialize a (dso_fn_ptr *) via (void *) to avoid strict aliasing warnings. */
union dso_fn_ptr_ptr
{
  void *in;
  dso_fn_ptr *value;
};

struct dso_syms_map
{
  const char *name;
  union dso_fn_ptr_ptr sym_ptr;
};

#define DSO_MAP_END { NULL, { NULL }}

CORE_LIBSPEC void delay(uint32_t ms);
CORE_LIBSPEC uint64_t get_ticks(void);
CORE_LIBSPEC boolean platform_init(void);
CORE_LIBSPEC void platform_quit(void);
CORE_LIBSPEC boolean platform_system_time(struct tm *tm,
 int64_t *epoch, int32_t *nano);

CORE_LIBSPEC struct dso_library *platform_load_library(const char *name);
CORE_LIBSPEC void platform_unload_library(struct dso_library *library);
CORE_LIBSPEC boolean platform_load_function(struct dso_library *library,
 const struct dso_syms_map *syms_map);

__M_END_DECLS

#endif // __PLATFORM_H
