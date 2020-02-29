/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

// This file recursively instantiates template functions to generate many
// versions of the render_layer_func function. The right function to call is
// selected through a dispatch mechanism.

// Errors and warnings in this file are duplicated hundreds of times over, so
// when working on this function it can be good to redirect make's stderr
// stream to a file.

#include "platform.h"

#if __cplusplus >= 201103
#include <type_traits>
#define HAS_CONSTEXPR
#endif

template<typename PIXTYPE>
static void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int align, int smzx, int ppal, int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE>
static void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int smzx, int ppal, int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX>
static void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int ppal, int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int PPAL>
static void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int trans, int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int PPAL, int TR>
static void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int clip);

template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int PPAL, int TR, int CLIP>
static void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer);

/**
 * Alignment of pixel buffer (8bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<Uint8>(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int align, int smzx, int ppal, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<Uint8, Uint64>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<Uint8, Uint32>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;

    case 16:
      render_layer_func<Uint8, Uint16>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;

    case 8:
      render_layer_func<Uint8, Uint8>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG align=%d (8bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Alignment of pixel buffer (16bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<Uint16>(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int align, int smzx, int ppal, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<Uint16, Uint64>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<Uint16, Uint32>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;

    case 16:
      render_layer_func<Uint16, Uint16>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG align=%d (16bpp)\n", align);
      exit(1);
      break;
  }
}

/**
 * Alignment of pixel buffer (32bpp).
 * This always must be >= the current renderer's bits-per-pixel.
 */
template<>
inline void render_layer_func<Uint32>(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int align, int smzx, int ppal, int trans, int clip)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      render_layer_func<Uint32, Uint64>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

    case 32:
      render_layer_func<Uint32, Uint32>(pixels, pitch, graphics, layer,
       smzx, ppal, trans, clip);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG align=%d (32bpp)\n", align);
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
static inline void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int bpp, int align, int smzx, int ppal, int trans, int clip)
{
  switch(bpp)
  {
#ifndef SKIP_8BPP
    case 8:
      render_layer_func<Uint8>(pixels, pitch, graphics, layer,
       align, smzx, ppal, trans, clip);
      break;
#endif
#ifndef SKIP_16BPP
    case 16:
      render_layer_func<Uint16>(pixels, pitch, graphics, layer,
       align, smzx, ppal, trans, clip);
      break;
#endif
#ifndef SKIP_32BPP
    case 32:
      render_layer_func<Uint32>(pixels, pitch, graphics, layer,
       align, smzx, ppal, trans, clip);
      break;
#endif
    default:
      fprintf(stderr, "INVALID RENDERER ARG bpp=%d\n"
       "(is this bpp enabled for this platform?)\n", bpp);
      exit(1);
      break;
  }
}

/**
 * Renderer is SMZX (1) or normal MZX (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE>
static inline void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int smzx, int ppal, int trans, int clip)
{
  switch(smzx)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, 0>(pixels, pitch, graphics, layer,
       ppal, trans, clip);
      break;

    case 1:
    case 2:
    case 3:
      render_layer_func<PIXTYPE, ALIGNTYPE, 1>(pixels, pitch, graphics, layer,
       ppal, trans, clip);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG smzx=%d\n", smzx);
      exit(1);
      break;
  }
}

/**
 * Protected palette offset is 16 or 256 (MZX).
 * 256 is valid for MZX mode, but 16 is invalid for SMZX.
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX>
static void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int ppal, int trans, int clip)
{
  switch(ppal)
  {

    case 256:
    {
      // This protected palette position is valid for all SMZX modes.
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, 256>(pixels, pitch, graphics, layer,
       trans, clip);
      break;
    }

    case 16:
    {
      // NOTE: the protected palette should always be at 256 during SMZX mode,
      // so reaching this point for an SMZX layer is nonsensical. This check
      // also lets the compiler optimize these out, reducing binary size.
      if(!SMZX)
      {
        render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, 16>(pixels, pitch, graphics, layer,
         trans, clip);
        break;
      }
    }

    /* fall-through */

    default:
      fprintf(stderr, "INVALID RENDERER ARG ppal=%d (smzx=%d)\n", ppal, SMZX);
      exit(1);
      break;
  }
}

