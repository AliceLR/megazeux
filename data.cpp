/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 2004 B.D.A - Gilead Kutnick
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

// Translation of data.asm to C

#include "data.h"
#include "const.inc"

char board_where[NUM_BOARDS];					// Array of where boards are
char curr_board = 0;              		// Current board
// Current MZX file
char curr_file[FILENAME_SIZE] = "CAVERNS.MZX";
// Current SAVE file
char curr_sav[FILENAME_SIZE] ="SAVED.SAV";
char help_file[PATHNAME_SIZE + 1] 		// Drive + Path + Filename
char config_file[PATHNAME_SIZE + 1] 	// Drive + Path + Filename
char MSE_file[PATHNAME_SIZE + 1]    	// Drive + Path + Filename
// Drive + Path + Filename
char mzx_blank_mod_file[PATHNAME_SIZE + 1];
// Drive + Path + Filename
char mzx_convert_mod_file[PATHNAME_SIZE + 1];
char megazeux_dir[PATHNAME_SIZE + 1]; // Directory started in
char current_dir[PATHNAME_SIZE + 1];	// Current directory
char megazeux_drive = 2;              // Startup drive (0=A...)
char current_drive = 2;               // Current drive (0=A...)
// Current quicksave filename
char quicksave_file[FILENAME_SIZE + 1];
short board_xsiz = 80;                // Size of board in memory
short board_ysiz = 25;							
char overlay_mode = OVERLAY_OFF;			// Mode for overlay
char max_bsiz_mode = 2;
short max_bxsiz = 100;
short max_bysiz	= 100;

// Mouse state stuff
short saved_mouse_x = 0;
short saved_mouse_y = 0;
short saved_mouse_buttons = 0;

// Board "save chunk"

char mod_playing[FILENAME_SIZE];  		// Mod currently playing
char real_mod_playing[FILENAME_SIZE]	// Real mod currently playing
char refresh_mod_playing = 1;         // Load mod no matter what
char viewport_x = 3;                 	// X pos of UL corner of view
char viewport_y = 2;		
char viewport_xsiz = 74;              // Size of view
char viewport_ysiz = 21;
char can_shoot = 1;	                  // Allowed to shoot on board
char can_bomb = 1;									  // Allowed to bomb on board
char fire_burn_brown = 0;
char fire_burn_space = 1;
char fire_burn_fakes = 1;
char fire_burn_trees = 1;
char explosions_leave = EXPL_LEAVE_ASH;
char save_mode = CAN_SAVE;
char forest_becomes = FOREST_TO_EMPTY;
char collect_bombs = 1;               // If zero, bombs auto set
char fire_burns = FIRE_BURNS_FOREVER;
char board_dir[4];			           		// Boards to dirs
char restart_if_zapped = 0;
// Stays the same, for resetting time when it runs out
short time_limit = 0;
char last_key = '?';                  // Last key pressed
short num_input = 0;                  // Numeric form of input
char input_size = 0;                  // Size of input
char input_string[81];                // Last inputted string
char player_last_dir[16];             // Last direction of player
char bottom_mesg[81];	            	  // Message at bottom of screen
char b_mesg_timer = 0;                // Timer for bottom message
char lazwall_start = 7;               // Current LazWalls to fire
char b_mesg_row = 24;                 // Row to display message on
char b_mesg_col = 255;                // Column (255=centered)
short scroll_x = 0;                   // +/- to x from scrollview
short scroll_y = 0;                   // +/- to y
// Position in array of ul corner of viewport when scroll locked (-1 = none)
short locked_x = -1;									
short locked_y = -1;                  
																 
char player_ns_locked = 0;
char player_ew_locked = 0;
char player_attack_locked	= 0;
char volume = 127;
char volume_inc = 0;                  // For fading (negative okay)
char volume_target = 127;             // Target volume for fading

// World "save chunk"

