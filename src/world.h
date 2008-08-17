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

#ifndef WORLD_H
#define WORLD_H

#include <stdio.h>

#include "board.h"
#include "const.h"
#include "robot.h"
#include "counter.h"
#include "sprite.h"
#include "sfx.h"
#include "configure.h"

#define VERSION 0x251

extern char version_number_string[20];
extern char world_version_string[4];
extern char save_version_string[6];

typedef struct _World World;

struct _World
{
  // 0 if a world has been loaded, 1 if it hasn't
  int active;
  char name[BOARD_NAME_SIZE];

  int version;
  // Move here eventually
  //char id_chars[455];
  char status_counters_shown[NUM_STATUS_CNTRS][COUNTER_NAME_SIZE];

  // Save game material, part 1
  char keys[NUM_KEYS];
  int score;
  int blind_dur;
  int firewalker_dur;
  int freeze_time_dur;
  int slow_time_dur;
  int wind_dur;
  int pl_saved_x[8];
  int pl_saved_y[8];
  int pl_saved_board[8];
  int saved_pl_color;
  int was_zapped;
  int under_player_id;
  int under_player_color;
  int under_player_param;
  int mesg_edges;
  int scroll_base_color;
  int scroll_corner_color;
  int scroll_pointer_color;
  int scroll_title_color;
  int scroll_arrow_color;
  char real_mod_playing[256];

  int edge_color;
  int first_board;
  int endgame_board;
  int death_board;
  int endgame_x;
  int endgame_y;
  int game_over_sfx;
  int death_x;
  int death_y;
  int starting_lives;
  int lives_limit;
  int starting_health;
  int health_limit;
  int enemy_hurt_enemy;
  int clear_on_exit;
  int only_from_swap;

  // Save game material, part 2
  int player_restart_x;
  int player_restart_y;
  int num_counters;
  int num_counters_allocated;
  counter **counter_list;
  int num_strings;
  int num_strings_allocated;
  mzx_string **string_list;
  int num_sprites;
  int num_sprites_allocated;
  int sprite_num;
  Sprite **sprite_list;
  int active_sprites;
  int sprite_y_order;
  int collision_count;
  int *collision_list;
  int multiplier;
  int divider;
  int c_divisions;
  int bi_mesg_status;
  char input_file_name[512];
  char output_file_name[512];
  FILE *input_file;
  FILE *output_file;
  int commands;
  int vlayer_size;
  int vlayer_width;
  int vlayer_height;
  char *vlayer_chars;
  char *vlayer_colors;

  int num_boards;
  int num_boards_allocated;
  Board **board_list;
  Board *current_board;
  int current_board_id;

  Robot global_robot;

  int custom_sfx_on;
  char custom_sfx[NUM_SFX * 69];

  // Not part of world/save files, but runtime globals
  int player_x;
  int player_y;

  // For moving the player between boards
  mzx_board_target target_where;
  int target_board;
  int target_x;
  int target_y;
  mzx_thing target_id;
  int target_color;
  mzx_thing target_d_id;
  int target_d_color;

  // Indiciates if the player is dead
  int dead;

  // Current bomb type
  int bomb_type;

  // Toggle for if the screen is in slow motion
  int slow_down;

  // For use in repeat delays for player movement
  int key_up_delay;
  int key_down_delay;
  int key_right_delay;
  int key_left_delay;

  // 1-3 normal 5-7 is 1-3 but from a REL FIRST cmd
  int first_prefix;
  // Just 1-3
  int mid_prefix;
  // 1-3 normal 5-7 is 1-3 but from a REL LAST cmd
  int last_prefix;

  // Lets the get counter routines indiciate to the caller
  // that the result is not a typical counter but something
  // special. It's there for those horribly hacked file open
  // counters. It should normally be set to 0.
  int special_counter_return;

  // Indicates if a robot swapped the world
  int swapped;

  // Current speed of MZX world
  int mzx_speed;
  // Default speed (as loaded by config file)
  int default_speed;
  // If we can change the speed from the F2 menu.
  int lock_speed;

  config_info conf;

  // Keep this open, just once
  FILE *help_file;

  // An array for game2.cpp
  char *update_done;
  int update_done_size;
};

// Taken from the old saveload.h

int save_world(World *mzx_world, char *file, int savegame, int faded);
// Returns non-0 for error so you don't jump to main board
int load_world(World *mzx_world, char *file, int savegame,
 int *faded);
int append_world(World *mzx_world, char *file);
int reload_world(World *mzx_world, char *file, int *faded);
int reload_savegame(World *mzx_world, char *file, int *faded);
int reload_swap(World *mzx_world, char *file, int *faded);
void clear_world(World *mzx_world);
void clear_global_data(World *mzx_world);
void default_global_data(World *mzx_world);
void default_scroll_values(World *mzx_world);
void create_blank_world(World *mzx_world);
void optimize_null_boards(World *mzx_world);
void set_update_done(World *mzx_world);
void set_update_done_current(World *mzx_world);

// Code to load multi-byte ints from little endian file

int fgetw(FILE *fp);
int fgetd(FILE *fp);
void fputw(int src, FILE *fp);
void fputd(int src, FILE *fp);
void add_ext(char *src, char *ext);
void get_path(char *file_name, char *dest);

#endif
