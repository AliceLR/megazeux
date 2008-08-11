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

#ifndef __UTILITY_H
#define __UTILITY_H

#include "types.h"

__G_BEGIN_DECLS

/**
 * endian stuff
 */

#if  defined(WIN32) || defined(__WIN32__) || defined(_MSC_VER) || \
     defined(__i386__) || defined(__ia64__) || \
    (defined(__alpha__) || defined(__alpha)) || \
     defined(__arm__) || \
    (defined(__mips__) && defined(__MIPSEL__)) || \
     defined(__SYMBIAN32__) || \
     defined(__x86_64__) || \
     defined(__LITTLE_ENDIAN__)
/* little endian systems; no magic necessary */
#define CHECK_ENDIAN_16(x)
#define CHECK_ENDIAN_32(x)
#else
/* big endian systems; we need to swap data before storage */
#define CHECK_ENDIAN_16(x) swap16(x)
#define CHECK_ENDIAN_32(x) swap32(x)
#endif

/**
 * We support MSVC and gcc; MSVC's macro support is awful
 */

#ifdef _MSC_VER
#define PRINT_OUT   printf
#define PRINT_ERR   printf
#define PRINT_DEBUG printf
#else /* _MSC_VER */
#if defined(WIN32) || defined(__WIN32__)
/**
 * An MSYS console flushing bug requires fflush after
 * fprintf (contrary to spec).
 */
#define PRINT_OUT(x...) \
  { \
    fprintf (stdout, x); \
    fflush (stdout); \
  }
#define PRINT_ERR(x...) \
  { \
    fprintf (stderr, x); \
    fflush (stderr); \
  }
#else /* __WIN32__ */
/**
 * LINUX, etc. does not require fflush().
 */
#define PRINT_OUT(x...) \
  { \
    fprintf (stdout, x); \
  }
#define PRINT_ERR(x...) \
  { \
    fprintf (stderr, x); \
  }
#endif /* __linux__ */

#ifdef DEBUG
#define PRINT_DEBUG(x...) \
  { \
    PRINT_OUT ("D: " x); \
  }
#else
#define PRINT_DEBUG(x...)
#endif /* DEBUG */
#endif /* !_MSC_VER */

/* function prototypes */
void swap16 (u16 *);
void swap32 (u32 *);
void stream_to_alloc (void *dest, u8 **src, u32 n);
void alloc_to_stream (void *src, u8 **dest, u32 n);
void check_s_to_a (u8 *start, u32 size, void *dest, u8 **src, u32 n);
void check_a_to_s (u8 **start, u32 *size, void *src, u8 **dest, u32 n);

__G_END_DECLS

#endif /* __UTILITY_H */
