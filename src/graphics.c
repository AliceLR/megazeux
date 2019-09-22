/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 *
 * YUV Renderers:
 *   Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 *
 * OpenGL #2 Renderer:
 *   Copyright (C) 2007 Joel Bouchard Lamontagne <logicow@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "graphics.h"
#include "world.h"
#include "configure.h"
#include "event.h"
#include "render.h"
#include "render_layer.h"
#include "renderers.h"
#include "platform.h"
#include "error.h"

#ifdef CONFIG_PNG
#include "pngops.h"
#endif

#ifdef CONFIG_SDL
#include "SDL.h"
#ifdef CONFIG_ICON
#include "SDL_syswm.h"
#endif // CONFIG_ICON
#include "compat_sdl.h"
#include "render_sdl.h"
#endif // CONFIG_SDL

#include "util.h"

#define CURSOR_BLINK_RATE 115

__editor_maybe_static struct graphics_data graphics;
static boolean graphics_was_initialized;

static const struct renderer_data renderers[] =
{
#if defined(CONFIG_RENDER_SOFT)
  { "software", render_soft_register },
#endif
#if defined(CONFIG_RENDER_SOFTSCALE)
  { "softscale", render_softscale_register },
  { "overlay1", render_softscale_register },
  { "overlay2", render_softscale_register },
#endif
#if defined(CONFIG_RENDER_GL_FIXED)
  { "opengl1", render_gl1_register },
  { "opengl2", render_gl2_register },
#endif
#if defined(CONFIG_RENDER_GL_PROGRAM)
  { "glsl", render_glsl_register },
  { "auto_glsl", render_auto_glsl_register },
#endif
#if defined(CONFIG_RENDER_YUV)
  { "overlay1", render_yuv1_register },
  { "overlay2", render_yuv2_register },
#endif
#if defined(CONFIG_RENDER_GP2X)
  { "gp2x", render_gp2x_register },
#endif
#if defined(CONFIG_NDS)
  { "nds", render_nds_register },
#endif
#if defined(CONFIG_3DS)
  { "3ds", render_ctr_register },
#endif
#if defined(CONFIG_WII)
#if defined(CONFIG_RENDER_GX)
  { "gx", render_gx_register },
#endif
  { "xfb", render_xfb_register },
#endif
  { NULL, NULL }
};

static const struct rgb_color default_pal[16] =
{
  /* r, g, b, unused */
  { 0, 0, 0, 0 },
  { 0, 0, 170, 0 },
  { 0, 170, 0, 0 },
  { 0, 170, 170, 0 },
  { 170, 0, 0, 0 },
  { 170, 0, 170, 0 },
  { 170, 85, 0, 0 },
  { 170, 170, 170, 0 },
  { 85, 85, 85, 0 },
  { 85, 85, 255, 0 },
  { 85, 255, 85, 0 },
  { 85, 255, 255, 0 },
  { 255, 85, 85, 0 },
  { 255, 85, 255, 0 },
  { 255, 255, 85, 0 },
  { 255, 255, 255, 0 }
};

static void remap_charbyte(struct graphics_data *graphics, Uint16 chr,
 Uint8 byte)
{
  if(graphics->renderer.remap_charbyte)
    graphics->renderer.remap_charbyte(graphics, chr, byte);
}

static void remap_char(struct graphics_data *graphics, Uint16 chr)
{
  if(graphics->renderer.remap_char)
    graphics->renderer.remap_char(graphics, chr);
}

static void remap_char_range(struct graphics_data *graphics, Uint16 first,
 Uint16 len)
{
  if(graphics->renderer.remap_char_range)
    graphics->renderer.remap_char_range(graphics, first, len);
}

void ec_change_byte(Uint16 chr, Uint8 byte, Uint8 new_value)
{
  chr = chr % PROTECTED_CHARSET_POSITION;
  graphics.charset[(chr * CHAR_SIZE) + byte] = new_value;

  remap_charbyte(&graphics, chr, byte);
}

void ec_change_char(Uint16 chr, char *matrix)
{
  chr = chr % PROTECTED_CHARSET_POSITION;
  memcpy(graphics.charset + (chr * CHAR_SIZE), matrix, CHAR_SIZE);

  remap_char(&graphics, chr);
}

Uint8 ec_read_byte(Uint16 chr, Uint8 byte)
{
  chr = chr % PROTECTED_CHARSET_POSITION;
  return graphics.charset[(chr * CHAR_SIZE) + byte];
}

void ec_read_char(Uint16 chr, char *matrix)
{
  chr = chr % PROTECTED_CHARSET_POSITION;
  memcpy(matrix, graphics.charset + (chr * CHAR_SIZE), CHAR_SIZE);
}

void ec_clear_set(void)
{
  if(layer_renderer_check(false))
  {
    memset(graphics.charset, 0, PROTECTED_CHARSET_POSITION * CHAR_SIZE);
    remap_char_range(&graphics, 0, FULL_CHARSET_SIZE);
  }

  // For compatibility with old renderers, clear only the first charset;
  // the second could be a duplicate of the protected set.
  else
  {
    memset(graphics.charset, 0, CHARSET_SIZE * CHAR_SIZE);
    remap_char_range(&graphics, 0, CHARSET_SIZE);
  }
}

Sint32 ec_load_set(char *name)
{
  FILE *fp = fopen_unsafe(name, "rb");
  Uint16 count;

  if(fp)
  {
    count = fread(graphics.charset, CHAR_SIZE, PROTECTED_CHARSET_POSITION, fp);
    fclose(fp);

    if(count > 0)
    {
      // some renderers may want to map charsets to textures
      remap_char_range(&graphics, 0, count);
      return 0;
    }
  }
  return -1;
}

__editor_maybe_static void ec_load_set_secondary(const char *name,
 Uint8 *dest)
{
  FILE *fp = fopen_unsafe(name, "rb");
  Uint16 count;

  if(fp)
  {
    count = fread(dest, CHAR_SIZE, CHARSET_SIZE, fp);
    fclose(fp);

    // This might have been somewhere in the charset, so
    // some renderers may want to map charsets to textures
    if(count > 0)
      remap_char_range(&graphics, 0, FULL_CHARSET_SIZE);
  }
}

Sint32 ec_load_set_var(char *name, Uint16 pos, int version)
{
  Uint32 size = CHARSET_SIZE;
  FILE *fp = fopen_unsafe(name, "rb");
  Uint32 maxChars = PROTECTED_CHARSET_POSITION;
  Uint16 count;

  if(version < V290)
    maxChars = 256;

  if(fp)
  {
    size = ftell_and_rewind(fp) / CHAR_SIZE;
    if(size + pos >= 256 && maxChars > 256 && !layer_renderer_check(true))
      maxChars = 256;

    if(size + pos > maxChars)
      size = maxChars - pos;

    count = fread(graphics.charset + (pos * CHAR_SIZE), CHAR_SIZE, size, fp);
    fclose(fp);

    // some renderers may want to map charsets to textures
    if(count > 0)
      remap_char_range(&graphics, pos, count);

    return count;
  }
  return -1;
}

void ec_mem_load_set(Uint8 *chars, size_t len)
{
  // This is used only for legacy and ZIP world loading and the default charsets
  // Use ec_clear_set() in conjunction with this for world loads.
  Uint16 count;

  if(!layer_renderer_check(false) && len > CHAR_SIZE * CHARSET_SIZE)
    len = CHAR_SIZE * CHARSET_SIZE;

  if(len > CHAR_SIZE * PROTECTED_CHARSET_POSITION)
    len = CHAR_SIZE * PROTECTED_CHARSET_POSITION;

  memcpy(graphics.charset, chars, len);

  // some renderers may want to map charsets to textures
  count = len / CHAR_SIZE;
  if(count > 0)
    remap_char_range(&graphics, 0, count);
}

void ec_mem_save_set(Uint8 *chars)
{
  memcpy(chars, graphics.charset, CHAR_SIZE * CHARSET_SIZE);
}

void ec_mem_load_set_var(char *chars, size_t len, Uint16 pos, int version)
{
  Uint32 maxChars = PROTECTED_CHARSET_POSITION;
  Uint32 offset = pos * CHAR_SIZE;
  Uint16 count = (len + CHAR_SIZE - 1) / CHAR_SIZE;

  if(version < V290)
    maxChars = 256;

  if(len + offset > (256 * CHAR_SIZE) && !layer_renderer_check(true))
    maxChars = 256;

  if(pos > maxChars)
    return;

  if(count > maxChars - pos)
  {
    count = maxChars - pos;
    len = count * CHAR_SIZE;
  }

  memcpy(graphics.charset + offset, chars, len);

  // some renderers may want to map charsets to textures
  if(count > 0)
    remap_char_range(&graphics, pos, count);
}

void ec_mem_save_set_var(Uint8 *chars, size_t len, Uint16 pos)
{
  Uint32 offset = pos * CHAR_SIZE;
  Uint32 size = MIN(PROTECTED_CHARSET_POSITION * CHAR_SIZE - offset, len);

  if(pos < PROTECTED_CHARSET_POSITION)
    memcpy(chars, graphics.charset + offset, size);
}

