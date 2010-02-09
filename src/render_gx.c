/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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
#include "renderers.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#define BOOL _BOOL
#include <ogc/system.h>
#include <ogc/cache.h>
#include <ogc/video.h>
#include <ogc/gx.h>
#include <ogc/gu.h>
#undef BOOL

#define DEFAULT_FIFO_SIZE (1024 * 1024)
#define CHAR_TEX_SIZE (sizeof(u32) * 512 * 16 * 2)
#define SMZX_TEX_OFFSET (512 * 16)

#define TEX_OFF_X (0.5 / 256)
#define TEX_OFF_Y (0.5 / 512)

// Must be multiple of 32 bytes
typedef struct {
  u16 pal[16];
} ci4tlut;

typedef struct {
  void *xfb[2];
  void *fifo;
  GXRModeObj *rmode;
  ci4tlut *smzxtlut;
  void *charimg;
  GXTlutObj mzxtlutobj;
  GXTlutObj smzxtlutobj[512];
  GXTexObj chartex;
  GXColor palette[SMZX_PAL_SIZE];
  u16 tlutpal[SMZX_PAL_SIZE];
  int chrdirty;
  int paldirty;
  int curfb;
} gx_render_data;

// IA8 TLUT for MZX/SMZX mode 1
static ci4tlut mzxtlut ATTRIBUTE_ALIGN(32) =
{{
  0x00FF,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0xAAFF,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x55FF,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0xFFFF
}};

