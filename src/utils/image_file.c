/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "image_common.h"
#include "image_file.h"
#include "image_gif.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#endif

#if 0
#define debug(...) imagedbg("IMG: " __VA_ARGS__)
#else
#define debug imagenodbg
#endif

/* TODO: hack for MegaZeux fopen_unsafe */
#undef fopen

/* Internal compat for MegaZeux boolean type. */
typedef image_bool boolean;
#ifndef __cplusplus
#undef true
#undef false
#define true IMAGE_TRUE
#define false IMAGE_FALSE
#endif

// These constraints are determined by the maximum size of a board/vlayer/MZM,
// multiplied by the number of pixels per char in a given dimension.
#define MAXIMUM_WIDTH  (32767u * 8u)
#define MAXIMUM_HEIGHT (32767u * 14u)
#define MAXIMUM_PIXELS ((16u << 20u) * 8u * 14u)

#define IMAGE_EOF -1

typedef struct imageinfo
{
  struct image_file *out;
  void *in;
  image_read_function readfn;
  int unget_chr;
}
imageinfo;

static uint16_t read16be(const uint8_t in[2])
{
  return (in[0] << 8u) | in[1];
}

static uint32_t read32be(const uint8_t in[4])
{
  return (in[0] << 24u) | (in[1] << 16u) | (in[2] << 8u) | in[3];
}

static uint16_t read16le(const uint8_t in[2])
{
  return (in[1] << 8u) | in[0];
}

static uint32_t read32le(const uint8_t in[4])
{
  return (in[3] << 24u) | (in[2] << 16u) | (in[1] << 8u) | in[0];
}

/**
 * Image dimensions check for image_init and PNG/GIF loaders.
 */
static boolean image_constraint(uint32_t width, uint32_t height)
{
  debug("Image constraint: %zu by %zu\n", (size_t)width, (size_t)height);
  if(!width || !height || width > MAXIMUM_WIDTH || height > MAXIMUM_HEIGHT ||
   (uint64_t)width * (uint64_t)height > MAXIMUM_PIXELS)
    return false;

  return true;
}

/**
 * Initialize an image file's pixel array and other values.
 */
static boolean image_init(uint32_t width, uint32_t height, struct image_file *dest)
{
  struct rgba_color *data;

  if(!image_constraint(width, height))
    return false;

  data = (struct rgba_color *)calloc(width * height, sizeof(struct rgba_color));
  if(!data)
    return false;

  dest->width = width;
  dest->height = height;
  dest->data = data;
  return true;
}

/**
 * Free an image struct's allocated data. Does not modify any other fields.
 */
void image_free(struct image_file *dest)
{
  free(dest->data);
  dest->data = NULL;
}


#ifdef CONFIG_PNG

/**
 * PNG loader.
 */

#include <setjmp.h>
#include <png.h>

#if PNG_LIBPNG_VER < 10504
#define png_set_scale_16(p) png_set_strip_16(p)
#endif

static void png_read_fn(png_struct *png, png_byte *dest, size_t count)
{
  imageinfo *s = (imageinfo *)png_get_io_ptr(png);
  if(s->readfn(dest, count, s->in) < count)
    png_error(png, "eof");
}

static boolean load_png(imageinfo *s)
{
  struct image_file *dest = s->out;
  png_struct *png = NULL;
  png_info *info = NULL;
  png_byte ** volatile row_ptrs = NULL;
  png_uint_32 w;
  png_uint_32 h;
  png_uint_32 i;
  png_uint_32 j;
  int bit_depth;
  int color_type;

  debug("Image type: PNG\n");
  dest->data = NULL;

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png)
    return false;
  info = png_create_info_struct(png);
  if(!info)
    goto error;

  if(setjmp(png_jmpbuf(png)))
    goto error;

  png_set_read_fn(png, s, png_read_fn);
  png_set_sig_bytes(png, 8);

  png_read_info(png, info);
  png_get_IHDR(png, info, &w, &h, &bit_depth, &color_type, NULL, NULL, NULL);

  if(!image_init(w, h, dest))
    goto error;

  row_ptrs = (png_byte **)malloc(sizeof(png_byte *) * h);
  if(!row_ptrs)
    goto error;

  for(i = 0, j = 0; i < h; i++, j += w)
    row_ptrs[i] = (png_byte *)(dest->data + j);

  /* This SHOULD convert everything to RGBA32.
   * See the far too complicated table in libpng-manual.txt for more info. */
  if(bit_depth == 16)
    png_set_scale_16(png);
  if(color_type & PNG_COLOR_MASK_PALETTE)
    png_set_palette_to_rgb(png);
  if(!(color_type & PNG_COLOR_MASK_COLOR))
    png_set_gray_to_rgb(png);
  if(!(color_type & PNG_COLOR_MASK_ALPHA))
    png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
  if(png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  png_read_image(png, row_ptrs);
  png_read_end(png, NULL);
  png_destroy_read_struct(&png, &info, NULL);

  free(row_ptrs);
  dest->width = w;
  dest->height = h;
  return true;

error:
  png_destroy_read_struct(&png, info ? &info : NULL, NULL);

  free(dest->data);
  free(row_ptrs);
  return false;
}

