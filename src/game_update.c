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

#include "caption.h"
#include "core.h"
#include "counter.h"
#include "event.h"
#include "game.h"
#include "game_ops.h"
#include "game_player.h"
#include "game_update.h"
#include "graphics.h"
#include "idarray.h"
#include "idput.h"
#include "scrdisp.h" // strlencolor
#include "sprite.h"
#include "robot.h"
#include "world.h"
#include "world_struct.h"

#include "audio/audio.h"
#include "audio/sfx.h"

// Number of cycles to make player idle before repeating a
// directional move
#define REPEAT_WAIT 2

static void focus_on_player(struct world *mzx_world)
{
  int player_x   = mzx_world->player_x;
  int player_y   = mzx_world->player_y;
  int viewport_x = mzx_world->current_board->viewport_x;
  int viewport_y = mzx_world->current_board->viewport_y;
  int top_x, top_y;

  calculate_xytop(mzx_world, &top_x, &top_y);

  // Focus on the player
  focus_screen(player_x - top_x + viewport_x, player_y - top_y + viewport_y);
}

static void update_player(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int board_width = src_board->board_width;
  enum thing under_id =
   (enum thing)src_board->level_under_id[player_x +
   (player_y * board_width)];

  // t1 = ID stood on
  if(!(flags[under_id] & A_AFFECT_IF_STOOD))
  {
    return; // Nothing special
  }

  switch(under_id)
  {
    case ICE:
    {
      int player_last_dir = src_board->player_last_dir;
      if(player_last_dir & 0x0F)
      {
        if(move_player(mzx_world, (player_last_dir & 0x0F) - 1))
          src_board->player_last_dir = player_last_dir & 0xF0;
      }
      break;
    }

    case LAVA:
    {
      if(mzx_world->firewalker_dur > 0)
        break;

      play_sfx(mzx_world, SFX_LAVA);
      set_mesg(mzx_world, "Augh!");
      dec_counter(mzx_world, "HEALTH", id_dmg[26], 0);
      return;
    }

    case FIRE:
    {
      if(mzx_world->firewalker_dur > 0)
        break;

      play_sfx(mzx_world, SFX_FIRE_HURT);
      set_mesg(mzx_world, "Ouch!");
      dec_counter(mzx_world, "HEALTH", id_dmg[63], 0);
      return;
    }

    default:
    {
      move_player(mzx_world, under_id - 21);
      break;
    }
  }

  find_player(mzx_world);
}

// Updates game variables
// Slowed = 1 to not update lazwall or time
// due to slowtime or freezetime

