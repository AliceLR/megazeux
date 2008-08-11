/**
 *  Copyright (C) 2003-2004  Alistair John Strachan  (alistair@devzero.co.uk)
 *
 *  This is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2, or (at your option) any later
 *  version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utility.h"
#include "error.h"

void swap16 (u16 *var)
{
  *var = ((*var << 8) | (*var >> 8));
}

void swap32 (u32 *var)
{
  *var = ((*var << 24) | ((*var << 8) & 0x00FF0000) |
         ((*var >> 8) & 0x0000FF00) | (*var >> 24));
}


void stream_to_alloc (void *dest, u8 **src, u32 n)
{
  /* copy count blocks */
  memcpy (dest, (*src), n);

  /* increment stream offset */
  (*src) += n;
}

void alloc_to_stream (void *src, u8 **dest, u32 n)
{
  /* copy count blocks */
  memcpy ((*dest), src, n);

  /* increment stream offset */
  (*dest) += n;
}

void check_s_to_a (u8 *start, u32 size, void *dest, u8 **src, u32 n)
{
  const u32 diff = (u32) (*src - start);

  /* check we've got enough of the stream */
  if (diff + n > size)
    error (OUT_OF_DATA);

  /* otherwise, call on */
  stream_to_alloc (dest, src, n);
}

void check_a_to_s (u8 **start, u32 *size, void *src, u8 **dest, u32 n)
{
  const u32 diff = (u32) (*dest - *start);

  /* check we've got enough allocated */
  if (diff + n > (*size)) {
    /* new size */
    (*size) = diff + n;

    /* increase allocation size */
    (*start) = (u8 *) realloc ((*start), (*size));

    /* reassign pointer */
    (*dest) = (u8 *) (*start + diff);
  }

  /* otherwise, call on */
  alloc_to_stream (src, dest, n);
}
