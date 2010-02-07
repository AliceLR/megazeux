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

#include "config.h"
#include "graphics.h"
#include "render.h"
#include "renderers.h"

#include <stdlib.h>

typedef struct
{
  SDL_Surface *screen;
  Uint32 halfmask;
} gp2x_render_data;

static void gp2x_set_colors_mzx (graphics_data *graphics, Uint32 *char_colors,
 Uint8 bg, Uint8 fg)
{
  gp2x_render_data *render_data = graphics->render_data;
  Uint32 cb_bg, cb_fg, cb_mx;

  cb_bg = graphics->flat_intensity_palette[bg];
  cb_fg = graphics->flat_intensity_palette[fg];
  cb_mx = ((cb_bg >> 1) & render_data->halfmask) +
   ((cb_fg >> 1) & render_data->halfmask);

#if SDL_BYTE_ORDER == SDL_BIG_ENDIAN
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

static void gp2x_set_colors_smzx (graphics_data *graphics, Uint32 *char_colors,
 Uint8 bg, Uint8 fg)
{
  Uint32 bb, bf, fb, ff;

  bg &= 0x0F;
  fg &= 0x0F;

  bb = graphics->flat_intensity_palette[(bg << 4) | bg];
  bf = graphics->flat_intensity_palette[(bg << 4) | fg];
  fb = graphics->flat_intensity_palette[(fg << 4) | bg];
  ff = graphics->flat_intensity_palette[(fg << 4) | fg];

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
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

static void gp2x_set_colors_smzx3 (graphics_data *graphics, Uint32 *char_colors,
 Uint8 bg, Uint8 fg)
{
  Uint8 base;
  Uint32 c0, c1, c2, c3;

  base = (bg << 4) | (fg & 0x0F);

  c0 = graphics->flat_intensity_palette[base];
  c1 = graphics->flat_intensity_palette[(base + 1) & 0xFF];
  c2 = graphics->flat_intensity_palette[(base + 2) & 0xFF];
  c3 = graphics->flat_intensity_palette[(base + 3) & 0xFF];

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
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

static void (*gp2x_set_colors[4])(graphics_data *, Uint32 *, Uint8, Uint8) =
{
  gp2x_set_colors_mzx,
  gp2x_set_colors_smzx,
  gp2x_set_colors_smzx,
  gp2x_set_colors_smzx3
};

static int gp2x_init_video(graphics_data *graphics, config_info *conf)
{
  gp2x_render_data *render_data = malloc(sizeof(gp2x_render_data));

  if(!render_data)
    return false;

  graphics->render_data = render_data;

  graphics->allow_resize = 0;

  graphics->bits_per_pixel = 16;

  graphics->resolution_width = 320;
  graphics->resolution_height = 350;
  graphics->window_width = 320;
  graphics->window_height = 350;

  return set_video_mode();
}

static int gp2x_check_video_mode(graphics_data *graphics, int width, int height,
 int depth, int flags)
{
  return SDL_VideoModeOK(width, height, 16, flags);
}

static int gp2x_set_video_mode(graphics_data *graphics, int width, int height,
 int depth, int flags, int fullscreen)
{
  gp2x_render_data *render_data = graphics->render_data;
  SDL_PixelFormat *format;
  Uint32 halfmask;

  render_data->screen = SDL_SetVideoMode(width, height, 16, flags);
  if(render_data->screen)
  {
    format = render_data->screen->format;
    halfmask = (format->Rmask >> 1) & format->Rmask;
    halfmask |= (format->Gmask >> 1) & format->Gmask;
    halfmask |= (format->Bmask >> 1) & format->Bmask;
    render_data->halfmask = halfmask;
    return true;
  }

  return false;
}

static void gp2x_update_colors(graphics_data *graphics, SDL_Color *palette,
 Uint32 count)
{
  gp2x_render_data *render_data = graphics->render_data;
  Uint32 i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
     SDL_MapRGBA(render_data->screen->format, palette[i].r, palette[i].g,
     palette[i].b, 255);
  }
}

static void gp2x_get_screen_coords(graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y)
{
  *x = screen_x * 2;
  *y = screen_y;
  *min_x = 0;
  *min_y = 0;
  *max_x = 319;
  *max_y = 349;
}

static void gp2x_set_screen_coords(graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y)
{
  *screen_x = x / 2;
  *screen_y = y;
}

static void gp2x_render_graph(graphics_data *graphics)
{
  gp2x_render_data *render_data = graphics->render_data;
  SDL_LockSurface(render_data->screen);
  render_graph8((Uint8 *)render_data->screen->pixels,
   render_data->screen->pitch, graphics,
   gp2x_set_colors[graphics->screen_mode]);
  SDL_UnlockSurface(render_data->screen);
}

static void gp2x_render_cursor(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 color, Uint8 lines, Uint8 offset)
{
  gp2x_render_data *render_data = graphics->render_data;
  Uint32 flatcolor = graphics->flat_intensity_palette[color];
  flatcolor |= flatcolor << 16;
  SDL_LockSurface(render_data->screen);
  render_cursor((Uint32 *)render_data->screen->pixels,
   render_data->screen->pitch, 8, x, y, flatcolor, lines, offset);
  SDL_UnlockSurface(render_data->screen);
}

static void gp2x_render_mouse(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 w, Uint8 h)
{
  gp2x_render_data *render_data = graphics->render_data;
  SDL_LockSurface(render_data->screen);
  render_mouse((Uint32 *)render_data->screen->pixels,
   render_data->screen->pitch, 8, x, y, 0xFFFFFFFF, w, h);
  SDL_UnlockSurface(render_data->screen);
}

static void gp2x_sync_screen(graphics_data *graphics)
{
  gp2x_render_data *render_data = graphics->render_data;
  SDL_Flip(render_data->screen);
}

void render_gp2x_register(graphics_data *graphics)
{
  graphics->init_video = gp2x_init_video;
  graphics->check_video_mode = gp2x_check_video_mode;
  graphics->set_video_mode = gp2x_set_video_mode;
  graphics->update_colors = gp2x_update_colors;
  graphics->resize_screen = resize_screen_standard;
  graphics->remap_charsets = NULL;
  graphics->remap_char = NULL;
  graphics->remap_charbyte = NULL;
  graphics->get_screen_coords = gp2x_get_screen_coords;
  graphics->set_screen_coords = gp2x_set_screen_coords;
  graphics->render_graph = gp2x_render_graph;
  graphics->render_cursor = gp2x_render_cursor;
  graphics->render_mouse = gp2x_render_mouse;
  graphics->sync_screen = gp2x_sync_screen;
}
