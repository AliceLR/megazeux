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

#include "graphics.h"

int png_write_screen(Uint8 *pixels, rgb_color *pal, int count,
 const char *name);

#if defined(CONFIG_SDL) && defined(CONFIG_ICON) && !defined(__WIN32__)
#include "SDL.h"
SDL_Surface *png_read_icon(const char *name);
#endif

__M_END_DECLS

#endif // __PNGOPS_H
