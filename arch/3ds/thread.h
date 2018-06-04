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

#ifndef __MUTEX_3DS_H
#define __MUTEX_3DS_H

#include "compat.h"

#define STACK_SIZE_CTR 8096

__M_BEGIN_DECLS

#include <3ds.h>

typedef LightLock platform_mutex;
typedef Thread platform_thread;
typedef ThreadFunc platform_thread_fn;

static inline void platform_mutex_init(platform_mutex *mutex)
{
  LightLock_Init(mutex);
}

static inline bool platform_mutex_lock(platform_mutex *mutex)
{
  if(LightLock_TryLock(mutex))
    return false;
  return true;
}

static inline bool platform_mutex_unlock(platform_mutex *mutex)
{
  LightLock_Unlock(mutex);
  return true;
}

static inline bool platform_mutex_destroy(platform_mutex *mutex)
{
  return true;
}

static inline int platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  s32 priority;

  svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
  *thread = threadCreate(start_function, data, STACK_SIZE_CTR, priority-1, -1, true);

  return (*thread == NULL) ? 1 : 0;
}

static inline void platform_thread_join(platform_thread *thread)
{
  threadJoin(*thread, U64_MAX);
}

__M_END_DECLS

#endif // __MUTEX_3DS_H
