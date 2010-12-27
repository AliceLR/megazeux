/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "graphics.h"

#include "../graphics.h"
#include "../util.h"

#include <string.h>
#include <stdio.h>

static Uint8 ascii_charset[CHAR_SIZE * CHARSET_SIZE];
static Uint8 blank_charset[CHAR_SIZE * CHARSET_SIZE];
static Uint8 smzx_charset[CHAR_SIZE * CHARSET_SIZE];

void save_editor_palette(void)
{
  if(graphics.screen_mode < 2)
    memcpy(graphics.editor_backup_palette, graphics.palette,
     sizeof(struct rgb_color) * SMZX_PAL_SIZE);
}

void load_editor_palette(void)
{
  memcpy(graphics.palette, graphics.editor_backup_palette,
   sizeof(struct rgb_color) * SMZX_PAL_SIZE);
  set_gui_palette();
}

void save_palette(char *fname)
{
  FILE *pal_file = fopen_unsafe(fname, "wb");

  if(pal_file)
  {
    int num_colors = SMZX_PAL_SIZE;
    int i;

    if(!graphics.screen_mode)
      num_colors = PAL_SIZE;

    for(i = 0; i < num_colors; i++)
    {
      fputc(get_red_component(i), pal_file);
      fputc(get_green_component(i), pal_file);
      fputc(get_blue_component(i), pal_file);
    }

    fclose(pal_file);
  }
}

void draw_char_linear(Uint8 color, Uint8 chr, Uint32 offset)
{
  draw_char_linear_ext(color, chr, offset, 256, 16);
}

void clear_screen_no_update(void)
{
  struct char_element *dest = graphics.text_video;
  Uint32 i;

  for(i = 0; i < (SCREEN_W * SCREEN_H); i++)
  {
    // use protected charset and palette
    dest->char_value = 177 + 256;
    dest->fg_color = 1 + 16;
    dest->bg_color = 0 + 16;
    dest++;
  }
}

void ec_save_set_var(char *name, Uint8 offset, Uint32 size)
{
  FILE *fp = fopen_unsafe(name, "wb");

  if(fp)
  {
    if(size + offset > CHARSET_SIZE)
    {
      size = CHARSET_SIZE - offset;
    }

    fwrite(graphics.charset + (offset * CHAR_SIZE), CHAR_SIZE, size, fp);
    fclose(fp);
  }
}

void load_editor_charsets(void)
{
  ec_load_set_secondary(mzx_res_get_by_id(MZX_ASCII_CHR), ascii_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_BLANK_CHR), blank_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_SMZX_CHR),  smzx_charset);
}

void ec_load_smzx(void)
{
  ec_mem_load_set(smzx_charset);
}

void ec_load_blank(void)
{
  ec_mem_load_set(blank_charset);
}

void ec_load_ascii(void)
{
  ec_mem_load_set(ascii_charset);
}

void ec_load_char_ascii(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * CHAR_SIZE),
   ascii_charset + (char_number * CHAR_SIZE), CHAR_SIZE);

  // some renderers may want to map charsets to textures
  if(graphics.renderer.remap_charsets)
    graphics.renderer.remap_charsets(&graphics);
}

void ec_load_char_mzx(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * CHAR_SIZE),
   graphics.default_charset + (char_number * CHAR_SIZE), CHAR_SIZE);

  // some renderers may want to map charsets to textures
  if(graphics.renderer.remap_charsets)
    graphics.renderer.remap_charsets(&graphics);
}
