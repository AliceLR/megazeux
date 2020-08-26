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
 * RAII wrappers for misc. data types used in MZX.
 */

#ifndef __SCOPED_HPP
#define __SCOPED_HPP

#include "../compat.h"
#include "../util.h"

/**
 * RAII wrapper for <FILE, fclose> and <vfile, vfclose>.
 */
template<class T, int (*RELEASE_FN)(T *)>
class Scoped
{
private:
  T *ptr;

public:
  Scoped(): ptr(nullptr) {}
  Scoped(T *p): ptr(p) {}
  ~Scoped()
  {
    if(ptr)
      RELEASE_FN(ptr);
  }

  Scoped &operator=(T *p)
  {
    if(p != ptr)
    {
      if(ptr)
        RELEASE_FN(ptr);
      ptr = p;
    }
    return *this;
  }

  operator T *() const
  {
    return ptr;
  }

#ifdef IS_CXX_11
  explicit
#endif
  operator bool() const
  {
    return ptr != nullptr;
  }
};

#endif /* __SCOPED_HPP */
