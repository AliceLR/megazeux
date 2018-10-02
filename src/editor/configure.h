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

#ifndef __EDITOR_CONFIGURE_H
#define __EDITOR_CONFIGURE_H

#include "../compat.h"
#include "../const.h"

__M_BEGIN_DECLS

#include "macro.h"

#define NUM_SAVED_POSITIONS 10

struct editor_config_info
{
  // Board editor options
  boolean board_editor_hide_help;
  boolean editor_space_toggles;
  boolean editor_tab_focuses_view;
  boolean editor_load_board_assets;
  boolean editor_thing_menu_places;
  int undo_history_size;

  // Defaults for new boards
  int viewport_x;
  int viewport_y;
  int viewport_w;
  int viewport_h;
  int board_width;
  int board_height;
  int can_shoot;
  int can_bomb;
  int fire_burns_spaces;
  int fire_burns_fakes;
  int fire_burns_trees;
  int fire_burns_brown;
  int fire_burns_forever;
  int forest_to_floor;
  int collect_bombs;
  int restart_if_hurt;
  int reset_on_entry;
  int player_locked_ns;
  int player_locked_ew;
  int player_locked_att;
  int time_limit;
  int explosions_leave;
  int saving_enabled;
  int overlay_enabled;
  char charset_path[MAX_PATH];
  char palette_path[MAX_PATH];

  // Palette editor options
  boolean palette_editor_hide_help;

  // Robot editor options
  boolean editor_enter_splits;
  char color_codes[32];
  boolean color_coding_on;
  int default_invalid_status;
  boolean disassemble_extras;
  int disassemble_base;
  boolean robot_editor_hide_help;

  // Backup options
  int backup_count;
  int backup_interval;
  char backup_name[256];
  char backup_ext[256];

  // Macro options
  char default_macros[5][64];
  int num_extended_macros;
  int num_macros_allocated;
  struct ext_macro **extended_macros;

  // Saved positions
  int saved_board[NUM_SAVED_POSITIONS];
  int saved_cursor_x[NUM_SAVED_POSITIONS];
  int saved_cursor_y[NUM_SAVED_POSITIONS];
  int saved_scroll_x[NUM_SAVED_POSITIONS];
  int saved_scroll_y[NUM_SAVED_POSITIONS];
  int saved_debug_x[NUM_SAVED_POSITIONS];
};

EDITOR_LIBSPEC void default_editor_config(void);
EDITOR_LIBSPEC void set_editor_config_from_file(const char *conf_file_name);
EDITOR_LIBSPEC void set_editor_config_from_command_line(int *argc,
 char *argv[]);

EDITOR_LIBSPEC void store_editor_config_backup(void);
EDITOR_LIBSPEC void free_editor_config(void);

struct editor_config_info *get_editor_config(void);
void load_editor_config_backup(void);

void save_local_editor_config(struct editor_config_info *conf,
 const char *mzx_file_path);

__M_END_DECLS

#endif // __EDITOR_CONFIGURE_H
