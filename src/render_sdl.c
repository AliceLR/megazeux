/* MegaZeux
 *
 * Copyright (C) 2007-2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2019-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "SDLmzx.h"
#include "render.h"
#include "render_sdl.h"
#include "util.h"
#include "yuv.h"

#include <limits.h>
#include <stdlib.h>

CORE_LIBSPEC Uint32 sdl_window_id;

#if SDL_VERSION_ATLEAST(2,0,0)
static void sdl_set_screensaver_enabled(boolean enable)
{
  debug("SDL %s screensaver.\n", enable ? "enabling" : "disabling");
  if(enable)
    SDL_EnableScreenSaver();
  else
    SDL_DisableScreenSaver();
}
#endif

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

#if SDL_VERSION_ATLEAST(2,0,0)
// Get the current desktop resolution, if possible.
static boolean sdl_get_desktop_display_mode(SDL_DisplayMode *display_mode)
{
#if SDL_VERSION_ATLEAST(3,0,0)

  const SDL_DisplayMode **list;
  const SDL_DisplayMode *mode;
  int count;

  mode = SDL_GetDesktopDisplayMode(0);
  if(mode)
  {
    *display_mode = *mode;
    return true;
  }

  warn("Failed to query desktop display mode: %s\n", SDL_GetError());
  list = (const SDL_DisplayMode **)SDL_GetFullscreenDisplayModes(0, &count);
  if(list)
  {
    if(count)
      mode = list[0];

    SDL_free(list);
    if(mode)
    {
      *display_mode = *mode;
      return true;
    }
  }

#else

  if(SDL_GetDesktopDisplayMode(0, display_mode) == 0)
    return true;

  warn("Failed to query desktop display mode: %s\n", SDL_GetError());

  if(SDL_GetNumDisplayModes(0))
    if(SDL_GetDisplayMode(0, 0, display_mode) == 0)
      return true;

#endif

  return false;
}

// Get the smallest possible resolution bigger than 640x350.
// Smaller means the software renderer will occupy more screen space.
static boolean sdl_get_smallest_usable_display_mode(SDL_DisplayMode *display_mode)
{
#if SDL_VERSION_ATLEAST(3,0,0)

  int min_size = INT_MAX;
  int count;
  int i;

  const SDL_DisplayMode **list =
   (const SDL_DisplayMode **)SDL_GetFullscreenDisplayModes(0, &count);
  if(!list)
    return false;

  debug("Display modes:\n");

  for(i = 0; i < count; i++)
  {
    if(!list[i])
      continue;

    debug("%d: %d x %d, %.2fHz, %s\n", i, list[i]->w, list[i]->h,
     list[i]->refresh_rate, SDL_GetPixelFormatName(list[i]->format));

    if((list[i]->w * list[i]->h < min_size) &&
     (list[i]->w >= SCREEN_PIX_W) && (list[i]->h >= SCREEN_PIX_H))
    {
      min_size = list[i]->w * list[i]->h;
      *display_mode = *list[i];
    }
  }
  SDL_free(list);

  if(min_size < INT_MAX)
    return true;

#else

  SDL_DisplayMode mode;
  int min_size = INT_MAX;
  int count;
  int i;

  count = SDL_GetNumDisplayModes(0);

  debug("Display modes:\n");

  for(i = 0; i < count; i++)
  {
    if(SDL_GetDisplayMode(0, i, &mode) != 0)
      continue;

    debug("%d: %d x %d, %dHz, %s\n", i, mode.w, mode.h, mode.refresh_rate, \
     SDL_GetPixelFormatName(mode.format));

    if((mode.w * mode.h < min_size) &&
     (mode.w >= SCREEN_PIX_W) && (mode.h >= SCREEN_PIX_H))
    {
      min_size = mode.w * mode.h;
      *display_mode = mode;
    }
  }
  if(min_size < INT_MAX)
    return true;

#endif

  return false;
}
#endif /* SDL_VERSION_ATLEAST(2,0,0) */

boolean sdl_get_fullscreen_resolution(int *width, int *height, boolean scaling)
{
#if SDL_VERSION_ATLEAST(2,0,0)

  SDL_DisplayMode display_mode;
  boolean have_mode = false;

  if(scaling)
  {
    // Use the current desktop resolution.
    have_mode = sdl_get_desktop_display_mode(&display_mode);
  }
  else
  {
    have_mode = sdl_get_smallest_usable_display_mode(&display_mode);
  }

  if(have_mode)
  {
    debug("\n");
    debug("selected: %d x %d, %.02fHz, %s\n", display_mode.w, display_mode.h,
     (float)display_mode.refresh_rate, SDL_GetPixelFormatName(display_mode.format));
    *width = display_mode.w;
    *height = display_mode.h;
    return true;
  }
  else
    warn("Failed to get display mode: %s\n", SDL_GetError());
#endif

  return false;
}