static void update_variables(struct world *mzx_world, int slowed)
{
  struct board *src_board = mzx_world->current_board;
  int blind_dur = mzx_world->blind_dur;
  int firewalker_dur = mzx_world->firewalker_dur;
  int freeze_time_dur = mzx_world->freeze_time_dur;
  int slow_time_dur = mzx_world->slow_time_dur;
  int wind_dur = mzx_world->wind_dur;
  int b_mesg_timer = src_board->b_mesg_timer;
  int invinco;
  int lazwall_start = src_board->lazwall_start;
  int slow_down;

  // Toggle slow_down
  mzx_world->slow_down ^= 1;
  slow_down = mzx_world->slow_down;

  // If odd, we...
  if(!slow_down)
  {
    // Change scroll color
    scroll_color++;

    if(scroll_color > 15)
      scroll_color = 7;

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
          play_sfx(mzx_world, SFX_OUT_OF_TIME);
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
      play_sfx(mzx_world, SFX_INVINCO_BEAT);
      id_chars[player_color] = Random(256);
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

static Uint32 get_viewport_layer(struct world *mzx_world)
{
  // Creates a new layer if needed to prevent graphical discrepancies between
  // the fallback and layer rendering. This can happen in SMZX mode if:
  // 1) there is a visible viewport border or
  // 2) the message is active.

  struct board *src_board = mzx_world->current_board;

  if(get_screen_mode())
  {
    if((src_board->viewport_width < 80) ||
     (src_board->viewport_height < 25) ||
     (src_board->b_mesg_timer > 0))
      return create_layer(0, 0, 80, 25, LAYER_DRAWORDER_UI - 1, -1, 0, 1);
  }

  return UI_LAYER;
}

__editor_maybe_static void draw_viewport(struct world *mzx_world)
{
  int i, i2;
  struct board *src_board = mzx_world->current_board;
  int viewport_x = src_board->viewport_x;
  int viewport_y = src_board->viewport_y;
  int viewport_width = src_board->viewport_width;
  int viewport_height = src_board->viewport_height;
  char edge_color = mzx_world->edge_color;

  // Draw the current viewport
  if(viewport_y > 1)
  {
    // Top
    for(i = 0; i < viewport_y; i++)
      fill_line_ext(80, 0, i, 177, edge_color, 0, 0);
  }

  if((viewport_y + viewport_height) < 24)
  {
    // Bottom
    for(i = viewport_y + viewport_height + 1; i < 25; i++)
      fill_line_ext(80, 0, i, 177, edge_color, 0, 0);
  }

  if(viewport_x > 1)
  {
    // Left
    for(i = 0; i < 25; i++)
      fill_line_ext(viewport_x, 0, i, 177, edge_color, 0, 0);
  }

  if((viewport_x + viewport_width) < 79)
  {
    // Right
    i2 = viewport_x + viewport_width + 1;
    for(i = 0; i < 25; i++)
    {
      fill_line_ext(80 - i2, i2, i, 177, edge_color, 0, 0);
    }
  }

  // Draw the box
  if(viewport_x > 0)
  {
    // left
    for(i = 0; i < viewport_height; i++)
    {
      draw_char_ext('\xba', edge_color, viewport_x - 1,
       i + viewport_y, 0, 0);
    }

    if(viewport_y > 0)
    {
      draw_char_ext('\xc9', edge_color, viewport_x - 1,
       viewport_y - 1, 0, 0);
    }
  }
  if((viewport_x + viewport_width) < 80)
  {
    // right
    for(i = 0; i < viewport_height; i++)
    {
      draw_char_ext('\xba', edge_color,
       viewport_x + viewport_width, i + viewport_y, 0, 0);
    }

    if(viewport_y > 0)
    {
      draw_char_ext('\xbb', edge_color,
       viewport_x + viewport_width, viewport_y - 1, 0, 0);
    }
  }

  if(viewport_y > 0)
  {
    // top
    for(i = 0; i < viewport_width; i++)
    {
      draw_char_ext('\xcd', edge_color, viewport_x + i,
       viewport_y - 1, 0, 0);
    }
  }

  if((viewport_y + viewport_height) < 25)
  {
    // bottom
    for(i = 0; i < viewport_width; i++)
    {
      draw_char_ext('\xcd', edge_color, viewport_x + i,
       viewport_y + viewport_height, 0, 0);
    }

    if(viewport_x > 0)
    {
      draw_char_ext('\xc8', edge_color, viewport_x - 1,
       viewport_y + viewport_height, 0, 0);
    }

    if((viewport_x + viewport_width) < 80)
    {
      draw_char_ext('\xbc', edge_color, viewport_x + viewport_width,
       viewport_y + viewport_height, 0, 0);
    }
  }
}

/** FIXME fix this comically bad function name and rearrange some things between
 * this function and its counterpart below. This function occurs as part of the
 * drawing loop and should probably only draw things. This can be fixed by
 * moving parts of this function to the end of "update2", although this would
 * result in a minor change in the way gameplay is handled (initial frames on
 * title/gameplay start, duplicate frames after focus returns from a different
 * context).
 */
// "update_world"?
// FIXME try to break this function down into comprehensible parts
// FIXME move player helper functions out of game_ops
void update1(struct world *mzx_world, boolean is_title, boolean *fadein)
{
  int time_remaining;
  static int reload = 0;
  static int slowed = 0; // Flips between 0 and 1 during slow_time
  char tmp_str[10];
  struct board *src_board = mzx_world->current_board;
  int volume = src_board->volume;
  int volume_inc = src_board->volume_inc;
  int volume_target = src_board->volume_target;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char *level_under_id = src_board->level_under_id;
  char *level_under_color = src_board->level_under_color;
  char *level_under_param = src_board->level_under_param;

  if(!is_title && mzx_world->version >= V251s1 &&
   get_counter(mzx_world, "CURSORSTATE", 0))
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
    audio_set_module_volume(volume);
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
  focus_on_player(mzx_world);
  update_variables(mzx_world, slowed);
  update_player(mzx_world); // Ice, fire, water, lava

  if(mzx_world->wind_dur > 0)
  {
    // Wind
    int wind_dir = Random(9);
    if(wind_dir < 4)
    {
      // No wind this turn if above 3
      src_board->player_last_dir =
       (src_board->player_last_dir & 0xF0) + wind_dir;
      move_player(mzx_world, wind_dir);
      find_player(mzx_world);
    }
  }

  // The following is during gameplay ONLY
  if(!is_title && (!mzx_world->dead))
  {
    // Shoot
    if(get_key_status(keycode_internal_wrt_numlock, IKEY_SPACE)
     && mzx_world->bi_shoot_status)
    {
      if((!reload) && (!src_board->player_attack_locked))
      {
        int move_dir = -1;

        if(get_key_status(keycode_internal_wrt_numlock, IKEY_UP))
          move_dir = 0;
        else

        if(get_key_status(keycode_internal_wrt_numlock, IKEY_DOWN))
          move_dir = 1;
        else

        if(get_key_status(keycode_internal_wrt_numlock, IKEY_RIGHT))
          move_dir = 2;
        else

        if(get_key_status(keycode_internal_wrt_numlock, IKEY_LEFT))
          move_dir = 3;

        if(move_dir != -1)
        {
          if(!src_board->can_shoot)
            set_mesg(mzx_world, "Can't shoot here!");
          else

          if(!get_counter(mzx_world, "AMMO", 0))
          {
            set_mesg(mzx_world, "You are out of ammo!");
            play_sfx(mzx_world, SFX_OUT_OF_AMMO);
          }
          else
          {
            dec_counter(mzx_world, "AMMO", 1, 0);
            play_sfx(mzx_world, SFX_SHOOT);
            shoot(mzx_world, mzx_world->player_x, mzx_world->player_y,
             move_dir, PLAYER_BULLET);
            reload = 2;
            src_board->player_last_dir =
             (src_board->player_last_dir & 0x0F) | (move_dir << 4);
          }
        }
      }
    }
    else

    if((get_key_status(keycode_internal_wrt_numlock, IKEY_UP)) &&
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

    if((get_key_status(keycode_internal_wrt_numlock, IKEY_DOWN)) &&
     (!src_board->player_ns_locked))
    {
      int key_down_delay = mzx_world->key_down_delay;
      if((key_down_delay == 0) || (key_down_delay > REPEAT_WAIT))
      {
        move_player(mzx_world, 1);
        src_board->player_last_dir =
         (src_board->player_last_dir & 0x0F) + 0x10;
      }
      if(key_down_delay <= REPEAT_WAIT)
        mzx_world->key_down_delay = key_down_delay + 1;
    }
    else

    if((get_key_status(keycode_internal_wrt_numlock, IKEY_RIGHT)) &&
     (!src_board->player_ew_locked))
    {
      int key_right_delay = mzx_world->key_right_delay;
      if((key_right_delay == 0) || (key_right_delay > REPEAT_WAIT))
      {
        move_player(mzx_world, 2);
        src_board->player_last_dir =
         (src_board->player_last_dir & 0x0F) + 0x20;
      }
      if(key_right_delay <= REPEAT_WAIT)
        mzx_world->key_right_delay = key_right_delay + 1;
    }
    else

    if((get_key_status(keycode_internal_wrt_numlock, IKEY_LEFT)) &&
     (!src_board->player_ew_locked))
    {
      int key_left_delay = mzx_world->key_left_delay;
      if((key_left_delay == 0) || (key_left_delay > REPEAT_WAIT))
      {
        move_player(mzx_world, 3);
        src_board->player_last_dir =
         (src_board->player_last_dir & 0x0F) + 0x30;
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
    if(get_key_status(keycode_internal_wrt_numlock, IKEY_DELETE) &&
     (!src_board->player_attack_locked))
    {
      int d_offset =
       mzx_world->player_x + (mzx_world->player_y * board_width);

      if(level_under_id[d_offset] != (char)LIT_BOMB)
      {
        // Okay.
        if(!src_board->can_bomb)
        {
          set_mesg(mzx_world, "Can't bomb here!");
        }
        else

        if((mzx_world->bomb_type == 0) &&
         (!get_counter(mzx_world, "LOBOMBS", 0)))
        {
          set_mesg(mzx_world, "You are out of low strength bombs!");
          play_sfx(mzx_world, SFX_OUT_OF_BOMBS);
        }
        else

        if((mzx_world->bomb_type == 1) &&
         (!get_counter(mzx_world, "HIBOMBS", 0)))
        {
          set_mesg(mzx_world, "You are out of high strength bombs!");
          play_sfx(mzx_world, SFX_OUT_OF_BOMBS);
        }
        else

        if(!(flags[(int)level_under_id[d_offset]] & A_ENTRANCE))
        {
          // Bomb!
          mzx_world->under_player_id = level_under_id[d_offset];
          mzx_world->under_player_color = level_under_color[d_offset];
          mzx_world->under_player_param = level_under_param[d_offset];
          level_under_id[d_offset] = 37;
          level_under_color[d_offset] = 8;
          level_under_param[d_offset] = mzx_world->bomb_type << 7;

          if(mzx_world->bomb_type)
          {
            play_sfx(mzx_world, SFX_PLACE_HI_BOMB);
            dec_counter(mzx_world, "HIBOMBS", 1, 0);
          }
          else
          {
            play_sfx(mzx_world, SFX_PLACE_LO_BOMB);
            dec_counter(mzx_world, "LOBOMBS", 1, 0);
          }
        }
      }
    }
    if(reload) reload--;
  }

  mzx_world->change_game_state = 0;

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
    int d_offset =
     mzx_world->player_x + (mzx_world->player_y * board_width);

    mzx_world->was_zapped = 0;
    if(flags[(int)level_under_id[d_offset]] & A_ENTRANCE)
      entrance = 0;

    update_board(mzx_world);

    src_board = mzx_world->current_board;
    level_under_id = src_board->level_under_id;
    board_width = src_board->board_width;
    d_offset = mzx_world->player_x + (mzx_world->player_y * board_width);

    // Pushed onto an entrance?

    if((entrance) && (flags[(int)level_under_id[d_offset]] & A_ENTRANCE)
     && (!mzx_world->was_zapped))
    {
      int d_board = src_board->level_under_param[d_offset];
      sfx_clear_queue(); // Since there is often a push sound
      play_sfx(mzx_world, SFX_ENTRANCE);

      // Same board or nonexistant?
      if((d_board != mzx_world->current_board_id)
       && (d_board < mzx_world->num_boards) &&
       (mzx_world->board_list[d_board] != NULL))
      {
        // Nope.
        // Go to board d_board
        mzx_world->target_board = d_board;
        mzx_world->target_where = TARGET_ENTRANCE;
        mzx_world->target_color = level_under_color[d_offset];
        mzx_world->target_id = (enum thing)level_under_id[d_offset];
      }
    }

    mzx_world->was_zapped = 0;
  }
  else
  {
    // Place the player and clean up clones
    // just in case the player was moved while the game was slowed
    find_player(mzx_world);
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
        id_remove_top(mzx_world, mzx_world->player_x,
         mzx_world->player_y);
        id_place(mzx_world, endgame_x, endgame_y, PLAYER, 0, 0);
        mzx_world->player_x = endgame_x;
        mzx_world->player_y = endgame_y;
      }
      else
      {
        mzx_world->target_board = endgame_board;
        mzx_world->target_where = TARGET_POSITION;
        mzx_world->target_x = endgame_x;
        mzx_world->target_y = endgame_y;
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
      /* I can't imagine anything actually relied on this obtuse misbehavior
       * but it's good to version lock anyhow.
       */
      if((mzx_world->version <= V283) || mzx_world->bi_mesg_status)
      {
        src_board->b_mesg_row = 24;
        src_board->b_mesg_col = -1;
      }
      if(mzx_world->game_over_sfx)
        play_sfx(mzx_world, SFX_GAME_OVER);
      mzx_world->dead = 1;
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
    sfx_clear_queue();
    play_sfx(mzx_world, SFX_DEATH);

    // Go somewhere else?
    if(death_board != DEATH_SAME_POS)
    {
      if(death_board == NO_DEATH_BOARD)
      {
        int player_restart_x = mzx_world->player_restart_x;
        int player_restart_y = mzx_world->player_restart_y;

        if(player_restart_x >= board_width)
          player_restart_x = board_width - 1;

        if(player_restart_y >= board_height)
          player_restart_y = board_height - 1;

        // Return to entry x/y
        id_remove_top(mzx_world, mzx_world->player_x,
         mzx_world->player_y);
        id_place(mzx_world, player_restart_x, player_restart_y,
         PLAYER, 0, 0);
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

          id_remove_top(mzx_world, mzx_world->player_x,
           mzx_world->player_y);
          id_place(mzx_world, death_x, death_y, PLAYER, 0, 0);
          mzx_world->player_x = death_x;
          mzx_world->player_y = death_y;
        }
        else
        {
          mzx_world->target_board = death_board;
          mzx_world->target_where = TARGET_POSITION;
          mzx_world->target_x = mzx_world->death_x;
          mzx_world->target_y = mzx_world->death_y;
        }
      }
    }
  }

  if(mzx_world->target_where != TARGET_TELEPORT)
  {
    Uint32 viewport_layer;
    int top_x;
    int top_y;

    blank_layers();

    // Draw border
    viewport_layer = get_viewport_layer(mzx_world);
    select_layer(viewport_layer);
    draw_viewport(mzx_world);

    // Draw screen
    if(is_title)
    {
      id_chars[player_color] = 0;
      id_chars[player_char + 0] = 32;
      id_chars[player_char + 1] = 32;
      id_chars[player_char + 2] = 32;
      id_chars[player_char + 3] = 32;
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

      select_layer(BOARD_LAYER);

      for(i = viewport_y; i < viewport_y + viewport_height; i++)
      {
        fill_line(viewport_width, viewport_x, i, 176, 8);
      }

      // Find where player would be and draw.
      if(!is_title)
      {
        id_put(src_board, player_x - top_x + viewport_x,
         player_y - top_y + viewport_y, player_x,
         player_y, player_x, player_y);
      }
    }
    else
    {
      draw_game_window(src_board, top_x, top_y);
    }
    select_layer(OVERLAY_LAYER);

    // Add sprites
    draw_sprites(mzx_world);

    // Add time limit
    time_remaining = get_counter(mzx_world, "TIME", 0);
    if(time_remaining)
    {
      int edge_color = mzx_world->edge_color;
      int timer_color;
      if(edge_color == 15)
        timer_color = 0xF0; // Prevent white on white for timer
      else
        timer_color = (edge_color << 4) + 15;

      select_layer(UI_LAYER);

      sprintf(tmp_str, "%d:%02d",
       (unsigned short)(time_remaining / 60), (time_remaining % 60) );
      write_string(tmp_str, 1, 24, timer_color, 0);

      // Border with spaces
      draw_char(' ', edge_color, (Uint32)strlen(tmp_str) + 1, 24);
      draw_char(' ', edge_color, 0, 24);
    }

    // Add message
    if(src_board->b_mesg_timer > 0)
    {
      int mesg_y = src_board->b_mesg_row;
      Uint8 tmp_color = scroll_color;
      char *lines[25];
      int i = 1, j;

      if(mzx_world->smzx_message)
        select_layer(viewport_layer);
      else
        select_layer(UI_LAYER);

      /* Always at least one line.. */
      lines[0] = src_board->bottom_mesg;

      /* Find pointers to each "line" terminated by \n */
      while(1)
      {
        char *pos = strchr(lines[i - 1], '\n');
        if(!pos)
          break;
        *pos = 0;
        if(i >= 25 - mesg_y)
          break;
        lines[i] = pos + 1;
        i++;
      }

      for(j = 0; j < i; j++)
      {
        int mesg_length = strlencolor(lines[j]);
        int mesg_edges = mzx_world->mesg_edges;
        int mesg_x = src_board->b_mesg_col;
        char backup = 0;

        if(mesg_length > 80)
        {
          backup = lines[j][80];
          lines[j][80] = 0;
          mesg_length = 80;
        }

        if(mesg_x == -1)
          mesg_x = 40 - (mesg_length / 2);

        color_string_ext_special(lines[j], mesg_x, mesg_y, &tmp_color,
         0, 0, false);

        if((mesg_x > 0) && (mesg_edges))
          draw_char_ext(' ', scroll_color, mesg_x - 1, mesg_y, 0, 0);

        mesg_x += mesg_length;
        if((mesg_x < 80) && (mesg_edges))
          draw_char_ext(' ', scroll_color, mesg_x, mesg_y, 0, 0);

        if(backup)
          lines[j][80] = backup;

        mesg_y++;
      }

      /* Restore original bottom mesg for next iteration */
      for(j = 1; j < i; j++)
        *(lines[j] - 1) = '\n';
    }

    select_layer(UI_LAYER);
    draw_intro_mesg(mzx_world);

    // Add debug box
    if(draw_debug_box && mzx_world->debug_mode)
    {
      draw_debug_box(mzx_world, 60, 19, mzx_world->player_x,
       mzx_world->player_y, 1);
    }
  }
}

/** FIXME fix this comically bad function name and rearrange some things between
 * these functions. Note that this occurs BEFORE the key function, so key labels
 * would have to move to the end of this function if parts of the main
 * update function were to join this mess.
 */
// "update_start_cycle"?
boolean update2(struct world *mzx_world, boolean is_title, boolean *fadein)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char *level_id = src_board->level_id;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;

  if(*fadein)
  {
    vquick_fadein();
    *fadein = false;
  }

  switch(mzx_world->change_game_state)
  {
    case CHANGE_STATE_SWAP_WORLD:
    {
      // Load the new board's mod
      load_board_module(mzx_world);

      // Send both JUSTLOADED and JUSTENTERED; the JUSTENTERED label will take
      // priority if a robot defines it (instead of JUSTLOADED like on the title
      // screen).
      send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
      send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);

      return true;
    }

    case CHANGE_STATE_LOAD_GAME_ROBOTIC:
    {
      load_game_module(mzx_world, mzx_world->real_mod_playing, false);

      // Only send JUSTLOADED for savegames.
      send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);

      return true;
    }

    case CHANGE_STATE_EXIT_GAME_ROBOTIC:
    case CHANGE_STATE_REQUEST_EXIT:
      // Exit game--skip input processing. The game state will exit.
      return true;

    case CHANGE_STATE_PLAY_GAME_ROBOTIC:
      // We need to continue input processing.
      return false;

    default:
      break;
  }

  if(mzx_world->target_where != TARGET_NONE)
  {
    int saved_player_last_dir = src_board->player_last_dir;
    int target_board = mzx_world->target_board;
    boolean load_assets = false;

    // Aha.. TELEPORT or ENTRANCE.
    // Destroy message, bullets, spitfire?

    if(mzx_world->clear_on_exit)
    {
      int offset;
      enum thing d_id;

      src_board->b_mesg_timer = 0;
      for(offset = 0; offset < (board_width * board_height); offset++)
      {
        d_id = (enum thing)level_id[offset];
        if((d_id == SHOOTING_FIRE) || (d_id == BULLET))
          offs_remove_id(mzx_world, offset);
      }
    }

    // Load board
    mzx_world->under_player_id = (char)SPACE;
    mzx_world->under_player_param = 0;
    mzx_world->under_player_color = 7;
    mzx_world->slow_down = 0;

    if(mzx_world->current_board_id != target_board)
    {
      change_board(mzx_world, target_board);
      load_assets = true;
    }

    src_board = mzx_world->current_board;

    level_id = src_board->level_id;
    level_color = src_board->level_color;
    level_under_id = src_board->level_under_id;
    board_width = src_board->board_width;
    board_height = src_board->board_height;

    // Find target x/y
    if(mzx_world->target_where == TARGET_ENTRANCE)
    {
      int i;
      int tmp_x[5];
      int tmp_y[5];
      int x, y, offset;
      enum thing d_id;
      enum thing target_id = mzx_world->target_id;
      int target_color = mzx_world->target_color;

      // Entrance
      // First, set searched for id to the first in the whirlpool
      // series if it is part of the whirlpool series
      if(is_whirlpool(target_id))
        target_id = WHIRLPOOL_1;

      // Now scan. Order-
      // 1) Same type & color
      // 2) Same color
      // 3) Same type & foreground
      // 4) Same foreground
      // 5) Same type
      // Search for first of all 5 at once

      for(i = 0; i < 5; i++)
      {
        // None found
        tmp_x[i] = -1;
        tmp_y[i] = -1;
      }

      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          d_id = (enum thing)level_id[offset];

          if(d_id == PLAYER)
          {
            // Remove the player, maybe readd
            mzx_world->player_x = x;
            mzx_world->player_y = y;
            id_remove_top(mzx_world, x, y);
            // Grab again - might have revealed an entrance
            d_id = (enum thing)level_id[offset];
          }

          if(is_whirlpool(d_id))
            d_id = WHIRLPOOL_1;

          if(d_id == target_id)
          {
            // Same type
            // Color match?
            if(level_color[offset] == target_color)
            {
              // Yep
              tmp_x[0] = x;
              tmp_y[0] = y;
            }
            else

            // Fg?
            if((level_color[offset] & 0x0F) ==
             (target_color & 0x0F))
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

          if(flags[(int)d_id] & A_ENTRANCE)
          {
            // Not same type, but an entrance
            // Color match?
            if(level_color[offset] == target_color)
            {
              // Yep
              tmp_x[1] = x;
              tmp_y[1] = y;
            }
            // Fg?
            else

            if((level_color[offset] & 0x0F) == (target_color & 0x0F))
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
        mzx_world->player_x = tmp_x[i];
        mzx_world->player_y = tmp_y[i];
      }

      id_place(mzx_world, mzx_world->player_x,
       mzx_world->player_y, PLAYER, 0, 0);
    }
    else
    {
      int target_x = mzx_world->target_x;
      int target_y = mzx_world->target_y;

      // Specified x/y
      if(target_x < 0)
        target_x = 0;

      if(target_y < 0)
        target_y = 0;

      if(target_x >= board_width)
        target_x = board_width - 1;

      if(target_y >= board_height)
        target_y = board_height - 1;

      find_player(mzx_world);
      place_player_xy(mzx_world, target_x, target_y);
    }

    send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);

    // Set the time/restart position/etc even if the board hasn't changed.
    change_board_set_values(mzx_world);

    // Now... Set player_last_dir for direction FACED
    src_board->player_last_dir = (src_board->player_last_dir & 0x0F) |
     (saved_player_last_dir & 0xF0);

    // ...and if player ended up on ICE, set last dir pressed as well
    if((enum thing)level_under_id[mzx_world->player_x +
     (mzx_world->player_y * board_width)] == ICE)
    {
      src_board->player_last_dir = saved_player_last_dir;
    }

    if(mzx_world->target_where != TARGET_TELEPORT)
    {
      // Prepare for fadein
      if(!get_fade_status())
        *fadein = true;
      vquick_fadeout();
    }

    // Load the new board's charset and palette, if necessary
    if(load_assets)
    {
      // Load board's mod unless it's the same mod playing.
      load_game_module(mzx_world, src_board->mod_playing, true);
      change_board_load_assets(mzx_world);

#ifdef CONFIG_EDITOR
      // Also, update the caption to indicate the current board.
      if(mzx_world->editing)
        caption_set_board(mzx_world, mzx_world->current_board);
#endif
    }

    mzx_world->target_where = TARGET_NONE;

    // Disallow any keypresses this cycle

    return true;
  }

  return false;
}