char edge_color = 8;                  // Color OUTSIDE viewport
char first_board = 0;                 // First board to play
//No board=Game Over
char endgame_board = NO_ENDGAME_BOARD; 
char death_board = NO_DEATH_BOARD;    // No board=Restart same
short endgame_x = 0;
short endgame_y = 0;
char game_over_sfx = 1;
short death_x = 0;
short death_y = 0;
short starting_lives = 5;
short lives_limit = 99;               // Limit's Limit- 65535
short starting_health = 100;
short health_limit = 200;             // Limit's Limit- 65535
char enemy_hurt_enemy = 0;
char clear_on_exit = 0;               // 1=Clear message/projeciles between screens
// Set if only playable from a SWAP robot command
char only_from_swap = 0;              																  

// Save game "save chunk"
char keys[NUM_KEYS];				       		// Array of keys
int score = 0;
char blind_dur = 0;                   // Cycles left
char firewalker_dur = 0;
char freeze_time_dur = 0;
char slow_time_dur = 0;
char wind_dur = 0;
short pl_saved_x[8];									// Array of 8
short pl_saved_y[8];									// Array of 8
char pl_saved_board[8];								// Array of 8
char saved_pl_color = 27;							// Saves color for energizer
char under_player_id = 0;
char under_player_color	= 7;
char under_player_param	= 0;
char mesg_edges = 1;

char current_page = 0;                // 0 or 1
short current_pg_seg = VIDEO_SEG;     // Segment of 0 or 1
// Counters shown on status screen
char status_shown_counters[COUNTER_NAME_SIZE + 1] = { NUM_STATUS_CNTRS };
char music_on = 1;                    // If music is on
char sfx_on = 1;                      // If pc sfx is on
char music_device = 0;                // Music device
short mixing_rate	= 30; 							// Mixing rate
char sfx_channels	= 2;								// # of sfx channels
char music_gvol	= 8;									// Global Music volume (x/8)
char sound_gvol	= 8;									// Global Sound volume (x/8)
char overall_speed = 4;               // 1 through 7
short player_x = 0;                   // Not updated during "update"
short player_y = 0;        						// "     "      "       "

char scroll_color = 15;               // Current scroll color
char protection_method = NO_PROTECTION;
char password[16];
char cheats_active = 0;               // (additive flag)
char current_help_sec = 0;						// Use for context-sens.help

