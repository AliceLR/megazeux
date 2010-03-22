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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __GRAPHICS_H
#define __GRAPHICS_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform.h"
#include "configure.h"

enum cursor_mode_types
{
  cursor_mode_underline,
  cursor_mode_solid,
  cursor_mode_invisible
};

struct rgb_color
{
  Uint8 r;
  Uint8 g;
  Uint8 b;
  Uint8 unused;
};

struct char_element
{
  Uint16 char_value;
  Uint8 bg_color;
  Uint8 fg_color;
};

#define SCREEN_W 80
#define SCREEN_H 25

#define CHAR_W 8
#define CHAR_H 14

#define CHAR_SIZE 14
#define CHARSET_SIZE 256
#define NUM_CHARSETS 16

#define PAL_SIZE 16
#define NUM_PALS 2
#define SMZX_PAL_SIZE 256

struct graphics_data;

struct renderer
{
  bool (*init_video)       (struct graphics_data *, struct config_info *);
  void (*free_video)       (struct graphics_data *);
  bool (*check_video_mode) (struct graphics_data *, int, int, int, bool, bool);
  bool (*set_video_mode)   (struct graphics_data *, int, int, int, bool, bool);
  void (*update_colors)    (struct graphics_data *, struct rgb_color *,
                             Uint32);
  void (*resize_screen)    (struct graphics_data *, int, int);
  void (*remap_charsets)   (struct graphics_data *);
  void (*remap_char)       (struct graphics_data *, Uint16 chr);
  void (*remap_charbyte)   (struct graphics_data *, Uint16 chr, Uint8 byte);
  void (*get_screen_coords)(struct graphics_data *, int, int, int *, int *,
                             int *, int *, int *, int *);
  void (*set_screen_coords)(struct graphics_data *, int, int, int *, int *);
  void (*render_graph)     (struct graphics_data *);
  void (*render_cursor)    (struct graphics_data *, Uint32, Uint32, Uint8,
                             Uint8, Uint8);
  void (*render_mouse)     (struct graphics_data *, Uint32, Uint32, Uint8,
                             Uint8);
  void (*sync_screen)      (struct graphics_data *);
  void (*focus_pixel)      (struct graphics_data *, Uint32, Uint32);
};

struct graphics_data
{
  Uint32 screen_mode;
  struct char_element text_video[SCREEN_W * SCREEN_H];
  Uint8 charset[CHAR_SIZE * CHARSET_SIZE * NUM_CHARSETS];
  struct rgb_color palette[SMZX_PAL_SIZE];
  struct rgb_color intensity_palette[SMZX_PAL_SIZE];
  struct rgb_color backup_palette[SMZX_PAL_SIZE];
  struct rgb_color editor_backup_palette[SMZX_PAL_SIZE];
  Uint32 current_intensity[SMZX_PAL_SIZE];
  Uint32 saved_intensity[SMZX_PAL_SIZE];
  Uint32 backup_intensity[SMZX_PAL_SIZE];

  enum cursor_mode_types cursor_mode;
  Uint32 fade_status;
  Uint32 cursor_x;
  Uint32 cursor_y;
  Uint32 mouse_width_mul;
  Uint32 mouse_height_mul;
  bool mouse_status;
  bool system_mouse;
  bool fullscreen;
  Uint32 resolution_width;
  Uint32 resolution_height;
  Uint32 window_width;
  Uint32 window_height;
  Uint32 bits_per_pixel;
  bool allow_resize;
  Uint32 cursor_timestamp;
  Uint32 cursor_flipflop;
  Uint32 default_smzx_loaded;
  char *gl_filter_method;
  int gl_vsync;

  Uint8 default_charset[CHAR_SIZE * CHARSET_SIZE];

  Uint32 flat_intensity_palette[SMZX_PAL_SIZE];
  struct renderer renderer;
  void *render_data;
};

CORE_LIBSPEC void color_string(const char *string, Uint32 x, Uint32 y,
 Uint8 color);
CORE_LIBSPEC void write_string(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed);
CORE_LIBSPEC void write_number(int number, char color, int x, int y,
 int minlen, int rightalign, int base);
CORE_LIBSPEC void color_line(Uint32 length, Uint32 x, Uint32 y, Uint8 color);
CORE_LIBSPEC void fill_line(Uint32 length, Uint32 x, Uint32 y, Uint8 chr,
 Uint8 color);
CORE_LIBSPEC void draw_char(Uint8 chr, Uint8 color, Uint32 x, Uint32 y);

CORE_LIBSPEC void write_string_ext(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset, Uint32 c_offset);
CORE_LIBSPEC void draw_char_ext(Uint8 chr, Uint8 color, Uint32 x,
 Uint32 y, Uint32 offset, Uint32 c_offset);