__editor_maybe_static void ec_load_mzx(void)
{
  ec_mem_load_set(graphics.default_charset, CHAR_SIZE * CHARSET_SIZE);
}

static void update_colors(struct rgb_color *palette, Uint32 count)
{
  graphics.renderer.update_colors(&graphics, palette, count);
}

static Uint32 make_palette(struct rgb_color *palette)
{
  Uint32 i;
  Uint32 paletteSize;

  // Is SMZX mode set?
  if(graphics.screen_mode)
  {
    // SMZX mode 1: set all colors to diagonals
    if(graphics.screen_mode == 1)
    {
      for(i = 0; i < SMZX_PAL_SIZE; i++)
      {
        palette[i].r =
         ((graphics.intensity_palette[i & 15].r << 1) +
          graphics.intensity_palette[i >> 4].r) / 3;
        palette[i].g =
         ((graphics.intensity_palette[i & 15].g << 1) +
          graphics.intensity_palette[i >> 4].g) / 3;
        palette[i].b =
         ((graphics.intensity_palette[i & 15].b << 1) +
          graphics.intensity_palette[i >> 4].b) / 3;
      }
    }
    else
    {
      memcpy(palette, graphics.intensity_palette,
       sizeof(struct rgb_color) * SMZX_PAL_SIZE);
    }
    paletteSize = SMZX_PAL_SIZE;
  }
  else
  {
    memcpy(palette, graphics.intensity_palette,
     sizeof(struct rgb_color) * PAL_SIZE);
    paletteSize = PAL_SIZE;
  }
  memcpy(palette + paletteSize, graphics.protected_palette,
   sizeof(struct rgb_color) * PROTECTED_PAL_SIZE);
  graphics.protected_pal_position = paletteSize;

  paletteSize += PROTECTED_PAL_SIZE;
  if(paletteSize > 256 && !layer_renderer_check(false))
  {
    // Renderers without layer support often have poor upport for large
    // palettes, so to be safe we restrict the palette size
    paletteSize = 256;
  }
  return paletteSize;
}

void update_palette(void)
{
  struct rgb_color new_palette[FULL_PAL_SIZE];
  update_colors(new_palette, make_palette(new_palette));
}

