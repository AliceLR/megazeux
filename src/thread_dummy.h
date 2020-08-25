/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Dummy threading functions to avoid ifdefs in the check alloc functions.
 * If your platform needs actual threading functions and SDL or pthread are
 * not available, see platform.h.
 */

#ifndef __THREAD_DUMMY_H
#define __THREAD_DUMMY_H

#include "compat.h"

__M_BEGIN_DECLS

typedef int platform_thread_id;

static inline platform_thread_id platform_get_thread_id(void)
{
  return 0;
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return true;
}

__M_END_DECLS

#endif /* __THREAD_DUMMY_H */
