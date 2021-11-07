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

#include <stdint.h>

#include "configure.h"

struct rgb_color
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t unused;
};

#define INVISIBLE_CHAR 65535

struct char_element
{
  uint16_t char_value;
  uint8_t bg_color;
  uint8_t fg_color;
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

#ifdef CONFIG_NO_LAYER_RENDERING
#define SET_SCREEN_SIZE (SCREEN_W * SCREEN_H)
#else
#define SET_SCREEN_SIZE (SCREEN_W * SCREEN_H * 3)
#endif

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
  boolean (*set_video_mode)   (struct graphics_data *, int width, int height,
                                int depth, boolean fullscreen, boolean resize);
  void    (*update_colors)    (struct graphics_data *, struct rgb_color *palette,
                                unsigned int count);
  void    (*resize_screen)    (struct graphics_data *, int width, int height);
  void    (*remap_char_range) (struct graphics_data *, uint16_t first, uint16_t count);
  void    (*remap_char)       (struct graphics_data *, uint16_t chr);
  void    (*remap_charbyte)   (struct graphics_data *, uint16_t chr, uint8_t byte);
  void    (*get_screen_coords)(struct graphics_data *, int screen_x,
                                int screen_y, int *x, int *y, int *min_x,
                                int *min_y, int *max_x, int *max_y);
  void    (*set_screen_coords)(struct graphics_data *, int x, int y,
                                int *screen_x, int *screen_y);
  boolean (*switch_shader)    (struct graphics_data *, const char *name);
  void    (*render_graph)     (struct graphics_data *);
  void    (*render_layer)     (struct graphics_data *, struct video_layer *);
  void    (*render_cursor)    (struct graphics_data *, unsigned x, unsigned y,
                                uint16_t color, unsigned lines, unsigned offset);
  void    (*hardware_cursor)  (struct graphics_data *, unsigned x, unsigned y,
                                uint16_t color, unsigned lines, unsigned offset,
                                boolean enable);
  void    (*render_mouse)     (struct graphics_data *, unsigned x, unsigned y,
                                unsigned w, unsigned h);
  void    (*sync_screen)      (struct graphics_data *);
  void    (*focus_pixel)      (struct graphics_data *, unsigned x, unsigned y);
};

struct video_layer
{
  unsigned int w, h;
  int x, y;
  struct char_element *data;
#if defined(CONFIG_3DS)
  void *platform_layer_data;
#endif /* CONFIG_3DS */
  int draw_order;
  int transparent_col;
  int offset;
  uint8_t mode;
  boolean empty;
};

struct graphics_data
{
  unsigned int screen_mode;
  char default_caption[32];
  struct char_element text_video[SCREEN_W * SCREEN_H];
  uint8_t charset[CHAR_SIZE * CHARSET_SIZE * NUM_CHARSETS];
  struct rgb_color palette[SMZX_PAL_SIZE];
  struct rgb_color protected_palette[PAL_SIZE];
  struct rgb_color intensity_palette[SMZX_PAL_SIZE];
  struct rgb_color backup_palette[SMZX_PAL_SIZE];
  uint8_t smzx_indices[SMZX_PAL_SIZE * 4];
  uint32_t current_intensity[SMZX_PAL_SIZE];
  uint32_t saved_intensity[SMZX_PAL_SIZE];
  uint32_t backup_intensity[SMZX_PAL_SIZE];
  boolean default_smzx_loaded;
  boolean palette_dirty;
  boolean fade_status;
  boolean dialog_fade_status;
  boolean requires_extended;

  uint32_t layer_count;
  uint32_t layer_count_prev;
  struct video_layer text_video_layer;
  struct video_layer video_layers[TEXTVIDEO_LAYERS];
  uint32_t current_layer;
  struct char_element *current_video;
  struct char_element *current_video_end;
  struct video_layer *sorted_video_layers[TEXTVIDEO_LAYERS];

  enum cursor_mode_types cursor_mode;
  enum cursor_mode_types cursor_hint_mode;
  unsigned int cursor_x;
  unsigned int cursor_y;
  unsigned int cursor_flipflop;
  uint32_t cursor_timestamp;
  unsigned int mouse_width;
  unsigned int mouse_height;
  boolean mouse_status;
  boolean system_mouse;
  boolean grab_mouse;
  boolean fullscreen;
  boolean fullscreen_windowed;
  boolean allow_resize;
  unsigned int resolution_width;
  unsigned int resolution_height;
  unsigned int window_width;
  unsigned int window_height;
  unsigned int bits_per_pixel;
  enum ratio_type ratio;
  enum gl_filter_type gl_filter_method;
  int gl_vsync;
  char gl_scaling_shader[32];
  char sdl_render_driver[16];

