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

#include "../../src/graphics.h"
#include "../../src/pngops.h"
#include "../../src/render.h"
#include "../../src/renderers.h"
#include "../../src/util.h"

#include "event.h"
#include "keyboard.h"
#include "platform.h"
#include "render.h"

#include "shader_shbin.h"
#include "shader_accel_shbin.h"

#define RDR_DEBUG

static u8 morton_lut[64] = {
        0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15, 0x02, 0x03, 0x06, 0x07, 0x12, 0x13, 0x16, 0x17,
        0x08, 0x09, 0x0c, 0x0d, 0x18, 0x19, 0x1c, 0x1d, 0x0a, 0x0b, 0x0e, 0x0f, 0x1a, 0x1b, 0x1e, 0x1f,
        0x20, 0x21, 0x24, 0x25, 0x30, 0x31, 0x34, 0x35, 0x22, 0x23, 0x26, 0x27, 0x32, 0x33, 0x36, 0x37,
        0x28, 0x29, 0x2c, 0x2d, 0x38, 0x39, 0x3c, 0x3d, 0x2a, 0x2b, 0x2e, 0x2f, 0x3a, 0x3b, 0x3e, 0x3f
};

// texture PNG dimensions must be powers of two
static bool tex_w_h_constraint(png_uint_32 w, png_uint_32 h)
{
  return w > 0 && h > 0 && ((w & (w - 1)) == 0) && ((h & (h - 1)) == 0);
}

static inline int next_power_of_two(int v)
{
  for (int i = 1; i <= 65536; i *= 2)
    if (i >= v) return i;
  return v;
}

static void* tex_alloc_png_surface(png_uint_32 w, png_uint_32 h,
  png_uint_32 *stride, void **pixels)
{
  C3D_Tex* tex;

  tex = ccalloc(1, sizeof(C3D_Tex));
  *stride = w << 2;

  if (!C3D_TexInit(tex, w, h, GPU_RGBA8))
  {
    free(tex);
    return NULL;
  }

  *pixels = tex->data;
  return tex;
}

static inline void ctr_set_2d_projection(struct ctr_render_data *render_data, int width, int height, bool tilt)
{
  if (tilt)
    Mtx_OrthoTilt(&render_data->projection, 0, width, height, 0, -1.0, 12100.0, true);
  else
    Mtx_Ortho(&render_data->projection, 0, width, 0, height, -1.0, 12100.0, true);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader.proj_loc, &render_data->projection);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader_accel.proj_loc, &render_data->projection);
}

static inline void ctr_set_2d_projection_screen(struct ctr_render_data *render_data, bool top_screen)
{
  ctr_set_2d_projection(render_data, top_screen ? 400 : 320, 240, true);
}

static inline void ctr_prepare_accel(struct ctr_render_data *render_data, struct v_char *array, float xo, float yo)
{
  C3D_BufInfo *bufInfo;

  bufInfo = C3D_GetBufInfo();
  BufInfo_Init(bufInfo);
  BufInfo_Add(bufInfo, array, sizeof(struct v_char), 3, 0x210);

  ctr_bind_shader(&render_data->shader_accel);

  C3D_FVUnifSet(GPU_VERTEX_SHADER, render_data->shader_accel.offs_loc, (float) xo, (float) yo, 0.0f, 0.0f);
}