// Lookup table for generating CI4 charset textures
static u32 mzxtexline[256] =
{
  0x00000000, 0x0000000F, 0x000000F0, 0x000000FF,
  0x00000F00, 0x00000F0F, 0x00000FF0, 0x00000FFF,
  0x0000F000, 0x0000F00F, 0x0000F0F0, 0x0000F0FF,
  0x0000FF00, 0x0000FF0F, 0x0000FFF0, 0x0000FFFF,
  0x000F0000, 0x000F000F, 0x000F00F0, 0x000F00FF,
  0x000F0F00, 0x000F0F0F, 0x000F0FF0, 0x000F0FFF,
  0x000FF000, 0x000FF00F, 0x000FF0F0, 0x000FF0FF,
  0x000FFF00, 0x000FFF0F, 0x000FFFF0, 0x000FFFFF,
  0x00F00000, 0x00F0000F, 0x00F000F0, 0x00F000FF,
  0x00F00F00, 0x00F00F0F, 0x00F00FF0, 0x00F00FFF,
  0x00F0F000, 0x00F0F00F, 0x00F0F0F0, 0x00F0F0FF,
  0x00F0FF00, 0x00F0FF0F, 0x00F0FFF0, 0x00F0FFFF,
  0x00FF0000, 0x00FF000F, 0x00FF00F0, 0x00FF00FF,
  0x00FF0F00, 0x00FF0F0F, 0x00FF0FF0, 0x00FF0FFF,
  0x00FFF000, 0x00FFF00F, 0x00FFF0F0, 0x00FFF0FF,
  0x00FFFF00, 0x00FFFF0F, 0x00FFFFF0, 0x00FFFFFF,
  0x0F000000, 0x0F00000F, 0x0F0000F0, 0x0F0000FF,
  0x0F000F00, 0x0F000F0F, 0x0F000FF0, 0x0F000FFF,
  0x0F00F000, 0x0F00F00F, 0x0F00F0F0, 0x0F00F0FF,
  0x0F00FF00, 0x0F00FF0F, 0x0F00FFF0, 0x0F00FFFF,
  0x0F0F0000, 0x0F0F000F, 0x0F0F00F0, 0x0F0F00FF,
  0x0F0F0F00, 0x0F0F0F0F, 0x0F0F0FF0, 0x0F0F0FFF,
  0x0F0FF000, 0x0F0FF00F, 0x0F0FF0F0, 0x0F0FF0FF,
  0x0F0FFF00, 0x0F0FFF0F, 0x0F0FFFF0, 0x0F0FFFFF,
  0x0FF00000, 0x0FF0000F, 0x0FF000F0, 0x0FF000FF,
  0x0FF00F00, 0x0FF00F0F, 0x0FF00FF0, 0x0FF00FFF,
  0x0FF0F000, 0x0FF0F00F, 0x0FF0F0F0, 0x0FF0F0FF,
  0x0FF0FF00, 0x0FF0FF0F, 0x0FF0FFF0, 0x0FF0FFFF,
  0x0FFF0000, 0x0FFF000F, 0x0FFF00F0, 0x0FFF00FF,
  0x0FFF0F00, 0x0FFF0F0F, 0x0FFF0FF0, 0x0FFF0FFF,
  0x0FFFF000, 0x0FFFF00F, 0x0FFFF0F0, 0x0FFFF0FF,
  0x0FFFFF00, 0x0FFFFF0F, 0x0FFFFFF0, 0x0FFFFFFF,
  0xF0000000, 0xF000000F, 0xF00000F0, 0xF00000FF,
  0xF0000F00, 0xF0000F0F, 0xF0000FF0, 0xF0000FFF,
  0xF000F000, 0xF000F00F, 0xF000F0F0, 0xF000F0FF,
  0xF000FF00, 0xF000FF0F, 0xF000FFF0, 0xF000FFFF,
  0xF00F0000, 0xF00F000F, 0xF00F00F0, 0xF00F00FF,
  0xF00F0F00, 0xF00F0F0F, 0xF00F0FF0, 0xF00F0FFF,
  0xF00FF000, 0xF00FF00F, 0xF00FF0F0, 0xF00FF0FF,
  0xF00FFF00, 0xF00FFF0F, 0xF00FFFF0, 0xF00FFFFF,
  0xF0F00000, 0xF0F0000F, 0xF0F000F0, 0xF0F000FF,
  0xF0F00F00, 0xF0F00F0F, 0xF0F00FF0, 0xF0F00FFF,
  0xF0F0F000, 0xF0F0F00F, 0xF0F0F0F0, 0xF0F0F0FF,
  0xF0F0FF00, 0xF0F0FF0F, 0xF0F0FFF0, 0xF0F0FFFF,
  0xF0FF0000, 0xF0FF000F, 0xF0FF00F0, 0xF0FF00FF,
  0xF0FF0F00, 0xF0FF0F0F, 0xF0FF0FF0, 0xF0FF0FFF,
  0xF0FFF000, 0xF0FFF00F, 0xF0FFF0F0, 0xF0FFF0FF,
  0xF0FFFF00, 0xF0FFFF0F, 0xF0FFFFF0, 0xF0FFFFFF,
  0xFF000000, 0xFF00000F, 0xFF0000F0, 0xFF0000FF,
  0xFF000F00, 0xFF000F0F, 0xFF000FF0, 0xFF000FFF,
  0xFF00F000, 0xFF00F00F, 0xFF00F0F0, 0xFF00F0FF,
  0xFF00FF00, 0xFF00FF0F, 0xFF00FFF0, 0xFF00FFFF,
  0xFF0F0000, 0xFF0F000F, 0xFF0F00F0, 0xFF0F00FF,
  0xFF0F0F00, 0xFF0F0F0F, 0xFF0F0FF0, 0xFF0F0FFF,
  0xFF0FF000, 0xFF0FF00F, 0xFF0FF0F0, 0xFF0FF0FF,
  0xFF0FFF00, 0xFF0FFF0F, 0xFF0FFFF0, 0xFF0FFFFF,
  0xFFF00000, 0xFFF0000F, 0xFFF000F0, 0xFFF000FF,
  0xFFF00F00, 0xFFF00F0F, 0xFFF00FF0, 0xFFF00FFF,
  0xFFF0F000, 0xFFF0F00F, 0xFFF0F0F0, 0xFFF0F0FF,
  0xFFF0FF00, 0xFFF0FF0F, 0xFFF0FFF0, 0xFFF0FFFF,
  0xFFFF0000, 0xFFFF000F, 0xFFFF00F0, 0xFFFF00FF,
  0xFFFF0F00, 0xFFFF0F0F, 0xFFFF0FF0, 0xFFFF0FFF,
  0xFFFFF000, 0xFFFFF00F, 0xFFFFF0F0, 0xFFFFF0FF,
  0xFFFFFF00, 0xFFFFFF0F, 0xFFFFFFF0, 0xFFFFFFFF
};