#endif /* CONFIG_PNG */


/**
 * GIF loader.
 */

static boolean load_gif(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct gif_info gif;
  struct gif_rgba *pixels;
  enum gif_error ret;
  unsigned width;
  unsigned height;

  debug("Image type: GIF\n");

  ret = gif_read(&gif, s->in, s->readfn, true);
  if(ret != GIF_OK)
  {
    debug("read failed: %s\n", gif_error_string(ret));
    return false;
  }

  gif_composite_size(&width, &height, &gif);

  if(!image_constraint(width, height))
  {
    gif_close(&gif);
    return false;
  }

  ret = gif_composite(&pixels, &gif, malloc);
  if(ret == GIF_OK)
  {
    dest->data = (struct rgba_color *)pixels;
    dest->width = width;
    dest->height = height;
  }
  else
    debug("composite failed: %s\n", gif_error_string(ret));

  gif_close(&gif);
  return (ret == GIF_OK);
}


/**
 * BMP loader.
 */

#define BMP_HEADER_LEN  14
#define BMP_DIB_MAX_LEN 128

enum dib_type
{
  DIB_UNKNOWN,
  BITMAPCOREHEADER,     // 12
  OS22XBITMAPHEADER,    // 64 or 16
  BITMAPINFOHEADER,     // 40
  BITMAPV2INFOHEADER,   // 52
  BITMAPV3INFOHEADER,   // 56
  BITMAPV4HEADER,       // 108
  BITMAPV5HEADER,       // 124
};

enum dib_compression
{
  BI_RGB,             // None
  BI_RLE8,            // 8bpp RLE
  BI_RLE4,            // 4bpp RLE
  BI_BITFIELDS,       // RGB or RGBA bitfield masks, huffman
  BI_JPEG,            // JPEG
  BI_PNG,             // PNG
  BI_ALPHABITFIELDS,  // RGBA bitfield masks
  BI_CMYK     = 11,
  BI_BMYKRLE8 = 12,
  BI_CMYKRLE4 = 13,
  BI_MAX
};

// NOTE: not packed, don't fread directly into this.
struct bmp_header
{
  uint16_t magic;
  uint32_t filesize;
  uint16_t reserved0;
  uint16_t reserved1;
  uint32_t pixarray;

  struct rgba_color *color_table;
  uint32_t streamidx;
  uint32_t rowpadding;
  enum dib_type type;

  // Core DIB header.
  uint32_t dibsize;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bpp;

  // BITMAPINFOHEADER fields.
  uint32_t compr_method;
  uint32_t image_size;
  uint32_t horiz_res;
  uint32_t vert_res;
  uint32_t palette_size;
  uint32_t important;
};

static enum dib_type bmp_get_dib_type(uint32_t length)
{
  switch(length)
  {
    case 12:  return BITMAPCOREHEADER;
    case 64:  return OS22XBITMAPHEADER;
    case 16:  return OS22XBITMAPHEADER;
    case 40:  return BITMAPINFOHEADER;
    case 52:  return BITMAPV2INFOHEADER;
    case 56:  return BITMAPV3INFOHEADER;
    case 108: return BITMAPV4HEADER;
    case 124: return BITMAPV5HEADER;
    default:  return DIB_UNKNOWN;
  }
}

static struct rgba_color *bmp_read_color_table(struct bmp_header *bmp, imageinfo *s)
{
  uint32_t palette_alloc = (1 << bmp->bpp);
  uint32_t palette_size = bmp->palette_size;
  uint32_t i;
  size_t col_len;
  uint8_t d[4];

  struct rgba_color *color_table = (struct rgba_color *)calloc(palette_alloc, sizeof(struct rgba_color));
  struct rgba_color *pos = color_table;
  if(!color_table)
    return NULL;

  // Ignore values after the allocated palette if they exist...
  if(palette_size > palette_alloc)
    palette_size = palette_alloc;

  // OS/2 1.x uses RGB, everything else uses XRGB.
  // (Bitfields may change this, but currently aren't supported.)
  if(bmp->type == BITMAPCOREHEADER)
  {
    col_len = 3;
  }
  else
    col_len = 4;

  for(i = 0; i < palette_size; i++)
  {
    if(s->readfn(d, col_len, s->in) < col_len)
      break;

    pos->r = d[2];
    pos->g = d[1];
    pos->b = d[0];
    pos->a = 255;
    pos++;
  }
  bmp->streamidx += col_len * palette_size;

  if(i < palette_size)
  {
    free(color_table);
    return NULL;
  }

  return color_table;
}

static boolean bmp_pixarray_u1(const struct bmp_header *bmp, imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;
  int i;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width;)
    {
      char v;
      int value;
      if(s->readfn(&v, 1, s->in) < 1)
        return false;

      for(value = v, i = 7; i >= 0 && x < bmp->width; i--, x++)
      {
        int idx = (value >> i) & 0x01;
        *(pos++) = color_table[idx];
      }
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return false;
  }
  return true;
}

