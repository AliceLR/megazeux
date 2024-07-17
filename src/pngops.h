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

#ifndef __PNGOPS_H
#define __PNGOPS_H

#include "compat.h"

__M_BEGIN_DECLS

#include <png.h>

#if (defined(CONFIG_SDL) && defined(CONFIG_ICON) && !defined(__WIN32__)) || \
    defined(CONFIG_UTILS) || defined(CONFIG_3DS)
#define NEED_PNG_READ_FILE
#endif

#ifdef NEED_PNG_WRITE_SCREEN

#include <stdint.h>

struct rgb_color;

/*
int png_write_image_8bpp(const char *name, size_t w, size_t h,
 const struct rgb_color *pal, unsigned count, void *priv,
 const uint8_t *(*row_pixels_callback)(size_t num_pixels, void *priv));
*/
int png_write_image_32bpp(const char *name, size_t w, size_t h, void *priv,
 const uint32_t *(*row_pixels_callback)(size_t num_pixels, void *priv));

#endif // NEED_PNG_WRITE_SCREEN

#ifdef NEED_PNG_READ_FILE

#include <stdio.h>

typedef boolean (*check_w_h_constraint_t)(png_uint_32 w, png_uint_32 h);
typedef void *(*rgba_surface_allocator_t)(png_uint_32 w, png_uint_32 h,
                                          png_uint_32 *stride, void **pixels);

void *png_read_stream(FILE *fp, png_uint_32 *_w, png_uint_32 *_h, boolean checked,
 check_w_h_constraint_t constraint, rgba_surface_allocator_t allocator);

void *png_read_file(const char *name, png_uint_32 *_w, png_uint_32 *_h,
 check_w_h_constraint_t constraint, rgba_surface_allocator_t allocator);

#endif // NEED_PNG_READ_FILE

__M_END_DECLS

#endif // __PNGOPS_H
