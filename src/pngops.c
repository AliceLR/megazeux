/* MegaZeux
 *
 * Copyright (C) 2008 Alistair Strachan <alistair@devzero.co.uk>
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

#include "graphics.h"
#include "pngops.h"
#include "util.h"
#include "io/vio.h"

#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NEED_PNG_WRITE_SCREEN

static void png_write_vfile(png_struct *png_ptr, png_byte *data, png_size_t length)
{
  vfile *vf = (vfile *)png_get_io_ptr(png_ptr);
  if(vfwrite(data, 1, length, vf) < length)
    png_error(png_ptr, "write error");
}

static void png_flush_vfile(png_struct *png_ptr) {}

/*
int png_write_screen(uint8_t *pixels, struct rgb_color *pal, int count,
 const char *name)
{
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep * volatile row_ptrs = NULL;
  png_colorp volatile pal_ptr = NULL;
  int volatile ret = false;
  int type;
  int i;
  vfile *vf;

  vf = vfopen_unsafe(name, "wb");
  if(!vf)
    goto exit_out;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_close;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_jmpbuf(png_ptr)))
    goto exit_free_close;

  //png_init_io(png_ptr, f);
  png_set_write_fn(png_ptr, vf, png_write_vfile, png_flush_vfile);

  // we know we have an 8-bit surface; save a palettized PNG
  type = PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE;
  png_set_IHDR(png_ptr, info_ptr, 640, 350, 8, type,
   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
   PNG_FILTER_TYPE_DEFAULT);

  pal_ptr = (png_colorp)cmalloc(count * sizeof(png_color));
  if(!pal_ptr)
    goto exit_free_close;

  for(i = 0; i < count; i++)
  {
    pal_ptr[i].red = pal[i].r;
    pal_ptr[i].green = pal[i].g;
    pal_ptr[i].blue = pal[i].b;
  }
  png_set_PLTE(png_ptr, info_ptr, pal_ptr, count);

  // do the write of the header
  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);

  row_ptrs = (png_bytep *)cmalloc(sizeof(png_bytep) * 350);
  if(!row_ptrs)
    goto exit_free_close;

  // and then the surface
  for(i = 0; i < 350; i++)
    row_ptrs[i] = (png_bytep)(pixels + i * 640);
  png_write_image(png_ptr, row_ptrs);
  png_write_end(png_ptr, info_ptr);

  // all done
  ret = true;

exit_free_close:
  png_destroy_write_struct(&png_ptr, &info_ptr);
  free(pal_ptr);
  free(row_ptrs);
exit_close:
  vfclose(vf);
exit_out:
  return ret;
}
*/

int png_write_screen_32bpp(uint32_t *pixels, const char *name)
{
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep * volatile row_ptrs = NULL;
  int volatile ret = false;
  int type;
  int i;
  vfile *vf;

  vf = vfopen_unsafe(name, "wb");
  if(!vf)
    goto exit_out;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_close;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_jmpbuf(png_ptr)))
    goto exit_free_close;

  //png_init_io(png_ptr, f);
  png_set_write_fn(png_ptr, vf, png_write_vfile, png_flush_vfile);

  // 24-bit png
  type = PNG_COLOR_TYPE_RGB;
  png_set_IHDR(png_ptr, info_ptr, 640, 350, 8, type,
   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
   PNG_FILTER_TYPE_DEFAULT);

  // do the write of the header
  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);

  row_ptrs = (png_bytep *)cmalloc(sizeof(png_bytep) * 350);
  if(!row_ptrs)
    goto exit_free_close;

  // our surface is 32bpp ABGR, so set up filler and order
  png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
  png_set_bgr(png_ptr);

  // and then the surface
  for(i = 0; i < 350; i++)
    row_ptrs[i] = (png_bytep)(pixels + i * 640);
  png_write_image(png_ptr, row_ptrs);
  png_write_end(png_ptr, info_ptr);

  // all done
  ret = true;

exit_free_close:
  png_destroy_write_struct(&png_ptr, &info_ptr);
  free(row_ptrs);
exit_close:
  vfclose(vf);
exit_out:
  return ret;
}

#endif /* NEED_PNG_WRITE_SCREEN */

#ifdef NEED_PNG_READ_FILE

/* TODO can vio-ize these, but it involves updating image_file.c. */

static boolean png_check_stream(FILE *fp)
{
  png_byte header[8];

  if(fread(header, 1, 8, fp) < 8)
    return false;

  if(png_sig_cmp(header, 0, 8))
    return false;

  return true;
}

/**
 * If `checked` is true, the first 8 bytes of the stream have already been
 * read and verified to be the PNG signature.
 */
void *png_read_stream(FILE *fp, png_uint_32 *_w, png_uint_32 *_h, boolean checked,
 check_w_h_constraint_t constraint, rgba_surface_allocator_t allocator)
{
  png_uint_32 i, w, h, stride;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep * volatile row_ptrs = NULL;
  void * volatile s = NULL;
  void *pixels;
  int type;
  int bpp;

  if(!checked && !png_check_stream(fp))
    goto exit_out;

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_out;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_jmpbuf(png_ptr)))
    goto exit_free_close;

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bpp, &type, NULL, NULL, NULL);

  if(!constraint(w, h))
  {
    warn("Requested image failed dimension checks.\n");
    goto exit_free_close;
  }

  if(type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);

  else if(type == PNG_COLOR_TYPE_GRAY_ALPHA || !(type & PNG_COLOR_MASK_COLOR))
    png_set_gray_to_rgb(png_ptr);

  else if(!(type & PNG_COLOR_MASK_COLOR) && bpp < 8)
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if(bpp == 16)
    png_set_strip_16(png_ptr);

  else if(bpp < 8)
    png_set_packing(png_ptr);

  if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png_ptr);
  else if(!(type & PNG_COLOR_MASK_ALPHA))
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);

  // FIXME: Are these necessary?
  png_read_update_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bpp, &type, NULL, NULL, NULL);

  row_ptrs = (png_bytep *)cmalloc(sizeof(png_bytep) * h);
  if(!row_ptrs)
    goto exit_free_close;

  s = allocator(w, h, &stride, &pixels);
  if(!s)
    goto exit_free_close;

  for(i = 0; i < h; i++)
    row_ptrs[i] = (png_bytep)(unsigned char *)pixels + i * stride;

  png_read_image(png_ptr, row_ptrs);

  if(_w)
    *_w = w;
  if(_h)
    *_h = h;

exit_free_close:
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  free(row_ptrs);
exit_out:
  return s;
}

void *png_read_file(const char *name, png_uint_32 *_w, png_uint_32 *_h,
 check_w_h_constraint_t constraint, rgba_surface_allocator_t allocator)
{
  void *s;

  FILE *fp = fopen_unsafe(name, "rb");
  if(!fp)
    return NULL;

  s = png_read_stream(fp, _w, _h, false, constraint, allocator);
  fclose(fp);

  if(!s)
    warn("Failed to load '%s'\n", name);

  return s;
}

#endif // NEED_PNG_READ_FILE
