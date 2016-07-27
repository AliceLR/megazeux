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

#define ENABLE_ACCEL

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
  struct ctr_shader_data shader;
  struct ctr_shader_data shader_accel;
  C3D_Mtx projection;
  C3D_Tex texture, chartex;
  u8 chartex_dirty;
  C3D_RenderTarget* target;
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

  render_data.map = linearAlloc(80 * 25 * 8 * sizeof(struct v_char));
  for (int i = 0; i < 80 * 50; i++)
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
  for (int i = 0; i < 80 * 200; i++)
  {
    render_data.map[i].z = (i < 80 * 100) ? 2 : 1;
//    render_data.map[i].w = 0;
  }

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  gfxSet3D(false);

  render_data.target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetClear(render_data.target, C3D_CLEAR_ALL, 0x338833FF, 0);
  C3D_RenderTargetSetOutput(render_data.target, GFX_TOP, GFX_LEFT,
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

  Mtx_OrthoTilt(&render_data.projection, 0.0, 640.0, 0.0, 350.0, 0.0, 3.0);

  C3D_TexInitVRAM(&render_data.texture, 1024, 512, GPU_RGBA8);
  C3D_TexSetFilter(&render_data.texture, GPU_NEAREST, GPU_NEAREST);

  C3D_TexInit(&render_data.chartex, 256, 256, GPU_RGBA4);
  C3D_TexSetFilter(&render_data.chartex, GPU_NEAREST, GPU_NEAREST);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
  C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 32;

  if(graphics->resolution_width < 640)
    graphics->resolution_width = 640;
  if(graphics->resolution_height < 350)
    graphics->resolution_height = 350;
  if(graphics->window_width < 640)
    graphics->window_width = 640;
  if(graphics->window_height < 350)
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

static inline void ctr_do_remap_charsets(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->chartex_buffer;
  unsigned int i, j, k;

  for(i = 0; i < 16; i++, c += -14 + 32 * 14)
    for(j = 0; j < 14; j++, c += -32 * 14 + 1)
      for(k = 0; k < 32; k++, c += 14)
        p = ctr_char_bitmask_to_texture(c, p);

  render_data->chartex_dirty = 1;
}

static inline void ctr_do_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  struct ctr_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->chartex_buffer;
  unsigned int i;

  p += ((chr & 0x1F) << 3) + ((chr & 0xE0) * 112);
  c += chr * 14;

  for(i = 0; i < 14; i++, c++)
    p = ctr_char_bitmask_to_texture(c, p);

  render_data->chartex_dirty = 1;
}

static void ctr_do_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
/*   struct ctr_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->chartex_buffer;

  p += ((chr & 0x1F) << 3) + ((chr & 0xE0) * 112) + (byte * 256);
  c += chr * 14 + byte;

  p = ctr_char_bitmask_to_texture(c, p);

  render_data->chartex_dirty = 1; */
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

  if(!mode)
  {
#ifdef ENABLE_ACCEL
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
#else
    render_graph32(pixels, pitch, graphics, set_colors32[mode]);
#endif
  }
  else
    render_graph32s(pixels, pitch, graphics, set_colors32[mode]);
}

static void ctr_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct ctr_render_data *render_data = graphics->render_data;

  Uint32 *pixels = render_data->buffer;
  Uint32 pitch = 1024 * 4;
  Uint32 bpp = 32;
  Uint32 flatcolor = graphics->flat_intensity_palette[color];

  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
}

static void ctr_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct ctr_render_data *render_data = graphics->render_data;

  Uint32 *pixels = render_data->buffer;
  Uint32 pitch = 1024 * 4;
  Uint32 bpp = 32;
  Uint32 mask = 0xFFFFFFFF;
  Uint32 amask = 0x000000FF;

  render_mouse(pixels, pitch, bpp, x, y, mask, amask, w, h);
}

