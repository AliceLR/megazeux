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

static boolean load_world_title(struct world *mzx_world, char *name)
{
  boolean ignore;

  if(reload_world(mzx_world, name, &ignore))
  {
    // Load was successful, so set curr_file
    if(curr_file != name)
      strcpy(curr_file, name);

    set_caption(mzx_world, NULL, NULL, false);

    // Only send JUSTLOADED on the title screen.
    send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

    load_board_module(mzx_world);
    sfx_clear_queue();

    set_counter(mzx_world, "TIME", mzx_world->current_board->time_limit, 0);
    set_intro_mesg_timer(MESG_TIMEOUT);
    return true;
  }
  else

  if(mzx_world->active)
  {
    clear_world(mzx_world);
    clear_global_data(mzx_world);
  }

  return false;
}

static void load_world_selection(struct world *mzx_world)
{
  char world_name[MAX_PATH] = { 0 };

  m_show();
  if(!choose_file_ch(mzx_world, world_ext,
   world_name, "Load World", 1))
  {
    vquick_fadeout();
    load_world_title(mzx_world, world_name);
  }
}

__editor_maybe_static void play_game(struct world *mzx_world)
{
  // We have the world loaded, on the proper scene.
  // We are faded out. Commence playing!
  boolean fadein = true;
  int exit = 0;
  int confirm_exit = 0;
  int key = -1;
  int key_status = 0;
  char keylbl[5] = "KEY?";
  struct board *src_board;
  struct config_info *conf = &mzx_world->conf;

  // Main game loop
  // Mouse remains hidden unless menu/etc. is invoked

  set_context(CTX_PLAY_GAME);

  if(!edit_world)
    mzx_world->editing = false;

  do
  {
    // Update
    if(update(mzx_world, false, &fadein))
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
      save_world(mzx_world, mzx_world->robotic_save_path, true, MZX_VERSION);
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
          }
          break;
        }

        case IKEY_F3:
        {
          // Save game
          if(!mzx_world->dead)
          {
            // Can we?
            if(player_can_save(mzx_world))
            {
              char save_game[MAX_PATH];
              strcpy(save_game, curr_sav);

              m_show();
              if(!new_file(mzx_world, save_ext, ".sav", save_game,
               "Save game", 1))
              {
                strcpy(curr_sav, save_game);
                save_world(mzx_world, curr_sav, true, MZX_VERSION);
              }
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
            // Restore saved game
            char save_file_name[MAX_PATH] = { 0 };
            m_show();

            if(!choose_file_ch(mzx_world, save_ext, save_file_name,
             "Choose game to restore", 1))
            {
              boolean faded = false;
              fadein = false;

              if(!reload_savegame(mzx_world, save_file_name, &faded))
                break;

              load_game_module(mzx_world, mzx_world->real_mod_playing, false);

              find_player(mzx_world);

              strcpy(curr_sav, save_file_name);
              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
              if(!faded)
                fadein = true;
            }
          }
          break;
        }

        case IKEY_F5:
        case IKEY_INSERT:
        {
          // Change bomb type
          if(!mzx_world->dead)
            player_switch_bomb_type(mzx_world);

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
            player_cheat_give_all(mzx_world);

          break;
        }

        // Cheat More
        case IKEY_F8:
        {
          if(mzx_world->editing)
            player_cheat_zap(mzx_world);

          break;
        }

        // Quick save
        case IKEY_F9:
        {
          if(!mzx_world->dead)
          {
            // Can we?
            if(player_can_save(mzx_world))
              save_world(mzx_world, curr_sav, true, MZX_VERSION);
          }
          break;
        }

        // Quick load saved game
        case IKEY_F10:
        {
          if(mzx_world->version < V282 ||
           get_counter(mzx_world, "LOAD_MENU", 0))
          {
            struct stat file_info;

            if(!stat(curr_sav, &file_info))
            {
              boolean faded = false;
              fadein = false;

              if(!reload_savegame(mzx_world, curr_sav, &faded))
                break;

              load_game_module(mzx_world, mzx_world->real_mod_playing, false);

              find_player(mzx_world);

              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
              if(!faded)
                fadein = true;
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
            }
          }
          // Debug counter editor
          else
          {
            if(debug_counters && mzx_world->editing)
            {
              debug_counters(mzx_world);
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
      }
    }

  } while(!exit);
  pop_context();
  vquick_fadeout();
  default_palette();
  clear_screen();
  audio_end_module();
  sfx_clear_queue();
}

