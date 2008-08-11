/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
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


// I added a bunch of stuff here. Whenever a window is popped up, the mouse
// cursor is re-activated, and then CURSORSTATE is checked upon closing the
// window. If it's 1, the cursor is left on, otherwise it's hidden as normal.
// You can find all instances by searching for CURSORSTATE. Spid


// Main title screen/gaming code

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "event.h"
#include "helpsys.h"
#include "scrdisp.h"
#include "error.h"
#include "idarray.h"
#include "audio.h"
#include "edit.h"
#include "sfx.h"
#include "hexchar.h"
#include "idput.h"
#include "data.h"
#include "const.h"
#include "game.h"
#include "window.h"
#include "graphics.h"
#include "counter.h"
#include "game2.h"
#include "sprite.h"
#include "world.h"
#include "delay.h"
#include "robot.h"

char *main_menu = "Enter- Menu\n"
                  "Esc  - Exit to DOS\n"
                  "F1/H - Help\n"
                  "F2/S - Settings\n"
                  "F3/L - Load world\n"
                  "F4/R - Restore game\n"
                  "F5/P - Play world\n"
                  "F6   - Debug menu\n"
                  "F8/E - World editor\n"
                  "F10  - Quickload"
                  "";

char *game_menu =  "F1    - Help\n"
                  "Enter - Menu/status\n"
                  "Esc   - Exit to title\n"
                  "F2    - Settings\n"
                  "F3    - Save game\n"
                  "F4    - Restore game\n"
                  "F5/Ins- Toggle bomb type\n"
                  "F6    - Debug menu\n"
                  "F9    - Quicksave\n"
                  "F10   - Quickload\n"
                  "Arrows- Move\n"
                  "Space - Shoot (w/dir)\n"
                  "Delete- Bomb";

char *world_ext[] = { ".MZX", NULL };

char *save_ext[] = { ".SAV", NULL };

//Easter Egg enter menu -Koji
// Idea by Exophase.
// Wasting mem with style.. okay, not anymore :( - Exo
/* char *lame_menu= "F1    - HELP!!1\n"
              "etner - TIHS MENUE\n"
              "EScapE- DONT PRESS LOL\n"
              "f2    - UR SETTINGS\n"
              "F3    - SAV UR GAEM\n"
              "F4    - LOAD UR GAAM\n"
              "F5-iNs- I DONNO! LOLZ\n"
              "f6    - :((((((((\n"
              "f9    - quickSNAD!\n"
              "ef10  - QUICKLOAD\n"
              "Arrowz- MOVE UR GUY :D :D\n"
              "spaec - shoot UR GUN\n"
              "deleet- DA BOMB!!!111111"; */


char debug_mode = 0;

int bomb_type = 1; // Start on hi-bombs
int dead = 0;
int update_music;

// For changing screens AFTER an update is done and shown
int target_board = -1; // Where to go
int target_where = -2; // 0 for x/y, 1 for entrance
// -1 for teleport (so fading isn't used)
int target_x = -1; // Or color of entrance
int target_y = -1; // Or id of entrance
// For RESTORE/EXCHANGE PLAYER POSITION with DUPLICATION.
int target_d_id = -1;
// For RESTORE/EXCHANGE PLAYER POSITION with DUPLICATION.
int target_d_color = -1;


int pal_update; // Whether to update a palette from robot activity

void load_world_selection(World *mzx_world)
{
  int fade_status = get_fade_status();

  // Load
  m_show();
  if(fade_status)
  {
    clear_screen(32, 7);
    insta_fadein();
  }

  if(!choose_file(world_ext, curr_file, "Load World", 1))
    load_world_file(mzx_world, curr_file);

  if(fade_status)
    insta_fadeout();
}

void load_world_file(World *mzx_world, char *name)
{
  Board *src_board;
  int fade = 0;

  // Load world curr_file
  end_mod();
  clear_sfx_queue();
  //Clear screen
  clear_screen(32, 7);
  // Palette
  default_palette();
  reload_world(mzx_world, name, &fade);
  send_robot_def(mzx_world, 0, 10);

  src_board = mzx_world->current_board;
  load_mod(src_board->mod_playing);
  strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
  set_counter(mzx_world, "TIME", src_board->time_limit, 0);
  dead = 0;

  set_mesg(mzx_world, "** BETA ** Press F1 for help, Enter for menu");
}

void title_screen(World *mzx_world)
{
  int fadein = 1;
  int key = 0;
  int fade;
  FILE *fp;
  char temp[FILENAME_SIZE];
  char temp2[FILENAME_SIZE];
  struct stat file_info;
  Board *src_board;

  debug_mode = 0;
  error_mode = 2;

  // Clear screen
  clear_screen(32, 7);
  default_palette();

  // Try to load curr_file
  if(!stat(curr_file, &file_info))
  {
    load_world_file(mzx_world, curr_file);
  }
  else
  {
    load_world_selection(mzx_world);
  }

  // Main game loop
  // Mouse remains hidden unless menu/etc. is invoked
  // Making the menu functions on by default -Koji
  // Do it HERE too - Exo
  set_counter(mzx_world, "ENTER_MENU", 1, 0);
  set_counter(mzx_world, "HELP_MENU", 1, 0);
  set_counter(mzx_world, "F2_MENU", 1, 0);

  src_board = mzx_world->current_board;

  // Main game loop
  // Mouse remains hidden unless menu/etc. is invoked

  do
  {
    // Update
    if(mzx_world->active)
    {
      if(update(mzx_world, 0, &fadein))
      {
        update_event_status();
        continue;
      }
    }

    src_board = mzx_world->current_board;

    update_event_status();

    // Keycheck
    key = get_key(keycode_SDL);

    if(key)
    {
      switch(key)
      {
        case SDLK_e: // E
        case SDLK_F8: // F8
        {
          // Editor
          clear_sfx_queue();
          vquick_fadeout();
          edit_world(mzx_world);

          if(curr_file[0])
            load_world_file(mzx_world, curr_file);

          fadein = 1;
          dead = 0;
          break;
        }

        case SDLK_s: // S
        case SDLK_F2: // F2
        {
          int fade_status = get_fade_status();
          // Settings
          m_show();

          if(fade_status)
          {
            clear_screen(32, 7);
            insta_fadein();
          }

          game_settings(mzx_world);
          if(fade_status)
            insta_fadeout();

          update_event_status();
          break;
        }

        case SDLK_RETURN: // Enter
        {
          int fade_status = get_fade_status();
          // Menu
          // 19x9

          save_screen();
          if(fade_status)
          {
            clear_screen(32, 7);
            insta_fadein();
          }
          draw_window_box(30, 4, 52, 16, 25, 16, 24, 1, 1);
          write_string(main_menu, 32, 5, 31, 1);
          write_string(" Main Menu ", 36, 4, 30, 0);
          update_screen();

          m_show();

          do
          {
            update_event_status_delay();
            update_screen();
          } while(!get_key(keycode_SDL) && !get_mouse_press());

          if(fade_status)
            insta_fadeout();

          restore_screen();
          update_event_status();
          break;
        }

        case SDLK_ESCAPE: // ESC
        {
          int fade_status = get_fade_status();

          // Quit
          m_show();

          if(fade_status)
          {
            clear_screen(32, 7);
            insta_fadein();
          }

          if(confirm(mzx_world, "Exit MegaZeux - Are you sure?"))
            key = 0;

          if(fade_status)
            insta_fadeout();

          update_event_status();
          break;
        }

        case SDLK_l: // L
        case SDLK_F3: // F3
        {
          load_world_selection(mzx_world);
          fadein = 1;
          src_board = mzx_world->current_board;
          update_event_status();
          break;
        }

        case SDLK_r: // R
        case SDLK_F4: // F4
        {
          int fade_status = get_fade_status();
          char save_file_name[64];

          // Restore
          m_show();
          if(fade_status)
          {
            clear_screen(32, 7);
            insta_fadein();
          }

          if(!choose_file(save_ext, save_file_name, "Choose game to restore", 1))
          {
            // Swap out current board...
            clear_sfx_queue();
            // Load game
            fadein = 0;

            if(reload_savegame(mzx_world, save_file_name, &fadein))
            {
              vquick_fadeout();
            }
            else
            {
              src_board = mzx_world->current_board;
              // Swap in starting board
              load_mod(src_board->mod_playing);
              strcpy(mzx_world->real_mod_playing, src_board->mod_playing);

              send_robot_def(mzx_world, 0, 10);
              // Copy filename
              strcpy(curr_sav, save_file_name);
              dead = 0;
              fadein ^= 1;

              send_robot_def(mzx_world, 0, 11);
              send_robot_def(mzx_world, 0, 10);
              set_counter(mzx_world, "TIME", src_board->time_limit, 0);

              find_player(mzx_world);
              mzx_world->player_restart_x = mzx_world->player_x;
              mzx_world->player_restart_y = mzx_world->player_y;
              vquick_fadeout();

              play_game(mzx_world, 1);

              // Done playing- load world again
              // Already faded out from play_game()
              end_mod();
              // Clear screen
              clear_screen(32, 7);
              // Palette
              default_palette();
              insta_fadein();
              // Reload original file
              if(!stat(curr_file, &file_info))
              {
                reload_world(mzx_world, curr_file, &fade);
                src_board = mzx_world->current_board;
                load_mod(src_board->mod_playing);
                strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
                set_counter(mzx_world, "TIME", src_board->time_limit, 0);
              }
              else
              {
                clear_world(mzx_world);
              }
              vquick_fadeout();
              fadein = 1;
              dead = 0;
            }
            break;
          }

          if(fade_status)
            insta_fadeout();

          update_event_status();
          break;
        }

        case SDLK_p: // P
        case SDLK_F5: // F5
        {
          if(mzx_world->active)
          {
						char old_mod_playing[128];
						strcpy(old_mod_playing, mzx_world->real_mod_playing);

            // Play
            // Only from swap?

            if(mzx_world->only_from_swap)
            {
              m_show();
              error("You can only play this game via a swap from another game",
               0, 24, 0x3101);
              break;
            }

            // Load world curr_file
            // Don't end mod- We want to have smooth transition for that.
            // Clear screen

            clear_screen(32, 7);
            // Palette
            default_palette();

            reload_world(mzx_world, curr_file, &fade);

            mzx_world->current_board_id = mzx_world->first_board;
            mzx_world->current_board =
             mzx_world->board_list[mzx_world->current_board_id];
            src_board = mzx_world->current_board;

            send_robot_def(mzx_world, 0, 11);
            send_robot_def(mzx_world, 0, 10);

						if(strcmp(src_board->mod_playing, "*") &&
						 strcmp(src_board->mod_playing, old_mod_playing))
							load_mod(src_board->mod_playing);

						strcpy(mzx_world->real_mod_playing, src_board->mod_playing);

            set_counter(mzx_world, "TIME", src_board->time_limit, 0);

            clear_sfx_queue();
            find_player(mzx_world);
            mzx_world->player_restart_x = mzx_world->player_x;
            mzx_world->player_restart_y = mzx_world->player_y;
            vquick_fadeout();

            play_game(mzx_world, 1);

            // Done playing- load world again
            // Already faded out from play_game()
            end_mod();
            // Clear screen
            clear_screen(32, 7);
            // Palette
            default_palette();
            insta_fadein();
            // Reload original file
            reload_world(mzx_world, curr_file, &fade);
            src_board = mzx_world->current_board;
            load_mod(src_board->mod_playing);
            strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
            set_counter(mzx_world, "TIME", src_board->time_limit, 0);
            vquick_fadeout();
            fadein = 1;
            dead = 0;
          }
          break;
        }

        case SDLK_F6://F6
        {
          // Debug menu
          debug_mode ^= 1;
          break;
        }

        case SDLK_F7: // F7
        {
          // SMZX Mode
          set_screen_mode(get_screen_mode() + 1);
          break;
        }

        case SDLK_F1: // F1
        {
          m_show();
          help_system(mzx_world);
          break;
        }

				// Quick load
        case SDLK_F10: // F4
        {
          int fade_status = get_fade_status();

          // Restore
          m_show();
          if(fade_status)
          {
            clear_screen(32, 7);
            insta_fadein();
          }

					// Swap out current board...
					clear_sfx_queue();
					// Load game
					fadein = 0;

					if(reload_savegame(mzx_world, curr_sav, &fadein))
					{
             vquick_fadeout();
          }
          else
          {
            src_board = mzx_world->current_board;
            // Swap in starting board
            load_mod(src_board->mod_playing);
            strcpy(mzx_world->real_mod_playing, src_board->mod_playing);

            send_robot_def(mzx_world, 0, 10);
            dead = 0;
            fadein ^= 1;

            send_robot_def(mzx_world, 0, 11);
            send_robot_def(mzx_world, 0, 10);
            set_counter(mzx_world, "TIME", src_board->time_limit, 0);

            find_player(mzx_world);
            mzx_world->player_restart_x = mzx_world->player_x;
            mzx_world->player_restart_y = mzx_world->player_y;
            vquick_fadeout();

            play_game(mzx_world, 1);

            // Done playing- load world again
            // Already faded out from play_game()
            end_mod();
            // Clear screen
            clear_screen(32, 7);
            // Palette
            default_palette();
            insta_fadein();
            // Reload original file
            if(!stat(curr_file, &file_info))
            {
              reload_world(mzx_world, curr_file, &fade);
              src_board = mzx_world->current_board;
              load_mod(src_board->mod_playing);
              strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
              set_counter(mzx_world, "TIME", src_board->time_limit, 0);
            }
            else
            {
              clear_world(mzx_world);
            }
            vquick_fadeout();
            fadein = 1;
            dead = 0;
          }
          break;
        }
      }
      update_event_status();
    }
  } while(key != SDLK_ESCAPE);

  vquick_fadeout();
  clear_sfx_queue();
}

