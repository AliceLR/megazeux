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

#include "graphics.h"
#include "render.h"
#include "renderers.h"
#include "util.h"

#include <3ds.h>
#include <citro3d.h>

#include "shader_shbin.h"
#include "shader_accel_shbin.h"

struct ctr_shader_data
{
  DVLB_s* dvlb;
  shaderProgram_s program;
  int proj_loc;
  C3D_AttrInfo attr;
};

struct ctr_render_data
{
  u32 *buffer;
  u8 *chartex_buffer;
  struct v_char *map;
  struct ctr_shader_data shader, shader_accel;
  C3D_Mtx projection;
  C3D_Tex texture, chartex;
  u8 chartex_dirty, cursor_on, mouse_on;
  C3D_RenderTarget *target, *target_bottom;
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
  u16 dud2;
  u32 col;
};

struct vertex
{
  vector_3f position;
  vector_2f texcoord;
};

void ctr_init_shader(struct ctr_shader_data *shader, const void* data, int size)
{
  shader->dvlb = DVLB_ParseFile((u32 *) data, size);
  shaderProgramInit(&shader->program);
  shaderProgramSetVsh(&shader->program, &shader->dvlb->DVLE[0]);
  shader->proj_loc = shaderInstanceGetUniformLocation(shader->program.vertexShader, "projection");  
  AttrInfo_Init(&shader->attr);
}

void ctr_bind_shader(struct ctr_shader_data *shader)
{
  C3D_BindProgram(&shader->program);
  C3D_SetAttrInfo(&shader->attr);
}

static bool ctr_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct ctr_render_data render_data;

  memset(&render_data, 0, sizeof(struct ctr_render_data));
  graphics->render_data = &render_data;
  render_data.buffer = linearAlloc(1024 * 512 * 4);
  render_data.chartex_buffer = linearAlloc(256 * 256 * 2);
  memset(render_data.chartex_buffer, 0xFFFFFFFF, 256 * 256 * 2);

  render_data.map = linearAlloc(((80 * 25 * 2) + 2) * 4 * sizeof(struct v_char));
  for (int i = 0; i < 80 * 50 + 2; i++)
  {
    render_data.map[i * 4 + 0].x = (i % 80);
    render_data.map[i * 4 + 0].y = ((i / 80) % 25);
    render_data.map[i * 4 + 0].u = 0;
    render_data.map[i * 4 + 0].v = 16;
    render_data.map[i * 4 + 1].x = (i % 80) + 1;
    render_data.map[i * 4 + 1].y = ((i / 80) % 25);
    render_data.map[i * 4 + 1].u = 1;
    render_data.map[i * 4 + 1].v = 16;
    render_data.map[i * 4 + 2].x = (i % 80);
    render_data.map[i * 4 + 2].y = ((i / 80) % 25) + 1;
    render_data.map[i * 4 + 2].u = 0;
    render_data.map[i * 4 + 2].v = 17;
    render_data.map[i * 4 + 3].x = (i % 80) + 1;
    render_data.map[i * 4 + 3].y = ((i / 80) % 25) + 1;
    render_data.map[i * 4 + 3].u = 1;
    render_data.map[i * 4 + 3].v = 17;
  }
  for (int i = 0; i < (80 * 50 + 2) * 4; i++)
  {
    render_data.map[i].z = (i < 80 * 100) ? 3 : ((i < 80 * 200) ? 2 : 1);
  }

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  gfxSet3D(false);

  render_data.target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetClear(render_data.target, C3D_CLEAR_ALL, 0x000000FF, 0);
  C3D_RenderTargetSetOutput(render_data.target, GFX_TOP, GFX_LEFT,
	GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)
	| GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

  render_data.target_bottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetClear(render_data.target_bottom, C3D_CLEAR_ALL, 0x000000FF, 0);
  C3D_RenderTargetSetOutput(render_data.target_bottom, GFX_BOTTOM, GFX_LEFT,
	GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)
	| GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

  ctr_init_shader(&render_data.shader, shader_shbin, shader_shbin_size);
  ctr_init_shader(&render_data.shader_accel, shader_accel_shbin, shader_accel_shbin_size);

  AttrInfo_AddLoader(&render_data.shader.attr, 0, GPU_FLOAT, 3); // v0 = position
  AttrInfo_AddLoader(&render_data.shader.attr, 1, GPU_FLOAT, 2); // v1 = texcoord

  AttrInfo_AddLoader(&render_data.shader_accel.attr, 0, GPU_FLOAT, 3); // v0 = position
  AttrInfo_AddLoader(&render_data.shader_accel.attr, 1, GPU_UNSIGNED_BYTE, 4); // v1 = texcoord
  AttrInfo_AddLoader(&render_data.shader_accel.attr, 2, GPU_UNSIGNED_BYTE, 4); // v2 = color

  C3D_TexInitVRAM(&render_data.texture, 1024, 512, GPU_RGBA8);
  C3D_TexSetFilter(&render_data.texture, GPU_LINEAR, GPU_LINEAR);

  C3D_TexInitVRAM(&render_data.chartex, 256, 256, GPU_RGBA4);
  C3D_TexSetFilter(&render_data.chartex, GPU_LINEAR, GPU_LINEAR);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
  C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 32;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  return set_video_mode();
}

