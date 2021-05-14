/* MegaZeux
 *
 * Copyright (C) 2007-2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "compat_sdl.h"
#include "render_sdl.h"
#include "util.h"

#include <limits.h>

#include <SDL.h>

CORE_LIBSPEC Uint32 sdl_window_id;

int sdl_flags(int depth, boolean fullscreen, boolean fullscreen_windowed,
 boolean resize)
{
  int flags = 0;

  if(fullscreen)
  {
#if SDL_VERSION_ATLEAST(2,0,0)
    if(fullscreen_windowed)
    {
      // FIXME There's no built-in flag to achieve fake fullscreen. Disabling
      // the border, ignoring the fullscreen flags, and using native resolution
      // seems to be the best way to approximate it.
      flags |= SDL_WINDOW_BORDERLESS;
    }
    else
#endif
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

boolean sdl_get_fullscreen_resolution(int *width, int *height, boolean scaling)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_DisplayMode display_mode;
  int ret = -1;
  int count;

  if(scaling)
  {
    // Use the current desktop resolution.
    ret = SDL_GetDesktopDisplayMode(0, &display_mode);

    if(ret)
    {
      count = SDL_GetNumDisplayModes(0);
      if(count)
        ret = SDL_GetDisplayMode(0, 0, &display_mode);
    }
  }
  else
  {
    // Get the smallest possible resolution bigger than 640x350.
    // Smaller means the software renderer will occupy more screen space.
    SDL_DisplayMode mode;
    int min_size = INT_MAX;
    int i;

    count = SDL_GetNumDisplayModes(0);

    debug("Display modes:\n");

    for(i = 0; i < count; i++)
    {
      ret = SDL_GetDisplayMode(0, i, &mode);
      debug("%d: %d x %d, %dHz, %s\n", i, mode.w, mode.h, mode.refresh_rate,
       SDL_GetPixelFormatName(mode.format));

      if(!ret && (mode.w * mode.h < min_size) &&
       (mode.w >= SCREEN_PIX_W) && (mode.h >= SCREEN_PIX_H))
      {
        min_size = mode.w * mode.h;
        display_mode = mode;
      }
    }
  }

  if(!ret)
  {
    *width = display_mode.w;
    *height = display_mode.h;
    return true;
  }
  else
    warn("Failed to get display mode: %s\n", SDL_GetError());
#endif

  return false;
}

/**
 * Determine if MZX supports a particular SDL pixel format. Returns a priority
 * value if the format is supported or 0 if the format is not supported. Values
 * returned for separate formats can be compared; higher values correspond to
 * higher priority.
 *
 * A bits_per_pixel value can be provided to filter out pixel formats that do
 * not correspond to the desired BPP value.
 */
Uint32 sdl_pixel_format_priority(Uint32 pixel_format, Uint32 bits_per_pixel,
 boolean force_rgb)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  switch(pixel_format)
  {
    case SDL_PIXELFORMAT_INDEX8:
    {
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 8)
        return 256;

      break;
    }

#if SDL_VERSION_ATLEAST(2,0,12)
    case SDL_PIXELFORMAT_BGR444:
#endif
    case SDL_PIXELFORMAT_RGB444:
    case SDL_PIXELFORMAT_ARGB4444:
    case SDL_PIXELFORMAT_RGBA4444:
    case SDL_PIXELFORMAT_ABGR4444:
    case SDL_PIXELFORMAT_BGRA4444:
    {
      // Favor 16bpp modes with more colors if possible.
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16)
        return 444;

      break;
    }

    case SDL_PIXELFORMAT_RGB555:
    case SDL_PIXELFORMAT_BGR555:
    case SDL_PIXELFORMAT_ARGB1555:
    case SDL_PIXELFORMAT_RGBA5551:
    case SDL_PIXELFORMAT_ABGR1555:
    case SDL_PIXELFORMAT_BGRA5551:
    {
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16)
        return 555;

      break;
    }

    case SDL_PIXELFORMAT_RGB565:
    case SDL_PIXELFORMAT_BGR565:
    {
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16)
        return 565;

      break;
    }

    case SDL_PIXELFORMAT_RGB888:
    case SDL_PIXELFORMAT_BGR888:
    case SDL_PIXELFORMAT_RGBX8888:
    case SDL_PIXELFORMAT_BGRX8888:
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_RGBA8888:
    case SDL_PIXELFORMAT_ABGR8888:
    case SDL_PIXELFORMAT_BGRA8888:
    case SDL_PIXELFORMAT_ARGB2101010:
    {
      // Any 32-bit RGB format is okay.
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 32)
        return 888;

      break;
    }

    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
    {
      if(force_rgb)
        break;

      // These can work as either 32-bpp (full encode) or partial 16-bpp
      // (chroma subsampling for render_graph, full encode for layers).
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16 || bits_per_pixel == 32)
        return YUV_PRIORITY;

      break;
    }
  }