  uint8_t default_charset[CHAR_SIZE * CHARSET_SIZE];

  uint32_t flat_intensity_palette[FULL_PAL_SIZE];
  uint32_t protected_pal_position;
  struct renderer renderer;
  void *render_data;
  int renderer_num;
};

CORE_LIBSPEC void color_string(const char *string, unsigned int x,
 unsigned int y, uint8_t color);
CORE_LIBSPEC void write_string(const char *string, unsigned int x,
 unsigned int y, uint8_t color, boolean tab_allowed);
CORE_LIBSPEC void write_number(int number, uint8_t color, unsigned int x,
 unsigned int y, int minlen, boolean rightalign, int base);
CORE_LIBSPEC void color_line(unsigned int length, unsigned int x,
 unsigned int y, uint8_t color);
CORE_LIBSPEC void fill_line(unsigned int length, unsigned int x, unsigned int y,
 uint8_t chr, uint8_t color);
CORE_LIBSPEC void draw_char(uint8_t chr, uint8_t color, unsigned int x, unsigned int y);
CORE_LIBSPEC void erase_char(unsigned int x, unsigned int y);
CORE_LIBSPEC void erase_area(unsigned int x, unsigned int y,
 unsigned int x2, unsigned int y2);

CORE_LIBSPEC void write_string_ext(const char *str, unsigned int x, unsigned int y,
 uint8_t color, boolean tab_allowed, unsigned int chr_offset, unsigned int color_offset);
CORE_LIBSPEC void write_string_mask(const char *str, unsigned int x, unsigned int y,
 uint8_t color, boolean tab_allowed);
CORE_LIBSPEC void draw_char_ext(uint8_t chr, uint8_t color, unsigned int x,
 unsigned int y, unsigned int chr_offset, unsigned int color_offset);
CORE_LIBSPEC void draw_char_bleedthru_ext(uint8_t chr, uint8_t color,
 unsigned int x, unsigned int y, unsigned int chr_offset, unsigned int color_offset);
CORE_LIBSPEC void draw_char_to_layer(uint8_t chr, uint8_t color,
 unsigned int x, unsigned int y, unsigned int chr_offset, unsigned int color_offset);

CORE_LIBSPEC void clear_screen(void);

CORE_LIBSPEC void cursor_underline(unsigned int x, unsigned int y);
CORE_LIBSPEC void cursor_solid(unsigned int x, unsigned int y);
CORE_LIBSPEC void cursor_hint(unsigned int x, unsigned int y);
CORE_LIBSPEC void cursor_off(void);

CORE_LIBSPEC boolean init_video(struct config_info *conf, const char *caption);
CORE_LIBSPEC void quit_video(void);
CORE_LIBSPEC void destruct_layers(void);
CORE_LIBSPEC void destruct_extra_layers(uint32_t first);
CORE_LIBSPEC uint32_t create_layer(int x, int y, unsigned int w, unsigned int h,
 int draw_order, int t_col, int offset, boolean unbound);
CORE_LIBSPEC void set_layer_offset(uint32_t layer, int offset);
CORE_LIBSPEC void set_layer_mode(uint32_t layer, int mode);
CORE_LIBSPEC void move_layer(uint32_t layer, int x, int y);
CORE_LIBSPEC void select_layer(uint32_t layer);
CORE_LIBSPEC void blank_layers(void);
CORE_LIBSPEC boolean has_video_initialized(void);
CORE_LIBSPEC void update_screen(void);
CORE_LIBSPEC void set_window_caption(const char *caption);

CORE_LIBSPEC void ec_read_char(uint16_t chr, char matrix[CHAR_SIZE]);
CORE_LIBSPEC void ec_change_char(uint16_t chr, const char matrix[CHAR_SIZE]);
CORE_LIBSPEC int ec_load_set_var(const char *filename, uint16_t first_chr, int ver);
CORE_LIBSPEC void ec_mem_load_set(const void *buffer, size_t len);
CORE_LIBSPEC void ec_mem_load_set_var(const void *buffer, size_t len,
 uint16_t first_chr, int ver);
CORE_LIBSPEC void ec_mem_save_set_var(void *buffer, size_t len, uint16_t first_chr);

