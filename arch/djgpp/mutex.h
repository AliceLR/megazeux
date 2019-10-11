/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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
// This isn't really a mutex. Audio is handled via interrupt.

#ifndef __MUTEX_DJGPP_H
#define __MUTEX_DJGPP_H

#include "compat.h"

__M_BEGIN_DECLS

typedef struct
{
  void (*cbfunc)(void *);
  void *cbdata;
  bool flag;
} platform_mutex;

static inline void platform_mutex_init(platform_mutex *mutex)
{
  mutex->cbfunc = NULL;
  mutex->cbdata = NULL;
  mutex->flag = false;
}

static inline bool platform_mutex_lock(platform_mutex *mutex)
{
  if (!mutex->flag)
  {
    mutex->flag = true;
    return true;
  }
  else
    return false;
}

static inline bool platform_mutex_unlock(platform_mutex *mutex)
{
  if(mutex->flag)
  {
    mutex->flag = false;
    if(mutex->cbfunc)
    {
      mutex->cbfunc(mutex->cbdata);
      mutex->cbfunc = NULL;
    }
    return true;
  }
  else
    return false;
}

__M_END_DECLS

#endif // __MUTEX_DJGPP_H
