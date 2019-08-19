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

#ifndef __MEMFILE_H
#define __MEMFILE_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdlib.h>
#include <string.h>

struct memfile
{
  unsigned char *current;
  unsigned char *start;
  unsigned char *end;
  boolean alloc;
};

/**
 * Open a memory buffer for reading.
 */

static inline void mfopen(const void *src, size_t len, struct memfile *mf)
{
  mf->start = (unsigned char *)src;
  mf->current = (unsigned char *)src;
  mf->end = (unsigned char *)src + len;
  mf->alloc = false;
}

/**
 * This version exists to be a drop-in alternative for fopen().
 * For general purposes, use mfopen() instead.
 */

static inline struct memfile *mfopen_alloc(const void *src, size_t len)
{
  struct memfile *mf = cmalloc(sizeof(struct memfile));

  mf->start = (unsigned char *)src;
  mf->current = (unsigned char *)src;
  mf->end = (unsigned char *)src + len;
  mf->alloc = true;
  return mf;
}

/**
 * This function is intended to be a drop-in fclose() alternative for
 * use with mfopen_alloc().
 */

static inline int mf_alloc_free(struct memfile *mf)
{
  if(mf && mf->alloc)
  {
    free(mf);
    return 0;
  }
  return -1;
}

/**
 * Copy the buffer position and length to external variables.
 */

static inline void mfsync(void **buf, size_t *len, struct memfile *mf)
{
  if(buf) *buf = mf->start;
  if(len) *len = mf->end - mf->start;
}

/**
 * Determine if the memfile has at least len space remaining.
 */

static inline int mfhasspace(size_t len, struct memfile *mf)
{
  return (len + mf->current) <= mf->end;
}

/**
 * Move the buffer of a memfile while preserving its current position.
 */

static inline void mfmove(void *new_buf, size_t new_len, struct memfile *mf)
{
  size_t pos = mf->current - mf->start;

  mf->start = new_buf;
  mf->current = mf->start + pos;
  mf->end = mf->start + new_len;

  if(mf->current > mf->end)
    mf->current = mf->end;
}

/**
 * Resize the memfile buffer and preserve the current position.
 * Do not use this function unless the memfile buffer is on the heap.
 */

static inline void mfresize(size_t new_len, struct memfile *mf)
{
  void *new_buf = realloc(mf->start, new_len);
  if(new_buf)
    mfmove(new_buf, new_len, mf);
}

static inline int mfgetc(struct memfile *mf)
{
  int v;
  v =  mf->current[0];
  mf->current += 1;
  return v;
}

static inline int mfgetw(struct memfile *mf)
{
  int v;
  v =  mf->current[0];
  v |= mf->current[1] << 8;
  mf->current += 2;
  return v;
}

static inline int mfgetd(struct memfile *mf)
{
  int v;
  v =  mf->current[0];
  v |= mf->current[1] << 8;
  v |= mf->current[2] << 16;
  v |= mf->current[3] << 24;
  mf->current += 4;
  return v;
}

static inline int mfputc(int ch, struct memfile *mf)
{
  mf->current[0] = ch & 0xFF;
  mf->current += 1;
  return ch & 0xFF;
}

static inline void mfputw(int ch, struct memfile *mf)
{
  mf->current[0] = ch & 0xFF;
  mf->current[1] = (ch >> 8) & 0xFF;
  mf->current += 2;
}

static inline void mfputd(int ch, struct memfile *mf)
{
  mf->current[0] = ch & 0xFF;
  mf->current[1] = (ch >> 8) & 0xFF;
  mf->current[2] = (ch >> 16) & 0xFF;
  mf->current[3] = (ch >> 24) & 0xFF;
  mf->current += 4;
}

static inline size_t mfread(void *dest, size_t len, size_t count,
 struct memfile *mf)
{
  unsigned int i;
  unsigned char *pos = dest;
  for(i = 0; i < count; i++)
  {
    if(mf->current + len > mf->end)
      break;

    memcpy(pos, mf->current, len);
    mf->current += len;
    pos += len;
  }

  return i;
}

static inline size_t mfwrite(const void *src, size_t len, size_t count,
 struct memfile *mf)
{
  unsigned int i;
  unsigned char *pos = (unsigned char *)src;
  for(i = 0; i < count; i++)
  {
    if(mf->current + len > mf->end)
      break;

    memcpy(mf->current, pos, len);
    mf->current += len;
    pos += len;
  }

  return i;
}

static inline int mfseek(struct memfile *mf, long int offs, int code)
{
  unsigned char *ptr;
  switch(code)
  {
    case SEEK_SET:
      ptr = mf->start + offs;
      break;

    case SEEK_CUR:
      ptr = mf->current + offs;
      break;

    case SEEK_END:
      ptr = mf->end + offs;
      break;

    default:
      ptr = NULL;
      break;
  }

  if(ptr && ptr >= mf->start && ptr <= mf->end)
  {
    mf->current = ptr;
    return 0;
  }

  return -1;
}

static inline long int mftell(struct memfile *mf)
{
  return (long int)(mf->current - mf->start);
}

__M_END_DECLS

#endif // __MEMFILE_H
