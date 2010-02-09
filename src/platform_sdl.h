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

#ifndef __PLATFORM_SDL_H
#define __PLATFORM_SDL_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform.h"

#include "SDL.h"
#include "SDL/SDL_thread.h"

#ifdef CONFIG_PTHREAD_MUTEXES

#include "pthread.h"

typedef pthread_mutex_t platform_mutex;
#define platform_mutex_init(mutex) pthread_mutex_init(&mutex, 0)
#define platform_mutex_lock(mutex) pthread_mutex_lock(&mutex)
#define platform_mutex_unlock(mutex) pthread_mutex_unlock(&mutex)

#else // !CONFIG_PTHREAD_MUTEXES

typedef SDL_mutex *platform_mutex;
#define platform_mutex_init(mutex) (mutex = SDL_CreateMutex())
#define platform_mutex_lock(mutex) SDL_LockMutex(mutex)
#define platform_mutex_unlock(mutex) SDL_UnlockMutex(mutex)

#endif // CONFIG_PTHREAD_MUTEXES

typedef SDL_Thread *platform_thread;
#define platform_thread_create(thread, func, arg) thread = SDL_CreateThread(&func, arg)

__M_END_DECLS

#endif // __PLATFORM_SDL_H