static boolean bmp_pixarray_u2(const struct bmp_header *bmp, imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;
  int i;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width;)
    {
      char v;
      int value;
      if(s->readfn(&v, 1, s->in) < 1)
        return false;

      for(value = v, i = 6; i >= 0 && x < bmp->width; i -= 2, x++)
      {
        int idx = (value >> i) & 0x03;
        *(pos++) = color_table[idx];
      }
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return false;
  }
  return true;
}

static boolean bmp_pixarray_u4(const struct bmp_header *bmp, imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x += 2)
    {
      char value;
      if(s->readfn(&value, 1, s->in) < 1)
        return false;

      d[0] = (value >> 0) & 0x0f;
      d[1] = (value >> 4) & 0x0f;

      *(pos++) = color_table[d[0]];
      if(x + 1 < bmp->width)
        *(pos++) = color_table[d[1]];
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return false;
  }
  return true;
}

static boolean bmp_pixarray_u8(const struct bmp_header *bmp, imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      uint8_t value;
      if(s->readfn(&value, 1, s->in) < 1)
        return false;

      *(pos++) = color_table[value];
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return false;
  }
  return true;
}

static boolean bmp_pixarray_u16(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      uint16_t value;
      if(s->readfn(d, 2, s->in) < 2)
        return false;

      value = read16le(d);

      pos->r = (((value >> 10) & 0x1f) * 255u + 16) / 31u;
      pos->g = (((value >>  5) & 0x1f) * 255u + 16) / 31u;
      pos->b = (((value >>  0) & 0x1f) * 255u + 16) / 31u;
      pos->a = 255;
      pos++;
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return false;
  }
  return true;
}

static boolean bmp_pixarray_u24(const struct bmp_header *bmp, imageinfo * RESTRICT s)
{
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      if(s->readfn(d, 3, s->in) < 3)
        return false;

      pos->r = d[2];
      pos->g = d[1];
      pos->b = d[0];
      pos->a = 255;
      pos++;
    }

    if(bmp->rowpadding && s->readfn(d, bmp->rowpadding, s->in) < bmp->rowpadding)
      return false;
  }
  return true;
}

static boolean bmp_pixarray_u32(const struct bmp_header *bmp, imageinfo * RESTRICT s)
{
  struct image_file *dest = s->out;
  uint8_t d[4];
  ssize_t y;
  ssize_t x;

  for(y = bmp->height - 1; y >= 0; y--)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width]);

    for(x = 0; x < bmp->width; x++)
    {
      if(s->readfn(d, 4, s->in) < 4)
        return false;

      pos->r = d[2];
      pos->g = d[1];
      pos->b = d[0];
      pos->a = 255;
      pos++;
    }
    // 32bpp is always aligned, no padding required.
  }
  return true;
}

static boolean bmp_read_pixarray_uncompressed(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  switch(bmp->bpp)
  {
    case 1:   return bmp_pixarray_u1(bmp, s);
    case 2:   return bmp_pixarray_u2(bmp, s);
    case 4:   return bmp_pixarray_u4(bmp, s);
    case 8:   return bmp_pixarray_u8(bmp, s);
    case 16:  return bmp_pixarray_u16(bmp, s);
    case 24:  return bmp_pixarray_u24(bmp, s);
    case 32:  return bmp_pixarray_u32(bmp, s);
  }
  return false;
}

/**
 * Microsoft RLE8 and RLE4 compression.
 */
