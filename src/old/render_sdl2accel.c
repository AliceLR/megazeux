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

// SDL2 hardware accelerated renderer using the SDL_Renderer API.
// This thing is too slow to have much use in practice right now, but it might
// be useful to keep around for reference. SMZX is not fully implemented yet
// and the placeholder mouse cursor implementation is pretty awful.
//
// The main performance problem here is writes to the chars texture. There's
// no compelling way to make it faster, and even a few rows updated can cause
// a significant FPS drop at speed 2. On top of that even just drawing chars to
// a screen is much slower than MZX's software-based renderers right now.

#include <SDL.h>

#include "graphics.h"
#include "platform.h"
#include "render.h"
#include "renderers.h"
#include "render_sdl.h"

// 6 versions of each char, 16*256 chars -> 24576 total "chars"
// -> 49152 8x8s -> sqrt ~= 221 > 32 * 6

// 32 * 6 * 8 = 1536 px
// 128 * 1 * 14 = 1792 px
#define CHARS_ROW_SIZE 32
#define CHARS_NUM_ROWS (FULL_CHARSET_SIZE / CHARS_ROW_SIZE)

#define NUM_CHAR_VARIANTS 6

// ARGB8888
#define CHARS_RAW_ROW_W (CHAR_W * NUM_CHAR_VARIANTS * CHARS_ROW_SIZE)
#define CHARS_RAW_ROW_H (CHAR_H)
#define CHARS_RAW_ROW_SIZE (CHARS_RAW_ROW_W * CHARS_RAW_ROW_H)
#define CHARS_RAW_ROW_PITCH (CHARS_RAW_ROW_W * sizeof(Uint32))

#define TEX_CHARS_PIX_W (CHAR_W * NUM_CHAR_VARIANTS * CHARS_ROW_SIZE)
#define TEX_CHARS_PIX_H (CHAR_H * CHARS_NUM_ROWS)

#define WHITE (0xFFFFFFFF)

#define NUM_WORKERS 8
#define QUEUE_SIZE (CHARS_NUM_ROWS * 2)

struct sdl2_render_data
{
  struct sdl_render_data sdl;
  Uint8 palette[FULL_PAL_SIZE * 3];
  Uint32 chars_raw[TEX_CHARS_PIX_W * TEX_CHARS_PIX_H];
  boolean chars_dirty_row[CHARS_NUM_ROWS];
  boolean chars_dirty;
  platform_thread t[NUM_WORKERS];
  platform_cond cond_main;
  platform_mutex mutex_main;
  Uint8 queue[QUEUE_SIZE];
  boolean join;
  int q_first;
  int q_last;
  int tex_chars_w;
  int tex_chars_h;
};

static inline int next_power_of_two(int v)
{
  int i;
  for(i = 1; i <= 65536; i *= 2)
    if(i >= v)
      return i;

  debug("Couldn't find power of two for %d\n", v);
  return v;
}

static void write_char_byte_mzx(Uint32 **_char_data, Uint8 byte)
{
  Uint32 *char_data = *_char_data;
  *(char_data++) = (Uint32)((Sint32)byte << 24 >> 31);
  *(char_data++) = (Uint32)((Sint32)byte << 25 >> 31);
  *(char_data++) = (Uint32)((Sint32)byte << 26 >> 31);
  *(char_data++) = (Uint32)((Sint32)byte << 27 >> 31);
  *(char_data++) = (Uint32)((Sint32)byte << 28 >> 31);
  *(char_data++) = (Uint32)((Sint32)byte << 29 >> 31);
  *(char_data++) = (Uint32)((Sint32)byte << 30 >> 31);
  *(char_data++) = (Uint32)((Sint32)byte << 31 >> 31);
  *_char_data = char_data;
}

static void write_char_byte_smzx(Uint32 **_char_data, Uint8 byte, Uint8 col)
{
  Uint32 *char_data = *_char_data;
  *(char_data++) = ((byte >> 6) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 6) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 4) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 4) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 2) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 2) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 0) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 0) & 3) == col ? WHITE : 0;
  *_char_data = char_data;
}

static void sdl2_do_remap_row(struct graphics_data *graphics,
 struct sdl2_render_data *render_data, Uint32 *dest, Uint32 pitch, int row)
{
  Uint8 char_byte;
  Uint32 skip = (pitch - CHARS_RAW_ROW_PITCH) / sizeof(Uint32);
  int x;
  int y;