static void init_palette(void)
{
  Uint32 i;

  memcpy(graphics.palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.protected_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.intensity_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memset(graphics.current_intensity, 0,
   sizeof(Uint32) * PAL_SIZE);

  for(i = 0; i < SMZX_PAL_SIZE; i++)
    graphics.saved_intensity[i] = 100;

  graphics.fade_status = 1;
  graphics.palette_dirty = true;
}

static int intensity(int component, int percent)
{
  component = (component * percent) / 100;

  if(component > 255)
    component = 255;

  return component;
}

void set_color_intensity(Uint32 color, Uint32 percent)
{
  if(graphics.fade_status)
  {
    graphics.saved_intensity[color] = percent;
  }
  else
  {
    int r = intensity(graphics.palette[color].r, percent);
    int g = intensity(graphics.palette[color].g, percent);
    int b = intensity(graphics.palette[color].b, percent);

    graphics.intensity_palette[color].r = r;
    graphics.intensity_palette[color].g = g;
    graphics.intensity_palette[color].b = b;

    graphics.current_intensity[color] = percent;
    graphics.palette_dirty = true;
  }
}

void set_palette_intensity(Uint32 percent)
{
  Uint32 i, num_colors;

  if(graphics.screen_mode >= 2)
    num_colors = SMZX_PAL_SIZE;
  else
    num_colors = PAL_SIZE;

  for(i = 0; i < num_colors; i++)
  {
    set_color_intensity(i, percent);
  }
  graphics.palette_dirty = true;
}

void set_rgb(Uint32 color, Uint32 r, Uint32 g, Uint32 b)
{
  int percent = graphics.current_intensity[color];
  r = r * 255 / 63;
  g = g * 255 / 63;
  b = b * 255 / 63;

  graphics.palette[color].r = r;
  graphics.intensity_palette[color].r = intensity(r, percent);

  graphics.palette[color].g = g;
  graphics.intensity_palette[color].g = intensity(g, percent);

  graphics.palette[color].b = b;
  graphics.intensity_palette[color].b = intensity(b, percent);
  graphics.palette_dirty = true;
}

void set_protected_rgb(Uint32 color, Uint32 r, Uint32 g, Uint32 b)
{
  r = r * 255 / 63;
  g = g * 255 / 63;
  b = b * 255 / 63;
  graphics.protected_palette[color].r = r;
  graphics.protected_palette[color].g = g;
  graphics.protected_palette[color].b = b;
  graphics.palette_dirty = true;
}

void set_red_component(Uint32 color, Uint32 r)
{
  int percent = graphics.current_intensity[color];
  r = r * 255 / 63;

  graphics.palette[color].r = r;
  graphics.intensity_palette[color].r = intensity(r, percent);
  graphics.palette_dirty = true;
}

void set_green_component(Uint32 color, Uint32 g)
{
  int percent = graphics.current_intensity[color];
  g = g * 255 / 63;

  graphics.palette[color].g = g;
  graphics.intensity_palette[color].g = intensity(g, percent);
  graphics.palette_dirty = true;
}

void set_blue_component(Uint32 color, Uint32 b)
{
  int percent = graphics.current_intensity[color];
  b = b * 255 / 63;

  graphics.palette[color].b = b;
  graphics.intensity_palette[color].b = intensity(b, percent);
  graphics.palette_dirty = true;
}

static Uint32 get_smzx_index_offset(Uint32 color, Uint32 index)
{
  index %= 4;

  if(index == 1)
    index = 2;
  else if(index == 2)
    index = 1;

  return (color % SMZX_PAL_SIZE) * 4 + index;
}

Uint32 get_smzx_index(Uint32 col, Uint32 offset)
{
  offset = get_smzx_index_offset(col, offset);

  return graphics.smzx_indices[offset];
}

void set_smzx_index(Uint32 col, Uint32 offset, Uint32 value)
{
  // Setting the SMZX index is only supported for mode 3
  if(graphics.screen_mode != 3)
    return;

  offset = get_smzx_index_offset(col, offset);

  graphics.smzx_indices[offset] = value % SMZX_PAL_SIZE;
  graphics.palette_dirty = true;
}

Uint32 get_color_intensity(Uint32 color)
{
  if(graphics.fade_status)
    return graphics.saved_intensity[color];
  return graphics.current_intensity[color];
}

void get_rgb(Uint32 color, Uint8 *r, Uint8 *g, Uint8 *b)
{
  *r = ((graphics.palette[color].r * 126) + 255) / 510;
  *g = ((graphics.palette[color].g * 126) + 255) / 510;
  *b = ((graphics.palette[color].b * 126) + 255) / 510;
}

Uint32 get_red_component(Uint32 color)
{
  return ((graphics.palette[color].r * 126) + 255) / 510;
}

Uint32 get_green_component(Uint32 color)
{
  return ((graphics.palette[color].g * 126) + 255) / 510;
}

Uint32 get_blue_component(Uint32 color)
{
  return ((graphics.palette[color].b * 126) + 255) / 510;
}

Uint32 get_color_luma(Uint32 color)
{
  int r = graphics.palette[color].r;
  int g = graphics.palette[color].g;
  int b = graphics.palette[color].b;

  return (Uint32)((r * .299) + (g * .587) + (b * .114) + .5);
}

void load_palette(const char *fname)
{
  int file_size, i, r, g, b;
  FILE *pal_file;

  pal_file = fopen_unsafe(fname, "rb");
  if(!pal_file)
    return;

  file_size = ftell_and_rewind(pal_file);

  switch(graphics.screen_mode)
  {
    // Regular text mode, 16 colors
    case 0:
      file_size = MIN(file_size, 16 * 3);
      break;

    // SMZX modes, up to 256 colors
    default:
      file_size = MIN(file_size, 256 * 3);
      break;
  }

  for(i = 0; i < file_size / 3; i++)
  {
    r = fgetc(pal_file);
    g = fgetc(pal_file);
    b = fgetc(pal_file);
    set_rgb(i, r, g, b);
  }

  fclose(pal_file);
}

void load_palette_mem(char *pal, size_t len)
{
  int size, r, g, b, i, j;

  switch(graphics.screen_mode)
  {
    // Regular text mode
    case 0:
      size = MIN(len, 16*3);
      break;

    // SMZX
    default:
      size = MIN(len, 256*3);
      break;
  }

  for(i = 0, j = 0; j+2 < size; i++)
  {
    r = pal[j++];
    g = pal[j++];
    b = pal[j++];
    set_rgb(i, r, g, b);
  }
}

void load_index_file(const char *fname)
{
  FILE *idx_file;
  int i;

  if(get_screen_mode() != 3)
    return;

  idx_file = fopen_unsafe(fname, "rb");
  if(idx_file)
  {
    for(i = 0; i < SMZX_PAL_SIZE; i++)
    {
      set_smzx_index(i, 0, fgetc(idx_file));
      set_smzx_index(i, 1, fgetc(idx_file));
      set_smzx_index(i, 2, fgetc(idx_file));
      set_smzx_index(i, 3, fgetc(idx_file));
    }

    fclose(idx_file);
  }
}

void save_indices(void *buffer)
{
  Uint8 *copy_buffer = buffer;
  int i;

  if(get_screen_mode() != 3)
    return;

  for(i = 0; i < SMZX_PAL_SIZE; i++)
  {
    *(copy_buffer++) = get_smzx_index(i, 0);
    *(copy_buffer++) = get_smzx_index(i, 1);
    *(copy_buffer++) = get_smzx_index(i, 2);
    *(copy_buffer++) = get_smzx_index(i, 3);
  }
}

void load_indices(void *buffer, size_t size)
{
  Uint8 *copy_buffer = buffer;
  unsigned int i;

  if(get_screen_mode() != 3)
    return;

  if(size > SMZX_PAL_SIZE)
    size = SMZX_PAL_SIZE;

  // Truncate incomplete colors
  size -= (size & 3);

  for(i = 0; i < size; i++)
  {
    set_smzx_index(i, 0, *(copy_buffer++));
    set_smzx_index(i, 1, *(copy_buffer++));
    set_smzx_index(i, 2, *(copy_buffer++));
    set_smzx_index(i, 3, *(copy_buffer++));
  }
}

void load_indices_direct(void *buffer, size_t size)
{
  memcpy(graphics.smzx_indices, buffer, size);
  graphics.palette_dirty = true;
}

void smzx_palette_loaded(int val)
{
  graphics.default_smzx_loaded = val;
}

static void update_intensity_palette(void)
{
  int i;
  for(i = 0; i < SMZX_PAL_SIZE; i++)
  {
    set_color_intensity(i, graphics.current_intensity[i]);
  }
}

static void swap_palettes(void)
{
  struct rgb_color temp_colors[SMZX_PAL_SIZE];
  Uint32 temp_intensities[SMZX_PAL_SIZE];
  memcpy(temp_colors, graphics.backup_palette,
   sizeof(struct rgb_color) * SMZX_PAL_SIZE);
  memcpy(graphics.backup_palette, graphics.palette,
   sizeof(struct rgb_color) * SMZX_PAL_SIZE);
  memcpy(graphics.palette, temp_colors,
   sizeof(struct rgb_color) * SMZX_PAL_SIZE);
  memcpy(temp_intensities, graphics.backup_intensity,
   sizeof(Uint32) * SMZX_PAL_SIZE);
  if(graphics.fade_status)
  {
    memcpy(graphics.backup_intensity,
     graphics.saved_intensity, sizeof(Uint32) * SMZX_PAL_SIZE);
    memcpy(graphics.saved_intensity, temp_intensities,
     sizeof(Uint32) * SMZX_PAL_SIZE);
  }
  else
  {
    memcpy(graphics.backup_intensity,
     graphics.current_intensity, sizeof(Uint32) * SMZX_PAL_SIZE);
    memcpy(graphics.current_intensity, temp_intensities,
     sizeof(Uint32) * SMZX_PAL_SIZE);
    update_intensity_palette();
  }
}

Uint32 get_fade_status(void)
{
  return graphics.fade_status;
}

void dialog_fadein(void)
{
  graphics.dialog_fade_status = get_fade_status();
  if(graphics.dialog_fade_status)
  {
    clear_screen();
    insta_fadein();
  }
}

void dialog_fadeout(void)
{
  if(graphics.dialog_fade_status)
  {
    insta_fadeout();
  }
}

static void fix_layer_screen_mode(void)
{
  // Fix the screen mode for all active layers except the UI_LAYER.
  Uint32 i;
  for(i = 0; i < graphics.layer_count; i++)
    graphics.video_layers[i].mode = graphics.screen_mode;

  graphics.video_layers[UI_LAYER].mode = 0;
}

void set_screen_mode(Uint32 mode)
{
  int i;
  char *pal_idx;
  char bg, fg;
  mode %= 4;

  if((mode >= 2) && (graphics.screen_mode < 2))
  {
    swap_palettes();
    graphics.screen_mode = mode;
    if(!graphics.default_smzx_loaded)
    {
      if(graphics.fade_status)
      {
        for(i = 0; i < SMZX_PAL_SIZE; i++)
        {
          graphics.current_intensity[i] = 0;
        }
      }
      set_palette_intensity(100);
      load_palette(mzx_res_get_by_id(SMZX_PAL));
      graphics.default_smzx_loaded = 1;
    }
  }
  else

  if((graphics.screen_mode >= 2) && (mode < 2))
  {
    swap_palettes();

    graphics.screen_mode = mode;
  }
  else
  {
    graphics.screen_mode = mode;
  }

  pal_idx = graphics.smzx_indices;
  if(mode == 1 || mode == 2)
  {
    for(i = 0; i < 256; i++)
    {
      bg = (i & 0xF0) >> 4;
      fg = i & 0x0F;
      pal_idx[0] = (bg << 4) | bg;
      pal_idx[1] = (bg << 4) | fg;
      pal_idx[2] = (fg << 4) | bg;
      pal_idx[3] = (fg << 4) | fg;
      pal_idx += 4;
    }
  }
  else

  if(mode == 3)
  {
    for(i = 0; i < 256; i++)
    {
      pal_idx[0] = (i + 0) & 0xFF;
      pal_idx[1] = (i + 2) & 0xFF;
      pal_idx[2] = (i + 1) & 0xFF;
      pal_idx[3] = (i + 3) & 0xFF;
      pal_idx += 4;
    }
  }

  fix_layer_screen_mode();
  graphics.palette_dirty = true;
}

Uint32 get_screen_mode(void)
{
  return graphics.screen_mode;
}

/**
 * The cursor needs to be reasonably visible whenever possible. A good deal of
 * the time this is possible with the classic cursor behavior of using the
 * foreground color of whatever is beneath it. For completely solid chars, the
 * background color is instead used.
 *
 * This behavior doesn't work very well when the foreground and background
 * colors are very close (modes 0 and 1), so instead we switch the cursor to
 * protected white or black. Foreground and background have no meaning in modes
 * 2 and 3, so always use protected white or black in these modes.
 */

static Uint16 get_cursor_color(void)
{
  struct char_element *cursor_element =
   graphics.text_video + graphics.cursor_x + (graphics.cursor_y * SCREEN_W);
  Uint16 cursor_color;
  Uint32 cursor_char = cursor_element->char_value;
  Uint32 fg_color = cursor_element->fg_color;
  Uint32 bg_color = cursor_element->bg_color;
  int i;

  if(bg_color >= 0x10)
  {
    // UI- use protected palette white or black.
    if(bg_color <= 0x19)
      cursor_color = graphics.protected_pal_position + 0x0F;

    else
      cursor_color = graphics.protected_pal_position;
  }
  else

  if(graphics.screen_mode <= 1)
  {
    // Modes 0 and 1- use the (modified) classic cursor color logic.
    Uint32 *offset = (Uint32 *)(graphics.charset + cursor_char * CHAR_SIZE);
    Uint32 cursor_solid = 0xFFFFFFFF;

    // Choose FG by default.
    cursor_color = fg_color;

    // If the char under the cursor is completely solid, use the BG instead.
    for(i = 0; i < 3; i++)
    {
      cursor_solid &= *offset;
      offset++;
    }
    cursor_solid &= (*((Uint16 *)offset)) | 0xFFFF0000;

    if(cursor_solid == 0xFFFFFFFF)
      cursor_color = bg_color;

    if(fg_color < 0x10 && bg_color < 0x10)
    {
      // If the fg and bg colors are game colors and close in brightness/the
      // same, neither color fits well, so choose a protected palette color.
      int fg_luma = get_color_luma(fg_color);
      int bg_luma = get_color_luma(bg_color);

      if(abs(fg_luma - bg_luma) < 32)
      {
        cursor_color = graphics.protected_pal_position;

        if(fg_luma + bg_luma < 256)
          cursor_color |= 0x0F;
      }
    }

    // Offset adjust protected colors if necessary.
    if(cursor_color >= 0x10)
      cursor_color = graphics.protected_pal_position + (cursor_color & 0x0F);

    // Offset adjust mode 1 colors if necessary.
    else if(graphics.screen_mode == 1)
      cursor_color = (cursor_color << 4) | (cursor_color & 0x0F);
  }

  else
  {
    // Modes 2 and 3- read the entire char and pick protected white or black.
    Uint8 char_buffer[CHAR_W * CHAR_H];
    Uint32 sum = 0;
    Uint8 pal;

    bg_color &= 0x0F;
    fg_color &= 0x0F;
    pal = (bg_color << 4) | fg_color;

    dump_char(cursor_char, pal, graphics.screen_mode, char_buffer);

    for(i = 0; i < CHAR_W * CHAR_H; i += 2)
      sum += get_color_luma(char_buffer[i]);

    cursor_color = graphics.protected_pal_position;

    if(sum < 128 * (CHAR_W * CHAR_H / 2))
      cursor_color |= 0x0F;
  }

  return cursor_color;
}

static int compare_layers(const void *a, const void *b)
{
  return (*(struct video_layer * const *)a)->draw_order -
    (*(struct video_layer * const *)b)->draw_order;
}

void update_screen(void)
{
  Uint32 ticks = get_ticks();
  Uint32 layer;

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  if(graphics.palette_dirty)
  {
    update_palette();
    graphics.palette_dirty = false;
  }

  if(graphics.requires_extended && graphics.renderer.render_layer)
  {
    for(layer = 0; layer < graphics.layer_count; layer++)
    {
      graphics.sorted_video_layers[layer] = &graphics.video_layers[layer];
    }

    qsort(graphics.sorted_video_layers, graphics.layer_count,
     sizeof(struct video_layer *), compare_layers);

    for(layer = 0; layer < graphics.layer_count; layer++)
    {
      if(graphics.sorted_video_layers[layer]->data &&
       !graphics.sorted_video_layers[layer]->empty)
        graphics.renderer.render_layer(&graphics,
         graphics.sorted_video_layers[layer]);
    }
  }
  else

  if(graphics.renderer.render_graph)
  {
    // Fallback if the layer renderer is unavailable or unnecessary
    graphics.renderer.render_graph(&graphics);
  }
  else

  if(graphics.renderer.render_layer)
  {
    // Fallback using the layer renderer
    graphics.text_video_layer.mode = graphics.screen_mode;
    graphics.text_video_layer.data = graphics.text_video;

    graphics.renderer.render_layer(&graphics, &(graphics.text_video_layer));
  }

  if(graphics.cursor_flipflop &&
   (graphics.cursor_mode != cursor_mode_invisible))
  {
    Uint16 cursor_color = get_cursor_color();
    Uint32 lines = 0;
    Uint32 offset = 0;

    switch(graphics.cursor_mode)
    {
      case cursor_mode_underline:
        lines = 2;
        offset = 12;
        break;
      case cursor_mode_solid:
        lines = 14;
        offset = 0;
        break;
      case cursor_mode_invisible:
        break;
    }

    graphics.renderer.render_cursor(&graphics,
     graphics.cursor_x, graphics.cursor_y, cursor_color, lines, offset);
  }

  if(graphics.mouse_status)
  {
    int mouse_x, mouse_y;
    get_real_mouse_position(&mouse_x, &mouse_y);

    mouse_x = (mouse_x / graphics.mouse_width_mul) * graphics.mouse_width_mul;
    mouse_y = (mouse_y / graphics.mouse_height_mul) * graphics.mouse_height_mul;

    graphics.renderer.render_mouse(&graphics, mouse_x, mouse_y,
     graphics.mouse_width_mul, graphics.mouse_height_mul);
  }

  graphics.renderer.sync_screen(&graphics);
}

// Very quick fade out. Saves intensity table for fade in. Be sure
// to use in conjuction with the next function.
void vquick_fadeout(void)
{
  if(!has_video_initialized())
  {
    // If we're running without video there's no point waiting 11 frames.
    insta_fadeout();
    return;
  }

  if(!graphics.fade_status)
  {
    Sint32 i, i2, num_colors;
    Uint32 ticks;

    if(graphics.screen_mode >= 2)
      num_colors = SMZX_PAL_SIZE;
    else
      num_colors = PAL_SIZE;

    memcpy(graphics.saved_intensity, graphics.current_intensity,
     sizeof(Uint32) * num_colors);

    for(i = 10; i >= 0; i--)
    {
      ticks = get_ticks();

      for(i2 = 0; i2 < num_colors; i2++)
        set_color_intensity(i2, (graphics.saved_intensity[i2] * i / 10));

      graphics.palette_dirty = true;
      update_screen();

      ticks = get_ticks() - ticks;
      if(ticks <= 16)
        delay(16 - ticks);
    }
    graphics.fade_status = 1;
  }
}

// Very quick fade in. Uses intensity table saved from fade out. For
// use in conjuction with the previous function.
void vquick_fadein(void)
{
  if(!has_video_initialized())
  {
    // If we're running without video there's no point waiting 11 frames.
    insta_fadein();
    return;
  }

  if(graphics.fade_status)
  {
    Uint32 i, i2, num_colors;
    Uint32 ticks;

    graphics.fade_status = 0;

    if(graphics.screen_mode >= 2)
      num_colors = SMZX_PAL_SIZE;
    else
      num_colors = PAL_SIZE;

    for(i = 0; i <= 10; i++)
    {
      ticks = get_ticks();

      for(i2 = 0; i2 < num_colors; i2++)
        set_color_intensity(i2, (graphics.saved_intensity[i2] * i / 10));

      graphics.palette_dirty = true;
      update_screen();

      ticks = get_ticks() - ticks;
      if(ticks <= 16)
        delay(16 - ticks);
    }
  }
}

// Instant fade out
void insta_fadeout(void)
{
  Uint32 i, num_colors;

  if(graphics.fade_status)
    return;

  if(graphics.screen_mode >= 2)
    num_colors = SMZX_PAL_SIZE;
  else
    num_colors = PAL_SIZE;

  memcpy(graphics.saved_intensity, graphics.current_intensity,
   sizeof(Uint32) * num_colors);

  for(i = 0; i < num_colors; i++)
    set_color_intensity(i, 0);

  graphics.palette_dirty = true;
  update_screen(); // NOTE: this was called conditionally in 2.81e

  graphics.fade_status = true;
}

// Instant fade in
void insta_fadein(void)
{
  Uint32 i, num_colors;

  if(!graphics.fade_status)
    return;

  graphics.fade_status = false;

  if(graphics.screen_mode >= 2)
    num_colors = SMZX_PAL_SIZE;
  else
    num_colors = PAL_SIZE;

  for(i = 0; i < num_colors; i++)
    set_color_intensity(i, graphics.saved_intensity[i]);

  graphics.palette_dirty = true;
  update_screen(); // NOTE: this was called conditionally in 2.81e
}

void default_palette(void)
{
  memcpy(graphics.palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.intensity_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  graphics.palette_dirty = true;
}

void default_protected_palette(void)
{
  memcpy(graphics.protected_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  graphics.palette_dirty = true;
}

static boolean set_graphics_output(struct config_info *conf)
{
  const char *video_output = conf->video_output;
  const struct renderer_data *renderer = renderers;
  int i = 0;

  // The first renderer was NULL, this shouldn't happen
  if(!renderer->name)
  {
    warn("No renderers built, please provide a valid config.h!\n");
    return false;
  }

  while(renderer->name)
  {
    if(!strcasecmp(video_output, renderer->name))
      break;
    renderer++;
    i++;
  }

  // If no match found, use first renderer in the renderer list
  if(!renderer->name)
  {
    renderer = renderers;
    i = 0;
  }

  renderer->reg(&graphics.renderer);
  graphics.renderer_num = i;

  debug("Video: using '%s' renderer.\n", renderer->name);
  return true;
}

#if defined(CONFIG_PNG) && defined(CONFIG_SDL) && \
    defined(CONFIG_ICON) && !defined(__WIN32__)

static boolean icon_w_h_constraint(png_uint_32 w, png_uint_32 h)
{
  // Icons must be multiples of 16 and square
  return (w == h) && ((w % 16) == 0) && ((h % 16) == 0);
}

static void *sdl_alloc_rgba_surface(png_uint_32 w, png_uint_32 h,
 png_uint_32 *stride, void **pixels)
{
  Uint32 rmask, gmask, bmask, amask;
  SDL_Surface *s;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  rmask = 0xff000000;
  gmask = 0x00ff0000;
  bmask = 0x0000ff00;
  amask = 0x000000ff;
#else
  rmask = 0x000000ff;
  gmask = 0x0000ff00;
  bmask = 0x00ff0000;
  amask = 0xff000000;
#endif

  s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, rmask, gmask, bmask, amask);
  if(!s)
    return NULL;

  *stride = s->pitch;
  *pixels = s->pixels;
  return s;
}

static SDL_Surface *png_read_icon(const char *name)
{
  return png_read_file(name, NULL, NULL, icon_w_h_constraint,
   sdl_alloc_rgba_surface);
}

#endif // CONFIG_PNG && CONFIG_SDL && CONFIG_ICON && !__WIN32__

static void set_window_grab(boolean grabbed)
{
#ifdef CONFIG_SDL
  SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
  SDL_SetWindowGrab(window, grabbed ? SDL_TRUE : SDL_FALSE);
#endif
}

void set_window_caption(const char *caption)
{
#ifdef CONFIG_SDL
  SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
  SDL_SetWindowTitle(window, caption);
#endif
}

char *get_default_caption(void)
{
  return graphics.default_caption;
}

static void set_window_icon(void)
{
#if defined(CONFIG_SDL) && defined(CONFIG_ICON)
#ifdef __WIN32__
  {
    /* Roll our own icon code; the SDL stuff is a mess and for some
     * reason limits us to 256 colour icons (with keying but no alpha).
     *
     * We also pull off some nifty WIN32 hackery to load the executable's
     * icon resource; saves having an external dependency.
     */
    HICON icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    if(icon)
    {
      SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
      SDL_SysWMinfo info;

      SDL_VERSION(&info.version);
      SDL_GetWindowWMInfo(window, &info);

      SendMessage(SDL_SysWMinfo_GetWND(&info),
       WM_SETICON, ICON_BIG, (LPARAM)icon);
    }
  }
#else // !__WIN32__
#if defined(CONFIG_PNG) && defined(ICONFILE)
  {
    SDL_Surface *icon = png_read_icon(ICONFILE);
    if(icon)
    {
      SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
      SDL_SetWindowIcon(window, icon);
      SDL_FreeSurface(icon);
    }
  }
#endif // CONFIG_PNG
#endif // __WIN32__
#endif // CONFIG_SDL && CONFIG_ICON
}

static void new_empty_layer(struct video_layer *layer, int x, int y, Uint32 w,
 Uint32 h, int draw_order)
{
  // Layers are persistent and static
  if(!layer->data || layer->w != w || layer->h != h)
    layer->data = crealloc(layer->data, sizeof(struct char_element) * w * h);

  layer->w = w;
  layer->h = h;
  layer->x = x;
  layer->y = y;
  layer->mode = graphics.screen_mode;
  layer->draw_order = draw_order;
  layer->transparent_col = -1;
  layer->offset = 0;
}

Uint32 create_layer(int x, int y, Uint32 w, Uint32 h, int draw_order, int t_col,
 int offset, boolean unbound)
{
  Uint32 layer_idx = graphics.layer_count;
  struct video_layer *layer = &graphics.video_layers[layer_idx];

  new_empty_layer(layer, x, y, w, h, draw_order);
  memset(layer->data, 0xFF, sizeof(struct char_element) * w * h);
  layer->empty = true;
  layer->transparent_col = t_col;
  layer->offset = offset;
  graphics.layer_count++;

  if(!graphics.requires_extended && unbound)
    graphics.requires_extended = true;

  return layer_idx;
}

void set_layer_offset(Uint32 layer, int offset)
{
  graphics.video_layers[layer].offset = offset;
}

void set_layer_mode(Uint32 layer, int mode)
{
  // In general, we want the layer to use the screen mode, but some
  // UI elements need to be able to change this.
  graphics.video_layers[layer].mode = mode;
}

void move_layer(Uint32 layer, int x, int y)
{
  graphics.video_layers[layer].x = x;
  graphics.video_layers[layer].y = y;
  if(!graphics.requires_extended && (x % CHAR_W != 0 || y % CHAR_H != 0))
    graphics.requires_extended = true;
}

static void init_layers(void)
{
  new_empty_layer(&graphics.video_layers[BOARD_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_BOARD);
  new_empty_layer(&graphics.video_layers[OVERLAY_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_OVERLAY);
  new_empty_layer(&graphics.video_layers[GAME_UI_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_GAME_UI);
  new_empty_layer(&graphics.video_layers[UI_LAYER],
   0, 0, SCREEN_W, SCREEN_H, LAYER_DRAWORDER_UI);

  select_layer(UI_LAYER);

  graphics.layer_count = NUM_DEFAULT_LAYERS;
  graphics.layer_count_prev = graphics.layer_count;
  blank_layers();
}

void select_layer(Uint32 layer)
{
  graphics.current_layer = layer;
  graphics.current_video = graphics.video_layers[layer].data;
}

void blank_layers(void)
{
  // This clears the default layers and deletes all other layers

  memset(graphics.video_layers[BOARD_LAYER].data, 0x00,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);
  memset(graphics.video_layers[OVERLAY_LAYER].data, 0xFF,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);
  memset(graphics.video_layers[GAME_UI_LAYER].data, 0xFF,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);
  memset(graphics.video_layers[UI_LAYER].data, 0xFF,
   sizeof(struct char_element) * SCREEN_W * SCREEN_H);

  graphics.video_layers[BOARD_LAYER].empty = false;
  graphics.video_layers[OVERLAY_LAYER].empty = true;
  graphics.video_layers[GAME_UI_LAYER].empty = true;
  graphics.video_layers[UI_LAYER].empty = true;

  // Delete the rest of the layers
  destruct_extra_layers(0);

  // Since the layers were cleared, their screen mode values need to be reset.
  fix_layer_screen_mode();
}

void destruct_extra_layers(Uint32 first)
{
  // Delete layers that have not persisted since the previous frame and
  // make all extra layers available for use.
  Uint32 i;

  if(first < NUM_DEFAULT_LAYERS)
    first = NUM_DEFAULT_LAYERS;

  if(graphics.layer_count_prev > graphics.layer_count)
  {
    for(i = graphics.layer_count; i < graphics.layer_count_prev; i++)
    {
      free(graphics.video_layers[i].data);
      graphics.video_layers[i].data = NULL;
    }
  }
  graphics.layer_count_prev = graphics.layer_count;

  if(graphics.layer_count > first)
    graphics.layer_count = first;

  // Extended graphics may be no longer needed after destroying layers
  if(graphics.layer_count == NUM_DEFAULT_LAYERS)
    graphics.requires_extended = false;
}

void destruct_layers(void)
{
  Uint32 i;
  for(i = 0; i < TEXTVIDEO_LAYERS; i++)
  {
    if(graphics.video_layers[i].data)
    {
      free(graphics.video_layers[i].data);
      graphics.video_layers[i].data = NULL;
    }
  }
  graphics.layer_count_prev = 0;
  graphics.layer_count = 0;
}

static void dirty_ui(void)
{
  if(graphics.requires_extended) return;
  if(graphics.current_layer != UI_LAYER) return;
  if(graphics.screen_mode == 0) return;
  graphics.requires_extended = true;
}

static void dirty_current(void)
{
  graphics.video_layers[graphics.current_layer].empty = false;
}

static int offset_adjust(int offset)
{
  // Transform the given offset from screen space to layer space
  struct video_layer *layer;
  int x, y;
  layer = &graphics.video_layers[graphics.current_layer];
  x = (offset % SCREEN_W) - (layer->x / CHAR_W);
  y = (offset / SCREEN_W) - (layer->y / CHAR_H);
  return y * layer->w + x;
}

boolean init_video(struct config_info *conf, const char *caption)
{
  graphics.screen_mode = 0;
  graphics.fullscreen = conf->fullscreen;
  graphics.fullscreen_windowed = conf->fullscreen_windowed;
  graphics.resolution_width = conf->resolution_width;
  graphics.resolution_height = conf->resolution_height;
  graphics.window_width = conf->window_width;
  graphics.window_height = conf->window_height;
  graphics.mouse_status = false;
  graphics.cursor_timestamp = get_ticks();
  graphics.cursor_flipflop = 1;
  graphics.system_mouse = conf->system_mouse;
  graphics.grab_mouse = conf->grab_mouse;

  memset(&(graphics.text_video_layer), 0, sizeof(struct video_layer));
  graphics.text_video_layer.w = SCREEN_W;
  graphics.text_video_layer.h = SCREEN_H;
  graphics.text_video_layer.transparent_col = -1;

  init_layers();

  if(!set_graphics_output(conf))
    return false;

  if(conf->resolution_width == -1 && conf->resolution_height == -1)
  {
#ifdef CONFIG_SDL
    // TODO maybe be able to communicate with the renderer instead of this hack
    boolean is_scaling = true;
    int width;
    int height;

    if(!strcmp(conf->video_output, "software"))
      is_scaling = false;

    if(sdl_get_fullscreen_resolution(&width, &height, is_scaling))
    {
      graphics.resolution_width = width;
      graphics.resolution_height = height;
    }
    else
#endif
    {
      // "Safe" resolution
      graphics.resolution_width = 640;
      graphics.resolution_height = 480;
    }
  }

  snprintf(graphics.default_caption, 32, "%s", caption);

  if(!graphics.renderer.init_video(&graphics, conf))
  {
    // Try falling back to the first registered renderer
    strcpy(conf->video_output, "");
    if(!set_graphics_output(conf))
      return false;

    // Fallback failed; bail out
    if(!graphics.renderer.init_video(&graphics, conf))
      return false;
  }

#ifdef CONFIG_SDL
  if(!graphics.system_mouse)
    SDL_ShowCursor(SDL_DISABLE);
#endif

  ec_load_set_secondary(mzx_res_get_by_id(MZX_DEFAULT_CHR),
   graphics.default_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_EDIT_CHR),
   graphics.charset + (PROTECTED_CHARSET_POSITION * CHAR_SIZE));

#if NUM_CHARSETS > 2
  if(!layer_renderer_check(false))
  {
    // As the renderer doesn't support advanced features, it
    // may also not support the number of charsets we want
    // Copy the protected charset to the end of the normal charset
    // position
    // TODO: This check ought to happen whenever the renderer changes, too
    memcpy(graphics.charset + (CHARSET_SIZE * CHAR_SIZE),
     graphics.charset + (PROTECTED_CHARSET_POSITION * CHAR_SIZE),
     CHARSET_SIZE * CHAR_SIZE);
  }
#endif

  ec_clear_set();
  ec_load_mzx();
  init_palette();
  graphics_was_initialized = true;
  return true;
}

void quit_video(void)
{
  if(graphics.renderer.free_video)
    graphics.renderer.free_video(&graphics);

  destruct_layers();
}

boolean has_video_initialized(void)
{
#ifdef CONFIG_SDL
  // Dummy SDL driver should act as headless.
  const char *sdl_driver = SDL_GetCurrentVideoDriver();
  if(sdl_driver && !strcmp(sdl_driver, "dummy")) return false;
#endif /* CONFIG_SDL */

  return graphics_was_initialized;
}

boolean set_video_mode(void)
{
  int target_width, target_height;
  int target_depth = graphics.bits_per_pixel;
  boolean fullscreen = graphics.fullscreen;
  boolean resize = graphics.allow_resize;
  boolean ret;

#ifdef CONFIG_SDL
  if(fullscreen && graphics.fullscreen_windowed)
  {
    // TODO maybe be able to communicate with the renderer instead of this hack
    if(sdl_get_fullscreen_resolution(&target_width, &target_height, true))
    {
      graphics.resolution_width = target_width;
      graphics.resolution_height = target_height;
    }
  }
#endif

  if(fullscreen)
  {
    target_width = graphics.resolution_width;
    target_height = graphics.resolution_height;
  }
  else
  {
    target_width = graphics.window_width;
    target_height = graphics.window_height;
  }

  // If video mode fails, replace it with 'safe' defaults
  if(!(graphics.renderer.check_video_mode(&graphics,
   target_width, target_height, target_depth, fullscreen, resize)))
  {
    target_width = 640;
    target_height = 350;
    target_depth = 8;
    fullscreen = false;
    resize = false;

    graphics.resolution_width = target_width;
    graphics.resolution_height = target_height;
    graphics.bits_per_pixel = target_depth;
    graphics.fullscreen = fullscreen;
  }

  ret = graphics.renderer.set_video_mode(&graphics,
   target_width, target_height, target_depth, fullscreen, resize);

  if(ret)
  {
    set_window_caption(graphics.default_caption);
    set_window_grab(graphics.grab_mouse);
    set_window_icon();
  }

  return ret;
}

boolean change_video_output(struct config_info *conf, const char *output)
{
  char old_video_output[16];
  boolean fallback = false;
  boolean retval = true;

  strncpy(old_video_output, conf->video_output, 16);
  old_video_output[15] = 0;

  strncpy(conf->video_output, output, 16);
  conf->video_output[15] = 0;

  if(graphics.renderer.free_video)
    graphics.renderer.free_video(&graphics);

  set_graphics_output(conf);

  if(!graphics.renderer.init_video(&graphics, conf))
  {
    retval = false;

    strcpy(conf->video_output, old_video_output);
    if(!set_graphics_output(conf))
    {
      warn("Failed to roll back renderer!\n");
      fallback = true;
    }
    else

    if(!graphics.renderer.init_video(&graphics, conf))
    {
      warn("Failed to roll back video mode!\n");
      fallback = true;
    }
  }

  if(fallback)
  {
    // Attempt the first renderer in the list (unless that just failed).
    if(!strcmp(conf->video_output, renderers->name))
    {
      warn("Aborting!\n");
      exit(1);
    }

    strcpy(conf->video_output, renderers->name);
    if(!set_graphics_output(conf))
    {
      warn("Failed to load fallback renderer, aborting!\n");
      exit(1);
    }

    if(!graphics.renderer.init_video(&graphics, conf))
    {
      warn("Failed to set fallback video mode, aborting!\n");
      exit(1);
    }
  }

  update_palette();
  return retval;
}

int get_available_video_output_list(const char **buffer, int buffer_len)
{
  const struct renderer_data *renderer = renderers;
  int i;

  for(i = 0; i < buffer_len; i++, renderer++)
  {
    if(!renderer->name)
      break;

    buffer[i] = renderer->name;
  }
  return i;
}

int get_current_video_output(void)
{
  return (int)(graphics.renderer_num);
}

boolean is_fullscreen(void)
{
  return graphics.fullscreen;
}

void toggle_fullscreen(void)
{
  graphics.fullscreen = !graphics.fullscreen;
  graphics.palette_dirty = true;
  if(!set_video_mode())
  {
    warn("Failed to set video mode toggling fullscreen. Aborting\n");
    exit(1);
  }
  update_screen();
}

void resize_screen(Uint32 w, Uint32 h)
{
  if(!graphics.fullscreen && graphics.allow_resize)
  {
    graphics.window_width = w;
    graphics.window_height = h;
    if(!set_video_mode())
    {
      warn("Failed to set video mode resizing window. Aborting\n");
      exit(1);
    }
    graphics.renderer.resize_screen(&graphics, w, h);
  }
}

void color_string_ext_special(const char *str, Uint32 x, Uint32 y,
 Uint8 *color, Uint32 offset, Uint32 c_offset, boolean respect_newline)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 next;
  Uint8 bg_color = (*color >> 4) + c_offset;
  Uint8 fg_color = (*color & 0x0F) + c_offset;

  char next_str[2];
  next_str[1] = 0;

  dirty_ui();
  dirty_current();

  while(cur_char)
  {
    switch(cur_char)
    {
      // Color character
      case '@':
      {
        str++;
        next = *str;

        // If 0, stop right there
        if(!next)
          goto exit_out;

        // If the next isn't hex, count as one
        if(isxdigit(next))
        {
          next_str[0] = next;
          bg_color = (Uint8)(strtol(next_str, NULL, 16) + c_offset);
        }
        else
        {
          if(next == '@')
          {
            dest->char_value = '@' + offset;
            dest->bg_color = bg_color;
            dest->fg_color = fg_color;
            *(dest_copy++) = *dest;
            dest++;
          }
          else
          {
            str--;
          }
        }

        break;
      }

      case '~':
      {
        str++;
        next = *str;

        // If 0, stop right there
        if(!next)
          goto exit_out;

        // If the next isn't hex, count as one
        if(isxdigit(next))
        {
          next_str[0] = next;
          fg_color = (Uint8)(strtol(next_str, NULL, 16) + c_offset);
        }
        else
        {
          if(next == '~')
          {
            dest->char_value = '~' + offset;
            dest->bg_color = bg_color;
            dest->fg_color = fg_color;
            *(dest_copy++) = *dest;
            dest++;
          }
          else
          {
            str--;
          }
        }

        break;
      }

      // Newline
      case '\n':
      {
        if(respect_newline)
        {
          y++;
          dest = graphics.current_video + offset_adjust((y * SCREEN_W) + x);
          dest_copy = graphics.text_video + (y * SCREEN_W) + x;
          break;
        }
      }

      /* fallthrough */

      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        *(dest_copy++) = *dest;
        dest++;
      }
    }
    if(dest >= graphics.current_video + offset_adjust(SCREEN_W * SCREEN_H))
      break;

    str++;
    cur_char = *str;
  }

exit_out:
  *color = (((bg_color - c_offset) << 4) & 0xF0) |
           (((fg_color - c_offset) << 0) & 0x0F);
}

void color_string_ext(const char *str, Uint32 x, Uint32 y, Uint8 color,
 Uint32 offset, Uint32 c_offset, boolean respect_newline)
{
  color_string_ext_special(str, x, y, &color, offset,
   c_offset, respect_newline);
}

// Write a normal string

void write_string_ext(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;

  dirty_ui();
  dirty_current();

  while(cur_char && (cur_char != 0))
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.current_video + offset_adjust((y * SCREEN_W) + x);
        dest_copy = graphics.text_video + (y * SCREEN_W) + x;
        break;
      }

      case '\t':
      {
        if(tab_allowed)
        {
          dest += 5;
          dest_copy += 5;
          break;
        }
      }

      /* fallthrough */

      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        *(dest_copy++) = *dest;
        dest++;
      }
    }
    if(dest >= graphics.current_video + offset_adjust(SCREEN_W * SCREEN_H))
      break;

    str++;
    cur_char = *str;
  }
}

