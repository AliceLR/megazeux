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

/* FAT NOTE: This is searched as a binary tree, the nodes must be
 *           sorted alphabetically, or they risk being ignored.
 */
static const struct editor_config_entry editor_config_options[] =
{
  { "backup_count", backup_count },
  { "backup_ext", backup_ext },
  { "backup_interval", backup_interval },
  { "backup_name", backup_name },
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
  { "macro_*", config_macro },
  { "robot_editor_hide_help", redit_hhelp },
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