CORE_LIBSPEC void update_palette(void);
CORE_LIBSPEC void load_palette(const char *filename);
CORE_LIBSPEC void load_palette_mem(const void *buffer, size_t len);
CORE_LIBSPEC void load_index_file(const char *filename);
CORE_LIBSPEC void smzx_palette_loaded(boolean is_loaded);
CORE_LIBSPEC void set_screen_mode(unsigned int mode);
CORE_LIBSPEC unsigned int get_screen_mode(void);
CORE_LIBSPEC void set_palette_intensity(unsigned int percent);
CORE_LIBSPEC void set_rgb(uint8_t color, unsigned int r,
 unsigned int g, unsigned int b);
CORE_LIBSPEC void set_protected_rgb(uint8_t color, unsigned int r,
 unsigned int g, unsigned int b);
CORE_LIBSPEC void set_red_component(uint8_t color, unsigned int r);
CORE_LIBSPEC void set_green_component(uint8_t color, unsigned int g);
CORE_LIBSPEC void set_blue_component(uint8_t color, unsigned int b);
CORE_LIBSPEC void get_rgb(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b);
CORE_LIBSPEC unsigned int get_red_component(uint8_t color);
CORE_LIBSPEC unsigned int get_green_component(uint8_t color);
CORE_LIBSPEC unsigned int get_blue_component(uint8_t color);
CORE_LIBSPEC uint8_t get_smzx_index(uint8_t palette, unsigned int offset);
CORE_LIBSPEC void set_smzx_index(uint8_t palette, unsigned int offset, uint8_t color);
CORE_LIBSPEC boolean get_fade_status(void);
CORE_LIBSPEC void vquick_fadeout(void);
CORE_LIBSPEC void insta_fadein(void);
CORE_LIBSPEC void insta_fadeout(void);
CORE_LIBSPEC void dialog_fadein(void);
CORE_LIBSPEC void dialog_fadeout(void);
CORE_LIBSPEC void default_palette(void);
CORE_LIBSPEC void default_protected_palette(void);

CORE_LIBSPEC void m_hide(void);
CORE_LIBSPEC void m_show(void);
CORE_LIBSPEC void mouse_size(unsigned int width, unsigned int height);

char *get_default_caption(void);

void color_string_ext(const char *string, unsigned int x, unsigned int y,
 uint8_t color, boolean allow_newline, unsigned int chr_offset, unsigned int color_offset);
void color_string_ext_special(const char *string, unsigned int x, unsigned int y,
 uint8_t *color, boolean allow_newline, unsigned int chr_offset, unsigned int color_offset);
void write_line_ext(const char *string, unsigned int x, unsigned int y,
 uint8_t color, boolean tab_allowed, unsigned int chr_offset, unsigned int color_offset);
void write_line_mask(const char *str, unsigned int x, unsigned int y,
 uint8_t color, boolean tab_allowed);
void fill_line_ext(unsigned int length, unsigned int x, unsigned int y,
 uint8_t chr, uint8_t color, unsigned int chr_offset, unsigned int color_offset);

boolean change_video_output(struct config_info *conf, const char *output);
int get_available_video_output_list(const char **buffer, int buffer_len);
int get_current_video_output(void);

boolean set_video_mode(void);
boolean is_fullscreen(void);
void toggle_fullscreen(void);
void resize_screen(unsigned int w, unsigned int h);
void set_screen(struct char_element *src);
void get_screen(struct char_element *dest);

void ec_change_byte(uint16_t chr, uint8_t byte, uint8_t new_value);
uint8_t ec_read_byte(uint16_t chr, uint8_t byte);
boolean ec_load_set(const char *filename);
void ec_clear_set(void);

void set_color_intensity(uint8_t color, unsigned int percent);
unsigned int get_color_intensity(uint8_t color);
void save_indices(void *buffer);
void load_indices(const void *buffer, size_t size);
void load_indices_direct(const void *buffer, size_t size);
void vquick_fadein(void);
boolean get_char_visible_bitmask(uint16_t char_idx, uint8_t palette,
 int transparent_color, uint8_t * RESTRICT buffer);

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
CORE_LIBSPEC int get_color_luma(unsigned int color);
CORE_LIBSPEC int get_char_average_luma(uint16_t chr, uint8_t palette, int mode,
 int mask_chr);
CORE_LIBSPEC void ec_load_mzx(void);
CORE_LIBSPEC void ec_load_set_secondary(const char *filename,
 uint8_t dest[CHAR_SIZE * CHARSET_SIZE]);
CORE_LIBSPEC void draw_char_mixed_pal_ext(uint8_t chr, uint8_t bg_color,
 uint8_t fg_color, unsigned int x, unsigned int y, unsigned int chr_offset);
#endif // CONFIG_EDITOR

#ifdef CONFIG_ENABLE_SCREENSHOTS
void dump_screen(void);
#endif

__M_END_DECLS

#endif // __GRAPHICS_H
