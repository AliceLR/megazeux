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

#include "render_gl.h"
#include "render.h"
#include "util.h"

#ifdef CONFIG_SDL
#include "render_sdl.h"
#endif

#ifdef CONFIG_EGL
#include "render_egl.h"
#endif

void gl_set_filter_method(const char *method,
 void (GLAPIENTRY *glTexParameteri_p)(GLenum target, GLenum pname, GLint param))
{
  GLint gl_filter_method = GL_LINEAR;

  if(!strcasecmp(method, CONFIG_GL_FILTER_NEAREST))
    gl_filter_method = GL_NEAREST;

  glTexParameteri_p(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_method);
  glTexParameteri_p(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_method);
}

void get_context_width_height(struct graphics_data *graphics,
 int *width, int *height)
{
  if(!graphics->fullscreen)
  {
    *width = graphics->window_width;
    *height = graphics->window_height;
  }
  else
  {
    *width = graphics->resolution_width;
    *height = graphics->resolution_height;
  }
}
