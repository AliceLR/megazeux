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
#include "world.h"
#include "configure.h"
#include "event.h"
#include "render.h"
#include "renderers.h"
#include "platform.h"

#ifdef CONFIG_PNG
#include "pngops.h"
#endif

#ifdef CONFIG_SDL
#include "SDL.h"
#ifdef CONFIG_ICON
#include "SDL_syswm.h"
#endif // CONFIG_ICON
#endif // CONFIG_SDL

#include "util.h"

#define CURSOR_BLINK_RATE 115

__editor_maybe_static struct graphics_data graphics;
static bool graphics_was_initialized;

static const struct renderer_data renderers[] =
{
#if defined(CONFIG_RENDER_SOFT)
  { "software", render_soft_register },
#endif
#if defined(CONFIG_RENDER_GL_FIXED)
  { "opengl1", render_gl1_register },
  { "opengl2", render_gl2_register },
#endif
#if defined(CONFIG_RENDER_GL_PROGRAM)
  { "glsl", render_glsl_register },
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
#if defined(CONFIG_RENDER_GX)
  { "gx", render_gx_register },
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

void ec_change_byte(Uint8 chr, Uint8 byte, Uint8 new_value)
{
  graphics.charset[(chr * CHAR_SIZE) + byte] = new_value;

  if(graphics.renderer.remap_charbyte)
    graphics.renderer.remap_charbyte(&graphics, chr, byte);
}

void ec_change_char(Uint8 chr, char *matrix)
{
  memcpy(graphics.charset + (chr * CHAR_SIZE), matrix, CHAR_SIZE);

  if(graphics.renderer.remap_char)
    graphics.renderer.remap_char(&graphics, chr);
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
  FILE *fp = fopen_unsafe(name, "rb");

  if(fp == NULL)
   return -1;

  fread(graphics.charset, CHAR_SIZE, CHARSET_SIZE, fp);

  fclose(fp);

  // some renderers may want to map charsets to textures
  if(graphics.renderer.remap_charsets)
    graphics.renderer.remap_charsets(&graphics);

  return 0;
}

__editor_maybe_static void ec_load_set_secondary(const char *name,
 Uint8 *dest)
{
  FILE *fp = fopen_unsafe(name, "rb");

  if(!fp)
    return;

  fread(dest, CHAR_SIZE, CHARSET_SIZE, fp);
  fclose(fp);

  // some renderers may want to map charsets to textures
  if(graphics.renderer.remap_charsets)
    graphics.renderer.remap_charsets(&graphics);
}

Sint32 ec_load_set_var(char *name, Uint8 pos)
{
  Uint32 size = CHARSET_SIZE;
  FILE *fp = fopen_unsafe(name, "rb");

  if(!fp)
    return -1;

  size = ftell_and_rewind(fp) / CHAR_SIZE;
  if(size + pos > CHARSET_SIZE)
    size = CHARSET_SIZE - pos;

  fread(graphics.charset + (pos * CHAR_SIZE), CHAR_SIZE, size, fp);
  fclose(fp);

  // some renderers may want to map charsets to textures
  if(graphics.renderer.remap_charsets)
    graphics.renderer.remap_charsets(&graphics);

  return size;
}

void ec_mem_load_set(Uint8 *chars)
{
  memcpy(graphics.charset, chars, CHAR_SIZE * CHARSET_SIZE);

  // some renderers may want to map charsets to textures
  if(graphics.renderer.remap_charsets)
    graphics.renderer.remap_charsets(&graphics);
}

void ec_mem_save_set(Uint8 *chars)
{
  memcpy(chars, graphics.charset, CHAR_SIZE * CHARSET_SIZE);
}

__editor_maybe_static void ec_load_mzx(void)
{
  ec_mem_load_set(graphics.default_charset);
}

static void update_colors(struct rgb_color *palette, Uint32 count)
{
  graphics.renderer.update_colors(&graphics, palette, count);
}

static Uint32 make_palette(struct rgb_color *palette)
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
      memcpy(palette, graphics.intensity_palette,
       sizeof(struct rgb_color) * SMZX_PAL_SIZE);
    }
    return SMZX_PAL_SIZE;
  }
  else
  {
    memcpy(palette, graphics.intensity_palette,
     sizeof(struct rgb_color) * PAL_SIZE * NUM_PALS);
    return PAL_SIZE * NUM_PALS;
  }
}