#endif /* SDL_VERSION_ATLEAST(2,0,0) */

  return 0;
}

#if !SDL_VERSION_ATLEAST(2,0,0)
/**
 * Precheck a provided video mode and select a BPP (if requested). This
 * works somewhat opposite to how SDL 2 works: instead of a pixel format
 * being picked by SDL (which needs to be checked for MZX support), MZX
 * instead needs to test its preferred pixel format by querying SDL.
 */
boolean sdl_check_video_mode(struct graphics_data *graphics, int width,
 int height, int *depth, int flags)
{
  static int system_bpp = 0;
  static boolean has_system_bpp = false;
  int in_depth = *depth;
  int out_depth;

  if(!has_system_bpp)
  {
    // Query the "best" video mode. This might actually be the current video
    // mode if another renderer has been initialized, so always do this first
    // (even if the provided depth isn't BPP_AUTO).
    const SDL_VideoInfo *info = SDL_GetVideoInfo();
    if(info && info->vfmt)
    {
      system_bpp = info->vfmt->BytesPerPixel * 8;
      has_system_bpp = true;
      debug("SDL_GetVideoInfo BPP=%d\n", system_bpp);
    }
  }

  if(in_depth == BPP_AUTO)
    in_depth = system_bpp;

  out_depth = SDL_VideoModeOK(width, height, in_depth, flags);
  if(out_depth <= 0)
  {
    debug("SDL_VideoModeOK failed.\n");
    return false;
  }

  if(*depth == BPP_AUTO)
  {
    if(out_depth == 8 || out_depth == 16 || out_depth == 32)
    {
      debug("SDL_VideoModeOK recommends BPP=%d\n", out_depth);
      graphics->bits_per_pixel = out_depth;
      *depth = out_depth;
    }
    else

    if(out_depth > 0)
    {
      debug("SDL_VideoModeOK recommends unsupported BPP=%d; using 32bpp\n", out_depth);
      graphics->bits_per_pixel = 32;
      *depth = 32;
    }
  }
  return true;
}
#endif /* !SDL_VERSION_ATLEAST(2,0,0) */

void sdl_destruct_window(struct graphics_data *graphics)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)

  // Used by the software renderer as a fallback if the window surface doesn't
  // match the pixel format MZX wants.
  if(render_data->shadow)
  {
    SDL_FreeSurface(render_data->shadow);
    render_data->shadow = NULL;
  }

  // Used for 8bpp support for the software renderer.
  if(render_data->palette)
  {
    SDL_FreePalette(render_data->palette);
    render_data->palette = NULL;
  }

  // Used by the softscale renderer for HW acceleration.
  if(render_data->texture)
  {
    SDL_DestroyTexture(render_data->texture);
    render_data->texture = NULL;
  }

  // Used by the softscale renderer for HW acceleration. Don't use for software.
  // Destroying this when exiting fullscreen can be slow for some reason.
  if(render_data->renderer)
  {
    SDL_DestroyRenderer(render_data->renderer);
    render_data->renderer = NULL;
  }

  // Used by the OpenGL renderers.
  if(render_data->context)
  {
    SDL_GL_DeleteContext(render_data->context);
    render_data->context = NULL;
  }

  // Used by all SDL renderers.
  if(render_data->window)
  {
    SDL_DestroyWindow(render_data->window);
    render_data->window = NULL;
  }

  // This is attached to the window for renderers that use it. Just clear it...
  render_data->screen = NULL;

#else /* !SDL_VERSION_ATLEAST(2,0,0) */

  // Used by the YUV renderers.
  if(render_data->overlay)
  {
    SDL_FreeYUVOverlay(render_data->overlay);
    render_data->overlay = NULL;
  }

  // Don't free render_data->screen. This pointer is managed by SDL.
  // The shadow surface isn't used by SDL 1.2 builds.
  render_data->screen = NULL;
  render_data->shadow = NULL;

#endif
}

boolean sdl_set_video_mode(struct graphics_data *graphics, int width,
 int height, int depth, boolean fullscreen, boolean resize)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  boolean fullscreen_windowed = graphics->fullscreen_windowed;
  boolean matched = false;
  Uint32 fmt;

  sdl_destruct_window(graphics);