void write_string_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;

  dirty_ui();
  dirty_current();

  while(cur_char && (cur_char != 0))
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.current_video + offset_adjust((y * SCREEN_W) + x);
        dest_copy = graphics.text_video + (y * SCREEN_W) + x;
        break;
      }

      case '\t':
      {
        if(tab_allowed)
        {
          dest += 5;
          dest_copy += 5;
          break;
        }
      }

      /* fallthrough */

      default:
      {
        if((cur_char >= 32) && (cur_char <= 126))
          dest->char_value = cur_char + PRO_CH;
        else
          dest->char_value = cur_char;

        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        *(dest_copy++) = *dest;
        dest++;
      }
    }
    if(dest >= graphics.current_video + offset_adjust(SCREEN_W * SCREEN_H))
      break;

    str++;
    cur_char = *str;
  }
}

// Write a normal string, without carriage returns

void write_line_ext(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;

  dirty_ui();
  dirty_current();

  while(cur_char && (cur_char != '\n'))
  {
    switch(cur_char)
    {
      // Color character
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 10;
          dest_copy += 10;
          break;
        }
      }

      /* fallthrough */

      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        *(dest_copy++) = *dest;
        dest++;
      }
    }
    str++;
    cur_char = *str;
  }
}

void write_line_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;

  dirty_ui();
  dirty_current();

  while(cur_char && (cur_char != '\n'))
  {
    switch(cur_char)
    {
      // Color character
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 10;
          dest_copy += 10;
          break;
        }
      }

      /* fallthrough */

      default:
      {
        if((cur_char >= 32) && (cur_char <= 127))
          dest->char_value = cur_char + PRO_CH;
        else
          dest->char_value = cur_char;

        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        *(dest_copy++) = *dest;
        dest++;
      }
    }
    str++;
    cur_char = *str;
  }
}

