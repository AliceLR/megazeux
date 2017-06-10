/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#ifndef __RENDER_LAYER_H
#define __RENDER_LAYER_H

#include "compat.h"

__M_BEGIN_DECLS

#include "graphics.h"

void render_layer(void *pixels, int force_bpp, Uint32 pitch, struct graphics_data
 *graphics, struct video_layer *layer);

void render_layer_32bpp(void *pixels, Uint32 pitch, struct graphics_data *graphics, struct video_layer *layer);
void render_layer_16bpp(void *pixels, Uint32 pitch, struct graphics_data *graphics, struct video_layer *layer);
void render_layer_8bpp(void *pixels, Uint32 pitch, struct graphics_data *graphics, struct video_layer *layer);

__M_END_DECLS

#endif // __RENDER_LAYER_H
