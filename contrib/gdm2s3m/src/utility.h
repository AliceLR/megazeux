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

#ifndef __UTILITY_H
#define __UTILITY_H

#include "types.h"

__G_BEGIN_DECLS

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

#else // !LITTLE_ENDIAN

/* big endian systems; we need to swap data before storage */

static inline void gdm2s3m_swap16(uint16_t *var)
{
  *var = ((*var << 8) | (*var >> 8));
}

static inline void gdm2s3m_swap32(uint32_t *var)
{
  *var = ((*var << 24) | ((*var << 8) & 0x00FF0000) |
         ((*var >> 8) & 0x0000FF00) | (*var >> 24));
}

#define CHECK_ENDIAN_16(x) gdm2s3m_swap16(x)
#define CHECK_ENDIAN_32(x) gdm2s3m_swap32(x)

#endif // LITTLE_ENDIAN

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
#define PRINT_OUT(...) \
  { \
    fprintf(stdout, __VA_ARGS__); \
    fflush(stdout); \
  }
#define PRINT_ERR(...) \
  { \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr); \
  }
#else /* __WIN32__ */
/**
 * LINUX, etc. does not require fflush().
 */
#define PRINT_OUT(...) \
  { \
    fprintf(stdout, __VA_ARGS__); \
  }
#define PRINT_ERR(...) \
  { \
    fprintf(stderr, __VA_ARGS__); \
  }
#endif /* __linux__ */

#ifdef DEBUG
#define PRINT_DEBUG(...) \
  { \
    PRINT_OUT("D: " __VA_ARGS__); \
  }
#else
#define PRINT_DEBUG(...)
#endif /* DEBUG */
#endif /* !_MSC_VER */

/* function prototypes */
void check_s_to_a (uint8_t *start, uint32_t size, void *dest,
                   uint8_t **src, uint32_t n);
void check_a_to_s (uint8_t **start, uint32_t *size, void *src,
                   uint8_t **dest, uint32_t n);

__G_END_DECLS

#endif /* __UTILITY_H */
