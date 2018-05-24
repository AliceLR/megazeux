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

#ifndef __MUTEX_WII_H
#define __MUTEX_WII_H

#include "compat.h"

__M_BEGIN_DECLS

#include <time.h>

#define BOOL _BOOL
#include <gctypes.h>
#include <ogc/mutex.h>
#include <ogc/cond.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#undef BOOL

typedef cond_t platform_cond;
typedef mutex_t platform_mutex;
typedef lwp_t platform_thread;
typedef void *(*platform_thread_fn)(void *);

static inline void platform_mutex_init(platform_mutex *mutex)
{
  // FIXME: Okay to ignore retval?
  LWP_MutexInit(mutex, false);
}

static inline void platform_mutex_destroy(platform_mutex *mutex)
{
  LWP_MutexDestroy(*mutex);
}

static inline bool platform_mutex_lock(platform_mutex *mutex)
{
  if(LWP_MutexLock(*mutex))
    return false;
  return true;
}

static inline bool platform_mutex_unlock(platform_mutex *mutex)
{
  if(LWP_MutexUnlock(*mutex))
    return false;
  return true;
}

static inline void platform_cond_init(platform_cond *cond)
{
  LWP_CondInit(cond);
}

static inline void platform_cond_destroy(platform_cond *cond)
{
  LWP_CondDestroy(*cond);
}

static inline bool platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  if(LWP_CondWait(*cond, *mutex))
    return false;
  return true;
}

static inline bool platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned int timeout_ms)
{
  struct timespec timeout;
  u64 ticks;

  // Use LWP watchdog to get a usable absolute time
  ticks = gettime() + millisecs_to_ticks(timeout_ms);

  timeout.tv_sec = ticks_to_secs(ticks);
  timeout.tv_nsec = ticks_to_nanosecs(ticks) % 1000000000;

  if(LWP_CondTimedWait(*cond, *mutex, &timeout))
    return false;
  return true;
}

static inline bool platform_cond_signal(platform_cond *cond)
{
  if(LWP_CondSignal(*cond))
    return false;
  return true;
}

static inline bool platform_cond_broadcast(platform_cond *cond)
{
  if(LWP_CondBroadcast(*cond))
    return false;
  return true;
}

static inline int platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  return LWP_CreateThread(thread, start_function, data, NULL, 0, 64);
}

static inline void platform_thread_join(platform_thread *thread)
{
  LWP_JoinThread(*thread, NULL);
}

static inline void platform_yield(void)
{
  LWP_YieldThread();
}

__M_END_DECLS

#endif // __MUTEX_WII_H
