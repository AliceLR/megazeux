/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

/* WINDOW.H- Declarations for WINDOW.CPP */

#ifndef __WINDOW_H
#define __WINDOW_H

#include "compat.h"

__M_BEGIN_DECLS

#include "world_struct.h"

CORE_LIBSPEC void set_context(int c);
CORE_LIBSPEC void pop_context(void);

// For name seeking in list_menu
#define TIME_SUSPEND 300

// All screen-affecting code preserves the mouse cursor
CORE_LIBSPEC int save_screen(void);
CORE_LIBSPEC int restore_screen(void);
CORE_LIBSPEC int draw_window_box(int x1, int y1, int x2, int y2, int color,
 int dark_color, int corner_color, int shadow, int fill_center);
CORE_LIBSPEC int char_selection(int current);

// Shell for run_dialog() (returns 0 for ok, 1 for cancel, -1 for ESC)
// Confirm_input is the same but takes an additional label name and input/output str
CORE_LIBSPEC int confirm(struct world *mzx_world, const char *str);
CORE_LIBSPEC int confirm_input(struct world *mzx_world, const char *title,
 const char *label, char *str);
CORE_LIBSPEC int ask_yes_no(struct world *mzx_world, char *str);

int draw_window_box_ext(int x1, int y1, int x2, int y2, int color,
 int dark_color, int corner_color, int shadow, int fill_center,
 int offset, int c_offset);

// Dialog box structure definition

struct dialog
{
  int x, y;
  int width, height;
  const char *title;
  char num_elements;
  struct element **elements;
  int current_element;
  int done;
  int return_value;
  int sfx_test_for_input;
  int pad_space;
  int (* idle_function)(struct world *mzx_world, struct dialog *di, int key);
};

struct element
{
  int x, y;
  int width, height;
  void (* draw_function)(struct world *mzx_world, struct dialog *di,
   struct element *e, int color, int active);
  int (* key_function)(struct world *mzx_world, struct dialog *di,
   struct element *e, int key);
  int (* click_function)(struct world *mzx_world, struct dialog *di,
   struct element *e, int mouse_button, int mouse_x, int mouse_y,
   int new_active);
  int (* drag_function)(struct world *mzx_world, struct dialog *di,
   struct element *e, int mouse_button, int mouse_x, int mouse_y);
  int (* idle_function)(struct world *mzx_world, struct dialog *di,
   struct element *e);
};

struct label_element
{
  struct element e;
  const char *text;
  bool respect_colors;
};

struct box
{
  struct element e;
};

enum align
{
  vertical,
  horizontal
};

struct line
{
  struct element e;
  enum align alignment;
};

struct input_box
{
  struct element e;
  const char *question;
  int input_flags;
  int max_length;
  char *result;
};

struct check_box
{
  struct element e;
  const char **choices;
  int num_choices;
  int current_choice;
  int max_length;
  int *results;
};

struct radio_button
{
  struct element e;
  const char **choices;
  int num_choices;
  int max_length;
  int *result;
};

struct char_box
{
  struct element e;
  const char *question;
  int allow_char_255;
  int *result;
};

struct color_box
{
  struct element e;
  const char *question;
  int allow_wildcard;
  int *result;
};

struct button
{
  struct element e;
  const char *label;
  int return_value;
};

struct number_box
{
  struct element e;
  const char *question;
  int lower_limit;
  int upper_limit;
  int mult_five;
  int is_null;
  int *result;
};

struct list_box
{
  struct element e;
  int num_choices;
  int num_choices_visible;
  int choice_length;
  int return_value;
  const char **choices;
  int *result;
  int scroll_offset;
  char key_buffer[64];
  int key_position;
  int last_keypress_time;
  int clicked_scrollbar;
  bool respect_color_codes;
};

struct board_list
{
  struct element e;
  const char *title;
  int board_zero_as_none;
  int *result;
};

CORE_LIBSPEC void construct_dialog(struct dialog *src, const char *title,
 int x, int y, int width, int height, struct element **elements,
 int num_elements, int start_element);
CORE_LIBSPEC void destruct_dialog(struct dialog *src);

CORE_LIBSPEC struct element *construct_label(int x, int y, const char *text);
CORE_LIBSPEC struct element *construct_radio_button(int x, int y,
 const char **choices, int num_choices, int max_length, int *result);
CORE_LIBSPEC struct element *construct_button(int x, int y, const char *label,
 int return_value);
CORE_LIBSPEC struct element *construct_number_box(int x, int y,
 const char *question, int lower_limit, int upper_limit,
 int mult_five, int *result);

CORE_LIBSPEC int choose_file_ch(struct world *mzx_world,
 const char *const *wildcards, char *ret, const char *title, int dirs_okay);
CORE_LIBSPEC int new_file(struct world *mzx_world,
 const char *const *wildcards, const char *default_ext, char *ret,
 const char *title, int dirs_okay);

#if defined(CONFIG_UPDATER) || defined(CONFIG_LOADSAVE_METER)
CORE_LIBSPEC void meter(const char *title, unsigned int progress,
 unsigned int out_of);
CORE_LIBSPEC void meter_interior(unsigned int progress, unsigned int out_of);
#endif // CONFIG_UPDATER || CONFIG_LOADSAVE_METER

// Dialog box color #define's-
#define DI_MAIN             31
#define DI_DARK             16
#define DI_CORNER           25
#define DI_TITLE            31
#define DI_LINE             16
#define DI_TEXT             27
#define DI_NONACTIVE        25
#define DI_ACTIVE           31
#define DI_INPUT            159
#define DI_CHAR             159
#define DI_NUMERIC          159
#define DI_LIST             159
#define DI_BUTTON           176
#define DI_ACTIVEBUTTON     252
#define DI_ARROWBUTTON      249
#define DI_METER            159
#define DI_PCARROW          30
#define DI_PCFILLER         25
#define DI_PCDOT            144
#define DI_ACTIVELIST       249
#define DI_SEMIACTIVELIST   159

#define DI_INPUT_BOX        76
#define DI_INPUT_BOX_DARK   64
#define DI_INPUT_BOX_CORNER 70
#define DI_INPUT_BOX_LABEL  78

#define DI_DEBUG_BOX          DI_INPUT_BOX
#define DI_DEBUG_BOX_DARK     DI_INPUT_BOX_DARK
#define DI_DEBUG_BOX_CORNER   DI_INPUT_BOX_CORNER
#define DI_DEBUG_LABEL        DI_INPUT_BOX_LABEL
#define DI_DEBUG_NUMBER       79

#define arrow_char '\x10'
#define pc_top_arrow '\x1E'
#define pc_bottom_arrow '\x1F'
#define pc_filler '\xB1'
#define pc_dot '\xFE'
#define pc_meter 219

CORE_LIBSPEC int run_dialog(struct world *mzx_world, struct dialog *di);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC extern int context;

CORE_LIBSPEC void construct_element(struct element *e, int x, int y,
 int width, int height,
 void (* draw_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int color, int active),
 int (* key_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int key),
 int (* click_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int mouse_button, int mouse_x, int mouse_y,
  int new_active),
 int (* drag_function)(struct world *mzx_world, struct dialog *di,
  struct element *e, int mouse_button, int mouse_x, int mouse_y),
 int (* idle_function)(struct world *mzx_world, struct dialog *di,
  struct element *e));
CORE_LIBSPEC struct element *construct_list_box(int x, int y,
 const char **choices, int num_choices, int num_choices_visible,
 int choice_length, int return_value, int *result, bool respect_color_codes);
CORE_LIBSPEC void construct_dialog_ext(struct dialog *src, const char *title,
 int x, int y, int width, int height, struct element **elements,
 int num_elements, int sfx_test_for_input, int pad_space, int start_element,
 int (* idle_function)(struct world *mzx_world, struct dialog *di, int key));

CORE_LIBSPEC int char_selection_ext(int current, int allow_multichar,
 int *width_ptr, int *height_ptr);
CORE_LIBSPEC struct element *construct_input_box(int x, int y,
 const char *question, int max_length, int input_flags, char *result);
CORE_LIBSPEC int file_manager(struct world *mzx_world,
 const char *const *wildcards, const char *default_ext, char *ret,
 const char *title, int dirs_okay, int allow_new,
 struct element **dialog_ext, int num_ext, int ext_height);
#endif // CONFIG_EDITOR

#ifdef CONFIG_HELPSYS
int get_context(void);
#endif

__M_END_DECLS

#endif // __WINDOW_H
