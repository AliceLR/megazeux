/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Declarations for DATA.ASM (most global data) */

#ifndef __DATA_H
#define __DATA_H

#include "const.h"
#include "struct.h"

/* This first section is from IDPUT.ASM */
extern unsigned char id_chars[455];
/*extern unsigned char id_chars[128];*/
extern unsigned char id_dmg[128];
extern unsigned char def_id_chars[455];
extern unsigned char *player_color;
extern unsigned char *player_char; /* was [4]*/
extern unsigned char bullet_color[3]; /* was [3]*/
extern unsigned char missile_color;
/*extern unsigned char bullet_char[12];*/
extern unsigned char *bullet_char;
extern char refresh_mod_playing;
extern char mesg_edges;
extern unsigned int saved_mouse_x;
extern unsigned int saved_mouse_y;
extern unsigned int saved_mouse_buttons;
extern unsigned char board_where[NUM_BOARDS];
extern unsigned char keys[NUM_KEYS];
extern unsigned long score;
extern char mod_playing[FILENAME_SIZE];
extern char real_mod_playing[FILENAME_SIZE];
extern unsigned char curr_board;
extern char curr_file[FILENAME_SIZE];
extern char curr_sav[FILENAME_SIZE];
extern char help_file[PATHNAME_SIZE];
extern char config_file[PATHNAME_SIZE];
extern char MSE_file[PATHNAME_SIZE];
extern char mzx_blank_mod_file[PATHNAME_SIZE];
extern char mzx_convert_mod_file[PATHNAME_SIZE];
extern char megazeux_dir[PATHNAME_SIZE];
extern char current_dir[PATHNAME_SIZE];
extern unsigned char megazeux_drive;
extern unsigned char current_drive;
extern char quicksave_file[FILENAME_SIZE];
extern unsigned char viewport_x;
extern unsigned char viewport_y;
extern unsigned char viewport_xsiz;
extern unsigned char viewport_ysiz;
extern unsigned int board_xsiz;
extern unsigned int board_ysiz;
extern unsigned char can_shoot;
extern unsigned char can_bomb;
extern unsigned char fire_burn_brown;
extern unsigned char fire_burn_space;
extern unsigned char fire_burn_fakes;
extern unsigned char fire_burn_trees;
extern unsigned char explosions_leave;
extern unsigned char save_mode;
extern unsigned char forest_becomes;
extern unsigned char collect_bombs;
extern unsigned char fire_burns;
extern unsigned char board_dir[4];
extern unsigned char restart_if_zapped;
extern unsigned int time_limit;
extern unsigned char first_board;
extern unsigned char clear_on_exit;
extern unsigned char endgame_board;
extern unsigned int endgame_x;
extern unsigned int endgame_y;
extern unsigned char game_over_sfx;
extern unsigned char death_board;
extern unsigned int death_x;
extern unsigned int death_y;
extern unsigned char only_from_swap;
extern unsigned int starting_lives;
extern unsigned int lives_limit;
extern unsigned int starting_health;
extern unsigned int health_limit;
extern unsigned char last_key;//Local
extern unsigned int num_input;//Local
extern unsigned char input_size;//Local
extern unsigned char volume;//Global
extern char volume_inc;//Global
extern unsigned char volume_target;//Global
extern unsigned char player_ns_locked;//Now global
extern unsigned char player_ew_locked;//Now global
extern unsigned char player_attack_locked;//Now global
extern char input_string[81];//Local
extern unsigned char blind_dur;//Now global
extern unsigned char firewalker_dur;//Now global
extern unsigned char freeze_time_dur;//Now global
extern unsigned char slow_time_dur;//Now global
extern unsigned char wind_dur;//Now global
extern unsigned char player_last_dir;//Local
extern unsigned char current_page;
extern unsigned int current_pg_seg;
extern char status_shown_counters[6*COUNTER_NAME_SIZE];
extern unsigned char music_on;
extern unsigned char sfx_on;
extern unsigned char music_gvol;//Global volume (settings)
extern unsigned char sound_gvol;//Global volume (settings)
extern unsigned char music_device;
extern unsigned int mixing_rate;
extern unsigned char sfx_channels;
extern unsigned char overall_speed;
extern char bottom_mesg[81];//Local
extern unsigned char b_mesg_timer;//Local
extern unsigned char b_mesg_row;//Local
extern unsigned char b_mesg_col;//Local, 255 for centered
extern unsigned int player_x;
extern unsigned int player_y;
extern unsigned int pl_saved_x[8];
extern unsigned int pl_saved_y[8];
extern unsigned char pl_saved_board[8];
extern unsigned char edge_color;//Global
extern unsigned char scroll_color;//Global, not saved
extern unsigned char lazwall_start;//Local
extern int scroll_x;//Local   // Something is SERIOUSLY wrong with these two, counter.cpp
extern int scroll_y;//Local   // can't even see them! Spid


extern unsigned int locked_x;//Local (-1 or 65535 for none)
extern unsigned int locked_y;//Local (-1 or 65535 for none)
extern unsigned char protection_method;
extern char password[16];
extern unsigned char enemy_hurt_enemy;//Global
extern unsigned char cheats_active;
extern unsigned char current_help_sec;
extern unsigned char saved_pl_color;
extern unsigned int flags[128];
extern char far *board_list;
extern bOffset far *board_offsets;
extern unsigned long far *board_sizes;
extern char far *board_filenames;
extern unsigned char far *level_id;
extern unsigned char far *level_color;
extern unsigned char far *level_param;
extern unsigned char far *level_under_id;
extern unsigned char far *level_under_color;
extern unsigned char far *level_under_param;
extern unsigned char far *overlay;
extern unsigned char far *overlay_color;
extern unsigned char far *update_done;

extern unsigned char overlay_mode;

extern unsigned char max_bsiz_mode;
extern unsigned char under_player_id;
extern unsigned char under_player_color;
extern unsigned char under_player_param;
extern unsigned int max_bxsiz;
extern unsigned int max_bysiz;

extern Robot far *robots;
extern Scroll far *scrolls;
extern Counter far *counters;
extern Sensor far *sensors;


extern char far *music_devices[NUM_DEVICES+1];
extern char far *music_MSEs[NUM_DEVICES];
extern unsigned int mixing_rates[NUM_DEVICES][3];
extern char far *music_quality[3];

extern char far *thing_names[128];

#endif