  for(y = 0; y < CHAR_H; y++)
  {
    for(x = 0; x < CHARS_ROW_SIZE; x++)
    {
      char_byte = graphics->charset[y + CHAR_H * (x + row * CHARS_ROW_SIZE)];
      write_char_byte_mzx(&dest, char_byte ^ 0xFF);
      write_char_byte_mzx(&dest, char_byte);
      write_char_byte_smzx(&dest, char_byte, 0);
      write_char_byte_smzx(&dest, char_byte, 1);
      write_char_byte_smzx(&dest, char_byte, 2);
      write_char_byte_smzx(&dest, char_byte, 3);
    }
    dest += skip;
  }
}

static THREAD_RES sdl2_remap_worker(void *data)
{
  struct graphics_data *graphics = (struct graphics_data *)data;
  struct sdl2_render_data *render_data = graphics->render_data;
  Uint32 *pixels;
  int row;

  while(true)
  {
    delay(1);

    if(render_data->join)
      THREAD_RETURN;

    if(render_data->q_first == render_data->q_last)
      continue;

    while(render_data->q_first != render_data->q_last)
    {
      row = render_data->queue[render_data->q_first];
      render_data->q_first = (render_data->q_first + 1) % QUEUE_SIZE;

      pixels = render_data->chars_raw + CHARS_RAW_ROW_SIZE * row;
      sdl2_do_remap_row(graphics, render_data, pixels, CHARS_RAW_ROW_PITCH, row);
    }

    platform_cond_signal(&(render_data->cond_main));
  }
}

static void sdl2_free_video(struct graphics_data *graphics)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  int i;

  if(render_data)
  {
    sdl_destruct_window(graphics);

/*
    render_data->join = true;

    for(i = 0; i < NUM_WORKERS; i++)
      platform_thread_join(&(render_data->t[i]));

    platform_mutex_destroy(&(render_data->mutex_main));
    platform_cond_destroy(&(render_data->cond_main));
*/
    graphics->render_data = NULL;
    free(render_data);
  }
}

static boolean sdl2_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct sdl2_render_data *render_data =
   cmalloc(sizeof(struct sdl2_render_data));
  int i;

  memset(render_data, 0, sizeof(struct sdl2_render_data));
  memset(render_data->chars_dirty_row, true, CHARS_NUM_ROWS);
  render_data->chars_dirty = true;
  graphics->render_data = render_data;

  if(!set_video_mode())
  {
    sdl2_free_video(graphics);
    return false;
  }
/*
  platform_mutex_init(&(render_data->mutex_main));
  platform_cond_init(&(render_data->cond_main));

  for(i = 0; i < NUM_WORKERS; i++)
    platform_thread_create(&(render_data->t[i]), sdl2_remap_worker, graphics);
*/
  return true;
}

static boolean sdl2_set_video_mode(struct graphics_data *graphics, int width,
 int height, int depth, boolean fullscreen, boolean resize)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  boolean fullscreen_windowed = graphics->fullscreen_windowed;
  SDL_Rect screen = { 0, 0, SCREEN_PIX_W, SCREEN_PIX_H };
  SDL_RendererInfo info;
  Uint32 format = SDL_PIXELFORMAT_RGBA8888;
  int tex_chars_w;
  int tex_chars_h;

  sdl_destruct_window(graphics);

  render_data->sdl.window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   sdl_flags(depth, fullscreen, fullscreen_windowed, resize));

  if(!render_data->sdl.window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

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

    warn("Accelerated renderer not available. SDL2Accel will be SLOW!\n");
  }

  if(SDL_RenderSetLogicalSize(render_data->sdl.renderer, screen.w, screen.h))
    warn("Failed to set renderer locical size!\n");

  if(SDL_RenderSetClipRect(render_data->sdl.renderer, &screen))
    warn("Failed to set clip rectangle!\n");

  if(!SDL_GetRendererInfo(render_data->sdl.renderer, &info))
  {
    Uint32 i;
    debug("Using SDL renderer '%s'\n", info.name);

    // Try to use a native texture format to improve performance.
    for(i = 0; i < info.num_texture_formats; i++)
    {
      debug("%d: %s\n", i, SDL_GetPixelFormatName(info.texture_formats[i]));
      switch(info.texture_formats[i])
      {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_ABGR8888:
        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_ARGB2101010:
        {
          // Any supported 32-bit alpha format is okay.
          format = info.texture_formats[i];
          break;
        }
      }
    }
  }
  else
    warn("Failed to get renderer info! Using RGBA8888\n");

  tex_chars_w = next_power_of_two(TEX_CHARS_PIX_W);
  tex_chars_h = next_power_of_two(TEX_CHARS_PIX_H);

  // Don't use SDL_TEXTUREACCESS_STREAMING here right now.
  //
  // The main difference at the moment is it keeps a duplicate buffer of the
  // entire texture in memory. Changing arbitrary parts of this texture
  // requires locking the whole thing, after which the whole thing will be
  // uploaded (SLOW). It's better for the typical use case to write to a
  // smaller local version and only upload recently updated parts.
  render_data->sdl.texture =
   SDL_CreateTexture(render_data->sdl.renderer, format,
    SDL_TEXTUREACCESS_STREAMING, tex_chars_w, tex_chars_h);

  if(!render_data->sdl.texture)
  {
    warn("Failed to create texture: %s\n", SDL_GetError());
    goto err_free;
  }

  if(SDL_SetTextureBlendMode(render_data->sdl.texture, SDL_BLENDMODE_BLEND))
    warn("Failed to set texture blend mode: %s\n", SDL_GetError());

  render_data->tex_chars_w = tex_chars_w;
  render_data->tex_chars_h = tex_chars_h;
  sdl_window_id = SDL_GetWindowID(render_data->sdl.window);
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

