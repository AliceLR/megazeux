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

#include <SDL_stdinc.h>
#include <SDL_thread.h>
#include <SDL_version.h>

#if !SDL_VERSION_ATLEAST(2,0,0)
typedef int (*SDL_ThreadFunction)(void *);
typedef Uint32 SDL_threadID;
#endif

#define THREAD_RES int
#define THREAD_RETURN do { return 0; } while(0)

typedef SDL_cond *platform_cond;
typedef SDL_mutex *platform_mutex;
typedef SDL_sem *platform_sem;
typedef SDL_Thread *platform_thread;
typedef SDL_threadID platform_thread_id;
typedef SDL_ThreadFunction platform_thread_fn;

static inline boolean platform_mutex_init(platform_mutex *mutex)
{
  platform_mutex m = SDL_CreateMutex();
  if(m)
  {
    *mutex = m;
    return true;
  }
  return false;
}

static inline boolean platform_mutex_destroy(platform_mutex *mutex)
{
  SDL_DestroyMutex(*mutex);
  return true;
}

static inline boolean platform_mutex_lock(platform_mutex *mutex)
{
  if(SDL_LockMutex(*mutex))
    return false;
  return true;
}

static inline boolean platform_mutex_unlock(platform_mutex *mutex)
{
  if(SDL_UnlockMutex(*mutex))
    return false;
  return true;
}

static inline boolean platform_cond_init(platform_cond *cond)
{
  platform_cond c = SDL_CreateCond();
  if(c)
  {
    *cond = c;
    return true;
  }
  return false;
}

static inline boolean platform_cond_destroy(platform_cond *cond)
{
  SDL_DestroyCond(*cond);
  return true;
}

static inline boolean platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  if(SDL_CondWait(*cond, *mutex))
    return false;
  return true;
}

static inline boolean platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned int timeout_ms)
{
  if(SDL_CondWaitTimeout(*cond, *mutex, (Uint32)timeout_ms))
    return false;
  return true;
}

static inline boolean platform_cond_signal(platform_cond *cond)
{
  if(SDL_CondSignal(*cond))
    return false;
  return true;
}

static inline boolean platform_cond_broadcast(platform_cond *cond)
{
  if(SDL_CondBroadcast(*cond))
    return false;
  return true;
}

static inline boolean platform_sem_init(platform_sem *sem, unsigned init_value)
{
  SDL_sem *ret = SDL_CreateSemaphore(init_value);
  if(ret)
  {
    *sem = ret;
    return true;
  }
  return false;
}

static inline boolean platform_sem_destroy(platform_sem *sem)
{
  SDL_DestroySemaphore(*sem);
  return true;
}

static inline boolean platform_sem_wait(platform_sem *sem)
{
  if(SDL_SemWait(*sem))
    return false;
  return true;
}

static inline boolean platform_sem_post(platform_sem *sem)
{
  if(SDL_SemPost(*sem))
    return false;
  return true;
}

static inline boolean platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  platform_thread t = SDL_CreateThread(start_function, "", data);
#else
  platform_thread t = SDL_CreateThread(start_function, data);
#endif

  if(t)
  {
    *thread = t;
    return true;
  }
  return false;
}

static inline boolean platform_thread_join(platform_thread *thread)
{
  SDL_WaitThread(*thread, NULL);
  return true;
}

static inline platform_thread_id platform_get_thread_id(void)
{
  return SDL_ThreadID();
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return a == b;
}

__M_END_DECLS

#endif // __THREAD_SDL_H