// Lookup table for generating CI4 SMZX charset textures
static u32 smzxtexline[256] =
{
  0x00000000, 0x00000055, 0x000000AA, 0x000000FF,
  0x00005500, 0x00005555, 0x000055AA, 0x000055FF,
  0x0000AA00, 0x0000AA55, 0x0000AAAA, 0x0000AAFF,
  0x0000FF00, 0x0000FF55, 0x0000FFAA, 0x0000FFFF,
  0x00550000, 0x00550055, 0x005500AA, 0x005500FF,
  0x00555500, 0x00555555, 0x005555AA, 0x005555FF,
  0x0055AA00, 0x0055AA55, 0x0055AAAA, 0x0055AAFF,
  0x0055FF00, 0x0055FF55, 0x0055FFAA, 0x0055FFFF,
  0x00AA0000, 0x00AA0055, 0x00AA00AA, 0x00AA00FF,
  0x00AA5500, 0x00AA5555, 0x00AA55AA, 0x00AA55FF,
  0x00AAAA00, 0x00AAAA55, 0x00AAAAAA, 0x00AAAAFF,
  0x00AAFF00, 0x00AAFF55, 0x00AAFFAA, 0x00AAFFFF,
  0x00FF0000, 0x00FF0055, 0x00FF00AA, 0x00FF00FF,
  0x00FF5500, 0x00FF5555, 0x00FF55AA, 0x00FF55FF,
  0x00FFAA00, 0x00FFAA55, 0x00FFAAAA, 0x00FFAAFF,
  0x00FFFF00, 0x00FFFF55, 0x00FFFFAA, 0x00FFFFFF,
  0x55000000, 0x55000055, 0x550000AA, 0x550000FF,
  0x55005500, 0x55005555, 0x550055AA, 0x550055FF,
  0x5500AA00, 0x5500AA55, 0x5500AAAA, 0x5500AAFF,
  0x5500FF00, 0x5500FF55, 0x5500FFAA, 0x5500FFFF,
  0x55550000, 0x55550055, 0x555500AA, 0x555500FF,
  0x55555500, 0x55555555, 0x555555AA, 0x555555FF,
  0x5555AA00, 0x5555AA55, 0x5555AAAA, 0x5555AAFF,
  0x5555FF00, 0x5555FF55, 0x5555FFAA, 0x5555FFFF,
  0x55AA0000, 0x55AA0055, 0x55AA00AA, 0x55AA00FF,
  0x55AA5500, 0x55AA5555, 0x55AA55AA, 0x55AA55FF,
  0x55AAAA00, 0x55AAAA55, 0x55AAAAAA, 0x55AAAAFF,
  0x55AAFF00, 0x55AAFF55, 0x55AAFFAA, 0x55AAFFFF,
  0x55FF0000, 0x55FF0055, 0x55FF00AA, 0x55FF00FF,
  0x55FF5500, 0x55FF5555, 0x55FF55AA, 0x55FF55FF,
  0x55FFAA00, 0x55FFAA55, 0x55FFAAAA, 0x55FFAAFF,
  0x55FFFF00, 0x55FFFF55, 0x55FFFFAA, 0x55FFFFFF,
  0xAA000000, 0xAA000055, 0xAA0000AA, 0xAA0000FF,
  0xAA005500, 0xAA005555, 0xAA0055AA, 0xAA0055FF,
  0xAA00AA00, 0xAA00AA55, 0xAA00AAAA, 0xAA00AAFF,
  0xAA00FF00, 0xAA00FF55, 0xAA00FFAA, 0xAA00FFFF,
  0xAA550000, 0xAA550055, 0xAA5500AA, 0xAA5500FF,
  0xAA555500, 0xAA555555, 0xAA5555AA, 0xAA5555FF,
  0xAA55AA00, 0xAA55AA55, 0xAA55AAAA, 0xAA55AAFF,
  0xAA55FF00, 0xAA55FF55, 0xAA55FFAA, 0xAA55FFFF,
  0xAAAA0000, 0xAAAA0055, 0xAAAA00AA, 0xAAAA00FF,
  0xAAAA5500, 0xAAAA5555, 0xAAAA55AA, 0xAAAA55FF,
  0xAAAAAA00, 0xAAAAAA55, 0xAAAAAAAA, 0xAAAAAAFF,
  0xAAAAFF00, 0xAAAAFF55, 0xAAAAFFAA, 0xAAAAFFFF,
  0xAAFF0000, 0xAAFF0055, 0xAAFF00AA, 0xAAFF00FF,
  0xAAFF5500, 0xAAFF5555, 0xAAFF55AA, 0xAAFF55FF,
  0xAAFFAA00, 0xAAFFAA55, 0xAAFFAAAA, 0xAAFFAAFF,
  0xAAFFFF00, 0xAAFFFF55, 0xAAFFFFAA, 0xAAFFFFFF,
  0xFF000000, 0xFF000055, 0xFF0000AA, 0xFF0000FF,
  0xFF005500, 0xFF005555, 0xFF0055AA, 0xFF0055FF,
  0xFF00AA00, 0xFF00AA55, 0xFF00AAAA, 0xFF00AAFF,
  0xFF00FF00, 0xFF00FF55, 0xFF00FFAA, 0xFF00FFFF,
  0xFF550000, 0xFF550055, 0xFF5500AA, 0xFF5500FF,
  0xFF555500, 0xFF555555, 0xFF5555AA, 0xFF5555FF,
  0xFF55AA00, 0xFF55AA55, 0xFF55AAAA, 0xFF55AAFF,
  0xFF55FF00, 0xFF55FF55, 0xFF55FFAA, 0xFF55FFFF,
  0xFFAA0000, 0xFFAA0055, 0xFFAA00AA, 0xFFAA00FF,
  0xFFAA5500, 0xFFAA5555, 0xFFAA55AA, 0xFFAA55FF,
  0xFFAAAA00, 0xFFAAAA55, 0xFFAAAAAA, 0xFFAAAAFF,
  0xFFAAFF00, 0xFFAAFF55, 0xFFAAFFAA, 0xFFAAFFFF,
  0xFFFF0000, 0xFFFF0055, 0xFFFF00AA, 0xFFFF00FF,
  0xFFFF5500, 0xFFFF5555, 0xFFFF55AA, 0xFFFF55FF,
  0xFFFFAA00, 0xFFFFAA55, 0xFFFFAAAA, 0xFFFFAAFF,
  0xFFFFFF00, 0xFFFFFF55, 0xFFFFFFAA, 0xFFFFFFFF
};