C3D_Tex* ctr_load_png(const char* name)
{
  C3D_Tex* output;
  u32 *data, *dataBuf;

  output = png_read_file(name, NULL, NULL, tex_w_h_constraint,
    tex_alloc_png_surface);

  if (output != NULL)
  {
    C3D_TexSetWrap(output, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    data = (u32*) output->data;
    dataBuf = (u32*) clinearAlloc(output->size);

    for (int i = 0; i < output->size >> 2; i++)
      dataBuf[i] = __builtin_bswap32(data[i]);

    GSPGPU_FlushDataCache(dataBuf, output->size);
    C3D_SafeDisplayTransfer(dataBuf, GX_BUFFER_DIM(output->width, output->height),
      data, GX_BUFFER_DIM(output->width, output->height),
      GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
      GX_TRANSFER_IN_FORMAT(GPU_RGBA8) | GX_TRANSFER_OUT_FORMAT(GPU_RGBA8)
      | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    gspWaitForPPF();
    linearFree(dataBuf);
  }

  return output;
}

void ctr_bind_shader(struct ctr_shader_data *shader)
{
  C3D_BindProgram(&shader->program);
  C3D_SetAttrInfo(&shader->attr);
}

static int vertex_heap_pos = 0, vertex_heap_len = 0;
static struct vertex* vertex_heap;

void ctr_draw_2d_texture(struct ctr_render_data *render_data, C3D_Tex* texture,
  int tx, int ty, int tw, int th,
  float x, float y, float w, float h, float z, bool flipy)
{
  struct vertex* vertices;
  float umin, umax, vmin, vmax;
  C3D_BufInfo* bufInfo;

  if (flipy)
  {
    ty = texture->height - ty;
    th = -th;
  }

  if (vertex_heap_len == 0)
  {
    vertex_heap_len = 256;
    vertex_heap = clinearAlloc(sizeof(struct vertex) * vertex_heap_len);
  }
  else if (vertex_heap_pos + 4 > vertex_heap_len)
  {
    vertex_heap_len *= 2;
    vertex_heap = clinearRealloc(vertex_heap, sizeof(struct vertex) * vertex_heap_len);
  }

  vertices = &vertex_heap[vertex_heap_pos];
  vertex_heap_pos += 4;

  ctr_bind_shader(&render_data->shader);

  vertices[0].position = (vector_3f){x, y+h, z};
  vertices[1].position = (vector_3f){x+w, y+h, z};
  vertices[2].position = (vector_3f){x, y, z};
  vertices[3].position = (vector_3f){x+w, y, z};

  umin = ((float) (tx)) / ((float) texture->width);
  umax = ((float) (tx + tw)) / ((float) texture->width);
  vmin = ((float) (ty)) / ((float) texture->height);
  vmax = ((float) (ty + th)) / ((float) texture->height);

  vertices[0].texcoord = (vector_2f){umin, vmin};
  vertices[1].texcoord = (vector_2f){umax, vmin};
  vertices[2].texcoord = (vector_2f){umin, vmax};
  vertices[3].texcoord = (vector_2f){umax, vmax};

  bufInfo = C3D_GetBufInfo();
  BufInfo_Init(bufInfo);
  BufInfo_Add(bufInfo, vertices, sizeof(struct vertex), 2, 0x10);

  C3D_TexBind(0, texture);
  C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 4);
}

void ctr_init_shader(struct ctr_shader_data *shader, const void* data, int size)
{
  shader->dvlb = DVLB_ParseFile((u32 *) data, size);
  shaderProgramInit(&shader->program);
  shaderProgramSetVsh(&shader->program, &shader->dvlb->DVLE[0]);
  shader->proj_loc = shaderInstanceGetUniformLocation(shader->program.vertexShader, "projection");
  shader->offs_loc = shaderInstanceGetUniformLocation(shader->program.vertexShader, "offset");
  AttrInfo_Init(&shader->attr);
}

static bool ctr_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct ctr_render_data render_data;
  C3D_TexEnv* texEnv;

#ifdef RDR_DEBUG
  consoleInit(GFX_TOP, NULL);
#endif

  memset(&render_data, 0, sizeof(struct ctr_render_data));
  graphics->render_data = &render_data;
  render_data.cursor_map = clinearAlloc(sizeof(struct v_char) * 4);
  render_data.mouse_map = clinearAlloc(sizeof(struct v_char) * 4);

  for (int i = 0; i < 4; i++)
  {
    render_data.cursor_map[i].z = 1;
    render_data.cursor_map[i].u = 0;
    render_data.cursor_map[i].v = (NUM_CHARSETS-1)*32;
    render_data.mouse_map[i].z = 1;
    render_data.mouse_map[i].u = 0;
    render_data.mouse_map[i].v = (NUM_CHARSETS-1)*32;
  }

  C3D_Init(0x100000);
  gfxSet3D(false);

  render_data.rendering_frame = false;

  C3D_TexInit(&render_data.playfield_tex, 1024, 512, GPU_RGB8);
  C3D_TexSetFilter(&render_data.playfield_tex, GPU_LINEAR, GPU_LINEAR);

  render_data.playfield = C3D_RenderTargetCreateFromTex(&render_data.playfield_tex, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);
  render_data.target_top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGB8, GPU_RB_DEPTH16);
  render_data.target_bottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGB8, GPU_RB_DEPTH16);

  C3D_RenderTargetSetClear(render_data.playfield, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_RenderTargetSetClear(render_data.target_top, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_RenderTargetSetClear(render_data.target_bottom, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_RenderTargetSetOutput(render_data.target_top, GFX_TOP, GFX_LEFT,
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
  C3D_RenderTargetSetOutput(render_data.target_bottom, GFX_BOTTOM, GFX_LEFT,
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

  ctr_init_shader(&render_data.shader, shader_shbin, shader_shbin_size);
  ctr_init_shader(&render_data.shader_accel, shader_accel_shbin, shader_accel_shbin_size);

  AttrInfo_AddLoader(&render_data.shader.attr, 0, GPU_FLOAT, 3); // v0 = position
  AttrInfo_AddLoader(&render_data.shader.attr, 1, GPU_FLOAT, 2); // v1 = texcoord

  AttrInfo_AddLoader(&render_data.shader_accel.attr, 0, GPU_FLOAT, 3); // v0 = position
  AttrInfo_AddLoader(&render_data.shader_accel.attr, 1, GPU_SHORT, 2); // v1 = texcoord
  AttrInfo_AddLoader(&render_data.shader_accel.attr, 2, GPU_UNSIGNED_BYTE, 4); // v2 = color

  for (int i = 0; i < 4; i++)
  {
    C3D_TexInit(&render_data.charset[i], 1024, NUM_CHARSETS * 32, GPU_LA4);
    C3D_TexSetFilter(&render_data.charset[i], GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&render_data.charset[i], GPU_REPEAT, GPU_REPEAT);

    C3D_TexInitVRAM(&render_data.charset_vram[i], 1024, NUM_CHARSETS * 32, GPU_LA4);
    C3D_TexSetFilter(&render_data.charset_vram[i], GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&render_data.charset_vram[i], GPU_REPEAT, GPU_REPEAT);
  }

  texEnv = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(texEnv, C3D_Both, GPU_TEXTURE0, 0, 0);
  C3D_TexEnvOp(texEnv, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(texEnv, C3D_Both, GPU_MODULATE);

  C3D_AlphaTest(true, GPU_GREATER, 0x80);
  C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 32;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  ctr_keyboard_init(&render_data);

  return set_video_mode();
}

static void ctr_free_video(struct graphics_data *graphics)
{
  // TODO: more freeing!
  struct ctr_render_data *render_data = graphics->render_data;
  for (int i = 0; i < 4; i++)
  {
    C3D_TexDelete(&render_data->charset[i]);
    C3D_TexDelete(&render_data->charset_vram[i]);
  }
  linearFree(render_data->cursor_map);
  linearFree(render_data->mouse_map);
  C3D_RenderTargetDelete(render_data->playfield);
  C3D_RenderTargetDelete(render_data->target_top);
  C3D_RenderTargetDelete(render_data->target_bottom);
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
      (palette[i].r << 24) | (palette[i].g << 16) | (palette[i].b << 8) | 0x000000FF;
  }
}

static inline void ctr_char_bitmask_to_texture(struct ctr_render_data *render_data, signed char c, u32 offset, int y)
{
  int y_lut = (y & 7) << 3;
  u8 *p, *p2, *p3;
  u8 t, i;

  offset += ((y & 8) << 10);
  p = ((u8*) render_data->charset[0].data) + offset;

  // SMZX MODE 0
  p[morton_lut[y_lut++]] = (c << 24 >> 31);
  p[morton_lut[y_lut++]] = (c << 25 >> 31);
  p[morton_lut[y_lut++]] = (c << 26 >> 31);
  p[morton_lut[y_lut++]] = (c << 27 >> 31);
  p[morton_lut[y_lut++]] = (c << 28 >> 31);
  p[morton_lut[y_lut++]] = (c << 29 >> 31);
  p[morton_lut[y_lut++]] = (c << 30 >> 31);
  p[morton_lut[y_lut++]] = (c << 31 >> 31);
  y_lut -= 8;

  // SMZX MODE 1+ (oh dear)
  p = ((u8*) render_data->charset[1].data) + offset;
  p2 = ((u8*) render_data->charset[2].data) + offset;
  p3 = ((u8*) render_data->charset[3].data) + offset;

  for (i = 0; i < 4; i++)
  {
    t = (c >> (6 - i*2)) & 3;
    *((u8*) (p + morton_lut[y_lut])) = (t == 1) ? 0xFF : 0x00;
    *((u8*) (p2 + morton_lut[y_lut])) = (t == 2) ? 0xFF : 0x00;
    *((u8*) (p3 + morton_lut[y_lut])) = (t == 3) ? 0xFF : 0x00;
    y_lut++;
  }
}

static inline void ctr_do_remap_charsets(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data;
  signed char *c;
  unsigned int i, j;

  render_data = graphics->render_data;
  c = (signed char *)graphics->charset;

  for(i = 0; i < FULL_CHARSET_SIZE; i++)
    for(j = 0; j < 14; j++, c++)
      ctr_char_bitmask_to_texture(render_data, *c, ((i & 127)*64) + ((i >> 7)*16384), j);

  render_data->charset_dirty = ((u64) 1 << (NUM_CHARSETS * 2)) - 1;
}

static inline void ctr_do_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  struct ctr_render_data *render_data;
  signed char *c;
  unsigned int i, offset;

  render_data = graphics->render_data;
  c = (signed char *)graphics->charset;
  c += chr * 14;
  offset = ((chr & 127)*64) + ((chr >> 7)*16384);

  for(i = 0; i < 14; i++, c++)
    ctr_char_bitmask_to_texture(render_data, *c, offset, i);

  render_data->charset_dirty |= (1 << (chr >> 7));
}

static void ctr_do_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
  struct ctr_render_data *render_data;
  signed char *c;

  render_data = graphics->render_data;
  c = (signed char *)graphics->charset;
  c += chr * 14 + byte;

  ctr_char_bitmask_to_texture(render_data, *c, ((chr & 127)*64) + ((chr >> 7)*16384), byte);

  render_data->charset_dirty |= (1 << (chr >> 7));
}

static bool ctr_should_render(struct ctr_render_data *render_data)
{
  if (!render_data->rendering_frame)
  {
    if (!C3D_FrameBegin(0)) {
      return false;
    }
    render_data->rendering_frame = true;
    render_data->layer_num = 0;
    vertex_heap_pos = 0;
    C3D_FrameDrawOn(render_data->playfield);
    ctr_set_2d_projection(render_data, 1024, 512, false);
  }
  return true;
}

static void ctr_render_layer(struct graphics_data *graphics, struct video_layer *vlayer)
{
  struct ctr_layer *layer = (struct ctr_layer*) vlayer->platform_layer_data;
  struct ctr_render_data *render_data = graphics->render_data;
  struct char_element *src = vlayer->data;
  int tcol = vlayer->transparent_col;
  int offset = vlayer->offset;
  u32 bufsize = 4 * vlayer->w * vlayer->h * (vlayer->mode > 0 ? 3 : 1);
  u32 max_bufsize = 4 * vlayer->w * vlayer->h * 3;
  int k, l, m, n, col, col2, col3, col4, idx;
  s16 u, v;
  u32 i, j, ch;
  u32 protected_pal_position = graphics->protected_pal_position;
  bool has_content = false;

  if (!ctr_should_render(render_data))
    return;

  if (layer != NULL && (layer->w != vlayer->w || layer->h != vlayer->h || layer->draw_order != vlayer->draw_order))
  {
    linearFree(layer->foreground);
    C3D_TexDelete(&(layer->background));
    free(layer);
    layer = NULL;
  }

  if (layer == NULL)
  {
    layer = cmalloc(sizeof(struct ctr_layer));
    layer->w = vlayer->w; layer->h = vlayer->h; layer->draw_order = vlayer->draw_order;
    layer->foreground = clinearAlloc(sizeof(struct v_char) * max_bufsize);
    layer->background.data = NULL;
    C3D_TexInit(&(layer->background), next_power_of_two(layer->w < 8 ? 8 : layer->w), next_power_of_two(layer->h < 8 ? 8 : layer->h), GPU_RGBA8);
    C3D_TexSetFilter(&(layer->background), GPU_NEAREST, GPU_NEAREST);
    vlayer->platform_layer_data = layer;
    for (i = 0; i < max_bufsize; i++)
    {
      layer->foreground[i].z = (2000 - layer->draw_order) * 3 + 1;
      if ((i & 3) != 0) continue;
      layer->foreground[i + 0].x = ((i >> 2) % layer->w);
      layer->foreground[i + 0].y = (((i >> 2) / layer->w) % layer->h);
      layer->foreground[i + 1].x = ((i >> 2) % layer->w) + 1;
      layer->foreground[i + 1].y = (((i >> 2) / layer->w) % layer->h);
      layer->foreground[i + 2].x = ((i >> 2) % layer->w);
      layer->foreground[i + 2].y = (((i >> 2) / layer->w) % layer->h) + 1;
      layer->foreground[i + 3].x = ((i >> 2) % layer->w) + 1;
      layer->foreground[i + 3].y = (((i >> 2) / layer->w) % layer->h) + 1;
    }
  }

  layer->mode = vlayer->mode;

  if(layer->mode == 0)
  {
    l = 0;
    for (j = 0; j < layer->h; j++)
    {
      for (i = 0; i < layer->w; i++, src++)
      {
        k = ( morton_lut[(i & 7) + ((j & 7) << 3)] + ((i & (~7)) << 3) + ((j & (~7)) * layer->background.width) );
        if (src->char_value == INVISIBLE_CHAR)
        {
          ((u32*) layer->background.data)[k] = 0;
          layer->foreground[l++].col = 0;
          layer->foreground[l++].col = 0;
          layer->foreground[l++].col = 0;
          layer->foreground[l++].col = 0;
          continue;
        }
        has_content = true;
        ch = src->char_value;
        if (ch >= 0x100)
          ch = (ch & 0xFF) + PROTECTED_CHARSET_POSITION;
        else
          ch = (ch + offset) % PROTECTED_CHARSET_POSITION;
        u = (ch & 127) << 3;
        v = (NUM_CHARSETS * 32)-((ch >> 7) << 4);
	col = src->bg_color;
        if (col == tcol) col = 0;
        else
        {
          if (col >= 16) col = ((col - 16) & 0xF) + protected_pal_position;
          col = graphics->flat_intensity_palette[col];
        }
	col2 = src->fg_color;
        if (col2 == tcol) col2 = 0;
        else
        {
          if (col2 >= 16) col2 = ((col2 - 16) & 0xF) + protected_pal_position;
          col2 = graphics->flat_intensity_palette[col2];
        }
        ((u32*) layer->background.data)[k] = col;
        layer->foreground[l].u = u;
        layer->foreground[l].v = v;
        layer->foreground[l++].col = col2;
        layer->foreground[l].u = u+8;
        layer->foreground[l].v = v;
        layer->foreground[l++].col = col2;
        layer->foreground[l].u = u;
        layer->foreground[l].v = v-14;
        layer->foreground[l++].col = col2;
        layer->foreground[l].u = u+8;
        layer->foreground[l].v = v-14;
        layer->foreground[l++].col = col2;
      }
    }
  }
  else
  {
    k = 0;
    l = 0;
    m = layer->w * layer->h * 4;
    n = m * 2;
    for (j = 0; j < layer->h; j++)
    {
      for (i = 0; i < layer->w; i++, src++)
      {
        k = ( morton_lut[(i & 7) + ((j & 7) << 3)] + ((i & (~7)) << 3) + ((j & (~7)) * layer->background.width) );
        if (src->char_value == INVISIBLE_CHAR)
        {
          ((u32*) layer->background.data)[k] = 0;
          layer->foreground[l++].col = 0;
          layer->foreground[l++].col = 0;
          layer->foreground[l++].col = 0;
          layer->foreground[l++].col = 0;
          layer->foreground[m++].col = 0;
          layer->foreground[m++].col = 0;
          layer->foreground[m++].col = 0;
          layer->foreground[m++].col = 0;
          layer->foreground[n++].col = 0;
          layer->foreground[n++].col = 0;
          layer->foreground[n++].col = 0;
          layer->foreground[n++].col = 0;
          continue;
        }
        idx = ((src->bg_color << 6) | (src->fg_color << 2));
        has_content = true;
        ch = src->char_value;
        if (ch >= 0x100)
          ch = (ch & 0xFF) + PROTECTED_CHARSET_POSITION;
        else
          ch = (ch + offset) % PROTECTED_CHARSET_POSITION;
        u = (ch & 127) << 3;
        v = (NUM_CHARSETS * 32)-((ch >> 7) << 4);
        col = graphics->smzx_indices[idx + 0] == tcol ? 0 : graphics->flat_intensity_palette[graphics->smzx_indices[idx + 0]];
        col2 = graphics->smzx_indices[idx + 2] == tcol ? 0 : graphics->flat_intensity_palette[graphics->smzx_indices[idx + 2]];
        col3 = graphics->smzx_indices[idx + 1] == tcol ? 0 : graphics->flat_intensity_palette[graphics->smzx_indices[idx + 1]];
        col4 = graphics->smzx_indices[idx + 3] == tcol ? 0 : graphics->flat_intensity_palette[graphics->smzx_indices[idx + 3]];
        ((u32*) layer->background.data)[k] = col;
        layer->foreground[l].u = u;
        layer->foreground[l].v = v;
        layer->foreground[l++].col = col2;
        layer->foreground[l].u = u+4;
        layer->foreground[l].v = v;
        layer->foreground[l++].col = col2;
        layer->foreground[l].u = u;
        layer->foreground[l].v = v-14;
        layer->foreground[l++].col = col2;
        layer->foreground[l].u = u+4;
        layer->foreground[l].v = v-14;
        layer->foreground[l++].col = col2;

        layer->foreground[m].u = u;
        layer->foreground[m].v = v;
        layer->foreground[m++].col = col3;
        layer->foreground[m].u = u+4;
        layer->foreground[m].v = v;
        layer->foreground[m++].col = col3;
        layer->foreground[m].u = u;
        layer->foreground[m].v = v-14;
        layer->foreground[m++].col = col3;
        layer->foreground[m].u = u+4;
        layer->foreground[m].v = v-14;
        layer->foreground[m++].col = col3;

        layer->foreground[n].u = u;
        layer->foreground[n].v = v;
        layer->foreground[n++].col = col4;
        layer->foreground[n].u = u+4;
        layer->foreground[n].v = v;
        layer->foreground[n++].col = col4;
        layer->foreground[n].u = u;
        layer->foreground[n].v = v-14;
        layer->foreground[n++].col = col4;
        layer->foreground[n].u = u+4;
        layer->foreground[n].v = v-14;
        layer->foreground[n++].col = col4;
      }
    }
  }

  if (!has_content)
    return;

  C3D_TexFlush(&layer->background);

  C3D_CullFace(GPU_CULL_FRONT_CCW);
  ctr_draw_2d_texture(render_data, &layer->background, 0, layer->background.height - layer->h, layer->w, layer->h, vlayer->x, vlayer->y,
    layer->w * 8, layer->h * 14, (2000 - layer->draw_order) * 3.0f + 2.0f, false);
  C3D_CullFace(GPU_CULL_BACK_CCW);

  ctr_prepare_accel(render_data, layer->foreground, vlayer->x, vlayer->y);
  GSPGPU_FlushDataCache(layer->foreground, sizeof(struct v_char) * bufsize);

  if (render_data->charset_dirty != 0)
  {
    int coffs = 0;
    int csize = 0;
    int cincr = 1024*16;
    while ((render_data->charset_dirty & 1) == 0)
    {
      coffs += cincr;
      render_data->charset_dirty >>= 1;
    }
    while (render_data->charset_dirty != 0)
    {
      csize += cincr;
      render_data->charset_dirty >>= 1;
    }
    for (int i = 0; i < 4; i++)
    {
      GSPGPU_FlushDataCache((u8*)(render_data->charset[i].data) + coffs, csize);
      C3D_SafeTextureCopy((u8*)(render_data->charset[i].data) + coffs, 0,
        (u8*)(render_data->charset_vram[i].data) + coffs, 0, csize, 8);
      gspWaitForPPF();
    }
  }

  if (layer->mode == 0)
  {
    C3D_TexBind(0, &render_data->charset_vram[0]);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, layer->w * layer->h * 4);
  }
  else
  {
    C3D_TexBind(0, &render_data->charset_vram[2]);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, layer->w * layer->h * 4);
    C3D_TexBind(0, &render_data->charset_vram[1]);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, layer->w * layer->h * 4, layer->w * layer->h * 4);
    C3D_TexBind(0, &render_data->charset_vram[3]);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, layer->w * layer->h * 8, layer->w * layer->h * 4);
  }

  render_data->layer_num++;
}