static boolean bmp_read_pixarray_rle(const struct bmp_header *bmp,
 imageinfo * RESTRICT s)
{
  const struct rgba_color *color_table = bmp->color_table;
  struct image_file *dest = s->out;
  ssize_t x = 0;
  ssize_t y = bmp->height - 1;
  ssize_t i;
  uint8_t d[2];

  if(bmp->bpp != 4 && bmp->bpp != 8) // Should never happen...
    return false;

  while(x < bmp->width && y >= 0)
  {
    struct rgba_color *pos = &(dest->data[y * bmp->width + x]);
    //debug("RLE%u now at: %zd %zd\n", bmp->bpp, x, y);

    while(true)
    {
      if(s->readfn(d, 2, s->in) < 2)
        return false;

      if(!d[0])
      {
        if(d[1] == 0)
        {
          // End of line.
          //debug("RLE%u EOL  : %3u %3u\n", bmp->bpp, d[0], d[1]);
          x = 0;
          y--;
          break;
        }
        else

        if(d[1] == 1)
        {
          // End of file.
          //debug("RLE%u EOF  : %3u %3u\n", bmp->bpp, d[0], d[1]);
          return true;
        }
        else

        if(d[1] == 2)
        {
          // Position delta.
          // Documentation refers to unsigned y moving the current position
          // "down", but since this is still OS/2 line order, it means "up".
          if(s->readfn(d, 2, s->in) < 2)
            return false;

          //debug("RLE%u delta: %3u %3u\n", bmp->bpp, d[0], d[1]);
          x += d[0];
          y -= d[1];
          break;
        }
        else
        {
          // Absolute mode.
          uint8_t idx;
          //debug("RLE%u abs. : %3u %3u\n", bmp->bpp, d[0], d[1]);
          if(bmp->bpp == 8)
          {
            for(i = 0; i < d[1] && x < bmp->width; i++, x++)
            {
              if(s->readfn(&idx, 1, s->in) < 1)
                return false;

              *(pos++) = color_table[idx];
            }
            // Absolute mode runs are padded to word boundaries.
            if(d[1] & 1)
              s->readfn(&idx, 1, s->in);
          }
          else // bpp == 4
          {
            for(i = 0; i < d[1] && x < bmp->width;)
            {
              if(s->readfn(&idx, 1, s->in) < 1)
                return false;

              *(pos++) = color_table[(idx >> 4) & 0x0f];
              i++, x++;

              if(i < d[1] && x < bmp->width)
              {
                *(pos++) = color_table[(idx >> 0) & 0x0f];
                i++, x++;
              }
            }
            // Absolute mode runs are padded to word boundaries.
            if((d[1] & 0x03) == 1 || (d[1] & 0x03) == 2)
              s->readfn(&idx, 1, s->in);
          }

          if(i < d[1])
          {
            debug("RLE%u reached x=bmp->width during absolute run (should this be valid)?\n",
             bmp->bpp);
            return false;
          }
        }
      }
      else
      {
        // Run.
        //debug("RLE%u run  : %3u %02xh\n", bmp->bpp, d[0], d[1]);
        if(bmp->bpp == 8)
        {
          for(i = 0; i < d[0] && x < bmp->width; i++, x++)
            *(pos++) = color_table[d[1]];
        }
        else // bpp == 4
        {
          uint8_t a = (d[1] >> 4) & 0x0f;
          uint8_t b = (d[1] >> 0) & 0x0f;
          for(i = 0; i < d[0] && x < bmp->width;)
          {
            *(pos++) = color_table[a];
            i++, x++;

            if(i < d[0] && x < bmp->width)
            {
              *(pos++) = color_table[b];
              i++, x++;
            }
          }
        }

        if(i < d[0])
        {
          debug("RLE%u reached x=bmp->width during encoded run (should this be valid)?\n",
           bmp->bpp);
          return false;
        }
      }
    }
  }

  // Check for EOF following EOL.
  if(x == 0 && y == -1)
  {
    if(s->readfn(d, 2, s->in) < 2 || d[0] != 0 || d[1] != 1)
      debug("RLE%u missing EOF!\n", bmp->bpp);

    return true;
  }

  debug("RLE%u out of bounds\n", bmp->bpp);
  return false;
}