#if SDL_VERSION_ATLEAST(2,0,0)
/**
 * Determine if MZX supports a particular SDL pixel format. Returns a priority
 * value if the format is supported or 0 if the format is not supported. Values
 * returned for separate formats can be compared; higher values correspond to
 * higher priority.
 *
 * A bits_per_pixel value can be provided to filter out pixel formats that do
 * not correspond to the desired BPP value.
 */
static uint32_t sdl_pixel_format_priority(uint32_t pixel_format,
 uint32_t bits_per_pixel, uint32_t yuv_priority)
{
  switch(pixel_format)
  {
    case SDL_PIXELFORMAT_INDEX8:
    {
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 8)
        return 256;

      break;
    }

#if SDL_VERSION_ATLEAST(2,0,12)
    case SDL_PIXELFORMAT_XBGR4444:
#endif
    case SDL_PIXELFORMAT_XRGB4444:
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

    case SDL_PIXELFORMAT_XRGB1555:
    case SDL_PIXELFORMAT_XBGR1555:
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

    case SDL_PIXELFORMAT_XRGB8888:
    case SDL_PIXELFORMAT_XBGR8888:
    case SDL_PIXELFORMAT_RGBX8888:
    case SDL_PIXELFORMAT_BGRX8888:
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_RGBA8888:
    case SDL_PIXELFORMAT_ABGR8888:
    case SDL_PIXELFORMAT_BGRA8888:
    case SDL_PIXELFORMAT_ARGB2101010:
#if SDL_VERSION_ATLEAST(3,0,0)
    case SDL_PIXELFORMAT_XRGB2101010:
    case SDL_PIXELFORMAT_XBGR2101010:
    case SDL_PIXELFORMAT_ABGR2101010:
#endif
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
      if(!yuv_priority)
        break;

      // These can work as either 32-bpp (full encode) or partial 16-bpp
      // (chroma subsampling for render_graph, full encode for layers).
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16 || bits_per_pixel == 32)
        return yuv_priority;

      break;
    }
  }
  return 0;
}
#endif /* SDL_VERSION_ATLEAST(2,0,0) */

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
  size_t i;

  // Used by the software renderer as a fallback if the window surface doesn't
  // match the pixel format MZX wants.
  if(render_data->shadow)
  {
    SDL_DestroySurface(render_data->shadow);
    render_data->shadow = NULL;
  }

  // Used for 8bpp support for the software renderer.
  if(render_data->palette)
  {
    SDL_DestroyPalette(render_data->palette);
    render_data->palette = NULL;
  }

#if !SDL_VERSION_ATLEAST(3,0,0)
  // Used for generating mapped colors for the SDL_Renderer renderers.
  if(render_data->pixel_format)
  {
    SDL_FreeFormat(render_data->pixel_format);
    render_data->pixel_format = NULL;
  }
#endif

  // Used by the SDL renderer-based renderers for HW acceleration.
  for(i = 0; i < ARRAY_SIZE(render_data->texture); i++)
  {
    if(render_data->texture[i])
    {
      SDL_DestroyTexture(render_data->texture[i]);
      render_data->texture[i] = NULL;
    }
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
    SDL_GL_DestroyContext(render_data->context);
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

  // Used to send the palette to SDL in indexed mode.
  if(render_data->palette_colors)
  {
    free(render_data->palette_colors);
    render_data->palette_colors = NULL;
  }
  render_data->flat_format = NULL;
}

void sdl_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct sdl_render_data *render_data = (struct sdl_render_data *)graphics->render_data;
  unsigned int i;

  if(graphics->bits_per_pixel == 8)
  {
    if(!render_data->palette_colors)
      return;
    if(count > 256)
      count = 256;

    for(i = 0; i < count; i++)
    {
      render_data->palette_colors[i].r = palette[i].r;
      render_data->palette_colors[i].g = palette[i].g;
      render_data->palette_colors[i].b = palette[i].b;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_SetPaletteColors(render_data->palette,
     render_data->palette_colors, 0, count);
#else
    SDL_SetColors(render_data->screen, render_data->palette_colors, 0, count);
#endif
  }
  else

  if(!render_data->rgb_to_yuv)
  {
    if(!render_data->flat_format)
      return;
    for(i = 0; i < count; i++)
    {
#if SDL_VERSION_ATLEAST(3,0,0)
      graphics->flat_intensity_palette[i] =
       SDL_MapRGBA(render_data->flat_format, NULL,
        palette[i].r, palette[i].g, palette[i].b, SDL_ALPHA_OPAQUE);
#else
      graphics->flat_intensity_palette[i] =
       SDL_MapRGBA(render_data->flat_format,
        palette[i].r, palette[i].g, palette[i].b, SDL_ALPHA_OPAQUE);
#endif
    }
  }
  else
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
       render_data->rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b);
    }
  }
}