static void ctr_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct ctr_render_data *render_data = graphics->render_data;
  Uint32 flatcolor = graphics->flat_intensity_palette[color];
  float y1 = (y + ((lines + offset) / 14.0f));
  float y2 = (y + (offset / 14.0f));
  struct v_char *map = render_data->cursor_map;

  if (!ctr_should_render(render_data))
    return;

  map[0].x = x;
  map[0].y = y1;
  map[0].col = flatcolor;
  map[1].x = x + 1;
  map[1].y = y1;
  map[1].col = flatcolor;
  map[2].x = x;
  map[2].y = y2;
  map[2].col = flatcolor;
  map[3].x = x + 1;
  map[3].y = y2;
  map[3].col = flatcolor;

  ctr_prepare_accel(render_data, map, 0, 0);
  C3D_TexBind(0, &render_data->charset_vram[0]);
  C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 4);
}

static void ctr_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct ctr_render_data *render_data = graphics->render_data;
  Uint32 col = 0xFFFFFF00;
  float y1 = ((y + h) / 14.0f);
  float y2 = (y / 14.0f);
  struct v_char *map = render_data->mouse_map;

  if (!ctr_should_render(render_data))
    return;

  map[0].x = x / 8.0f;
  map[0].y = y1;
  map[0].col = col;
  map[1].x = (x + w) / 8.0f;
  map[1].y = y1;
  map[1].col = col;
  map[2].x = x / 8.0f;
  map[2].y = y2;
  map[2].col = col;
  map[3].x = (x + w) / 8.0f;
  map[3].y = y2;
  map[3].col = col;

  C3D_AlphaBlend(GPU_BLEND_SUBTRACT, GPU_BLEND_ADD, GPU_SRC_COLOR, GPU_DST_COLOR, GPU_SRC_ALPHA, GPU_DST_ALPHA);
  ctr_prepare_accel(render_data, map, 0, 0);
  C3D_TexBind(0, &render_data->charset_vram[0]);
  C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 4);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}

