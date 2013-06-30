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
    flags |= SDL_WINDOW_FULLSCREEN;
#if !SDL_VERSION_ATLEAST(2,0,0)
    if(depth == 8)
      flags |= SDL_HWPALETTE;
#endif
  }
  else
    if(resize)
      flags |= SDL_WINDOW_RESIZABLE;

  return flags;
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)

bool gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  graphics->render_data = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize)));

  return graphics->render_data != NULL;
}

bool gl_check_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  return true;
}

void gl_set_attributes(struct graphics_data *graphics)
{
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

bool gl_swap_buffers(struct graphics_data *graphics)
{
  SDL_GL_SwapWindow(graphics->render_data);
  return true;
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM

