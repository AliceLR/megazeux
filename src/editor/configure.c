/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "../counter.h"
#include "../configure.h"
#include "../const.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct editor_config_entry
{
  char option_name[OPTION_NAME_LEN];
  editor_config_function change_option;
};

// Default colors for color coding:
// 0 current line - 11
// 1 immediates - 10
// 2 characters - 14
// 3 colors - color box or 2
// 4 directions - 3
// 5 things - 11
// 6 params - 2
// 7 strings - 14
// 8 equalities - 0
// 9 conditions - 15
// 10 items - 11
// 11 extras - 7
// 12 commands and command fragments - 15

static void config_ccode_colors(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[4] = (char)strtol(value, NULL, 10);
}

static void config_ccode_commands(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[13] = (char)strtol(value, NULL, 10);
}

static void config_ccode_conditions(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[10] = (char)strtol(value, NULL, 10);
}

static void config_ccode_current_line(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[0] = (char)strtol(value, NULL, 10);
}

static void config_ccode_directions(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[5] = (char)strtol(value, NULL, 10);
}

static void config_ccode_equalities(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[9] = (char)strtol(value, NULL, 10);
}

static void config_ccode_extras(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[8] = (char)strtol(value, NULL, 10);
}

static void config_ccode_on(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_coding_on = strtol(value, NULL, 10);
}

static void config_ccode_immediates(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  int new_color = strtol(value, NULL, 10);
  conf->color_codes[1] = new_color;
  conf->color_codes[2] = new_color;
}

static void config_ccode_items(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[11] = (char)strtol(value, NULL, 10);
}

static void config_ccode_params(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[7] = (char)strtol(value, NULL, 10);
}

static void config_ccode_strings(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[8] = (char)strtol(value, NULL, 10);
}

static void config_ccode_things(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->color_codes[6] = (char)strtol(value, NULL, 10);
}

static void config_default_invald(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  if(!strcasecmp(value, "ignore"))
  {
    conf->default_invalid_status = 1;
  }
  else

  if(!strcasecmp(value, "delete"))
  {
    conf->default_invalid_status = 2;
  }
  else

  if(!strcasecmp(value, "comment"))
  {
    conf->default_invalid_status = 3;
  }
}

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

static void bedit_hhelp(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->bedit_hhelp = strtol(value, NULL, 10);
}

static void redit_hhelp(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->redit_hhelp = strtol(value, NULL, 10);
}

static void backup_count(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->backup_count = strtol(value, NULL, 10);
}

static void backup_interval(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->backup_interval = strtol(value, NULL, 10);
}

static void backup_name(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  strncpy(conf->backup_name, value, 256);
}

static void backup_ext(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  strncpy(conf->backup_ext, value, 256);
}

static void config_editor_space_toggles(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->editor_space_toggles = strtol(value, NULL, 10);
}

static void config_editor_enter_splits(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->editor_enter_splits = strtol(value, NULL, 10);
}

