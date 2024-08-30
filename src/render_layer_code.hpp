/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 * Copyright (C) 2020, 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

// This file recursively instantiates template functions to generate many
// versions of the render_layer_func function. The right function to call is
// selected through a dispatch mechanism.

// Errors and warnings in this file are duplicated hundreds of times over, so
// when working on this function it can be good to redirect make's stderr
// stream to a file.

#include "graphics.h"
#include "platform_endian.h"
#include "util.h"

#include <stdlib.h>

#ifdef IS_CXX_11
#include <type_traits>
#endif

#ifdef _MSC_VER
#include <cstdio>
#endif

template<typename PIXTYPE>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR, int CLIP>
static void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer);

/**
 * Alignment of pixel buffer (8bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<uint8_t>(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<uint8_t, uint64_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<uint8_t, uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    case 16:
      render_layer_func<uint8_t, uint16_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    case 8:
      render_layer_func<uint8_t, uint8_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG align=%d (8bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Alignment of pixel buffer (16bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<uint16_t>(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<uint16_t, uint64_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<uint16_t, uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    case 16:
      render_layer_func<uint16_t, uint16_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG align=%d (16bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Alignment of pixel buffer (32bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<uint32_t>(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int align, int smzx, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<uint32_t, uint64_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<uint32_t, uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       smzx, trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG align=%d (32bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Renderer bits-per-pixel (8, 16, or 32).
 * Sets of renderers for lower bpps are larger since they try to support more
 * alignment options, so several platforms disable them altogether to reduce
 * executable size and/or compilation time.
 */
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int bpp, int align, int smzx, int trans, int clip)
{
  switch(bpp)
  {
#ifndef SKIP_8BPP
    case 8:
      render_layer_func<uint8_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       align, smzx, trans, clip);
      break;
#endif
#ifndef SKIP_16BPP
    case 16:
      render_layer_func<uint16_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       align, smzx, trans, clip);
      break;
#endif
#ifndef SKIP_32BPP
    case 32:
      render_layer_func<uint32_t>(
       pixels, width_px, height_px, pitch, graphics, layer,
       align, smzx, trans, clip);
      break;
#endif
    default:
      fprintf(mzxerr, "INVALID RENDERER ARG bpp=%d\n"
       "(is this bpp enabled for this platform?)\n", bpp);
      exit(1);
      break;
  }
}

/**
 * Renderer is SMZX (1) or normal MZX (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int smzx, int trans, int clip)
{
  switch(smzx)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, 0>(
       pixels, width_px, height_px, pitch, graphics, layer,
       trans, clip);
      break;

    case 1:
    case 2:
    case 3:
      render_layer_func<PIXTYPE, ALIGNTYPE, 1>(
       pixels, width_px, height_px, pitch, graphics, layer,
       trans, clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG smzx=%d\n", smzx);
      exit(1);
      break;
  }
}

/**
 * Layer transparency enabled (1) or disabled (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int trans, int clip)
{
  switch(trans)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, 0>(
       pixels, width_px, height_px, pitch, graphics, layer,
       clip);
      break;

    case 1:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, 1>(
       pixels, width_px, height_px, pitch, graphics, layer,
       clip);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG trans=%d\n", trans);
      exit(1);
      break;
  }
}

/**
 * Renderer should clip the layer at the screen boundaries (1) or not (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, TR, 0>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    case 1:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, TR, 1>(
       pixels, width_px, height_px, pitch, graphics, layer);
      break;

    default:
      fprintf(mzxerr, "INVALID RENDERER ARG clip=%d\n", clip);
      exit(1);
      break;
  }
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

// Macros to perform these shifts while ignoring spurious compiler warnings
// that can't be turned off for older versions of GCC. Since BPP is constexpr
// these should all optimize to single shifts.
#define BPPx1(c) (c << (BPP - 1) << 1)
#define BPPx2(c) (c << (BPP - 1) << (BPP - 1) << 2)
#define BPPx3(c) (c << (BPP - 1) << (BPP - 1) << (BPP - 1) << 3)

/**
 * Precompute an array of color combinations for multipixel write renderers.
 *
 * Since this requires 1<<PPW values to be computed, 8 PPW renderers would
 * need 256 combinations, which would probably hurt performance more than help
 * it. Instead, 8 PPW just uses a special case of the 4 PPW code (see below).
 */