void draw_viewport(World *mzx_world)
{
  int t1, t2;
  Board *src_board = mzx_world->current_board;
  int viewport_x = src_board->viewport_x;
  int viewport_y = src_board->viewport_y;
  int viewport_width = src_board->viewport_width;
  int viewport_height = src_board->viewport_height;
  char edge_color = mzx_world->edge_color;

  // Draw the current viewport
  if(viewport_y > 1)
  {
    // Top
    for(t1 = 0; t1 < viewport_y; t1++)
      fill_line(80, 0, t1, 177, edge_color);
  }

  if((viewport_y + viewport_height) < 24)
  {
    // Bottom
    for(t1 = viewport_y + viewport_height + 1; t1 < 25; t1++)
      fill_line(80, 0, t1, 177, edge_color);
  }

  if(viewport_x > 1)
  {
    // Left
    for(t1 = 0; t1 < 25; t1++)
      fill_line(viewport_x, 0, t1, 177, edge_color);
  }

  if((viewport_x + viewport_width) < 79)
  {
    // Right
    t2 = viewport_x + viewport_width + 1;
    for(t1 = 0; t1 < 25; t1++)
      fill_line(80 - t2, t2, t1, 177, edge_color);
  }

  // Draw the box
  if(viewport_x > 0)
  {
    // left
    for(t1 = 0; t1 < viewport_height; t1++)
      draw_char('º', edge_color, viewport_x - 1, t1 + viewport_y);
    if(viewport_y > 0)
      draw_char('É', edge_color, viewport_x - 1, viewport_y - 1);
  }
  if((viewport_x + viewport_width) < 80)
  {
    // right
    for(t1 = 0; t1 < viewport_height; t1++)
      draw_char('º', edge_color, viewport_x + viewport_width, t1 + viewport_y);
    if(viewport_y > 0)
      draw_char('»', edge_color, viewport_x + viewport_width, viewport_y - 1);
  }
  if(viewport_y > 0)
  {
    // top
    for(t1 = 0; t1 < viewport_width; t1++)
      draw_char('Í', edge_color, viewport_x + t1, viewport_y - 1);
  }
  if((viewport_y + viewport_height) < 25)
  {
    // bottom
    for(t1 = 0; t1 < viewport_width; t1++)
      draw_char('Í', edge_color, viewport_x + t1, viewport_y + viewport_height);
    if(viewport_x > 0)
      draw_char('È', edge_color, viewport_x - 1, viewport_y + viewport_height);
    if((viewport_x + viewport_width) < 80)
      draw_char('¼', edge_color, viewport_x + viewport_width,
       viewport_y + viewport_height);
  }
}

// Updates game variables
// Slowed = 1 to not update lazwall or time due to slowtime or freezetime

void update_variables(World *mzx_world, int slowed)
{
  Board *src_board = mzx_world->current_board;
  int blind_dur = mzx_world->blind_dur;
  int firewalker_dur = mzx_world->firewalker_dur;
  int freeze_time_dur = mzx_world->freeze_time_dur;
  int slow_time_dur = mzx_world->slow_time_dur;
  int wind_dur = mzx_world->wind_dur;
  int b_mesg_timer = src_board->b_mesg_timer;
  int invinco;
  int lazwall_start = src_board->lazwall_start;
  static int slowdown = 0; // Slows certain things down to every other cycle
  slowdown ^= 1;

  // If odd, we...
  if(!slowdown)
  {
    // Change scroll color
    if(++scroll_color > 15) scroll_color = 7;

    // Decrease time limit
    if(!slowed)
    {
      int time_left = get_counter(mzx_world, "TIME", 0);
      if(time_left > 0)
      {
        if(time_left == 1)
        {
          // Out of time
          dec_counter(mzx_world, "HEALTH", 10, 0);
          set_mesg(mzx_world, "Out of time!");
          play_sfx(mzx_world, 42);
          // Reset time
          set_counter(mzx_world, "TIME", src_board->time_limit, 0);
        }
        else
        {
          dec_counter(mzx_world, "TIME", 1, 0);
        }
      }
    }
  }
  // Decrease effect durations
  if(blind_dur > 0)
    mzx_world->blind_dur = blind_dur - 1;

  if(firewalker_dur > 0)
    mzx_world->firewalker_dur = firewalker_dur - 1;

  if(freeze_time_dur > 0)
    mzx_world->freeze_time_dur = freeze_time_dur - 1;

  if(slow_time_dur > 0)
    mzx_world->slow_time_dur = slow_time_dur - 1;

  if(wind_dur > 0)
    mzx_world->wind_dur = wind_dur - 1;

  // Decrease message timer
  if(b_mesg_timer > 0)
    src_board->b_mesg_timer = b_mesg_timer - 1;

  // Invinco
  invinco = get_counter(mzx_world, "INVINCO", 0);
  if(invinco > 0)
  {
    if(invinco == 1)
    {
      // Just finished-
      set_counter(mzx_world, "INVINCO", 0, 0);
    }
    else
    {
      // Decrease
      dec_counter(mzx_world, "INVINCO", 1, 0);
      play_sfx(mzx_world, 17);
      *player_color = rand() & 255;
    }
  }
  // Lazerwall start- cycle 0 to 7 then -7 to -1
  if(!slowed)
  {
    if(((signed char)lazwall_start) < 7)
      src_board->lazwall_start = lazwall_start + 1;
    else
      src_board->lazwall_start = -7;
  }
  // Done
}

void set_mesg(World *mzx_world, char *str)
{
  if(mzx_world->bi_mesg_status)
  {
    set_mesg_direct(mzx_world->current_board, str);
  }
}

void set_mesg_direct(Board *src_board, char *str)
{
  char *bottom_mesg = src_board->bottom_mesg;

  // Sets the current message
  if(strlen(str) > 80)
  {
    memcpy(bottom_mesg, str, 80);
    bottom_mesg[80] = 0;
  }
  else
  {
    strcpy(bottom_mesg, str);
  }
  src_board->b_mesg_timer = 160;
}

void set_3_mesg(World *mzx_world, char *str1, int num, char *str2)
{
  if(mzx_world->bi_mesg_status)
  {
    Board *src_board = mzx_world->current_board;
    sprintf(src_board->bottom_mesg, "%s%d%s", str1, num, str2);
    src_board->b_mesg_timer = 160;
  }
}

//Bit 1- +1
//Bit 2- -1
//Bit 4- +width
//Bit 8- -width

char cw_offs[8] = { 10, 2, 6, 4, 5, 1, 9, 8 };
char ccw_offs[8] = { 10, 8, 9, 1, 5, 4, 6, 2 };

// Rotate an area
void rotate(World *mzx_world, int x, int y, int dir)
{
  Board *src_board = mzx_world->current_board;
  char *offsp = cw_offs;
  int offs[8];
  int offset, i, i2;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int cw, ccw;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char id, param, color;
  char cur_id;
  int d_flag;
  int cur_offset, next_offset;

  offset = x + (y * board_width);
  if((x == 0) || (y == 0) || (x == (board_width - 1)) ||
   (y == (board_height - 1))) return;

  if(dir)
    offsp = ccw_offs;

  // Fix offsets
  for(i = 0; i < 8; i++)
  {
    int offsval = offsp[i];
    if(offsval & 1)
      offs[i] = 1;
    else
      offs[i] = 0;

    if(offsval & 2)
      offs[i]--;

    if(offsval & 4)
      offs[i] += board_width;

    if(offsval & 8)
      offs[i] -= board_width;
  }

  for(i = 0; i < 8; i++)
  {
    cur_id = level_id[offset + offs[i]];
    if((flags[cur_id] & A_UNDER) && (cur_id != 34))
      break;
  }

  if(i == 8)
  {
    for(i = 0; i < 8; i++)
    {
      cur_id = level_id[offset + offs[i]];
      d_flag = flags[cur_id];

      if((!(d_flag & A_PUSHABLE) || (d_flag & A_SPEC_PUSH)) &&
       (cur_id != 47))
        break; // Transport NOT pushable
    }

    if(i == 8)
    {
      cur_offset = offset + offs[0];
      id = level_id[cur_offset];
      color = level_color[cur_offset];
      param = level_param[cur_offset];

      for(i = 0; i < 7; i++)
      {
        cur_offset = offset + offs[i];
        next_offset = offset + offs[i + 1];
        level_id[cur_offset] = level_id[next_offset];
        level_color[cur_offset] = level_color[next_offset];
        level_param[cur_offset] = level_param[next_offset];
      }

      cur_offset = offset + offs[7];
      level_id[cur_offset] = id;
      level_color[cur_offset] = color;
      level_param[cur_offset] = param;
    }
  }
  else
  {
    cw = i - 1;

    if(cw == -1)
      cw = 7;

    do
    {
      ccw = i + 1;
      if(ccw == 8)
        ccw = 0;

      cur_offset = offset + offs[ccw];
      next_offset = offset + offs[i];
      cur_id = level_id[cur_offset];
      d_flag = flags[cur_id];

      if(((d_flag & A_PUSHABLE) || (d_flag & A_SPEC_PUSH)) &&
       (cur_id != 47) && (!(update_done[cur_offset] & 2)))
      {
        offs_place_id(mzx_world, next_offset, cur_id,
         level_color[cur_offset], level_param[cur_offset]);
        offs_remove_id(mzx_world, cur_offset);
        update_done[offset + offs[i]] |= 2;
        i = ccw;
      }
      else
      {
        i = ccw;
        while(i != cw)
        {
          cur_id = level_id[offset + offs[i]];
          if((flags[cur_id] & A_UNDER) && (cur_id != 34))
            break;

          i++;
          if(i == 8)
            i = 0;
        }
      }
    } while(i != cw);
  }
}

