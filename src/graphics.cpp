/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#include "SDL.h"
#include "graphics.h"
#include "delay.h"
#include "world.h"
#include "configure.h"
#include "config.h"
#include "event.h"

graphics_data graphics;

SDL_Color default_pal[16] =
{
  { 00, 00, 00 },
  { 00, 00, 170 },
  { 00, 170, 00 },
  { 00, 170, 170 },
  { 170, 00, 00 },
  { 170, 00, 170 },
  { 170, 85, 00 },
  { 170, 170, 170 },
  { 85, 85, 85 },
  { 85, 85, 255 },
  { 85, 255, 85 },
  { 85, 255, 255 },
  { 255, 85, 85 },
  { 255, 85, 255 },
  { 255, 255, 85 },
  { 255, 255, 255 }
};

void init_video(config_info *conf)
{
  int res_x = conf->resolution_x;
  int res_y = conf->resolution_y;
  int fullscreen = conf->fullscreen;

  char temp[64];

  /*
  SDL_Surface *icon;

  icon = SDL_LoadBMP("mzx_icon.bmp");

  if(icon)
  {
    SDL_SetColorKey(icon, 0, 0xDD);
    SDL_WM_SetIcon(icon, NULL);
    SDL_FreeSurface(icon);
  }
  */

  if(!fullscreen)
  {
    graphics.screen = SDL_SetVideoMode(640, 350, 8, 0);
  }
  else
  {
    graphics.screen = SDL_SetVideoMode(res_x, res_y, 8,
     SDL_FULLSCREEN);
  }

  sprintf(temp, "MegaZeux %s", version_number_string);
  SDL_WM_SetCaption(temp, "MZX");
  SDL_ShowCursor(SDL_DISABLE);

  graphics.screen_mode = 0;
  graphics.fullscreen = fullscreen;
  graphics.resolution_x = res_x;
  graphics.resolution_y = res_y;
  graphics.height_multiplier = conf->height_multiplier;
  graphics.mouse_status = 0;
  graphics.cursor_timestamp = SDL_GetTicks();
  graphics.cursor_flipflop = 1;

  ec_load_set_secondary(MZX_DEFAULT_CHR, graphics.default_charset);
  ec_load_set_secondary(MZX_BLANK_CHR, graphics.blank_charset);
  ec_load_set_secondary(MZX_SMZX_CHR, graphics.smzx_charset);
  ec_load_set_secondary(MZX_ASCII_CHR, graphics.ascii_charset);
  ec_load_set_secondary(MZX_EDIT_CHR, graphics.charset + (256 * 14));
  ec_load_mzx();
  init_palette();
}

void toggle_fullscreen()
{
  if(graphics.fullscreen)
  {
    graphics.screen = SDL_SetVideoMode(640, 350, 8, 0);
    graphics.fullscreen = 0;
  }
  else
  {
    int res_x = graphics.resolution_x;
    int res_y = graphics.resolution_y;

    graphics.screen = SDL_SetVideoMode(res_x, res_y, 8,
     SDL_FULLSCREEN);

    graphics.fullscreen = 1;
  }
  update_screen();
  update_palette();
}

void color_string(char *string, Uint32 x, Uint32 y, Uint8 color)
{
  color_string_ext(string, x, y, color, 256, 16);
}

void write_string(char *string, Uint32 x, Uint32 y, Uint8 color,
 Uint32 tab_allowed)
{
  write_string_ext(string, x, y, color, tab_allowed, 256, 16);
}

void write_line(char *string, Uint32 x, Uint32 y, Uint8 color,
 Uint32 tab_allowed)
{
  write_line_ext(string, x, y, color, tab_allowed, 256, 16);
}

void color_line(Uint32 length, Uint32 x, Uint32 y, Uint8 color)
{
  color_line_ext(length, x, y, color, 256, 16);
}

void fill_line(Uint32 length, Uint32 x, Uint32 y, Uint8 chr,
 Uint8 color)
{
  fill_line_ext(length, x, y, chr, color, 256, 16);
}
void draw_char(Uint8 chr, Uint8 color, Uint32 x, Uint32 y)
{
  draw_char_ext(chr, color, x, y, 256, 16);
}

void draw_char_linear(Uint8 color, Uint8 chr, Uint32 offset)
{
  draw_char_linear_ext(color, chr, offset, 256, 16);
}

void draw_char_nocolor(Uint8 chr, Uint32 x, Uint32 y)
{
  draw_char_nocolor_ext(chr, x, y, 256, 16);
}

