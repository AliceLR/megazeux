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

#include "graphics.h"
#include "render.h"
#include "render_sdl.h"
#include "render_gl.h"

int gl_check_video_mode(graphics_data *graphics, int width, int height,
 int depth, int fullscreen, int resize)
{
  return SDL_VideoModeOK(width, height, depth,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize)));
}

void gl_set_filter_method(const char *method,
 void (APIENTRY *glTexParameteri_p)(GLenum target, GLenum pname, GLint param))
{
  GLint gl_filter_method = GL_LINEAR;

  if(!strcasecmp(method, CONFIG_GL_FILTER_NEAREST))
    gl_filter_method = GL_NEAREST;

  glTexParameteri_p(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_method);
  glTexParameteri_p(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_method);
}

void gl_set_attributes(graphics_data *graphics)
{
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if(graphics->gl_vsync == 0)
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
  else if(graphics->gl_vsync >= 1)
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
}