static boolean load_bmp(imageinfo *s)
{
  struct image_file *dest = s->out;
  uint8_t buf[BMP_DIB_MAX_LEN] = { 'B', 'M' };
  struct bmp_header bmp;

  debug("Image type: BMP\n");

  // Note: already read magic bytes.
  if(s->readfn(buf + 2, BMP_HEADER_LEN - 2, s->in) < BMP_HEADER_LEN - 2)
    return false;

  memset(&bmp, 0, sizeof(bmp));

  bmp.filesize  = read32le(buf + 2);
  bmp.reserved0 = read16le(buf + 6);
  bmp.reserved0 = read16le(buf + 8);
  bmp.pixarray  = read32le(buf + 10);
  bmp.streamidx = 14;

  // DIB header size.
  if(s->readfn(buf, 4, s->in) < 4)
    return false;

  bmp.dibsize = read32le(buf + 0);
  bmp.type = bmp_get_dib_type(bmp.dibsize);

  if(bmp.type == DIB_UNKNOWN)
  {
    debug("Unknown BMP DIB header size %zu!\n", (size_t)bmp.dibsize);
    return false;
  }

  if(s->readfn(buf + 4, bmp.dibsize - 4, s->in) < bmp.dibsize - 4)
    return false;

  if(bmp.type == BITMAPCOREHEADER)
  {
    debug("BITMAPCOREHEADER\n");
    bmp.width           = read16le(buf + 4);
    bmp.height          = read16le(buf + 6);
    bmp.planes          = read16le(buf + 8);
    bmp.bpp             = read16le(buf + 10);
  }
  else
  {
    debug("BITMAPINFOHEADER or compatible\n");
    bmp.width           = (int32_t)read32le(buf + 4);
    bmp.height          = (int32_t)read32le(buf + 8);
    bmp.planes          = read16le(buf + 12);
    bmp.bpp             = read16le(buf + 14);

    if(bmp.dibsize > 16)
    {
      bmp.compr_method  = read32le(buf + 16);
      bmp.image_size    = read32le(buf + 20);
      bmp.horiz_res     = read32le(buf + 24);
      bmp.vert_res      = read32le(buf + 28);
      bmp.palette_size  = read32le(buf + 32);
      bmp.important     = read32le(buf + 36);
    }
  }
  bmp.streamidx += bmp.dibsize;

  if(bmp.width < 0 || bmp.height < 0)
  {
    debug("invalid BMP dimensions %zd x %zd", (ssize_t)bmp.width, (ssize_t)bmp.height);
    return false;
  }

  if(bmp.planes != 1)
  {
    debug("invalid BMP planes %u\n", bmp.planes);
    return false;
  }

  if(bmp.compr_method != BI_RGB &&
   bmp.compr_method != BI_RLE8 && bmp.compr_method != BI_RLE4)
  {
    debug("unsupported BMP compression type %zu\n", (size_t)bmp.compr_method);
    return false;
  }
  debug("Compression: %zu\n", (size_t)bmp.compr_method);

  if(bmp.bpp != 1 && bmp.bpp != 2 && bmp.bpp != 4 && bmp.bpp != 8 &&
   bmp.bpp != 16 && bmp.bpp != 24 && bmp.bpp != 32)
  {
    debug("unsupported BMP BPP %u\n", bmp.bpp);
    return false;
  }
  debug("BPP: %u\n", bmp.bpp);

  if(bmp.compr_method == BI_RLE8 && bmp.bpp != 8)
  {
    debug("unsupported BPP %u for RLE8\n", bmp.bpp);
    return false;
  }

  if(bmp.compr_method == BI_RLE4 && bmp.bpp != 4)
  {
    debug("unsupported BPP %u for RLE4\n", bmp.bpp);
    return false;
  }

  if(bmp.bpp <= 8)
  {
    // If palette_size == 0, use 2^bpp.
    if(!bmp.palette_size)
    {
      bmp.palette_size = (1 << bmp.bpp);
      debug("Palette size: %zu (default)\n", (size_t)bmp.palette_size);
    }
    else
      debug("Palette size: %zu\n", (size_t)bmp.palette_size);

    // Read color table.
    bmp.color_table = bmp_read_color_table(&bmp, s);
    if(!bmp.color_table)
    {
      debug("failed to read BMP color table\n");
      return false;
    }
  }

  // Padding: round up row size (bpp*width) to a whole number of bytes. Padding
  // size is 0 to 3 extra bytes required to pad that value to a multiple of 4.
  bmp.rowpadding = (4 - (((bmp.bpp * (uint64_t)bmp.width + 7) / 8))) & 3;
  debug("Row padding: %zu\n", (size_t)bmp.rowpadding);

  if(bmp.streamidx < bmp.pixarray)
    debug("Bytes to skip for pixarray: %zu\n", (size_t)bmp.pixarray - bmp.streamidx);

  // Assume non-seeking stream, skip everything prior to pixarray with fread.
  while(bmp.streamidx < bmp.pixarray)
  {
    size_t r = IMAGE_MIN(bmp.pixarray - bmp.streamidx, sizeof(buf));
    bmp.streamidx += r;

    if(s->readfn(buf, 1, s->in) < 1)
      goto err_free;
  }

  if(!image_init(bmp.width, bmp.height, dest))
    goto err_free;

  switch(bmp.compr_method)
  {
    case BI_RGB:
      if(!bmp_read_pixarray_uncompressed(&bmp, s))
        goto err_free_image;
      break;

    // ImageMagick tends to automatically emit this for 8bpp BMPs.
    // GIMP can emit both types of RLE, though this is turned off by default.
    case BI_RLE8:
    case BI_RLE4:
      if(!bmp_read_pixarray_rle(&bmp, s))
        goto err_free_image;
      break;

    default:
      goto err_free_image; // Shouldn't happen--method is filtered above!
  }

  free(bmp.color_table);
  return true;

err_free_image:
  debug("error reading pixarray\n");
  image_free(dest);
err_free:
  free(bmp.color_table);
  return false;
}


/**
 * NetPBM loaders.
 */

#define ROUND_32 (1u << 31u)

static int next_byte(imageinfo *s)
{
  uint8_t chr;
  if(s->unget_chr >= 0)
  {
    int tmp = s->unget_chr;
    s->unget_chr = -1;
    return tmp;
  }

  if(s->readfn(&chr, 1, s->in) < 1)
    return IMAGE_EOF;

  return chr;
}

static boolean skip_whitespace(imageinfo *s)
{
  int value = next_byte(s);
  if(value < 0)
    return false;

  if(value == '#')
  {
    // Skip line comment...
    while(value != '\n' && value >= 0)
      value = next_byte(s);

    return true;
  }

  if(isspace(value))
    return true;

  s->unget_chr = value;
  return false;
}

static int next_number(uint32_t *output, imageinfo *s)
{
  int value;
  int count;
  *output = 0;

  while(skip_whitespace(s));

  count = 0;
  for(count = 0; count <= 10; count++)
  {
    value = next_byte(s);
    if(!isdigit(value))
    {
      if(value == '#')
        skip_whitespace(s);
      else

      if(!isspace(value))
        break;

      return value;
    }

    *output = (*output * 10) + (value - '0');
  }

  // Digit count overflowed the uint32_t return value, or invalid character.
  return IMAGE_EOF;
}

static int next_value(uint32_t maxval, imageinfo *s)
{
  uint32_t v;
  if(next_number(&v, s) < 0 || v > maxval)
    return IMAGE_EOF;

  return v;
}

static int next_bit_value(imageinfo *s)
{
  int v;

  // Unlike plaintext PGM and PPM, plaintext PBM doesn't require whitespace to
  // separate components--each component is always a single digit character.
  while(skip_whitespace(s));

  v = next_byte(s);
  if(v != '0' && v != '1')
    return IMAGE_EOF;

  return (v - '0');
}

static int next_binary(uint32_t maxval, imageinfo *s)
{
  uint32_t v;
  if(maxval > 255)
    v = (next_byte(s) << 8) | next_byte(s);
  else
    v = next_byte(s);

  return (v > maxval) ? IMAGE_EOF : (int)v;
}