void calculate_xytop(World *mzx_world, int *x, int *y)
{
  Board *src_board = mzx_world->current_board;
  int nx, ny;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int viewport_width = src_board->viewport_width;
  int viewport_height = src_board->viewport_height;
  int locked_y = src_board->locked_y;

  // Calculate xy top from player position and scroll view pos, or
  // as static position if set.
  if(locked_y != -1)
  {
    nx = src_board->locked_x + src_board->scroll_x;
    ny = locked_y + src_board->scroll_y;
  }
  else
  {
    // Calculate from player position
    // Center screen around player, add scroll factor
    nx = mzx_world->player_x - (viewport_width / 2);
    ny = mzx_world->player_y - (viewport_height / 2);

    if(nx < 0)
      nx = 0;

    if(ny < 0)
      ny = 0;

    if(nx > (board_width - viewport_width))
     nx = board_width - viewport_width;

    if(ny > (board_height - viewport_height))
     ny = board_height - viewport_height;

    nx += src_board->scroll_x;
    ny += src_board->scroll_y;
  }
  // Prevent from going offscreen
  if(nx < 0)
    nx = 0;

  if(ny < 0)
    ny = 0;

  if(nx > (board_width - viewport_width))
    nx = board_width - viewport_width;

  if(ny > (board_height - viewport_height))
    ny = board_height - viewport_height;

  *x = nx;
  *y = ny;
}

void update_player(World *mzx_world)
{
  Board *src_board = mzx_world->current_board;
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int board_width = src_board->board_width;
  int under_id = src_board->level_under_id[player_x + (player_y * board_width)];

  // t1 = ID stood on
  if(!(flags[under_id] & A_AFFECT_IF_STOOD))
  {
    return; // Nothing special
  }

  switch(under_id)
  {
    case 25: // Ice
    {
      int player_last_dir = src_board->player_last_dir;
      if(player_last_dir & 0x0F)
      {
        if(move_player(mzx_world, (player_last_dir & 0x0F) - 1))
          src_board->player_last_dir = player_last_dir & 0xF0;
      }
      break;
    }

    case 26: // Lava
      if(mzx_world->firewalker_dur > 0) break;
      play_sfx(mzx_world, 22);
      set_mesg(mzx_world, "Augh!");
      dec_counter(mzx_world, "HEALTH", id_dmg[26], 0);
      return;

    case 63: // Fire
      if(mzx_world->firewalker_dur > 0) break;
      play_sfx(mzx_world, 43);
      set_mesg(mzx_world, "Ouch!");
      dec_counter(mzx_world, "HEALTH", id_dmg[63], 0);
      return;

    default: // Water
      move_player(mzx_world, under_id - 21);
      break;
  }

  find_player(mzx_world);
}

//Settings dialog-

//------------------------
//
//  Speed- 123456789
//
//   ( ) Music on
//   ( ) Music off
//
//   ( ) SFX on
//   ( ) SFX off
//
//    OK        Cancel
//
//------------------------

//----------------------------
//
//  Speed- 123456789
//
//   ( ) Digitized music on
//   ( ) Digitized music off
//
//   ( ) PC speaker SFX on
//   ( ) PC speaker SFX off
//
//  Sound card volumes-
//  Overall volume- 12345678
//  SoundFX volume- 12345678
//
//    OK        Cancel
//
//----------------------------

int spd_tmp, music, sfx, mgvol, sgvol;
char stdi_types[8] =
{ DE_NUMBER, DE_RADIO, DE_RADIO, DE_TEXT, DE_NUMBER,
  DE_NUMBER, DE_BUTTON, DE_BUTTON };
char stdi_xs[8] = { 3, 4, 4, 3, 3, 3, 4, 14 };
char stdi_ys[8] = { 2, 4, 7, 10 ,11 ,12 ,14 ,14 };
char *stdi_strs[8] =
{ "Speed- ", "Digitized music on\nDigitized music off",
  "PC speaker SFX on\nPC speaker SFX off", "Sound card volumes-",
  "Overall volume- ", "SoundFX volume- ", "OK", "Cancel" };
int stdi_p1s[8] = { 1, 2, 2, 0, 1, 1, 0, 1 };
int stdi_p2s[6]= { 9, 9, 7, 0, 8, 8 };
void *stdi_storage[6] = { &spd_tmp, &music, &sfx, NULL, &mgvol, &sgvol };
dialog stdi =
{ 25, 4, 54, 20, "Game settings", 8,
  stdi_types, stdi_xs, stdi_ys,
  stdi_strs, stdi_p1s, stdi_p2s, stdi_storage, 0 };

void game_settings(World *mzx_world)
{
  Board *src_board = mzx_world->current_board;
  char temp[FILENAME_SIZE];
  set_context(92);
  spd_tmp = overall_speed;
  music = music_on ^ 1;
  sfx = sfx_on ^ 1;
  mgvol = music_gvol;
  sgvol = sound_gvol;

  if(run_dialog(mzx_world, &stdi))
  {
    pop_context();
    return;
  }
  pop_context();
  if(sound_gvol != sgvol)
  {
    sound_gvol = sgvol;
  }

  if(music_gvol != mgvol)
  {
    music_gvol = mgvol;
    volume_mod(src_board->volume);
  }

  // Check- turn off sound?
  if(sfx == sfx_on)
  {
    sfx_on = sfx ^ 1;
  }
  else
  sfx_on = sfx ^ 1;

  // Check- turn music on/off?
  if(music == music_on)
  {
    char temp2[FILENAME_SIZE];
    if(music_on == 1)
    {
      // Turn off music.
      music_on = 0;
      end_mod();
    }
    else
    {
      music_on = 1;
      // Turn on music.
      strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
      load_mod(mzx_world->real_mod_playing);
    }
  }
  overall_speed = spd_tmp;
}

// Number of cycles to make player idle before repeating a directional move

#define REPEAT_WAIT 2