static inline void ctr_draw_playfield(struct ctr_render_data *render_data, bool top_screen)
{
  int x, y;
  int width, height;
  float slider;

  ctr_set_2d_projection_screen(render_data, top_screen);
  if(top_screen)
  {
    if(ctr_keyboard_force_zoom_out())
    {
      slider = 1.0f;
    }
    else
    {
      slider = (osGet3DSliderState() * 1.3f) - 0.15f;

      if(slider < 0.0f) slider = 0.0f;
      if(slider > 1.0f) slider = 1.0f;
    }

    width = 400 + (240 * slider);
    height = 240 + (144 * slider);

    x = render_data->focus_x - (width / 2);
    y = render_data->focus_y - (height / 2);

    if (x < 0) { x = 0; }
    if ((x + width) > 640) { x = 640 - width; }

    if (height > 350) {
      ctr_draw_2d_texture(render_data, &render_data->playfield_tex, x, 512 - 350, width, 350, 0, (240 - (240 * 350 / height)) / 2, 400, 240 * 350 / height, 2.0f, true);
    } else {
      if (y < 0) { y = 0; }
      if ((y + height) > 350) { y = 350 - height; }

      ctr_draw_2d_texture(render_data, &render_data->playfield_tex, x, 512 - y - height, width, height, 0, 0, 400, 240, 2.0f, true);
    }
  }
  else
  {
    if (get_bottom_screen_mode() == BOTTOM_SCREEN_MODE_KEYBOARD)
      ctr_draw_2d_texture(render_data, &render_data->playfield_tex, 0, 512 - 350, 640, 350, 80, 12.75, 160, 87.5, 2.0f, true);
    else
      ctr_draw_2d_texture(render_data, &render_data->playfield_tex, 0, 512 - 350, 640, 350, 0, 32, 320, 175, 2.0f, true);
  }
}

