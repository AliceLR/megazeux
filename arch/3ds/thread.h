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

#ifndef __THREAD_3DS_H
#define __THREAD_3DS_H

#include "../../src/compat.h"

#define STACK_SIZE_CTR 8096

__M_BEGIN_DECLS

#include <3ds.h>

#define THREAD_RES void
#define THREAD_RETURN do { return; } while(0)

typedef CondVar platform_cond;
typedef LightLock platform_mutex;
typedef Thread platform_thread;
typedef Thread platform_thread_id;
typedef ThreadFunc platform_thread_fn;

static inline void platform_mutex_init(platform_mutex *mutex)
{
  LightLock_Init(mutex);
}

static inline boolean platform_mutex_lock(platform_mutex *mutex)
{
  LightLock_Lock(mutex);
  return true;
}

static inline boolean platform_mutex_unlock(platform_mutex *mutex)
{
  LightLock_Unlock(mutex);
  return true;
}

static inline boolean platform_mutex_destroy(platform_mutex *mutex)
{
  return true;
}

static inline boolean platform_cond_init(platform_cond *cond)
{
  CondVar_Init(cond);
  return true;
}

static inline void platform_cond_destroy(platform_cond *cond)
{
  return;
}

static inline boolean platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  CondVar_Wait(cond, mutex);
  return true;
}

static inline boolean platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned int timeout_ms)
{
  s64 timeout_ns = (s64)timeout_ms * 1000000;
  if(CondVar_WaitTimeout(cond, mutex, timeout_ns))
    return false;
  return true;
}

static inline boolean platform_cond_signal(platform_cond *cond)
{
  CondVar_Signal(cond);
  return true;
}

static inline boolean platform_cond_broadcast(platform_cond *cond)
{
  CondVar_Broadcast(cond);
  return true;
}

static inline int platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  s32 priority;

  svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
  *thread = threadCreate(start_function, data, STACK_SIZE_CTR, priority-1, -1,
   true);

  return (*thread == NULL) ? 1 : 0;
}

static inline void platform_thread_join(platform_thread *thread)
{
  threadJoin(*thread, U64_MAX);
}

static inline platform_thread_id platform_get_thread_id(void)
{
  return threadGetCurrent();
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return a == b;
}

__M_END_DECLS

#endif // __THREAD_3DS_H