void play_game(World *mzx_world, int fadein)
{
  // We have the world loaded, on the proper scene.
  // We are faded out. Commence playing!
  int key;
  FILE *fp;
  char keylbl[5] = "KEY?";
  Board *src_board;

  // Main game loop
  // Mouse remains hidden unless menu/etc. is invoked
  // Making the menu functions on by default -Koji

  set_counter(mzx_world, "ENTER_MENU", 1, 0);
  set_counter(mzx_world, "HELP_MENU", 1, 0);
  set_counter(mzx_world, "F2_MENU", 1, 0);

  set_context(91);

  dead = 0;

  do
  {
    // Update

    if(update(mzx_world, 1, &fadein))
    {
      update_event_status();
      continue;
    }
    update_event_status();

    src_board = mzx_world->current_board;

    // Keycheck

    key = get_key(keycode_SDL);

    if(key)
    {
			int key_char = get_key(keycode_unicode);

			if(key_char)
			{
				keylbl[3] = key_char;
				send_robot_all(mzx_world, keylbl);
			}

      switch(key)
      {
        case SDLK_F1: // F1
        {
          m_show();
          help_system(mzx_world);
          break;
        }

        case SDLK_F2: // F2
        {
          // Settings
          if(get_counter(mzx_world, "F2_MENU", 0))
          {
            int fade_status = get_fade_status();

            m_show();

            if(fade_status)
            {
              clear_screen(32, 7);
              insta_fadein();
            }
            game_settings(mzx_world);
            if(fade_status)
              insta_fadeout();

            update_event_status();
          }
          break;
        }

        case SDLK_RETURN: // Enter
        {
          send_robot_all(mzx_world, "KeyEnter");
          // Menu
          // 19x9
          if(get_counter(mzx_world, "ENTER_MENU", 0))
          {
            int fade_status = get_fade_status();
            save_screen();

            if(fade_status)
            {
              clear_screen(32, 7);
              insta_fadein();
            }

            draw_window_box(8, 4, 35, 18, 25, 16, 24, 1, 1);
            write_string(game_menu, 10, 5, 31, 1);
            write_string(" Game Menu ", 14, 4, 30, 0);
            show_status(mzx_world); // Status screen too
            update_screen();
            m_show();
            do
            {
              update_event_status_delay();
              update_screen();
            } while(!get_key(keycode_SDL) && !get_mouse_press());

            if(fade_status)
              insta_fadeout();

            restore_screen();
            if(fade_status)
              insta_fadeout();

            update_event_status();
          }
          break;
        }

        case SDLK_ESCAPE: // ESC
        {
          int fade_status = get_fade_status();
          // Quit
          m_show();

          if(fade_status)
          {
            clear_screen(32, 7);
            insta_fadein();
          }

          if(confirm(mzx_world, "Quit playing- Are you sure?"))
            key = 0;

          if(fade_status)
            insta_fadeout();

          update_event_status();
          break;
        }

        case SDLK_F3: // F3
        {
          // Save game
          if(!dead)
          {
            // Can we?
            if((cheats_active <= 1) && (src_board->save_mode != CANT_SAVE) &&
             ((src_board->save_mode != CAN_SAVE_ON_SENSOR) ||
             (src_board->level_under_id[mzx_world->player_x +
             (src_board->board_width * mzx_world->player_y)] == 122)))
            {
              int fade_status = get_fade_status();
              struct stat file_info;
              m_show();
              // If faded...
              if(fade_status)
              {
                clear_screen(32, 7);
                insta_fadein();
              }
              if(!save_game_dialog(mzx_world))
              {
                // Name in curr_sav....
                if(curr_sav[0] == 0)
                {
                  if(fade_status)
                    insta_fadeout();
                  break;
                }
                add_ext(curr_sav, ".sav");
                // Check for an overwrite
                fp = fopen(curr_sav, "rb");

                if(!stat(curr_sav, &file_info))
                {
                  if(confirm(mzx_world, "File exists- Overwrite?"))
                  {
                    if(fade_status)
                      insta_fadeout();
                    break;
                  }
                }
                // Save entire game
                save_world(mzx_world, curr_sav, 1, fade_status);
              }
              if(fade_status)
                insta_fadeout();
            }
          }
          update_event_status();
          break;
        }

        case SDLK_F4:
        {
          // Restore
          if(cheats_active <= 1)
          {
            char save_file_name[64];
            int fade_status = get_fade_status();
            m_show();

            if(fade_status)
            {
              clear_screen(32, 7);
              insta_fadein();
            }

            if(!choose_file(save_ext, save_file_name,
             "Choose game to restore", 1))
            {
              // Load game
              fadein = 0;
              if(reload_savegame(mzx_world, save_file_name, &fadein))
              {
                vquick_fadeout();
                return;
              }

              // Reset this
              src_board = mzx_world->current_board;
              // Swap in starting board
              load_mod(src_board->mod_playing);
              strcpy(mzx_world->real_mod_playing, src_board->mod_playing);

              strcpy(curr_sav, save_file_name);
              send_robot_def(mzx_world, 0, 10);
              dead = 0;
              fadein ^= 1;
            }
            if(fade_status)
              insta_fadeout();
          }
          update_event_status();
          break;
        }

        case SDLK_F5: // F5
        case SDLK_INSERT: // Ins
        {
          // Change bomb type
          if(!dead)
          {
            bomb_type ^= 1;
            if((!src_board->player_attack_locked) && (src_board->can_bomb))
            {
              play_sfx(mzx_world, 35);
              if(bomb_type)
                set_mesg(mzx_world, "You switch to high strength bombs.");
              else
                set_mesg(mzx_world, "You switch to low strength bombs.");
            }
          }
          break;
        }

        case SDLK_F6: // F6
        {
          // Debug menu
          debug_mode ^= 1;
          break;
        }

        case SDLK_F7: // F7
        {
          int i;

          // Cheat #1- Give all
          set_counter(mzx_world, "GEMS", 32767, 1);
          set_counter(mzx_world, "AMMO", 32767, 1);
          set_counter(mzx_world, "HEALTH", 32767, 1);
          set_counter(mzx_world, "COINS", 32767, 1);
          set_counter(mzx_world, "LIVES", 32767, 1);
          set_counter(mzx_world, "TIME", src_board->time_limit, 1);
          set_counter(mzx_world, "LOBOMBS", 32767, 1);
          set_counter(mzx_world, "HIBOMBS", 32767, 1);

          mzx_world->score = 0;

          for(i = 0; i < 16; i++)
					{
            mzx_world->keys[i] = i;
					}

          dead = 0; // :)
          src_board->player_ns_locked = 0;
          src_board->player_ew_locked = 0;
          src_board->player_attack_locked = 0;

          break;
        }

        case SDLK_F8:
				{
					int player_x = mzx_world->player_x;
					int player_y = mzx_world->player_y;
					int board_width = src_board->board_width;
					int board_height = src_board->board_height;

          if(player_x > 0)
          {
						place_at_xy(mzx_world, 0, 7, 0, player_x - 1, player_y);
            if(player_y > 0)
							place_at_xy(mzx_world, 0, 7, 0, player_x - 1, player_y - 1);

            if(player_y < (board_height - 1))
							place_at_xy(mzx_world, 0, 7, 0, player_x - 1, player_y + 1);
          }

          if(player_x < (board_width - 1))
          {
						place_at_xy(mzx_world, 0, 7, 0, player_x + 1, player_y);

            if(player_y > 0)
							place_at_xy(mzx_world, 0, 7, 0, player_x + 1, player_y - 1);

            if(player_y < (board_height - 1))
							place_at_xy(mzx_world, 0, 7, 0, player_x + 1, player_y + 1);
          }

          if(player_y > 0)
						place_at_xy(mzx_world, 0, 7, 0, player_x, player_y - 1);

          if(player_y < (board_height - 1))
						place_at_xy(mzx_world, 0, 7, 0, player_x, player_y + 1);

          break;
        }

				// Quick save
				case SDLK_F9:
				{
          if(!dead)
          {
            // Can we?
            if((cheats_active <= 1) && (src_board->save_mode != CANT_SAVE) &&
             ((src_board->save_mode != CAN_SAVE_ON_SENSOR) ||
             (src_board->level_under_id[mzx_world->player_x +
             (src_board->board_width * mzx_world->player_y)] == 122)))
            {
							int fade_status;

              // Save entire game
              save_world(mzx_world, curr_sav, 1, fade_status);

              if(fade_status)
                insta_fadeout();
            }
					}
					break;
				}

				// Quick load
				case SDLK_F10:
				{
					struct stat file_info;

					if(!stat(curr_sav, &file_info))
					{
						// Load game
						fadein = 0;
						if(reload_savegame(mzx_world, curr_sav, &fadein))
						{
							vquick_fadeout();
							return;
						}
	
						// Reset this
						src_board = mzx_world->current_board;
						// Swap in starting board
						load_mod(src_board->mod_playing);
						strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
	
						send_robot_def(mzx_world, 0, 10);
						dead = 0;
						fadein ^= 1;
					}
					break;
				}

				case SDLK_F11: // F11
				{
					// SMZX Mode
					set_screen_mode(get_screen_mode() + 1);
					break;
				}
      }
    }
  } while(key != SDLK_ESCAPE);

  pop_context();
  vquick_fadeout();
  clear_sfx_queue();
}

void place_player(World *mzx_world, int x, int y, int dir)
{
  Board *src_board = mzx_world->current_board;
  if((mzx_world->player_x != x) || (mzx_world->player_y != y))
  {
    id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
  }
  id_place(mzx_world, x, y, 127, 0, 0);
  mzx_world->player_x = x;
  mzx_world->player_y = y;
  src_board->player_last_dir = (src_board->player_last_dir & 240) | (dir + 1);
}

// Returns 1 if didn't move
int move_player(World *mzx_world, int dir)
{
  Board *src_board = mzx_world->current_board;
  // Dir is from 0 to 3
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int new_x = player_x;
  int new_y = player_y;
  int edge = 0;

  switch(dir)
  {
    case 0:
      if(--new_y < 0)
        edge = 1;
      break;
    case 1:
      if(++new_y >= src_board->board_height)
        edge = 1;
      break;
    case 2:
      if(++new_x >= src_board->board_width)
        edge = 1;
      break;
    case 3:
      if(--new_x < 0)
        edge = 1;
      break;
  }

  if(edge)
  {
    // Hit an edge, teleport to another board?
    int board_dir = src_board->board_dir[dir];
    // Board must be valid
    if((board_dir == NO_BOARD) || (board_dir >= mzx_world->num_boards) ||
     (!mzx_world->board_list[board_dir]))
      return 1;

    target_board = board_dir;
    target_where = 0;
    target_x = player_x;
    target_y = player_y;

    switch(dir)
    {
      case 0: // North- Enter south side
        target_y = 199;
        break;
      case 1: // South- Enter north side
        target_y = 0;
        break;
      case 2: // East- Enter west side
        target_x = 0;
        break;
      case 3: // West- Enter east side
        target_x = 399;
        break;
    }
    src_board->player_last_dir = (src_board->player_last_dir & 240) + dir + 1;
    return 0;
  }
  else
  {
    // Not edge
    int d_offset = new_x + (new_y * src_board->board_width);
    char d_id = src_board->level_id[d_offset];
    int d_flag = flags[d_id];

    if(d_flag & A_SPEC_STOOD)
    {
      // Sensor
      // Activate label and then move player
      char d_param = src_board->level_param[d_offset];
      send_robot(mzx_world,
       (src_board->sensor_list[d_param])->robot_to_mesg, "SENSORON", 0);
      place_player(mzx_world, new_x, new_y, dir);
    }
    else

    if(d_flag & A_ENTRANCE)
    {
      // Entrance
      char d_board = src_board->level_param[d_offset];
      play_sfx(mzx_world, 37);
      // Can move?
      if((d_board != mzx_world->current_board_id) &&
       (d_board < mzx_world->num_boards) &&
       (mzx_world->board_list[d_board]))
      {
        // Go to board t1 AFTER showing update
        target_board = d_board;
        target_where = 1;
        target_x = src_board->level_color[d_offset];
        target_y = d_id;
      }

      place_player(mzx_world, new_x, new_y, dir);
    }
    else

    if((d_flag & A_ITEM) && (d_id != 123))
    {
      // Item
      char d_under_id = mzx_world->under_player_id;
      char d_under_color = mzx_world->under_player_color;
      char d_under_param = mzx_world->under_player_param;
      int grab_result = grab_item(mzx_world, d_offset, dir);
      if(grab_result)
      {
        if(d_id == 49)
        {
          int player_last_dir = src_board->player_last_dir;
          // Teleporter
          id_remove_top(mzx_world, player_x, player_y);
          mzx_world->under_player_id = d_under_id;
          mzx_world->under_player_color = d_under_color;
          mzx_world->under_player_param = d_under_param;
          src_board->player_last_dir = (player_last_dir & 240) + dir + 1;
          // New player x/y will be found after update !!! maybe fix.
        }
        else
        {
          place_player(mzx_world, new_x, new_y, dir);
        }
        return 0;
      }
      return 1;
    }
    else

    if(d_flag & A_UNDER)
    {
      // Under
      place_player(mzx_world, new_x, new_y, dir);
      return 0;
    }
    else

    if((d_flag & A_ENEMY) || (d_flag & A_HURTS))
    {
      if(d_id == 61)
      {
        // Bullet
        if((src_board->level_param[d_offset] >> 2) == PLAYER_BULLET)
        {
          // Player bullet no hurty
          id_remove_top(mzx_world, new_x, new_y);
          place_player(mzx_world, new_x, new_y, dir);
          return 0;
        }
        else
        {
          // Enemy or hurtsy
          dec_counter(mzx_world, "HEALTH", id_dmg[61], 0);
          play_sfx(mzx_world, 21);
          set_mesg(mzx_world, "Ouch!");
        }
      }
      else
      {
        dec_counter(mzx_world, "HEALTH", id_dmg[d_id], 0);
        play_sfx(mzx_world, 21);
        set_mesg(mzx_world, "Ouch!");

        if(d_flag & A_ENEMY)
        {
          // Kill/move
          id_remove_top(mzx_world, new_x, new_y);
          if((d_id != 34) && (!src_board->restart_if_zapped)) // Not onto goop..
          if(!src_board->restart_if_zapped)
          {
            place_player(mzx_world, new_x, new_y, dir);
            return 0;
          }
        }
      }
    }
    else
    {
      int dir_mask;
      // Check for push
      if(dir > 1)
        dir_mask = d_flag & A_PUSHEW;
      else
        dir_mask = d_flag & A_PUSHNS;

      if(dir_mask || (d_flag & A_SPEC_PUSH))
      {
        // Push
        // Pushable robot needs to be sent the touch label
        if(d_id == 123)
          send_robot_def(mzx_world,
           src_board->level_param[d_offset], 0);

        if(!push(mzx_world, player_x, player_y, dir, 0))
        {
          place_player(mzx_world, new_x, new_y, dir);
          return 0;
        }
      }
    }
    // Nothing.
  }
  return 1;
}