static void sdl2_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  Uint32 i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] = (0xFF << 24) | (palette[i].b << 16) |
     (palette[i].g << 8) | palette[i].r;

    render_data->palette[i*3  ] = palette[i].r;
    render_data->palette[i*3+1] = palette[i].g;
    render_data->palette[i*3+2] = palette[i].b;
  }
}

static void sdl2_remap_char_range(struct graphics_data *graphics, Uint16 first,
 Uint16 count)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  int last;

  if(first + count > FULL_CHARSET_SIZE)
    count = FULL_CHARSET_SIZE - first;

  last = (first + count - 1) / CHARS_ROW_SIZE;
  first = first / CHARS_ROW_SIZE;

  memset(render_data->chars_dirty_row + first, true, last - first + 1);
  render_data->chars_dirty = true;
}

static void sdl2_remap_char(struct graphics_data *graphics, Uint16 chr)
{
  struct sdl2_render_data *render_data = graphics->render_data;

  if(chr >= FULL_CHARSET_SIZE)
    chr = FULL_CHARSET_SIZE - 1;

  render_data->chars_dirty_row[chr / CHARS_ROW_SIZE] = true;
  render_data->chars_dirty = true;
}

static void sdl2_remap_charbyte(struct graphics_data *graphics, Uint16 chr,
 Uint8 byte)
{
  sdl2_remap_char(graphics, chr);
}

static void sdl2_do_remap_chars(struct graphics_data *graphics,
 struct sdl2_render_data *render_data)
{
  SDL_Rect rect = { 0, 0, CHARS_RAW_ROW_W, CHARS_RAW_ROW_H };
  int start;
  int i;

  void *pixels;
  int pitch = CHARS_RAW_ROW_PITCH;

  //Uint32 *pixels_row;

  for(i = 0; i < CHARS_NUM_ROWS; i++)
  {
    if(render_data->chars_dirty_row[i])
    {
      //render_data->queue[render_data->q_last] = i;
      //render_data->q_last = (render_data->q_last + 1) % QUEUE_SIZE;
      rect.y = i * CHAR_H;
      SDL_LockTexture(render_data->sdl.texture, &rect, &pixels, &pitch);

      //pixels = render_data->chars_raw + i * CHARS_RAW_ROW_SIZE;

      sdl2_do_remap_row(graphics, render_data, pixels, pitch, i);

      //rect.y = i * CHAR_H;
      //SDL_UpdateTexture(render_data->sdl.texture, &rect, pixels, pitch);

      SDL_UnlockTexture(render_data->sdl.texture);
      render_data->chars_dirty_row[i] = false;
    }
  }
/*
  if(render_data->q_first == render_data->q_last)
    return;

  platform_mutex_lock(&(render_data->mutex_main));
  platform_cond_wait(&(render_data->cond_main), &(render_data->mutex_main));
  platform_mutex_unlock(&(render_data->mutex_main));

  for(i = 0; i < CHARS_NUM_ROWS; i++)
  {
    if(render_data->chars_dirty_row[i])
    {
      start = i;
      do
      {
        render_data->chars_dirty_row[i] = false;
        i++;
      }
      while(i < CHARS_NUM_ROWS && render_data->chars_dirty_row[i]);

      pixels = render_data->chars_raw + start * CHARS_RAW_ROW_SIZE;

      rect.y = start * CHAR_H;
      rect.h = (i - start) * CHARS_RAW_ROW_H;
      SDL_UpdateTexture(render_data->sdl.texture, &rect, pixels, pitch);
    }
  }*/
}

