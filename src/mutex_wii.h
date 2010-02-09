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

#define BOOL _BOOL
#include <ogc/mutex.h>
#undef BOOL

typedef mutex_t platform_mutex;

static inline void platform_mutex_init(platform_mutex *mutex)
{
  // FIXME: Okay to ignore retval?
  LWP_MutexInit(mutex, false);
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

__M_END_DECLS

#endif // __MUTEX_WII_H
