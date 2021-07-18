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

#ifndef __UTILS_IMAGE_FILE_H
#define __UTILS_IMAGE_FILE_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <stdint.h>
#include <stdio.h>

struct rgba_color
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct image_file
{
  uint32_t width;
  uint32_t height;
  struct rgba_color *data;
};

/**
 * If raw images are allowed, let the caller specify how the raw data should be
 * interpreted.
 */
struct image_raw_format
{
  uint32_t width;
  uint32_t height;
  uint32_t  bytes_per_pixel;
};

boolean load_image_from_file(const char *filename, struct image_file *dest,
 const struct image_raw_format *raw_format);
boolean load_image_from_stream(FILE *fp, struct image_file *dest,
 const struct image_raw_format *raw_format);
void image_free(struct image_file *dest);

__M_END_DECLS

#endif /* __UTILS_IMAGE_FILE_H */
