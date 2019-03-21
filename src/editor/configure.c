/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "configure.h"
#include "robo_ed.h"

#include "../counter.h"
#include "../configure.h"
#include "../configure_option.h"
#include "../const.h"
#include "../util.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static struct editor_config_info editor_conf;
static struct editor_config_info editor_conf_backup;

static const struct editor_config_info editor_conf_default =
{
  // Board editor options
  true,                         // board_editor_hide_help
  false,                        // editor_space_toggles
  false,                        // editor_tab_focuses_view
  false,                        // editor_load_board_assets
  true,                         // editor_thing_menu_places
  100,                          // Undo history size

  // Defaults for new boards
  0,                            // viewport_x
  0,                            // viewport_y
  80,                           // viewport_w
  25,                           // viewport_h
  100,                          // board_width
  100,                          // board_height
  1,                            // can_shoot
  1,                            // can_bomb
  0,                            // fire_burns_spaces
  1,                            // fire_burns_fakes
  1,                            // fire_burns_trees
  0,                            // fire_burns_brown
  0,                            // fire_burns_forever
  1,                            // forest_to_floor
  1,                            // collect_bombs
  0,                            // restart_if_hurt
  0,                            // reset_on_entry
  0,                            // player_locked_ns
  0,                            // player_locked_ew
  0,                            // player_locked_att
  0,                            // time_limit
  1,                            // explosions_leave (default = ash)
  0,                            // saving_enabled (default = enabled)
  1,                            // overlay_enabled (default = enabled)
  "",                           // charset_path
  "",                           // palette_path

  // Palette editor options
  false,                        // palette_editor_hide_help

  // Robot editor options
  true,                         // editor_enter_splits
  { 11, 10, 10, 14, 255, 3, 11, 2, 14, 0, 15, 11, 7, 15, 1, 2, 3 },
  true,                         // color_coding_on
  1,                            // default_invalid_status
  true,                         // disassemble_extras
  10,                           // disassemble_base
  true,                         // robot_editor_hide_help

  // Backup options
  3,                            // backup_count
  60,                           // backup_interval
  "backup",                     // backup_name
  ".mzx",                       // backup_ext

  // Macro options
  { "char ", "color ", "goto ", "send ", ": playershot^" },
  0,                            // num_extended_macros
  0,
  NULL,

  // Saved positions
  { 0 },
  { 0 },
  { 0 },
  { 0 },
  { 0 },
  { 60, 60, 60, 60, 60, 60, 60, 60, 60, 60 },

};

static const struct config_enum boolean_values[] =
{
  { "0", false },
  { "1", true }
};

static const struct config_enum explosions_leave_values[] =
{
  { "space", EXPL_LEAVE_SPACE },
  { "ash", EXPL_LEAVE_ASH },
  { "fire", EXPL_LEAVE_FIRE }
};

static const struct config_enum overlay_enabled_values[] =
{
  { "disabled", OVERLAY_OFF },
  { "enabled", OVERLAY_ON },
  { "static", OVERLAY_STATIC },
  { "transparent", OVERLAY_TRANSPARENT },
};

static const struct config_enum saving_enabled_values[] =
{
  { "disabled", CANT_SAVE },
  { "enabled", CAN_SAVE },
  { "sensoronly", CAN_SAVE_ON_SENSOR }
};

static const struct config_enum default_invalid_status_values[] =
{
  { "ignore", invalid_uncertain },
  { "delete", invalid_discard },
  { "comment", invalid_comment }
};

static const struct config_enum disassemble_base_values[] =
{
  { "10", 10 },
  { "16", 16 }
};

struct editor_config_entry
{
  char option_name[OPTION_NAME_LEN];
  struct config_option dest;
};

typedef void (* editor_config_function)(struct editor_config_info *conf,
 char *name, char *value, char *extended_data);

static void config_macro(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  char *macro_name = name + 6;

  if(isdigit((int)macro_name[0]) && !macro_name[1] && !extended_data)
  {
    int macro_num = macro_name[0] - 0x31;
    value[63] = 0;
    strcpy(conf->default_macros[macro_num], value);
  }
  else
  {
    if(extended_data)
      add_ext_macro(conf, macro_name, extended_data, value);
  }
}