// Set rightalign to print the rightmost char at xy and proceed to the left
// minlen is the minimum length to print. Pad with 0.

void write_number(int number, char color, int x, int y,
 int minlen, boolean rightalign, int base)
{
  char temp[12];
  minlen = CLAMP(minlen, 0, 11);

  if(base == 10)
    snprintf(temp, 12, "%0*d", minlen, number);
  else
    snprintf(temp, 12, "%0*x", minlen, number);

  temp[11] = 0;

  if(rightalign)
  {
    x -= strlen(temp) - 1;
    if(x < 0)
      x = 0;
  }

  write_string(temp, x, y, color, false);
}

static void color_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 color, Uint32 offset, Uint32 c_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  Uint32 i;

  dirty_ui();
  dirty_current();

  for(i = 0; i < length; i++)
  {
    dest->char_value = dest_copy->char_value;
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    *(dest_copy++) = *dest;
    dest++;
  }
}

void fill_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 chr, Uint8 color, Uint32 offset, Uint32 c_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  Uint32 i;

  dirty_ui();
  dirty_current();

  for(i = 0; i < length; i++)
  {
    dest->char_value = chr + offset;
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    *(dest_copy++) = *dest;
    dest++;
  }
}

void draw_char_mixed_pal_ext(Uint8 chr, Uint8 bg_color,
 Uint8 fg_color, Uint32 x, Uint32 y, Uint32 offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  dest->char_value = chr + offset;

  dest->bg_color = bg_color & 31;
  dest->fg_color = fg_color & 31;

  *(dest_copy++) = *dest;

  dirty_ui();
  dirty_current();
}

