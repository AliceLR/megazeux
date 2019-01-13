/* MegaZeux
 *
 * Copyright (C) 2016, 2018 Adrian Siekierka <asiekierka@gmail.com>
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

#ifndef __THREAD_DREAMCAST_H
#define __THREAD_DREAMCAST_H

#include "../../src/compat.h"

__M_BEGIN_DECLS

#include <kos.h>
#include <stdbool.h>

#define THREAD_RES void*
#define THREAD_RETURN do { return; } while(0)

typedef mutex_t platform_mutex;
typedef kthread_t platform_thread;
typedef void* (*platform_thread_fn)(void*);

static inline void platform_mutex_init(platform_mutex *mutex)
{
  mutex_init(mutex, MUTEX_TYPE_NORMAL);
}

static inline bool platform_mutex_lock(platform_mutex *mutex)
{
  if(mutex_trylock(mutex))
    return false;
  return true;
}

static inline bool platform_mutex_unlock(platform_mutex *mutex)
{
  if (mutex_unlock(mutex))
    return false;
  return true;
}

static inline bool platform_mutex_destroy(platform_mutex *mutex)
{
  return true;
}

static inline int platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  // TODO
  return 1;
}

static inline void platform_thread_join(platform_thread *thread)
{
  // TODO
}

__M_END_DECLS

#endif // __THREAD_DREAMCAST_H