char door_first_movement[8] = { 0, 3, 0, 2, 1, 3, 1, 2 };

void give_potion(World *mzx_world, int type)
{
  Board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;

  play_sfx(mzx_world, 39);
  inc_counter(mzx_world, "SCORE", 5, 0);

  switch(type)
  {
    case 0: // Dud
    {
      set_mesg(mzx_world, "* No effect *");
      break;
    }

    case 1: // Invinco
    {
      set_mesg(mzx_world, "* Invinco *");
      send_robot_def(mzx_world, 0, 2);
      set_counter(mzx_world, "INVINCO", 113, 0);
      break;
    }

    case 2: // Blast
    {
      int x, y, offset;
      // Adjust the rate for board size - it was hardcoded for 10000
      int placement_rate = 18 * (board_width * board_height) / 10000;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          char d_id = level_id[offset];
          int d_flag = flags[d_id];

          if((d_flag & A_UNDER) && !(d_flag & A_ENTRANCE))
          {
            // Adjust the ratio for board size

            if((rand() % placement_rate) == 0)
            {
              id_place(mzx_world, x, y, 38, 0, 16 * ((rand() % 5) + 2));
            }
          }
        }
      }

      set_mesg(mzx_world, "* Blast *");
      break;
    }

    case 3: // Health +10
    {
      inc_counter(mzx_world, "HEALTH", 10, 0);
      set_mesg(mzx_world, "* Healing *");
      break;
    }

    case 4: // Health +50
    {
      inc_counter(mzx_world, "HEALTH", 50, 0);
      set_mesg(mzx_world, "* Healing *");
      break;
    }

    case 5: // Poison
    {
      dec_counter(mzx_world, "HEALTH", 10, 0);
      set_mesg(mzx_world, "* Poison *");
      break;
    }

    case 6: // Blind
    {
      mzx_world->blind_dur = 200;
      set_mesg(mzx_world, "* Blind *");
      break;
    }

    case 7: // Kill
    {
      int x, y, offset;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          char d_id = level_id[offset];
          // Enemy
          if((d_id > 79) && (d_id < 96) && (d_id != 92) && (d_id != 93))
            id_remove_top(mzx_world, x, y);
        }
      }
      set_mesg(mzx_world, "* Kill enemies *");
      break;
    }

    case 8: // Firewalk
    {
      mzx_world->firewalker_dur = 200;
      set_mesg(mzx_world, "* Firewalking *");
      break;
    }

    case 9: // Detonate
    {
      int x, y, offset;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          if(level_id[offset] == 36)
          {
            level_id[offset] = 38;
            if(level_param[offset] == 0)
              level_param[offset] = 32;
            else
              level_param[offset] = 64;
          }
        }
      }
      set_mesg(mzx_world, "* Detonate *");
      break;
    }

    case 10: // Banish
    {
      int x, y, offset;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          if(level_id[offset] == 86)
          {
            level_id[offset] = 85;
            level_param[offset] = 51;
          }
        }
      }
      set_mesg(mzx_world, "* Banish dragons *");
      break;
    }

    case 11: // Summon
    {
      int x, y, offset;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          char d_id = level_id[offset];
          if((d_id > 79) && (d_id < 96) && (d_id != 92) && (d_id != 93))
          {
            // Enemy
            level_id[offset] = 86;
            level_color[offset] = 4;
            level_param[offset] = 102;
          }
        }
      }
      set_mesg(mzx_world, "* Summon dragons *");
      break;
    }

    case 12: // Avalanche
    {
      int x, y, offset;
      // Adjust the rate for board size - it was hardcoded for 10000
      int placement_rate = 18 * (board_width * board_height) / 10000;

      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          int d_flag = flags[level_id[offset]];

          if((d_flag & A_UNDER) && !(d_flag & A_ENTRANCE))
          {
            if((rand() % placement_rate) == 0)
            {
              id_place(mzx_world, x, y, 8, 7, 0);
            }
          }
        }
      }
      set_mesg(mzx_world, "* Avalanche *");
      break;
    }

    case 13: // Freeze
    {
      mzx_world->freeze_time_dur = 200;
      set_mesg(mzx_world, "* Freeze time *");
      break;
    }

    case 14: // Wind
    {
      mzx_world->wind_dur = 200;
      set_mesg(mzx_world, "* Wind *");
      break;
    }

    case 15: // Slow
    {
      mzx_world->slow_time_dur = 200;
      set_mesg(mzx_world, "* Slow time *");
      break;
    }
  }
}

int grab_item(World *mzx_world, int offset, int dir)
{
  // Dir is for transporter
  Board *src_board = mzx_world->current_board;
  char id = src_board->level_id[offset];
  char param = src_board->level_param[offset];
  char color = src_board->level_color[offset];
  int fade = get_fade_status();
  int remove = 0;

  char tmp[81];

  switch(id)
  {
    case 27: // Chest
    {
      int item;

      if(!(param & 15))
      {
        play_sfx(mzx_world, 40);
        break;
      }

      // Act upon contents
      play_sfx(mzx_world, 41);
      item = ((param & 240) >> 4); // Amount for most things
      switch(param & 15)
      {
        case 1: // Key
        {
          if(give_key(mzx_world, item))
          {
            set_mesg(mzx_world,
             "Inside the chest is a key, but you can't carry any more keys!");
            return 0;
          }
          set_mesg(mzx_world, "Inside the chest you find a key.");
          break;
        }

        case 2: // Coins
        {
          item *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ", item, " coins.");
          inc_counter(mzx_world, "COINS", item, 0);
          inc_counter(mzx_world, "SCORE", item, 0);
          break;
        }

        case 3: // Life
        {
          if(item > 1)
            set_3_mesg(mzx_world, "Inside the chest you find ", item, " free lives.");
          else
            set_mesg(mzx_world, "Inside the chest you find 1 free life.");
          inc_counter(mzx_world, "LIVES", item, 0);
          break;
        }

        case 4: // Ammo
        {
          item *= 5;
          set_3_mesg(mzx_world,
           "Inside the chest you find ", item, " rounds of ammo.");
          inc_counter(mzx_world, "AMMO", item, 0);
          break;
        }

        case 5: // Gems
        {
          item *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ", item, " gems.");
          inc_counter(mzx_world, "GEMS", item, 0);
          inc_counter(mzx_world, "SCORE", item, 0);
          break;
        }

        case 6: // Health
        {
          item *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ", item, " health.");
          inc_counter(mzx_world, "HEALTH", item, 0);
          break;
        }

        case 7: // Potion
        {
          int answer;
          m_show();
          if(fade)
          {
            clear_screen(32, 7);
            insta_fadein();
          }
          answer = confirm(mzx_world,
           "Inside the chest you find a potion. Drink it?");
          if(fade)
            insta_fadeout();

          if(answer)
          {
            return 0;
          }

          src_board->level_param[offset] = 0;
          give_potion(mzx_world, item);
          break;
        }

        case 8: // Ring
        {
          int answer;

          m_show();
          if(fade)
          {
            clear_screen(32, 7);
            insta_fadein();
          }

          answer = confirm(mzx_world, "Inside the chest you find a ring. Wear it?");

          if(fade)
            insta_fadeout();

          if(answer)
          {
            return 0;
          }

          src_board->level_param[offset] = 0;
          give_potion(mzx_world, item);
          break;
        }

        case 9: // Lobombs
        {
          item *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ", item,
           " low strength bombs.");
          inc_counter(mzx_world, "LOBOMBS", item, 0);
          break;
        }

        case 10: // Hibombs
        {
          item *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ", item,
           " high strength bombs.");
          inc_counter(mzx_world, "HIBOMBS", item, 0);
          break;
        }
      }
      // Empty chest
      src_board->level_param[offset] = 0;
      break;
    }

    case 28: // Gem
    case 29: // Magic gem
    {
      play_sfx(mzx_world, id - 28);
      inc_counter(mzx_world, "GEMS", 1, 0);
      if(id == 29)
        inc_counter(mzx_world, "HEALTH", 1, 0);
      inc_counter(mzx_world, "SCORE", 1, 0);
      remove = 1;
      break;
    }

    case 30: // Health
    {
      play_sfx(mzx_world, 2);
      inc_counter(mzx_world, "HEALTH", param, 0);
      remove = 1;
      break;
    }

    case 31: // Ring
    case 32: // Potion
    {
      give_potion(mzx_world, param);
      remove = 1;
      break;
    }

    case 34: // Goop
    {
      play_sfx(mzx_world, 48);
      set_mesg(mzx_world, "There is goop in your way!");
      send_robot_def(mzx_world, 0, 12);
      break;
    }

    case 33: // Energizer
    {
      play_sfx(mzx_world, 16);
      set_mesg(mzx_world, "Energize!");
      send_robot_def(mzx_world, 0, 2);
      set_counter(mzx_world, "INVINCO", 113, 0);
      remove = 1;
      break;
    }

    case 35: // Ammo
    {
      play_sfx(mzx_world, 3);
      inc_counter(mzx_world, "AMMO", param, 0);
      remove = 1;
      break;
    }

    case 36: // Bomb
    {
      if(src_board->collect_bombs)
      {
        if(param)
        {
          play_sfx(mzx_world, 7);
          inc_counter(mzx_world, "HIBOMBS", 1, 0);
        }
        else
        {
          play_sfx(mzx_world, 6);
          inc_counter(mzx_world, "LOBOMBS", 1, 0);
        }
        remove = 1;
      }
      else
      {
        // Light bomb
        play_sfx(mzx_world, 33);
        src_board->level_id[offset] = 37;
        src_board->level_param[offset] = param << 7;
      }
      break;
    }

    case 39: // Key
    {
      if(give_key(mzx_world, color))
      {
        play_sfx(mzx_world, 9);
        set_mesg(mzx_world, "You can't carry any more keys!");
      }
      else
      {
        play_sfx(mzx_world, 8);
        set_mesg(mzx_world, "You grab the key.");
        remove = 1;
      }
      break;
    }

    case 40: // Lock
    {
      if(take_key(mzx_world, color))
      {
        play_sfx(mzx_world, 11);
        set_mesg(mzx_world, "You need an appropriate key!");
      }
      else
      {
        play_sfx(mzx_world, 10);
        set_mesg(mzx_world, "You open the lock.");
        remove = 1;
      }
      break;
    }

    case 41: // Door
    {
      int board_width = src_board->board_width;
      int board_height = src_board->board_height;
      char *level_id = src_board->level_id;
      char *level_param = src_board->level_param;
      int x = offset % board_width;
      int y = offset / board_width;

      if(param & 8)
      {
        // Locked
        if(take_key(mzx_world, color))
        {
          // Need key
          play_sfx(mzx_world, 19);
          set_mesg(mzx_world, "The door is locked!");
          break;
        }

        // Unlocked
        set_mesg(mzx_world, "You unlock and open the door.");
      }
      else
      {
        set_mesg(mzx_world, "You open the door.");
      }

      src_board->level_id[offset] = 42;
      src_board->level_param[offset] = (param & 7);

      if(move(mzx_world, x, y, door_first_movement[param & 7], 1 | 4 | 8 | 16))
      {
        set_mesg(mzx_world, "The door is blocked from opening!");
        play_sfx(mzx_world, 19);
        level_id[offset] = 41;
        level_param[offset] = param & 7;
      }
      else
      {
        play_sfx(mzx_world, 20);
      }
      break;
    }

    case 47: // Gate
    {
      if(param)
      {
        // Locked
        if(take_key(mzx_world, color))
        {
          // Need key
          play_sfx(mzx_world, 14);
          set_mesg(mzx_world, "The gate is locked!");
          break;
        }
        // Unlocked
        set_mesg(mzx_world, "You unlock and open the gate.");
      }
      else
      {
        set_mesg(mzx_world, "You open the gate.");
      }

      src_board->level_id[offset] = 48;
      src_board->level_param[offset] = 22;
      play_sfx(mzx_world, 15);
      break;
    }

    case 49: // Transport
    {
      int x = offset % src_board->board_width;
      int y = offset / src_board->board_width;

      if(transport(mzx_world, x, y, dir, 127, 0, 0, 1)) break;
      return 1;
    }

    case 50: // Coin
    {
      play_sfx(mzx_world, 4);
      inc_counter(mzx_world, "COINS", 1, 0);
      inc_counter(mzx_world, "SCORE", 1, 0);
      remove = 1;
      break;
    }

    case 55: // Pouch
    {
      play_sfx(mzx_world, 38);
      inc_counter(mzx_world, "GEMS", (param & 15) * 5, 0);
      inc_counter(mzx_world, "COINS", (param >> 4) * 5, 0);
      inc_counter(mzx_world, "SCORE", ((param & 15) + (param >> 4)) * 5, 1);
      sprintf(tmp, "The pouch contains %d gems and %d coins.",
       (param & 15) * 5, (param >> 4) * 5);
      set_mesg(mzx_world, tmp);
      remove = 1;
      break;
    }

    case 65: // Forest
    {
      play_sfx(mzx_world, 13);
      if(src_board->forest_becomes == FOREST_TO_EMPTY)
      {
        remove = 1;
      }
      else
      {
        src_board->level_id[offset] = 15;
        return 1;
      }
      break;
    }

    case 66: // Life
    {
      play_sfx(mzx_world, 5);
      inc_counter(mzx_world, "LIVES", 1, 0);
      set_mesg(mzx_world, "You find a free life!");
      remove = 1;
      break;
    }

    case 71: // Invis. wall
    {
      src_board->level_id[offset] = 1;
      set_mesg(mzx_world, "Oof! You ran into an invisible wall.");
      play_sfx(mzx_world, 12);
      break;
    }

    case 74: // Mine
    {
      src_board->level_id[offset] = 38;
      src_board->level_param[offset] = param & 240;
      play_sfx(mzx_world, 36);
      break;
    }

    case 81: // Eye
    {
      src_board->level_id[offset] = 38;
      src_board->level_param[offset] = (param << 1) & 112;
      break;
    }

    case 82: // Thief
    {
      dec_counter(mzx_world, "GEMS", (param & 128) >> 7, 0);
      play_sfx(mzx_world, 44);
      break;
    }

    case 83: // Slime
    {
      if(param & 64)
      {
        // Hurtsy
        dec_counter(mzx_world, "HEALTH", id_dmg[83], 0);
        play_sfx(mzx_world, 21);
        set_mesg(mzx_world, "Ouch!");
      }
      if(param & 128)
        break; // Invinco
      src_board->level_id[offset] = 6;
      break;
    }

    case 85: // Ghost
    {
      // Hurtsy
      dec_counter(mzx_world, "HEALTH", id_dmg[85], 0);
      play_sfx(mzx_world, 21);
      set_mesg(mzx_world, "Ouch!");
      // Die !?
      if(!(param & 8))
        remove = 1;
      break;
    }

    case 86: // Dragon
    {
      // Hurtsy
      dec_counter(mzx_world, "HEALTH", id_dmg[86], 0);
      play_sfx(mzx_world, 21);
      set_mesg(mzx_world, "Ouch!");
      break;
    }

    case 87: // Fish
    {
      if(param & 64)
      {
        // Hurtsy
        dec_counter(mzx_world, "HEALTH", id_dmg[87], 0);
        play_sfx(mzx_world, 21);
        set_mesg(mzx_world, "Ouch!");
        remove = 1;
      }
      break;
    }

    case 124: // Robot
    {
      send_robot_def(mzx_world, param, 0);
      break;
    }

    case 125: // Sign
    case 126: // Scroll
    {
      play_sfx(mzx_world, 47);

      m_show();
      scroll_edit(mzx_world, src_board->scroll_list[param], id & 1);

      if(id == 126)
      {
        remove = 1;
      }
      break;
    }
  }

  if(remove == 1)
  {
    offs_remove_id(mzx_world, offset);
  }

  return remove; // Not grabbed
}

