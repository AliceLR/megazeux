/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Software renderer with SDL2 hardware accelerated scaling. An improvement on
 * the former SDL2 implementation of the overlay renderers.
 *
 * The overlay renderers use a method to get cheap hardware scaling that isn't
 * applicable to SDL2. The original approach to adapting them to SDL2 was to
 * use YUV textures with an accelerated SDL_Renderer, but as these packed YUV
 * textures tend to be non-native this incurs significant performance penalties
 * converting the pixels to the native format in software.
 *
 * By using a native texture format, this can be bypassed, making scaling much
 * faster. Additionally, rendering can be streamed directly to a texture pixel
 * buffer, saving more time. Finally, this renderer can automatically pick
 * nearest or linear scaling based on the scaling ratio and window size.
 */

#include <SDL.h>
#include <stdlib.h>

#include "graphics.h"
#include "render.h"
#include "render_layer.h"
#include "render_sdl.h"
#include "renderers.h"
#include "util.h"
#include "yuv.h"

struct softscale_render_data
{
  struct sdl_render_data sdl;
  uint32_t (*rgb_to_yuv)(uint8_t r, uint8_t g, uint8_t b);
  set_colors_function subsample_set_colors;
  SDL_PixelFormat *sdl_format;
  SDL_Rect texture_rect;
  uint32_t *texture_pixels;
  uint32_t texture_format;
  uint32_t texture_amask;
  unsigned int texture_pitch;
  unsigned int texture_bpp;
  unsigned int texture_width;
  boolean allow_subsampling;
  boolean enable_subsampling;
  int w;
  int h;
};

static void softscale_free_video(struct graphics_data *graphics)
{
  struct softscale_render_data *render_data = graphics->render_data;

  if(render_data)
  {
    if(render_data->sdl_format)
    {
      SDL_FreeFormat(render_data->sdl_format);
      render_data->sdl_format = NULL;
    }

    sdl_destruct_window(graphics);

    graphics->render_data = NULL;
    free(render_data);
  }
}

static boolean softscale_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct softscale_render_data *render_data =
   cmalloc(sizeof(struct softscale_render_data));

  memset(render_data, 0, sizeof(struct softscale_render_data));
  graphics->render_data = render_data;
  graphics->allow_resize = conf->allow_resize;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 32;

  // This renderer can technically do 8bpp indexed or RGB332. The former is
  // too slow to be worthwhile and the latter requires fixing assumptions in
  // the layer renderer and a new set_colors8 (also, it looks awful).
  if(conf->force_bpp == BPP_AUTO || conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  snprintf(graphics->sdl_render_driver, ARRAY_SIZE(graphics->sdl_render_driver),
   "%s", conf->sdl_render_driver);

  if(!set_video_mode())
  {
    softscale_free_video(graphics);
    return false;
  }
  return true;
}

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

  SDL_PixelFormatEnumToMasks(format, &bpp, &rmask, &gmask, &bmask, &amask);
  return amask;
}

