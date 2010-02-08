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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "graphics.h"
#include "delay.h"
#include "world.h"
#include "configure.h"
#include "event.h"
#include "render.h"
#include "renderers.h"
#include "util.h"

#ifndef VERSION
#error Must define VERSION for MegaZeux version string
#endif

// Base names for MZX's resource files.
// Prepends SHAREDIR from config.h for you, so these are ready to use.

#define MZX_DEFAULT_CHR SHAREDIR "mzx_default.chr"
#define MZX_BLANK_CHR   SHAREDIR "mzx_blank.chr"
#define MZX_SMZX_CHR    SHAREDIR "mzx_smzx.chr"
#define MZX_ASCII_CHR   SHAREDIR "mzx_ascii.chr"
#define MZX_EDIT_CHR    SHAREDIR "mzx_edit.chr"
#define SMZX_PAL        SHAREDIR "smzx.pal"

static graphics_data graphics;

static const renderer_data renderers[] =
{
#if defined(CONFIG_SOFTWARE)
  { "software", render_soft_register },
#endif
#if defined(CONFIG_OPENGL)
  { "opengl1", render_gl1_register },
  { "opengl2", render_gl2_register },
#endif
#if defined(CONFIG_OVERLAY)
  { "overlay1", render_yuv1_register },
  { "overlay2", render_yuv2_register },
#endif
#if defined(CONFIG_GP2X)
  { "gp2x", render_gp2x_register },
#endif
  { NULL, NULL }
};

static const SDL_Color default_pal[16] =
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

void ec_change_byte(Uint8 chr, Uint8 byte, Uint8 new_value)
{
  graphics.charset[(chr * CHAR_SIZE) + byte] = new_value;

  if(graphics.remap_charbyte)
    graphics.remap_charbyte(&graphics, chr, byte);
}

void ec_change_char(Uint8 chr, char *matrix)
{
  memcpy(graphics.charset + (chr * CHAR_SIZE), matrix, CHAR_SIZE);

  if(graphics.remap_char)
    graphics.remap_char(&graphics, chr);
}

Uint8 ec_read_byte(Uint8 chr, Uint8 byte)
{
  return graphics.charset[(chr * CHAR_SIZE) + byte];
}

void ec_read_char(Uint8 chr, char *matrix)
{
  memcpy(matrix, graphics.charset + (chr * CHAR_SIZE), CHAR_SIZE);
}

Sint32 ec_load_set(char *name)
{
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
   return -1;

  fread(graphics.charset, CHAR_SIZE, CHARSET_SIZE, fp);

  fclose(fp);

  // some renderers may want to map charsets to textures
  if(graphics.remap_charsets)
    graphics.remap_charsets(&graphics);

  return 0;
}

static Sint32 ec_load_set_secondary(const char *name, Uint8 *dest)
{
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  fread(dest, CHAR_SIZE, CHARSET_SIZE, fp);

  fclose(fp);

  // some renderers may want to map charsets to textures
  if(graphics.remap_charsets)
    graphics.remap_charsets(&graphics);

  return 0;
}

Sint32 ec_load_set_var(char *name, Uint8 pos)
{
  Uint32 size = CHARSET_SIZE;
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  size = ftell_and_rewind(fp) / CHAR_SIZE;
  if(size + pos > CHARSET_SIZE)
    size = CHARSET_SIZE - pos;

  fread(graphics.charset + (pos * CHAR_SIZE), CHAR_SIZE, size, fp);
  fclose(fp);

  // some renderers may want to map charsets to textures
  if(graphics.remap_charsets)
    graphics.remap_charsets(&graphics);

  return size;
}

void ec_mem_load_set(Uint8 *chars)
{
  memcpy(graphics.charset, chars, CHAR_SIZE * CHARSET_SIZE);

  // some renderers may want to map charsets to textures
  if(graphics.remap_charsets)
    graphics.remap_charsets(&graphics);
}

void ec_mem_save_set(Uint8 *chars)
{
  memcpy(chars, graphics.charset, CHAR_SIZE * CHARSET_SIZE);
}

__editor_maybe_static void ec_load_mzx(void)
{
  ec_mem_load_set(graphics.default_charset);
}

static void update_colors(SDL_Color *palette, Uint32 count)
{
  graphics.update_colors(&graphics, palette, count);
}