// Show status screen
void show_status(World *mzx_world)
{
  int i;
  char temp[11];
  char (*status_counters_shown)[15] = mzx_world->status_counters_shown;
  char *keys = mzx_world->keys;

  draw_window_box(37, 4, 67, 21, 25, 16, 24, 1, 1);
  show_counter(mzx_world, "Gems", 39, 5, 0);
  show_counter(mzx_world, "Ammo", 39, 6, 0);
  show_counter(mzx_world, "Health", 39, 7, 0);
  show_counter(mzx_world, "Lives", 39, 8, 0);
  show_counter(mzx_world, "Lobombs", 39, 9, 0);
  show_counter(mzx_world, "Hibombs", 39, 10, 0);
  show_counter(mzx_world, "Coins", 39, 11, 0);

  for(i = 0; i < NUM_STATUS_CNTRS; i++) // Show custom status counters
  {
    if(status_counters_shown[i][0])
      show_counter(mzx_world, status_counters_shown[i], 39, i + 15, 1);
  }

  write_string("Score", 39, 12, 27, 0);
  sprintf(temp, "%d", mzx_world->score);
  write_string(temp, 55, 12, 31, 0); // Show score
  write_string("Keys", 39, 13, 27, 0);

  for(i = 0; i < 8; i++) // Show keys
  {
    if(keys[i] != NO_KEY)
      draw_char('', 16 + keys[i], 55 + i, 13);
  }

  for(; i < 16; i++) // Show keys, 2nd row
  {
    if(keys[i] != NO_KEY)
      draw_char('', 16 + keys[i], 47 + i, 14);
  }

  // Show hi/lo bomb selection

  write_string("(cur.)", 48, 9 + bomb_type, 25, 0);
  // Show effects
  if(mzx_world->firewalker_dur > 0)
    write_string("-W-", 43, 21, 28, 0);

  if(mzx_world->freeze_time_dur > 0)
    write_string("-F-", 47, 21, 27, 0);

  if(mzx_world->slow_time_dur > 0)
    write_string("-S-", 51, 21, 30, 0);

  if(mzx_world->wind_dur > 0)
    write_string("-W-", 55, 21, 31, 0);

  if(mzx_world->blind_dur > 0)
    write_string("-B-", 59, 21, 25, 0);
}

void show_counter(World *mzx_world, char *str, int x, int y,
 int skip_if_zero)
{
  int counter_value = get_counter(mzx_world, str, 0);
  if((skip_if_zero) && (!counter_value))
    return;
  write_string(str, x, y, 27, 0); // Name

  write_number(counter_value, 31, x + 16, y, 1); // Value
}