static bool ctr_check_video_mode(struct graphics_data *graphics, int width,
  int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static bool ctr_set_video_mode(struct graphics_data *graphics, int width,
  int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static void ctr_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  Uint32 i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
      (palette[i].r << 24) | (palette[i].g << 16) | (palette[i].b << 8) | 0xFF;
  }
}

static char *ctr_char_bitmask_to_texture(signed char *c, short *p)
{
  // This tests the 7th bit, if 0, result is 0.
  // If 1, result is 255 (because of sign extended bit shift!).
  // Note the use of char constants to force 8bit calculation.
  *(p++) = (*c << 24 >> 31) * 0x101;
  *(p++) = (*c << 25 >> 31) * 0x101;
  *(p++) = (*c << 26 >> 31) * 0x101;
  *(p++) = (*c << 27 >> 31) * 0x101;
  *(p++) = (*c << 28 >> 31) * 0x101;
  *(p++) = (*c << 29 >> 31) * 0x101;
  *(p++) = (*c << 30 >> 31) * 0x101;
  *(p++) = (*c << 31 >> 31) * 0x101;
  return p;
}


static char *ctr_char_bitmask_to_texture_smzx_1(signed char *c, short *p)
{
  u8 alphas[] = {0, 10, 5, 15};
  u16 col;

  col = alphas[((*c & 0xC0) >> 6)] | 0xFFF0;
  *(p++) = col;
  *(p++) = col;

  col = alphas[((*c & 0x30) >> 4)] | 0xFFF0;
  *(p++) = col;
  *(p++) = col;

  col = alphas[((*c & 0x0C) >> 2)] | 0xFFF0;
  *(p++) = col;
  *(p++) = col;

  col = alphas[(*c & 0x03)] | 0xFFF0;
  *(p++) = col;
  *(p++) = col;
  return p;
}

static inline void ctr_do_remap_charsets(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->chartex_buffer;
  unsigned int i, j, k;

  switch(graphics->screen_mode)
  {
    case 1:
      for(i = 0; i < 16; i++, c += -14 + 32 * 14)
        for(j = 0; j < 14; j++, c += -32 * 14 + 1)
          for(k = 0; k < 32; k++, c += 14)
            p = ctr_char_bitmask_to_texture_smzx_1(c, p);
      break;
    default:
      for(i = 0; i < 16; i++, c += -14 + 32 * 14)
        for(j = 0; j < 14; j++, c += -32 * 14 + 1)
          for(k = 0; k < 32; k++, c += 14)
            p = ctr_char_bitmask_to_texture(c, p);
      break;
  }

  render_data->chartex_dirty = 1;
}

static inline void ctr_do_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  struct ctr_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->chartex_buffer;
  unsigned int i;

  p += (((chr & 0x1F) << 3) + ((chr & 0xE0) * 112)) * 2;
  c += chr * 14;

  switch(graphics->screen_mode)
  {
    case 1:
      for(i = 0; i < 14; i++, c++)
        p = ctr_char_bitmask_to_texture_smzx_1(c, p);
      break;
    default:
      for(i = 0; i < 14; i++, c++)
        p = ctr_char_bitmask_to_texture(c, p);
      break;
  }

  render_data->chartex_dirty = 1;
}

static void ctr_do_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
  // FIXME: optimize
  ctr_do_remap_char(graphics, chr);
}