static void find_texture_format(struct graphics_data *graphics)
{
  struct softscale_render_data *render_data = graphics->render_data;
  uint32_t texture_format = SDL_PIXELFORMAT_UNKNOWN;
  unsigned int texture_width = SCREEN_PIX_W;
  unsigned int texture_bpp = 0;
  uint32_t texture_amask = 0;
  boolean allow_subsampling = false;
  boolean is_yuv = false;
  SDL_RendererInfo rinfo;

  if(!SDL_GetRendererInfo(render_data->sdl.renderer, &rinfo))
  {
    unsigned int depth = graphics->bits_per_pixel;
    unsigned int priority = 0;
    unsigned int i;

    info("SDL render driver: '%s'\n", rinfo.name);

    // Try to use a native texture format to improve performance.
    for(i = 0; i < rinfo.num_texture_formats; i++)
    {
      uint32_t format = rinfo.texture_formats[i];
      unsigned int format_priority;

      debug("%d: %s\n", i, SDL_GetPixelFormatName(format));

      if(SDL_ISPIXELFORMAT_INDEXED(texture_format))
        continue;

      format_priority = sdl_pixel_format_priority(format, depth, false);
      if(format_priority > priority)
      {
        texture_format = format;
        priority = format_priority;
      }
    }

    if(priority == YUV_PRIORITY)
      is_yuv = true;
  }
  else
    warn("Failed to get renderer info!\n");

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

  if(is_yuv)
  {
    // These modes treat the texture as 16bpp so double the width.
    texture_width = SCREEN_PIX_W * 2;
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

  // Initialize the texture data.
  render_data->texture_pixels = NULL;
  render_data->texture_format = texture_format;
  render_data->texture_amask = texture_amask;
  render_data->texture_bpp = texture_bpp;
  render_data->texture_width = texture_width;
  render_data->allow_subsampling = allow_subsampling;
}

static boolean softscale_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  struct softscale_render_data *render_data = graphics->render_data;
  boolean fullscreen_windowed = graphics->fullscreen_windowed;

  sdl_destruct_window(graphics);

  // Use linear filtering unless the display is being integer scaled.
  if(is_integer_scale(graphics, width, height))
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

#if defined(__EMSCRIPTEN__) && SDL_VERSION_ATLEAST(2,0,10)
  // Not clear if this hint is required to make this renderer not crash, but
  // considering both software and GLSL need it...
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif

  if(graphics->sdl_render_driver[0])
  {
    info("Requesting SDL render driver: '%s'\n", graphics->sdl_render_driver);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, graphics->sdl_render_driver);
  }

  render_data->sdl.window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   sdl_flags(depth, fullscreen, fullscreen_windowed, resize));

  if(!render_data->sdl.window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  render_data->sdl.renderer =
   SDL_CreateRenderer(render_data->sdl.window, -1, SDL_RENDERER_ACCELERATED);

  if(!render_data->sdl.renderer)
  {
    render_data->sdl.renderer =
     SDL_CreateRenderer(render_data->sdl.window, -1, SDL_RENDERER_SOFTWARE);

    if(!render_data->sdl.renderer)
    {
      warn("Failed to create renderer: %s\n", SDL_GetError());
      goto err_free;
    }

    warn("Accelerated renderer not available. Softscale will be SLOW!\n");
  }

  find_texture_format(graphics);

  render_data->sdl.texture =
   SDL_CreateTexture(render_data->sdl.renderer, render_data->texture_format,
    SDL_TEXTUREACCESS_STREAMING, render_data->texture_width, SCREEN_PIX_H);

  if(!render_data->sdl.texture)
  {
    warn("Failed to create texture: %s\n", SDL_GetError());
    goto err_free;
  }

  if(!render_data->rgb_to_yuv)
  {
    // This is required for SDL_MapRGBA to work, but YUV formats can ignore it.
    render_data->sdl_format = SDL_AllocFormat(render_data->texture_format);
    if(!render_data->sdl_format)
    {
      warn("Failed to allocate pixel format: %s\n", SDL_GetError());
      goto err_free;
    }
  }

  SDL_SetRenderDrawColor(render_data->sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(render_data->sdl.renderer);

  sdl_window_id = SDL_GetWindowID(render_data->sdl.window);
  render_data->w = width;
  render_data->h = height;
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

static void softscale_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct softscale_render_data *render_data = graphics->render_data;
  unsigned int i;

  if(!render_data->rgb_to_yuv)
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] = SDL_MapRGBA(render_data->sdl_format,
       palette[i].r, palette[i].g, palette[i].b, SDL_ALPHA_OPAQUE);
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

/**
 * Prepare the texture for streaming. This is a lot faster than handling a
 * separate pixels buffer. If the texture is already locked, only return the
 * stored pixels and pitch.
 */
static void softscale_lock_texture(struct softscale_render_data *render_data,
 boolean is_render_graph, uint32_t **pixels, unsigned int *pitch, unsigned int *bpp)
{
  if(!render_data->texture_pixels)
  {
    SDL_Rect *texture_rect = &(render_data->texture_rect);
    void *pixels;
    int pitch;

    texture_rect->x = 0;
    texture_rect->y = 0;
    texture_rect->w = render_data->texture_width;
    texture_rect->h = SCREEN_PIX_H;

    if(is_render_graph && render_data->allow_subsampling)
    {
      // Chroma subsampling can be used to draw half as much at the cost of
      // color accuracy. This trick only works with render_graph.
      texture_rect->w = render_data->texture_width / 2;
      render_data->enable_subsampling = true;
    }
    else
      render_data->enable_subsampling = false;

    SDL_LockTexture(render_data->sdl.texture, texture_rect, &pixels, &pitch);
    render_data->texture_pixels = pixels;
    render_data->texture_pitch = pitch;
  }

  *pixels = render_data->texture_pixels;
  *pitch = render_data->texture_pitch;
  *bpp = render_data->enable_subsampling ? 16 : render_data->texture_bpp;
}

/**
 * Commit the newly rendered screen to the texture and invalidate the pixel
 * array pointer.
 */
static void softscale_unlock_texture(struct softscale_render_data *render_data)
{
  if(render_data->texture_pixels)
  {
    SDL_UnlockTexture(render_data->sdl.texture);
    render_data->texture_pixels = NULL;
  }
}

static void softscale_render_graph(struct graphics_data *graphics)
{
  struct softscale_render_data *render_data = graphics->render_data;
  int mode = graphics->screen_mode;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  softscale_lock_texture(render_data, true, &pixels, &pitch, &bpp);

  if(render_data->enable_subsampling)
  {
    if(!mode)
      render_graph16((uint16_t *)pixels, pitch, graphics,
       render_data->subsample_set_colors);
    else
      render_graph16((uint16_t *)pixels, pitch, graphics, set_colors32[mode]);
  }
  else

  if(bpp == 16)
    render_graph16((uint16_t *)pixels, pitch, graphics, set_colors16[mode]);
  else

  if(bpp == 32)
  {
    if(!mode)
      render_graph32(pixels, pitch, graphics);
    else
      render_graph32s(pixels, pitch, graphics);
  }
}

static void softscale_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct softscale_render_data *render_data = graphics->render_data;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  softscale_lock_texture(render_data, false, &pixels, &pitch, &bpp);
  render_layer(pixels, bpp, pitch, graphics, layer);
}