void color_string_ext(char *str, Uint32 x, Uint32 y, Uint8 color,
 Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;

  char next_str[2];
  next_str[1] = 0;

  while(cur_char)
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.text_video + (y * 80) + x;
        break;
      }
      // Color character
      case '@':
      {
        str++;
        next = *str;

        // If 0, stop right there
        if(!next)
          return;

        // If the next isn't hex, count as one
        if(isxdigit(next))
        {
          next_str[0] = next;
          bg_color = strtol(next_str, NULL, 16) + c_offset;
        }
        else
        {
          if(next == '@')
          {
            dest->char_value = '@' + offset;
            dest->bg_color = bg_color;
            dest->fg_color = fg_color;
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
          return;

        // If the next isn't hex, count as one
        if(isxdigit(next))
        {
          next_str[0] = next;
          fg_color = strtol(next_str, NULL, 16) + c_offset;
        }
        else
        {
          if(next == '~')
          {
            dest->char_value = '~' + offset;
            dest->bg_color = bg_color;
            dest->fg_color = fg_color;
            dest++;
          }
          else
          {
            str--;
          }
        }

        break;
      }
      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    if(dest >= graphics.text_video + (80 * 25))
      break;

    str++;
    cur_char = *str;
  }
}

// Write a normal string

void write_string_ext(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  next_str[1] = 0;

  while(cur_char && (cur_char != 0))
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.text_video + (y * 80) + x;
        break;
      }
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 5;
          break;
        }
      }
      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    if(dest >= graphics.text_video + (80 * 25))
      break;

    str++;
    cur_char = *str;
  }
}

void write_string_mask(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;
  next_str[1] = 0;

  while(cur_char && (cur_char != 0))
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.text_video + (y * 80) + x;
        break;
      }
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 5;
          break;
        }
      }
      default:
      {
        if((cur_char >= 32) && (cur_char <= 127))
          dest->char_value = cur_char + 256;
        else
          dest->char_value = cur_char;

        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    if(dest >= graphics.text_video + (80 * 25))
      break;

    str++;
    cur_char = *str;
  }
}

// Write a normal string, without carriage returns

void write_line_ext(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  next_str[1] = 0;

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
          break;
        }
      }
      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    str++;
    cur_char = *str;
  }
}

void write_line_mask(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;
  next_str[1] = 0;

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
          break;
        }
      }
      default:
      {
        if((cur_char >= 32) && (cur_char <= 127))
          dest->char_value = cur_char + 256;
        else
          dest->char_value = cur_char;

        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    str++;
    cur_char = *str;
  }
}

void color_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 color, Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  Uint32 i;

  for(i = 0; i < length; i++)
  {
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    dest++;
  }
}

void fill_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 chr, Uint8 color, Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  Uint32 i;

  for(i = 0; i < length; i++)
  {
    dest->char_value = chr + offset;
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    dest++;
  }
}

void draw_char_ext(Uint8 chr, Uint8 color, Uint32 x,
 Uint32 y, Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  dest->char_value = chr + offset;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

void draw_char_nocolor_ext(Uint8 chr, Uint32 x, Uint32 y,
 Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  dest->char_value = chr + offset;
}

void draw_char_linear_ext(Uint8 color, Uint8 chr,
 Uint32 offset, Uint32 offset_b, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + offset;
  dest->char_value = chr + offset_b;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

Uint8 get_char(Uint32 x, Uint32 y)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  return dest->char_value;
}

Uint8 get_color(Uint32 x, Uint32 y)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  return (dest->bg_color << 4) | (dest->fg_color & 0x0F);
}

Uint8 get_char_linear(Uint32 offset)
{
  char_element *dest = graphics.text_video + offset;
  return dest->char_value;
}

Uint8 get_color_linear(Uint32 offset)
{
  char_element *dest = graphics.text_video + offset;
  return (dest->bg_color << 4) | (dest->fg_color & 0x0F);
}

Uint8 get_bg_linear(Uint32 offset)
{
  char_element *dest = graphics.text_video + offset;
  return dest->bg_color;
}

void clear_screen(Uint8 chr, Uint8 color)
{
  Uint32 i;
  Uint8 fg_color = color & 0x0F;
  Uint8 bg_color = color >> 4;
  char_element *dest = graphics.text_video;

  for(i = 0; i < (80 * 25); i++)
  {
    dest->char_value = chr;
    dest->fg_color = fg_color;
    dest->bg_color = bg_color;
    dest++;
  }
  update_screen();
}

void clear_screen_no_update(Uint8 chr, Uint8 color)
{
  Uint32 i;
  Uint8 fg_color = color & 0x0F;
  Uint8 bg_color = color >> 4;
  char_element *dest = graphics.text_video;

  for(i = 0; i < (80 * 25); i++)
  {
    dest->char_value = chr;
    dest->fg_color = fg_color;
    dest->bg_color = bg_color;
    dest++;
  }
}

void set_screen(char_element *src)
{
  memcpy(graphics.text_video, src,
   80 * 25 * sizeof(char_element));
}

