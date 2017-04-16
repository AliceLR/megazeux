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
#include "render.h"

#include "shader_shbin.h"
#include "shader_accel_shbin.h"

// #define RDR_DEBUG

static struct linear_ptr_list_entry* linear_ptr_list = NULL;

static void free_linear_ptrs(void)
{
  while (linear_ptr_list != NULL)
  {
    linearFree(linear_ptr_list->ptr);
    linear_ptr_list = linear_ptr_list->next;
  }
}

// texture PNG dimensions must be powers of two
static bool tex_w_h_constraint(png_uint_32 w, png_uint_32 h)
{
  return w > 0 && h > 0 && ((w & (w - 1)) == 0) && ((h & (h - 1)) == 0);
}

static void* tex_alloc_png_surface(png_uint_32 w, png_uint_32 h,
  png_uint_32 *stride, void **pixels)
{
  C3D_Tex* tex;

  tex = calloc(1, sizeof(C3D_Tex));
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
    Mtx_OrthoTilt(&render_data->projection, 0, width, height, 0, -1.0, 6.0, true);
  else
    Mtx_Ortho(&render_data->projection, 0, width, 0, height, -1.0, 6.0, true);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader.proj_loc, &render_data->projection);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, render_data->shader_accel.proj_loc, &render_data->projection);
}

static inline void ctr_set_2d_projection_screen(struct ctr_render_data *render_data, bool top_screen)
{
  ctr_set_2d_projection(render_data, top_screen ? 400 : 320, 240, true);
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
    dataBuf = (u32*) linearAlloc(output->size);

    for (int i = 0; i < output->size >> 2; i++)
      dataBuf[i] = __builtin_bswap32(data[i]);

    GSPGPU_InvalidateDataCache(dataBuf, output->size);

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

void ctr_draw_2d_texture(struct ctr_render_data *render_data, C3D_Tex* texture,
  int tx, int ty, int tw, int th,
  float x, float y, float w, float h, float z)
{
  struct vertex* vertices;
  struct linear_ptr_list_entry* entry;
  float umin, umax, vmin, vmax;
  C3D_BufInfo* bufInfo;

  vertices = linearAlloc(sizeof(struct vertex) * 4);

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

  entry = malloc(sizeof(struct linear_ptr_list_entry));
  entry->ptr = vertices;
  entry->next = linear_ptr_list;
  linear_ptr_list = entry;
}

void ctr_init_shader(struct ctr_shader_data *shader, const void* data, int size)
{
  shader->dvlb = DVLB_ParseFile((u32 *) data, size);
  shaderProgramInit(&shader->program);
  shaderProgramSetVsh(&shader->program, &shader->dvlb->DVLE[0]);
  shader->proj_loc = shaderInstanceGetUniformLocation(shader->program.vertexShader, "projection");
  AttrInfo_Init(&shader->attr);
}

static bool ctr_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct ctr_render_data render_data;
  C3D_TexEnv* texEnv;

#ifdef RDR_DEBUG
  consoleInit(GFX_BOTTOM, NULL);
#endif

  memset(&render_data, 0, sizeof(struct ctr_render_data));
  graphics->render_data = &render_data;
  render_data.buffer = linearAlloc(1024 * 512 * 4);

  render_data.map = linearAlloc(MAP_QUADS * 4 * sizeof(struct v_char));
  for (int i = 0; i < MAP_QUADS; i++)
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

  for (int i = 0; i < MAP_QUADS * 3; i++)
  {
    render_data.map[i].z = 4 - (i / (80 * 25 * 4));
  }

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  gfxSet3D(false);

  render_data.bg_buf = linearAlloc(128 * 32 * 4);
  C3D_TexInitVRAM(&render_data.bg_tex, 128, 32, GPU_RGB8);
  render_data.playfield = C3D_RenderTargetCreate(1024, 512, GPU_RB_RGBA8, GPU_RB_DEPTH16);
  render_data.target_top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH16);
  render_data.target_bottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH16);

  C3D_RenderTargetSetClear(render_data.playfield, C3D_CLEAR_ALL, 0x000000FF, 0);
  C3D_RenderTargetSetClear(render_data.target_top, C3D_CLEAR_ALL, 0x000000FF, 0);
  C3D_RenderTargetSetClear(render_data.target_bottom, C3D_CLEAR_ALL, 0x000000FF, 0);
  C3D_RenderTargetSetOutput(render_data.target_top, GFX_TOP, GFX_LEFT,
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
  C3D_RenderTargetSetOutput(render_data.target_bottom, GFX_BOTTOM, GFX_LEFT,
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

  ctr_init_shader(&render_data.shader, shader_shbin, shader_shbin_size);
  ctr_init_shader(&render_data.shader_accel, shader_accel_shbin, shader_accel_shbin_size);

  AttrInfo_AddLoader(&render_data.shader.attr, 0, GPU_FLOAT, 3); // v0 = position
  AttrInfo_AddLoader(&render_data.shader.attr, 1, GPU_FLOAT, 2); // v1 = texcoord

  AttrInfo_AddLoader(&render_data.shader_accel.attr, 0, GPU_FLOAT, 3); // v0 = position
  AttrInfo_AddLoader(&render_data.shader_accel.attr, 1, GPU_UNSIGNED_BYTE, 4); // v1 = texcoord
  AttrInfo_AddLoader(&render_data.shader_accel.attr, 2, GPU_UNSIGNED_BYTE, 4); // v2 = color

  C3D_TexSetFilter(&render_data.playfield->renderBuf.colorBuf, GPU_LINEAR, GPU_LINEAR);
  C3D_TexSetFilter(&render_data.bg_tex, GPU_NEAREST, GPU_NEAREST);

  for (int i = 0; i < 3; i++)
  {
    render_data.charset[i].buffer = linearAlloc(256 * 256 * 2);
    memset(render_data.charset[i].buffer, 0xFF, 256 * 256 * 2);

    C3D_TexInitVRAM(&render_data.charset[i].texture, 256, 256, GPU_RGBA4);
    C3D_TexSetFilter(&render_data.charset[i].texture, GPU_NEAREST, GPU_NEAREST);
  }

  texEnv = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(texEnv, C3D_Both, GPU_TEXTURE0, 0, 0);
  C3D_TexEnvOp(texEnv, C3D_Both, 0, 0, 0);
  C3D_TexEnvFunc(texEnv, C3D_Both, GPU_MODULATE);

  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 32;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  ctr_keyboard_init(&render_data);

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
      (palette[i].r << 24) | (palette[i].g << 16) | (palette[i].b << 8) | 0x000000FF;
  }
}

