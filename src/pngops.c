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

#include "pngops.h"
#include "util.h"

#include <stdlib.h>
#include <png.h>

int png_write_screen(Uint8 *pixels, struct rgb_color *pal, int count,
 const char *name)
{
  png_structp png_ptr = NULL;
  png_bytep *row_ptrs = NULL;
  png_colorp pal_ptr = NULL;
  png_infop info_ptr = NULL;
  int i, type, ret = false;
  FILE *f;

  f = fopen(name, "wb");
  if(!f)
    goto exit_out;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_close;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_jmpbuf(png_ptr)))
    goto exit_free_close;

  png_init_io(png_ptr, f);

  // we know we have an 8-bit surface; save a palettized PNG
  type = PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE;
  png_set_IHDR(png_ptr, info_ptr, 640, 350, 8, type,
   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
   PNG_FILTER_TYPE_DEFAULT);

  pal_ptr = cmalloc(count * sizeof(png_color));
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

  row_ptrs = cmalloc(sizeof(png_bytep) * 350);
  if(!row_ptrs)
    goto exit_free_close;

  // and then the surface
  for(i = 0; i < 350; i++)
    row_ptrs[i] = (png_bytep)(Uint8 *)pixels + i * 640;
  png_write_image(png_ptr, row_ptrs);
  png_write_end(png_ptr, info_ptr);

  // all done
  ret = true;

exit_free_close:
  png_destroy_write_struct(&png_ptr, &info_ptr);
  free(pal_ptr);
  free(row_ptrs);
exit_close:
  fclose(f);
exit_out:
  return ret;
}

#if defined(CONFIG_SDL) && defined(CONFIG_ICON) && !defined(__WIN32__)

SDL_Surface *png_read_icon(const char *name)
{
  Uint32 rmask, gmask, bmask, amask;
  const char magic[] = "\x89PNG";
  png_structp png_ptr = NULL;
  png_bytep *row_ptrs = NULL;
  png_infop info_ptr = NULL;
  SDL_Surface *s = NULL;
  png_uint_32 i, w, h;
  int type, bpp;
  FILE *f;

  f = fopen(name, "rb");
  if(!f)
    goto exit_out;

  // try to abort early if the file can't be a valid PNG.
  for(i = 0; i < 4; i++)
  {
    int byte = fgetc(f);
    if(byte != magic[i])
      goto exit_close;
  }
  rewind(f);

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    goto exit_close;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    goto exit_free_close;

  if(setjmp(png_ptr->jmpbuf))
    goto err_free_close;

  png_init_io(png_ptr, f);

  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bpp, &type, NULL, NULL, NULL);

  if(w != h || (w % 16) != 0 || (h % 16) != 0)
  {
    warn("Requested icon '%s' not a valid dimension.\n", name);
    goto exit_free_close;
  }

  switch(type)
  {
    case PNG_COLOR_TYPE_RGB_ALPHA:
      break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
      png_set_gray_to_rgb(png_ptr);
      break;
    default:
      warn("Requested icon '%s' not an RGBA image.\n", name);
      goto exit_free_close;
  }

  png_set_strip_16(png_ptr);

  /* After this update the icon will be of a valid size (square, modulo 16)
   * and RGB with alpha channel. It will also be 8bit per channel.
   */
  png_read_update_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bpp, &type, NULL, NULL, NULL);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  rmask = 0xff000000;
  gmask = 0x00ff0000;
  bmask = 0x0000ff00;
  amask = 0x000000ff;
#else
  rmask = 0x000000ff;
  gmask = 0x0000ff00;
  bmask = 0x00ff0000;
  amask = 0xff000000;
#endif

  s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, rmask, gmask, bmask, amask);
  if(!s)
    goto exit_free_close;

  row_ptrs = cmalloc(sizeof(png_bytep) * h);
  if(!row_ptrs)
    goto exit_free_close;

  for(i = 0; i < h; i++)
    row_ptrs[i] = (png_bytep)(Uint8 *)s->pixels + i * s->pitch;

  png_read_image(png_ptr, row_ptrs);

exit_free_close:
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  free(row_ptrs);
exit_close:
  fclose(f);
exit_out:
  return s;

err_free_close:
  if(s)
    SDL_FreeSurface(s);
  s = NULL;
  goto exit_free_close;
}

#endif // CONFIG_SDL && CONFIG_ICON && !__WIN32__