static void config_saved_positions(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  unsigned int pos;
  unsigned int board_num;
  unsigned int board_x;
  unsigned int board_y;
  unsigned int scroll_x;
  unsigned int scroll_y;
  unsigned int debug_x;

  sscanf(name, "saved_position%u", &pos);
  sscanf(value, "%u, %u, %u, %u, %u, %u",
   &board_num, &board_x, &board_y, &scroll_x, &scroll_y, &debug_x);

  if(pos >= NUM_SAVED_POSITIONS)
    return;

  conf->saved_board[pos] = board_num;
  conf->saved_cursor_x[pos] = board_x;
  conf->saved_cursor_y[pos] = board_y;
  conf->saved_scroll_x[pos] = scroll_x;
  conf->saved_scroll_y[pos] = scroll_y;
  conf->saved_debug_x[pos] = debug_x ? 60 : 0;
}

static void config_board_width(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->board_width = CLAMP(strtol(value, NULL, 10), 1, 32767);
  if(conf->viewport_w > conf->board_width)
    conf->viewport_w = conf->board_width;
  if(conf->board_width * conf->board_height > MAX_BOARD_SIZE)
    conf->board_height = MAX_BOARD_SIZE / conf->board_width;
}

static void config_board_height(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->board_height = CLAMP(strtol(value, NULL, 10), 1, 32767);
  if(conf->viewport_h > conf->board_height)
    conf->viewport_h = conf->board_height;
  if(conf->board_width * conf->board_height > MAX_BOARD_SIZE)
    conf->board_width = MAX_BOARD_SIZE / conf->board_height;
}

static void config_board_viewport_w(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->viewport_w = CLAMP(strtol(value, NULL, 10), 1, MIN(conf->board_width, 80));
  if(conf->viewport_x + conf->viewport_w > 80)
    conf->viewport_x = 80 - conf->viewport_w;
}

static void config_board_viewport_h(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->viewport_h = CLAMP(strtol(value, NULL, 10), 1, MIN(conf->board_height, 25));
  if(conf->viewport_y + conf->viewport_h > 25)
    conf->viewport_y = 25 - conf->viewport_h;
}

static void config_board_viewport_x(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->viewport_x = CLAMP(strtol(value, NULL, 10), 0, 80 - conf->viewport_w);
}

static void config_board_viewport_y(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->viewport_y = CLAMP(strtol(value, NULL, 10), 0, (25 - conf->viewport_h));
}

#define INT(field, min, max) CONF_INT(struct editor_config_info, field, min, max)
#define ENUM(field) CONF_ENUM(struct editor_config_info, field, field ## _values)
#define BOOL(field) CONF_ENUM(struct editor_config_info, field, boolean_values)
#define STR(field) CONF_STR(struct editor_config_info, field)
#define FN(field) CONF_FN(struct editor_config_info, field)

/* NOTE: This is searched as a binary tree, the nodes must be
 *       sorted alphabetically, or they risk being ignored.
 *
 * For values that can easily be written directly to the struct,
 * use the INT/ENUM/BOOL/STR macro functions. These generate offset
 * and bounding information that allows config_option_set to reuse
 * the same code for similar options. For more complex options, the
 * FN macro can be used to provide an editor_config_function instead.
 *
 * Note: ccode_immediates previously set both index 1 and index 2.
 * Index 2 appears to be completely unused.
 */
static const struct editor_config_entry editor_config_options[] =
{
  { "backup_count",                     INT(backup_count, 0, INT_MAX) },
  { "backup_ext",                       STR(backup_ext) },
  { "backup_interval",                  INT(backup_interval, 0, INT_MAX) },
  { "backup_name",                      STR(backup_name) },
  { "board_default_can_bomb",           BOOL(can_bomb) },
  { "board_default_can_shoot",          BOOL(can_shoot) },
  { "board_default_charset_path",       STR(charset_path) },
  { "board_default_collect_bombs",      BOOL(collect_bombs) },
  { "board_default_explosions_leave",   ENUM(explosions_leave) },
  { "board_default_fire_burns_brown",   BOOL(fire_burns_brown) },
  { "board_default_fire_burns_fakes",   BOOL(fire_burns_fakes) },
  { "board_default_fire_burns_forever", BOOL(fire_burns_forever) },
  { "board_default_fire_burns_spaces",  BOOL(fire_burns_spaces) },
  { "board_default_fire_burns_trees",   BOOL(fire_burns_trees) },
  { "board_default_forest_to_floor",    BOOL(forest_to_floor) },
  { "board_default_height",             FN(config_board_height) },
  { "board_default_overlay",            ENUM(overlay_enabled) },
  { "board_default_palette_path",       STR(palette_path) },
  { "board_default_player_locked_att",  BOOL(player_locked_att) },
  { "board_default_player_locked_ew",   BOOL(player_locked_ew) },
  { "board_default_player_locked_ns",   BOOL(player_locked_ns) },
  { "board_default_reset_on_entry",     BOOL(reset_on_entry) },
  { "board_default_restart_if_hurt",    BOOL(restart_if_hurt) },
  { "board_default_saving",             ENUM(saving_enabled) },
  { "board_default_time_limit",         INT(time_limit, 0, 32767) },
  { "board_default_viewport_h",         FN(config_board_viewport_h) },
  { "board_default_viewport_w",         FN(config_board_viewport_w) },
  { "board_default_viewport_x",         FN(config_board_viewport_x) },
  { "board_default_viewport_y",         FN(config_board_viewport_y) },
  { "board_default_width",              FN(config_board_width) },
  { "board_editor_hide_help",           BOOL(board_editor_hide_help) },
  { "ccode_colors",                     INT(color_codes[4], 0, 255) },
  { "ccode_commands",                   INT(color_codes[13], 0, 15) },
  { "ccode_conditions",                 INT(color_codes[10], 0, 15) },
  { "ccode_current_line",               INT(color_codes[0], 0, 15) },
  { "ccode_directions",                 INT(color_codes[5], 0, 15) },
  { "ccode_equalities",                 INT(color_codes[9], 0, 15) },
  { "ccode_extras",                     INT(color_codes[12], 0, 15) },
  { "ccode_immediates",                 INT(color_codes[1], 0, 15) },//see note
  { "ccode_items",                      INT(color_codes[11], 0, 15) },
  { "ccode_params",                     INT(color_codes[7], 0, 15) },
  { "ccode_strings",                    INT(color_codes[8], 0, 15) },
  { "ccode_things",                     INT(color_codes[6], 0, 15) },
  { "color_coding_on",                  BOOL(color_coding_on) },
  { "default_invalid_status",           ENUM(default_invalid_status) },
  { "disassemble_base",                 ENUM(disassemble_base) },
  { "disassemble_extras",               BOOL(disassemble_extras) },
  { "editor_enter_splits",              BOOL(editor_enter_splits) },
  { "editor_load_board_assets",         BOOL(editor_load_board_assets) },
  { "editor_space_toggles",             BOOL(editor_space_toggles) },
  { "editor_tab_focuses_view",          BOOL(editor_tab_focuses_view) },
  { "editor_thing_menu_places",         BOOL(editor_thing_menu_places) },
  { "macro_*",                          FN(config_macro) },
  { "palette_editor_hide_help",         BOOL(palette_editor_hide_help) },
  { "robot_editor_hide_help",           BOOL(robot_editor_hide_help) },
  { "saved_position!",                  FN(config_saved_positions) },
  { "undo_history_size",                INT(undo_history_size, 0, 1000) },
};

static const struct editor_config_entry *find_editor_option(char *name,
 const struct editor_config_entry options[], int num_options)
{
  int cmpval, top = num_options - 1, middle, bottom = 0;
  const struct editor_config_entry *base = options;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = match_function_counter(name, (base[middle]).option_name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return base + middle;
  }

  return NULL;
}

static int editor_config_change_option(void *conf, char *name, char *value,
 char *extended_data)
{
  const struct editor_config_entry *current_option = find_editor_option(name,
   editor_config_options, ARRAY_SIZE(editor_config_options));

  if(current_option)
  {
    fn_ptr fn = config_option_set(&(current_option->dest), conf, name, value);

    if(fn)
      ((editor_config_function)fn)(conf, name, value, extended_data);
    return 1;
  }
  return 0;
}