// Array of flags for things
short flags[] = 
{ 
 	A_UNDER,																										// Space
 	0,																													// Normal
 	0,																													// Solid
 	0,																													// Tree
 	0,																													// Line
 	0,																													// Customblock
 	A_SHOOTABLE | A_BLOW_UP,																		// Breakaway
 	A_SHOOTABLE | A_BLOW_UP,																		// Customblock
 	A_PUSHABLE | A_BLOW_UP,																			// Boulder
 	A_PUSHABLE | A_BLOW_UP,																			// Crate
 	A_PUSHABLE | A_BLOW_UP,																			// Custompush
 	A_PUSHABLE,																									// Box
 	A_PUSHABLE,																									// Customsolidpush
 	A_UNDER,																										// Fake
 	A_UNDER,																										// Carpet
 	A_UNDER,																										// Floor
 	A_UNDER,																										// Tiles
 	A_UNDER,																										// Customfloor
 	A_UNDER | A_BLOW_UP,																				// Web
 	A_UNDER | A_BLOW_UP,																				// Thickweb
 	A_UNDER,																										// Stillwater
 	A_UNDER | A_AFFECT_IF_STOOD,																// NWater
 	A_UNDER | A_AFFECT_IF_STOOD,																// SWater
 	A_UNDER | A_AFFECT_IF_STOOD,																// EWater
 	A_UNDER | A_AFFECT_IF_STOOD,																// WWater
 	A_UNDER | A_AFFECT_IF_STOOD | A_UPDATE,											// Ice
 	A_UNDER | A_AFFECT_IF_STOOD | A_UPDATE,											// Lava
 	A_ITEM | A_BLOW_UP,																					// Chest
 	A_ITEM | A_PUSHABLE | A_BLOW_UP | A_SHOOTABLE,							// Gem
 	A_ITEM | A_PUSHABLE | A_BLOW_UP | A_SHOOTABLE,							// Magicgem
 	A_ITEM | A_PUSHABLE,																				// Health
 	A_ITEM | A_PUSHABLE,																				// Ring
 	A_ITEM | A_PUSHABLE,																				// Potion
 	A_ITEM | A_PUSHABLE,          														  // Health
 	A_ITEM | A_PUSHABLE,     																		// Ring
 	A_ITEM | A_PUSHABLE,     																		// Potion
  A_ITEM | A_PUSHABLE | A_UPDATE,   												  // Energizer
  A_UNDER | A_ITEM,					   																// Goop
  A_ITEM | A_PUSHABLE,              												  // Ammo
  A_ITEM | A_PUSHABLE | A_EXPLODE,  												  // Bomb
  A_PUSHABLE | A_EXPLODE | A_UPDATE,  												// LitBomb
  A_HURTS | A_UPDATE,              										   			// Explosion
  A_ITEM | A_PUSHABLE,             														// Key
  A_ITEM,                        	 													 	// Lock
  A_ITEM,                        	 													 	// Door
  A_UPDATE,                      	 													 	// Opening/closing door
  A_ENTRANCE | A_UNDER,    				 													 	// Stairs
  A_ENTRANCE | A_UNDER,    				 													 	// Cave
  A_UPDATE,              					 													 	// CW
  A_UPDATE,              					 													 	// CCW
  A_ITEM,                        	 													 	// Gate
  A_UPDATE | A_UNDER,              														// OpenGate
  A_SPEC_PUSH | A_ITEM | A_UPDATE, 										   		  // Transport
  A_ITEM | A_PUSHABLE,             														// Coin
  A_UPDATE,						             							 						 	// MovingWall N
  A_UPDATE,						             							 						 	// MovingWall S
  A_UPDATE,						             							 						 	// MovingWall E
  A_UPDATE,						             							 						 	// MovingWall W
  A_ITEM | A_PUSHABLE | A_BLOW_UP, 							 						  // Pouch
  A_UPDATE,                      	 							 						 	// Pusher
  A_PUSHNS,                      	 							 						 	// SliderNS
  A_PUSHEW,                      	 							 						 	// SliderEW
  A_UPDATE | A_HURTS,              							 							// Lazer
  A_UPDATE,                      	 							 						 	// LazerWallShooter
  A_UPDATE | A_BLOW_UP | A_SHOOTABLE | A_ITEM |  A_ENEMY,			// Bullet
  A_UPDATE | A_HURTS | A_EXPLODE,    													// Missile       
  A_UPDATE | A_UNDER | A_AFFECT_IF_STOOD 											// Fire     
  0,                             															// '?'        
  A_ITEM,									                        						// Forest        
  A_PUSHABLE | A_ITEM | A_UPDATE,    													// Life          
  A_UPDATE | A_UNDER | A_ENTRANCE, 														// Whirlpool 1
  A_UPDATE | A_UNDER | A_ENTRANCE, 														// Whirlpool 2
  A_UPDATE | A_UNDER | A_ENTRANCE, 														// Whirlpool 3
  A_UPDATE | A_UNDER | A_ENTRANCE, 														// Whirlpool 4
  A_ITEM,                        															// Invisible     
 	A_SPEC_SHOT,						               						    			// RicochetPanel 
  A_SPEC_SHOT,                   															// Ricochet      
  A_ITEM | A_SPEC_SHOT | A_SPEC_BOMB | A_UPDATE 							// Mine
  A_HURTS,																		            	  // Spike
  A_HURTS,																		            	  // Customhurt
  0,																                        	// Text          
  A_UPDATE | A_ENEMY | A_BLOW_UP,    													// ShootingFire
  A_UPDATE | A_ENEMY | A_BLOW_UP | A_PUSHABLE, 								// Seeker
  A_UPDATE | A_PUSHABLE | A_ENEMY | A_SHOOTABLE | A_BLOW_UP,	// Snake
  A_UPDATE | A_EXPLODE | A_ITEM | A_SPEC_SHOT | A_PUSHABLE, 	// Eye
  A_UPDATE | A_BLOW_UP | A_PUSHABLE | A_ITEM | A_SHOOTABLE, 	// Thief
  A_UPDATE | A_ITEM | A_SPEC_BOMB | A_SPEC_SHOT, 							// SlimeBlob
  A_UPDATE | A_ENEMY | A_BLOW_UP | A_PUSHABLE | A_SPEC_SHOT, 	// Runner
  A_UPDATE | A_ITEM | A_PUSHABLE | A_SPEC_BOMB | A_SPEC_SHOT,	// Ghost
 	A_UPDATE | A_SPEC_SHOT | A_SPEC_BOMB | A_ITEM, 							// Dragon
  A_UPDATE | A_ITEM | A_BLOW_UP | A_SPEC_SHOT | A_PUSHABLE, 	// Fish
  A_UPDATE | A_BLOW_UP | A_SHOOTABLE | A_ENEMY,							 	// Shark
  A_UPDATE | A_ENEMY | A_BLOW_UP | A_SPEC_SHOT | A_PUSHABLE,	// Spider
  A_UPDATE | A_ENEMY | A_SHOOTABLE | A_PUSHABLE | A_BLOW_UP, 	// Goblin (90)
  A_UPDATE | A_PUSHABLE | A_SHOOTABLE | A_ENEMY | A_BLOW_UP, 	// Tiger
  A_UPDATE,														              					// BulletGun
  A_UPDATE,														              					// SpinningGun
  A_UPDATE | A_ENEMY | A_SPEC_SHOT | A_PUSHABLE | A_BLOW_UP, 	// Bear
  A_UPDATE | A_BLOW_UP | A_ENEMY | A_SHOOTABLE | A_PUSHABLE, 	// BearCub
 	0,															                            // '?'
  A_UPDATE,														                      	// MissileGun
 	0,															                            // 98
 	0,															                            // 99
 	0,															                            // 100
 	0,															                            // 101
 	0,															                            // 102
 	0,															                            // 103
 	0,															                            // 104
 	0,															                            // 105
 	0,															                            // 106
 	0,															                            // 107
 	0,															                            // 108
 	0,															                            // 109
 	0,															                            // 110
 	0,															                            // 111
 	0,															                            // 112
 	0,															                            // 113
 	0,															                            // 114
 	0,															                            // 115
 	0,															                            // 116
 	0,															                            // 117
 	0,															                            // 118
 	0,															                            // 119
 	0,															                            // 120
 	0,															                            // 121
  A_SPEC_STOOD | A_SPEC_PUSH,      														// Sensor
  A_ITEM | A_UPDATE | A_SPEC_SHOT | A_SPEC_BOMB | A_SPEC_PUSH,// Robot (pushable)
  A_ITEM | A_UPDATE | A_SPEC_SHOT | A_SPEC_BOMB 							// Robot
  A_ITEM,	                        														// Sign
 	A_ITEM | A_PUSHABLE,             														// Scroll
  A_SPEC_SHOT | A_SPEC_PUSH | A_SPEC_BOMB, 										// Player
};			


char *board_list = NULL;    					// Far pointer to array of board names
bOffset *board_offsets = NULL;    		// Far pointer to array of mem/file offsets
unsigned long *board_sizes = NULL; 		// Far pointer to array of sizes
char *board_filenames = NULL; 				// Far pointer to array of filenames

unsigned char *level_id = NULL; 			// Far pointer to array of 100x100
unsigned char *level_color = NULL;
unsigned char *level_param;
unsigned char *level_under_id;
unsigned char *level_under_color;
unsigned char *level_under_param;
unsigned char *overlay;
unsigned char *overlay_color;
unsigned char *update_done;