static bool gx_init_video(graphics_data *graphics, config_info *conf)
{
  const GXColor black = {0, 0, 0, 255};

  gx_render_data *render_data;
  f32 yscale;
  u32 xfbHeight;
  Mtx identmtx;
  GXRModeObj *rmode;
  int i;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  render_data = malloc(sizeof(gx_render_data));
  graphics->render_data = render_data;

  rmode = render_data->rmode = VIDEO_GetPreferredMode(NULL);
  render_data->xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  render_data->xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  render_data->curfb = 0;

  render_data->charimg = memalign(32, CHAR_TEX_SIZE);
  memset(render_data->charimg, 0, CHAR_TEX_SIZE);
  render_data->smzxtlut = memalign(32, sizeof(ci4tlut) * 512);
  memset(render_data->smzxtlut, 0, sizeof(ci4tlut) * 512);

  GX_InitTlutObj(&render_data->mzxtlutobj, &mzxtlut, GX_TL_IA8, 16);
  for(i = 0; i < 512; i++)
    GX_InitTlutObj(render_data->smzxtlutobj + i, render_data->smzxtlut + i,
     GX_TL_RGB565, 16);
  GX_InitTexObjCI(&render_data->chartex, render_data->charimg, 256, 512,
   GX_TF_CI4, GX_REPEAT, GX_REPEAT, GX_FALSE, GX_TLUT0);

  VIDEO_Configure(rmode);
  VIDEO_SetNextFramebuffer(render_data->xfb[0]);
  VIDEO_SetBlack(FALSE);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if(rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync();

  render_data->fifo = memalign(32, DEFAULT_FIFO_SIZE);
  memset(render_data->fifo, 0, DEFAULT_FIFO_SIZE);
  GX_Init(render_data->fifo, DEFAULT_FIFO_SIZE);

  yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
  xfbHeight = GX_SetDispCopyYScale(yscale);
  GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
  GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
  GX_SetFieldMode(rmode->field_rendering,
   ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  if(rmode->aa)
    GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
  else
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetDispCopyGamma(GX_GM_1_0);
  GX_SetCopyClear(black, GX_MAX_Z24);

  GX_ClearVtxDesc();
  GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

  GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);
  GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  GX_SetNumChans(1);
  GX_SetNumTexGens(1);
  GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHTNULL,
   GX_DF_NONE, GX_AF_NONE);
  GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
  GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
  GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  guMtxIdentity(identmtx);
  GX_LoadPosMtxImm(identmtx, GX_PNMTX0);

  GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
  GX_SetAlphaUpdate(GX_TRUE);
  GX_SetCullMode(GX_CULL_NONE);

  GX_CopyDisp(render_data->xfb[1], GX_TRUE);
  GX_CopyDisp(render_data->xfb[0], GX_FALSE);
  GX_Flush();
  delay(500);

  render_data->chrdirty = 1;
  render_data->paldirty = 1;

  return set_video_mode();
}