struct editor_config_info *get_editor_config(void)
{
  return &editor_conf;
}

void default_editor_config(void)
{
  memcpy(&editor_conf, &editor_conf_default, sizeof(struct editor_config_info));
}

void set_editor_config_from_file(const char *conf_file_name)
{
  __set_config_from_file(editor_config_change_option, &editor_conf,
   conf_file_name);
}

void set_editor_config_from_command_line(int *argc, char *argv[])
{
  __set_config_from_command_line(editor_config_change_option, &editor_conf,
   argc, argv);
}

void store_editor_config_backup(void)
{
  // Back up the config.
  memcpy(&editor_conf_backup, &editor_conf, sizeof(struct editor_config_info));
  editor_conf_backup.extended_macros = NULL;
}

void load_editor_config_backup(void)
{
  // Restore the editor config (but keep the macros).
  int num_macros = editor_conf.num_extended_macros;
  int num_macros_alloc = editor_conf.num_macros_allocated;
  struct ext_macro **macros = editor_conf.extended_macros;

  memcpy(&editor_conf, &editor_conf_backup, sizeof(struct editor_config_info));

  editor_conf.extended_macros = macros;
  editor_conf.num_extended_macros = num_macros;
  editor_conf.num_macros_allocated = num_macros_alloc;
}

static void __free_editor_config(struct editor_config_info *conf)
{
  // Extended Macros
  if(conf->extended_macros)
  {
    int i;
    for(i = 0; i < conf->num_extended_macros; i++)
      free_macro(conf->extended_macros[i]);

    free(conf->extended_macros);
    conf->extended_macros = NULL;
  }
}

void free_editor_config(void)
{
  __free_editor_config(&editor_conf);
  __free_editor_config(&editor_conf_backup);
}

