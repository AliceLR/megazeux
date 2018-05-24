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

#ifndef __THREAD_SDL_H
#define __THREAD_SDL_H

#include "compat.h"

__M_BEGIN_DECLS

#include "SDL_thread.h"

typedef SDL_cond* platform_cond;
typedef SDL_mutex* platform_mutex;
typedef SDL_Thread* platform_thread;
typedef SDL_ThreadFunction platform_thread_fn;

static inline void platform_mutex_init(platform_mutex *mutex)
{
  *mutex = SDL_CreateMutex();
}

static inline void platform_mutex_destroy(platform_mutex *mutex)
{
  SDL_DestroyMutex(*mutex);
}

static inline bool platform_mutex_lock(platform_mutex *mutex)
{
  if(SDL_LockMutex(*mutex))
    return false;
  return true;
}

static inline bool platform_mutex_unlock(platform_mutex *mutex)
{
  if(SDL_UnlockMutex(*mutex))
    return false;
  return true;
}

static inline void platform_cond_init(platform_cond *cond)
{
  *cond = SDL_CreateCond();
}

static inline void platform_cond_destroy(platform_cond *cond)
{
  SDL_DestroyCond(*cond);
}

static inline bool platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  if(SDL_CondWait(*cond, *mutex))
    return false;
  return true;
}

static inline bool platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned int timeout_ms)
{
  if(SDL_CondWaitTimeout(*cond, *mutex, (Uint32)timeout_ms))
    return false;
  return true;
}

static inline bool platform_cond_signal(platform_cond *cond)
{
  if(SDL_CondSignal(*cond))
    return false;
  return true;
}

static inline bool platform_cond_broadcast(platform_cond *cond)
{
  if(SDL_CondBroadcast(*cond))
    return false;
  return true;
}

static inline int platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  *thread = SDL_CreateThread(start_function, "", data);
  if(*thread)
    return 0;
  return -1;
}

static inline void platform_thread_join(platform_thread *thread)
{
  SDL_WaitThread(*thread, NULL);
}

static inline void platform_yield(void)
{
  // FIXME
  SDL_Delay(1);
}

__M_END_DECLS

#endif // __THREAD_SDL_H
