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
  DISPLAY_ADAPTER_VBE20,
  DISPLAY_ADAPTER_VBE30
};

struct vbe_info
{
  char signature[4];
  uint16_t version;
  uint32_t oem_name;
  uint32_t capabilities;
  uint32_t modes;
  uint16_t memory_size;
  uint16_t oem_version;
  uint32_t vendor_name;
  uint32_t product_name;
  uint32_t product_revision;
} __attribute__((packed));

struct vbe_mode_info
{
  uint16_t attr;
  uint8_t window_a_attr;
  uint8_t window_b_attr;
  uint16_t window_granularity;
  uint16_t window_size;
  uint16_t window_a_start;
  uint16_t window_b_start;
  uint32_t window_positioning_func;
  uint16_t pitch;
  uint16_t width;
  uint16_t height;
  uint8_t char_width;
  uint8_t char_height;
  uint8_t memory_planes;
  uint8_t bpp;
  uint8_t memory_banks;
  uint8_t memory_model_type;
  uint8_t bank_size;
  uint8_t image_pages;
  uint8_t reserved1;
  uint8_t red_mask_size;
  uint8_t red_field_size;
  uint8_t green_mask_size;
  uint8_t green_field_size;
  uint8_t blue_mask_size;
  uint8_t blue_field_size;
  uint16_t reserved2;
  uint8_t direct_color_mode_info;
  uint32_t linear_ptr;
  uint32_t offscreen_ptr;
  uint16_t offscreen_size;
} __attribute__((packed));

int djgpp_display_adapter_detect(void);
const char *djgpp_display_adapter_name(int adapter);
int djgpp_malloc_boundary(int len_bytes, int boundary_bytes, int *selector);
boolean djgpp_push_enable_nearptr(void);
boolean djgpp_pop_enable_nearptr(void);
void djgpp_enable_dma(uint8_t port, uint8_t mode, int offset, int bytes);
void djgpp_disable_dma(uint8_t port);

#endif // __PLATFORM_DJGPP_H