void update_palette(void)
{
  struct rgb_color new_palette[SMZX_PAL_SIZE];
  update_colors(new_palette, make_palette(new_palette));
}

void set_gui_palette(void)
{
  int i;

  memcpy(graphics.palette + PAL_SIZE, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.intensity_palette + PAL_SIZE, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);

  for(i = 16; i < PAL_SIZE * NUM_PALS; i++)
    graphics.current_intensity[i] = 100;
}

static void init_palette(void)
{
  Uint32 i;

  memcpy(graphics.palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.intensity_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memset(graphics.current_intensity, 0,
   sizeof(Uint32) * PAL_SIZE);

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
  if(get_fade_status())
  {
    clear_screen(32, 0);
    insta_fadein();
  }
}

void dialog_fadeout(void)
{
  if(get_fade_status())
  {
    insta_fadeout();
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
        for(i = 0; i < SMZX_PAL_SIZE; i++)
        {
          graphics.current_intensity[i] = 0;
        }
      }
      set_palette_intensity(100);
      load_palette(mzx_res_get_by_id(SMZX_PAL));
      graphics.default_smzx_loaded = 1;
    }
    update_palette();
  }
  else

  if((graphics.screen_mode >= 2) && (mode < 2))
  {
    swap_palettes();

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
  Uint32 ticks = get_ticks();

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  graphics.renderer.render_graph(&graphics);
  if(graphics.cursor_flipflop &&
   (graphics.cursor_mode != cursor_mode_invisible))
  {
    struct char_element *cursor_element =
     graphics.text_video + graphics.cursor_x + (graphics.cursor_y * SCREEN_W);
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
      ticks = get_ticks();
      set_palette_intensity(i);
      update_palette();
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
      ticks = get_ticks();
      for(i2 = 0; i2 < num_colors; i2++)
      {
        graphics.current_intensity[i2] += 10;
        if(graphics.current_intensity[i2] > graphics.saved_intensity[i2])
          graphics.current_intensity[i2] = graphics.saved_intensity[i2];
        set_color_intensity(i2, graphics.current_intensity[i2]);
      }
      update_palette();
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
  memcpy(graphics.palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  memcpy(graphics.intensity_palette, default_pal,
   sizeof(struct rgb_color) * PAL_SIZE);
  update_palette();
}

static bool set_graphics_output(struct config_info *conf)
{
  const char *video_output = conf->video_output;
  const struct renderer_data *renderer = renderers;

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
  }

  // If no match found, use first renderer in the renderer list
  if(!renderer->name)
    renderer = renderers;

  renderer->reg(&graphics.renderer);

  debug("Video: using '%s' renderer.\n", renderer->name);
  return true;
}

#if defined(CONFIG_PNG) && defined(CONFIG_SDL) && \
    defined(CONFIG_ICON) && !defined(__WIN32__)

static bool icon_w_h_constraint(png_uint_32 w, png_uint_32 h)
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

bool init_video(struct config_info *conf, const char *caption)
{
  graphics.screen_mode = 0;
  graphics.fullscreen = conf->fullscreen;
  graphics.resolution_width = conf->resolution_width;
  graphics.resolution_height = conf->resolution_height;
  graphics.window_width = conf->window_width;
  graphics.window_height = conf->window_height;
  graphics.mouse_status = false;
  graphics.cursor_timestamp = get_ticks();
  graphics.cursor_flipflop = 1;
  graphics.system_mouse = conf->system_mouse;

  if(!set_graphics_output(conf))
    return false;

#ifdef CONFIG_SDL
  SDL_WM_SetCaption(caption, "");
#endif

  // These values (the defaults, actually) are special and tell MZX to try
  // to use the current desktop resolution as the fullscreen resolution
  if(conf->resolution_width == -1 && conf->resolution_height == -1)
  {
    graphics.resolution_width = 640;
    graphics.resolution_height = 480;

#ifdef CONFIG_SDL
#if !(SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2 && SDL_PATCHLEVEL < 10)
    // We'll only do this for hardware scaling renderers
    if(strcmp(conf->video_output, "software") != 0)
    {
      const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
      graphics.resolution_width = video_info->current_w;
      graphics.resolution_height = video_info->current_h;
    }
#endif
#endif // CONFIG_SDL
  }

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
      SDL_SysWMinfo info;

      SDL_VERSION(&info.version);
      SDL_GetWMInfo(&info);

      SendMessage(info.window, WM_SETICON, ICON_BIG, (LPARAM)icon);
    }
  }
