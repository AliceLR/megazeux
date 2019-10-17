/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
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

#ifndef __PLATFORM_DJGPP_H
#define __PLATFORM_DJGPP_H

enum
{
  DISPLAY_ADAPTER_UNSUPPORTED,
  DISPLAY_ADAPTER_EGA,
  DISPLAY_ADAPTER_VGA,
  DISPLAY_ADAPTER_SVGA,
  DISPLAY_ADAPTER_VBE20
};

struct vbe_info
{
  char signature[4];
  Uint16 version;
  Uint32 oem_name;
  Uint32 capabilities;
  Uint32 modes;
  Uint16 memory_size;
  Uint16 oem_version;
  Uint32 vendor_name;
  Uint32 product_name;
  Uint32 product_revision;
} __attribute__((packed));

struct vbe_mode_info
{
  Uint16 attr;
  Uint8 window_a_attr;
  Uint8 window_b_attr;
  Uint16 window_granularity;
  Uint16 window_size;
  Uint16 window_a_start;
  Uint16 window_b_start;
  Uint32 window_positioning_func;
  Uint16 pitch;
  Uint16 width;
  Uint16 height;
  Uint8 char_width;
  Uint8 char_height;
  Uint8 memory_planes;
  Uint8 bpp;
  Uint8 memory_banks;
  Uint8 memory_model_type;
  Uint8 bank_size;
  Uint8 image_pages;
  Uint8 reserved1;
  Uint8 red_mask_size;
  Uint8 red_field_size;
  Uint8 green_mask_size;
  Uint8 green_field_size;
  Uint8 blue_mask_size;
  Uint8 blue_field_size;
  Uint16 reserved2;
  Uint8 direct_color_mode_info;
  Uint32 linear_ptr;
  Uint32 offscreen_ptr;
  Uint16 offscreen_size;
} __attribute__((packed));

int djgpp_display_adapter_detect(void);
const char *djgpp_display_adapter_name(int adapter);
int djgpp_malloc_boundary(int len_bytes, int boundary_bytes, int *selector);
boolean djgpp_push_enable_nearptr(void);
boolean djgpp_pop_enable_nearptr(void);
void djgpp_enable_dma(Uint8 port, Uint8 mode, int offset, int bytes);
void djgpp_disable_dma(Uint8 port);

#endif // __PLATFORM_DJGPP_H