void get_screen(char_element *dest)
{
  memcpy(dest, graphics.text_video,
   80 * 25 * sizeof(char_element));
}

// Draw the entire screen

void update_screen()
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  Uint32 cb_bg;
  Uint32 cb_fg;
  char_element *src = graphics.text_video;
  Uint8 *char_ptr;
  Uint32 char_colors[16];
  Uint32 current_char_byte;
  Uint8 current_color;
  Uint32 i, i2, i3;
  Uint32 line_advance = graphics.screen->pitch / 4;
  Uint32 row_advance = (graphics.screen->pitch * 14) / 4;
  Uint32 ticks = SDL_GetTicks();
  Uint32 *old_dest = NULL;

  if((graphics.height_multiplier > 1) && (graphics.fullscreen))
  {
    dest = (Uint32 *)(graphics.screen->pixels) +
     (graphics.screen->pitch * (((graphics.screen->h /
     graphics.height_multiplier) - 350) / 8)) +
     ((graphics.screen->w - 640) / 8);
    line_advance *= graphics.height_multiplier;
    row_advance *= graphics.height_multiplier;
  }
  else
  {
    dest = (Uint32 *)(graphics.screen->pixels) +
     (graphics.screen->pitch * ((graphics.screen->h - 350) / 8)) +
     ((graphics.screen->w - 640) / 8);
  }

  old_dest = dest;

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  SDL_LockSurface(graphics.screen);

  if(!graphics.screen_mode)
  {
    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        // Fill in background color
        cb_bg = src->bg_color;
        cb_fg = src->fg_color;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        char_colors[0] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[1] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[2] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[3] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[4] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[5] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[6] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[7] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[8] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[9] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[10] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[11] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[12] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[13] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[14] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[15] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
#else
        char_colors[0] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[1] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[2] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[3] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[4] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[5] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[6] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[7] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[8] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[9] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[10] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[11] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[12] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[13] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[14] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[15] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
#endif

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 4];
          *(dest + 1) = char_colors[current_char_byte & 0x0F];
          dest += line_advance;
        }
        dest = ldest + 2;
      }
      dest = ldest2 + row_advance;
    }
  }
  else

  if(graphics.screen_mode != 3)
  {
    Uint32 cb_bb;
    Uint32 cb_bf;
    Uint32 cb_fb;
    Uint32 cb_ff;

    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        cb_bg = src->bg_color & 0x0F;
        cb_fg = src->fg_color & 0x0F;
        cb_bb = (cb_bg << 4) | cb_bg;
        cb_bb = (cb_bb << 8) | cb_bb;
        cb_bf = (cb_bg << 4) | cb_fg;
        cb_bf = (cb_bf << 8) | cb_bf;
        cb_fb = (cb_fg << 4) | cb_bg;
        cb_fb = (cb_fb << 8) | cb_fb;
        cb_ff = (cb_fg << 4) | cb_fg;
        cb_ff = (cb_ff << 8) | cb_ff;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        char_colors[0] = (cb_bb << 16) | cb_bb;
        char_colors[1] = (cb_bb << 16) | cb_bf;
        char_colors[2] = (cb_bb << 16) | cb_fb;
        char_colors[3] = (cb_bb << 16) | cb_ff;
        char_colors[4] = (cb_bf << 16) | cb_bb;
        char_colors[5] = (cb_bf << 16) | cb_bf;
        char_colors[6] = (cb_bf << 16) | cb_fb;
        char_colors[7] = (cb_bf << 16) | cb_ff;
        char_colors[8] = (cb_fb << 16) | cb_bb;
        char_colors[9] = (cb_fb << 16) | cb_bf;
        char_colors[10] = (cb_fb << 16) | cb_fb;
        char_colors[11] = (cb_fb << 16) | cb_bb;
        char_colors[12] = (cb_ff << 16) | cb_bb;
        char_colors[13] = (cb_ff << 16) | cb_bf;
        char_colors[14] = (cb_ff << 16) | cb_fb;
        char_colors[15] = (cb_ff << 16) | cb_bf;
#else
        char_colors[0] = (cb_bb << 16) | cb_bb;
        char_colors[1] = (cb_bf << 16) | cb_bb;
        char_colors[2] = (cb_fb << 16) | cb_bb;
        char_colors[3] = (cb_ff << 16) | cb_bb;
        char_colors[4] = (cb_bb << 16) | cb_bf;
        char_colors[5] = (cb_bf << 16) | cb_bf;
        char_colors[6] = (cb_fb << 16) | cb_bf;
        char_colors[7] = (cb_ff << 16) | cb_bf;
        char_colors[8] = (cb_bb << 16) | cb_fb;
        char_colors[9] = (cb_bf << 16) | cb_fb;
        char_colors[10] = (cb_fb << 16) | cb_fb;
        char_colors[11] = (cb_ff << 16) | cb_fb;
        char_colors[12] = (cb_bb << 16) | cb_ff;
        char_colors[13] = (cb_bf << 16) | cb_ff;
        char_colors[14] = (cb_fb << 16) | cb_ff;
        char_colors[15] = (cb_ff << 16) | cb_ff;
#endif

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 4];
          *(dest + 1) = char_colors[current_char_byte & 0x0F];
          dest += line_advance;
        }
        dest = ldest + 2;
      }
      dest = ldest2 + row_advance;
    }
  }
  else
  {
    Uint32 cb_0;
    Uint32 cb_1;
    Uint32 cb_2;
    Uint32 cb_3;

    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        current_color = (src->bg_color << 4) | (src->fg_color & 0x0F);
        // Fill in background color
        cb_0 = current_color;
        cb_1 = (current_color + 1) & 0xFF;
        cb_2 = (current_color + 2) & 0xFF;
        cb_3 = (current_color + 3) & 0xFF;

        cb_0 |= cb_0 << 8;
        cb_1 |= cb_1 << 8;
        cb_2 |= cb_2 << 8;
        cb_3 |= cb_3 << 8;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        char_colors[0] = (cb_0 << 16) | cb_0;
        char_colors[1] = (cb_0 << 16) | cb_2;
        char_colors[2] = (cb_0 << 16) | cb_1;
        char_colors[3] = (cb_0 << 16) | cb_3;
        char_colors[4] = (cb_2 << 16) | cb_0;
        char_colors[5] = (cb_2 << 16) | cb_2;
        char_colors[6] = (cb_2 << 16) | cb_1;
        char_colors[7] = (cb_2 << 16) | cb_3;
        char_colors[8] = (cb_1 << 16) | cb_0;
        char_colors[9] = (cb_1 << 16) | cb_2;
        char_colors[10] = (cb_1 << 16) | cb_1;
        char_colors[11] = (cb_1 << 16) | cb_3;
        char_colors[12] = (cb_3 << 16) | cb_0;
        char_colors[13] = (cb_3 << 16) | cb_2;
        char_colors[14] = (cb_3 << 16) | cb_1;
        char_colors[15] = (cb_3 << 16) | cb_3;
#else
        char_colors[0] = (cb_0 << 16) | cb_0;
        char_colors[1] = (cb_2 << 16) | cb_0;
        char_colors[2] = (cb_1 << 16) | cb_0;
        char_colors[3] = (cb_3 << 16) | cb_0;
        char_colors[4] = (cb_0 << 16) | cb_2;
        char_colors[5] = (cb_2 << 16) | cb_2;
        char_colors[6] = (cb_1 << 16) | cb_2;
        char_colors[7] = (cb_3 << 16) | cb_2;
        char_colors[8] = (cb_0 << 16) | cb_1;
        char_colors[9] = (cb_2 << 16) | cb_1;
        char_colors[10] = (cb_1 << 16) | cb_1;
        char_colors[11] = (cb_3 << 16) | cb_1;
        char_colors[12] = (cb_0 << 16) | cb_3;
        char_colors[13] = (cb_2 << 16) | cb_3;
        char_colors[14] = (cb_1 << 16) | cb_3;
        char_colors[15] = (cb_3 << 16) | cb_3;
#endif

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 4];
          *(dest + 1) = char_colors[current_char_byte & 0x0F];
          dest += line_advance;
        }
        dest = ldest + 2;
      }
      dest = ldest2 + row_advance;
    }
  }

  if(graphics.mouse_status)
  {
    int mouse_x, mouse_y;
    int current_colors;
    get_real_mouse_position(&mouse_x, &mouse_y);

    mouse_x = (mouse_x / graphics.mouse_width_mul) *
     graphics.mouse_width_mul;
    mouse_y = (mouse_y / graphics.mouse_height_mul) *
     graphics.mouse_height_mul;

    dest = old_dest + (mouse_x / 4) + (mouse_y * line_advance);

    for(i = 0; i < graphics.mouse_height_mul; i++)
    {
      ldest = dest;
      for(i2 = 0; i2 < (graphics.mouse_width_mul / 4);
       i2++, dest++)
      {
        current_colors = *dest;
        *dest =
         (0x0F0F0F0F - (current_colors & 0x0F0F0F0F)) |
         (current_colors & 0xF0F0F0F0);
      }
      dest = ldest + line_advance;
    }
  }

  // Draw cursor perhaps
  if(graphics.cursor_flipflop &&
   (graphics.cursor_mode != cursor_mode_invisible))
  {
    char_element *cursor_element = graphics.text_video +
     graphics.cursor_x +  (graphics.cursor_y * 80);
    Uint32 cursor_color;
    Uint32 cursor_char = cursor_element->char_value;
    Uint32 lines = 0;
    Uint32 *cursor_offset = old_dest;
    Uint32 i;
    Uint32 cursor_solid = 0xFFFFFFFF;
    Uint32 *char_offset = (Uint32 *)(graphics.charset + (cursor_char * 14));
    Uint32 bg_color = cursor_element->bg_color;
    cursor_offset += (graphics.cursor_x * 2) +
     (graphics.cursor_y * row_advance);

    // Choose FG
    cursor_color = cursor_element->fg_color;

    // See if the cursor char is completely solid or completely
    // empty
    for(i = 0; i < 3; i++)
    {
      cursor_solid &= *char_offset;
      char_offset++;
    }
    // Get the last bit
    cursor_solid &= (*((Uint16 *)char_offset)) | 0xFFFF0000;

    // Solid cursor, use BG instead
    if(cursor_solid == 0xFFFFFFFF)
    {
      // But wait! What if the background is the same as the foreground?
      // If so, use +8 instead.
      if(bg_color == cursor_color)
      {
        cursor_color = (bg_color + 8) & 0x0F;
      }
      else
      {
        cursor_color = bg_color;
      }
    }
    else

    // What if the foreground is the same as the background?
    // It needs to flash +8 then
    if(bg_color == cursor_color)
    {
      cursor_color = (cursor_color + 8) & 0x0F;
    }

    cursor_color = cursor_color | (cursor_color << 8);
    cursor_color = cursor_color | (cursor_color << 16);

    switch(graphics.cursor_mode)
    {
      case cursor_mode_underline:
      {
        lines = 2;
        cursor_offset += (12 * line_advance);
        break;
      }

      case cursor_mode_solid:
      {
        lines = 14;
        break;
      }

      default:
      {
        break;
      }
    }

    for(i = 0; i < lines; i++)
    {
      *cursor_offset = cursor_color;
      *(cursor_offset + 1) = cursor_color;
      cursor_offset += line_advance;
    }
  }

  if((graphics.height_multiplier > 1) && (graphics.fullscreen))
  {
    // Duplicate the screen
    Uint32 *dest2;
    Uint32 line_advance2 = graphics.screen->pitch / 4;
    dest = old_dest;
    dest2 = dest + line_advance2;

    for(i = 0; i < 350; i++)
    {
      for(i2 = 0; i2 < graphics.height_multiplier - 1; i2++)
      {
        memcpy(dest2, dest, 640);
        dest2 += line_advance2;
      }
      dest2 += line_advance2;
      dest += line_advance;
    }
  }

  SDL_Flip(graphics.screen);

  SDL_UnlockSurface(graphics.screen);
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