static bool gx_check_video_mode(graphics_data *graphics, int width, int height,
 int depth, int fullscreen, int resize)
{
  return true;
}

static bool gx_set_video_mode(graphics_data *graphics, int width, int height,
 int depth, int fullscreen, int resize)
{
  gx_render_data *render_data = graphics->render_data;
  float w, h, scale;
  Mtx projmtx;

  if(fullscreen)
    scale = 8.0 / 9.0;
  else
    scale = 8.0 / 10.0;

  w = 640.0 * (int)render_data->rmode->viWidth / scale / 720.0;
  h = 350.0 / scale;
  guOrtho(projmtx, 175 - h / 2, 175 + h / 2, 320 - w / 2, 320 + w / 2,
   -1.0, 1.0);
  GX_LoadProjectionMtx(projmtx, GX_ORTHOGRAPHIC);

  return true;
}

static void gx_update_colors(graphics_data *graphics, rgb_color *palette,
 Uint32 count)
{
  gx_render_data *render_data = graphics->render_data;
  Uint32 i;

  render_data->paldirty = 1;
  for(i = 0; i < count; i++)
  {
    render_data->palette[i].r = palette[i].r;
    render_data->palette[i].g = palette[i].g;
    render_data->palette[i].b = palette[i].b;
    render_data->palette[i].a = 255;
    render_data->tlutpal[i] = (palette[i].b >> 3) | ((palette[i].g & 0xFC) << 3)
     | ((palette[i].r & 0xF8) << 8);
  }
}

static void gx_draw_char(u32 *tex, Uint8 *chr)
{
  u32 *dest = tex;
  Uint8 *src = chr;
  int i;
  for(i = 0; i < 14; i++)
  {
    *dest = mzxtexline[*src];
    src++; dest++;
    if((i & 0x7) == 0x7)
      dest += 31 * 8;
  }

  dest = tex + SMZX_TEX_OFFSET;
  src = chr;
  for(i = 0; i < 14; i++)
  {
    *dest = smzxtexline[*src];
    src++; dest++;
    if((i & 0x7) == 0x7)
      dest += 31 * 8;
  }
}