void draw_char_ext(Uint8 chr, Uint8 color, Uint32 x,
 Uint32 y, Uint32 offset, Uint32 c_offset)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  struct char_element *dest_copy = graphics.text_video + scr_off;
  dest->char_value = chr + offset;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
  *(dest_copy++) = *dest;

  dirty_ui();
  dirty_current();
}

void draw_char_linear_ext(Uint8 color, Uint8 chr,
 Uint32 offset, Uint32 offset_b, Uint32 c_offset)
{
  struct char_element *dest = graphics.current_video + offset_adjust(offset);
  struct char_element *dest_copy = graphics.text_video + offset;
  dest->char_value = chr + offset_b;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
  *(dest_copy++) = *dest;

  dirty_ui();
  dirty_current();
}

void draw_char_to_layer(Uint8 color, Uint8 chr,
 Uint32 x, Uint32 y, Uint32 offset_b, Uint32 c_offset)
{
  int w = graphics.video_layers[graphics.current_layer].w;
  struct char_element *dest = graphics.current_video + (y * w) + x;
  dest->char_value = chr + offset_b;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
  dirty_current();
}

void color_string(const char *string, Uint32 x, Uint32 y, Uint8 color)
{
  color_string_ext(string, x, y, color, PRO_CH, 16, false);
}

