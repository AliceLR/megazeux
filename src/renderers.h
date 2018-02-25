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

struct renderer_data
{
  const char *name;
  void (*reg)(struct renderer *renderer);
};

#if defined(CONFIG_RENDER_SOFT)
void render_soft_register(struct renderer *renderer);
#endif
#if defined(CONFIG_RENDER_GL_FIXED)
void render_gl1_register(struct renderer *renderer);
void render_gl2_register(struct renderer *renderer);
#endif
#if defined(CONFIG_RENDER_GL_PROGRAM)
void render_glsl_register(struct renderer *renderer);
void render_auto_glsl_register(struct renderer *renderer);
#endif
#if defined(CONFIG_RENDER_YUV)
void render_yuv1_register(struct renderer *renderer);
void render_yuv2_register(struct renderer *renderer);
#endif
#if defined(CONFIG_RENDER_GP2X)
void render_gp2x_register(struct renderer *renderer);
#endif
#if defined(CONFIG_NDS)
void render_nds_register(struct renderer *renderer);
#endif
#if defined(CONFIG_3DS)
void render_ctr_register(struct renderer *renderer);
#endif
#if defined(CONFIG_RENDER_GX)
void render_gx_register(struct renderer *renderer);
#endif

__M_END_DECLS

#endif // __RENDERERS_H