CORE_LIBSPEC void draw_char_linear_ext(Uint8 color, Uint8 chr,
 Uint32 offset, Uint32 offset_b, Uint32 c_offset);
CORE_LIBSPEC void write_string_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed);

CORE_LIBSPEC void clear_screen(Uint8 chr, Uint8 color);

CORE_LIBSPEC void cursor_solid(void);
CORE_LIBSPEC void cursor_off(void);
CORE_LIBSPEC void move_cursor(Uint32 x, Uint32 y);

CORE_LIBSPEC bool init_video(struct config_info *conf, const char *caption);
CORE_LIBSPEC bool has_video_initialized(void);
CORE_LIBSPEC void update_screen(void);

CORE_LIBSPEC void ec_read_char(Uint8 chr, char *matrix);
CORE_LIBSPEC void ec_change_char(Uint8 chr, char *matrix);
CORE_LIBSPEC Sint32 ec_load_set_var(char *name, Uint8 pos);
CORE_LIBSPEC void ec_mem_load_set(Uint8 *chars);

CORE_LIBSPEC void update_palette(void);
CORE_LIBSPEC void set_gui_palette(void);
CORE_LIBSPEC void load_palette(const char *fname);
CORE_LIBSPEC void smzx_palette_loaded(int val);
CORE_LIBSPEC void set_screen_mode(Uint32 mode);
CORE_LIBSPEC Uint32 get_screen_mode(void);
CORE_LIBSPEC void set_palette_intensity(Uint32 percent);
CORE_LIBSPEC void set_rgb(Uint32 color, Uint32 r, Uint32 g, Uint32 b);
CORE_LIBSPEC void set_red_component(Uint32 color, Uint32 r);
CORE_LIBSPEC void set_green_component(Uint32 color, Uint32 g);
CORE_LIBSPEC void set_blue_component(Uint32 color, Uint32 b);
CORE_LIBSPEC void get_rgb(Uint32 color, Uint8 *r, Uint8 *g, Uint8 *b);
CORE_LIBSPEC Uint32 get_red_component(Uint32 color);
CORE_LIBSPEC Uint32 get_green_component(Uint32 color);
CORE_LIBSPEC Uint32 get_blue_component(Uint32 color);
CORE_LIBSPEC void vquick_fadeout(void);
CORE_LIBSPEC void insta_fadein(void);
CORE_LIBSPEC void insta_fadeout(void);
CORE_LIBSPEC void default_palette(void);

CORE_LIBSPEC void m_hide(void);
CORE_LIBSPEC void m_show(void);

CORE_LIBSPEC void set_mouse_mul(int width_mul, int height_mul);

void color_string_ext(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 offset, Uint32 c_offset, bool respect_newline);
void color_string_ext_special(const char *string, Uint32 x, Uint32 y,
 Uint8 *color, Uint32 offset, Uint32 c_offset, bool respect_newline);
void write_line_ext(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset);
void fill_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 chr, Uint8 color, Uint32 offset, Uint32 c_offset);
void write_line_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed);

Uint8 get_color_linear(Uint32 offset);

void cursor_underline(void);

bool set_video_mode(void);
void toggle_fullscreen(void);
void resize_screen(Uint32 w, Uint32 h);
void set_screen(struct char_element *src);
void get_screen(struct char_element *dest);

void ec_change_byte(Uint8 chr, Uint8 byte, Uint8 new_value);
Uint8 ec_read_byte(Uint8 chr, Uint8 byte);
Sint32 ec_load_set(char *name);
void ec_mem_save_set(Uint8 *chars);

void set_color_intensity(Uint32 color, Uint32 percent);
Uint32 get_color_intensity(Uint32 color);
Uint32 get_fade_status(void);
void dialog_fadein(void);
void dialog_fadeout(void);
void vquick_fadein(void);
void dump_screen(void);

void get_screen_coords(int screen_x, int screen_y, int *x, int *y,
 int *min_x, int *min_y, int *max_x, int *max_y);
void set_screen_coords(int x, int y, int *screen_x, int *screen_y);

/* Renderers might have the facility to center a screen on a given
 * region of the window. Currently only the NDS renderer implements
 * this feature.
 */
void focus_screen(int x, int y); // Board coordinates
void focus_pixel(int x, int y);  // Pixel coordinates

#ifdef CONFIG_EDITOR
CORE_LIBSPEC extern struct graphics_data graphics;
CORE_LIBSPEC void ec_load_mzx(void);
CORE_LIBSPEC void ec_load_set_secondary(const char *name, Uint8 *dest);
#endif // CONFIG_EDITOR

#ifdef CONFIG_HELPSYS
void set_cursor_mode(enum cursor_mode_types mode);
enum cursor_mode_types get_cursor_mode(void);
#endif // CONFIG_HELPSYS

__M_END_DECLS

#endif // __GRAPHICS_H
