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

#ifndef __RENDER_SDL_H
#define __RENDER_SDL_H

#include "compat.h"

__M_BEGIN_DECLS

#include "graphics.h"

#include <SDL.h>

struct sdl_render_data
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_Palette *palette;
  SDL_Window *window;
  SDL_GLContext context;
#else
  SDL_Overlay *overlay;
#endif
  SDL_Surface *screen;
  SDL_Surface *shadow;
};

#ifdef __MACOSX__
// Mac OS X has a special OpenGL YCbCr native texture mode which is
// faster than RGB for older machines.
#define YUV_PRIORITY 1000000
#else
#define YUV_PRIORITY 422
#endif

extern CORE_LIBSPEC Uint32 sdl_window_id;

int sdl_flags(int depth, boolean fullscreen, boolean fullscreen_windowed,
 boolean resize);
boolean sdl_get_fullscreen_resolution(int *width, int *height, boolean scaling);
Uint32 sdl_pixel_format_priority(Uint32 pixel_format,  Uint32 bits_per_pixel,
 boolean force_rgb);
void sdl_destruct_window(struct graphics_data *graphics);

boolean sdl_set_video_mode(struct graphics_data *graphics, int width,
 int height, int depth, boolean fullscreen, boolean resize);

#if !SDL_VERSION_ATLEAST(2,0,0)
// Used internally only.
boolean sdl_check_video_mode(struct graphics_data *graphics, int width,
 int height, int *depth, int flags);
#endif

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)

#include "render_gl.h"

#if SDL_VERSION_ATLEAST(2,0,0)
#define GL_ALLOW_FLAGS \
 (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP | \
  SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE)
#else
#define GL_ALLOW_FLAGS (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE)
#endif

#define GL_STRIP_FLAGS(A) ((A & GL_ALLOW_FLAGS) | SDL_WINDOW_OPENGL)

boolean gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, boolean fullscreen, boolean resize, struct gl_version req_ver);
void gl_set_attributes(struct graphics_data *graphics);
boolean gl_swap_buffers(struct graphics_data *graphics);

static inline void gl_cleanup(struct graphics_data *graphics)
{
  sdl_destruct_window(graphics);
}

static inline boolean GL_LoadLibrary(enum gl_lib_type type)
{
  if(!SDL_GL_LoadLibrary(NULL)) return true;
#if !SDL_VERSION_ATLEAST(2,0,0)
  // If the context already exists, don't reload the library
  // This is for SDL 1.2 which doesn't let us unload OpenGL
  if(strcmp(SDL_GetError(), "OpenGL context already created") == 0) return true;
#endif
  return false;
}

static inline void *GL_GetProcAddress(const char *proc)
{
  return SDL_GL_GetProcAddress(proc);
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM

__M_END_DECLS

#endif // __RENDER_SDL_H