static char *next_line(char *dest, int dest_len, imageinfo *s)
{
  char *pos = dest;
  int num = 1;
  int v = -1;
  while(num < dest_len)
  {
    v = next_byte(s);
    if(v < 0)
    {
      if(num == 1)
        return NULL;
      break;
    }
    if(v == '\r' || v == '\n')
      break;

    *(pos++) = v;
    num++;
  }
  if(v == '\r')
  {
    v = next_byte(s);
    if(v != '\n')
      s->unget_chr = v;
  }
  *pos = '\0';
  return dest;
}

static boolean pbm_header(uint32_t *width, uint32_t *height, imageinfo *s)
{
  // NOTE: already read magic.
  uint32_t w = 0;
  uint32_t h = 0;

  if(next_number(&w, s) < 0 ||
   next_number(&h, s) < 0)
    return false;

  // Skip exactly one whitespace (for binary files).
  skip_whitespace(s);

  *width = w;
  *height = h;
  return true;
}

static boolean ppm_header(uint32_t *width, uint32_t *height, uint32_t *maxval,
 imageinfo *s)
{
  // NOTE: already read magic. PGM and PPM use the same header format.
  uint32_t w = 0;
  uint32_t h = 0;
  uint32_t m = 0;

  if(next_number(&w, s) < 0 ||
   next_number(&h, s) < 0 ||
   next_number(&m, s) < 0)
    return false;

  // Skip exactly one whitespace (for binary files).
  skip_whitespace(s);

  if(!m || m > 65535)
    return false;

  *width = w;
  *height = h;
  *maxval = m;
  return true;
}

static boolean load_portable_bitmap_plain(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t size;
  uint32_t i;

  debug("Image type: PBM (P1)\n");

  if(!pbm_header(&width, &height, s))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;

  for(i = 0; i < size; i++)
  {
    int val = next_bit_value(s);
    if(val < 0)
      break;

    val = val ? 0 : 255;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_bitmap_binary(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t x;
  uint32_t y;

  debug("Image type: PBM (P4)\n");

  if(!pbm_header(&width, &height, s))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;

  // Each row is padded to a whole number of bytes...
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width;)
    {
      int val = next_byte(s);
      int mask;
      if(val < 0)
        break;

      for(mask = 0x80; mask && x < width; mask >>= 1, x++)
      {
        int pixval = (val & mask) ? 0 : 255;
        pos->r = pixval;
        pos->g = pixval;
        pos->b = pixval;
        pos->a = 255;
        pos++;
      }
    }
  }
  return true;
}

static boolean load_portable_greymap_plain(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PGM (P2)\n");

  if(!ppm_header(&width, &height, &maxval, s))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int val = next_value(maxval, s);
    if(val < 0)
      break;

    val = (val * maxscale + ROUND_32) >> 32u;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_greymap_binary(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PGM (P5)\n");

  if(!ppm_header(&width, &height, &maxval, s))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int val = next_binary(maxval, s);
    if(val < 0)
      break;

    val = (val * maxscale + ROUND_32) >> 32u;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_pixmap_plain(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PPM (P3)\n");

  if(!ppm_header(&width, &height, &maxval, s))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int r = next_value(maxval, s);
    int g = next_value(maxval, s);
    int b = next_value(maxval, s);
    if(r < 0 || g < 0 || b < 0)
      break;

    pos->r = (r * maxscale + ROUND_32) >> 32u;
    pos->g = (g * maxscale + ROUND_32) >> 32u;
    pos->b = (b * maxscale + ROUND_32) >> 32u;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_pixmap_binary(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PPM (P6)\n");

  if(!ppm_header(&width, &height, &maxval, s))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int r = next_binary(maxval, s);
    int g = next_binary(maxval, s);
    int b = next_binary(maxval, s);
    if(r < 0 || g < 0 || b < 0)
      break;

    pos->r = (r * maxscale + ROUND_32) >> 32u;
    pos->g = (g * maxscale + ROUND_32) >> 32u;
    pos->b = (b * maxscale + ROUND_32) >> 32u;
    pos->a = 255;
    pos++;
  }
  return true;
}

struct pam_tupl
{
  const char *name;
  uint8_t mindepth;
  uint8_t red_pos;
  uint8_t green_pos;
  uint8_t blue_pos;
  int8_t alpha_pos;
};

static boolean pam_set_tupltype(struct pam_tupl *dest, const char *tuplstr)
{
  static const struct pam_tupl tupltypes[] =
  {
    { "BLACKANDWHITE",        1, 0, 0, 0, -1 },
    { "GRAYSCALE",            1, 0, 0, 0, -1 },
    { "RGB",                  3, 0, 1, 2, -1 },
    { "BLACKANDWHITE_ALPHA",  2, 0, 0, 0,  1 },
    { "GRAYSCALE_ALPHA",      2, 0, 0, 0,  1 },
    { "RGB_ALPHA",            4, 0, 1, 2,  3 },
  };

  size_t i;
  for(i = 0; i < ARRAY_SIZE(tupltypes); i++)
  {
    if(!strcasecmp(tupltypes[i].name, tuplstr))
    {
      *dest = tupltypes[i];
      return true;
    }
  }
  debug("unsupported TUPLTYPE '%s'!\n", tuplstr);
  return false;
}

static boolean pam_header(uint32_t *width, uint32_t *height, uint32_t *maxval,
 uint32_t *depth, struct pam_tupl *tupltype, imageinfo *s)
{
  char linebuf[256];
  char tuplstr[256];
  char *key;
  char *value;
  boolean has_w = false;
  boolean has_h = false;
  boolean has_m = false;
  boolean has_d = false;
  boolean has_e = false;
  boolean has_tu = false;

  while(next_line(linebuf, 256, s))
  {
    value = linebuf;
    while(isspace(*value))
      value++;

    if(!*value || *value == '#')
      continue;

    key = value;
    while(*value && !isspace(*value))
      value++;

    if(*value)
      *(value++) = '\0';

    if(!strcasecmp(key, "ENDHDR"))
    {
      has_e = true;
      break;
    }
    else

    if(!strcasecmp(key, "WIDTH"))
    {
      if(!*value || has_w)
        return false;

      *width = strtoul(value, NULL, 10);
      has_w = true;
    }
    else

    if(!strcasecmp(key, "HEIGHT"))
    {
      if(!*value || has_h)
        return false;

      *height = strtoul(value, NULL, 10);
      has_h = true;
    }
    else

    if(!strcasecmp(key, "DEPTH"))
    {
      if(!*value || has_d)
        return false;

      *depth = strtoul(value, NULL, 10);
      has_d = true;
    }
    else

    if(!strcasecmp(key, "MAXVAL"))
    {
      if(!*value || has_m)
        return false;

      *maxval = strtoul(value, NULL, 10);
      has_m = true;
    }
    else

    if(!strcasecmp(key, "TUPLTYPE"))
    {
      // NOTE: the spec supports combining multiple of these, but doesn't use it.
      if(!*value || has_tu)
        return false;

      // NOTE: tuplstr size MUST be >= linebuf size.
      strcpy(tuplstr, value);
      has_tu = true;
    }
  }

  if(!has_w || !has_h || !has_d || !has_m || !has_e)
    return false;

  if(*depth < 1 || *depth > 4)
  {
    debug("invalid depth '%u'!\n", (unsigned int)*depth);
    return false;
  }

  if(*maxval < 1 || *maxval > 65535)
  {
    debug("invalid maxval '%u'!\n", (unsigned int)*maxval);
    return false;
  }

  if(has_tu)
    has_tu = pam_set_tupltype(tupltype, tuplstr);

  if(!has_tu)
  {
    // Guess TUPLTYPE from depth.
    const char *guess = "";
    switch(*depth)
    {
      case 1: guess = "GRAYSCALE"; break;
      case 2: guess = "GRAYSCALE_ALPHA"; break;
      case 3: guess = "RGB"; break;
      case 4: guess = "RGB_ALPHA"; break;
    }
    if(!pam_set_tupltype(tupltype, guess))
      return false;
  }

  if(tupltype->mindepth > *depth)
    return false;

  return true;
}

static boolean load_portable_arbitrary_map(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  struct pam_tupl tupltype;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;
  int v[4] = { 0 };

  debug("Image type: PAM (P7)\n");

  if(!pam_header(&width, &height, &maxval, &depth, &tupltype, s))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    uint32_t j;

    for(j = 0; j < depth; j++)
    {
      v[j] = next_binary(maxval, s);
      if(v[j] < 0)
        return true;

      v[j] = (v[j] * maxscale + ROUND_32) >> 32u;
    }

    pos->r = v[tupltype.red_pos];
    pos->g = v[tupltype.green_pos];
    pos->b = v[tupltype.blue_pos];
    pos->a = tupltype.alpha_pos >= 0 ? v[tupltype.alpha_pos] : 255;
    pos++;
  }
  return true;
}


/**
 * Farbfeld loader.
 */

static uint8_t convert_16b_to_8b(uint16_t component)
{
  return (uint32_t)(component * 255u + 32768u) / 65535u;
}

static boolean load_farbfeld(imageinfo *s)
{
  struct image_file *dest = s->out;
  struct rgba_color *pos;
  uint8_t buf[8];
  uint32_t width;
  uint32_t height;
  uint32_t size;
  uint32_t i;

  debug("Image type: farbfeld\n");

  if(s->readfn(buf, 8, s->in) < 8)
    return false;

  width  = read32be(buf + 0);
  height = read32be(buf + 4);
  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = width * height;

  for(i = 0; i < size; i++)
  {
    if(s->readfn(buf, 8, s->in) < 8)
      break;

    pos->r = convert_16b_to_8b(read16be(buf + 0));
    pos->g = convert_16b_to_8b(read16be(buf + 2));
    pos->b = convert_16b_to_8b(read16be(buf + 4));
    pos->a = convert_16b_to_8b(read16be(buf + 6));
    pos++;
  }
  return true;
}


/**
 * Raw format loader.
 * `not_magic` is the first few bytes of the file read for the purposes of
 * identifying a file format. For raw files, this is actually pixel data.
 */
static boolean load_raw(imageinfo *s,
 const struct image_raw_format *format, const uint8_t not_magic[8])
{
  struct image_file *dest = s->out;
  struct rgba_color *dest_pos;
  uint8_t *src_pos;
  uint8_t *data;
  uint64_t size = (uint64_t)format->width * format->height;
  uint64_t raw_size = size * format->bytes_per_pixel;
  uint64_t i;
  char tmp;

  if(format->bytes_per_pixel < 1 || format->bytes_per_pixel > 4)
  {
    debug("unsupported raw bytes per pixel %zu\n", (size_t)format->bytes_per_pixel);
    return false;
  }

  debug("checking if this is a raw file with properties: %zu %zu %zu\n",
   (size_t)format->width, (size_t)format->height, (size_t)format->bytes_per_pixel);

  data = (uint8_t *)malloc(raw_size);
  if(!data)
  {
    debug("can't allocate memory for raw image with given size\n");
    return false;
  }

  memcpy(data, not_magic, 8);
  if(s->readfn(data + 8, raw_size - 8, s->in) < raw_size - 8)
  {
    debug("failed to read required data for raw image\n");
    goto err_free;
  }

  debug("Image type: raw\n");

  if(s->readfn(&tmp, 1, s->in) != 0)
    debug("data exists after expected EOF in raw file\n");

  if(!image_init(format->width, format->height, dest))
    goto err_free;

  dest_pos = dest->data;
  src_pos = data;
  for(i = 0; i < size; i++)
  {
    uint8_t r, g, b, a;
    if(format->bytes_per_pixel < 3)
    {
      r = src_pos[0];
      g = src_pos[0];
      b = src_pos[0];
      a = (format->bytes_per_pixel == 2) ? src_pos[1] : 255;
    }
    else
    {
      r = src_pos[0];
      g = src_pos[1];
      b = src_pos[2];
      a = (format->bytes_per_pixel == 4) ? src_pos[3] : 255;
    }

    dest_pos->r = r;
    dest_pos->g = g;
    dest_pos->b = b;
    dest_pos->a = a;
    dest_pos++;
    src_pos += format->bytes_per_pixel;
  }

  free(data);
  return true;

err_free:
  free(data);
  return false;
}


/**
 * Common file handling.
 */

#define MAGIC(a,b) (((a) << 8) | (b))

/**
 * Load an image from a stream.
 * Assume this stream can NOT be seeked.
 */
boolean load_image_from_stream(void *fp, image_read_function readfn,
 struct image_file *dest, const struct image_raw_format *raw_format)
{
  imageinfo s = { dest, fp, readfn, -1 };
  uint8_t magic[8];

  memset(dest, 0, sizeof(struct image_file));

  if(readfn(magic, 2, fp) < 2)
    return false;

  switch(MAGIC(magic[0], magic[1]))
  {
    /* BMP */
    case MAGIC('B','M'): return load_bmp(&s);

    /* NetPBM */
    case MAGIC('P','1'): return load_portable_bitmap_plain(&s);
    case MAGIC('P','2'): return load_portable_greymap_plain(&s);
    case MAGIC('P','3'): return load_portable_pixmap_plain(&s);
    case MAGIC('P','4'): return load_portable_bitmap_binary(&s);
    case MAGIC('P','5'): return load_portable_greymap_binary(&s);
    case MAGIC('P','6'): return load_portable_pixmap_binary(&s);
    case MAGIC('P','7'): return load_portable_arbitrary_map(&s);
  }

  if(readfn(magic + 2, 1, fp) < 1)
    return false;

  /* GIF */
  if(!memcmp(magic, "GIF", 3))
    return load_gif(&s);

  if(readfn(magic + 3, 5, fp) < 5)
    return false;

  /* farbfeld */
  if(!memcmp(magic, "farbfeld", 8))
    return load_farbfeld(&s);

#ifdef CONFIG_PNG

  /* PNG (via libpng). */
  if(png_sig_cmp(magic, 0, 8) == 0)
    return load_png(&s);

#else

  if(!memcmp(magic, "\x89PNG\r\n\x1A\n", 8))
    debug("MegaZeux utils were compiled without PNG support--is this a PNG?\n");

#endif

  if(raw_format)
  {
    boolean res = load_raw(&s, raw_format, magic);
    if(res)
      return true;
  }

  debug("unknown format\n");
  return false;
}

static size_t stdio_read_fn(void *dest, size_t num, void *handle)
{
  return fread(dest, 1, num, (FILE *)handle);
}

/**
 * Load an image from a file.
 */
boolean load_image_from_file(const char *filename, struct image_file *dest,
 const struct image_raw_format *raw_format)
{
  FILE *fp;
  boolean ret;

  if(!strcmp(filename, "-"))
  {
#ifdef _WIN32
    /* Windows forces stdin to be text mode by default, fix it. */
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    return load_image_from_stream(stdin, stdio_read_fn, dest, raw_format);
  }

  fp = fopen(filename, "rb");
  if(!fp)
    return false;

  setvbuf(fp, NULL, _IOFBF, 32768);

  ret = load_image_from_stream(fp, stdio_read_fn, dest, raw_format);
  fclose(fp);

  return ret;
}
