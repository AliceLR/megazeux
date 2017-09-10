/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __3DS_RENDER_H__
#define __3DS_RENDER_H__

#include <stdlib.h>
#include <3ds.h>
#include <citro3d.h>

struct ctr_shader_data
{
  DVLB_s* dvlb;
  shaderProgram_s program;
  int proj_loc, offs_loc, uvoffs_loc, geo_count;
  C3D_AttrInfo attr;
};

struct ctr_layer
{
  Uint32 w, h, mode;
  int draw_order;
  struct v_char *foreground;
  C3D_Tex background;
};

struct v_char
{
  s16 x, y, z, w;
  u32 uv;
  u32 col;
};

struct vertex
{
  float x, y, w, h;
  float z;
  float tx, ty, tw, th;
  u32 color;
};

struct linear_ptr_list_entry
{
  void* ptr;
  struct linear_ptr_list_entry* next;
};

struct ctr_render_data
{
  C3D_Tex charset[5], charset_vram[5];
  struct v_char *cursor_map, *mouse_map;
  u64 charset_dirty;
  bool rendering_frame, checked_frame;
  struct ctr_shader_data shader_2d, shader_playfield;
  C3D_Mtx projection;
  C3D_Tex playfield_tex;
  C3D_TexEnv env_normal, env_no_texture, env_playfield, env_playfield_inv;
  C3D_RenderTarget *playfield, *target_top, *target_bottom;
  u8 cursor_on, mouse_on;
  u32 focus_x, focus_y;
  u32 layer_num;
};

C3D_Tex* ctr_load_png(const char *name);

void ctr_init_shader(struct ctr_shader_data *shader, const void* data, int size, int geo_count);
void ctr_bind_shader(struct ctr_shader_data *shader);

void ctr_draw_2d_texture(struct ctr_render_data *render_data, C3D_Tex* texture,
  int tx, int ty, int tw, int th,
  float x, float y, float w, float h, float z, u32 color, bool flipy);

#endif /* __3DS_RENDER_H__ */