void set_cursor_mode(cursor_mode_types mode)
{
  graphics.cursor_mode = mode;
}

cursor_mode_types get_cursor_mode()
{
  return graphics.cursor_mode;
}

void ec_change_byte(Uint8 chr, Uint8 byte, Uint8 new_value)
{
  graphics.charset[(chr * 14) + byte] = new_value;
}

void ec_change_char(Uint8 chr, char *matrix)
{
  memcpy(graphics.charset + (chr * 14), matrix, 14);
}

Uint8 ec_read_byte(Uint8 chr, Uint8 byte)
{
  return graphics.charset[(chr * 14) + byte];
}

void ec_read_char(Uint8 chr, char *matrix)
{
  memcpy(matrix, graphics.charset + (chr * 14), 14);
}

Sint32 ec_load_set(char *name)
{
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
   return -1;

  fread(graphics.charset, 14, 256, fp);

  fclose(fp);
  return 0;
}

Sint32 ec_load_set_secondary(char *name, Uint8 *dest)
{
  /**
   * this function is used only by internally loaded files.
   * theoretically, we trust these. there is no need to
   * translate these names, but the packaged files become
   * a case sensitive issue (i.e. default.pal not DEFAULT.PaL).
   */
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  fread(dest, 14, 256, fp);

  fclose(fp);
  return 0;
}

Sint32 ec_load_set_var(char *name, Uint8 pos)
{
  Uint32 size = 256;
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  fseek(fp, 0, SEEK_END);
  size = ftell(fp) / 14;
  fseek(fp, 0, 0);

  if(size + pos > 256)
  {
    size = 256 - pos;
  }
  fread(graphics.charset + (pos * 14), 14, size, fp);
  fclose(fp);

  return size;
}

Sint32 ec_load_set_ext(char *name, Uint8 offset)
{
  Uint32 size = 256;
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  fseek(fp, 0, SEEK_END);
  size = ftell(fp) / 14;
  fseek(fp, 0, 0);

  if(size + offset > (16 * 256))
  {
    size = (16 * 256) - offset;
  }

  fread(graphics.charset + (offset * 14), 14, size, fp);
  fclose(fp);

  return size;
}

void ec_save_set(char *name)
{
  FILE *fp = fopen(name, "wb");

  if(fp)
  {
    fwrite(graphics.charset, 14, 256, fp);
    fclose(fp);
  }
}

void ec_save_set_var(char *name, Uint8 offset, Uint32 size)
{
  FILE *fp = fopen(name, "wb");

  if(fp)
  {
    if(size + offset > 256)
    {
      size = 256 - offset;
    }

    fwrite(graphics.charset + (offset * 14), 14, size, fp);
    fclose(fp);
  }
}

