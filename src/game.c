/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
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

// Main title screen/gaming code

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "configure.h"
#include "const.h"
#include "core.h"
#include "counter.h"
#include "data.h"
#include "error.h"
#include "event.h"
#include "fsafeopen.h"
#include "game.h"
#include "game_ops.h"
#include "graphics.h"
#include "helpsys.h"
#include "platform.h"
#include "util.h"
#include "window.h"
#include "world.h"
#include "world_struct.h"

#include "audio/audio.h"
#include "audio/sfx.h"

static const char *const world_ext[] = { ".MZX", NULL };
static const char *const save_ext[] = { ".SAV", NULL };

boolean load_game_module(struct world *mzx_world, char *filename,
 boolean fail_if_same)
{
  struct board *src_board = mzx_world->current_board;
  char translated_name[MAX_PATH];
  size_t mod_name_size = strlen(filename);
  boolean mod_star = false;
  int n_result;

  // Special case: mod "" ends the module.
  if(!filename[0])
  {
    mzx_world->real_mod_playing[0] = 0;
    audio_end_module();
    return false;
  }

  // Do nothing.
  if(!strcmp(filename, "*"))
    return false;

  // Temporarily get rid of the star.
  if(mod_name_size && filename[mod_name_size - 1] == '*')
  {
    filename[mod_name_size - 1] = 0;
    mod_star = true;
  }

  // Get the translated name (the one we want to compare against later)
  n_result = fsafetranslate(filename, translated_name);

  // Add * back
  if(mod_star)
    filename[mod_name_size - 1] = '*';

  if(n_result == FSAFE_SUCCESS)
  {
    // In certain situations, play the mod only if the names are different.
    if((mod_star || fail_if_same) &&
     !strcasecmp(translated_name, mzx_world->real_mod_playing))
      return false;

    // If the mod actually changes, update real mod playing.
    // The safety check has already been done, so don't do it again ('false').
    if(audio_play_module(translated_name, false, src_board->volume))
    {
      strcpy(mzx_world->real_mod_playing, translated_name);
      return true;
    }
  }
  return false;
}

void load_board_module(struct world *mzx_world)
{
  load_game_module(mzx_world, mzx_world->current_board->mod_playing, false);
}

static void load_world_file(struct world *mzx_world, char *name)
{
  struct board *src_board;
  int fade = 0;

  // Load world
  audio_end_module();
  sfx_clear_queue();
  //Clear screen
  clear_screen(32, 7);
  // Palette
  default_palette();
  if(reload_world(mzx_world, name, &fade))
  {
    // Load was successful, so set curr_file
    if(curr_file != name)
      strcpy(curr_file, name);

    set_caption(mzx_world, NULL, NULL, 0);

    send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

    src_board = mzx_world->current_board;
    load_board_module(mzx_world);
    set_counter(mzx_world, "TIME", src_board->time_limit, 0);
    set_intro_mesg_timer(MESG_TIMEOUT);
  }
  else
  {
    // Restart the music if we still have a world in memory.
    if(mzx_world->current_board)
      load_board_module(mzx_world);
  }
}

static void load_world_selection(struct world *mzx_world)
{
  char world_name[MAX_PATH] = { 0 };

  m_show();
  if(!choose_file_ch(mzx_world, world_ext,
   world_name, "Load World", 1))
  {
    load_world_file(mzx_world, world_name);
  }
}