// Returns non-0 to skip all keys this cycle
int update(World *mzx_world, int game, int *fadein)
{
  int start_ticks = get_ticks();
  char r, g, b;
  int time_remaining;
  static int reload = 0;
  static int slowed = 0; // Flips between 0 and 1 during slow_time
  char tmp_str[10];
  Board *src_board = mzx_world->current_board;
  int volume = src_board->volume;
  int volume_inc = src_board->volume_inc;
  int volume_target = src_board->volume_target;
  char *bottom_mesg = src_board->bottom_mesg;
  int mesg_edges = mzx_world->mesg_edges;
  int player_attack_locked = src_board->player_attack_locked;
  int player_last_dir = src_board->player_last_dir;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char *level_id = src_board->level_id;
  char *level_color = src_board->level_color;
  char *level_param = src_board->level_param;
  char *level_under_id = src_board->level_under_id;
  char *level_under_color = src_board->level_under_color;
  char *level_under_param = src_board->level_under_param;
  int fade_status = get_fade_status();
  int total_ticks;

  pal_update = 0;

  if((game) && get_counter(mzx_world, "CURSORSTATE", 0))
  {
    // Turn on mouse
    m_show();
  }
  else
  {
    // Turn off mouse
    m_hide();
  }

  // Fade mod
  if(volume_inc)
  {
    int result_volume = volume;
    result_volume += volume_inc;
    if(volume_inc < 0)
    {
      if(result_volume <= volume_target)
      {
        result_volume = volume_target;
        volume_inc = 0;
      }
    }
    else
    {
      if(result_volume >= volume_target)
      {
        result_volume = volume_target;
        volume_inc = 0;
      }
    }
    src_board->volume = result_volume;
    volume_mod(volume);
  }

  // Slow_time- flip slowed
  if(mzx_world->freeze_time_dur)
    slowed = 1;
  else

  if(mzx_world->slow_time_dur)
    slowed ^= 1;
  else
    slowed = 0;

  // Update
  update_variables(mzx_world, slowed);
  update_player(mzx_world); // Ice, fire, water, lava

  if(mzx_world->wind_dur > 0)
  {
    // Wind
    int wind_dir = rand() % 9;
    if(wind_dir < 4)
    {
      // No wind this turn if above 3
      src_board->player_last_dir = (src_board->player_last_dir & 0xF0) + wind_dir;
      move_player(mzx_world, wind_dir);
    }
  }

  // The following is during gameplay ONLY
  if((game) && (!dead))
  {
    // Shoot
    if(get_key_status(keycode_SDL, SDLK_SPACE))
    {
      if((!reload) && (!src_board->player_attack_locked))
      {
        int move_dir = -1;

        if(get_key_status(keycode_SDL, SDLK_UP))
          move_dir = 0;
        else

        if(get_key_status(keycode_SDL, SDLK_DOWN))
          move_dir = 1;
        else

        if(get_key_status(keycode_SDL, SDLK_RIGHT))
          move_dir = 2;
        else

        if(get_key_status(keycode_SDL, SDLK_LEFT))
          move_dir = 3;

        if(move_dir != -1)
        {
          if(!src_board->can_shoot)
            set_mesg(mzx_world, "Can't shoot here!");
          else

          if(!get_counter(mzx_world, "AMMO", 0))
          {
            set_mesg(mzx_world, "You are out of ammo!");
            play_sfx(mzx_world, 30);
          }
          else
          {
            dec_counter(mzx_world, "AMMO", 1, 0);
            play_sfx(mzx_world, 28);
            shoot(mzx_world, mzx_world->player_x, mzx_world->player_y,
             move_dir, PLAYER_BULLET);
            reload = 2;
            src_board->player_last_dir = (src_board->player_last_dir & 0x0F) |
             (move_dir << 4);
          }
        }
      }
    }
    else

    if((get_key_status(keycode_SDL, SDLK_UP)) &&
     (!src_board->player_ns_locked))
    {
      int key_up_delay = mzx_world->key_up_delay;
      if((key_up_delay == 0) || (key_up_delay > REPEAT_WAIT))
      {
        move_player(mzx_world, 0);
        src_board->player_last_dir = (src_board->player_last_dir & 0x0F);
      }
      if(key_up_delay <= REPEAT_WAIT)
        mzx_world->key_up_delay = key_up_delay + 1;
    }
    else

    if((get_key_status(keycode_SDL, SDLK_DOWN)) &&
     (!src_board->player_ns_locked))
    {
      int key_down_delay = mzx_world->key_down_delay;
      if((key_down_delay == 0) || (key_down_delay > REPEAT_WAIT))
      {
        move_player(mzx_world, 1);
        src_board->player_last_dir = (src_board->player_last_dir & 0x0F) + 0x10;
      }
      if(key_down_delay <= REPEAT_WAIT)
        mzx_world->key_down_delay = key_down_delay + 1;
    }
    else

    if((get_key_status(keycode_SDL, SDLK_RIGHT)) &&
     (!src_board->player_ew_locked))
    {
      int key_right_delay = mzx_world->key_right_delay;
      if((key_right_delay == 0) || (key_right_delay > REPEAT_WAIT))
      {
        move_player(mzx_world, 2);
        src_board->player_last_dir = (src_board->player_last_dir & 0x0F) + 0x20;
      }
      if(key_right_delay <= REPEAT_WAIT)
        mzx_world->key_right_delay = key_right_delay + 1;
    }
    else

    if((get_key_status(keycode_SDL, SDLK_LEFT)) &&
     (!src_board->player_ew_locked))
    {
      int key_left_delay = mzx_world->key_left_delay;
      if((key_left_delay == 0) || (key_left_delay > REPEAT_WAIT))
      {
        move_player(mzx_world, 3);
        src_board->player_last_dir = (src_board->player_last_dir & 0x0F) + 0x30;
      }
      if(key_left_delay <= REPEAT_WAIT)
        mzx_world->key_left_delay = key_left_delay + 1;
    }
    else
    {
      mzx_world->key_up_delay = 0;
      mzx_world->key_down_delay = 0;
      mzx_world->key_right_delay = 0;
      mzx_world->key_left_delay = 0;
    }

    // Bomb
    if(get_key_status(keycode_SDL, SDLK_DELETE) &&
     (!src_board->player_attack_locked))
    {
      int d_offset = mzx_world->player_x + (mzx_world->player_y * board_width);
      if(level_under_id[d_offset] != 37)
      {
        // Okay.
        if(!src_board->can_bomb)
        {
          set_mesg(mzx_world, "Can't bomb here!");
        }
        else

        if((bomb_type == 0) && (!get_counter(mzx_world, "LOBOMBS", 0)))
        {
          set_mesg(mzx_world, "You are out of low strength bombs!");
          play_sfx(mzx_world, 32);
        }
        else

        if((bomb_type == 1) && (!get_counter(mzx_world, "HIBOMBS", 0)))
        {
          set_mesg(mzx_world, "You are out of high strength bombs!");
          play_sfx(mzx_world, 32);
        }
        else

        if(!(flags[level_under_id[d_offset]] & A_ENTRANCE))
        {
          // Bomb!
          mzx_world->under_player_id = level_under_id[d_offset];
          mzx_world->under_player_color = level_under_color[d_offset];
          mzx_world->under_player_param = level_under_param[d_offset];
          level_under_id[d_offset] = 37;
          level_under_color[d_offset] = 8;
          level_under_param[d_offset] = bomb_type << 7;
          play_sfx(mzx_world, 33 + bomb_type);

          if(bomb_type)
            dec_counter(mzx_world, "HIBOMBS", 1, 0);
          else
            dec_counter(mzx_world, "LOBOMBS", 1, 0);
        }
      }
    }
    if(reload) reload--;
  }

  mzx_world->swapped = 0;

  if((src_board->robot_list[0] != NULL) &&
   (src_board->robot_list[0])->used)
  {
    run_robot(mzx_world, 0, -1, -1);
    src_board = mzx_world->current_board;
    level_under_id = src_board->level_under_id;
    board_width = src_board->board_width;
  }

  if(!slowed)
  {
    int entrance = 1;
    int d_offset = mzx_world->player_x + (mzx_world->player_y * board_width);

    was_zapped = 0;
    if(flags[level_under_id[d_offset]] & A_ENTRANCE)
      entrance = 0;

    update_board(mzx_world);

    src_board = mzx_world->current_board;
    level_under_id = src_board->level_under_id;
    board_width = src_board->board_width;
    d_offset = mzx_world->player_x + (mzx_world->player_y * board_width);

    // Pushed onto an entrance?

    if((entrance) &&
     (flags[level_under_id[d_offset]] & A_ENTRANCE)
     && (!was_zapped))
    {
      int d_board = src_board->level_under_param[d_offset];
      clear_sfx_queue(); // Since there is often a push sound
      play_sfx(mzx_world, 37);

      // Same board or nonexistant?
      if((d_board != mzx_world->current_board_id)
       && (d_board < mzx_world->num_boards) && (mzx_world->board_list[d_board] != NULL))
      {
        // Nope.
        // Go to board d_board
        target_board = d_board;
        target_where = 1;
        target_x = level_under_color[d_offset];
        target_y = level_under_id[d_offset];
      }
    }

    was_zapped = 0;
  }

  // Death and game over
  if(get_counter(mzx_world, "LIVES", 0) == 0)
  {
    int endgame_board = mzx_world->endgame_board;
    int endgame_x = mzx_world->endgame_x;
    int endgame_y = mzx_world->endgame_y;

    // Game over
    if(endgame_board != NO_ENDGAME_BOARD)
    {
      // Jump to given board
      if(mzx_world->current_board_id == endgame_board)
      {
        id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
        id_place(mzx_world, endgame_x, endgame_y, 127, 0, 0);
        mzx_world->player_x = endgame_x;
        mzx_world->player_y = endgame_y;
      }
      else
      {
        target_board = endgame_board;
        target_where = 0;
        target_x = endgame_x;
        target_y = endgame_y;
      }
      // Clear "endgame" part
      endgame_board = NO_ENDGAME_BOARD;
      // Give one more life with minimal health
      set_counter(mzx_world, "LIVES", 1, 0);
      set_counter(mzx_world, "HEALTH", 1, 0);
    }
    else
    {
      set_mesg(mzx_world, "Game over");
      src_board->b_mesg_row = 24;
      src_board->b_mesg_col = -1;
      if(mzx_world->game_over_sfx)
        play_sfx(mzx_world, 24);
      dead = 1;
    }
  }
  else

  if(get_counter(mzx_world, "HEALTH", 0) == 0)
  {
    int death_board = mzx_world->death_board;

    // Death
    set_counter(mzx_world, "HEALTH", mzx_world->starting_health, 0);
    dec_counter(mzx_world, "Lives", 1, 0);
    set_mesg(mzx_world, "You have died...");
    clear_sfx_queue();
    play_sfx(mzx_world, 23);

    // Go somewhere else?
    if(death_board != DEATH_SAME_POS)
    {
      if(death_board == NO_DEATH_BOARD)
      {        
				int player_restart_x = mzx_world->player_restart_x;
        int player_restart_y = mzx_world->player_restart_y;
        // Return to entry x/y
        id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
        id_place(mzx_world, player_restart_x, player_restart_y, 127, 0, 0);
        mzx_world->player_x = player_restart_x;
        mzx_world->player_y = player_restart_y;
      }
      else
      {
        // Jump to given board
        if(mzx_world->current_board_id == death_board)
        {
          int death_x = mzx_world->death_x;
          int death_y = mzx_world->death_y;

          id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
          id_place(mzx_world, death_x, death_y, 127, 0, 0);
          mzx_world->player_x = death_x;
          mzx_world->player_y = death_y;
        }
        else
        {
          target_board = death_board;
          target_where = 0;
          target_x = mzx_world->death_x;
          target_y = mzx_world->death_y;
        }
      }
    }
  }

  if(target_where != -1)
  {
    int top_x, top_y;

    if(get_fade_status())
    {
      insta_fadeout();
    }

    // Draw border
    draw_viewport(mzx_world);

    //Draw screen
    if(!game)
    {
      *player_color = 0;
      player_char[0] = 32;
      player_char[1] = 32;
      player_char[2] = 32;
      player_char[3] = 32;
    }

    // Figure out x/y of top
    calculate_xytop(mzx_world, &top_x, &top_y);

    if(mzx_world->blind_dur > 0)
    {
      int i;
      int viewport_x = src_board->viewport_x;
      int viewport_y = src_board->viewport_y;
      int viewport_width = src_board->viewport_width;
      int viewport_height = src_board->viewport_height;
      int player_x = mzx_world->player_x;
      int player_y = mzx_world->player_y;

      for(i = viewport_y; i < viewport_y + viewport_height; i++)
      {
        fill_line(viewport_width, viewport_x, i, 176, 8);
      }

      // Find where player would be and draw.
      if(game)
      {
        id_put(src_board, player_x - top_x + viewport_x,
         player_y - top_y + viewport_y, player_x, player_y, player_x, player_y);
      }
    }
    else
    {
      draw_game_window(src_board, top_x, top_y);
    }

    // Add time limit
    time_remaining = get_counter(mzx_world, "TIME", 0);
    if(time_remaining)
    {
      int edge_color = mzx_world->edge_color;
      int seconds = time_remaining % 60;
      int timer_color;
      if(edge_color == 15)
        timer_color = 0xF0; // Prevent white on white for timer
      else
        timer_color = (edge_color << 4) + 15;

      sprintf(tmp_str, "%d:%02d", time_remaining / 60, time_remaining % 60);
      write_string(tmp_str, 1, 24, timer_color, 0);

      // Border with spaces
      draw_char(' ', edge_color, strlen(tmp_str) + 1, 24);
      draw_char(' ', edge_color, 0, 24);
    }

    // Add message
    if(src_board->b_mesg_timer > 0)
    {
      int mesg_x = src_board->b_mesg_col;
      int mesg_y = src_board->b_mesg_row;

      if(mesg_y > 24)
        mesg_y = 24;

      char *bottom_mesg = src_board->bottom_mesg;
      int mesg_length = strlencolor(bottom_mesg);
      int mesg_edges = mzx_world->mesg_edges;

      if(mesg_x == -1)
        mesg_x = 40 - (mesg_length / 2);

      color_string(bottom_mesg, mesg_x, mesg_y, scroll_color);

      if((mesg_x > 0) && (mesg_edges))
        draw_char(' ', scroll_color, mesg_x - 1, mesg_y);

      mesg_x += mesg_length;
      if((mesg_x < 80) && (mesg_edges))
        draw_char(' ', scroll_color, mesg_x, mesg_y);
    }

    // Add debug box

    if(debug_mode)
    {
      draw_debug_box(mzx_world, 60, 19, mzx_world->player_x,
       mzx_world->player_y);
    }

    // Add SPRITES! - Exo
    draw_sprites(mzx_world);
  }

  if(update_music)
  {
    load_mod(src_board->mod_playing);
    strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
  }

  update_music = 0;

  if(pal_update)
    update_palette();

  // Update screen
  update_screen();

  //Retrace/speed wait
  if(overall_speed > 1)
  {
    // Number of ms the update cycle took
    total_ticks = (16 * (overall_speed - 1)) - (get_ticks() - start_ticks);
    if(total_ticks < 0)
      total_ticks = 0;
    // Delay for 16 * (speed - 1) since the beginning of the update
    delay(total_ticks);
  }

  if(*fadein)
  {
    vquick_fadein();
    *fadein = 0;
  }

  if(mzx_world->swapped)
  {
    mzx_world->current_board_id = mzx_world->first_board;
    mzx_world->current_board =
    mzx_world->board_list[mzx_world->current_board_id];
    src_board = mzx_world->current_board;
    load_mod(src_board->mod_playing);
    strcpy(mzx_world->real_mod_playing, src_board->mod_playing);

    send_robot_def(mzx_world, 0, 10);
    send_robot_def(mzx_world, 0, 11);

    return 1;
  }

  if(target_board >= 0)
  {
    int faded = get_fade_status();
    int saved_player_last_dir = src_board->player_last_dir;
    int player_x = mzx_world->player_x;
    int player_y = mzx_world->player_y;

    // Aha.. TELEPORT or ENTRANCE.
    // Destroy message, bullets, spitfire?

    if(mzx_world->clear_on_exit)
    {
      int i;
      char d_id;
      src_board->b_mesg_timer = 0;
      for(i = 0; i < (board_width * board_height); i++)
      {
        d_id = level_id[i];
        if((d_id == 78) || (d_id == 61))
          offs_remove_id(mzx_world, 0);
      }
    }

    // Load board
    mzx_world->under_player_id = 0;
    mzx_world->under_player_param = 0;
    mzx_world->under_player_color = 7;

    mzx_world->current_board_id = target_board;
    mzx_world->current_board = mzx_world->board_list[target_board];
    src_board = mzx_world->current_board;

    if(strcasecmp(mzx_world->real_mod_playing, src_board->mod_playing) &&
     strcmp(src_board->mod_playing, "*"))
      update_music = 1;

    level_id = src_board->level_id;
    level_param = src_board->level_param;
    level_color = src_board->level_color;
    level_under_id = src_board->level_under_id;
    level_under_param = src_board->level_under_param;
    level_under_color = src_board->level_under_color;
    board_width = src_board->board_width;
    board_height = src_board->board_height;

    set_counter(mzx_world, "TIME", src_board->time_limit, 0);

    // Find target x/y
    if(target_where == 1)
    {
      int i;
      int tmp_x[5];
      int tmp_y[5];
      int x, y, offset;
      char d_id;

      // Entrance
      // First, set searched for id to the first in the whirlpool
      // series if it is part of the whirlpool series
      if((target_y > 67) && (target_y < 71))
        target_y = 67;

      // Now scan. Order- (type == target_y, color == target_x)
      // 1) Same type & color
      // 2) Same color
      // 3) Same type & foreground
      // 4) Same foreground
      // 5) Same type
      // Search for first of all 5 at once

      for(i = 0; i < 5; i++)
      {
        tmp_x[i] = -1; // None found
      }

      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          d_id = level_id[offset];

          if(d_id == 127)
          {
            // Remove the player, maybe readd
            player_x = x;
            player_y = y;
            id_remove_top(mzx_world, player_x, player_y);
            // Grab again - might have revealed an entrance
            d_id = level_id[offset];
          }

          if((d_id > 67) && (d_id < 71))
            d_id = 67;

          if(d_id == target_y)
          {
            // Same type
            // Color match?
            if(level_color[offset] == target_x)
            {
              // Yep
              tmp_x[0] = x;
              tmp_y[0] = y;
            }
            else

            // Fg?
            if((level_color[offset] & 0x0F) == (target_x & 0x0F))
            {
              // Yep
              tmp_x[2] = x;
              tmp_y[2] = y;
            }
            else
            {
              // Just same type
              tmp_x[4] = x;
              tmp_y[4] = y;
            }
          }
          else

          if(flags[d_id] & A_ENTRANCE)
          {
            // Not same type, but an entrance
            // Color match?
            if(level_color[offset] == target_x)
            {
              // Yep
              tmp_x[1] = x;
              tmp_y[1] = y;
            }
            // Fg?
            else

            if((level_color[offset] & 0x0F) == (target_x & 0x0F))
            {
              // Yep
              tmp_x[3] = x;
              tmp_y[3] = y;
            }
          }
          // Done with this x/y
        }
      }

      // We've got it... maybe.
      for(i = 0; i < 5; i++)
      {
        if(tmp_x[i] >= 0)
          break;
      }

      if(i < 5)
      {
        // Goto tmp_x[i], tmp_y[i]
        mzx_world->player_x = tmp_x[i];
        mzx_world->player_y = tmp_y[i];
        player_x = mzx_world->player_x;
        player_y = mzx_world->player_y;
      }
      // Else we stay at default player position
      id_place(mzx_world, player_x, player_y, 127, 0, 0);
      mzx_world->player_x = player_x;
      mzx_world->player_y = player_y;
    }
    else
    {
      // Specified x/y
      if(target_x < 0)
        target_x = 0;

      if(target_y < 0)
        target_y = 0;

      if(target_x >= board_width)
        target_x = board_width - 1;

      if(target_y >= board_height)
        target_y = board_height - 1;

      mzx_world->player_x = 0;
      mzx_world->player_y = 0;
      find_player(mzx_world);
      player_x = mzx_world->player_x;
      player_y = mzx_world->player_y;

      if((target_x != player_x) || (target_y != player_y))
      {
        int offset;
        offset = target_x + (target_y * board_width);

        if((level_id[offset] == 123) || (level_id[offset] == 124))
          clear_robot_id(src_board, level_param[offset]);

        if((level_id[offset] == 125) || (level_id[offset] == 126))
          clear_scroll_id(src_board, level_param[offset]);

        if(level_id[offset] == 122)
          step_sensor(mzx_world, level_param[offset]);

        // Place the player
        id_remove_top(mzx_world, player_x, player_y);
        id_place(mzx_world, target_x, target_y, 127, 0, 0);
        mzx_world->player_x = target_x;
        mzx_world->player_y = target_y;
      }
    }

    send_robot_def(mzx_world, 0, 11);
    find_player(mzx_world);
    mzx_world->player_restart_x = mzx_world->player_x;
    mzx_world->player_restart_y = mzx_world->player_y;
    // Now... Set player_last_dir for direction FACED
    src_board->player_last_dir = (src_board->player_last_dir & 0x0F) |
     (saved_player_last_dir & 0xF0);

    // ...and if player ended up on ICE, set last dir pressed as well
    if(level_under_id[player_x + (player_y * board_width)] == 25)
      src_board->player_last_dir = saved_player_last_dir;

    // Fix palette
    if(target_where >= 0)
    {
      // Prepare for fadein
      if(!get_fade_status())
        *fadein = 1;
      vquick_fadeout();
    }

    target_x = -1;
    target_y = -1;
    target_board = -1;
    target_where = -2;

    // Disallow any keypresses this cycle

    return 1;
  }

  return 0;
}