static void ctr_sync_screen(struct graphics_data *graphics)
{
  struct vertex vertices[4];
  struct ctr_render_data *render_data = graphics->render_data;
  float xp, yp;
  float umin, vmin;
  float umax, vmax;
  float slider = osGet3DSliderState() * 1.15f;
  void* vbo_data;

  if(slider < 0.0f)
    slider = 0.0f;
  if(slider > 1.0f)
    slider = 1.0f;

#ifdef ENABLE_ACCEL
  if(!graphics->screen_mode)
#else
  if(0)
#endif
  {
    ctr_bind_shader(&render_data->shader_accel);
    //ctr_bind_shader(&render_data->shader);

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

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameDrawOn(render_data->target);

    if(0) // CHARTEX DEBUG
    {
      C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader.proj_loc, &render_data->projection);

      vertices[0].position = (vector_3f){0.0f, 0.0f, 0.5f};
      vertices[1].position = (vector_3f){640.0f, 0.0f, 0.5f};
      vertices[2].position = (vector_3f){0.0f, 350.0f, 0.5f};
      vertices[3].position = (vector_3f){640.0f, 350.0f, 0.5f};

      umin = vmin = 0;
      umax = 1;
      vmax = 1;

      vertices[0].texcoord = (vector_2f){umin, vmax};
      vertices[1].texcoord = (vector_2f){umax, vmax};
      vertices[2].texcoord = (vector_2f){umin, vmin};
      vertices[3].texcoord = (vector_2f){umax, vmin};

      vbo_data = linearAlloc(sizeof(struct vertex) * 4);
      memcpy(vbo_data, vertices, sizeof(struct vertex) * 4);

      C3D_BufInfo* bufInfo = C3D_GetBufInfo();
      BufInfo_Init(bufInfo);
      BufInfo_Add(bufInfo, vbo_data, sizeof(struct vertex), 2, 0x10);

      C3D_TexBind(0, &render_data->chartex);
      C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 4);
    }
    else
    {
      C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader_accel.proj_loc, &render_data->projection);

      C3D_BufInfo* bufInfo = C3D_GetBufInfo();
      BufInfo_Init(bufInfo);
      BufInfo_Add(bufInfo, render_data->map, sizeof(struct v_char), 3, 0x210);

      C3D_TexBind(0, &render_data->chartex);
      C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 80 * 25 * 8);
    }

    C3D_FrameEnd(0);
  }
  else
  {
    ctr_bind_shader(&render_data->shader);

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameDrawOn(render_data->target);

    GSPGPU_FlushDataCache(render_data->buffer, 1024 * 512 * 4);
    C3D_SafeDisplayTransfer((u32 *) render_data->buffer, GX_BUFFER_DIM(1024, 512),
      (u32 *) render_data->texture.data, GX_BUFFER_DIM(1024, 512),
      GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
      GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)
      | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    gspWaitForPPF();
    GSPGPU_InvalidateDataCache(render_data->texture.data, 1024 * 512 * 4);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader.proj_loc, &render_data->projection);

    vertices[0].position = (vector_3f){0.0f, 0.0f, 0.5f};
    vertices[1].position = (vector_3f){640.0f, 0.0f, 0.5f};
    vertices[2].position = (vector_3f){0.0f, 350.0f, 0.5f};
    vertices[3].position = (vector_3f){640.0f, 350.0f, 0.5f};

    umin = vmin = 0;
    umax = 640 / 1024.0f;
    vmax = 350 / 512.0f;

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

    C3D_FrameEnd(0);

    linearFree(vbo_data);
  }
}

void render_ctr_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = ctr_init_video;
  renderer->check_video_mode = ctr_check_video_mode;
  renderer->set_video_mode = ctr_set_video_mode;
  renderer->update_colors = ctr_update_colors;
  renderer->resize_screen = resize_screen_standard;
#ifdef ENABLE_ACCEL
  renderer->remap_charsets = ctr_do_remap_charsets;
  renderer->remap_char = ctr_do_remap_char;
  renderer->remap_charbyte = ctr_do_remap_charbyte;
#endif
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = ctr_render_graph;
  renderer->render_cursor = ctr_render_cursor;
  renderer->render_mouse = ctr_render_mouse;
  renderer->sync_screen = ctr_sync_screen;
}