static void gx_remap_charsets(graphics_data *graphics)
{
  gx_render_data *render_data = graphics->render_data;
  Uint8 *src = graphics->charset;
  u32 *dest = render_data->charimg;
  int i;
  render_data->chrdirty = 1;

  for(i = 0; i < 512; i++)
  {
    gx_draw_char(dest, src);
    src += 14;
    dest += 8;
    if((i & 0x1F) == 0x1F)
      dest += 32 * 8;
  }
}

static void gx_remap_char(graphics_data *graphics, Uint16 chr)
{
  gx_render_data *render_data = graphics->render_data;
  u32 *tex = render_data->charimg;
  int offset = (chr & 0x1F) * 8 + (chr >> 5) * 512;
  render_data->chrdirty = 1;
  gx_draw_char(tex + offset, graphics->charset + (chr * 14));
}

static void gx_remap_charbyte(graphics_data *graphics, Uint16 chr, Uint8 byte)
{
  gx_render_data *render_data = graphics->render_data;
  u32 *tex = render_data->charimg;
  int offset = (chr & 0x1F) * 8 + (chr >> 5) * 512 + byte;
  if(byte > 7)
    offset += 31 * 8;
  render_data->chrdirty = 1;
  tex[offset] = mzxtexline[graphics->charset[chr * 14 + byte]];
  tex[offset + SMZX_TEX_OFFSET] =
   smzxtexline[graphics->charset[chr * 14 + byte]];
}