void save_local_editor_config(struct editor_config_info *conf,
 const char *mzx_file_path)
{
  int mzx_file_len = strlen(mzx_file_path) - 4;
  char config_file_name[MAX_PATH];

  char buf[MAX_PATH + 60] = { 0 };
  char buf2[20] = { 0 };

  const char *comment =
    "\n############################################################";
  const char *comment_a =
    "\n#####  Editor generated configuration - do not modify  #####";
  const char *comment_b =
    "\n#####        End editor generated configuration        #####";
  FILE *fp;
  int i;

  if(mzx_file_len <= 0)
    return;

  // Get our filename
  snprintf(config_file_name, MAX_PATH, "%.*s.editor.cnf",
   mzx_file_len, mzx_file_path);

  // Does it exist?
  fp = fopen_unsafe(config_file_name, "rb");
  if(fp)
  {
    char *a, *b;
    char *config_file = NULL;
    char *config_file_b = NULL;
    int config_file_size = 0;
    int config_file_size_b = 0;

    config_file_size = ftell_and_rewind(fp);
    config_file = cmalloc(config_file_size + 1);

    config_file_size = fread(config_file, 1, config_file_size, fp);
    config_file[config_file_size] = 0;
    fclose(fp);

    a = strstr(config_file, comment_a);
    b = strstr(config_file, comment_b);

    // Start of special section
    if(a)
    {
      a[0] = '\0';
      a = strrchr(config_file, '\n');
      if(a)
      {
        if(a > config_file && a[-1] == '\r')
          a[-1] = '\0';
        else
          a[0] = '\0';

        config_file_size = strlen(config_file);
      }
      else
      {
        config_file[0] = '\0';
        config_file_size = 0;
      }

      // End of special section
      // Skip matched line and comment line afterward
      if(b && (b = strchr(b + 1, '\n')) && (b = strchr(b + 1, '\n')))
      {
        config_file_b = b + 1;
        config_file_size_b = strlen(config_file_b);
      }
    }

    fp = fopen_unsafe(config_file_name, "wb");

    if(config_file_size)
      fwrite(config_file, 1, config_file_size, fp);
    if(config_file_size_b)
      fwrite(config_file_b, 1, config_file_size_b, fp);

    fclose(fp);
    free(config_file);
  }

  fp = fopen_unsafe(config_file_name, "a");

  fwrite(comment, 1, strlen(comment), fp);
  fwrite(comment_a, 1, strlen(comment_a), fp);
  fwrite(comment, 1, strlen(comment), fp);
  fwrite("\n\n", 1, 2, fp);

  sprintf(buf, "board_default_width = %i\n", conf->board_width);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_height = %i\n", conf->board_height);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_viewport_w = %i\n", conf->viewport_w);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_viewport_h = %i\n", conf->viewport_h);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_viewport_x = %i\n", conf->viewport_x);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_viewport_y = %i\n", conf->viewport_y);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_can_shoot = %i\n", conf->can_shoot);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_can_bomb = %i\n", conf->can_bomb);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_fire_burns_spaces = %i\n", conf->fire_burns_spaces);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_fire_burns_fakes = %i\n", conf->fire_burns_fakes);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_fire_burns_trees = %i\n", conf->fire_burns_trees);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_fire_burns_brown = %i\n", conf->fire_burns_brown);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_fire_burns_forever = %i\n", conf->fire_burns_forever);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_forest_to_floor = %i\n", conf->forest_to_floor);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_collect_bombs = %i\n", conf->collect_bombs);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_restart_if_hurt = %i\n", conf->restart_if_hurt);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_reset_on_entry = %i\n", conf->reset_on_entry);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_player_locked_ns = %i\n", conf->player_locked_ns);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_player_locked_ew = %i\n", conf->player_locked_ew);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_player_locked_att = %i\n", conf->player_locked_att);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_time_limit = %i\n", conf->time_limit);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_charset_path = %s\n", conf->charset_path);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_palette_path = %s\n", conf->palette_path);
  fwrite(buf, 1, strlen(buf), fp);

  switch(conf->explosions_leave)
  {
    case EXPL_LEAVE_SPACE:
      strcpy(buf2, "space");
      break;
    case EXPL_LEAVE_ASH:
      strcpy(buf2, "ash");
      break;
    case EXPL_LEAVE_FIRE:
      strcpy(buf2, "fire");
      break;
  }
  sprintf(buf, "board_default_explosions_leave = %s\n", buf2);
  fwrite(buf, 1, strlen(buf), fp);

  switch(conf->saving_enabled)
  {
    case CANT_SAVE:
      strcpy(buf2, "disabled");
      break;
    case CAN_SAVE:
      strcpy(buf2, "enabled");
      break;
    case CAN_SAVE_ON_SENSOR:
      strcpy(buf2, "sensoronly");
      break;
  }
  sprintf(buf, "board_default_saving = %s\n", buf2);
  fwrite(buf, 1, strlen(buf), fp);

  switch(conf->overlay_enabled)
  {
    case OVERLAY_OFF:
      strcpy(buf2, "disabled");
      break;
    case OVERLAY_ON:
      strcpy(buf2, "enabled");
      break;
    case OVERLAY_STATIC:
      strcpy(buf2, "static");
      break;
    case OVERLAY_TRANSPARENT:
      strcpy(buf2, "transparent");
      break;
  }
  sprintf(buf, "board_default_overlay = %s\n", buf2);
  fwrite(buf, 1, strlen(buf), fp);

  fwrite("\n", 1, 1, fp);
  for(i = 0; i < NUM_SAVED_POSITIONS; i++)
  {
    sprintf(buf, "saved_position%u = %u, %u, %u, %u, %u, %u\n",
     i,
     conf->saved_board[i],
     conf->saved_cursor_x[i],
     conf->saved_cursor_y[i],
     conf->saved_scroll_x[i],
     conf->saved_scroll_y[i],
     conf->saved_debug_x[i]
    );
    fwrite(buf, 1, strlen(buf), fp);
  }

  fwrite(comment, 1, strlen(comment), fp);
  fwrite(comment_b, 1, strlen(comment_b), fp);
  fwrite(comment, 1, strlen(comment), fp);
  fwrite("\n", 1, 1, fp);

  fclose(fp);

}
