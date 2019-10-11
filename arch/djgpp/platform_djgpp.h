/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
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
  DISPLAY_ADAPTER_NONE,
  DISPLAY_ADAPTER_MDA,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_EGA_MONO,
  DISPLAY_ADAPTER_EGA_COLOR,
  DISPLAY_ADAPTER_VGA_MONO,
  DISPLAY_ADAPTER_VGA_COLOR,
  DISPLAY_ADAPTER_MCGA_MONO,
  DISPLAY_ADAPTER_MCGA_COLOR
};

extern const char *disp_names[];
int detect_graphics(void);

#endif // __PLATFORM_DJGPP_H
