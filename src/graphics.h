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

#define INVISIBLE_CHAR 65535

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

#define SCREEN_PIX_W (SCREEN_W * CHAR_W)
#define SCREEN_PIX_H (SCREEN_H * CHAR_H)

/**
 * The Nintendo DS is a bit of a special case in that we support it despite it
 * not currently handling either the protected charset or the protected palette.
 */
#if defined(CONFIG_NDS)
#define NUM_CHARSETS 2
#define NO_PROTECTED_PALETTE
#endif

#define CHAR_SIZE 14
#define CHARSET_SIZE 256
#ifndef NUM_CHARSETS
#define NUM_CHARSETS 16
#endif
#define PROTECTED_CHARSET_POSITION ((NUM_CHARSETS - 1) * CHARSET_SIZE)
#define PRO_CH  PROTECTED_CHARSET_POSITION
#define FULL_CHARSET_SIZE (CHARSET_SIZE * NUM_CHARSETS)

#define PAL_SIZE 16
#define SMZX_PAL_SIZE 256
#define PROTECTED_PAL_SIZE 16
#define FULL_PAL_SIZE (SMZX_PAL_SIZE + PROTECTED_PAL_SIZE)

#define SET_SCREEN_SIZE (SCREEN_W * SCREEN_H * 3)

#define TEXTVIDEO_LAYERS 512

#define LAYER_DRAWORDER_BOARD 0
#define LAYER_DRAWORDER_OVERLAY 1000
#define LAYER_DRAWORDER_GAME_UI 2000
#define LAYER_DRAWORDER_UI 3000
#define LAYER_DRAWORDER_MAX (LAYER_DRAWORDER_UI + 1000)

enum default_video_layers
{
  BOARD_LAYER         = 0,
  OVERLAY_LAYER       = 1,
  GAME_UI_LAYER       = 2,
  UI_LAYER            = 3,
  NUM_DEFAULT_LAYERS  = 4
};

struct graphics_data;
struct video_layer;

struct renderer
{
  boolean (*init_video)       (struct graphics_data *, struct config_info *);
  void    (*free_video)       (struct graphics_data *);
  boolean (*check_video_mode) (struct graphics_data *, int width, int height,
                                int depth, boolean fullscreen, boolean resize);
  boolean (*set_video_mode)   (struct graphics_data *, int width, int height,
                                int depth, boolean fullscreen, boolean resize);
  void    (*update_colors)    (struct graphics_data *, struct rgb_color *palette,
                                Uint32 count);
  void    (*resize_screen)    (struct graphics_data *, int width, int height);
  void    (*remap_char_range) (struct graphics_data *, Uint16 first, Uint16 count);
  void    (*remap_char)       (struct graphics_data *, Uint16 chr);
  void    (*remap_charbyte)   (struct graphics_data *, Uint16 chr, Uint8 byte);
  void    (*get_screen_coords)(struct graphics_data *, int screen_x,
                                int screen_y, int *x, int *y, int *min_x,
                                int *min_y, int *max_x, int *max_y);
  void    (*set_screen_coords)(struct graphics_data *, int x, int y,
                                int *screen_x, int *screen_y);
  void    (*switch_shader)    (struct graphics_data *, const char *name);
  void    (*render_graph)     (struct graphics_data *);
  void    (*render_layer)     (struct graphics_data *, struct video_layer *);
  void    (*render_cursor)    (struct graphics_data *, Uint32 x, Uint32 y,
                                Uint16 color, Uint8 lines, Uint8 offset);
  void    (*render_mouse)     (struct graphics_data *, Uint32 x, Uint32 y,
                                Uint8 w, Uint8 h);
  void    (*sync_screen)      (struct graphics_data *);
  void    (*focus_pixel)      (struct graphics_data *, Uint32 x, Uint32 y);
};

struct video_layer
{
  Uint32 w, h, mode;
  int x, y;
  struct char_element *data;
#if defined(CONFIG_3DS)
  void *platform_layer_data;
#endif /* CONFIG_3DS */
  int draw_order;
  int transparent_col;
  int offset;
  boolean empty;
};

struct graphics_data
{
  Uint32 screen_mode;
  char default_caption[32];
  struct char_element text_video[SCREEN_W * SCREEN_H];
  Uint8 charset[CHAR_SIZE * CHARSET_SIZE * NUM_CHARSETS];
  struct rgb_color palette[SMZX_PAL_SIZE];
  struct rgb_color protected_palette[PAL_SIZE];
  struct rgb_color intensity_palette[SMZX_PAL_SIZE];
  struct rgb_color backup_palette[SMZX_PAL_SIZE];
  char smzx_indices[SMZX_PAL_SIZE * 4];
  Uint32 current_intensity[SMZX_PAL_SIZE];
  Uint32 saved_intensity[SMZX_PAL_SIZE];
  Uint32 backup_intensity[SMZX_PAL_SIZE];
  boolean palette_dirty;