static void sdl2_set_color(struct graphics_data *graphics, int color, int mode)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  Uint8 *palette = render_data->palette;

  if(!mode && color >= 16)
    color = graphics->protected_pal_position + (color % 16);

  SDL_SetTextureColorMod(render_data->sdl.texture,
   palette[color * 3], palette[color * 3 + 1], palette[color * 3 + 2]);
}

static void sdl2_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  struct char_element *next = layer->data;
  int offx = layer->x;
  int offy = layer->y;
  int w = layer->w;
  int h = layer->h;
  int x;
  int y;
  int transparent_color = layer->transparent_col;
  int chr;

  SDL_Renderer *renderer = render_data->sdl.renderer;
  SDL_Texture *chars_tex = render_data->sdl.texture;
  SDL_Rect dest_rect = { 0, 0, CHAR_W, CHAR_H };
  SDL_Rect src_rect = { 0, 0, CHAR_W, CHAR_H };

  if(render_data->chars_dirty)
  {
    sdl2_do_remap_chars(graphics, render_data);
    render_data->chars_dirty = false;
  }

  if(!layer->mode)
  {
    for(y = 0; y < h; y++)
    {
      for(x = 0; x < w; x++, next++)
      {
        if(next->char_value == INVISIBLE_CHAR)
          continue;

        chr = next->char_value + layer->offset;
        src_rect.y = (chr / CHARS_ROW_SIZE) * CHAR_H;
        dest_rect.x = offx + x * CHAR_W;
        dest_rect.y = offy + y * CHAR_H;

        if(next->bg_color != transparent_color)
        {
          src_rect.x = (((chr % CHARS_ROW_SIZE) * NUM_CHAR_VARIANTS) + 0) * CHAR_W;

          sdl2_set_color(graphics, next->bg_color, 0);
          SDL_RenderCopy(renderer, chars_tex, &src_rect, &dest_rect);
        }

        if(next->fg_color != transparent_color)
        {
          src_rect.x = (((chr % CHARS_ROW_SIZE) * NUM_CHAR_VARIANTS) + 1) * CHAR_W;

          sdl2_set_color(graphics, next->fg_color, 0);
          SDL_RenderCopy(renderer, chars_tex, &src_rect, &dest_rect);
        }
      }
    }
  }
  else
  {
    // FIXME
  }
}

static void sdl2_render_cursor(struct graphics_data *graphics, Uint32 x,
 Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  Uint8 *palette = render_data->palette;

  // x/y are char values :/
  SDL_Rect dest = { x * CHAR_W, y * CHAR_H + offset, CHAR_W, lines };

  SDL_SetRenderDrawBlendMode(render_data->sdl.renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(render_data->sdl.renderer,
   palette[color * 3], palette[color * 3 + 1], palette[color * 3 + 2],
   SDL_ALPHA_OPAQUE);

  SDL_RenderFillRect(render_data->sdl.renderer, &dest);
}

static void sdl2_render_mouse(struct graphics_data *graphics, Uint32 x,
 Uint32 y, Uint8 w, Uint8 h)
{
  struct sdl2_render_data *render_data = graphics->render_data;
  //Uint8 *palette = render_data->palette;

  // x/y are pixel values :/
  SDL_Rect dest = { x, y, w, h };

  // FIXME: SDL forgot to add inversion blend mode. Whoops!
  // Just draw the MZX VERSION DISCO mouse or whatever
  int n = rand();
  int r = (n>>1)&0xFF;
  int g = (n>>4)&0xFF;
  int b = (n>>7)&0xFF;
  SDL_SetRenderDrawBlendMode(render_data->sdl.renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(render_data->sdl.renderer, r, g, b, 0xBF);

  SDL_RenderFillRect(render_data->sdl.renderer, &dest);
}

static void sdl2_sync_screen(struct graphics_data *graphics)
{
  struct sdl2_render_data *render_data = graphics->render_data;

  SDL_RenderPresent(render_data->sdl.renderer);

  SDL_SetRenderDrawColor(render_data->sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(render_data->sdl.renderer);
}

void render_sdl2accel_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = sdl2_init_video;
  renderer->free_video = sdl2_free_video;
  renderer->set_video_mode = sdl2_set_video_mode;
  renderer->update_colors = sdl2_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_char_range = sdl2_remap_char_range;
  renderer->remap_char = sdl2_remap_char;
  renderer->remap_charbyte = sdl2_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_layer = sdl2_render_layer;
  renderer->render_cursor = sdl2_render_cursor;
  renderer->render_mouse = sdl2_render_mouse;
  renderer->sync_screen = sdl2_sync_screen;
}