__editor_maybe_static void play_game(struct world *mzx_world)
{
  // We have the world loaded, on the proper scene.
  // We are faded out. Commence playing!
  int exit = 0;
  int confirm_exit = 0;
  int key = -1;
  int key_status = 0;
  char keylbl[5] = "KEY?";
  struct board *src_board;
  int fadein = 1;
  struct config_info *conf = &mzx_world->conf;

  // Main game loop
  // Mouse remains hidden unless menu/etc. is invoked

  set_context(CTX_PLAY_GAME);

  if(!edit_world)
    mzx_world->editing = false;

  do
  {
    // Update
    if(update(mzx_world, 1, &fadein))
    {
      // Exit game state change
      if(mzx_world->change_game_state == CHANGE_STATE_EXIT_GAME_ROBOTIC ||
       mzx_world->change_game_state == CHANGE_STATE_REQUEST_EXIT)
      {
        if(conf->no_titlescreen)
        {
          mzx_world->full_exit = true;
        }
        break;
      }

      update_event_status();
      continue;
    }
    update_event_status();

    src_board = mzx_world->current_board;

    // Has the game/world been saved from robotic?
    if(mzx_world->robotic_save_type == SAVE_GAME)
    {
      save_world(mzx_world, mzx_world->robotic_save_path, 1, MZX_VERSION);
      mzx_world->robotic_save_type = SAVE_NONE;
    }

    // Keycheck

    key = get_key(keycode_internal_wrt_numlock);
    key_status = get_key_status(keycode_internal_wrt_numlock, key);

    exit = get_exit_status();

    if(key && !exit)
    {
#ifdef CONFIG_PANDORA
      // The Pandora (at least, at one point) did not have proper support for
      // keycode_unicode. This is a workaround to get the "keyN" labels to work
      // at all. This will break certain keyN labels, such as "key$", however.
      int key_char = key;
#else
      int key_char = get_key(keycode_unicode);
#endif

      if(key_char)
      {
        keylbl[3] = key_char;
        send_robot_all_def(mzx_world, keylbl);
        // In pre-port MZX versions key was a board counter
        if(mzx_world->version < VERSION_PORT)
        {
          char keych = toupper(key_char);
          // <2.60 it only supported 1-9 and A-Z
          // This is difficult to version check, so apply it to <2.62
          if(mzx_world->version >= V262 ||
           (keych >= 'A' && keych <= 'Z') ||
           (keych >= '1' && keych <= '9'))
          {
            src_board->last_key = keych;
          }
        }
      }

      switch(key)
      {
#ifdef CONFIG_HELPSYS
        case IKEY_F1:
        {
          if(mzx_world->version < V260 ||
           get_counter(mzx_world, "HELP_MENU", 0))
          {
            m_show();
            help_system(mzx_world);

            update_event_status();
          }
          break;
        }
#endif
        case IKEY_F2:
        {
          // Settings
          if(mzx_world->version < V260 ||
           get_counter(mzx_world, "F2_MENU", 0) ||
           !mzx_world->active)
          {
            m_show();

            game_settings(mzx_world);

            update_event_status();
          }
          break;
        }

        case IKEY_F3:
        {
          // Save game
          if(!mzx_world->dead)
          {
            // Can we?
            if((src_board->save_mode != CANT_SAVE) &&
             ((src_board->save_mode != CAN_SAVE_ON_SENSOR) ||
             (src_board->level_under_id[mzx_world->player_x +
             (src_board->board_width * mzx_world->player_y)] ==
             SENSOR)))
            {
              char save_game[MAX_PATH];

              strcpy(save_game, curr_sav);

              m_show();
              if(!new_file(mzx_world, save_ext, ".sav", save_game,
               "Save game", 1))
              {
                strcpy(curr_sav, save_game);
                // Save entire game
                save_world(mzx_world, curr_sav, 1, MZX_VERSION);
              }

              update_event_status();
            }
          }
          break;
        }

        case IKEY_F4:
        {
          // ALT+F4 - do nothing.
          if(get_alt_status(keycode_internal))
            break;

          if(mzx_world->version < V282 ||
           get_counter(mzx_world, "LOAD_MENU", 0))
          {
            // Restore
            char save_file_name[MAX_PATH] = { 0 };
            m_show();

            if(!choose_file_ch(mzx_world, save_ext, save_file_name,
             "Choose game to restore", 1))
            {
              // Load game
              fadein = 0;
              if(!reload_savegame(mzx_world, save_file_name, &fadein))
                break;

              // Reset this
              src_board = mzx_world->current_board;
              load_game_module(mzx_world, mzx_world->real_mod_playing, false);

              find_player(mzx_world);

              strcpy(curr_sav, save_file_name);
              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
              fadein ^= 1;
            }

            update_event_status();
          }
          break;
        }

        case IKEY_F5:
        case IKEY_INSERT:
        {
          // Change bomb type
          if(!mzx_world->dead)
          {
            mzx_world->bomb_type ^= 1;
            if((!src_board->player_attack_locked) &&
             (src_board->can_bomb))
            {
              play_sfx(mzx_world, 35);
              if(mzx_world->bomb_type)
              {
                set_mesg(mzx_world,
                 "You switch to high strength bombs.");
              }
              else
              {
                set_mesg(mzx_world,
                 "You switch to low strength bombs.");
              }
            }
          }
          break;
        }

        // Toggle debug mode
        case IKEY_F6:
        {
          if(edit_world && mzx_world->editing)
            mzx_world->debug_mode = !(mzx_world->debug_mode);
          break;
        }

        // Cheat
        case IKEY_F7:
        {
          if(mzx_world->editing)
          {
            int i;

            // Cheat #1- Give all
            set_counter(mzx_world, "AMMO", 32767, 1);
            set_counter(mzx_world, "COINS", 32767, 1);
            set_counter(mzx_world, "GEMS", 32767, 1);
            set_counter(mzx_world, "HEALTH", 32767, 1);
            set_counter(mzx_world, "HIBOMBS", 32767, 1);
            set_counter(mzx_world, "LIVES", 32767, 1);
            set_counter(mzx_world, "LOBOMBS", 32767, 1);
            set_counter(mzx_world, "SCORE", 0, 1);
            set_counter(mzx_world, "TIME", src_board->time_limit, 1);

            mzx_world->dead = 0;

            for(i = 0; i < 16; i++)
            {
              mzx_world->keys[i] = i;
            }

            src_board->player_ns_locked = 0;
            src_board->player_ew_locked = 0;
            src_board->player_attack_locked = 0;
          }

          break;
        }

        // Cheat More
        case IKEY_F8:
        {
          if(mzx_world->editing)
          {
            int player_x = mzx_world->player_x;
            int player_y = mzx_world->player_y;
            int board_width = src_board->board_width;
            int board_height = src_board->board_height;

            if(player_x > 0)
            {
              place_at_xy(mzx_world, SPACE, 7, 0, player_x - 1,
               player_y);

              if(player_y > 0)
              {
                place_at_xy(mzx_world, SPACE, 7, 0, player_x - 1,
                 player_y - 1);
              }

              if(player_y < (board_height - 1))
              {
                place_at_xy(mzx_world, SPACE, 7, 0, player_x - 1,
                 player_y + 1);
              }
            }

            if(player_x < (board_width - 1))
            {
              place_at_xy(mzx_world, SPACE, 7, 0, player_x + 1,
               player_y);

              if(player_y > 0)
              {
                place_at_xy(mzx_world, SPACE, 7, 0,
                 player_x + 1, player_y - 1);
              }

              if(player_y < (board_height - 1))
              {
                place_at_xy(mzx_world, SPACE, 7, 0,
                 player_x + 1, player_y + 1);
              }
            }

            if(player_y > 0)
            {
              place_at_xy(mzx_world, SPACE, 7, 0,
               player_x, player_y - 1);
            }

            if(player_y < (board_height - 1))
            {
              place_at_xy(mzx_world, SPACE, 7, 0,
               player_x, player_y + 1);
            }
          }

          break;
        }

        // Quick save
        case IKEY_F9:
        {
          if(!mzx_world->dead)
          {
            // Can we?
            if((src_board->save_mode != CANT_SAVE) &&
             ((src_board->save_mode != CAN_SAVE_ON_SENSOR) ||
             (src_board->level_under_id[mzx_world->player_x +
             (src_board->board_width * mzx_world->player_y)] ==
             SENSOR)))
            {
              // Save entire game
              save_world(mzx_world, curr_sav, 1, MZX_VERSION);
            }
          }
          break;
        }

        // Quick load
        case IKEY_F10:
        {
          if(mzx_world->version < V282 ||
           get_counter(mzx_world, "LOAD_MENU", 0))
          {
            struct stat file_info;

            if(!stat(curr_sav, &file_info))
            {
              // Load game
              fadein = 0;
              if(!reload_savegame(mzx_world, curr_sav, &fadein))
                break;

              // Reset this
              src_board = mzx_world->current_board;
              load_game_module(mzx_world, mzx_world->real_mod_playing, false);

              find_player(mzx_world);

              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
              fadein ^= 1;
            }
          }
          break;
        }

        case IKEY_F11:
        {
          // Breakpoint editor
          if(get_alt_status(keycode_internal))
          {
            if(debug_robot_config && mzx_world->editing)
            {
              debug_robot_config(mzx_world);

              update_event_status();
            }
          }
          // Debug counter editor
          else
          {
            if(debug_counters && mzx_world->editing)
            {
              debug_counters(mzx_world);

              update_event_status();
            }
          }
          break;
        }

        case IKEY_RETURN:
        {
          int enter_menu_status =
           get_counter(mzx_world, "ENTER_MENU", 0);
          send_robot_all_def(mzx_world, "KeyEnter");

          // Ignore if this isn't a fresh press
          if(key_status != 1)
            break;

          if(mzx_world->version < V260 || enter_menu_status)
            game_menu(mzx_world);

          break;
        }

        case IKEY_ESCAPE:
        {
          // ESCAPE_MENU (2.90+)
          int escape_menu_status =
           get_counter(mzx_world, "ESCAPE_MENU", 0);

          if(mzx_world->version < V290 || escape_menu_status)
          {
            if(key_status == 1)
            {
              confirm_exit = 1;
              exit = 1;
            }
          }

          break;
        }
      }
    }

    // Quit
    if(exit)
    {
      // Special behaviour in standalone- only escape exits
      // ask for confirmation
      if(conf->no_titlescreen || (conf->standalone_mode && !confirm_exit))
      {
        mzx_world->full_exit = true;
      }

      if(confirm_exit || !conf->standalone_mode)
      {
        confirm_exit = 0;
        m_show();

        exit = !confirm(mzx_world, "Quit playing- Are you sure?");

        update_event_status();
      }
    }

  } while(!exit);
  pop_context();
  vquick_fadeout();
  sfx_clear_queue();
}