static void ctr_sync_screen(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data;
  render_data = graphics->render_data;

  if (!ctr_should_render(render_data))
    return;

#ifndef RDR_DEBUG
  C3D_FrameDrawOn(render_data->target_top);
  ctr_draw_playfield(render_data, true);
#endif

  C3D_FrameDrawOn(render_data->target_bottom);
  if (get_bottom_screen_mode() == BOTTOM_SCREEN_MODE_KEYBOARD)
  {
    ctr_set_2d_projection_screen(render_data, false);
    ctr_keyboard_draw(render_data);
  }

  ctr_draw_playfield(render_data, false);

  render_data->cursor_on = 0;
  render_data->mouse_on = 0;

  C3D_FrameEnd(0);
  render_data->rendering_frame = false;
}

static void ctr_focus_pixel(struct graphics_data *graphics, Uint32 x, Uint32 y)
{
  struct ctr_render_data *render_data;

  if(!get_allow_focus_changes())
  {
    return;
  }

  render_data = graphics->render_data;
  render_data->focus_x = x;
  render_data->focus_y = y;
}

void render_ctr_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = ctr_init_video;
  renderer->free_video = ctr_free_video;
  renderer->check_video_mode = ctr_check_video_mode;
  renderer->set_video_mode = ctr_set_video_mode;
  renderer->update_colors = ctr_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_charsets = ctr_do_remap_charsets;
  renderer->remap_char = ctr_do_remap_char;
  renderer->remap_charbyte = ctr_do_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_layer = ctr_render_layer;
  renderer->render_cursor = ctr_render_cursor;
  renderer->render_mouse = ctr_render_mouse;
  renderer->sync_screen = ctr_sync_screen;
  renderer->focus_pixel = ctr_focus_pixel;
}