/**
 * Layer transparency enabled (1) or disabled (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int PPAL>
static inline void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int trans, int clip)
{
  switch(trans)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, PPAL, 0>(pixels, pitch, graphics, layer,
       clip);
      break;

    case 1:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, PPAL, 1>(pixels, pitch, graphics, layer,
       clip);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG trans=%d\n", trans);
      exit(1);
      break;
  }
}

/**
 * Renderer should clip the layer at the screen boundaries (1) or not (0).
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int PPAL, int TR>
static inline void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer,
 int clip)
{
  switch(clip)
  {
    case 0:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, PPAL, TR, 0>(pixels, pitch, graphics, layer);
      break;

    case 1:
      render_layer_func<PIXTYPE, ALIGNTYPE, SMZX, PPAL, TR, 1>(pixels, pitch, graphics, layer);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG clip=%d\n", clip);
      exit(1);
      break;
  }
}

/**
 * Mode 0 and UI layer color selection function.
 * This needs to be done for both colors.
 */
template<int BPP, int TR, int PPAL>
static inline int select_color_16(Uint8 color, int tcol)
{
  // Palette values >= 16, prior to offsetting, are from the protected palette.
  if((BPP > 8 || PPAL < 240) && color >= 16)
  {
    return (color - 16) % 16 + PPAL;
  }
  else

  // Check for protected palette tcols in 8bpp SMZX mode and allow this
  // special case through (as it will not be displayed).
  if(TR && BPP == 8 && PPAL >= 240 && color >= 16 &&
   ((color - 16) % 16 + PPAL) == tcol)
  {
    return tcol;
  }
  else
    return color & 0x0F;
}

/**
 * Finally, render the layer.
 * The optimizer will optimize out the unnecessary parts for relevant renderers.
 */