void ec_mem_load_set(Uint8 *chars)
{
  memcpy(graphics.charset, chars, 3584);
}

void ec_mem_save_set(Uint8 *chars)
{
  memcpy(chars, graphics.charset, 3584);
}

void ec_load_mzx(void)
{
  ec_mem_load_set(graphics.default_charset);
}

void ec_load_smzx(void)
{
  ec_mem_load_set(graphics.smzx_charset);
}

void ec_load_blank(void)
{
  ec_mem_load_set(graphics.blank_charset);
}

void ec_load_ascii(void)
{
  ec_mem_load_set(graphics.ascii_charset);
}

void ec_load_char_mzx(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * 14),
   graphics.default_charset + (char_number * 14), 14);
}

void ec_load_char_smzx(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * 14),
   graphics.smzx_charset + (char_number * 14), 14);
}

void ec_load_char_ascii(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * 14),
   graphics.ascii_charset + (char_number * 14), 14);
}

void init_palette(void)
{
  Uint32 i;

  memcpy(graphics.palette, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.palette + 16, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.intensity_palette, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.intensity_palette + 16, default_pal,
   sizeof(SDL_Color) * 16);
  memset(graphics.current_intensity, 0, sizeof(Uint32) * 16);

  for(i = 0; i < 256; i++)
  {
    graphics.saved_intensity[i] = 100;
  }

  for(i = 16; i < 32; i++)
  {
    graphics.current_intensity[i] = 100;
  }

  graphics.fade_status = 1;

  update_palette();
}

void update_palette()
{
  Uint32 i;

  // Is SMZX mode set?
  if(graphics.screen_mode)
  {
    // SMZX mode 1: set all colors to diagonals
    if(graphics.screen_mode == 1)
    {
      SDL_Color new_palette[256];
      for(i = 0; i < 256; i++)
      {
        new_palette[i].r =
         ((graphics.intensity_palette[i & 15].r << 1) +
          graphics.intensity_palette[i >> 4].r) / 3;
        new_palette[i].g =
         ((graphics.intensity_palette[i & 15].g << 1) +
          graphics.intensity_palette[i >> 4].g) / 3;
        new_palette[i].b =
         ((graphics.intensity_palette[i & 15].b << 1) +
          graphics.intensity_palette[i >> 4].b) / 3;
      }
      SDL_SetColors(graphics.screen, new_palette, 0, 256);
    }
    else
    {
      SDL_SetColors(graphics.screen, graphics.intensity_palette, 0, 256);
    }
  }
  else
  {
    SDL_SetColors(graphics.screen, graphics.intensity_palette, 0, 32);
  }
}

void load_palette(char *fname)
{
  FILE *pal_file = fopen(fname, "rb");

  if(pal_file)
  {
    int file_size;
    int i;
    int r, g, b;

    fseek(pal_file, 0, SEEK_END);
    file_size = ftell(pal_file);
    fseek(pal_file, 0, SEEK_SET);

    if(!graphics.screen_mode && (file_size > 48))
      file_size = 48;

    for(i = 0; i < file_size / 3; i++)
    {
      r = fgetc(pal_file);
      g = fgetc(pal_file);
      b = fgetc(pal_file);
      set_rgb(i, r, g, b);
    }

    fclose(pal_file);
  }
}

void load_palette_mem(char mem_pal[][3], int count)
{
  int i;

  for(i = 0; i < count; i++)
  {
    set_rgb(i, mem_pal[i][0], mem_pal[i][1], mem_pal[i][2]);
  }
}

void save_palette_mem(char mem_pal[][3], int count)
{
  int i;

  for(i = 0; i < count; i++)
  {
    mem_pal[i][0] = get_red_component(i);
    mem_pal[i][1] = get_green_component(i);
    mem_pal[i][2] = get_blue_component(i);
  }
}

void save_palette(char *fname)
{
  FILE *pal_file = fopen(fname, "wb");

  if(pal_file)
  {
    int num_colors = 256;
    int i;

    if(!graphics.screen_mode)
      num_colors = 16;

    for(i = 0; i < num_colors; i++)
    {
      fputc(get_red_component(i), pal_file);
      fputc(get_green_component(i), pal_file);
      fputc(get_blue_component(i), pal_file);
    }

    fclose(pal_file);
  }
}