void write_string(const char *string, Uint32 x, Uint32 y, Uint8 color,
 Uint32 tab_allowed)
{
  write_string_ext(string, x, y, color, tab_allowed, PRO_CH, 16);
}

void color_line(Uint32 length, Uint32 x, Uint32 y, Uint8 color)
{
  color_line_ext(length, x, y, color, PRO_CH, 16);
}

void fill_line(Uint32 length, Uint32 x, Uint32 y, Uint8 chr,
 Uint8 color)
{
  fill_line_ext(length, x, y, chr, color, PRO_CH, 16);
}

void draw_char(Uint8 chr, Uint8 color, Uint32 x, Uint32 y)
{
  draw_char_ext(chr, color, x, y, PRO_CH, 16);
}

void erase_char(Uint32 x, Uint32 y)
{
  int scr_off = (y * SCREEN_W) + x;
  struct char_element *dest = graphics.current_video + offset_adjust(scr_off);
  dest->char_value = INVISIBLE_CHAR;
}

void erase_area(Uint32 x, Uint32 y, Uint32 x2, Uint32 y2)
{
  Uint32 i, j;

  for(i = y; i <= y2; i++)
    for(j = x; j <= x2; j++)
      erase_char(j, i);
}

Uint8 get_color_linear(Uint32 offset)
{
  struct char_element *dest = graphics.text_video + offset;
  return (dest->bg_color << 4) | (dest->fg_color & 0x0F);
}

void clear_screen(void)
{
  // Hide the game screen by drawing blank chars over the UI.
  Uint32 i;
  struct char_element *dest = graphics.current_video;
  struct char_element *dest_copy = graphics.text_video;
  Uint32 current_layer = graphics.current_layer;

  select_layer(UI_LAYER);
  dirty_current();

  for(i = 0; i < (SCREEN_W * SCREEN_H); i++)
  {
    dest->char_value = 0;
    dest->fg_color = 16; // Protected black
    dest->bg_color = 16; // Protected black
    *(dest_copy++) = *dest;
    dest++;
  }

  select_layer(current_layer);
  update_screen();
}

void set_screen(struct char_element *src)
{
  int offset = SCREEN_W * SCREEN_H;
  int size = offset * sizeof(struct char_element);

  memcpy(graphics.video_layers[UI_LAYER].data, src, size);
  src += offset;

  memcpy(graphics.video_layers[GAME_UI_LAYER].data, src, size);
  src += offset;

  memcpy(graphics.text_video, src, size);
}

void get_screen(struct char_element *dest)
{
  int offset = SCREEN_W * SCREEN_H;
  int size = offset * sizeof(struct char_element);

  memcpy(dest, graphics.video_layers[UI_LAYER].data, size);
  dest += offset;

  memcpy(dest, graphics.video_layers[GAME_UI_LAYER].data, size);
  dest += offset;

  memcpy(dest, graphics.text_video, size);
}

void cursor_underline(void)
{
  graphics.cursor_mode = cursor_mode_underline;
}

void cursor_solid(void)
{
  graphics.cursor_mode = cursor_mode_solid;
}

void cursor_off(void)
{
  graphics.cursor_mode = cursor_mode_invisible;
}

void move_cursor(Uint32 x, Uint32 y)
{
  graphics.cursor_x = x;
  graphics.cursor_y = y;
}

#ifdef CONFIG_HELPSYS

void set_cursor_mode(enum cursor_mode_types mode)
{
  graphics.cursor_mode = mode;
}

enum cursor_mode_types get_cursor_mode(void)
{
  return graphics.cursor_mode;
}

#endif // CONFIG_HELPSYS

void m_hide(void)
{
  if(!graphics.system_mouse)
    graphics.mouse_status = false;
}

void m_show(void)
{
  if(!graphics.system_mouse)
    graphics.mouse_status = true;
}

#ifdef CONFIG_ENABLE_SCREENSHOTS
#ifdef CONFIG_PNG

#define DUMP_FMT_EXT "png"

/* Trivial PNG dumper; this routine is a modification (simplification) of
 * code pinched from http://www2.autistici.org/encelo/prog_sdldemos.php.
 *
 * Palette support was added, the original support was broken.
 *
 * Copyright (C) 2006 Angelo "Encelo" Theodorou
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 */
/*
static void dump_screen_real(Uint8 *pix, struct rgb_color *pal, int count,
 const char *name)
{
  png_write_screen(pix, pal, count, name);
}
*/

static void dump_screen_real_32bpp(Uint32 *pix, const char *name)
{
  png_write_screen_32bpp(pix, name);
}

#else

#define DUMP_FMT_EXT "bmp"