void find_player(World *mzx_world)
{
  Board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;;
  int board_height = src_board->board_height;
  int board_size = board_width * board_height;
  char *level_id = src_board->level_id;
  int offset;

  if(level_id[mzx_world->player_x +
   (mzx_world->player_y * board_width)] != 127)
  {
    for(offset = 0; offset < board_size; offset++)
    {
      if(level_id[offset] == 127)
        break;
    }

    if(offset == board_size)
    {
      int d_id = level_id[0];
      mzx_world->player_x = 0;
      mzx_world->player_y = 0;

      if((d_id == 123) || (d_id == 124)) // (robot)
      {
        clear_robot_id(src_board, src_board->level_param[0]);
      }
      else

      if((d_id == 125) || (d_id == 126)) // (scroll)
      {
        clear_scroll_id(src_board, src_board->level_param[0]);
      }

      // Place.
      id_place(mzx_world, 0, 0, 127, 0, 0);
    }
    else
    {
      mzx_world->player_x = offset % board_width;
      mzx_world->player_y = offset / board_width;
    }
  }
}

int take_key(World *mzx_world, int color)
{
  int i;
  char *keys = mzx_world->keys;

  color &= 15;

  for(i = 0; i < NUM_KEYS; i++)
  {
    if(keys[i] == color) break;
  }

  if(i < NUM_KEYS)
  {
    keys[i] = NO_KEY;
    return 0;
  }

  return 1;
}

// Give a key. Returns non-0 if no room.
int give_key(World *mzx_world, int color)
{
  int i;
  char *keys = mzx_world->keys;

  color &= 15;

  for(i = 0; i < NUM_KEYS; i++)
    if(keys[i] == NO_KEY) break;

  if(i < NUM_KEYS)
  {
    keys[i] = color;
    return 0;
  }

  return 1;
}

void draw_debug_box(World *mzx_world, int x, int y, int d_x, int d_y)
{
  Board *src_board = mzx_world->current_board;
  draw_window_box(x, y, x + 19, y + 5, EC_DEBUG_BOX, EC_DEBUG_BOX_DARK,
   EC_DEBUG_BOX_CORNER, 0, 1);

  write_string
  (
    "X/Y:        /\n"
    "Board:\n",
    x + 1, y + 1, EC_DEBUG_LABEL, 0
  );

  write_number(d_x, EC_DEBUG_NUMBER, x + 8, y + 1, 5);
  write_number(d_y, EC_DEBUG_NUMBER, x + 14, y + 1, 5);
  write_number(mzx_world->current_board_id, EC_DEBUG_NUMBER, x + 18, y + 2, 0, 1);

  if(*(src_board->mod_playing) != 0)
  {
    if(strlen(src_board->mod_playing) > 12)
    {
      char tempc = src_board->mod_playing[18];
      src_board->mod_playing[18] = 0;
      write_string(src_board->mod_playing, x + 1, y + 4, EC_DEBUG_NUMBER, 0);
      src_board->mod_playing[18] = tempc;
    }
    else
    {
      write_string(src_board->mod_playing, x + 1, y + 4, EC_DEBUG_NUMBER, 0);
    }
  }
  else
  {
    write_string("(no module)", x + 2, y + 4, EC_DEBUG_NUMBER, 0);
  }
}