static short *ctr_char_bitmask_to_texture(signed char *c, short *p)
{
  // This tests the 7th bit, if 0, result is 0.
  // If 1, result is 255 (because of sign extended bit shift!).
  // Note the use of char constants to force 8bit calculation.
  *(p++) = (*c << 24 >> 31);
  *(p++) = (*c << 25 >> 31);
  *(p++) = (*c << 26 >> 31);
  *(p++) = (*c << 27 >> 31);
  *(p++) = (*c << 28 >> 31);
  *(p++) = (*c << 29 >> 31);
  *(p++) = (*c << 30 >> 31);
  *(p++) = (*c << 31 >> 31);
  return p;
}

static short *ctr_char_bitmask_to_texture_smzx_1(signed char *c, short *p_)
{
  static const u32 alphas[] = {0x00000000, 0x000A000A, 0x00050005, 0x000F000F};
  u32 *p = (u32*) p_;

  *(p++) = alphas[((*c & 0xC0) >> 6)] | 0xFFF0FFF0;
  *(p++) = alphas[((*c & 0x30) >> 4)] | 0xFFF0FFF0;
  *(p++) = alphas[((*c & 0x0C) >> 2)] | 0xFFF0FFF0;
  *(p++) = alphas[(*c & 0x03)] | 0xFFF0FFF0;

  return (short*) p;
}

