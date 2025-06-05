/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __RENDER_LAYER_COMMON_HPP
#define __RENDER_LAYER_COMMON_HPP

#include "graphics.h"
#include "platform_endian.h"

/**
 * Select the real display character for a char element.
 * TODO: this should be done at draw time.
 */
static inline int select_char(const struct char_element *src,
 const struct video_layer *layer)
{
  int ch = src->char_value;
  if(ch >= INVISIBLE_CHAR)
    return INVISIBLE_CHAR;

  // Char values >= 256, prior to offsetting, are from the protected charset.
  if(ch > 0xff)
  {
    return (ch & 0xff) + PROTECTED_CHARSET_POSITION;
  }
  else
    return ((ch & 0xff) + layer->offset) % PROTECTED_CHARSET_POSITION;
}

/**
 * Mode 0 and UI layer color selection function.
 * This needs to be done for both colors.
 */
static inline int select_color_16(uint8_t color, int ppal)
{
  // Palette values >= 16, prior to offsetting, are from the protected palette.
  if(color >= 16)
  {
    return (color - 16) % 16 + ppal;
  }
  else
    return color;
}

/**
 * Precompute clipping ranges for a layer.
 * Because the precision of the input is in chars (not pixels),
 * the destination x/y may be negative, and the effective final
 * destination x/y may extend beyond the screen. The caller is
 * responsible for correctly handling these clip areas.
 *
 * @param start_x   starting x within the layer, in chars.
 * @param start_y   starting y within the layer, in chars.
 * @param end_x     last x + 1 within the layer, in chars.
 * @param end_y     last y + 1 within the layer, in chars.
 * @param dest_x    first x to draw, in pixels. May be negative.
 * @param dest_y    first y to draw, in pixels. May be negative.
 */
static inline boolean precompute_clip(int &start_x, int &start_y,
 int &end_x, int &end_y, int &dest_x, int &dest_y,
 int width_px, int height_px, const struct video_layer *layer)
{
  int dest_last_x = layer->x + layer->w * CHAR_W;
  int dest_last_y = layer->y + layer->h * CHAR_H;
  boolean ret = false;

  if(layer->x < 0)
  {
    start_x = -(layer->x / CHAR_W);
    dest_x = layer->x % CHAR_W; // intentional truncated modulo
    ret = true;
  }
  else
  {
    start_x = 0;
    dest_x = layer->x;
  }

  if(layer->y < 0)
  {
    start_y = -(layer->y / CHAR_H);
    dest_y = layer->y % CHAR_H; // intentional truncated modulo
    ret = true;
  }
  else
  {
    start_y = 0;
    dest_y = layer->y;
  }

  end_x = layer->w;
  end_y = layer->h;

  if(dest_last_x > width_px)
  {
    end_x -= (dest_last_x - width_px) / CHAR_W;
    ret = true;
  }

  if(dest_last_y > height_px)
  {
    end_y -= (dest_last_y - height_px) / CHAR_H;
    ret = true;
  }
  return ret;
}

/**
 * Get the combined colors value from a char_element.
 */
static inline unsigned both_colors(const char_element *src)
{
#ifdef IS_CXX_11
  static_assert(offsetof(char_element, bg_color) == 2, "");
  static_assert(offsetof(char_element, fg_color) == 3, "");
#endif
  return reinterpret_cast<const uint16_t *>(src)[1];
}

#ifndef CONFIG_NO_VECTOR_RENDERING

/**
 * render_layer32x4_sse2
 */
boolean render_layer32x4_sse2(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip);

#if defined(__i386__) || defined(__x86_64__) || \
    defined(_M_AMD64) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP == 2)
#define HAS_RENDER_LAYER32X4
#define HAS_RENDER_LAYER32X4_SSE2

static inline boolean render_layer32x4(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  if(!platform_has_sse2())
    return false;

  return render_layer32x4_sse2(
   pixels, width_px, height_px, pitch, graphics, layer,
   smzx, trans, clip);
}
#endif /* SSE2 */

/**
 * render_layer32x8_avx
 */
boolean render_layer32x8_avx(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip);

boolean render_layer32x8_avx2(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip);

// Only enable AVX if the version supports intrinsics (GCC 4.7+, clang 5?).
// Currently requiring MSVC to enable them globally.
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__x86_64__) && \
  ((__GNUC__ >= 4 && __GNUC_MINOR__ >= 7) || (__GNUC__ >= 5))
#define GCC_HAS_AVX
#endif

#if defined(__clang__) && defined(__clang_major__) && defined(__x86_64__) && \
 (__clang_major__ >= 5)
#define CLANG_HAS_AVX
#endif

#if defined(_MSC_VER) && defined(__AVX__) && defined(__AVX2__)
#define MSVC_HAS_AVX
#endif

#if defined(GCC_HAS_AVX) || defined(CLANG_HAS_AVX) || defined(MSVC_HAS_AVX)
#define HAS_RENDER_LAYER32X8
#define HAS_RENDER_LAYER32X8_AVX

static inline boolean render_layer32x8(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  if(!platform_has_avx())
    return false;

  // Only one of these sets of renderers is enabled per build.
  if(platform_has_avx2())
  {
    if(render_layer32x8_avx2(
     pixels, width_px, height_px, pitch, graphics, layer,
     smzx, trans, clip))
      return true;
  }

  return render_layer32x8_avx(
   pixels, width_px, height_px, pitch, graphics, layer,
   smzx, trans, clip);
}
#endif /* AVX */

/**
 * render_layer32x4_neon
 */
boolean render_layer32x4_neon(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip);

#ifdef __ARM_NEON
#define HAS_RENDER_LAYER32X4
#define HAS_RENDER_LAYER32X4_NEON

static inline boolean render_layer32x4(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  if(!platform_has_neon())
    return false;

  return render_layer32x4_neon(
   pixels, width_px, height_px, pitch, graphics, layer,
   smzx, trans, clip);
}
#endif /* NEON */

/**
 * render_layer32x8_rvv
 */
boolean render_layer32x8_rvv(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int tr, int clip);

#if defined(__riscv) || defined(__riscv__)
#define HAS_RENDER_LAYER32X8
#define HAS_RENDER_LAYER32X8_RVV

#include <riscv_vector.h>

static inline boolean render_layer32x8(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  if(!platform_has_rvv())
    return false;

  return render_layer32x8_rvv(
   pixels, width_px, height_px, pitch, graphics, layer,
   smzx, trans, clip);
}

#endif

#endif /* CONFIG_NO_VECTOR_RENDERING */

#endif /* __RENDER_LAYER_COMMON_HPP */