  Uint32 layer_count;
  Uint32 layer_count_prev;
  struct video_layer text_video_layer;
  struct video_layer video_layers[TEXTVIDEO_LAYERS];
  Uint32 current_layer;
  struct char_element *current_video;
  struct video_layer *sorted_video_layers[TEXTVIDEO_LAYERS];
  boolean requires_extended;

  enum cursor_mode_types cursor_mode;
  boolean fade_status;
  boolean dialog_fade_status;
  Uint32 cursor_x;
  Uint32 cursor_y;
  Uint32 mouse_width_mul;
  Uint32 mouse_height_mul;
  boolean mouse_status;
  boolean system_mouse;
  boolean grab_mouse;
  boolean fullscreen;
  boolean fullscreen_windowed;
  Uint32 resolution_width;
  Uint32 resolution_height;
  Uint32 window_width;
  Uint32 window_height;
  Uint32 bits_per_pixel;
  boolean allow_resize;
  Uint32 cursor_timestamp;
  Uint32 cursor_flipflop;
  Uint32 default_smzx_loaded;
  enum ratio_type ratio;
  char *gl_filter_method;
  char *gl_scaling_shader;
  int gl_vsync;

  Uint8 default_charset[CHAR_SIZE * CHARSET_SIZE];

  Uint32 flat_intensity_palette[FULL_PAL_SIZE];
  Uint32 protected_pal_position;
  struct renderer renderer;
  void *render_data;
  Uint32 renderer_num;
};

CORE_LIBSPEC void color_string(const char *string, Uint32 x, Uint32 y,
 Uint8 color);
CORE_LIBSPEC void write_string(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed);
CORE_LIBSPEC void write_number(int number, char color, int x, int y,
 int minlen, boolean rightalign, int base);
CORE_LIBSPEC void color_line(Uint32 length, Uint32 x, Uint32 y, Uint8 color);
CORE_LIBSPEC void fill_line(Uint32 length, Uint32 x, Uint32 y, Uint8 chr,
 Uint8 color);
CORE_LIBSPEC void draw_char(Uint8 chr, Uint8 color, Uint32 x, Uint32 y);
CORE_LIBSPEC void erase_char(Uint32 x, Uint32 y);
CORE_LIBSPEC void erase_area(Uint32 x, Uint32 y, Uint32 x2, Uint32 y2);

CORE_LIBSPEC void write_string_ext(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset, Uint32 c_offset);
CORE_LIBSPEC void draw_char_mixed_pal_ext(Uint8 chr, Uint8 bg_color,
 Uint8 fg_color, Uint32 x, Uint32 y, Uint32 offset);
CORE_LIBSPEC void draw_char_ext(Uint8 chr, Uint8 color, Uint32 x,
 Uint32 y, Uint32 offset, Uint32 c_offset);
CORE_LIBSPEC void draw_char_linear_ext(Uint8 color, Uint8 chr,
 Uint32 offset, Uint32 offset_b, Uint32 c_offset);
CORE_LIBSPEC void draw_char_to_layer(Uint8 color, Uint8 chr,
 Uint32 x, Uint32 y, Uint32 offset_b, Uint32 c_offset);
CORE_LIBSPEC void write_string_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed);

CORE_LIBSPEC void clear_screen(void);

CORE_LIBSPEC void cursor_solid(void);
CORE_LIBSPEC void cursor_off(void);
CORE_LIBSPEC void move_cursor(Uint32 x, Uint32 y);

CORE_LIBSPEC boolean init_video(struct config_info *conf, const char *caption);
CORE_LIBSPEC void quit_video(void);
CORE_LIBSPEC void destruct_layers(void);
CORE_LIBSPEC void destruct_extra_layers(Uint32 first);
CORE_LIBSPEC Uint32 create_layer(int x, int y, Uint32 w, Uint32 h,
 int draw_order, int t_col, int offset, boolean unbound);
CORE_LIBSPEC void set_layer_offset(Uint32 layer, int offset);
CORE_LIBSPEC void set_layer_mode(Uint32 layer, int mode);
CORE_LIBSPEC void move_layer(Uint32 layer, int x, int y);
CORE_LIBSPEC void select_layer(Uint32 layer);
CORE_LIBSPEC void blank_layers(void);
CORE_LIBSPEC boolean has_video_initialized(void);
CORE_LIBSPEC void update_screen(void);
CORE_LIBSPEC void set_window_caption(const char *caption);