/* SDL software renderer functions for: *************************************/
// software
// gp2x

boolean sdl_set_video_mode(struct graphics_data *graphics, int width,
 int height, int depth, boolean fullscreen, boolean resize)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_PixelFormatDetails *format;
  boolean fullscreen_windowed = graphics->fullscreen_windowed;
  boolean matched = false;
  Uint32 fmt;

  sdl_destruct_window(graphics);

#if defined(__EMSCRIPTEN__) && SDL_VERSION_ATLEAST(2,0,10)
  // Also, a hint needs to be set to make SDL_UpdateWindowSurface not crash.
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif

  render_data->window = SDL_CreateWindow("MegaZeux",
  // FIXME:
#if !SDL_VERSION_ATLEAST(3,0,0)
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
#endif
   width, height, sdl_flags(depth, fullscreen, fullscreen_windowed, resize));

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

  if(!sdl_pixel_format_priority(fmt, depth, YUV_DISABLE))
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
    render_data->shadow = SDL_CreateSurface(width, height, fmt);
    debug("Blitting enabled. Rendering performance will be reduced.\n");
  }
  else
  {
    render_data->shadow = NULL;
  }

  format = render_data->shadow ? render_data->shadow->format :
                                 render_data->screen->format;
  render_data->flat_format = format;

  if(fmt == SDL_PIXELFORMAT_INDEX8)
  {
    render_data->palette = SDL_CreatePalette(SMZX_PAL_SIZE);
    if(!render_data->palette)
    {
      warn("Failed to allocate palette: %s\n", SDL_GetError());
      goto err_free;
    }
    render_data->palette_colors =
     (SDL_Color *)ccalloc(SMZX_PAL_SIZE, sizeof(SDL_Color));
    if(!render_data->palette_colors)
    {
      warn("Failed to allocate palette colors\n");
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
  sdl_set_screensaver_enabled(graphics->disable_screensaver == SCREENSAVER_ENABLE);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  if(!sdl_check_video_mode(graphics, width, height, &depth,
   sdl_flags(depth, fullscreen, false, resize)))
    return false;

  render_data->screen = SDL_SetVideoMode(width, height, depth,
   sdl_flags(depth, fullscreen, false, resize));

  if(!render_data->screen)
    return false;

  render_data->shadow = NULL;
  render_data->flat_format = render_data->screen->format;

  if(depth == 8)
  {
    render_data->palette_colors =
     (SDL_Color *)ccalloc(SMZX_PAL_SIZE, sizeof(SDL_Color));
    if(!render_data->palette_colors)
      return false;
  }

#endif // !SDL_VERSION_ATLEAST(2,0,0)

  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_free:
  sdl_destruct_window(graphics);
  return false;
#endif // SDL_VERSION_ATLEAST(2,0,0)
}


/* SDL hardware renderer API functions for: *********************************/
// softscale
// sdlaccel

#if defined(CONFIG_RENDER_SOFTSCALE) || defined(CONFIG_RENDER_SDLACCEL)

#if SDL_VERSION_ATLEAST(3,0,0)
#define BEST_RENDERER NULL
#else
#define BEST_RENDERER -1
#endif

static boolean is_integer_scale(struct graphics_data *graphics, int width,
 int height)
{
  int v_width, v_height;
  fix_viewport_ratio(width, height, &v_width, &v_height, graphics->ratio);

  if((v_width / SCREEN_PIX_W * SCREEN_PIX_W == v_width) &&
   (v_height / SCREEN_PIX_H * SCREEN_PIX_H == v_height))
    return true;

  return false;
}

static uint32_t get_format_amask(uint32_t format)
{
  Uint32 rmask, gmask, bmask, amask;
  int bpp;

  SDL_GetMasksForPixelFormat(format, &bpp, &rmask, &gmask, &bmask, &amask);
  return amask;
}

static void find_texture_format(struct graphics_data *graphics,
 boolean requires_blend_ops)
{
  struct sdl_render_data *render_data = (struct sdl_render_data *)graphics->render_data;
  uint32_t texture_format = SDL_PIXELFORMAT_UNKNOWN;
  unsigned int texture_bpp = 0;
  uint32_t texture_amask = 0;
  boolean allow_subsampling = false;
  boolean need_alpha = false;
  uint32_t yuv_priority = YUV_PRIORITY;
  uint32_t priority = 0;
  boolean is_software_renderer = false;
  const char *renderer_name;
  const uint32_t *formats = NULL;
  int num_formats;

#if SDL_VERSION_ATLEAST(3,0,0)

  SDL_PropertiesID props = SDL_GetRendererProperties(render_data->renderer);
  const uint32_t *pos;

  renderer_name = SDL_GetRendererName(render_data->renderer);
  if(!strcmp(renderer_name, SDL_SOFTWARE_RENDERER))
    is_software_renderer = true;

  // thanks for the michael mouse API
  formats = (const uint32_t *)SDL_GetPointerProperty(props,
   SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
  num_formats = 0;
  for(pos = formats; pos && *pos != SDL_PIXELFORMAT_UNKNOWN; pos++)
    num_formats++;

#else

  SDL_RendererInfo rinfo;

  if(!SDL_GetRendererInfo(render_data->renderer, &rinfo))
  {
    renderer_name = rinfo.name;
    num_formats = rinfo.num_texture_formats;
    formats = rinfo.texture_formats;

    if(rinfo.flags & SDL_RENDERER_SOFTWARE)
      is_software_renderer = true;
  }
  else
    warn("Failed to get renderer info!\n");

#endif /* !SDL_VERSION_ATLEAST(3,0,0) */

  if(formats)
  {
    unsigned int depth = graphics->bits_per_pixel;
    int i;

    info("SDL render driver: '%s'\n", renderer_name);
    if(is_software_renderer)
      warn("Accelerated renderer not available. Rendering will be SLOW!\n");

#ifdef __MACOSX__
    // Not clear if Metal supports the custom Apple YUV texture format.
    if(!strcasecmp(renderer_name, "opengl"))
      yuv_priority = YUV_PRIORITY_APPLE;
#endif

    // Anything using hardware blending must support alpha.
    if(requires_blend_ops)
      need_alpha = true;

    // Try to use a native texture format to improve performance.
    for(i = 0; i < num_formats; i++)
    {
      uint32_t format = formats[i];
      unsigned int format_priority;

      debug("%d: %s\n", i, SDL_GetPixelFormatName(format));

      if(SDL_ISPIXELFORMAT_INDEXED(format))
        continue;
      if(need_alpha && !SDL_ISPIXELFORMAT_ALPHA(format))
        continue;

      format_priority = sdl_pixel_format_priority(format, depth, yuv_priority);
      if(format_priority > priority)
      {
        texture_format = format;
        priority = format_priority;
      }
    }
  }

  if(texture_format == SDL_PIXELFORMAT_UNKNOWN)
  {
    // 16bpp RGB seems moderately faster than YUV with chroma subsampling
    // when neither are natively supported.
    if(graphics->bits_per_pixel == 16)
      texture_format = SDL_PIXELFORMAT_RGB565;
    else
      texture_format = SDL_PIXELFORMAT_ARGB8888;

    debug("No matching pixel format. Using %s. Rendering may be slower.\n",
     SDL_GetPixelFormatName(texture_format));
  }
  else
    debug("Using pixel format %s.\n", SDL_GetPixelFormatName(texture_format));

  if(priority == yuv_priority)
  {
    texture_bpp = 32;

    if(texture_format == SDL_PIXELFORMAT_YUY2)
    {
      render_data->subsample_set_colors = yuy2_subsample_set_colors_mzx;
      render_data->rgb_to_yuv = rgb_to_yuy2;
    }

    if(texture_format == SDL_PIXELFORMAT_UYVY)
    {
      render_data->subsample_set_colors = uyvy_subsample_set_colors_mzx;
#ifdef __MACOSX__
      render_data->rgb_to_yuv = rgb_to_apple_ycbcr_422;
#else
      render_data->rgb_to_yuv = rgb_to_uyvy;
#endif
    }

    if(texture_format == SDL_PIXELFORMAT_YVYU)
    {
      render_data->subsample_set_colors = yvyu_subsample_set_colors_mzx;
      render_data->rgb_to_yuv = rgb_to_yvyu;
    }

    // If this renderer was activated with force_bpp=16, enable subsampling
    // support for render_graph.
    if(graphics->bits_per_pixel == 16)
    {
      debug("Allowing YUV subsampling for render_graph.\n");
      allow_subsampling = true;
    }
  }
  else
  {
    texture_bpp = SDL_BYTESPERPIXEL(texture_format) * 8;
    texture_amask = get_format_amask(texture_format);
  }

  if(graphics->bits_per_pixel == BPP_AUTO)
    graphics->bits_per_pixel = texture_bpp;

  // Initialize the texture format data.
  render_data->texture_format = texture_format;
  render_data->texture_amask = texture_amask;
  render_data->texture_bpp = texture_bpp;
  render_data->allow_subsampling = allow_subsampling;
}

/**
 * Initialize a window for SDL Renderer-based renderers. This function will
 * automatically select a native texture format for the renderer (if applicable)
 * for better performance. This texture format is stored to `sdl_render_data`.
 * In some cases (but usually not), this may include YUV texture formats. The
 * corresponding `sdlrender_update_colors` should be used with these renderers.
 *
 * This function will also pick nearest or linear scaling automatically based
 * on the scaling ratio and window size.
 */
boolean sdlrender_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize,
 boolean requires_blend_ops)
{
  struct sdl_render_data *render_data = graphics->render_data;
  boolean fullscreen_windowed = graphics->fullscreen_windowed;
  uint32_t sdl_rendererflags = 0;

  sdl_destruct_window(graphics);

  // Use linear filtering unless the display is being integer scaled.
#if SDL_VERSION_ATLEAST(2,0,12)
  if(is_integer_scale(graphics, width, height))
    render_data->screen_scale_mode = SDL_SCALEMODE_NEAREST;
  else
    render_data->screen_scale_mode = SDL_SCALEMODE_LINEAR;
#else
  if(is_integer_scale(graphics, width, height))
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
#endif

#if defined(__EMSCRIPTEN__) && SDL_VERSION_ATLEAST(2,0,10)
  // Not clear if this hint is required to make this renderer not crash, but
  // considering both software and GLSL need it...
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif

#if !SDL_VERSION_ATLEAST(3,0,0)
  // Flag removed in SDL3; all drivers support it and its presence needs to be
  // tested by trying to create a target texture instead.
  if(requires_blend_ops)
    sdl_rendererflags |= SDL_RENDERER_TARGETTEXTURE;
#endif

  if(graphics->sdl_render_driver[0])
  {
    info("Requesting SDL render driver: '%s'\n", graphics->sdl_render_driver);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, graphics->sdl_render_driver);
  }

  render_data->window = SDL_CreateWindow("MegaZeux",
// FIXME:
#if !SDL_VERSION_ATLEAST(3,0,0)
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
#endif
   width, height,
   sdl_flags(depth, fullscreen, fullscreen_windowed, resize));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  // Note: previously attempted SDL_RENDERER_ACCELERATED first, then
  // SDL_RENDERER_SOFTWARE, but these flags were removed in SDL3.
#if SDL_VERSION_ATLEAST(3,0,0)
  render_data->renderer = SDL_CreateRenderer(render_data->window, BEST_RENDERER);
#else
  render_data->renderer =
   SDL_CreateRenderer(render_data->window, BEST_RENDERER, sdl_rendererflags);
#endif
  if(!render_data->renderer)
  {
    warn("Failed to create renderer: %s\n", SDL_GetError());
    goto err_free;
  }

  find_texture_format(graphics, sdl_rendererflags);

  if(!render_data->rgb_to_yuv)
  {
#if SDL_VERSION_ATLEAST(3,0,0)
#error wtf
#else
    // This is required for SDL_MapRGBA to work, but YUV formats can ignore it.
    render_data->pixel_format = SDL_AllocFormat(render_data->texture_format);
    if(!render_data->pixel_format)
    {
      warn("Failed to allocate pixel format: %s\n", SDL_GetError());
      goto err_free;
    }
    render_data->flat_format = render_data->pixel_format;
#endif
  }

  SDL_SetRenderDrawColor(render_data->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(render_data->renderer);

  sdl_window_id = SDL_GetWindowID(render_data->window);
  sdl_set_screensaver_enabled(graphics->disable_screensaver == SCREENSAVER_ENABLE);
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

#endif /* CONFIG_RENDER_SOFTSCALE || CONFIG_RENDER_SDLACCEL */


/* SDL direct OpenGL functions for: *****************************************/
// opengl1
// opengl2
// glsl / glslscale

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
  // FIXME:
#if !SDL_VERSION_ATLEAST(3,0,0)
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
#endif
   width, height,
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
  sdl_set_screensaver_enabled(graphics->disable_screensaver == SCREENSAVER_ENABLE);

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