static void ctr_render_graph(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data = graphics->render_data;
  struct char_element *src = graphics->text_video;

  Uint32 *pixels = render_data->buffer;
  Uint32 pitch = 1024 * 4;
  Uint32 mode = graphics->screen_mode;
  int i, j, k, l, col, col2;
  u8 u, v;

  if(mode < 2)
  {
    k = 0;
    l = 80 * 25 * 4;
    src = &graphics->text_video[80 * 24];
    for (j = 0; j < 25; j++, src -= 160)
      for (i = 0; i < 80; i++, src++)
      {
        u = src->char_value & 31;
        v = src->char_value >> 5;
        col = graphics->flat_intensity_palette[src->bg_color];
        col2 = graphics->flat_intensity_palette[src->fg_color];
        render_data->map[k++].col = col;
        render_data->map[k++].col = col;
        render_data->map[k++].col = col;
        render_data->map[k++].col = col;
        render_data->map[l].u = u;
        render_data->map[l].v = v+1;
        render_data->map[l++].col = col2;
        render_data->map[l].u = u+1;
        render_data->map[l].v = v+1;
        render_data->map[l++].col = col2;
        render_data->map[l].u = u;
        render_data->map[l].v = v;
        render_data->map[l++].col = col2;
        render_data->map[l].u = u+1;
        render_data->map[l].v = v;
        render_data->map[l++].col = col2;
      }
  }
  else
    render_graph32s(pixels, pitch, graphics, set_colors32[mode]);
}

static void ctr_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct ctr_render_data *render_data = graphics->render_data;
  Uint32 flatcolor = graphics->flat_intensity_palette[color];
  int ind = 80 * 25 * 8;
  float y1 = 25 - (y + ((lines + offset) / 14.0f));
  float y2 = 25 - (y + (offset / 14.0f));

  render_data->cursor_on = 1;
  render_data->map[ind].x = x;
  render_data->map[ind].y = y1;
  render_data->map[ind++].col = flatcolor;
  render_data->map[ind].x = x + 1;
  render_data->map[ind].y = y1;
  render_data->map[ind++].col = flatcolor;
  render_data->map[ind].x = x;
  render_data->map[ind].y = y2;
  render_data->map[ind++].col = flatcolor;
  render_data->map[ind].x = x + 1;
  render_data->map[ind].y = y2;
  render_data->map[ind++].col = flatcolor;
}

static void ctr_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct ctr_render_data *render_data = graphics->render_data;
  Uint32 col = 0xFFFFFF00;
  int offset = 80 * 25 * 8 + 4;
  float y1 = 25 - ((y + h) / 14.0f);
  float y2 = 25 - (y / 14.0f);

  render_data->mouse_on = 1;
  render_data->map[offset].x = x / 8.0f;
  render_data->map[offset].y = y1;
  render_data->map[offset++].col = col;
  render_data->map[offset].x = (x + w) / 8.0f;
  render_data->map[offset].y = y1;
  render_data->map[offset++].col = col;
  render_data->map[offset].x = x / 8.0f;
  render_data->map[offset].y = y2;
  render_data->map[offset++].col = col;
  render_data->map[offset].x = (x + w) / 8.0f;
  render_data->map[offset].y = y2;
  render_data->map[offset++].col = col;
}

static void ctr_calc_projection(struct ctr_render_data *render_data, bool top_screen)
{
  float w1, h1, w2, h2;
  float umin, vmin;
  float umax, vmax;
  float slider = osGet3DSliderState() * 1.15f;

  if(slider < 0.0f)
    slider = 0.0f;
  if(slider > 1.0f)
    slider = 1.0f;

  if(top_screen)
  {
    if(render_data->focus_x < 200) render_data->focus_x = 200;
    if(render_data->focus_x > 440) render_data->focus_x = 440;
    if(render_data->focus_y < 120) render_data->focus_y = 120;
    if(render_data->focus_y > 230) render_data->focus_y = 230;

    umin = render_data->focus_x - 200;
    umax = render_data->focus_x + 200;
    vmin = 350 - (render_data->focus_y + 120);
    vmax = 350 - (render_data->focus_y - 120);
  }
  else
  {
    umin = 0;
    umax = 640;
    vmin = -64;
    vmax = 350+66;
  }

  Mtx_OrthoTilt(&render_data->projection, umin, umax, vmin, vmax, 0.0, 4.0);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader.proj_loc, &render_data->projection);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader_accel.proj_loc, &render_data->projection);
}