/*
static void dump_screen_real(Uint8 *pix, struct rgb_color *pal, int count,
 const char *name)
{
  FILE *file;
  int i;

  file = fopen_unsafe(name, "wb");
  if(!file)
    return;

  // BMP header
  fputw(0x4D42, file); // BM
  fputd(14 + 40 + count * 4 + 640 * 350, file); // BMP + DIB + palette + image
  fputd(0, file); // Reserved
  fputd(14, file); // DIB header offset

  // DIB header
  fputd(40, file); // DIB header size (Windows 3/BITMAPINFOHEADER)
  fputd(640, file); // Width in pixels
  fputd(350, file); // Height in pixels
  fputw(1, file); // Number of color planes
  fputw(8, file); // Bits per pixel
  fputd(0, file); // Compression method (none)
  fputd(640 * 350, file); // Image data size
  fputd(3780, file); // Horizontal dots per meter
  fputd(3780, file); // Vertical dots per meter
  fputd(count, file); // Number of colors in palette
  fputd(0, file); // Number of important colors

  // Color palette
  for(i = 0; i < count; i++)
  {
    fputc(pal[i].b, file);
    fputc(pal[i].g, file);
    fputc(pal[i].r, file);
    fputc(0, file);
  }

  // Image data
  for(i = 349; i >= 0; i--)
  {
    fwrite(pix + i * 640, sizeof(Uint8), 640, file);
  }

  fclose(file);
}
*/
static void dump_screen_real_32bpp(Uint32 *pix, const char *name)
{
  FILE *file;
  int i, x;
  Uint8 rowbuffer[SCREEN_PIX_W * 3]; // 24bpp
  Uint8 *rowbuffer_ptr;
  Uint32 *pix_ptr;

  file = fopen_unsafe(name, "wb");
  if(!file)
    return;

  // BMP header
  fputw(0x4D42, file); // BM
  // BMP + DIB + image
  fputd(14 + 40 + SCREEN_PIX_W * SCREEN_PIX_H * 3, file);
  fputd(0, file); // Reserved
  fputd(14, file); // DIB header offset

  // DIB header
  fputd(40, file); // DIB header size (Windows 3/BITMAPINFOHEADER)
  fputd(SCREEN_PIX_W, file); // Width in pixels
  fputd(SCREEN_PIX_H, file); // Height in pixels
  fputw(1, file); // Number of color planes
  fputw(24, file); // Bits per pixel
  fputd(0, file); // Compression method (none)
  fputd(SCREEN_PIX_W * SCREEN_PIX_H * 3, file); // Image data size
  fputd(3780, file); // Horizontal dots per meter
  fputd(3780, file); // Vertical dots per meter
  fputd(0, file); // Number of colors in palette
  fputd(0, file); // Number of important colors

  // Image data
  for(i = SCREEN_PIX_H - 1; i >= 0; i--)
  {
    pix_ptr = pix + i * SCREEN_PIX_W;
    rowbuffer_ptr = rowbuffer;
    for(x = 0; x < SCREEN_PIX_W; x++)
    {
      rowbuffer_ptr[0] = (*pix_ptr & 0x000000FF) >> 0;
      rowbuffer_ptr[1] = (*pix_ptr & 0x0000FF00) >> 8;
      rowbuffer_ptr[2] = (*pix_ptr & 0x00FF0000) >> 16;
      rowbuffer_ptr += 3;
      pix_ptr++;
    }
    fwrite(rowbuffer, SCREEN_PIX_W * 3, 1, file);
    //fwrite(pix + i * 640, sizeof(Uint8), 640, file);
  }

  fclose(file);
}

#endif // CONFIG_PNG

#define MAX_NAME_SIZE 20

void dump_screen(void)
{
  struct rgb_color palette[FULL_PAL_SIZE];
  Uint32 backup_palette[FULL_PAL_SIZE];
  size_t palette_size_bytes = sizeof(Uint32) * FULL_PAL_SIZE;
  int palette_size;
  char name[MAX_NAME_SIZE];
  struct stat file_info;
  Uint32 *ss;
  int i;
  Uint32 layer;

  for(i = 0; i < 99999; i++)
  {
    snprintf(name, MAX_NAME_SIZE - 1, "screen%d.%s", i, DUMP_FMT_EXT);
    name[MAX_NAME_SIZE - 1] = '\0';
    if(stat(name, &file_info))
      break;
  }

  ss = cmalloc(sizeof(Uint32) * 640 * 350);

  // Unfortunately, render_layer wants flat_intensity_palette set, so we need
  // to back this up, fill it, render the layer to memory, then copy the old
  // palette back.
  memcpy(backup_palette, graphics.flat_intensity_palette, palette_size_bytes);

  palette_size = make_palette(palette);
  for(i = 0; i < palette_size; i++)
  {
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
    graphics.flat_intensity_palette[i] =
     (palette[i].r << 8) | (palette[i].g << 16) | (palette[i].b << 24);
#else
    graphics.flat_intensity_palette[i] =
     (palette[i].r << 16) | (palette[i].g << 8) | (palette[i].b << 0);
#endif /* PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN */
  }

  for(layer = 0; layer < graphics.layer_count; layer++)
    graphics.sorted_video_layers[layer] = &graphics.video_layers[layer];

  qsort(graphics.sorted_video_layers, graphics.layer_count,
   sizeof(struct video_layer *), compare_layers);

  for(layer = 0; layer < graphics.layer_count; layer++)
  {
    render_layer(ss, 32, 640 * sizeof(Uint32), &graphics,
     graphics.sorted_video_layers[layer]);
  }

  memcpy(graphics.flat_intensity_palette, backup_palette, palette_size_bytes);

  //dump_screen_real(ss, palette, make_palette(palette), name);
  dump_screen_real_32bpp(ss, name);
  free(ss);
}
#endif /* CONFIG_ENABLE_SCREENSHOTS */

void dump_char(Uint16 char_idx, Uint8 color, int mode, Uint8 *buffer)
{
  // Dumps the specified char into a buffer. Expects CHAR_W * CHAR_H bytes.
  int x, y;
  Uint8 cols[4];
  char_idx = char_idx % PROTECTED_CHARSET_POSITION;
  color = color % SMZX_PAL_SIZE;

  if(mode == -1)
    mode = graphics.screen_mode;

  if(mode == 0)
  {
    cols[0] = (color & 0xF0) >> 4;
    cols[1] = color & 0x0F;

    for(y = 0; y < CHAR_H; y++)
    {
      char row = graphics.charset[char_idx * CHAR_SIZE + y];
      for(x = 0; x < CHAR_W; x++)
      {
        buffer[y * CHAR_W + x] = cols[(row >> (7-x)) & 0x01];
      }
    }
  }
  else
  {
    cols[0] = graphics.smzx_indices[color * 4 + 0];
    cols[1] = graphics.smzx_indices[color * 4 + 1];
    cols[2] = graphics.smzx_indices[color * 4 + 2];
    cols[3] = graphics.smzx_indices[color * 4 + 3];

    for(y = 0; y < CHAR_H; y++)
    {
      char row = graphics.charset[char_idx * CHAR_SIZE + y];
      for(x = 0; x < CHAR_W; x += 2)
      {
        buffer[y * CHAR_W + x] = cols[(row >> (7-x)) & 0x03];
        buffer[y * CHAR_W + x + 1] = cols[(row >> (7-x)) & 0x03];
      }
    }
  }
}

void get_screen_coords(int screen_x, int screen_y, int *x, int *y,
 int *min_x, int *min_y, int *max_x, int *max_y)
{
  graphics.renderer.get_screen_coords(&graphics, screen_x, screen_y, x, y,
   min_x, min_y, max_x, max_y);
}

void set_screen_coords(int x, int y, int *screen_x, int *screen_y)
{
  graphics.renderer.set_screen_coords(&graphics, x, y, screen_x, screen_y);
}

void set_mouse_mul(int width_mul, int height_mul)
{
  graphics.mouse_width_mul = width_mul;
  graphics.mouse_height_mul = height_mul;
}

void focus_screen(int x, int y)
{
  int pixel_x = x * graphics.resolution_width  / SCREEN_W +
                    graphics.resolution_width  / SCREEN_W / 2;
  int pixel_y = y * graphics.resolution_height / SCREEN_H +
                    graphics.resolution_height / SCREEN_H / 2;
  focus_pixel(pixel_x, pixel_y);
}

void focus_pixel(int x, int y)
{
  if(graphics.renderer.focus_pixel)
    graphics.renderer.focus_pixel(&graphics, x, y);
}

boolean switch_shader(const char *name)
{
  if(graphics.renderer.switch_shader)
    graphics.renderer.switch_shader(&graphics, name);

  if(graphics.gl_scaling_shader[0])
    return true;

  return false;
}

boolean layer_renderer_check(boolean show_error)
{
  if(!graphics.renderer.render_layer)
  {
    if(show_error)
    {
      error_message(E_NO_LAYER_RENDERER, 0, NULL);
      set_error_suppression(E_NO_LAYER_RENDERER, 1);
    }
    return false;
  }
  return true;
}