template<typename PIXTYPE, typename ALIGNTYPE, int SMZX, int PPAL, int TR, int CLIP>
static inline void render_layer_func(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer)
{
#ifdef HAS_CONSTEXPR
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

  // FIXME remove
  static boolean printed = false;
  if(!printed)
  {
    fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
    fflush(stderr);
    printed = true;
  }

  Uint32 ch_x, ch_y;
  Uint16 c;

  struct char_element *src = layer->data;
  int row;

  // Transparency vars...
  int tcol = layer->transparent_col;
  ALIGNTYPE bgdata;
  ALIGNTYPE mask = (PIXTYPE)(~0);

  Uint8 *char_ptr;
  Uint8 current_char_byte;
  PIXTYPE pcol;

  ALIGNTYPE *drawPtr;
  ALIGNTYPE pix;

  ALIGNTYPE *outPtr;
  Uint32 align_pitch = pitch / sizeof(ALIGNTYPE);
  Uint32 advance_char = CHAR_W * sizeof(PIXTYPE) / sizeof(ALIGNTYPE);
  Uint32 advance_char_row = align_pitch * CHAR_H - advance_char * layer->w;

  int i;

  PIXTYPE char_colors[4];
  int char_idx[4];
  int write_pos;

  int pixel_x;
  int pixel_y;

  // Position the output ptr at the location of the first char
  outPtr = (ALIGNTYPE *)pixels;
  outPtr += layer->y * (int)align_pitch;
  outPtr += layer->x * (int)sizeof(PIXTYPE) / (int)sizeof(ALIGNTYPE);

  for(ch_y = 0; ch_y < layer->h; ch_y++)
  {
    for(ch_x = 0; ch_x < layer->w; ch_x++)
    {
      c = src->char_value;

      if(CLIP)
      {
        pixel_x = layer->x + ch_x * CHAR_W;
        pixel_y = layer->y + ch_y * CHAR_H;

        if(pixel_x <= -CHAR_W || pixel_y <= -CHAR_H ||
         pixel_x >= SCREEN_PIX_W || pixel_y >= SCREEN_PIX_H)
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

        if(SMZX)
        {
          for(i = 0; i < 4; i++)
          {
            Uint16 pal = ((src->bg_color & 0xF) << 4) | (src->fg_color & 0xF);
            char_idx[i] = graphics->smzx_indices[pal * 4 + i];

            if(BPP > 8)
              char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
            else
              char_colors[i] = char_idx[i];
          }
        }
        else
        {
          char_idx[0] = select_color_16<BPP, TR, PPAL>(src->bg_color, tcol);
          char_idx[1] = select_color_16<BPP, TR, PPAL>(src->fg_color, tcol);

          for(i = 0; i < 2; i++)
          {
            if(BPP > 8)
              char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
            else
              char_colors[i] = char_idx[i];
          }
        }
        char_ptr = graphics->charset + (c * CHAR_H);
        drawPtr = outPtr;

        for(row = 0; row < CHAR_H; row++)
        {
          current_char_byte = char_ptr[row];

          if(!CLIP || (pixel_y + row >= 0 && pixel_y + row < SCREEN_PIX_H))
          {
            for(write_pos = 0; write_pos < CHAR_W / PPW; write_pos++)
            {
              if(!CLIP ||
               ((pixel_x + write_pos * PPW >= PIXEL_X_MINIMUM) &&
                (pixel_x + write_pos * PPW < SCREEN_PIX_W)))
              {
                if(!SMZX)
                {
                  if(TR)
                    bgdata = drawPtr[write_pos];

                  pix = 0;
                  for(i = 1; i <= PPW; i++)
                  {
                    //ALIGNTYPE shift = write_pos * PPW + (PPW - i);
                    //pcol = (current_char_byte & (0x80 >> shift)) >> (7 - shift);

                    // This seems to perform a little better than the old method (above).
                    pcol = !!(current_char_byte & (0x80 >> (write_pos * PPW + (PPW - i))));

                    if(TR && char_idx[pcol] == tcol)
                      pix |= bgdata & (mask << BPP * (PPW - i));
                    else
                      pix |= (ALIGNTYPE)char_colors[pcol] << BPP * (PPW - i);
                  }

                  drawPtr[write_pos] = pix;
                }
                else
                {
                  if(PPW == 1)
                  {
                    pcol = (current_char_byte & (0xC0 >> write_pos)) << write_pos >> 6;

                    pix = char_colors[pcol];
                    if(!CLIP || (pixel_x + write_pos * PPW >= 0))
                    {
                      if(TR && tcol == char_idx[pcol])
                        pix = drawPtr[write_pos];

                      drawPtr[write_pos] = pix;
                    }
                    write_pos++;

                    if(!CLIP || (pixel_x + write_pos * PPW < SCREEN_PIX_W))
                    {
                      if(TR && tcol == char_idx[pcol])
                        pix = drawPtr[write_pos];

                      drawPtr[write_pos] = pix;
                    }
                  }
                  else
                  {
                    pix = 0;
                    if(TR)
                      bgdata = drawPtr[write_pos];

                    for(i = 2; i <= PPW; i += 2)
                    {
                      ALIGNTYPE shift = write_pos * PPW + (PPW - i);

                      pcol = (current_char_byte & (0xC0 >> shift)) << shift >> 6;

                      if(TR && char_idx[pcol] == tcol)
                      {
                        pix |= bgdata &
                         ((mask << BPP * (PPW - i) |
                          (mask << BPP * (PPW - i + 1))));
                      }
                      else
                      {
                        pix |= (ALIGNTYPE)char_colors[pcol] << BPP * (PPW - i);

                        if(TR)
                          pix |= (ALIGNTYPE)char_colors[pcol] << BPP * (PPW - i + 1);
                      }
                    }

                    // GCC likes to give warnings about the left shift width
                    // here despite this being unreachable when this would be
                    // an issue. Trick it into shutting up...
                    if(!TR)
                    //pix |= (pix << BPP);
                      pix |= (pix << (BPP - 1)) << 1;

                    drawPtr[write_pos] = pix;
                  }
                }
              }
            }
          }

          drawPtr += align_pitch;
        }
      }
      src++;
      outPtr += advance_char;
    }
    outPtr += advance_char_row;
  }
}
