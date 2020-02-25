/* MegaZeux
 *
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

#include "platform.h"
#include "graphics.h"
#include "render.h"
#include "render_sdl.h"
#include "renderers.h"

#include <stdlib.h>
#include <string.h>

#include "SDL.h"

struct gp2x_render_data
{
  struct sdl_render_data sdl;
  Uint32 halfmask;
  Uint16 buffer[320*350];
};

static SDL_Surface *
gp2x_get_screen_surface(struct gp2x_render_data *render_data)
{
  return render_data->sdl.shadow ?
   render_data->sdl.shadow : render_data->sdl.screen;
}

static void gp2x_set_colors_mzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  Uint32 cb_bg, cb_fg, cb_mx;

  cb_bg = graphics->flat_intensity_palette[bg];
  cb_fg = graphics->flat_intensity_palette[fg];
  if(cb_bg == cb_fg)
    cb_mx = cb_bg;
  else
    cb_mx = ((cb_bg >> 1) & render_data->halfmask) +
     ((cb_fg >> 1) & render_data->halfmask);

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (cb_bg << 16) | cb_bg;
  char_colors[1] = (cb_bg << 16) | cb_mx;
  char_colors[2] = (cb_bg << 16) | cb_mx;
  char_colors[3] = (cb_bg << 16) | cb_fg;
  char_colors[4] = (cb_mx << 16) | cb_bg;
  char_colors[5] = (cb_mx << 16) | cb_mx;
  char_colors[6] = (cb_mx << 16) | cb_mx;
  char_colors[7] = (cb_mx << 16) | cb_fg;
  char_colors[8] = (cb_mx << 16) | cb_bg;
  char_colors[9] = (cb_mx << 16) | cb_mx;
  char_colors[10] = (cb_mx << 16) | cb_mx;
  char_colors[11] = (cb_mx << 16) | cb_fg;
  char_colors[12] = (cb_fg << 16) | cb_bg;
  char_colors[13] = (cb_fg << 16) | cb_mx;
  char_colors[14] = (cb_fg << 16) | cb_mx;
  char_colors[15] = (cb_fg << 16) | cb_fg;
#else
  char_colors[0] = (cb_bg << 16) | cb_bg;
  char_colors[1] = (cb_mx << 16) | cb_bg;
  char_colors[2] = (cb_mx << 16) | cb_bg;
  char_colors[3] = (cb_fg << 16) | cb_bg;
  char_colors[4] = (cb_bg << 16) | cb_mx;
  char_colors[5] = (cb_mx << 16) | cb_mx;
  char_colors[6] = (cb_mx << 16) | cb_mx;
  char_colors[7] = (cb_fg << 16) | cb_mx;
  char_colors[8] = (cb_bg << 16) | cb_mx;
  char_colors[9] = (cb_mx << 16) | cb_mx;
  char_colors[10] = (cb_mx << 16) | cb_mx;
  char_colors[11] = (cb_fg << 16) | cb_mx;
  char_colors[12] = (cb_bg << 16) | cb_fg;
  char_colors[13] = (cb_mx << 16) | cb_fg;
  char_colors[14] = (cb_mx << 16) | cb_fg;
  char_colors[15] = (cb_fg << 16) | cb_fg;
#endif
}

static void gp2x_set_colors_smzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  Uint32 bb, bf, fb, ff;

  bg &= 0x0F;
  fg &= 0x0F;

  bb = graphics->flat_intensity_palette[(bg << 4) | bg];
  bf = graphics->flat_intensity_palette[(bg << 4) | fg];
  fb = graphics->flat_intensity_palette[(fg << 4) | bg];
  ff = graphics->flat_intensity_palette[(fg << 4) | fg];

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (bb << 16) | bb;
  char_colors[1] = (bb << 16) | bf;
  char_colors[2] = (bb << 16) | fb;
  char_colors[3] = (bb << 16) | ff;
  char_colors[4] = (bf << 16) | bb;
  char_colors[5] = (bf << 16) | bf;
  char_colors[6] = (bf << 16) | fb;
  char_colors[7] = (bf << 16) | ff;
  char_colors[8] = (fb << 16) | bb;
  char_colors[9] = (fb << 16) | bf;
  char_colors[10] = (fb << 16) | fb;
  char_colors[11] = (fb << 16) | ff;
  char_colors[12] = (ff << 16) | bb;
  char_colors[13] = (ff << 16) | bf;
  char_colors[14] = (ff << 16) | fb;
  char_colors[15] = (ff << 16) | ff;
#else
  char_colors[0] = (bb << 16) | bb;
  char_colors[1] = (bf << 16) | bb;
  char_colors[2] = (fb << 16) | bb;
  char_colors[3] = (ff << 16) | bb;
  char_colors[4] = (bb << 16) | bf;
  char_colors[5] = (bf << 16) | bf;
  char_colors[6] = (fb << 16) | bf;
  char_colors[7] = (ff << 16) | bf;
  char_colors[8] = (bb << 16) | fb;
  char_colors[9] = (bf << 16) | fb;
  char_colors[10] = (fb << 16) | fb;
  char_colors[11] = (ff << 16) | fb;
  char_colors[12] = (bb << 16) | ff;
  char_colors[13] = (bf << 16) | ff;
  char_colors[14] = (fb << 16) | ff;
  char_colors[15] = (ff << 16) | ff;
#endif
}

static void gp2x_set_colors_smzx3 (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  Uint8 base;
  Uint32 c0, c1, c2, c3;

  base = (bg << 4) | (fg & 0x0F);

  c0 = graphics->flat_intensity_palette[base];
  c1 = graphics->flat_intensity_palette[(base + 1) & 0xFF];
  c2 = graphics->flat_intensity_palette[(base + 2) & 0xFF];
  c3 = graphics->flat_intensity_palette[(base + 3) & 0xFF];

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (c0 << 16) | c0;
  char_colors[1] = (c0 << 16) | c2;
  char_colors[2] = (c0 << 16) | c1;
  char_colors[3] = (c0 << 16) | c3;
  char_colors[4] = (c2 << 16) | c0;
  char_colors[5] = (c2 << 16) | c2;
  char_colors[6] = (c2 << 16) | c1;
  char_colors[7] = (c2 << 16) | c3;
  char_colors[8] = (c1 << 16) | c0;
  char_colors[9] = (c1 << 16) | c2;
  char_colors[10] = (c1 << 16) | c1;
  char_colors[11] = (c1 << 16) | c3;
  char_colors[12] = (c3 << 16) | c0;
  char_colors[13] = (c3 << 16) | c2;
  char_colors[14] = (c3 << 16) | c1;
  char_colors[15] = (c3 << 16) | c3;
#else
  char_colors[0] = (c0 << 16) | c0;
  char_colors[1] = (c2 << 16) | c0;
  char_colors[2] = (c1 << 16) | c0;
  char_colors[3] = (c3 << 16) | c0;
  char_colors[4] = (c0 << 16) | c2;
  char_colors[5] = (c2 << 16) | c2;
  char_colors[6] = (c1 << 16) | c2;
  char_colors[7] = (c3 << 16) | c2;
  char_colors[8] = (c0 << 16) | c1;
  char_colors[9] = (c2 << 16) | c1;
  char_colors[10] = (c1 << 16) | c1;
  char_colors[11] = (c3 << 16) | c1;
  char_colors[12] = (c0 << 16) | c3;
  char_colors[13] = (c2 << 16) | c3;
  char_colors[14] = (c1 << 16) | c3;
  char_colors[15] = (c3 << 16) | c3;
#endif
}

static void (*gp2x_set_colors[4])
 (struct graphics_data *, Uint32 *, Uint8, Uint8) =
{
  gp2x_set_colors_mzx,
  gp2x_set_colors_smzx,
  gp2x_set_colors_smzx,
  gp2x_set_colors_smzx3
};

static boolean gp2x_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct gp2x_render_data *render_data =
   cmalloc(sizeof(struct gp2x_render_data));

  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct gp2x_render_data));
  graphics->render_data = render_data;

  graphics->allow_resize = 0;

  graphics->bits_per_pixel = 16;

  graphics->resolution_width = 320;
  graphics->resolution_height = 240;
  graphics->window_width = 320;
  graphics->window_height = 240;

  return set_video_mode();
}

static void gp2x_free_video(struct graphics_data *graphics)
{
  sdl_destruct_window(graphics);

  free(graphics->render_data);
  graphics->render_data = NULL;
}

static boolean gp2x_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  SDL_PixelFormat *format;
  Uint32 halfmask;

  if(!sdl_set_video_mode(graphics, width, height, depth, fullscreen, resize))
    return false;

  format = gp2x_get_screen_surface(render_data)->format;
  halfmask = (format->Rmask >> 1) & format->Rmask;
  halfmask |= (format->Gmask >> 1) & format->Gmask;
  halfmask |= (format->Bmask >> 1) & format->Bmask;
  render_data->halfmask = halfmask;

  return true;
}

static void gp2x_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  SDL_Surface *screen = gp2x_get_screen_surface(render_data);
  Uint32 i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
     SDL_MapRGBA(screen->format, palette[i].r, palette[i].g,
      palette[i].b, 255);
  }
}

static void gp2x_get_screen_coords(struct graphics_data *graphics,
 int screen_x, int screen_y, int *x, int *y, int *min_x, int *min_y,
 int *max_x, int *max_y)
{
  *x = screen_x * 2;
  *y = screen_y * 35 / 24;
  *min_x = 0;
  *min_y = 0;
  *max_x = 319;
  *max_y = 239;
}

static void gp2x_set_screen_coords(struct graphics_data *graphics,
 int x, int y, int *screen_x, int *screen_y)
{
  *screen_x = x / 2;
  *screen_y = y * 24 / 35;
}

static void gp2x_render_graph(struct graphics_data *graphics)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  render_graph8((Uint8 *)render_data->buffer, 640, graphics,
   gp2x_set_colors[graphics->screen_mode]);
}

static void gp2x_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  Uint32 flatcolor = graphics->flat_intensity_palette[color];
  flatcolor |= flatcolor << 16;
  render_cursor((Uint32 *)render_data->buffer, 640, 8, x, y, flatcolor, lines,
   offset);
}

static void gp2x_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  render_mouse((Uint32 *)render_data->buffer, 640, 8, x, y, 0xFFFFFFFF,
   0x0, w, h);
}

static void gp2x_sync_screen(struct graphics_data *graphics)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  SDL_Surface *screen = gp2x_get_screen_surface(render_data);

  Uint32 line_advance = screen->pitch / 2;
  Uint16 *dest = (Uint16*)screen->pixels;
  Uint16 *src = render_data->buffer;
  Uint32 i, j, skip = 0;

  SDL_LockSurface(screen);
  for(i = 0; i < 240; i++)
  {
    skip += 35 - 24;
    if(skip < 24)
      memcpy(dest, src, sizeof(Uint16) * 320);
    else
    {
      for(j = 0; j < 320; j++)
      {
        if(src[j] == src[j+320])
          dest[j] = src[j];
        else
          dest[j] = ((src[j] >> 1) & render_data->halfmask) +
           ((src[j+320] >> 1) & render_data->halfmask);
      }
      skip -= 24;
      src += 320;
    }
    src += 320;
    dest += line_advance;
  }

  SDL_UnlockSurface(screen);

  if(render_data->sdl.shadow)
  {
    SDL_BlitSurface(render_data->sdl.shadow,
     &render_data->sdl.shadow->clip_rect, render_data->sdl.screen,
     &render_data->sdl.screen->clip_rect);
  }

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_UpdateWindowSurface(render_data->sdl.window);
#else
  SDL_Flip(render_data->sdl.screen);
#endif
}

void render_gp2x_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = gp2x_init_video;
  renderer->free_video = gp2x_free_video;
  renderer->check_video_mode = sdl_check_video_mode;
  renderer->set_video_mode = gp2x_set_video_mode;
  renderer->update_colors = gp2x_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = gp2x_get_screen_coords;
  renderer->set_screen_coords = gp2x_set_screen_coords;
  renderer->render_graph = gp2x_render_graph;
  renderer->render_cursor = gp2x_render_cursor;
  renderer->render_mouse = gp2x_render_mouse;
  renderer->sync_screen = gp2x_sync_screen;
}