#else // !__WIN32__
#if defined(CONFIG_PNG)
  {
    SDL_Surface *icon = png_read_icon("/usr/share/icons/megazeux.png");
    if(icon)
    {
      SDL_WM_SetIcon(icon, NULL);
      SDL_FreeSurface(icon);
    }
  }
#endif // CONFIG_PNG
#endif // __WIN32__
#endif // CONFIG_SDL && CONFIG_ICON

  ec_load_set_secondary(mzx_res_get_by_id(MZX_DEFAULT_CHR),
   graphics.default_charset);
  ec_load_set_secondary(mzx_res_get_by_id(MZX_EDIT_CHR),
   graphics.charset + (CHARSET_SIZE * CHAR_SIZE));

  ec_load_mzx();
  init_palette();
  graphics_was_initialized = true;
  return true;
}

bool has_video_initialized(void)
{
  return graphics_was_initialized;
}

bool set_video_mode(void)
{
  int target_width, target_height;
  int target_depth = graphics.bits_per_pixel;
  bool fullscreen = graphics.fullscreen;
  bool resize = graphics.allow_resize;

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

  return graphics.renderer.set_video_mode(&graphics,
   target_width, target_height, target_depth, fullscreen, resize);
}

#if 0

static bool change_video_output(struct config_info *conf, const char *output)
{
  char old_video_output[16];

  strncpy(old_video_output, conf->video_output, 16);
  old_video_output[15] = 0;

  strncpy(conf->video_output, output, 16);
  conf->video_output[15] = 0;

  if(graphics.renderer.free_video)
    graphics.renderer.free_video(&graphics);

  set_graphics_output(conf);

  if(!graphics.renderer.init_video(&graphics, conf))
  {
    strcpy(conf->video_output, old_video_output);
    if(!set_graphics_output(conf))
    {
      warn("Failed to roll back renderer, aborting!\n");
      exit(0);
    }

    if(!graphics.renderer.init_video(&graphics, conf))
    {
      warn("Failed to roll back video mode, aborting!\n");
      exit(0);
    }
  }

  return true;
}

#endif

void toggle_fullscreen(void)
{
  graphics.fullscreen = !graphics.fullscreen;
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
    graphics.renderer.resize_screen(&graphics, w, h);
  }
}

void color_string_ext_special(const char *str, Uint32 x, Uint32 y,
 Uint8 *color, Uint32 offset, Uint32 c_offset, bool respect_newline)
{
  struct char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 next;
  Uint8 bg_color = (*color >> 4) + c_offset;
  Uint8 fg_color = (*color & 0x0F) + c_offset;

  char next_str[2];
  next_str[1] = 0;

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
          dest = graphics.text_video + (y * SCREEN_W) + x;
          break;
        }
        // Fall thru
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

exit_out:
  *color = (((bg_color - c_offset) << 4) & 0xF0) |
           (((fg_color - c_offset) << 0) & 0x0F);
}

