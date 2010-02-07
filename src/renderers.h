/* MegaZeux
 *
 *   Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef __RENDERERS_H
#define __RENDERERS_H

#include "compat.h"

__M_BEGIN_DECLS

#include "graphics.h"

typedef struct
{
  char *name;
  void (*reg)(graphics_data *);
} renderer_data;

void render_soft_register(graphics_data *graphics);
#if defined(CONFIG_OPENGL) && !defined(PSP_BUILD)
void render_gl1_register(graphics_data *graphics);
void render_gl2_register(graphics_data *graphics);
#endif
#if !defined(PSP_BUILD)
void render_yuv1_register(graphics_data *graphics);
void render_yuv2_register(graphics_data *graphics);
#endif
void render_gp2x_register(graphics_data *graphics);

__M_END_DECLS

#endif // __RENDERERS_H