static void ctr_draw_playfield(struct graphics_data *graphics, struct ctr_render_data *render_data)
{
  struct vertex vertices[4];
  void* vbo_data;

  if(graphics->screen_mode < 2)
  {
    ctr_bind_shader(&render_data->shader_accel);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader_accel.proj_loc, &render_data->projection);

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, render_data->map, sizeof(struct v_char), 3, 0x210);

    C3D_TexBind(0, &render_data->chartex);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 80 * 25 * 8);
  }
  else
  {
    ctr_bind_shader(&render_data->shader);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader.proj_loc, &render_data->projection);

    vertices[0].position = (vector_3f){0.0f, 0.0f, 2.0f};
    vertices[1].position = (vector_3f){640.0f, 0.0f, 2.0f};
    vertices[2].position = (vector_3f){0.0f, 350.0f, 2.0f};
    vertices[3].position = (vector_3f){640.0f, 350.0f, 2.0f};

    float umin = 0;
    float vmin = 0;
    float umax = 640 / 1024.0f;
    float vmax = 350 / 512.0f;

    vertices[0].texcoord = (vector_2f){umin, vmax};
    vertices[1].texcoord = (vector_2f){umax, vmax};
    vertices[2].texcoord = (vector_2f){umin, vmin};
    vertices[3].texcoord = (vector_2f){umax, vmin};

    vbo_data = linearAlloc(sizeof(struct vertex) * 4);
    memcpy(vbo_data, vertices, sizeof(struct vertex) * 4);

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, vbo_data, sizeof(struct vertex), 2, 0x10);

    C3D_TexBind(0, &render_data->texture);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 4);

    linearFree(vbo_data);
  }

  ctr_bind_shader(&render_data->shader_accel);

  if (render_data->cursor_on)
  {
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 80 * 25 * 8, 4);
  }

  if (render_data->mouse_on)
  {
    C3D_AlphaBlend(GPU_BLEND_SUBTRACT, GPU_BLEND_ADD, GPU_SRC_COLOR, GPU_DST_COLOR, GPU_SRC_ALPHA, GPU_DST_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 80 * 25 * 8 + 4, 4);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
  }
}

static void ctr_sync_screen(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data = graphics->render_data;

  if(graphics->screen_mode < 2)
  {
    if(render_data->chartex_dirty)
    {
      GSPGPU_FlushDataCache(render_data->chartex_buffer, 256 * 256 * 2);
      C3D_SafeDisplayTransfer((u32 *) render_data->chartex_buffer, GX_BUFFER_DIM(256, 256),
        (u32 *) render_data->chartex.data, GX_BUFFER_DIM(256, 256),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GPU_RGBA4) | GX_TRANSFER_OUT_FORMAT(GPU_RGBA4)
        | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
      gspWaitForPPF();
      GSPGPU_InvalidateDataCache(render_data->chartex.data, 256 * 256 * 2);

      render_data->chartex_dirty = false;
    }
  }
  else
  {
    GSPGPU_FlushDataCache(render_data->buffer, 1024 * 512 * 4);
    C3D_SafeDisplayTransfer((u32 *) render_data->buffer, GX_BUFFER_DIM(1024, 512),
      (u32 *) render_data->texture.data, GX_BUFFER_DIM(1024, 512),
      GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
      GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)
      | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    gspWaitForPPF();
    GSPGPU_InvalidateDataCache(render_data->texture.data, 1024 * 512 * 4);
  }

  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

  ctr_calc_projection(render_data, true);
  C3D_FrameDrawOn(render_data->target);

  ctr_draw_playfield(graphics, render_data);

  ctr_calc_projection(render_data, false);
  C3D_FrameDrawOn(render_data->target_bottom);

  ctr_draw_playfield(graphics, render_data);

  render_data->cursor_on = 0;
  render_data->mouse_on = 0;

  C3D_FrameEnd(0);
}

void ctr_focus_pixel(struct graphics_data *graphics, Uint32 x, Uint32 y)
{
  struct ctr_render_data *render_data = graphics->render_data;
  render_data->focus_x = x;
  render_data->focus_y = y;
}

void render_ctr_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = ctr_init_video;
  renderer->check_video_mode = ctr_check_video_mode;
  renderer->set_video_mode = ctr_set_video_mode;
  renderer->update_colors = ctr_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_charsets = ctr_do_remap_charsets;
  renderer->remap_char = ctr_do_remap_char;
  renderer->remap_charbyte = ctr_do_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = ctr_render_graph;
  renderer->render_cursor = ctr_render_cursor;
  renderer->render_mouse = ctr_render_mouse;
  renderer->sync_screen = ctr_sync_screen;
  renderer->focus_pixel = ctr_focus_pixel;
}