static void config_editor_tab_focus(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->editor_tab_focuses_view = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_undo_history_size(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->undo_history_size = strtol(value, NULL, 10);
}

static void config_saved_positions(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  int p = conf->num_jump_points;
  char *board_id = NULL, *board_x = NULL, *board_y = NULL;

  if(!(board_id = strchr(value, ',')))
    return;
  board_id[0] = '\0';
  board_id++;

  if(!(board_x = strchr(board_id, ',')))
    return;
  board_x[0] = '\0';
  board_x++;

  if(!(board_y = strchr(board_x, ',')))
    return;
  board_y[0] = '\0';
  board_y++;

  // Verification of this will have to be done when the editor
  // attempts to do a jump.

  // Add to the list

  if(conf->num_jump_points == 0)
    conf->jump_points = cmalloc(0);

  conf->num_jump_points++;
  conf->jump_points = crealloc(conf->jump_points,
   sizeof(struct jump_point) * conf->num_jump_points);

  conf->jump_points[p].board_id = strtol(board_id, NULL, 10);
  conf->jump_points[p].dest_x = strtol(board_x, NULL, 10);
  conf->jump_points[p].dest_y = strtol(board_y, NULL, 10);
  strncpy(conf->jump_points[p].name, value, BOARD_NAME_SIZE);
  conf->jump_points[p].name[BOARD_NAME_SIZE] = '\0';
}

/******************/
/* BOARD DEFAULTS */
/******************/

static void config_board_width(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->board_width = CLAMP(strtol(value, NULL, 10), 1, 32767);
  if(conf->viewport_w > conf->board_width)
    conf->viewport_w = conf->board_width;
}

static void config_board_height(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->board_height = CLAMP(strtol(value, NULL, 10), 1, 32767);
  if(conf->viewport_h > conf->board_height)
    conf->viewport_h = conf->board_height;
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

static void config_board_can_shoot(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->can_shoot = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_can_bomb(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->can_bomb = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_fire_spaces(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->fire_burns_spaces = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_fire_fakes(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->fire_burns_fakes = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_fire_trees(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->fire_burns_trees = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_fire_brown(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->fire_burns_brown = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_fire_forever(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->fire_burns_forever = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_forest(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->forest_to_floor = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_collect_bombs(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->collect_bombs = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_restart(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->restart_if_hurt = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_locked_ns(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->player_locked_ns = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_locked_ew(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->player_locked_ew = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_locked_att(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->player_locked_att = CLAMP(strtol(value, NULL, 10), 0, 1);
}

static void config_board_time_limit(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  conf->time_limit = CLAMP(strtol(value, NULL, 10), 0, 32767);
}

static void config_board_explosions(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  if(!strcasecmp(value, "space"))
    conf->explosions_leave = EXPL_LEAVE_SPACE;

  if(!strcasecmp(value, "ash"))
    conf->explosions_leave = EXPL_LEAVE_ASH;

  if(!strcasecmp(value, "fire"))
    conf->explosions_leave = EXPL_LEAVE_FIRE;
}

static void config_board_saving(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  if(!strcasecmp(value, "disabled"))
    conf->saving_enabled = CANT_SAVE;

  if(!strcasecmp(value, "enabled"))
    conf->saving_enabled = CAN_SAVE;

  if(!strcasecmp(value, "sensoronly"))
    conf->saving_enabled = CAN_SAVE_ON_SENSOR;
}

static void config_board_overlay(struct editor_config_info *conf,
 char *name, char *value, char *extended_data)
{
  if(!strcasecmp(value, "disabled"))
    conf->overlay_enabled = OVERLAY_OFF;

  if(!strcasecmp(value, "enabled"))
    conf->overlay_enabled = OVERLAY_ON;

  if(!strcasecmp(value, "static"))
    conf->overlay_enabled = OVERLAY_STATIC;

  if(!strcasecmp(value, "transparent"))
    conf->overlay_enabled = OVERLAY_TRANSPARENT;
}

/**********************/
/* END BOARD DEFAULTS */
/**********************/

/* FAT NOTE: This is searched as a binary tree, the nodes must be
 *           sorted alphabetically, or they risk being ignored.
 */
static const struct editor_config_entry editor_config_options[] =
{
  { "backup_count", backup_count },
  { "backup_ext", backup_ext },
  { "backup_interval", backup_interval },
  { "backup_name", backup_name },
  { "board_default_can_bomb", config_board_can_bomb },
  { "board_default_can_shoot", config_board_can_shoot },
  { "board_default_collect_bombs", config_board_collect_bombs },
  { "board_default_explosions_leave", config_board_explosions },
  { "board_default_fire_burns_brown", config_board_fire_brown },
  { "board_default_fire_burns_fakes", config_board_fire_fakes },
  { "board_default_fire_burns_forever", config_board_fire_forever },
  { "board_default_fire_burns_spaces", config_board_fire_spaces },
  { "board_default_fire_burns_trees", config_board_fire_trees },
  { "board_default_forest_to_floor", config_board_forest },
  { "board_default_height", config_board_height },
  { "board_default_overlay", config_board_overlay },
  { "board_default_player_locked_att", config_board_locked_att },
  { "board_default_player_locked_ew", config_board_locked_ew },
  { "board_default_player_locked_ns", config_board_locked_ns },
  { "board_default_restart_if_hurt", config_board_restart },
  { "board_default_saving", config_board_saving },
  { "board_default_time_limit", config_board_time_limit },
  { "board_default_viewport_h", config_board_viewport_h },
  { "board_default_viewport_w", config_board_viewport_w },
  { "board_default_viewport_x", config_board_viewport_x },
  { "board_default_viewport_y", config_board_viewport_y },
  { "board_default_width", config_board_width },
  { "board_editor_hide_help", bedit_hhelp },
  { "ccode_colors", config_ccode_colors },
  { "ccode_commands", config_ccode_commands },
  { "ccode_conditions", config_ccode_conditions },
  { "ccode_current_line", config_ccode_current_line },
  { "ccode_directions", config_ccode_directions },
  { "ccode_equalities", config_ccode_equalities },
  { "ccode_extras", config_ccode_extras },
  { "ccode_immediates", config_ccode_immediates },
  { "ccode_items", config_ccode_items },
  { "ccode_params", config_ccode_params },
  { "ccode_strings", config_ccode_strings },
  { "ccode_things", config_ccode_things },
  { "color_coding_on", config_ccode_on },
  { "default_invalid_status", config_default_invald },
  { "editor_enter_splits", config_editor_enter_splits },
  { "editor_space_toggles", config_editor_space_toggles },
  { "editor_tab_focuses_view", config_editor_tab_focus },
  { "macro_*", config_macro },
  { "robot_editor_hide_help", redit_hhelp },
  { "saved_position", config_saved_positions },
  { "undo_history_size", config_undo_history_size },
};

static const int num_editor_config_options =
 sizeof(editor_config_options) / sizeof(struct editor_config_entry);

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

static const struct editor_config_info default_editor_options =
{
  // Board editor options
  0,                            // editor_space_toggles
  0,                            // board_editor_hide_help
  1,                            // editor_tab_focuses_view

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
  0,                            // forest_to_floor
  0,                            // collect_bombs
  0,                            // restart_if_hurt
  0,                            // player_locked_ns
  0,                            // player_locked_ew
  0,                            // player_locked_att
  0,                            // time_limit
  1,                            // explosions_leave (default = ash)
  0,                            // saving_enabled (default = enabled)
  1,                            // overlay_enabled (default = enabled)

  // Char editor options
  10,                           // Undo history size

  // Robot editor options
  true,
  { 11, 10, 10, 14, 255, 3, 11, 2, 14, 0, 15, 11, 7, 15, 1, 2, 3 },
  1,                            // color_coding_on
  1,                            // default_invalid_status
  0,                            // robot_editor_hide_help

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

  // Jump points
  0,
  NULL,

};

static void editor_config_change_option(void *conf, char *name, char *value,
 char *extended_data)
{
  const struct editor_config_entry *current_option = find_editor_option(name,
   editor_config_options, num_editor_config_options);

  if(current_option)
    current_option->change_option(conf, name, value, extended_data);
}

void set_editor_config_from_file(struct editor_config_info *conf,
 const char *conf_file_name)
{
  __set_config_from_file(editor_config_change_option, conf, conf_file_name);
}

void default_editor_config(struct editor_config_info *conf)
{
  memcpy(conf, &default_editor_options, sizeof(struct editor_config_info));
}

void set_editor_config_from_command_line(struct editor_config_info *conf,
 int *argc, char *argv[])
{
  __set_config_from_command_line(editor_config_change_option, conf, argc, argv);
}

void save_local_editor_config(struct editor_config_info *conf,
 const char *mzx_file_path)
{
  int mzx_file_len = strlen(mzx_file_path) - 4;
  char config_file_name[MAX_PATH];

  char buf[60] = { 0 };
  char buf2[20] = { 0 };

  const char *comment =
    "\n############################################################";
  const char *comment_a =
    "\n#####  Editor generated configuration - do not modify  #####";
  const char *comment_b =
    "\n#####        End editor generated configuration        #####";
  FILE *fp;

  if(mzx_file_len <= 0)
    return;

  // Get our filename
  strncpy(config_file_name, mzx_file_path, mzx_file_len);
  strncpy(config_file_name + mzx_file_len, ".editor.cnf", 12);

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
    config_file[config_file_size] = 0;

    fread(config_file, 1, config_file_size, fp);
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
  sprintf(buf, "board_default_player_locked_ns = %i\n", conf->player_locked_ns);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_player_locked_ew = %i\n", conf->player_locked_ew);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_player_locked_att = %i\n", conf->player_locked_att);
  fwrite(buf, 1, strlen(buf), fp);
  sprintf(buf, "board_default_time_limit = %i\n", conf->time_limit);
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

  if(conf->num_jump_points)
  {
    int i;

    fwrite("\n", 1, 1, fp);
    for(i = 0; i < conf->num_jump_points; i++)
    {
      struct jump_point *j = &(conf->jump_points[i]);
      sprintf(buf, "saved_position = %s,%i,%i,%i\n", j->name, j->board_id, j->dest_x, j->dest_y);
    }
  }

  fwrite(comment, 1, strlen(comment), fp);
  fwrite(comment_b, 1, strlen(comment_b), fp);
  fwrite(comment, 1, strlen(comment), fp);
  fwrite("\n", 1, 1, fp);

  fclose(fp);

}