static inline void ctr_char_bitmask_to_textures_smzx_23(struct ctr_render_data *render_data,
  signed char *charset, u16 c, u8 b)
{
  int p, i;
  u8 ch, t;

  ch = charset[c * 14 + b];
  p = ((c & 0x1F) * 8) + ((c & 0x1E0) * 112) + (b * 256);

  for (i = 0; i < 4; i++)
  {
    t = (ch >> (6 - i*2)) & 3;

    *((u32*) (&render_data->charset[0].buffer[p])) = (t == 1) ? 0xFFFFFFFF : 0;
    *((u32*) (&render_data->charset[1].buffer[p])) = (t == 2) ? 0xFFFFFFFF : 0;
    *((u32*) (&render_data->charset[2].buffer[p])) = (t == 3) ? 0xFFFFFFFF : 0;
    p += 2;
  }
}

static inline void ctr_do_remap_charsets(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data;
  signed char *c;
  short *p;
  unsigned int i, j, k;

  render_data = graphics->render_data;
  c = (signed char *)graphics->charset;
  p = (short *)render_data->charset[0].buffer;

  switch(graphics->screen_mode)
  {
    case 2:
    case 3:
      for(i = 0; i < 512; i++)
        for(j = 0; j < 14; j++)
          ctr_char_bitmask_to_textures_smzx_23(render_data, c, i, j);
      break;
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

  render_data->charset_dirty = 1;
}

static inline void ctr_do_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  struct ctr_render_data *render_data;
  signed char *c;
  short *p;
  unsigned int i;

  render_data = graphics->render_data;
  c = (signed char *)graphics->charset;
  p = (short *)render_data->charset[0].buffer;

  p += ((chr & 0x1F) * 8) + ((chr & 0x1E0) * 112);
  c += chr * 14;

  switch(graphics->screen_mode)
  {
    case 2:
    case 3:
      for(i = 0; i < 14; i++)
        ctr_char_bitmask_to_textures_smzx_23(render_data, (signed char *)graphics->charset, chr, i);
      break;
    case 1:
      for(i = 0; i < 14; i++, c++, p += 256)
        ctr_char_bitmask_to_texture_smzx_1(c, p);
      break;
    default:
      for(i = 0; i < 14; i++, c++, p += 256)
        ctr_char_bitmask_to_texture(c, p);
      break;
  }

  render_data->charset_dirty = 1;
}

static void ctr_do_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
  struct ctr_render_data *render_data;
  signed char *c;
  short *p;

  render_data = graphics->render_data;
  c = (signed char *)graphics->charset;
  p = (short *)render_data->charset[0].buffer;

  p += ((chr & 0x1F) * 8) + ((chr & 0x1E0) * 112) + (byte << 8);
  c += chr * 14;

  switch(graphics->screen_mode)
  {
    case 2:
    case 3:
      ctr_char_bitmask_to_textures_smzx_23(render_data, (signed char *)graphics->charset, chr, byte);
      break;
    case 1:
      ctr_char_bitmask_to_texture_smzx_1(c, p);
      break;
    default:
      ctr_char_bitmask_to_texture(c, p);
      break;
  }

  render_data->charset_dirty = 1;
}

static inline int getSmzxColor23(struct graphics_data *graphics, struct char_element *src, int i)
{
  if(graphics->screen_mode == 3)
  {
    return graphics->flat_intensity_palette[(((src->bg_color << 4) | src->fg_color) + i) & 255];
  }
  else
  {
    switch(i)
    {
      case 0:
      default:
        return graphics->flat_intensity_palette[src->bg_color * 0x11];
      case 1:
        return graphics->flat_intensity_palette[(src->fg_color << 4) | src->bg_color];
      case 2:
        return graphics->flat_intensity_palette[(src->bg_color << 4) | src->fg_color];
      case 3:
        return graphics->flat_intensity_palette[src->fg_color * 0x11];
    }
  }
}

