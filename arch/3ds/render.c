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

struct ctr_render_data
{
  Uint32* buffer;
  DVLB_s* dvlb;
  shaderProgram_s shader;
  int projection_loc;
  C3D_Mtx projection;
  C3D_Tex texture;
  C3D_RenderTarget* target;
};

typedef struct {
  float u, v;
} vector_2f;

typedef struct {
  float x, y, z;
} vector_3f;

struct vertex
{
  vector_3f position;
  vector_2f texcoord;
};

static bool ctr_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct ctr_render_data render_data;

  memset(&render_data, 0, sizeof(struct ctr_render_data));
  graphics->render_data = &render_data;
  render_data.buffer = linearAlloc(1024 * 512 * 4);

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  gfxSet3D(false);

  render_data.target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetClear(render_data.target, C3D_CLEAR_ALL, 0x00FF00FF, 0);
  C3D_RenderTargetSetOutput(render_data.target, GFX_TOP, GFX_LEFT,
	GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)
	| GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

  render_data.dvlb = DVLB_ParseFile((u32 *)shader_shbin, shader_shbin_size);
  shaderProgramInit(&render_data.shader);
  shaderProgramSetVsh(&render_data.shader, &render_data.dvlb->DVLE[0]);
  C3D_BindProgram(&render_data.shader);
  render_data.projection_loc = shaderInstanceGetUniformLocation(render_data.shader.vertexShader, "projection");

  C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
  AttrInfo_Init(attrInfo);
  AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0 = position
  AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1 = texcoord

  Mtx_OrthoTilt(&render_data.projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0);

  C3D_TexInit(&render_data.texture, 1024, 512, GPU_RGBA8);
  C3D_TexSetFilter(&render_data.texture, GPU_LINEAR, GPU_LINEAR);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
  C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

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

static void ctr_render_graph(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data = graphics->render_data;

  Uint32 *pixels = render_data->buffer;
  Uint32 pitch = 1024 * 4;
  Uint32 mode = graphics->screen_mode;

  if(!mode)
    render_graph32(pixels, pitch, graphics, set_colors32[mode]);
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

  GSPGPU_FlushDataCache(render_data->buffer, 1024 * 512 * 4);
  C3D_SafeDisplayTransfer((u32 *) render_data->buffer, GX_BUFFER_DIM(1024, 512),
	(u32 *) render_data->texture.data, GX_BUFFER_DIM(1024, 512),
	GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)
	| GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
  gspWaitForPPF();
  GSPGPU_InvalidateDataCache(render_data->texture.data, 1024 * 512 * 4);

  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
  C3D_FrameDrawOn(render_data->target);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->projection_loc, &render_data->projection);

  xp = (slider * 120.0);
  yp = (slider * 65.0);

  umin = xp / 1024.0f;
  vmin = yp / 512.0f;
  umax = (640.0f - xp) / 1024.0f;
  vmax = (350.0f - yp) / 512.0f;

  vertices[0].position = (vector_3f){0.0f, 0.0f, 0.5f};
  vertices[1].position = (vector_3f){400.0f, 0.0f, 0.5f};
  vertices[2].position = (vector_3f){0.0f, 240.0f, 0.5f};
  vertices[3].position = (vector_3f){400.0f, 240.0f, 0.5f};

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

void render_ctr_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = ctr_init_video;
  renderer->check_video_mode = ctr_check_video_mode;
  renderer->set_video_mode = ctr_set_video_mode;
  renderer->update_colors = ctr_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = ctr_render_graph;
  renderer->render_cursor = ctr_render_cursor;
  renderer->render_mouse = ctr_render_mouse;
  renderer->sync_screen = ctr_sync_screen;
}