template<int BPP, int PPW, typename ALIGNTYPE>
static inline void set_colors_mzx(ALIGNTYPE (&dest)[16], ALIGNTYPE bg, ALIGNTYPE fg)
{
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
  switch(PPW)
  {
    case 1:
      break;

    case 2:
      dest[0] = BPPx1(bg) | bg;
      dest[1] = BPPx1(fg) | bg;
      dest[2] = BPPx1(bg) | fg;
      dest[3] = BPPx1(fg) | fg;
      break;

    case 4:
    case 8:
      dest[0]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[1]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[2]  = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[3]  = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[4]  = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[5]  = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[6]  = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[7]  = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[8]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[9]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[10] = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[11] = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[12] = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[13] = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[14] = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | fg;
      dest[15] = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | fg;
      break;
  }
#else
  switch(PPW)
  {
    case 1:
      break;

    case 2:
      dest[0] = BPPx1(bg) | bg;
      dest[1] = BPPx1(bg) | fg;
      dest[2] = BPPx1(fg) | bg;
      dest[3] = BPPx1(fg) | fg;
      break;

    case 4:
    case 8:
      dest[0]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[1]  = BPPx3(bg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[2]  = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[3]  = BPPx3(bg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[4]  = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[5]  = BPPx3(bg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[6]  = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[7]  = BPPx3(bg) | BPPx2(fg) | BPPx1(fg) | fg;
      dest[8]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | bg;
      dest[9]  = BPPx3(fg) | BPPx2(bg) | BPPx1(bg) | fg;
      dest[10] = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | bg;
      dest[11] = BPPx3(fg) | BPPx2(bg) | BPPx1(fg) | fg;
      dest[12] = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | bg;
      dest[13] = BPPx3(fg) | BPPx2(fg) | BPPx1(bg) | fg;
      dest[14] = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | bg;
      dest[15] = BPPx3(fg) | BPPx2(fg) | BPPx1(fg) | fg;
      break;
  }
#endif
}

/* Colors should be pre-doubled here. */
template<int BPP, int PPW, typename ALIGNTYPE>
static inline void set_colors_smzx(ALIGNTYPE (&dest)[16], ALIGNTYPE (&cols)[4])
{
  ALIGNTYPE c0 = cols[0];
  ALIGNTYPE c1 = cols[1];
  ALIGNTYPE c2 = cols[2];
  ALIGNTYPE c3 = cols[3];
  switch(PPW)
  {
    case 1:
    case 2:
      break;

    case 4:
    case 8:
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
      dest[0]  = BPPx2(c0) | c0;
      dest[1]  = BPPx2(c1) | c0;
      dest[2]  = BPPx2(c2) | c0;
      dest[3]  = BPPx2(c3) | c0;
      dest[4]  = BPPx2(c0) | c1;
      dest[5]  = BPPx2(c1) | c1;
      dest[6]  = BPPx2(c2) | c1;
      dest[7]  = BPPx2(c3) | c1;
      dest[8]  = BPPx2(c0) | c2;
      dest[9]  = BPPx2(c1) | c2;
      dest[10] = BPPx2(c2) | c2;
      dest[11] = BPPx2(c3) | c2;
      dest[12] = BPPx2(c0) | c3;
      dest[13] = BPPx2(c1) | c3;
      dest[14] = BPPx2(c2) | c3;
      dest[15] = BPPx2(c3) | c3;
#else
      dest[0]  = BPPx2(c0) | c0;
      dest[1]  = BPPx2(c0) | c1;
      dest[2]  = BPPx2(c0) | c2;
      dest[3]  = BPPx2(c0) | c3;
      dest[4]  = BPPx2(c1) | c0;
      dest[5]  = BPPx2(c1) | c1;
      dest[6]  = BPPx2(c1) | c2;
      dest[7]  = BPPx2(c1) | c3;
      dest[8]  = BPPx2(c2) | c0;
      dest[9]  = BPPx2(c2) | c1;
      dest[10] = BPPx2(c2) | c2;
      dest[11] = BPPx2(c2) | c3;
      dest[12] = BPPx2(c3) | c0;
      dest[13] = BPPx2(c3) | c1;
      dest[14] = BPPx2(c3) | c2;
      dest[15] = BPPx2(c3) | c3;
#endif
      break;
  }
}

template<int PPW>
static inline unsigned get_colors_index(unsigned char_byte, int write_pos)
{
  return ((char_byte << (write_pos * PPW)) & 0xff) >> (8 - PPW);
}

/**
 * Use the set_colors array to compute multiple pixel values simultaneously.
 * This optimization is only useful for PPW >= 2 renderers.
 */
template<int PPW, typename ALIGNTYPE>
static inline ALIGNTYPE get_colors(ALIGNTYPE (&set_colors)[16], unsigned idx)
{
  switch(PPW)
  {
    // Should be unreachable, but some compilers complain...
    case 1:
      return 0;

    case 2:
    case 4:
      return set_colors[idx];

    case 8:
    {
      unsigned int left = (idx & 0xF0) >> 4;
      unsigned int right = (idx & 0x0F);
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
      return (set_colors[right] << sizeof(ALIGNTYPE) * 4) | set_colors[left];
#else
      return (set_colors[left] << sizeof(ALIGNTYPE) * 4) | set_colors[right];
#endif
    }
  }
}

#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
#define PIXEL_POS(i)      (BPP * (PPW - 1 - (i)))
#define PIXEL_POS_PAIR(i) (BPP * (PPW - 2 - (i)))
#else
#define PIXEL_POS(i)      (BPP * (i))
#define PIXEL_POS_PAIR(i) (BPP * (i))
#endif

/**
 * Finally, render the layer.
 * The optimizer will optimize out the unnecessary parts for relevant renderers.
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int TR, int CLIP>
static inline void render_layer_func(
 void * RESTRICT pixels, int width_px, int height_px, size_t pitch,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
#ifdef IS_CXX_11
  constexpr int BPP = sizeof(PIXTYPE) * 8;
  constexpr int PPW = sizeof(ALIGNTYPE) / sizeof(PIXTYPE);

  /**
   * The minimum pixel position allowed to be drawn.
   * Since the SMZX,PPW=1 renderers have a special behavior that draws two
   * pixels, it should always use -1 here (as if PPW is 2).
   */
  constexpr int PIXEL_X_MINIMUM = (SMZX && PPW == 1) ? -1 : 1 - PPW;

  // Make sure some pointless/impossible renderers are never instantiated.
  static_assert((PPW >= 1), "invalid ppw < 1");
  static_assert((PPW <= 8), "invalid ppw > 8");
  static_assert((PPW == 1) || (PPW == 2) || (PPW == 4) || (PPW == 8),
   "invalid ppw (must be power of 2)");
#else
// Use these if for whatever reason C++11 isn't available.
#define BPP (int)(sizeof(PIXTYPE) * 8)
#define PPW (int)(sizeof(ALIGNTYPE) / sizeof(PIXTYPE))
#define PIXEL_X_MINIMUM (int)((SMZX && PPW == 1) ? -1 : 1 - PPW)
#endif

#ifdef DEBUG
  static boolean printed = false;
  if(!printed)
  {
    fprintf(mzxerr, "%s\n", __PRETTY_FUNCTION__);
    fflush(mzxerr);
    printed = true;
  }
#endif

  unsigned int ch_x, ch_y;
  uint16_t c;

  const struct char_element *src = layer->data;
  int row;

  // Transparency vars...
  int tcol = layer->transparent_col;
  ALIGNTYPE mask = (PIXTYPE)(~0);
  ALIGNTYPE smask = BPPx1(mask) | mask;

  const uint8_t *char_ptr;
  unsigned int current_char_byte;
  unsigned int pcol;
  unsigned idx;

  ALIGNTYPE *drawPtr;
  ALIGNTYPE pix;

  ALIGNTYPE *outPtr;
  size_t align_pitch = pitch / sizeof(ALIGNTYPE);
  size_t advance_char = CHAR_W * sizeof(PIXTYPE) / sizeof(ALIGNTYPE);
  size_t advance_char_row = align_pitch * CHAR_H - advance_char * layer->w;

  int i;

  // This array will actually only store PIXTYPE values, but making it
  // ALIGNTYPE instead helps avoid some warnings.
  ALIGNTYPE char_colors[4];
  int char_idx[4];
  int ppal = graphics->protected_pal_position;
  int write_pos;

  int pixel_x;
  int pixel_y;

  ALIGNTYPE set_colors[16];
  ALIGNTYPE set_opaque[16];
  uint16_t last_fg = 0xFFFF;
  uint16_t last_bg = 0xFFFF;
  boolean has_tcol = false;
  boolean all_tcol = true;
  unsigned int byte_tcol = 0xFFFF;

  // Position the output ptr at the location of the first char
  outPtr = (ALIGNTYPE *)pixels;
  outPtr += layer->y * (int)align_pitch;
  outPtr += layer->x * (int)sizeof(PIXTYPE) / (int)sizeof(ALIGNTYPE);

  for(ch_y = 0; ch_y < layer->h; ch_y++, outPtr += advance_char_row)
  {
    for(ch_x = 0; ch_x < layer->w; ch_x++, src++, outPtr += advance_char)
    {
      c = src->char_value;

      if(CLIP)
      {
        pixel_x = layer->x + ch_x * CHAR_W;
        pixel_y = layer->y + ch_y * CHAR_H;

        if(pixel_x <= -CHAR_W || pixel_y <= -CHAR_H ||
         pixel_x >= width_px || pixel_y >= height_px)
          c = INVISIBLE_CHAR;
      }

      if(c != INVISIBLE_CHAR)
      {
        // Char values of 256+, prior to offsetting, are from the protected set
        if(c > 0xFF)
        {
          c = (c & 0xFF) + PROTECTED_CHARSET_POSITION;
        }
        else
        {
          c += layer->offset;
          c %= PROTECTED_CHARSET_POSITION;
        }

        if(src->bg_color != last_bg || src->fg_color != last_fg)
        {
          if(SMZX)
          {
            unsigned int pal = ((src->bg_color & 0xF) << 4) | (src->fg_color & 0xF);
            ALIGNTYPE masks[4];
            all_tcol = true;
            has_tcol = false;
            byte_tcol = 0xFFFF;
            for(i = 0; i < 4; i++)
            {
              char_idx[i] = graphics->smzx_indices[pal * 4 + i];

              if(BPP > 8)
                char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
              else
                char_colors[i] = char_idx[i];

              if(TR)
              {
                if(!has_tcol)
                {
                  static const uint8_t tcol_bytes[] = { 0x00, 0x55, 0xAA, 0xFF };
                  byte_tcol = tcol_bytes[i];
                }
                has_tcol |= char_idx[i] == tcol;
                all_tcol &= char_idx[i] == tcol;
                if(PPW > 2)
                  masks[i] = char_idx[i] == tcol ? 0 : smask;
              }

              // If writing more than 2 pixels at once, preemptively double
              // them. This saves having to perform this operation later...
              if(PPW > 1)
                char_colors[i] |= BPPx1(char_colors[i]);
            }
            if(PPW > 2)
            {
              set_colors_smzx<BPP, PPW>(set_colors, char_colors);
              if(TR && has_tcol)
                set_colors_smzx<BPP, PPW>(set_opaque, masks);
            }
          }
          else
          {
            char_idx[0] = select_color_16(src->bg_color, ppal);
            char_idx[1] = select_color_16(src->fg_color, ppal);

            for(i = 0; i < 2; i++)
            {
              if(BPP > 8)
                char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
              else
                char_colors[i] = (uint8_t)char_idx[i];
            }

            if(TR)
            {
              has_tcol = (char_idx[0] == tcol || char_idx[1] == tcol);
              all_tcol = (char_idx[0] == tcol && char_idx[1] == tcol);
              byte_tcol = (char_idx[1] == tcol) ? 0xFF : 0x00;
            }

            if(PPW > 1)
            {
              set_colors_mzx<BPP, PPW>(set_colors, char_colors[0], char_colors[1]);
              if(TR && has_tcol)
              {
                ALIGNTYPE m0 = !byte_tcol ? 0 : mask;
                ALIGNTYPE m1 = byte_tcol ? 0 : mask;
                set_colors_mzx<BPP, PPW>(set_opaque, m0, m1);
              }
            }
          }
        }

        // Don't bother drawing chars that are completely transparent.
        if(TR && all_tcol)
          continue;

        char_ptr = graphics->charset + (c * CHAR_H);
        drawPtr = outPtr;

        for(row = 0; row < CHAR_H; row++, drawPtr += align_pitch)
        {
          current_char_byte = char_ptr[row];

          // In mode 0 it's possible to quickly determine if an entire byte is
          // transparent. If it is, there's no point in drawing it. SMZX modes
          // can try to do a similar trick, but it won't work if the char has
          // multiple indices set to the transparent color.
          if(TR && has_tcol && current_char_byte == byte_tcol)
            continue;

          if(!CLIP || (pixel_y + row >= 0 && pixel_y + row < height_px))
          {
            for(write_pos = 0; write_pos < CHAR_W / PPW; write_pos++)
            {
              if(!CLIP ||
               ((pixel_x + write_pos * PPW >= PIXEL_X_MINIMUM) &&
                (pixel_x + write_pos * PPW < width_px)))
              {
                if(!SMZX && PPW == 1)
                {
                  pcol = !!(current_char_byte & (0x80 >> write_pos));
                  if(!TR || !has_tcol || tcol != char_idx[pcol])
                    drawPtr[write_pos] = char_colors[pcol];
                }
                else

                if(SMZX && PPW == 1)
                {
                  pcol = (current_char_byte & (0xC0 >> write_pos)) << write_pos >> 6;
                  if(TR && has_tcol && tcol == char_idx[pcol])
                  {
                    write_pos++;
                    continue;
                  }

                  pix = char_colors[pcol];
                  if(!CLIP || (pixel_x + write_pos * PPW >= 0))
                    drawPtr[write_pos] = pix;

                  write_pos++;

                  if(!CLIP || (pixel_x + write_pos * PPW < width_px))
                    drawPtr[write_pos] = pix;
                }
                else

                if(SMZX && PPW == 2)
                {
                  ALIGNTYPE shift = write_pos * PPW;
                  pcol = (current_char_byte & (0xC0 >> shift)) << shift >> 6;

                  if(!TR || !has_tcol || tcol != char_idx[pcol])
                    drawPtr[write_pos] = char_colors[pcol];
                }
                else
                {
                  idx = get_colors_index<PPW>(current_char_byte, write_pos);
                  pix = get_colors<PPW>(set_colors, idx);

                  if(TR && has_tcol)
                  {
                    ALIGNTYPE opaque = get_colors<PPW>(set_opaque, idx);
                    pix = (pix & opaque) | (drawPtr[write_pos] & ~opaque);
                  }
                  drawPtr[write_pos] = pix;
                }
              }
              else

              if(SMZX && PPW == 1) // Skip two pixels instead of 1.
                write_pos++;
            }
          }
        }
      }
    }
  }
}