CORE_LIBSPEC void ec_read_char(Uint16 chr, char *matrix);
CORE_LIBSPEC void ec_change_char(Uint16 chr, char *matrix);
CORE_LIBSPEC Sint32 ec_load_set_var(char *name, Uint16 pos, int version);
CORE_LIBSPEC void ec_mem_load_set(Uint8 *chars, size_t len);
CORE_LIBSPEC void ec_mem_save_set(Uint8 *chars);
CORE_LIBSPEC void ec_mem_load_set_var(char *chars, size_t len, Uint16 pos, int v);
CORE_LIBSPEC void ec_mem_save_set_var(Uint8 *chars, size_t len, Uint16 pos);

CORE_LIBSPEC void update_palette(void);
CORE_LIBSPEC void load_palette(const char *fname);
CORE_LIBSPEC void load_palette_mem(char *pal, size_t len);
CORE_LIBSPEC void load_index_file(const char *fname);
CORE_LIBSPEC void smzx_palette_loaded(int val);
CORE_LIBSPEC void set_screen_mode(Uint32 mode);
CORE_LIBSPEC Uint32 get_screen_mode(void);
CORE_LIBSPEC void set_palette_intensity(Uint32 percent);
CORE_LIBSPEC void set_rgb(Uint32 color, Uint32 r, Uint32 g, Uint32 b);
CORE_LIBSPEC void set_protected_rgb(Uint32 color, Uint32 r, Uint32 g, Uint32 b);
CORE_LIBSPEC Uint32 get_smzx_index(Uint32 col, Uint32 offset);
CORE_LIBSPEC void set_smzx_index(Uint32 col, Uint32 offset, Uint32 value);
CORE_LIBSPEC void set_red_component(Uint32 color, Uint32 r);
CORE_LIBSPEC void set_green_component(Uint32 color, Uint32 g);
CORE_LIBSPEC void set_blue_component(Uint32 color, Uint32 b);
CORE_LIBSPEC void get_rgb(Uint32 color, Uint8 *r, Uint8 *g, Uint8 *b);
CORE_LIBSPEC Uint32 get_red_component(Uint32 color);
CORE_LIBSPEC Uint32 get_green_component(Uint32 color);
CORE_LIBSPEC Uint32 get_blue_component(Uint32 color);
CORE_LIBSPEC Uint32 get_color_luma(Uint32 color);
CORE_LIBSPEC Uint32 get_fade_status(void);
CORE_LIBSPEC void vquick_fadeout(void);
CORE_LIBSPEC void insta_fadein(void);
CORE_LIBSPEC void insta_fadeout(void);
CORE_LIBSPEC void dialog_fadein(void);
CORE_LIBSPEC void dialog_fadeout(void);
CORE_LIBSPEC void default_palette(void);
CORE_LIBSPEC void default_protected_palette(void);

CORE_LIBSPEC void m_hide(void);
CORE_LIBSPEC void m_show(void);

CORE_LIBSPEC void set_mouse_mul(int width_mul, int height_mul);

char *get_default_caption(void);

void color_string_ext(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 offset, Uint32 c_offset, boolean respect_newline);
void color_string_ext_special(const char *string, Uint32 x, Uint32 y,
 Uint8 *color, Uint32 offset, Uint32 c_offset, boolean respect_newline);
void write_line_ext(const char *string, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset);
void fill_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 chr, Uint8 color, Uint32 offset, Uint32 c_offset);
void write_line_mask(const char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed);

Uint8 get_color_linear(Uint32 offset);

void cursor_underline(void);

boolean change_video_output(struct config_info *conf, const char *output);
int get_available_video_output_list(const char **buffer, int buffer_len);
int get_current_video_output(void);

boolean set_video_mode(void);
boolean is_fullscreen(void);
void toggle_fullscreen(void);
void resize_screen(Uint32 w, Uint32 h);
void set_screen(struct char_element *src);
void get_screen(struct char_element *dest);

void ec_change_byte(Uint16 chr, Uint8 byte, Uint8 new_value);
Uint8 ec_read_byte(Uint16 chr, Uint8 byte);
Sint32 ec_load_set(char *name);
void ec_clear_set(void);

void set_color_intensity(Uint32 color, Uint32 percent);
Uint32 get_color_intensity(Uint32 color);
void save_indices(void *buffer);
void load_indices(void *buffer, size_t size);
void load_indices_direct(void *buffer, size_t size);
void vquick_fadein(void);
void dump_char(Uint16 char_idx, Uint8 color, int mode, Uint8 *buffer);

void get_screen_coords(int screen_x, int screen_y, int *x, int *y,
 int *min_x, int *min_y, int *max_x, int *max_y);
void set_screen_coords(int x, int y, int *screen_x, int *screen_y);

CORE_LIBSPEC boolean switch_shader(const char *name);
CORE_LIBSPEC boolean layer_renderer_check(boolean show_error);

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

#ifdef CONFIG_ENABLE_SCREENSHOTS
void dump_screen(void);
#endif

__M_END_DECLS

#endif // __GRAPHICS_H