static void ctr_render_graph(struct graphics_data *graphics)
{
  unsigned long tmp;  
  struct ctr_render_data *render_data;
  struct char_element *src;
  Uint32 mode;
  int i, j, k, l, m, n, col, col2, col3, col4;
  u8 u, v;

  render_data = graphics->render_data;
  src = graphics->text_video;
  mode = graphics->screen_mode;

  if(mode < 2)
  {
    k = 0;
    l = 0;
    src = &graphics->text_video[80 * 24];
    for (j = 0; j < 25; j++, src -= 160)
    {
      for (i = 0; i < 80; i++, src++)
      {
        u = src->char_value & 31;
        v = src->char_value >> 5;
        col = graphics->flat_intensity_palette[src->bg_color];
        col2 = graphics->flat_intensity_palette[src->fg_color];
        render_data->bg_buf[k++] = col;
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
      k += (128 - 80);
    }
  }
  else
  {
    k = 0;
    l = 0;
    m = 80 * 25 * 4;
    n = 80 * 25 * 8;
    src = &graphics->text_video[80 * 24];
    for (j = 0; j < 25; j++, src -= 160)
    {
      for (i = 0; i < 80; i++, src++)
      {
        u = src->char_value & 31;
        v = src->char_value >> 5;
        col = getSmzxColor23(graphics, src, 0);
        col2 = getSmzxColor23(graphics, src, 2);
        col3 = getSmzxColor23(graphics, src, 1);
        col4 = getSmzxColor23(graphics, src, 3);
        render_data->bg_buf[k++] = col;
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

        render_data->map[m].u = u;
        render_data->map[m].v = v+1;
        render_data->map[m++].col = col3;
        render_data->map[m].u = u+1;
        render_data->map[m].v = v+1;
        render_data->map[m++].col = col3;
        render_data->map[m].u = u;
        render_data->map[m].v = v;
        render_data->map[m++].col = col3;
        render_data->map[m].u = u+1;
        render_data->map[m].v = v;
        render_data->map[m++].col = col3;

        render_data->map[n].u = u;
        render_data->map[n].v = v+1;
        render_data->map[n++].col = col4;
        render_data->map[n].u = u+1;
        render_data->map[n].v = v+1;
        render_data->map[n++].col = col4;
        render_data->map[n].u = u;
        render_data->map[n].v = v;
        render_data->map[n++].col = col4;
        render_data->map[n].u = u+1;
        render_data->map[n].v = v;
        render_data->map[n++].col = col4;
      }
      k += (128 - 80);
    }
  }

  GSPGPU_InvalidateDataCache(render_data->bg_buf, 128 * 25);
  C3D_SafeDisplayTransfer((u32 *) render_data->bg_buf, GX_BUFFER_DIM(128, 32),
    (u32 *) render_data->bg_tex.data, GX_BUFFER_DIM(128, 32),
    GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
    GX_TRANSFER_IN_FORMAT(GPU_RGBA8) | GX_TRANSFER_OUT_FORMAT(GPU_RGB8)
    | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
}

static void ctr_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct ctr_render_data *render_data = graphics->render_data;
  Uint32 flatcolor = graphics->flat_intensity_palette[color];
  int ind = MAP_CHAR_SIZE;
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
  int offset = MAP_CHAR_SIZE + 4;
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
      ctr_draw_2d_texture(render_data, &render_data->playfield->renderBuf.colorBuf, x, 0, width, 350, 0, (240 - (240 * 350 / height)) / 2, 400, 240 * 350 / height, 2.0f);
    } else {
      if (y < 0) { y = 0; }
      if ((y + height) > 350) { y = 350 - height; }

      ctr_draw_2d_texture(render_data, &render_data->playfield->renderBuf.colorBuf, x, 350 - y - height, width, height, 0, 0, 400, 240, 2.0f);
    }
  }
  else
  {
    if (get_bottom_screen_mode() == BOTTOM_SCREEN_MODE_KEYBOARD)
      ctr_draw_2d_texture(render_data, &render_data->playfield->renderBuf.colorBuf, 0, 0, 640, 350, 80, 12.75, 160, 87.5, 2.0f);
    else
      ctr_draw_2d_texture(render_data, &render_data->playfield->renderBuf.colorBuf, 0, 0, 640, 350, 0, 32, 320, 175, 2.0f);
  }
}