static void softscale_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct softscale_render_data *render_data = graphics->render_data;
  uint32_t flatcolor;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  flatcolor = graphics->flat_intensity_palette[color];

  softscale_lock_texture(render_data, false, &pixels, &pitch, &bpp);

  if(bpp == 16 && !render_data->enable_subsampling)
    flatcolor |= flatcolor << 16;

  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
}

static void softscale_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct softscale_render_data *render_data = graphics->render_data;
  uint32_t amask = render_data->texture_amask;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  softscale_lock_texture(render_data, false, &pixels, &pitch, &bpp);

  if(bpp == 16)
    amask |= amask << 16;

  render_mouse(pixels, pitch, bpp, x, y, 0xFFFFFFFF, amask, w, h);
}

static void softscale_sync_screen(struct graphics_data *graphics)
{
  struct softscale_render_data *render_data = graphics->render_data;
  SDL_Renderer *renderer = render_data->sdl.renderer;
  SDL_Texture *texture = render_data->sdl.texture;
  SDL_Rect *src_rect = &(render_data->texture_rect);
  SDL_Rect dest_rect;
  int width = render_data->w;
  int height = render_data->h;
  int v_width, v_height;

  fix_viewport_ratio(width, height, &v_width, &v_height, graphics->ratio);

  dest_rect.x = (width - v_width) / 2;
  dest_rect.y = (height - v_height) / 2;
  dest_rect.w = v_width;
  dest_rect.h = v_height;

  softscale_unlock_texture(render_data);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(renderer);

  SDL_RenderCopy(renderer, texture, src_rect, &dest_rect);
  SDL_RenderPresent(renderer);
}

void render_softscale_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = softscale_init_video;
  renderer->free_video = softscale_free_video;
  renderer->set_video_mode = softscale_set_video_mode;
  renderer->update_colors = softscale_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = softscale_render_graph;
  renderer->render_layer = softscale_render_layer;
  renderer->render_cursor = softscale_render_cursor;
  renderer->render_mouse = softscale_render_mouse;
  renderer->sync_screen = softscale_sync_screen;
}
