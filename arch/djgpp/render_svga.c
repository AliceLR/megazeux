/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2018, 2019 Adrian Siekierka <kontakt@asie.pl>
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

#define delay delay_dos
#include <sys/nearptr.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#undef delay

#include "../../src/compat.h"
#include "../../src/graphics.h"
#include "../../src/render.h"
#include "../../src/render_layer.h"
#include "../../src/renderers.h"
#include "../../src/util.h"

#include "platform_djgpp.h"

struct svga_render_data
{
  __dpmi_meminfo mapping;
  Uint8 *ptr;
  Uint16 mode, pitch;
};

static boolean svga_try_mode(struct graphics_data *graphics, Uint16 mode, Uint16 width, Uint16 height, Uint16 bpp)
{
  struct svga_render_data *render_data = graphics->render_data;
  __dpmi_regs reg;

  reg.x.ax = 0x4F02;
  reg.x.bx = mode | 0x4000 /* LFB */;
  __dpmi_int(0x10, &reg);

  if(reg.h.ah != 0x00)
    return false;

  graphics->resolution_width = graphics->window_width = width;
  graphics->resolution_height = graphics->window_height = height;
  graphics->bits_per_pixel = bpp;
  render_data->mode = mode | 0x4000;

  return true;
}

static boolean svga_try_modes(struct graphics_data *graphics)
{
  if(graphics->bits_per_pixel == 16)
  {
    if(svga_try_mode(graphics, 0x111, 640, 480, 16))
      return true;
  }

  if(graphics->bits_per_pixel == 8)
  {
    if(svga_try_mode(graphics, 0x100, 640, 400, 8))
      return true;
    if(svga_try_mode(graphics, 0x101, 640, 480, 8))
      return true;
  }
  return false;
}

static boolean svga_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct svga_render_data render_data;
  int display;
  struct vbe_mode_info vbe;
  int x_offset, y_offset;
  __dpmi_regs reg;

  memset(&render_data, 0, sizeof(struct svga_render_data));
  graphics->render_data = &render_data;

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 16;

  if(!__djgpp_nearptr_enable())
    return false;

  display = djgpp_display_adapter_detect();
  if(display < DISPLAY_ADAPTER_VBE20)
  {
    warn("Could not find VBE 2.0+-compatible graphics card!\n");
    return false;
  }

  if(!svga_try_modes(graphics))
  {
    warn("Could not find supported VESA graphics mode!\n");
    return false;
  }

  reg.x.ax = 0x4F01;
  reg.x.cx = render_data.mode;
  reg.x.di = __tb & 0xF;
  reg.x.es = (__tb >> 4);
  __dpmi_int(0x10, &reg);

  if(reg.x.ax != 0x004F)
  {
    warn("Could not initialize VESA graphics mode!\n");
    return false;
  }

  dosmemget(__tb, sizeof(struct vbe_mode_info), &vbe);
  render_data.mapping.address = vbe.linear_ptr;
  render_data.mapping.size = ((graphics->resolution_width * graphics->resolution_height * (graphics->bits_per_pixel >> 3)) + 4095) & (~4095);
  if(__dpmi_physical_address_mapping(&render_data.mapping) != 0)
  {
    // set text video mode, leave
    reg.x.ax = 0x0010;
    __dpmi_int(0x10, &reg);
    warn("Could not map VESA video memory! (requested %ld bytes at %08lX for mode %04X)\n", render_data.mapping.size, render_data.mapping.address, render_data.mode);
    return false;
  }

  x_offset = (graphics->resolution_width - 640) / 2;
  y_offset = (graphics->resolution_height - 350) / 2;

  render_data.ptr = (Uint8*)(render_data.mapping.address + __djgpp_conventional_base) + (vbe.pitch * y_offset) + ((vbe.bpp >> 3) * x_offset);
  render_data.pitch = vbe.pitch;

  memset((Uint8*)(render_data.mapping.address + __djgpp_conventional_base), 0, vbe.pitch * vbe.height);

  return set_video_mode();
}

static void svga_free_video(struct graphics_data *graphics)
{
  struct svga_render_data *render_data = graphics->render_data;
  __dpmi_free_physical_address_mapping(&(render_data->mapping));
  __djgpp_nearptr_disable();
}

static boolean svga_check_video_mode(struct graphics_data *graphics, int width,
 int height, int depth, boolean fullscreen, boolean resize)
{
  return true;
}

static boolean svga_set_video_mode(struct graphics_data *graphics, int width,
 int height, int depth, boolean fullscreen, boolean resize)
{
  return true;
}

static void svga_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  Uint32 i;

  if(graphics->bits_per_pixel == 16)
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
          ((palette[i].r & 0xF8) << 8)
        | ((palette[i].g & 0xFC) << 3)
        | ((palette[i].b & 0xF8) >> 3);
    }
  else
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
          (palette[i].r << 16)
        | (palette[i].g << 8)
        | (palette[i].b << 0);
    }
}

static void svga_render_graph(struct graphics_data *graphics)
{
  struct svga_render_data *render_data = graphics->render_data;
  // TODO: 8bpp
  render_graph16((Uint16*)render_data->ptr, render_data->pitch, graphics, set_colors16[graphics->screen_mode]);
}

static void svga_render_layer(struct graphics_data *graphics,
 struct video_layer *vlayer)
{
  struct svga_render_data *render_data = graphics->render_data;
  render_layer(render_data->ptr, graphics->bits_per_pixel, render_data->pitch, graphics, vlayer);
}

static void svga_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  struct svga_render_data *render_data = graphics->render_data;
  Uint32 flatcolor = graphics->flat_intensity_palette[color] * 0x10001;
  // TODO: 8bpp
  render_cursor((Uint32*)render_data->ptr, render_data->pitch, graphics->bits_per_pixel, x, y, flatcolor, lines, offset);
}

static void svga_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct svga_render_data *render_data = graphics->render_data;
  render_mouse((Uint32*)render_data->ptr, render_data->pitch, graphics->bits_per_pixel, x, y, 0xFFFFFFFF, 0, w, h);
}

static void svga_sync_screen(struct graphics_data *graphics)
{
//  struct svga_render_data *render_data = graphics->render_data;
//  vid_flip(-1);
}

void render_svga_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = svga_init_video;
  renderer->free_video = svga_free_video;
  renderer->check_video_mode = svga_check_video_mode;
  renderer->set_video_mode = svga_set_video_mode;
  renderer->update_colors = svga_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = svga_render_graph;
  renderer->render_layer = svga_render_layer;
  renderer->render_cursor = svga_render_cursor;
  renderer->render_mouse = svga_render_mouse;
  renderer->sync_screen = svga_sync_screen;
}