void title_screen(struct world *mzx_world)
{
  boolean fadein = true;
  boolean ignore;
  int exit = 0;
  int confirm_exit = 0;
  int key = 0;
  int key_status = 0;
  struct board *src_board;
  struct config_info *conf = &mzx_world->conf;

  set_caption(mzx_world, NULL, NULL, false);
  mzx_world->debug_mode = false;

  // Clear screen
  default_palette();
  clear_screen();

  // First, disable standalone mode if this is a build of MZX
  // with the editor enabled
  if(edit_world)
  {
    conf->standalone_mode = 0;
  }

  if(edit_world && mzx_world->conf.startup_editor)
  {
    set_intro_mesg_timer(0);
    edit_world(mzx_world, true);
  }
  else
  {
    if(!load_world_title(mzx_world, curr_file))
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
        if(update(mzx_world, true, &fadein))
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
      boolean reload_curr_world_in_editor = true;

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
          break;
        }

        case IKEY_F3:
        case IKEY_l:
        {
          if(conf->standalone_mode)
            break;

          load_world_selection(mzx_world);
          fadein = true;
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
          // Restore saved game
          char save_file_name[MAX_PATH] = { 0 };

          if(conf->standalone_mode && !get_counter(mzx_world, "LOAD_MENU", 0))
            break;

          m_show();

          if(!choose_file_ch(mzx_world, save_ext, save_file_name,
           "Choose game to restore", 1))
          {
            boolean faded = false;

            vquick_fadeout();
            default_palette();
            clear_screen();
            fadein = true;

            // FIXME can't do anything with faded right now.
            if(reload_savegame(mzx_world, save_file_name, &faded))
            {
              if(curr_sav != save_file_name)
                strcpy(curr_sav, save_file_name);

              load_game_module(mzx_world, mzx_world->real_mod_playing, false);
              sfx_clear_queue();

              set_intro_mesg_timer(0);

              // Only send JUSTLOADED for savegames.
              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

              find_player(mzx_world);

              play_game(mzx_world);

              if(mzx_world->full_exit)
                break;

              // Done playing- load world again
              load_world_title(mzx_world, curr_file);
            }
          }
          break;
        }

        case IKEY_F5:
        case IKEY_p:
        {
          // Play game
          if(mzx_world->active)
          {
            char old_mod_playing[MAX_PATH];
            strcpy(old_mod_playing, mzx_world->real_mod_playing);

            if(mzx_world->only_from_swap)
            {
              m_show();
              error("You can only play this game via a swap"
               " from another game", 0, 24, 0x3101);
              break;
            }

            // Load world curr_file
            // Don't end mod- We want a smooth transition for that.

            vquick_fadeout();
            default_palette();
            clear_screen();
            fadein = true;

            if(reload_world(mzx_world, curr_file, &ignore))
            {
              if(mzx_world->current_board_id != mzx_world->first_board)
              {
                change_board(mzx_world, mzx_world->first_board);
              }

              src_board = mzx_world->current_board;

              // Send both JUSTENTERED and JUSTLOADED; the JUSTLOADED label will
              // take priority if a robot defines it.
              send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);
              send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

              // Load the mod unless it's the mod from the title screen or *.
              strcpy(mzx_world->real_mod_playing, old_mod_playing);
              load_game_module(mzx_world, src_board->mod_playing, true);
              sfx_clear_queue();

              set_intro_mesg_timer(0);

              set_counter(mzx_world, "TIME", src_board->time_limit, 0);

              find_player(mzx_world);
              mzx_world->player_restart_x = mzx_world->player_x;
              mzx_world->player_restart_y = mzx_world->player_y;

              // Load board palette and charset
              change_board_load_assets(mzx_world);

              play_game(mzx_world);

              if(mzx_world->full_exit)
                break;

              // Done playing- load world again
              load_world_title(mzx_world, curr_file);
            }
          }
          break;
        }

        case IKEY_F7:
        case IKEY_u:
        {
          if(check_for_updates)
          {
            // FIXME this is garbage
            int current_music_vol = audio_get_music_volume();
            int current_pcs_vol = audio_get_pcs_volume();
            audio_set_music_volume(0);
            audio_set_pcs_volume(0);
            if(mzx_world->active)
              audio_set_module_volume(0);

            check_for_updates(mzx_world, &mzx_world->conf, 0);
            set_caption(mzx_world, NULL, NULL, false);

            audio_set_pcs_volume(current_pcs_vol);
            audio_set_music_volume(current_music_vol);
            if(mzx_world->active)
              audio_set_module_volume(src_board->volume);
          }
          break;
        }

        case IKEY_F8:
        case IKEY_n:
          reload_curr_world_in_editor = false;

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

            load_world_title(mzx_world, curr_file);
            set_intro_mesg_timer(0);

            fadein = true;
          }
          break;
        }

        // Quickload saved game
        case IKEY_F10:
        {
          boolean faded = false;
          if(conf->standalone_mode && !get_counter(mzx_world, "LOAD_MENU", 0))
            break;

          m_show();

          vquick_fadeout();
          default_palette();
          clear_screen();
          fadein = true;

          // FIXME can't do anything with faded right now.
          if(reload_savegame(mzx_world, curr_sav, &faded))
          {
            load_game_module(mzx_world, mzx_world->real_mod_playing, false);
            sfx_clear_queue();

            set_intro_mesg_timer(0);

            // Only send JUSTLOADED for savegames.
            send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

            find_player(mzx_world);

            play_game(mzx_world);

            if(mzx_world->full_exit)
              break;

            // Done playing- load world again
            load_world_title(mzx_world, curr_file);
          }
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

          main_menu(mzx_world);
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
      }
    }

    if(mzx_world->full_exit)
      break;

  } while(!exit && !mzx_world->full_exit);

  vquick_fadeout();
  default_palette();
  clear_screen();
  audio_end_module();
  sfx_clear_queue();
}
