/* MegaZeux
 *
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

// General macros and functions for YUV manipulation. Supports YUY2, UYVY, and
// YVYU. SDL doesn't provide functions for packing/unpacking these.

#ifndef __YUV_H
#define __YUV_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform.h"

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN

#define YUY2_Y1_SHIFT 24
#define YUY2_Y2_SHIFT  8
#define YUY2_U_SHIFT  16
#define YUY2_V_SHIFT   0

#define UYVY_Y1_SHIFT 16
#define UYVY_Y2_SHIFT  0
#define UYVY_U_SHIFT  24
#define UYVY_V_SHIFT   8

#define YVYU_Y1_SHIFT 24
#define YVYU_Y2_SHIFT  8
#define YVYU_U_SHIFT   0
#define YVYU_V_SHIFT  16

#else

#define YUY2_Y1_SHIFT  0
#define YUY2_Y2_SHIFT 16
#define YUY2_U_SHIFT   8
#define YUY2_V_SHIFT  24

#define UYVY_Y1_SHIFT  8
#define UYVY_Y2_SHIFT 24
#define UYVY_U_SHIFT   0
#define UYVY_V_SHIFT  16

#define YVYU_Y1_SHIFT  0
#define YVYU_Y2_SHIFT 16
#define YVYU_U_SHIFT  24
#define YVYU_V_SHIFT   8

#endif

#define YUY2_Y1_MASK ((Uint32)(0xFF) << YUY2_Y1_SHIFT)
#define YUY2_Y2_MASK ((Uint32)(0xFF) << YUY2_Y2_SHIFT)
#define YUY2_U_MASK  ((Uint32)(0xFF) << YUY2_U_SHIFT)
#define YUY2_V_MASK  ((Uint32)(0xFF) << YUY2_V_SHIFT)
#define YUY2_UV_MASK (YUY2_U_MASK | YUY2_V_MASK)

#define UYVY_Y1_MASK ((Uint32)(0xFF) << UYVY_Y1_SHIFT)
#define UYVY_Y2_MASK ((Uint32)(0xFF) << UYVY_Y2_SHIFT)
#define UYVY_U_MASK  ((Uint32)(0xFF) << UYVY_U_SHIFT)
#define UYVY_V_MASK  ((Uint32)(0xFF) << UYVY_V_SHIFT)
#define UYVY_UV_MASK (UYVY_U_MASK | UYVY_V_MASK)

#define YVYU_Y1_MASK ((Uint32)(0xFF) << YVYU_Y1_SHIFT)
#define YVYU_Y2_MASK ((Uint32)(0xFF) << YVYU_Y2_SHIFT)
#define YVYU_U_MASK  ((Uint32)(0xFF) << YVYU_U_SHIFT)
#define YVYU_V_MASK  ((Uint32)(0xFF) << YVYU_V_SHIFT)
#define YVYU_UV_MASK (YVYU_U_MASK | YVYU_V_MASK)

static inline Uint32 yuy2_pack(Uint8 y1, Uint8 u, Uint8 y2, Uint8 v)
{
  return
   (y1 << YUY2_Y1_SHIFT) | (u << YUY2_U_SHIFT) |
   (y2 << YUY2_Y2_SHIFT) | (v << YUY2_V_SHIFT);
}

static inline Uint32 uyvy_pack(Uint8 y1, Uint8 u, Uint8 y2, Uint8 v)
{
  return
   (y1 << UYVY_Y1_SHIFT) | (u << UYVY_U_SHIFT) |
   (y2 << UYVY_Y2_SHIFT) | (v << UYVY_V_SHIFT);
}

static inline Uint32 yvyu_pack(Uint8 y1, Uint8 u, Uint8 y2, Uint8 v)
{
  return
   (y1 << YVYU_Y1_SHIFT) | (u << YVYU_U_SHIFT) |
   (y2 << YVYU_Y2_SHIFT) | (v << YVYU_V_SHIFT);
}

static inline Uint32 yuv_subsample(Uint32 a, Uint32 b,
 Uint32 y1_mask, Uint32 y2_mask, Uint32 uv_mask)
{
  Uint32 y1 = a & y1_mask;
  Uint32 y2 = b & y2_mask;
  Uint32 uv = ((a & uv_mask) / 2 + (b & uv_mask) / 2) & uv_mask;
  return y1 | y2 | uv;
}

static inline Uint32 yuy2_subsample(Uint32 a, Uint32 b)
{
  return yuv_subsample(a, b, YUY2_Y1_MASK, YUY2_Y2_MASK, YUY2_UV_MASK);
}

static inline Uint32 uyvy_subsample(Uint32 a, Uint32 b)
{
  return yuv_subsample(a, b, UYVY_Y1_MASK, UYVY_Y2_MASK, UYVY_UV_MASK);
}

static inline Uint32 yvyu_subsample(Uint32 a, Uint32 b)
{
  return yuv_subsample(a, b, YVYU_Y1_MASK, YVYU_Y2_MASK, YVYU_UV_MASK);
}

static inline void rgb_to_yuv(Uint8 r, Uint8 g, Uint8 b,
                              Uint8 *y, Uint8 *u, Uint8 *v)
{
  *y = (9797 * r + 19237 * g + 3734 * b) >> 15;
  *u = ((18492 * (b - *y)) >> 15) + 128;
  *v = ((23372 * (r - *y)) >> 15) + 128;
}

static inline Uint32 rgb_to_yuy2(Uint8 r, Uint8 g, Uint8 b)
{
  Uint8 y, u, v;
  rgb_to_yuv(r, g, b, &y, &u, &v);
  return yuy2_pack(y, u, y, v);
}

static inline Uint32 rgb_to_uyvy(Uint8 r, Uint8 g, Uint8 b)
{
  Uint8 y, u, v;
  rgb_to_yuv(r, g, b, &y, &u, &v);
  return uyvy_pack(y, u, y, v);
}

static inline Uint32 rgb_to_yvyu(Uint8 r, Uint8 g, Uint8 b)
{
  Uint8 y, u, v;
  rgb_to_yuv(r, g, b, &y, &u, &v);
  return yvyu_pack(y, u, y, v);
}

__M_END_DECLS

#endif /* __YUV_H */
