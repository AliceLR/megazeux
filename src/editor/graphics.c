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

void store_backup_palette(char dest[SMZX_PAL_SIZE])
{
  Uint32 i;
  Uint8 *r;
  Uint8 *g;
  Uint8 *b;

  for(i = 0; i < graphics.protected_pal_position; i++)
  {
    r = (Uint8 *)(dest++);
    g = (Uint8 *)(dest++);
    b = (Uint8 *)(dest++);
    get_rgb(i, r, g, b);
  }
}

void load_backup_palette(char src[SMZX_PAL_SIZE * 3])
{
  load_palette_mem(src, SMZX_PAL_SIZE * 3);
}

void store_backup_indices(char dest[SMZX_PAL_SIZE * 4])
{
  memcpy(dest, graphics.smzx_indices, SMZX_PAL_SIZE * 4);
}

void load_backup_indices(char src[SMZX_PAL_SIZE * 4])
{
  memcpy(graphics.smzx_indices, src, SMZX_PAL_SIZE * 4);
}

void save_palette(char *fname)
{
  FILE *pal_file = fopen_unsafe(fname, "wb");

  if(pal_file)
  {
    int num_colors = PAL_SIZE;
    int i;

    if(graphics.screen_mode >= 2)
      num_colors = SMZX_PAL_SIZE;

    for(i = 0; i < num_colors; i++)
    {
      fputc(get_red_component(i), pal_file);
      fputc(get_green_component(i), pal_file);
      fputc(get_blue_component(i), pal_file);
    }

    fclose(pal_file);
  }
}

void save_index_file(char *fname)
{
  FILE *idx_file = fopen_unsafe(fname, "wb");

  if(idx_file)
  {
    int i;

    for(i = 0; i < SMZX_PAL_SIZE; i++)
    {
      fputc(get_smzx_index(i, 0), idx_file);
      fputc(get_smzx_index(i, 1), idx_file);
      fputc(get_smzx_index(i, 2), idx_file);
      fputc(get_smzx_index(i, 3), idx_file);
    }

    fclose(idx_file);
  }
}

void draw_char_mixed_pal(Uint8 chr, Uint8 bg_color, Uint8 fg_color,
 Uint32 x, Uint32 y)
{
  draw_char_mixed_pal_ext(chr, bg_color, fg_color, x, y, PRO_CH);
}

void draw_char_linear(Uint8 color, Uint8 chr, Uint32 offset,
 boolean use_protected_pal)
{
  draw_char_linear_ext(color, chr, offset, PRO_CH, use_protected_pal ? 16 : 0);
}

void ec_save_set_var(char *name, Uint16 offset, Uint32 size)
{
  FILE *fp = fopen_unsafe(name, "wb");

  if(fp)
  {
    if(size + offset > PROTECTED_CHARSET_POSITION)
    {
      size = PROTECTED_CHARSET_POSITION - offset;
    }

    fwrite(graphics.charset + (offset * CHAR_SIZE), CHAR_SIZE, size, fp);
    fclose(fp);
  }
}

void ec_change_block(Uint8 offset, Uint8 charset,
 Uint8 width, Uint8 height, char *matrix)
{
  // Change a block of chars on the 32x8 charset
  int skip;
  int x;
  int y;

  width = CLAMP(width, 1, 32);
  height = CLAMP(height, 1, 8);

  skip = 32 - width;

  // No need to bound offset (Uint8)
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++)
    {
      ec_change_char((charset * 256) + offset, matrix);
      matrix += CHAR_SIZE;
      offset++;
    }
    offset += skip;
  }
}

void ec_read_block(Uint8 offset, Uint8 charset,
 Uint8 width, Uint8 height, char *matrix)
{
  // Read a block of chars from the 32x8 charset
  int skip;
  int x;
  int y;

  width = CLAMP(width, 1, 32);
  height = CLAMP(height, 1, 8);

  skip = 32 - width;

  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++)
    {
      ec_read_char((charset * 256) + offset, matrix);
      matrix += CHAR_SIZE;
      offset++;
    }
    offset += skip;
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
  ec_mem_load_set(smzx_charset, CHAR_SIZE * CHARSET_SIZE);
}

void ec_load_blank(void)
{
  ec_mem_load_set(blank_charset, CHAR_SIZE * CHARSET_SIZE);
}

void ec_load_ascii(void)
{
  ec_mem_load_set(ascii_charset, CHAR_SIZE * CHARSET_SIZE);
}

void ec_load_char_ascii(Uint32 char_number)
{
  Uint32 ascii_number = char_number & 0xFF;
  char *ascii_char = (char *)(ascii_charset + ascii_number * CHAR_SIZE);

  ec_change_char(char_number, ascii_char);
}

void ec_load_char_mzx(Uint32 char_number)
{
  Uint8 *default_charset = graphics.default_charset;
  Uint32 default_number = char_number & 0xFF;

  char *default_char = (char *)(default_charset + default_number * CHAR_SIZE);

  ec_change_char(char_number, default_char);
}