void title_screen(struct world *mzx_world)
{
  int exit = 0;
  int confirm_exit = 0;
  int fadein = 1;
  int key = 0;
  int key_status = 0;
  int fade;
  struct stat file_info;
  struct board *src_board;
  struct config_info *conf = &mzx_world->conf;

  set_caption(mzx_world, NULL, NULL, 0);
  mzx_world->debug_mode = false;

  // Clear screen
  clear_screen(32, 7);
  default_palette();

  // First, disable standalone mode if this is a build of MZX
  // with the editor enabled
  if(edit_world)
  {
    conf->standalone_mode = 0;
  }

  if(edit_world && mzx_world->conf.startup_editor)
  {
    // Disable standalone mode
    conf->standalone_mode = 0;

    set_intro_mesg_timer(0);
    edit_world(mzx_world, 0);
  }
  else
  {
    if(!stat(curr_file, &file_info))
      load_world_file(mzx_world, curr_file);
    else
    {
      // Disable standalone mode
      conf->standalone_mode = 0;

      load_world_selection(mzx_world);
    }
  }

  // if standalone mode is disabled, also disable no_titlescreen
  if(!conf->standalone_mode)
    conf->no_titlescreen = 0;

  src_board = mzx_world->current_board;
  draw_intro_mesg(mzx_world);

  // Main game loop

  do
  {
    if(!conf->no_titlescreen)
    {
      // Focus on center
      if(fadein)
      {
        int x, y;
        set_screen_coords(640/2, 350/2, &x, &y);
        focus_pixel(x, y);
      }

      // Update
      if(mzx_world->active)
      {
        if(update(mzx_world, 0, &fadein))
        {
          if(conf->standalone_mode &&
           mzx_world->change_game_state == CHANGE_STATE_EXIT_GAME_ROBOTIC)
            break;

          if(mzx_world->change_game_state == CHANGE_STATE_REQUEST_EXIT)
            break;

          update_event_status();
          continue;
        }
      }
      else
      {
        // Give some delay time if nothing's loaded
        update_event_status_delay();
        update_screen();
      }

      src_board = mzx_world->current_board;

      update_event_status();

      // Keycheck
      key = get_key(keycode_internal_wrt_numlock);
      key_status = get_key_status(keycode_internal_wrt_numlock, key);

      exit = get_exit_status();
      if(conf->standalone_mode)
      {
        switch(mzx_world->change_game_state)
        {
          case CHANGE_STATE_PLAY_GAME_ROBOTIC:
            key = IKEY_p;
            break;

          default:
            break;
        }
      }
    }
    else
    {
      // No titlescreen mode, so jump straight to the game
      key = IKEY_p;
    }

    if(key && !exit)
    {
      int reload_curr_world_in_editor = 1;
      switch(key)
      {
#ifdef CONFIG_HELPSYS
        case IKEY_F1:
        case IKEY_h:
        {
          int help_menu_status =
           get_counter(mzx_world, "HELP_MENU", 0);
          if(conf->standalone_mode && !help_menu_status)
            break;
          m_show();
          help_system(mzx_world);
          update_screen();
          break;
        }
#endif
        case IKEY_F2:
        case IKEY_s:
        {
          int f2_menu_status =
           get_counter(mzx_world, "F2_MENU", 0);
          if(conf->standalone_mode && !f2_menu_status)
            break;
          // Settings
          m_show();

          game_settings(mzx_world);

          update_screen();
          update_event_status();
          break;
        }

        case IKEY_F3:
        case IKEY_l:
        {
          if(conf->standalone_mode)
            break;

          load_world_selection(mzx_world);
          fadein = 1;
          src_board = mzx_world->current_board;
          update_screen();
          update_event_status();
          break;
        }

        case IKEY_F4:
        {
          // ALT+F4 - do nothing.
          if(get_alt_status(keycode_internal))
            break;
        }

        /* fallthrough */

        case IKEY_r:
        {
          char save_file_name[MAX_PATH] = { 0 };
          int load_menu_status =
           get_counter(mzx_world, "LOAD_MENU", 0);
          if(conf->standalone_mode && !load_menu_status)
            break;

          // Restore
          m_show();

          if(!choose_file_ch(mzx_world, save_ext, save_file_name,
           "Choose game to restore", 1))
          {
            // Swap out current board...
            sfx_clear_queue();
            // Load game
            fadein = 0;

            if(reload_savegame(mzx_world, save_file_name, &fadein))
            {
              src_board = mzx_world->current_board;
              load_game_module(mzx_world, mzx_world->real_mod_playing, false);
              set_intro_mesg_timer(0);

              // Copy filename
              strcpy(curr_sav, save_file_name);
              fadein ^= 1;

              // do not send JUSTENTERED when loading a SAV game from the
              // title screen or when no game is loaded; here we ONLY send
              // JUSTLOADED.
              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

              set_counter(mzx_world, "TIME", src_board->time_limit, 0);

              find_player(mzx_world);
              mzx_world->player_restart_x = mzx_world->player_x;
              mzx_world->player_restart_y = mzx_world->player_y;
              vquick_fadeout();

              update_event_status();
              play_game(mzx_world);

              if(mzx_world->full_exit)
                break;

              // Done playing- load world again
              // Already faded out from play_game()
              audio_end_module();
              // Clear screen
              clear_screen(32, 7);
              // Palette
              default_palette();
              insta_fadein();
              // Reload original file
              if(!stat(curr_file, &file_info))
              {
                if(reload_world(mzx_world, curr_file, &fade))
                {
                  src_board = mzx_world->current_board;
                  load_board_module(mzx_world);
                  set_counter(mzx_world, "TIME",
                   src_board->time_limit, 0);
                }
              }
              else
              {
                clear_world(mzx_world);
                clear_global_data(mzx_world);
              }
              vquick_fadeout();
              fadein = 1;
            }
            break;
          }

          update_screen();
          update_event_status();
          break;
        }

        case IKEY_F5:
        case IKEY_p:
        {
          if(mzx_world->active)
          {
            char old_mod_playing[MAX_PATH];
            strcpy(old_mod_playing, mzx_world->real_mod_playing);

            // Play
            // Only from swap?

            if(mzx_world->only_from_swap)
            {
              m_show();
              error("You can only play this game via a swap"
               " from another game", 0, 24, 0x3101);
              break;
            }

            // Load world curr_file
            // Don't end mod- We want a smooth transition for that.
            // Clear screen

            clear_screen(32, 7);
            // Palette
            default_palette();

            if(reload_world(mzx_world, curr_file, &fade))
            {
              if(mzx_world->current_board_id != mzx_world->first_board)
              {
                change_board(mzx_world, mzx_world->first_board);
              }

              src_board = mzx_world->current_board;

              // send both JUSTENTERED and JUSTLOADED respectively; the
              // JUSTLOADED label will take priority if a robot defines it
              send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);
              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

              // Load the mod unless it's the mod from the title screen or *.
              strcpy(mzx_world->real_mod_playing, old_mod_playing);
              load_game_module(mzx_world, src_board->mod_playing, true);

              set_intro_mesg_timer(0);

              set_counter(mzx_world, "TIME", src_board->time_limit, 0);

              sfx_clear_queue();
              find_player(mzx_world);
              mzx_world->player_restart_x = mzx_world->player_x;
              mzx_world->player_restart_y = mzx_world->player_y;
              vquick_fadeout();

              // Load board palette and charset
              change_board_load_assets(mzx_world);

              play_game(mzx_world);

              if(mzx_world->full_exit)
                break;

              // Done playing- load world again
              // Already faded out from play_game()
              audio_end_module();
              // Clear screen
              clear_screen(32, 7);
              // Palette
              default_palette();
              insta_fadein();
              // Reload original file
              if(reload_world(mzx_world, curr_file, &fade))
              {
                src_board = mzx_world->current_board;
                load_board_module(mzx_world);
                set_counter(mzx_world, "TIME", src_board->time_limit, 0);
              }
              // Whoops, something happened! Make a blank world instead
              else
              {
                clear_world(mzx_world);
                clear_global_data(mzx_world);
              }
              vquick_fadeout();
              fadein = 1;
            }
            else
            {
              audio_end_module();
              clear_screen(32, 7);
            }
          }
          break;
        }

        case IKEY_F7:
        case IKEY_u:
        {
          if(check_for_updates)
          {
            int current_music_vol = audio_get_music_volume();
            int current_pcs_vol = audio_get_pcs_volume();
            audio_set_music_volume(0);
            audio_set_pcs_volume(0);
            if(mzx_world->active)
              audio_set_module_volume(0);

            check_for_updates(mzx_world, &mzx_world->conf, 0);
            set_caption(mzx_world, NULL, NULL, 0);

            audio_set_pcs_volume(current_pcs_vol);
            audio_set_music_volume(current_music_vol);
            if(mzx_world->active)
              audio_set_module_volume(src_board->volume);
          }
          break;
        }

        case IKEY_F8:
        case IKEY_n:
          reload_curr_world_in_editor = 0;

          /* fallthrough */

        case IKEY_F9:
        case IKEY_e:
        {
          if(edit_world)
          {
            // Editor
            sfx_clear_queue();
            vquick_fadeout();
            set_intro_mesg_timer(0);
            edit_world(mzx_world, reload_curr_world_in_editor);

            if(curr_file[0])
              load_world_file(mzx_world, curr_file);

            fadein = 1;
          }
          break;
        }

        // Quickload
        case IKEY_F10:
        {
          int load_menu_status =
           get_counter(mzx_world, "LOAD_MENU", 0);
          if(conf->standalone_mode && !load_menu_status)
            break;

          // Restore
          m_show();

          // Swap out current board...
          sfx_clear_queue();
          // Load game
          fadein = 0;

          if(!reload_savegame(mzx_world, curr_sav, &fadein))
          {
            vquick_fadeout();
          }
          else
          {
            src_board = mzx_world->current_board;
            load_game_module(mzx_world, mzx_world->real_mod_playing, false);
            set_intro_mesg_timer(0);

            fadein ^= 1;

            // do not send JUSTENTERED when loading a SAV game from the
            // title screen or when no game is loaded; here we ONLY send
            // JUSTLOADED.
            send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

            set_counter(mzx_world, "TIME", src_board->time_limit, 0);

            find_player(mzx_world);
            mzx_world->player_restart_x = mzx_world->player_x;
            mzx_world->player_restart_y = mzx_world->player_y;
            vquick_fadeout();

            play_game(mzx_world);

            if(mzx_world->full_exit)
              break;

            // Done playing- load world again
            // Already faded out from play_game()
            audio_end_module();
            // Clear screen
            clear_screen(32, 7);
            // Palette
            default_palette();
            insta_fadein();
            // Reload original file
            if(!stat(curr_file, &file_info))
            {
              if(reload_world(mzx_world, curr_file, &fade))
              {
                src_board = mzx_world->current_board;
                load_board_module(mzx_world);
                set_counter(mzx_world, "TIME",
                 src_board->time_limit, 0);
              }
            }
            else
            {
              clear_world(mzx_world);
              clear_global_data(mzx_world);
            }
            vquick_fadeout();
            fadein = 1;
          }

          update_screen();
          update_event_status();

          break;
        }

        case IKEY_RETURN: // Enter
        {

          int enter_menu_status =
           get_counter(mzx_world, "ENTER_MENU", 0);

          // Ignore if this isn't a fresh press
          if(key_status != 1)
            break;

          if(conf->standalone_mode && !enter_menu_status)
            break;


          break;
        }

        case IKEY_ESCAPE:
        {
          // ESCAPE_MENU (2.90+)
          int escape_menu_status =
           get_counter(mzx_world, "ESCAPE_MENU", 0);

          // Escape menu only works on the title screen if the
          // standalone_mode config option is set
          if(mzx_world->version < V290 || escape_menu_status ||
           !conf->standalone_mode)
          {
            if(key_status == 1)
            {
              confirm_exit = 1;
              exit = 1;
            }
          }

          break;
        }
      }
      draw_intro_mesg(mzx_world);
    }

    // Quit
    if(exit)
    {
      // Special behaviour in standalone- only escape exits
      // ask for confirmation
      if(confirm_exit || !conf->standalone_mode)
      {
        confirm_exit = 0;
        m_show();

        exit = !confirm(mzx_world, "Exit MegaZeux - Are you sure?");

        update_screen();
        update_event_status();
      }
    }

    if(mzx_world->full_exit)
      break;

  } while(!exit && !mzx_world->full_exit);

  vquick_fadeout();
  sfx_clear_queue();
}