static Uint32 make_palette(SDL_Color *palette)
{
  Uint32 i;

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
      memcpy(palette, graphics.intensity_palette, sizeof(SDL_Color) *
       SMZX_PAL_SIZE);
    }
    return SMZX_PAL_SIZE;
  }
  else
  {
    memcpy(palette, graphics.intensity_palette, sizeof(SDL_Color) * PAL_SIZE *
     NUM_PALS);
    return PAL_SIZE * NUM_PALS;
  }
}

void update_palette(void)
{
  SDL_Color new_palette[SMZX_PAL_SIZE];
  update_colors(new_palette, make_palette(new_palette));
}

static void set_gui_palette(void)
{
  int i;

  memcpy(graphics.palette + PAL_SIZE, default_pal,
   sizeof(SDL_Color) * PAL_SIZE);
  memcpy(graphics.intensity_palette + PAL_SIZE, default_pal,
   sizeof(SDL_Color) * PAL_SIZE);

  for(i = 16; i < PAL_SIZE * NUM_PALS; i++)
    graphics.current_intensity[i] = 100;
}

static void init_palette(void)
{
  Uint32 i;

  memcpy(graphics.palette, default_pal, sizeof(SDL_Color) * PAL_SIZE);
  memcpy(graphics.intensity_palette, default_pal, sizeof(SDL_Color) * PAL_SIZE);
  memset(graphics.current_intensity, 0, sizeof(Uint32) * PAL_SIZE);

  for(i = 0; i < SMZX_PAL_SIZE; i++)
    graphics.saved_intensity[i] = 100;

  graphics.fade_status = 1;

  set_gui_palette();
  update_palette();
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

void load_palette(const char *fname)
{
  int file_size, i, r, g, b;
  FILE *pal_file;
  
  pal_file = fopen(fname, "rb");
  if(!pal_file)
    return;

  file_size = ftell_and_rewind(pal_file);

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
  SDL_Color temp_colors[SMZX_PAL_SIZE];
  Uint32 temp_intensities[SMZX_PAL_SIZE];
  memcpy(temp_colors, graphics.backup_palette,
   sizeof(SDL_Color) * SMZX_PAL_SIZE);
  memcpy(graphics.backup_palette, graphics.palette,
   sizeof(SDL_Color) * SMZX_PAL_SIZE);
  memcpy(graphics.palette, temp_colors,
   sizeof(SDL_Color) * SMZX_PAL_SIZE);
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

static void set_fade_status(Uint32 fade)
{
  graphics.fade_status = fade;
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
        for(i = 0; i < SMZX_PAL_SIZE; i++)
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
    swap_palettes();

    if(fade_status)
    {
      set_fade_status(0);
      set_fade_status(fade_status);
    }
    graphics.screen_mode = mode;

    set_gui_palette();
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

Uint32 get_screen_mode(void)
{
  return graphics.screen_mode;
}

void update_screen(void)
{
  Uint32 ticks = SDL_GetTicks();

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  graphics.render_graph(&graphics);
  if(graphics.cursor_flipflop &&
   (graphics.cursor_mode != cursor_mode_invisible))
  {
    char_element *cursor_element = graphics.text_video + graphics.cursor_x +
     (graphics.cursor_y * SCREEN_W);
    Uint8 cursor_color;
    Uint32 cursor_char = cursor_element->char_value;
    Uint32 lines = 0;
    Uint32 offset = 0;
    Uint32 i;
    Uint32 cursor_solid = 0xFFFFFFFF;
    Uint32 *char_offset = (Uint32 *)(graphics.charset +
     (cursor_char * CHAR_SIZE));
    Uint32 bg_color = cursor_element->bg_color;

    // Choose FG
    cursor_color = cursor_element->fg_color;

    // See if the cursor char is completely solid or completely empty
    for(i = 0; i < 3; i++)
    {
      cursor_solid &= *char_offset;
      char_offset++;
    }
    cursor_solid &= (*((Uint16 *)char_offset)) | 0xFFFF0000;
    if(cursor_solid == 0xFFFFFFFF)
    {
      // But wait! What if the background is the same as the foreground?
      // If so, use +8 instead.
      if(bg_color == cursor_color)
        cursor_color = bg_color ^ 8;
      else
        cursor_color = bg_color;
    }
    else
      if(bg_color == cursor_color)
        cursor_color = cursor_color ^ 8;

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

    if(graphics.screen_mode)
    {
      if(graphics.screen_mode != 3)
        cursor_color = (cursor_color << 4) | (cursor_color & 0x0F);
      else
        cursor_color = ((bg_color << 4) | (cursor_color & 0x0F)) + 3;
    }

    graphics.render_cursor(&graphics, graphics.cursor_x, graphics.cursor_y,
     cursor_color, lines, offset);
  }
  if(graphics.mouse_status)
  {
    int mouse_x, mouse_y;
    get_real_mouse_position(&mouse_x, &mouse_y);

    mouse_x = (mouse_x / graphics.mouse_width_mul) * graphics.mouse_width_mul;
    mouse_y = (mouse_y / graphics.mouse_height_mul) * graphics.mouse_height_mul;

    graphics.render_mouse(&graphics, mouse_x, mouse_y, graphics.mouse_width_mul,
     graphics.mouse_height_mul);
  }
  graphics.sync_screen(&graphics);
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
      num_colors = SMZX_PAL_SIZE;
    else
      num_colors = PAL_SIZE;

    memcpy(graphics.saved_intensity, graphics.current_intensity,
     sizeof(Uint32) * num_colors);

    for(i = 100; i >= 0; i -= 10)
    {
      ticks = SDL_GetTicks();
      set_palette_intensity(i);
      update_palette();
      update_screen();
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
      num_colors = SMZX_PAL_SIZE;
    else
      num_colors = PAL_SIZE;

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
      update_screen();
      ticks = SDL_GetTicks() - ticks;
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

  for(i = 0; i < num_colors; i++)
    set_color_intensity(i, 0);

  delay(1);
  update_palette();
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

  update_palette();
  update_screen(); // NOTE: this was called conditionally in 2.81e
}

void default_palette(void)
{
  memcpy(graphics.palette, default_pal, sizeof(SDL_Color) * PAL_SIZE);
  memcpy(graphics.intensity_palette, default_pal, sizeof(SDL_Color) * PAL_SIZE);
  update_palette();
}

static void set_graphics_output(const char *video_output)
{
  const renderer_data *renderer = renderers;

  while(renderer->name)
  {
    if(!strcasecmp(video_output, renderer->name))
      break;
    renderer++;
  }

  // If no match found, use first renderer in the renderer list
  if(!(renderer->name))
    renderer = renderers;

  renderer->reg(&graphics);

#ifdef DEBUG
  fprintf(stdout, "Selected video output: %s\n", renderer->name);
#endif
}

void init_video(config_info *conf)
{
  graphics.screen_mode = 0;
  graphics.fullscreen = conf->fullscreen;
  graphics.resolution_width = conf->resolution_width;
  graphics.resolution_height = conf->resolution_height;
  graphics.window_width = conf->window_width;
  graphics.window_height = conf->window_height;
  graphics.mouse_status = 0;
  graphics.cursor_timestamp = SDL_GetTicks();
  graphics.cursor_flipflop = 1;

  set_graphics_output(conf->video_output);

  SDL_WM_SetCaption("MegaZeux " VERSION, "MZX");
  SDL_ShowCursor(SDL_DISABLE);

  if(!(graphics.init_video(&graphics, conf)))
  {
    set_graphics_output("");
    graphics.init_video(&graphics, conf);
  }

  ec_load_set_secondary(MZX_DEFAULT_CHR, graphics.default_charset);
  ec_load_set_secondary(MZX_BLANK_CHR, graphics.blank_charset);
  ec_load_set_secondary(MZX_SMZX_CHR, graphics.smzx_charset);
  ec_load_set_secondary(MZX_ASCII_CHR, graphics.ascii_charset);
  ec_load_set_secondary(MZX_EDIT_CHR, graphics.charset + (CHARSET_SIZE * CHAR_SIZE));
  ec_load_mzx();

  init_palette();
}

int set_video_mode(void)
{
  int target_width, target_height;
  int target_depth = graphics.bits_per_pixel;
  int target_flags = 0;
  int fullscreen = graphics.fullscreen;

  if(fullscreen)
  {
    target_width = graphics.resolution_width;
    target_height = graphics.resolution_height;
    target_flags |= SDL_FULLSCREEN;

    if(target_depth == 8)
      target_flags |= SDL_HWPALETTE;
  }
  else
  {
    if(graphics.allow_resize)
      target_flags |= SDL_RESIZABLE;

    target_width = graphics.window_width;
    target_height = graphics.window_height;
  }

#ifdef PSP_BUILD
  target_width = 640;
  target_height = 363;
  target_depth = 8;
  fullscreen = 1;
#endif

  // If video mode fails, replace it with 'safe' defaults
  if(!(graphics.check_video_mode(&graphics, target_width, target_height,
                                 target_depth, target_flags)))
  {
    target_width = 640;
    target_height = 350;
    target_depth = 8;
    fullscreen = 0;

    graphics.resolution_width = target_width;
    graphics.resolution_height = target_height;
    graphics.bits_per_pixel = target_depth;
    graphics.fullscreen = fullscreen;

    target_flags = SDL_SWSURFACE;
  }

  return graphics.set_video_mode(&graphics, target_width, target_height,
                                 target_depth, target_flags, fullscreen);
}

void toggle_fullscreen(void)
{
  graphics.fullscreen ^= 1;
  set_video_mode();
  update_screen();
  update_palette();
}

void resize_screen(Uint32 w, Uint32 h)
{
  if(!graphics.fullscreen && graphics.allow_resize)
  {
    graphics.window_width = w;
    graphics.window_height = h;
    set_video_mode();
    graphics.resize_screen(&graphics, w, h);
  }
}

void color_string_ext(const char *str, Uint32 x, Uint32 y, Uint8 color,
 Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
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
        dest = graphics.text_video + (y * SCREEN_W) + x;
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
    if(dest >= graphics.text_video + (SCREEN_W * SCREEN_H))
      break;

    str++;
    cur_char = *str;
  }
}

// Write a normal string

void write_string_ext(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
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
        dest = graphics.text_video + (y * SCREEN_W) + x;
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
    if(dest >= graphics.text_video + (SCREEN_W * SCREEN_H))
      break;

    str++;
    cur_char = *str;
  }
}

void write_string_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
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
        dest = graphics.text_video + (y * SCREEN_W) + x;
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
    if(dest >= graphics.text_video + (SCREEN_W * SCREEN_H))
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
  char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
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

void write_line_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
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

static void color_line_ext(Uint32 length, Uint32 x, Uint32 y,
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
  char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
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
  char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  dest->char_value = chr + offset;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

void draw_char_linear_ext(Uint8 color, Uint8 chr,
 Uint32 offset, Uint32 offset_b, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + offset;
  dest->char_value = chr + offset_b;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

void color_string(const char *string, Uint32 x, Uint32 y, Uint8 color)
{
  color_string_ext(string, x, y, color, 256, 16);
}

void write_string(const char *string, Uint32 x, Uint32 y, Uint8 color,
 Uint32 tab_allowed)
{
  write_string_ext(string, x, y, color, tab_allowed, 256, 16);
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

Uint8 get_color_linear(Uint32 offset)
{
  char_element *dest = graphics.text_video + offset;
  return (dest->bg_color << 4) | (dest->fg_color & 0x0F);
}

void clear_screen(Uint8 chr, Uint8 color)
{
  Uint32 i;
  Uint8 fg_color = color & 0x0F;
  Uint8 bg_color = color >> 4;
  char_element *dest = graphics.text_video;

  for(i = 0; i < (SCREEN_W * SCREEN_H); i++)
  {
    dest->char_value = chr;
    dest->fg_color = fg_color;
    dest->bg_color = bg_color;
    dest++;
  }
  update_screen();
}

void set_screen(char_element *src)
{
  memcpy(graphics.text_video, src,
   SCREEN_W * SCREEN_H * sizeof(char_element));
}

void get_screen(char_element *dest)
{
  memcpy(dest, graphics.text_video,
   SCREEN_W * SCREEN_H * sizeof(char_element));
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

cursor_mode_types get_cursor_mode(void)
{
  return graphics.cursor_mode;
}

void m_hide(void)
{
  graphics.mouse_status = 0;
}

void m_show(void)
{
  graphics.mouse_status = 1;
}

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
static void dump_screen_real(SDL_Surface *surface, const char *name)
{
  const SDL_Palette *palette = surface->format->palette;
  png_bytep *row_ptrs;
  png_structp png_ptr;
  png_infop info_ptr;
  png_colorp pal_ptr;
  int i, type;
  FILE *file;

  file = fopen(name, "wb");
  if(!file)
    return;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr)
    return;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr)
    return;

  if(setjmp(png_jmpbuf(png_ptr)))
  {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(file);
    return;
  }

  png_init_io(png_ptr, file);

  // we know we have an 8-bit surface; save a palettized PNG
  type = PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE;
  png_set_IHDR(png_ptr, info_ptr, surface->w, surface->h, 8, type,
   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
   PNG_FILTER_TYPE_DEFAULT);

  pal_ptr = malloc(palette->ncolors * sizeof(png_color));
  for(i = 0; i < palette->ncolors; i++)
  {
    pal_ptr[i].red = palette->colors[i].r;
    pal_ptr[i].green = palette->colors[i].g;
    pal_ptr[i].blue = palette->colors[i].b;
  }
  png_set_PLTE(png_ptr, info_ptr, pal_ptr, palette->ncolors);

  // do the write of the header
  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);

  // and then the surface
  row_ptrs = malloc(sizeof(png_bytep) * surface->h);
  for(i = 0; i < surface->h; i++)
    row_ptrs[i] = (png_bytep)(Uint8 *)surface->pixels + i * surface->pitch;
  png_write_image(png_ptr, row_ptrs);
  png_write_end(png_ptr, info_ptr);

  free(pal_ptr);
  free(row_ptrs);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(file);
}

#else

#define DUMP_FMT_EXT "bmp"

static void dump_screen_real(SDL_Surface *surface, const char *name)
{
  SDL_SaveBMP(surface, name);
}

#endif // CONFIG_PNG

#define MAX_NAME_SIZE 16

void dump_screen(void)
{
  int i;
  char name[MAX_NAME_SIZE];
  struct stat file_info;
  SDL_Surface *ss;
  SDL_Color palette[SMZX_PAL_SIZE];

  for(i = 0; i < 99999; i++)
  {
    snprintf(name, MAX_NAME_SIZE - 1, "screen%d.%s", i, DUMP_FMT_EXT);
    name[MAX_NAME_SIZE - 1] = '\0';
    if(stat(name, &file_info))
      break;
  }

  ss = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 350, 8, 0, 0, 0, 0);
  if(ss)
  {
    SDL_SetColors(ss, palette, 0, make_palette(palette));
    SDL_LockSurface(ss);
    render_graph8((Uint8*)ss->pixels, ss->pitch, &graphics,
     set_colors8[graphics.screen_mode]);
    SDL_UnlockSurface(ss);
    dump_screen_real(ss, name);
    SDL_FreeSurface(ss);
  }
}

void get_screen_coords(int screen_x, int screen_y, int *x, int *y,
 int *min_x, int *min_y, int *max_x, int *max_y)
{
  graphics.get_screen_coords(&graphics, screen_x, screen_y, x, y, min_x, min_y,
   max_x, max_y);
}

void set_screen_coords(int x, int y, int *screen_x, int *screen_y)
{
  graphics.set_screen_coords(&graphics, x, y, screen_x, screen_y);
}

void set_mouse_mul(int width_mul, int height_mul)
{
  graphics.mouse_width_mul = width_mul;
  graphics.mouse_height_mul = height_mul;
}

#ifdef CONFIG_EDITOR

void save_editor_palette(void)
{
  if(graphics.screen_mode < 2)
    memcpy(graphics.editor_backup_palette, graphics.palette,
     sizeof(SDL_Color) * SMZX_PAL_SIZE);
}

void load_editor_palette(void)
{
  memcpy(graphics.palette, graphics.editor_backup_palette,
   sizeof(SDL_Color) * SMZX_PAL_SIZE);
}

void save_palette(char *fname)
{
  FILE *pal_file = fopen(fname, "wb");

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

void clear_screen_no_update(Uint8 chr, Uint8 color)
{
  Uint32 i;
  Uint8 fg_color = color & 0x0F;
  Uint8 bg_color = color >> 4;
  char_element *dest = graphics.text_video;

  for(i = 0; i < (SCREEN_W * SCREEN_H); i++)
  {
    dest->char_value = chr;
    dest->fg_color = fg_color;
    dest->bg_color = bg_color;
    dest++;
  }
}

void ec_save_set_var(char *name, Uint8 offset, Uint32 size)
{
  FILE *fp = fopen(name, "wb");

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

void ec_load_char_ascii(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * CHAR_SIZE),
   graphics.ascii_charset + (char_number * CHAR_SIZE), CHAR_SIZE);

  // some renderers may want to map charsets to textures
  if(graphics.remap_charsets)
    graphics.remap_charsets(&graphics);
}

void ec_load_char_mzx(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * CHAR_SIZE),
   graphics.default_charset + (char_number * CHAR_SIZE), CHAR_SIZE);

  // some renderers may want to map charsets to textures
  if(graphics.remap_charsets)
    graphics.remap_charsets(&graphics);
}

#endif // CONFIG_EDITOR
