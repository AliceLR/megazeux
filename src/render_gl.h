/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef __RENDER_GL_H
#define __RENDER_GL_H

#include "compat.h"

__M_BEGIN_DECLS

#include "graphics.h"

#ifndef CONFIG_EGL
#include "SDL_opengl.h"
#include <GL/gl.h>
#else
#include <EGL/egl.h>
#include <GLES/gl.h>
#endif

#define GL_NON_POWER_2_WIDTH      640
#define GL_NON_POWER_2_HEIGHT     350
#define GL_POWER_2_WIDTH          1024
#define GL_POWER_2_HEIGHT         512

#define CONFIG_GL_FILTER_LINEAR   "linear"
#define CONFIG_GL_FILTER_NEAREST  "nearest"

#ifndef GL_APIENTRY
#define GL_APIENTRY APIENTRY
#endif

extern const float vertex_array_single[2 * 4];

void gl_set_filter_method(const char *method,
 void (GL_APIENTRY *glTexParameterf_p)(GLenum target, GLenum pname, GLfloat param));
void get_context_width_height(struct graphics_data *graphics,
 int *width, int *height);

__M_END_DECLS

#endif // __RENDER_GL_H
