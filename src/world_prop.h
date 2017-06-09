/* MegaZeux
 *
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __WORLD_PROP_H
#define __WORLD_PROP_H

#include "compat.h"

__M_BEGIN_DECLS

#include <util.h>


// Static functions for world/board/robot IO. Include in those C files only.


#define PROP_HEADER_SIZE 6

// This function is used to save properties files in world saving.
// There are no safety checks here. USE THE BOUNDING FUNCTIONS WHEN ALLOCATING.
static inline void save_prop_eof(struct memfile *mf)
{
  mfputw(0, mf);
}

static inline void save_prop_c(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(1, mf);
  mfputc(value, mf);
}

static inline void save_prop_w(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(2, mf);
  mfputw(value, mf);
}

static inline void save_prop_d(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(4, mf);
  mfputd(value, mf);
}

static inline void save_prop_s(int ident, const void *src, size_t len,
 size_t count, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(len * count, mf);
  mfwrite(src, len, count, mf);
}

static inline void save_prop_v(int ident, size_t len, struct memfile *prop,
 struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(len, mf);
  mfopen_static(mf->current, len, prop);
  mf->current += len;
}

static inline int load_prop_int(int length, struct memfile *prop)
{
  switch(length)
  {
    case 1:
      return mfgetc(prop);

    case 2:
      return mfgetw(prop);

    case 4:
      return mfgetd(prop);

    default:
      return 0;
  }
}

// This function is used to read properties files in world loading.
static int next_prop(struct memfile *prop, int *ident, int *length,
 struct memfile *mf)
{
  char *end = mf->end;
  char *cur;
  int len;

  if((end - mf->current)<PROP_HEADER_SIZE)
  {
    return 0;
  }

  *ident = mfgetw(mf);
  len = mfgetd(mf);
  cur = mf->current;

  if((end - cur)<len)
  {
    return 0;
  }

  *length = len;
  prop->current = cur;
  prop->start = cur;
  prop->end = cur + len;

  mf->current += len;
  return 1;
}

__M_END_DECLS

#endif // __WORLD_PROP_H