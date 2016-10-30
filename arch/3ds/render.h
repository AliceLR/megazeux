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

#define MAP_QUADS ((80 * 25 * 4) + 2)
#define MAP_CHAR_SIZE ((MAP_QUADS - 2) * 4)

struct ctr_shader_data
{
  DVLB_s* dvlb;
  shaderProgram_s program;
  int proj_loc;
  C3D_AttrInfo attr;
};

struct ctr_charset_data
{
  C3D_Tex texture;
  short *buffer;
};

struct ctr_render_data
{
  u32 *buffer;
  struct ctr_charset_data charset[3];
  bool charset_dirty;
  unsigned int last_smzx_mode;
  struct v_char *map;
  struct ctr_shader_data shader, shader_accel;
  C3D_Mtx projection;
  C3D_RenderBuf playfield, target_top, target_bottom;
  u8 cursor_on, mouse_on;
  u32 focus_x, focus_y;
};

typedef struct {
  float u, v;
} vector_2f;

typedef struct {
  float x, y, z;
} vector_3f;

struct v_char
{
  float x, y, z;
  u8 u, v;
  u16 dud;
  u32 col;
};

struct vertex
{
  vector_3f position;
  vector_2f texcoord;
};

struct linear_ptr_list_entry
{
  void* ptr;
  struct linear_ptr_list_entry* next;
};

C3D_Tex* ctr_load_png(const char *name);

void ctr_init_shader(struct ctr_shader_data *shader, const void* data, int size);
void ctr_bind_shader(struct ctr_shader_data *shader);

void ctr_draw_2d_texture(struct ctr_render_data *render_data, C3D_Tex* texture,
  int tx, int ty, int tw, int th,
  float x, float y, float w, float h, float z);

#endif /* __3DS_RENDER_H__ */