static void gx_render_graph(graphics_data *graphics)
{
  gx_render_data *render_data = graphics->render_data;
  GXColor *pal = render_data->palette;
  int i, j, x, y;
  float u, v;
  char_element *src = graphics->text_video;

  if(render_data->chrdirty)
  {
    DCFlushRange(render_data->charimg, CHAR_TEX_SIZE);
    GX_InvalidateTexAll();
  }

  GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

  if(graphics->screen_mode > 1)
  {
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    if(render_data->paldirty)
    {
      if(!render_data->chrdirty)
        GX_InvalidateTexAll();
      for(i = 0; i < 512; i++)
        render_data->smzxtlut[i].pal[1] = 0;
      render_data->paldirty = 0;
    }
  }
  else
  {
    GX_LoadTlut(&render_data->mzxtlutobj, GX_TLUT0);
    GX_LoadTexObj(&render_data->chartex, GX_TEXMAP0);
  }

  render_data->chrdirty = 0;

  switch(graphics->screen_mode)
  {
    case 0:
    {
      for(y = 0; y < 25; y++)
      {
        for(x = 0; x < 80; x++)
        {
          GX_SetChanMatColor(GX_COLOR0A0, pal[src->bg_color]);
          GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position2u16(x * 8, y * 14);
            GX_TexCoord2f32(0, 0);
            GX_Position2u16(x * 8 + 8, y * 14);
            GX_TexCoord2f32(0, 0);
            GX_Position2u16(x * 8 + 8, y * 14 + 14);
            GX_TexCoord2f32(0, 0);
            GX_Position2u16(x * 8, y * 14 + 14);
            GX_TexCoord2f32(0, 0);
          GX_End();
          src++;
        }
      }

      GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

      src = graphics->text_video;
      for(y = 0; y < 25; y++)
      {
        for(x = 0; x < 80; x++)
        {
          GX_SetChanMatColor(GX_COLOR0A0, pal[src->fg_color]);
          u = ((int)src->char_value & 0x1f) / 32.0;
          v = ((int)src->char_value >> 5) / 32.0;
          GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position2u16(x * 8, y * 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14 + 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X,
             v + 7 / 256.0 - TEX_OFF_Y);
            GX_Position2u16(x * 8, y * 14 + 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + 7 / 256.0 - TEX_OFF_Y);
          GX_End();
          src++;
        }
      }
      break;
    }
    case 1:
    {
      for(y = 0; y < 25; y++)
      {
        for(x = 0; x < 80; x++)
        {
          GX_SetChanMatColor(GX_COLOR0A0, pal[(src->bg_color & 0xF) * 0x11]);
          GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position2u16(x * 8, y * 14);
            GX_TexCoord2f32(0, 0);
            GX_Position2u16(x * 8 + 8, y * 14);
            GX_TexCoord2f32(0, 0);
            GX_Position2u16(x * 8 + 8, y * 14 + 14);
            GX_TexCoord2f32(0, 0);
            GX_Position2u16(x * 8, y * 14 + 14);
            GX_TexCoord2f32(0, 0);
          GX_End();
          src++;
        }
      }

      GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

      src = graphics->text_video;
      for(y = 0; y < 25; y++)
      {
        for(x = 0; x < 80; x++)
        {
          GX_SetChanMatColor(GX_COLOR0A0, pal[(src->fg_color & 0xF) * 0x11]);
          u = ((int)src->char_value & 0x1f) / 32.0;
          v = ((int)src->char_value >> 5) / 32.0 + 0.5;
          GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position2u16(x * 8, y * 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14 + 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X,
             v + 7 / 256.0 - TEX_OFF_Y);
            GX_Position2u16(x * 8, y * 14 + 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + 7 / 256.0 - TEX_OFF_Y);
          GX_End();
          src++;
        }
      }
      break;
    }
    case 2:
    {
      j = -1;
      for(y = 0; y < 25; y++)
      {
        for(x = 0; x < 80; x++)
        {
          i = ((src->fg_color & 0xF) | (src->bg_color << 4)) & 0xFF;
          if(i != j)
          {
            if(!render_data->smzxtlut[i].pal[1])
            {
              render_data->smzxtlut[i].pal[0] =
               render_data->tlutpal[(i >> 4) * 0x11];
              render_data->smzxtlut[i].pal[5] = render_data->tlutpal[i];
              render_data->smzxtlut[i].pal[10] =
               render_data->tlutpal[((i << 4) | (i >> 4)) & 0xFF];
              render_data->smzxtlut[i].pal[15] =
               render_data->tlutpal[(i & 0xF) * 0x11];
              render_data->smzxtlut[i].pal[1] = 0xFFFF;
              DCFlushRange(render_data->smzxtlut + i, sizeof(ci4tlut));
            }
            GX_LoadTlut(&render_data->smzxtlutobj[i], GX_TLUT0);
            GX_LoadTexObj(&render_data->chartex, GX_TEXMAP0);
            j = i;
          }
          u = ((int)src->char_value & 0x1f) / 32.0;
          v = ((int)src->char_value >> 5) / 32.0 + 0.5;
          GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position2u16(x * 8, y * 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14 + 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X,
             v + 7 / 256.0 - TEX_OFF_Y);
            GX_Position2u16(x * 8, y * 14 + 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + 7 / 256.0 - TEX_OFF_Y);
          GX_End();
          src++;
        }
      }
      break;
    }
    default:
    case 3:
    {
      j = -1;
      for(y = 0; y < 25; y++)
      {
        for(x = 0; x < 80; x++)
        {
          i = (((src->fg_color & 0xF) | (src->bg_color << 4)) & 0xFF) | 0x100;
          if(i != j)
          {
            if(!render_data->smzxtlut[i].pal[1])
            {
              render_data->smzxtlut[i].pal[0] = render_data->tlutpal[i & 0xFF];
              render_data->smzxtlut[i].pal[5] =
               render_data->tlutpal[(i + 2) & 0xFF];
              render_data->smzxtlut[i].pal[10] =
               render_data->tlutpal[(i + 1) & 0xFF];
              render_data->smzxtlut[i].pal[15] =
               render_data->tlutpal[(i + 3) & 0xFF];
              render_data->smzxtlut[i].pal[1] = 0xFFFF;
              DCFlushRange(render_data->smzxtlut + i, sizeof(ci4tlut));
            }
            GX_LoadTlut(&render_data->smzxtlutobj[i], GX_TLUT0);
            GX_LoadTexObj(&render_data->chartex, GX_TEXMAP0);
            j = i;
          }
          u = ((int)src->char_value & 0x1f) / 32.0;
          v = ((int)src->char_value >> 5) / 32.0 + 0.5;
          GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position2u16(x * 8, y * 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X, v + TEX_OFF_Y);
            GX_Position2u16(x * 8 + 8, y * 14 + 14);
            GX_TexCoord2f32(u + 1 / 32.0 - TEX_OFF_X,
             v + 7 / 256.0 - TEX_OFF_Y);
            GX_Position2u16(x * 8, y * 14 + 14);
            GX_TexCoord2f32(u + TEX_OFF_X, v + 7 / 256.0 - TEX_OFF_Y);
          GX_End();
          src++;
        }
      }
      break;
    }
  }

  GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

static void gx_render_cursor(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 color, Uint8 lines, Uint8 offset)
{
  gx_render_data *render_data = graphics->render_data;
  GXColor *pal = render_data->palette;

  GX_SetChanMatColor(GX_COLOR0A0, pal[color]);
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2u16(x * 8, y * 14 + offset);
    GX_TexCoord2f32(0, 0);
    GX_Position2u16(x * 8 + 8, y * 14 + offset);
    GX_TexCoord2f32(0, 0);
    GX_Position2u16(x * 8 + 8, y * 14 + offset + lines);
    GX_TexCoord2f32(0, 0);
    GX_Position2u16(x * 8, y * 14 + offset + lines);
    GX_TexCoord2f32(0, 0);
  GX_End();
}

static void gx_render_mouse(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 w, Uint8 h)
{
  const GXColor white = {255, 255, 255, 255};

  GX_SetBlendMode(GX_BM_BLEND, GX_BL_INVDSTCLR, GX_BL_ZERO, GX_LO_CLEAR);

  GX_SetChanMatColor(GX_COLOR0A0, white);
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2u16(x, y);
    GX_TexCoord2f32(0, 0);
    GX_Position2u16(x + w, y);
    GX_TexCoord2f32(0, 0);
    GX_Position2u16(x + w, y + h);
    GX_TexCoord2f32(0, 0);
    GX_Position2u16(x, y + h);
    GX_TexCoord2f32(0, 0);
  GX_End();
}

static void gx_sync_screen(graphics_data *graphics)
{
  gx_render_data *render_data = graphics->render_data;

  render_data->curfb ^= 1;
  GX_DrawDone();
  GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
  GX_SetColorUpdate(GX_TRUE);
  GX_CopyDisp(render_data->xfb[render_data->curfb], GX_TRUE);
  GX_Flush();
  VIDEO_SetNextFramebuffer(render_data->xfb[render_data->curfb]);
  VIDEO_Flush();
  VIDEO_WaitVSync();
}

void render_gx_register(graphics_data *graphics)
{
  memset(graphics, 0, sizeof(graphics_data));
  graphics->init_video = gx_init_video;
  graphics->check_video_mode = gx_check_video_mode;
  graphics->set_video_mode = gx_set_video_mode;
  graphics->update_colors = gx_update_colors;
  graphics->resize_screen = resize_screen_standard;
  graphics->remap_charsets = gx_remap_charsets;
  graphics->remap_char = gx_remap_char;
  graphics->remap_charbyte = gx_remap_charbyte;
  graphics->get_screen_coords = get_screen_coords_centered;
  graphics->set_screen_coords = set_screen_coords_centered;
  graphics->render_graph = gx_render_graph;
  graphics->render_cursor = gx_render_cursor;
  graphics->render_mouse = gx_render_mouse;
  graphics->sync_screen = gx_sync_screen;
}