void color_string_ext(const char *str, Uint32 x, Uint32 y, Uint8 color,
 Uint32 offset, Uint32 c_offset, bool respect_newline)
{
  color_string_ext_special(str, x, y, &color, offset,
   c_offset, respect_newline);
}

// Write a normal string

void write_string_ext(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  struct char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;

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
  struct char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;

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
  struct char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;

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
  struct char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  const char *src = str;
  Uint8 cur_char = *src;
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;

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

// Set rightalign to print the rightmost char at xy and proceed to the left
// minlen is the minimum length to print. Pad with 0.

void write_number(int number, char color, int x, int y,
 int minlen, int rightalign, int base)
{
  char temp[12];
  int t1, t2;

  if(base == 10)
    snprintf(temp, 12, "%d", number);
  else
    snprintf(temp, 12, "%x", number);

  temp[11] = 0;

  if(rightalign)
  {
    t1 = (int)strlen(temp);
    if(minlen > t1)
      t1 = minlen;
    x -= t1 - 1;
  }

  if((t2 = (int)strlen(temp)) < minlen)
  {
    t2 = minlen - t2;
    for(t1 = 0; t1 < t2; t1++)
      draw_char('0', color, x++, y);
  }

  write_string(temp, x, y, color, 0);
}

static void color_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 color, Uint32 offset, Uint32 c_offset)
{
  struct char_element *dest = graphics.text_video + (y * 80) + x;
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
  struct char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
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
  struct char_element *dest = graphics.text_video + (y * SCREEN_W) + x;
  dest->char_value = chr + offset;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

void draw_char_linear_ext(Uint8 color, Uint8 chr,
 Uint32 offset, Uint32 offset_b, Uint32 c_offset)
{
  struct char_element *dest = graphics.text_video + offset;
  dest->char_value = chr + offset_b;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

void color_string(const char *string, Uint32 x, Uint32 y, Uint8 color)
{
  color_string_ext(string, x, y, color, 256, 16, false);
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
  struct char_element *dest = graphics.text_video + offset;
  return (dest->bg_color << 4) | (dest->fg_color & 0x0F);
}

void clear_screen(Uint8 chr, Uint8 color)
{
  Uint32 i;
  Uint8 fg_color = color & 0x0F;
  Uint8 bg_color = color >> 4;
  struct char_element *dest = graphics.text_video;

  for(i = 0; i < (SCREEN_W * SCREEN_H); i++)
  {
    dest->char_value = chr;
    dest->fg_color = fg_color;
    dest->bg_color = bg_color;
    dest++;
  }
  update_screen();
}

void set_screen(struct char_element *src)
{
  memcpy(graphics.text_video, src,
   SCREEN_W * SCREEN_H * sizeof(struct char_element));
}

void get_screen(struct char_element *dest)
{
  memcpy(dest, graphics.text_video,
   SCREEN_W * SCREEN_H * sizeof(struct char_element));
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
static void dump_screen_real(Uint8 *pix, struct rgb_color *pal, int count,
 const char *name)
{
  png_write_screen(pix, pal, count, name);
}

#else

#define DUMP_FMT_EXT "bmp"

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

#endif // CONFIG_PNG

#define MAX_NAME_SIZE 16

void dump_screen(void)
{
  struct rgb_color palette[SMZX_PAL_SIZE];
  char name[MAX_NAME_SIZE];
  struct stat file_info;
  Uint8 *ss;
  int i;

  for(i = 0; i < 99999; i++)
  {
    snprintf(name, MAX_NAME_SIZE - 1, "screen%d.%s", i, DUMP_FMT_EXT);
    name[MAX_NAME_SIZE - 1] = '\0';
    if(stat(name, &file_info))
      break;
  }

  ss = cmalloc(sizeof(Uint8) * 640 * 350);
  if(ss)
  {
    render_graph8(ss, 640, &graphics, set_colors8[graphics.screen_mode]);
    dump_screen_real(ss, palette, make_palette(palette), name);
    free(ss);
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