void smzx_palette_loaded(int val)
{
  graphics.default_smzx_loaded = val;
}

void update_intensity_palette()
{
  int i;
  for(i = 0; i < 256; i++)
  {
    set_color_intensity(i, graphics.current_intensity[i]);
  }
}

void swap_palettes()
{
  SDL_Color temp_colors[256];
  Uint32 temp_intensities[256];
  memcpy(temp_colors, graphics.backup_palette,
   sizeof(SDL_Color) * 256);
  memcpy(graphics.backup_palette, graphics.palette,
   sizeof(SDL_Color) * 256);
  memcpy(graphics.palette, temp_colors,
   sizeof(SDL_Color) * 256);
  memcpy(temp_intensities, graphics.backup_intensity,
   sizeof(Uint32) * 256);
  if(graphics.fade_status)
  {
    memcpy(graphics.backup_intensity,
     graphics.saved_intensity, sizeof(Uint32) * 256);
    memcpy(graphics.saved_intensity, temp_intensities,
     sizeof(Uint32) * 256);
  }
  else
  {
    memcpy(graphics.backup_intensity,
     graphics.current_intensity, sizeof(Uint32) * 256);
    memcpy(graphics.current_intensity, temp_intensities,
     sizeof(Uint32) * 256);
    update_intensity_palette();
  }
}

void set_screen_mode(Uint32 mode)
{
  mode %= 4;

  if((mode >= 2) && (graphics.screen_mode < 2))
  {
    swap_palettes();
    graphics.screen_mode = mode;
    if(!graphics.default_smzx_loaded)
    {
      int i;

      if(graphics.fade_status)
      {
        for(i = 0; i < 256; i++)
        {
          graphics.current_intensity[i] = 0;
        }
      }
      set_palette_intensity(100);
      load_palette(SMZX_PAL);
      graphics.default_smzx_loaded = 1;
    }
    update_palette();
  }
  else

  if((graphics.screen_mode >= 2) && (mode < 2))
  {
    int fade_status = get_fade_status();
    int i;
    swap_palettes();

    if(fade_status)
    {
      set_fade_status(0);
      for(i = 16; i < 32; i++)
      {
        set_color_intensity(i, 100);
      }
      set_fade_status(fade_status);
    }
    graphics.screen_mode = mode;
    update_palette();
  }
  else
  {
    graphics.screen_mode = mode;
  }

  if(mode == 1)
  {
    update_palette();
  }
}

Uint32 get_screen_mode()
{
  return graphics.screen_mode;
}

void set_palette_intensity(Uint32 percent)
{
  Uint32 i, num_colors;

  if(graphics.screen_mode >= 2)
    num_colors = 256;
  else
    num_colors = 16;

  for(i = 0; i < num_colors; i++)
  {
    set_color_intensity(i, percent);
  }
}

void set_color_intensity(Uint32 color, Uint32 percent)
{
  if(graphics.fade_status)
  {
    graphics.saved_intensity[color] = percent;
  }
  else
  {
    int r = (graphics.palette[color].r * percent) / 100;
    int g = (graphics.palette[color].g * percent) / 100;
    int b = (graphics.palette[color].b * percent) / 100;

    if(r > 255)
      r = 255;

    if(g > 255)
      g = 255;

    if(b > 255)
      b = 255;

    graphics.intensity_palette[color].r = r;
    graphics.intensity_palette[color].g = g;
    graphics.intensity_palette[color].b = b;

    graphics.current_intensity[color] = percent;
  }
}

void set_rgb(Uint32 color, Uint32 r, Uint32 g, Uint32 b)
{
  int intensity = graphics.current_intensity[color];
  r = r * 255 / 63;
  g = g * 255 / 63;
  b = b * 255 / 63;

  graphics.palette[color].r = r;
  r = (r * intensity) / 100;
  if(r > 255)
    r = 255;
  graphics.intensity_palette[color].r = r;

  graphics.palette[color].g = g;
  g = (g * intensity) / 100;
  if(g > 255)
    g = 255;
  graphics.intensity_palette[color].g = g;

  graphics.palette[color].b = b;
  b = (b * intensity) / 100;
  if(b > 255)
    b = 255;
  graphics.intensity_palette[color].b = b;
}

void set_red_component(Uint32 color, Uint32 r)
{
  r = r * 255 / 63;
  graphics.palette[color].r = r;

  r = r * graphics.current_intensity[color] / 100;
  if(r > 255)
    r = 255;

  graphics.intensity_palette[color].r = r;
}