#if defined(__EMSCRIPTEN__) && SDL_VERSION_ATLEAST(2,0,10)
  // Also, a hint needs to be set to make SDL_UpdateWindowSurface not crash.
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif

  render_data->window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   sdl_flags(depth, fullscreen, fullscreen_windowed, resize));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  render_data->screen = SDL_GetWindowSurface(render_data->window);
  if(!render_data->screen)
  {
    warn("Failed to get window surface: %s\n", SDL_GetError());
    goto err_free;
  }

  /* SDL 2.0 allows the window system to offer up buffers of a specific
   * format, and expects the application to perform a blit if the format
   * doesn't match what the app wanted. To accomodate this, allocate a
   * shadow screen that we render to in the case the formats do not match,
   * and blit it at present time.
   */

  fmt = SDL_GetWindowPixelFormat(render_data->window);
  debug("Window pixel format: %s\n", SDL_GetPixelFormatName(fmt));

  if(!sdl_pixel_format_priority(fmt, depth, true))
  {
    switch(depth)
    {
      case 8:
        fmt = SDL_PIXELFORMAT_INDEX8;
        break;
      case 16:
        fmt = SDL_PIXELFORMAT_RGB565;
        break;
      case 32:
      default:
        fmt = SDL_PIXELFORMAT_RGBA8888;
        break;
    }
  }
  else
    matched = true;

  debug("Using pixel format: %s\n", SDL_GetPixelFormatName(fmt));

  if(depth == BPP_AUTO)
    graphics->bits_per_pixel = SDL_BYTESPERPIXEL(fmt) * 8;

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
      goto err_free;
    }

    if(SDL_SetPixelFormatPalette(format, render_data->palette))
    {
      warn("Failed to set pixel format palette: %s\n", SDL_GetError());
      goto err_free;
    }
  }
  else
  {
    render_data->palette = NULL;
  }

  sdl_window_id = SDL_GetWindowID(render_data->window);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  if(!sdl_check_video_mode(graphics, width, height, &depth,
   sdl_flags(depth, fullscreen, false, resize)))
    return false;

  render_data->screen = SDL_SetVideoMode(width, height, depth,
   sdl_flags(depth, fullscreen, false, resize));

  if(!render_data->screen)
    return false;

  render_data->shadow = NULL;

#endif // !SDL_VERSION_ATLEAST(2,0,0)

  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_free:
  sdl_destruct_window(graphics);
  return false;
#endif // SDL_VERSION_ATLEAST(2,0,0)
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)

boolean gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, boolean fullscreen, boolean resize, struct gl_version req_ver)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  boolean fullscreen_windowed = graphics->fullscreen_windowed;

  sdl_destruct_window(graphics);

#ifdef CONFIG_GLES
  // Hints to make SDL use OpenGL ES drivers (e.g. ANGLE) on Windows/Linux.
  // These may be ignored by SDL unless using the Windows or X11 video drivers.
#if SDL_VERSION_ATLEAST(2,0,6)
  SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif
#if SDL_VERSION_ATLEAST(2,0,2)
  SDL_SetHint(SDL_HINT_VIDEO_WIN_D3DCOMPILER, "none");
#endif

  // Declare the OpenGL version to source functions from. This is necessary
  // since OpenGL ES 1.x and OpenGL ES 2.x are not compatible. This must be
  // done after old windows are destroyed and before new windows are created.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, req_ver.major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, req_ver.minor);
#endif

// Emscripten's EGL requires explicitly declaring alpha
// for RGBA textures to work.
#ifdef __EMSCRIPTEN__
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

#if SDL_VERSION_ATLEAST(2,0,10)
  // Also, a hint needs to be set to make gl_swap_buffers not crash. Brilliant.
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif
#endif

  render_data->window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, fullscreen_windowed, resize)));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  render_data->context = SDL_GL_CreateContext(render_data->window);

  if(!render_data->context)
  {
    warn("Failed to create context: %s\n", SDL_GetError());
    goto err_free;
  }

  if(SDL_GL_MakeCurrent(render_data->window, render_data->context))
  {
    warn("Failed to make context current: %s\n", SDL_GetError());
    goto err_free;
  }

  sdl_window_id = SDL_GetWindowID(render_data->window);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  if(!sdl_check_video_mode(graphics, width, height, &depth,
       GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, false, resize))))
    return false;

  if(!SDL_SetVideoMode(width, height, depth,
       GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, false, resize))))
    return false;

#endif // !SDL_VERSION_ATLEAST(2,0,0)

  render_data->screen = NULL;
  render_data->shadow = NULL;

  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_free:
  sdl_destruct_window(graphics);
  return false;
#endif // SDL_VERSION_ATLEAST(2,0,0)
}

void gl_set_attributes(struct graphics_data *graphics)
{
  // Note that this function is called twice- both before and after
  // gl_set_video_mode
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if(graphics->gl_vsync == 0)
    SDL_GL_SetSwapInterval(0);
  else if(graphics->gl_vsync >= 1)
    SDL_GL_SetSwapInterval(1);
}

boolean gl_swap_buffers(struct graphics_data *graphics)
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