static void ctr_render_playfield(struct graphics_data *graphics, struct ctr_render_data *render_data)
{
  C3D_BufInfo* bufInfo;

  ctr_set_2d_projection(render_data, 1024, 512, false);

  if(render_data->last_smzx_mode != graphics->screen_mode)
  {
    // SMZX mode changed, re-draw textures (unless mode change is 2<->3,
    // those use the same texture format)
    if(!((render_data->last_smzx_mode == 3 && graphics->screen_mode == 2)
      || (render_data->last_smzx_mode == 2 && graphics->screen_mode == 3)))
    {
      ctr_do_remap_charsets(graphics);
    }
  }

  if(render_data->charset_dirty)
  {
    for (int i = 0; i < ((graphics->screen_mode < 2) ? 1 : 3); i++)
    {
      GSPGPU_InvalidateDataCache(render_data->charset[i].buffer, 256 * (14 * 17) * 2);
      C3D_SafeDisplayTransfer((u32 *) render_data->charset[i].buffer, GX_BUFFER_DIM(256, 256),
        (u32 *) render_data->charset[i].texture.data, GX_BUFFER_DIM(256, 256),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GPU_RGBA4) | GX_TRANSFER_OUT_FORMAT(GPU_RGBA4)
        | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    }

    render_data->charset_dirty = false;
    render_data->last_smzx_mode = graphics->screen_mode;
  }

  C3D_CullFace(GPU_CULL_NONE);
  ctr_draw_2d_texture(render_data, &render_data->bg_tex, 0, 32 - 25, 80, 25, 0, 0, 640, 350, 4.0f);
  C3D_CullFace(GPU_CULL_BACK_CCW);

  bufInfo = C3D_GetBufInfo();
  BufInfo_Init(bufInfo);
  BufInfo_Add(bufInfo, render_data->map, sizeof(struct v_char), 3, 0x210);

  ctr_bind_shader(&render_data->shader_accel);
  if(graphics->screen_mode < 2)
  {
    C3D_TexBind(0, &render_data->charset[0].texture);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 80 * 25 * 4);
  }
  else
  {
    C3D_TexBind(0, &render_data->charset[0].texture);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 80 * 25 * 4);

    C3D_TexBind(0, &render_data->charset[1].texture);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 80 * 25 * 4, 80 * 25 * 4);

    C3D_TexBind(0, &render_data->charset[2].texture);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, 80 * 25 * 8, 80 * 25 * 4);
  }

  if (render_data->cursor_on)
  {
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, MAP_CHAR_SIZE, 4);
  }

  if (render_data->mouse_on)
  {
    C3D_AlphaBlend(GPU_BLEND_SUBTRACT, GPU_BLEND_ADD, GPU_SRC_COLOR, GPU_DST_COLOR, GPU_SRC_ALPHA, GPU_DST_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLE_STRIP, MAP_CHAR_SIZE + 4, 4);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
  }
}

static void ctr_sync_screen(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data;
  render_data = graphics->render_data;

  if (!C3D_FrameBegin(0)) {
    return;
  }
  free_linear_ptrs();

  C3D_FrameDrawOn(render_data->playfield);
  ctr_render_playfield(graphics, render_data);

  C3D_FrameDrawOn(render_data->target_top);
  ctr_draw_playfield(render_data, true);

#ifndef RDR_DEBUG
  C3D_FrameDrawOn(render_data->target_bottom);
  if (get_bottom_screen_mode() == BOTTOM_SCREEN_MODE_KEYBOARD)
  {
    ctr_set_2d_projection_screen(render_data, false);
    ctr_keyboard_draw(render_data);
  }

  ctr_draw_playfield(render_data, false);
#endif

  render_data->cursor_on = 0;
  render_data->mouse_on = 0;

  C3D_FrameEnd(0);
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