void set_green_component(Uint32 color, Uint32 g)
{
  g = g * 255 / 63;
  graphics.palette[color].g = g;

  g = g * graphics.current_intensity[color] / 100;
  if(g > 255)
    g = 255;

  graphics.intensity_palette[color].g = g;
}

void set_blue_component(Uint32 color, Uint32 b)
{
  b = b * 255 / 63;
  graphics.palette[color].b = b;

  b = b * graphics.current_intensity[color] / 100;
  if(b > 255)
    b = 255;

  graphics.intensity_palette[color].b = b;
}

Uint32 get_color_intensity(Uint32 color)
{
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

Uint32 get_fade_status()
{
  return graphics.fade_status;
}

void set_fade_status(Uint32 fade)
{
  graphics.fade_status = fade;
}

void get_palette(Uint8 *dest)
{
  Uint32 i, i2;

  for(i = 0, i2 = 0; i < 16; i++, i2 += 3)
  {
    dest[i2] = graphics.intensity_palette[i].r;
    dest[i2 + 1] = graphics.intensity_palette[i].g;
    dest[i2 + 2] = graphics.intensity_palette[i].b;
  }
}

void get_palette_smzx(Uint8 *dest)
{
  Uint32 i, i2;

  for(i = 0, i2 = 0; i < 16; i++, i2 += 3)
  {
    dest[i2] = graphics.intensity_palette[i].r;
    dest[i2 + 1] = graphics.intensity_palette[i].g;
    dest[i2 + 2] = graphics.intensity_palette[i].b;
  }
}

// Very quick fade out. Saves intensity table for fade in. Be sure
// to use in conjuction with the next function.
void vquick_fadeout(void)
{
  if(!graphics.fade_status)
  {
    Sint32 i, num_colors;
    Uint32 ticks;

    if(graphics.screen_mode >= 2)
      num_colors = 256;
    else
      num_colors = 16;

    memcpy(graphics.saved_intensity, graphics.current_intensity,
     sizeof(Uint32) * num_colors);

    for(i = 100; i >= 0; i -= 10)
    {
      ticks = SDL_GetTicks();
      set_palette_intensity(i);
      update_palette();
      ticks = SDL_GetTicks() - ticks;
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
  if(graphics.fade_status)
  {
    Uint32 i, i2, num_colors;
    Uint32 ticks;

    graphics.fade_status = 0;

    if(graphics.screen_mode >= 2)
      num_colors = 256;
    else
      num_colors = 16;

    for(i = 0; i < 10; i++)
    {
      ticks = SDL_GetTicks();
      for(i2 = 0; i2 < num_colors; i2++)
      {
        graphics.current_intensity[i2] += 10;
        if(graphics.current_intensity[i2] > graphics.saved_intensity[i2])
          graphics.current_intensity[i2] = graphics.saved_intensity[i2];
        set_color_intensity(i2, graphics.current_intensity[i2]);
      }
      update_palette();
      ticks = SDL_GetTicks() - ticks;
      if(ticks <= 16)
        delay(16 - ticks);
    }
  }
}

// Instant fade out
void insta_fadeout(void)
{
  if(!graphics.fade_status)
  {
    Uint32 i, num_colors;

    if(graphics.screen_mode >= 2)
      num_colors = 256;
    else
      num_colors = 16;

    for(i = 0; i < num_colors; i++)
    {
      set_color_intensity(i, 0);
    }

    delay(1);
    update_palette();
    graphics.fade_status = 1;
  }
}

// Instant fade in
void insta_fadein(void)
{
  if(graphics.fade_status)
  {
    Uint32 i, num_colors;

    graphics.fade_status = 0;

    if(graphics.screen_mode >= 2)
      num_colors = 256;
    else
      num_colors = 16;

    for(i = 0; i < num_colors; i++)
    {
      set_color_intensity(i, graphics.saved_intensity[i]);
    }

    update_palette();
  }
}

void default_palette(void)
{
  memcpy(graphics.palette, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.intensity_palette, default_pal, sizeof(SDL_Color) * 16);
  update_palette();
}

void m_hide(void)
{
  graphics.mouse_status = 0;
}

void m_show(void)
{
  graphics.mouse_status = 1;
}

void dump_screen()
{
  int i;
  char name[16];
  struct stat file_info;

  for(i = 0; i < 99999; i++)
  {
    sprintf(name, "screen%d.bmp", i);
    if(stat(name, &file_info))
      break;
  }
  SDL_SaveBMP(graphics.screen, name);
}

int get_resolution_w()
{
  return graphics.screen->w;
}

int get_resolution_h()
{
  return graphics.screen->h;
}

int get_height_multiplier()
{
  return graphics.height_multiplier;
}

void set_mouse_mul(int width_mul, int height_mul)
{
  graphics.mouse_width_mul = width_mul;
  graphics.mouse_height_mul = height_mul;
}
