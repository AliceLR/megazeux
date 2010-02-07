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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utility.h"
#include "error.h"

static void stream_to_alloc (void *dest, uint8_t **src, uint32_t n)
{
  /* copy count blocks */
  memcpy (dest, *src, n);

  /* increment stream offset */
  *src += n;
}

static void alloc_to_stream (void *src, uint8_t **dest, uint32_t n)
{
  /* copy count blocks */
  memcpy (*dest, src, n);

  /* increment stream offset */
  *dest += n;
}

void check_s_to_a (uint8_t *start, uint32_t size, void *dest, uint8_t **src,
                   uint32_t n)
{
  const size_t diff = *src - start;

  /* check we've got enough of the stream */
  if (diff + n > size)
    error (OUT_OF_DATA);

  /* otherwise, call on */
  stream_to_alloc (dest, src, n);
}

void check_a_to_s (uint8_t **start, uint32_t *size, void *src, uint8_t **dest, uint32_t n)
{
  const size_t diff = *dest - *start;

  /* check we've got enough allocated */
  if (diff + n > *size) {
    /* new size */
    *size = diff + n;

    /* increase allocation size */
    *start = realloc (*start, *size);

    /* reassign pointer */
    *dest = (uint8_t *) (*start + diff);
  }

  /* otherwise, call on */
  alloc_to_stream (src, dest, n);
}
