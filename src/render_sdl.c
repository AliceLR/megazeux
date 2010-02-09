/* MegaZeux
 *
 * Copyright (C) 2007-2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "render_sdl.h"

#include "SDL.h"

int sdl_flags(int depth, bool fullscreen, bool resize)
{
  int flags = 0;

  if(fullscreen)
  {
    flags |= SDL_FULLSCREEN;
    if(depth == 8)
      flags |= SDL_HWPALETTE;
  }
  else
    if(resize)
      flags |= SDL_RESIZABLE;

  return flags;
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)

bool gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  return SDL_SetVideoMode(width, height, depth,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize))) != NULL;
}

bool gl_check_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  return SDL_VideoModeOK(width, height, depth,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize)));
}

void gl_set_attributes(struct graphics_data *graphics)
{
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if(graphics->gl_vsync == 0)
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
  else if(graphics->gl_vsync >= 1)
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
}

bool gl_swap_buffers(struct graphics_data *graphics)
{
  SDL_GL_SwapBuffers();
  return true;
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM

