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

CORE_LIBSPEC Uint32 sdl_window_id;

#ifndef CONFIG_RENDER_YUV
static
#endif
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

bool sdl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  bool matched = false;
  Uint32 fmt;

  if(render_data->palette)
    SDL_FreePalette(render_data->palette);

  if(render_data->renderer)
    SDL_DestroyRenderer(render_data->renderer);

  if(render_data->window)
    SDL_DestroyWindow(render_data->window);

  render_data->window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   sdl_flags(depth, fullscreen, resize));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_out;
  }

  render_data->renderer =
   SDL_CreateRenderer(render_data->window, -1, SDL_RENDERER_SOFTWARE);

  if(!render_data->renderer)
  {
    warn("Failed to create renderer: %s\n", SDL_GetError());
    goto err_destroy_window;
  }

  render_data->screen = SDL_GetWindowSurface(render_data->window);
  if(!render_data->screen)
  {
    warn("Failed to get window surface: %s\n", SDL_GetError());
    goto err_destroy_renderer;
  }

  /* SDL 2.0 allows the window system to offer up buffers of a specific
   * format, and expects the application to perform a blit if the format
   * doesn't match what the app wanted. To accomodate this, allocate a
   * shadow screen that we render to in the case the formats do not match,
   * and blit it at present time.
   */

  fmt = SDL_GetWindowPixelFormat(render_data->window);
  switch(depth)
  {
    case 8:
      matched = fmt == SDL_PIXELFORMAT_INDEX8;
      fmt = SDL_PIXELFORMAT_INDEX8;
      break;
    case 16:
      switch(fmt)
      {
        case SDL_PIXELFORMAT_RGB565:
        case SDL_PIXELFORMAT_BGR565:
          matched = true;
          break;
        default:
          fmt = SDL_PIXELFORMAT_RGB565;
          break;
      }
      break;
    case 32:
      switch(fmt)
      {
        case SDL_PIXELFORMAT_RGB888:
        case SDL_PIXELFORMAT_RGBX8888:
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_BGRX8888:
        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_ABGR8888:
          matched = true;
          break;
        default:
          fmt = SDL_PIXELFORMAT_RGBA8888;
          break;
      }
      break;
  }

  if(!matched)
  {
    Uint32 Rmask, Gmask, Bmask, Amask;
    int bpp;

    SDL_PixelFormatEnumToMasks(fmt, &bpp, &Rmask, &Gmask, &Bmask, &Amask);

    render_data->shadow = SDL_CreateRGBSurface(0, width, height, bpp,
     Rmask, Gmask, Bmask, Amask);

    debug("Blitting enabled. Rendering performance will be reduced.\n");
  }
  else
  {
    render_data->shadow = NULL;
  }

  if(fmt == SDL_PIXELFORMAT_INDEX8)
  {
    SDL_PixelFormat *format =
      render_data->shadow ? render_data->shadow->format :
                            render_data->screen->format;

    render_data->palette = SDL_AllocPalette(SMZX_PAL_SIZE);
    if(!render_data->palette)
    {
      warn("Failed to allocate palette: %s\n", SDL_GetError());
      goto err_destroy_renderer;
    }

    if(SDL_SetPixelFormatPalette(format, render_data->palette))
    {
      warn("Failed to set pixel format palette: %s\n", SDL_GetError());
      goto err_free_palette;
    }
  }
  else
  {
    render_data->palette = NULL;
  }

  sdl_window_id = SDL_GetWindowID(render_data->window);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  render_data->screen = SDL_SetVideoMode(width, height, depth,
   sdl_flags(depth, fullscreen, resize));

  if(!render_data->screen)
    return false;

  render_data->shadow = NULL;

#endif // !SDL_VERSION_ATLEAST(2,0,0)

  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_free_palette:
  SDL_FreePalette(render_data->palette);
  render_data->palette = NULL;
err_destroy_renderer:
  SDL_DestroyRenderer(render_data->renderer);
  render_data->renderer = NULL;
err_destroy_window:
  SDL_DestroyWindow(render_data->window);
  render_data->window = NULL;
err_out:
  return false;
#endif // SDL_VERSION_ATLEAST(2,0,0)
}

bool sdl_check_video_mode(struct graphics_data *graphics, int width,
 int height, int depth, bool fullscreen, bool resize)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  return true;
#else
  return SDL_VideoModeOK(width, height, depth,
   sdl_flags(depth, fullscreen, resize));
#endif
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)

bool gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  if(render_data->context)
    SDL_GL_DeleteContext(render_data->context);

  if(render_data->renderer)
    SDL_DestroyRenderer(render_data->renderer);

  if(render_data->window)
    SDL_DestroyWindow(render_data->window);

  render_data->window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize)));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_out;
  }

  render_data->renderer =
   SDL_CreateRenderer(render_data->window, -1, SDL_RENDERER_ACCELERATED);

  if(!render_data->renderer)
  {
    warn("Failed to create renderer: %s\n", SDL_GetError());
    goto err_destroy_window;
  }

  render_data->context = SDL_GL_CreateContext(render_data->window);

  if(!render_data->context)
  {
    warn("Failed to create context: %s\n", SDL_GetError());
    goto err_destroy_renderer;
  }

  if(SDL_GL_MakeCurrent(render_data->window, render_data->context))
  {
    warn("Failed to make context current: %s\n", SDL_GetError());
    goto err_delete_context;
  }

  sdl_window_id = SDL_GetWindowID(render_data->window);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  if(!SDL_SetVideoMode(width, height, depth,
       GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize))))
    return false;

#endif // !SDL_VERSION_ATLEAST(2,0,0)

  render_data->screen = NULL;
  render_data->shadow = NULL;

  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_delete_context:
  SDL_GL_DeleteContext(render_data->context);
  render_data->context = NULL;
err_destroy_renderer:
  SDL_DestroyRenderer(render_data->renderer);
  render_data->renderer = NULL;
err_destroy_window:
  SDL_DestroyWindow(render_data->window);
  render_data->window = NULL;
err_out:
  return false;
#endif // SDL_VERSION_ATLEAST(2,0,0)
}

bool gl_check_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  return true;
#else
  return SDL_VideoModeOK(width, height, depth,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize)));
#endif
}

void gl_set_attributes(struct graphics_data *graphics)
{
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if(graphics->gl_vsync == 0)
    SDL_GL_SetSwapInterval(0);
  else if(graphics->gl_vsync >= 1)
    SDL_GL_SetSwapInterval(1);
}

bool gl_swap_buffers(struct graphics_data *graphics)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  struct sdl_render_data *render_data = graphics->render_data;
  SDL_GL_SwapWindow(render_data->window);
#else
  SDL_GL_SwapBuffers();
#endif
  return true;
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM

