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

#include "counter.h"
#include "game_ops.h"
#include "game_player.h"
#include "graphics.h"
#include "idarray.h"
#include "idput.h"
#include "robot.h"
#include "scrdisp.h"
#include "window.h"
#include "world_struct.h"

#include "audio/sfx.h"

/**
 * Player functions for gameplay.
 */

// FIXME context
static unsigned int intro_mesg_timer = MESG_TIMEOUT;

void enable_intro_mesg(void)
{
  intro_mesg_timer = MESG_TIMEOUT;
}

void clear_intro_mesg(void)
{
  intro_mesg_timer = 0;
}

void draw_intro_mesg(struct world *mzx_world)
{
  static const char mesg1[] = "F1: Help   ";
  static const char mesg2[] = "Enter: Menu   Ctrl-Alt-Enter: Fullscreen";
  struct config_info *conf = &mzx_world->conf;

  if(conf->standalone_mode)
    return;

  if(intro_mesg_timer == 0)
    return;

  intro_mesg_timer--;

  if(mzx_world->help_file)
  {
    write_string(mesg1, 14, 24, scroll_color, 0);
    write_string(mesg2, 25, 24, scroll_color, 0);
  }
  else
  {
    write_string(mesg2, 20, 24, scroll_color, 0);
  }
}

void set_mesg(struct world *mzx_world, const char *str)
{
  if(mzx_world->bi_mesg_status)
  {
    set_mesg_direct(mzx_world->current_board, str);
  }
}

void set_mesg_direct(struct board *src_board, const char *str)
{
  strncpy(src_board->bottom_mesg, str, ROBOT_MAX_TR - 1);
  src_board->bottom_mesg[ROBOT_MAX_TR - 1] = 0;
  src_board->b_mesg_timer = MESG_TIMEOUT;
  clear_intro_mesg();
}

static void set_3_mesg(struct world *mzx_world, const char *str1, int num,
 const char *str2)
{
  if(mzx_world->bi_mesg_status)
  {
    struct board *src_board = mzx_world->current_board;
    sprintf(src_board->bottom_mesg, "%s%d%s", str1, num, str2);
    src_board->b_mesg_timer = MESG_TIMEOUT;
  }
}

boolean player_can_save(struct world *mzx_world)
{
  struct board *cur_board = mzx_world->current_board;
  int offset;

  if(cur_board->save_mode == CANT_SAVE)
    return false;

  if(cur_board->save_mode == CAN_SAVE_ON_SENSOR)
  {
    offset = xy_to_offset(cur_board, mzx_world->player_x, mzx_world->player_y);

    if(cur_board->level_under_id[offset] != SENSOR)
      return false;
  }

  return true;
}

void player_switch_bomb_type(struct world *mzx_world)
{
  struct board *cur_board = mzx_world->current_board;

  mzx_world->bomb_type ^= 1;
  if(!cur_board->player_attack_locked && cur_board->can_bomb)
  {
    play_sfx(mzx_world, SFX_SWITCH_BOMB_TYPE);
    if(mzx_world->bomb_type)
    {
      set_mesg(mzx_world, "You switch to high strength bombs.");
    }
    else
    {
      set_mesg(mzx_world, "You switch to low strength bombs.");
    }
  }
}

void player_cheat_give_all(struct world *mzx_world)
{
  struct board *cur_board = mzx_world->current_board;
  int i;

  set_counter(mzx_world, "AMMO", 32767, 1);
  set_counter(mzx_world, "COINS", 32767, 1);
  set_counter(mzx_world, "GEMS", 32767, 1);
  set_counter(mzx_world, "HEALTH", 32767, 1);
  set_counter(mzx_world, "HIBOMBS", 32767, 1);
  set_counter(mzx_world, "LIVES", 32767, 1);
  set_counter(mzx_world, "LOBOMBS", 32767, 1);
  set_counter(mzx_world, "SCORE", 0, 1);
  set_counter(mzx_world, "TIME", cur_board->time_limit, 1);

  mzx_world->dead = 0;

  for(i = 0; i < 16; i++)
  {
    mzx_world->keys[i] = i;
  }

  cur_board->player_ns_locked = 0;
  cur_board->player_ew_locked = 0;
  cur_board->player_attack_locked = 0;
}

void player_cheat_zap(struct world *mzx_world)
{
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int board_width = mzx_world->current_board->board_width;
  int board_height = mzx_world->current_board->board_height;

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

static void give_potion(struct world *mzx_world, enum potion type)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;

  play_sfx(mzx_world, SFX_RING_POTION);
  inc_counter(mzx_world, "SCORE", 5, 0);

  switch(type)
  {
    case POTION_DUD:
    {
      set_mesg(mzx_world, "* No effect *");
      break;
    }

    case POTION_INVINCO:
    {
      set_mesg(mzx_world, "* Invinco *");
      send_robot_def(mzx_world, 0, LABEL_INVINCO);
      set_counter(mzx_world, "INVINCO", 113, 0);
      break;
    }

    case POTION_BLAST:
    {
      int placement_period = 18;
      int x, y, offset, d_flag;
      enum thing d_id;

      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          d_id = (enum thing)level_id[offset];
          d_flag = flags[d_id];

          if((d_flag & A_UNDER) && !(d_flag & A_ENTRANCE))
          {
            // Adjust the ratio for board size

            if(Random(placement_period) == 0)
            {
              id_place(mzx_world, x, y, EXPLOSION, 0,
               16 * ((Random(5)) + 2));
            }
          }
        }
      }

      set_mesg(mzx_world, "* Blast *");
      break;
    }

    case POTION_SMALL_HEALTH:
    {
      inc_counter(mzx_world, "HEALTH", 10, 0);
      set_mesg(mzx_world, "* Healing *");
      break;
    }

    case POTION_BIG_HEALTH:
    {
      inc_counter(mzx_world, "HEALTH", 50, 0);
      set_mesg(mzx_world, "* Healing *");
      break;
    }

    case POTION_POISON:
    {
      dec_counter(mzx_world, "HEALTH", 10, 0);
      set_mesg(mzx_world, "* Poison *");
      break;
    }

    case POTION_BLIND:
    {
      mzx_world->blind_dur = 200;
      set_mesg(mzx_world, "* Blind *");
      break;
    }

    case POTION_KILL:
    {
      int x, y, offset;
      enum thing d_id;

      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          d_id = (enum thing)level_id[offset];

          if(is_enemy(d_id))
            id_remove_top(mzx_world, x, y);
        }
      }
      set_mesg(mzx_world, "* Kill enemies *");
      break;
    }

    case POTION_FIREWALK:
    {
      mzx_world->firewalker_dur = 200;
      set_mesg(mzx_world, "* Firewalking *");
      break;
    }

    case POTION_DETONATE:
    {
      int x, y, offset;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          if(level_id[offset] == BOMB)
          {
            level_id[offset] = EXPLOSION;
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

    case POTION_BANISH:
    {
      int x, y, offset;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          if(level_id[offset] == (char)DRAGON)
          {
            level_id[offset] = GHOST;
            level_param[offset] = 51;
          }
        }
      }
      set_mesg(mzx_world, "* Banish dragons *");
      break;
    }

    case POTION_SUMMON:
    {
      enum thing d_id;
      int x, y, offset;

      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          d_id = (enum thing)level_id[offset];
          if(is_enemy(d_id))
          {
            level_id[offset] = (char)DRAGON;
            level_color[offset] = 4;
            level_param[offset] = 102;
          }
        }
      }
      set_mesg(mzx_world, "* Summon dragons *");
      break;
    }

    case POTION_AVALANCHE:
    {
      int placement_period = 18;
      int x, y, offset;

      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          int d_flag = flags[(int)level_id[offset]];

          if((d_flag & A_UNDER) && !(d_flag & A_ENTRANCE))
          {
            if((Random(placement_period)) == 0)
            {
              id_place(mzx_world, x, y, BOULDER, 7, 0);
            }
          }
        }
      }
      set_mesg(mzx_world, "* Avalanche *");
      break;
    }

    case POTION_FREEZE:
    {
      mzx_world->freeze_time_dur = 200;
      set_mesg(mzx_world, "* Freeze time *");
      break;
    }

    case POTION_WIND:
    {
      mzx_world->wind_dur = 200;
      set_mesg(mzx_world, "* Wind *");
      break;
    }

    case POTION_SLOW:
    {
      mzx_world->slow_time_dur = 200;
      set_mesg(mzx_world, "* Slow time *");
      break;
    }
  }
}

void hurt_player(struct world *mzx_world, enum thing damage_src)
{
  int amount = id_dmg[damage_src];
  dec_counter(mzx_world, "health", amount, 0);
  play_sfx(mzx_world, SFX_HURT);
  set_mesg(mzx_world, "Ouch!");
}

int take_key(struct world *mzx_world, int color)
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
int give_key(struct world *mzx_world, int color)
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

int grab_item(struct world *mzx_world, int offset, int dir)
{
  // Dir is for transporter
  struct board *src_board = mzx_world->current_board;
  enum thing id = (enum thing)src_board->level_id[offset];
  char param = src_board->level_param[offset];
  char color = src_board->level_color[offset];
  int remove = 0;

  char tmp[81];

  switch(id)
  {
    case CHEST: // Chest
    {
      enum chest_contents item_type = (param & 15);
      int item_value = (param & 240) >> 4;

      if(item_type == ITEM_NONE)
      {
        play_sfx(mzx_world, SFX_EMPTY_CHEST);
        break;
      }

      // Act upon contents
      play_sfx(mzx_world, SFX_CHEST);

      switch(item_type)
      {
        case ITEM_NONE: // Nothing
          break;

        case ITEM_KEY: // Key
        {
          if(give_key(mzx_world, item_value))
          {
            set_mesg(mzx_world, "Inside the chest is a key, "
             "but you can't carry any more keys!");
            return 0;
          }
          set_mesg(mzx_world, "Inside the chest you find a key.");
          break;
        }

        case ITEM_COINS: // Coins
        {
          item_value *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ",
           item_value, " coins.");
          inc_counter(mzx_world, "COINS", item_value, 0);
          inc_counter(mzx_world, "SCORE", item_value, 0);
          break;
        }

        case ITEM_LIFE: // Life
        {
          if(item_value > 1)
          {
            set_3_mesg(mzx_world, "Inside the chest you find ",
             item_value, " free lives.");
          }
          else
          {
            set_mesg(mzx_world,
             "Inside the chest you find 1 free life.");
          }
          inc_counter(mzx_world, "LIVES", item_value, 0);
          break;
        }

        case ITEM_AMMO: // Ammo
        {
          item_value *= 5;
          set_3_mesg(mzx_world,
           "Inside the chest you find ", item_value, " rounds of ammo.");
          inc_counter(mzx_world, "AMMO", item_value, 0);
          break;
        }

        case ITEM_GEMS: // Gems
        {
          item_value *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ",
           item_value, " gems.");
          inc_counter(mzx_world, "GEMS", item_value, 0);
          inc_counter(mzx_world, "SCORE", item_value, 0);
          break;
        }

        case ITEM_HEALTH: // Health
        {
          item_value *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ",
           item_value, " health.");
          inc_counter(mzx_world, "HEALTH", item_value, 0);
          break;
        }

        case ITEM_POTION: // Potion
        {
          int answer;
          m_show();
          answer = confirm(mzx_world,
           "Inside the chest you find a potion. Drink it?");

          if(answer)
            return 0;

          src_board->level_param[offset] = 0;
          give_potion(mzx_world, (enum potion)item_value);
          break;
        }

        case ITEM_RING: // Ring
        {
          int answer;

          m_show();

          answer = confirm(mzx_world,
           "Inside the chest you find a ring. Wear it?");

          if(answer)
            return 0;

          src_board->level_param[offset] = 0;
          give_potion(mzx_world, (enum potion)item_value);
          break;
        }

        case ITEM_LOBOMBS: // Lobombs
        {
          item_value *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ", item_value,
           " low strength bombs.");
          inc_counter(mzx_world, "LOBOMBS", item_value, 0);
          break;
        }

        case ITEM_HIBOMBS: // Hibombs
        {
          item_value *= 5;
          set_3_mesg(mzx_world, "Inside the chest you find ", item_value,
           " high strength bombs.");
          inc_counter(mzx_world, "HIBOMBS", item_value, 0);
          break;
        }
      }
      // Empty chest
      src_board->level_param[offset] = 0;
      break;
    }

    case GEM:
    case MAGIC_GEM:
    {
      if(id == GEM)
        play_sfx(mzx_world, SFX_GEM);
      else
        play_sfx(mzx_world, SFX_MAGIC_GEM);

      inc_counter(mzx_world, "GEMS", 1, 0);

      if(id == MAGIC_GEM)
        inc_counter(mzx_world, "HEALTH", 1, 0);

      inc_counter(mzx_world, "SCORE", 1, 0);
      remove = 1;
      break;
    }

    case HEALTH:
    {
      play_sfx(mzx_world, SFX_HEALTH);
      inc_counter(mzx_world, "HEALTH", param, 0);
      remove = 1;
      break;
    }

    case RING:
    case POTION:
    {
      give_potion(mzx_world, (enum potion)param);
      remove = 1;
      break;
    }

    case GOOP:
    {
      play_sfx(mzx_world, SFX_GOOP);
      set_mesg(mzx_world, "There is goop in your way!");
      send_robot_def(mzx_world, 0, LABEL_GOOPTOUCHED);
      break;
    }

    case ENERGIZER:
    {
      play_sfx(mzx_world, SFX_INVINCO_START);
      set_mesg(mzx_world, "Energize!");
      send_robot_def(mzx_world, 0, LABEL_INVINCO);
      set_counter(mzx_world, "INVINCO", 113, 0);
      remove = 1;
      break;
    }

    case AMMO:
    {
      play_sfx(mzx_world, SFX_AMMO);
      inc_counter(mzx_world, "AMMO", param, 0);
      remove = 1;
      break;
    }

    case BOMB:
    {
      if(src_board->collect_bombs)
      {
        if(param)
        {
          play_sfx(mzx_world, SFX_HI_BOMB);
          inc_counter(mzx_world, "HIBOMBS", 1, 0);
        }
        else
        {
          play_sfx(mzx_world, SFX_LO_BOMB);
          inc_counter(mzx_world, "LOBOMBS", 1, 0);
        }
        remove = 1;
      }
      else
      {
        // Light bomb
        play_sfx(mzx_world, SFX_PLACE_LO_BOMB);
        src_board->level_id[offset] = 37;
        src_board->level_param[offset] = param << 7;
      }
      break;
    }

    case KEY:
    {
      if(give_key(mzx_world, color))
      {
        play_sfx(mzx_world, SFX_FULL_KEYS);
        set_mesg(mzx_world, "You can't carry any more keys!");
      }
      else
      {
        play_sfx(mzx_world, SFX_KEY);
        set_mesg(mzx_world, "You grab the key.");
        remove = 1;
      }
      break;
    }

    case LOCK:
    {
      if(take_key(mzx_world, color))
      {
        play_sfx(mzx_world, SFX_CANT_UNLOCK);
        set_mesg(mzx_world, "You need an appropriate key!");
      }
      else
      {
        play_sfx(mzx_world, SFX_UNLOCK);
        set_mesg(mzx_world, "You open the lock.");
        remove = 1;
      }
      break;
    }

    case DOOR:
    {
      int board_width = src_board->board_width;
      char *level_id = src_board->level_id;
      char *level_param = src_board->level_param;
      int x = offset % board_width;
      int y = offset / board_width;
      char door_first_movement[8] = { 0, 3, 0, 2, 1, 3, 1, 2 };

      if(param & 8)
      {
        // Locked
        if(take_key(mzx_world, color))
        {
          // Need key
          play_sfx(mzx_world, SFX_DOOR_LOCKED);
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

      if(move(mzx_world, x, y, door_first_movement[param & 7],
       CAN_PUSH | CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK))
      {
        set_mesg(mzx_world, "The door is blocked from opening!");
        play_sfx(mzx_world, SFX_DOOR_LOCKED);
        level_id[offset] = 41;
        level_param[offset] = param & 7;
      }
      else
      {
        play_sfx(mzx_world, SFX_OPEN_DOOR);
      }
      break;
    }

    case GATE:
    {
      if(param)
      {
        // Locked
        if(take_key(mzx_world, color))
        {
          // Need key
          play_sfx(mzx_world, SFX_GATE_LOCKED);
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

      src_board->level_id[offset] = (char)OPEN_GATE;
      src_board->level_param[offset] = 22;
      play_sfx(mzx_world, SFX_OPEN_GATE);
      break;
    }

    case TRANSPORT:
    {
      int x = offset % src_board->board_width;
      int y = offset / src_board->board_width;

      if(transport(mzx_world, x, y, dir, PLAYER, 0, 0, 1))
        break;

      return 1;
    }

    case COIN:
    {
      play_sfx(mzx_world, SFX_COIN);
      inc_counter(mzx_world, "COINS", 1, 0);
      inc_counter(mzx_world, "SCORE", 1, 0);
      remove = 1;
      break;
    }

    case POUCH:
    {
      play_sfx(mzx_world, SFX_POUCH);
      inc_counter(mzx_world, "GEMS", (param & 15) * 5, 0);
      inc_counter(mzx_world, "COINS", (param >> 4) * 5, 0);
      inc_counter(mzx_world, "SCORE", ((param & 15) + (param >> 4)) * 5, 1);
      sprintf(tmp, "The pouch contains %d gems and %d coins.",
       (param & 15) * 5, (param >> 4) * 5);
      set_mesg(mzx_world, tmp);
      remove = 1;
      break;
    }

    case FOREST:
    {
      play_sfx(mzx_world, SFX_FOREST);
      if(src_board->forest_becomes == FOREST_TO_EMPTY)
      {
        remove = 1;
      }
      else
      {
        src_board->level_id[offset] = (char)FLOOR;
        return 1;
      }
      break;
    }

    case LIFE:
    {
      play_sfx(mzx_world, SFX_LIFE);
      inc_counter(mzx_world, "LIVES", 1, 0);
      set_mesg(mzx_world, "You find a free life!");
      remove = 1;
      break;
    }

    case INVIS_WALL:
    {
      src_board->level_id[offset] = (char)NORMAL;
      set_mesg(mzx_world, "Oof! You ran into an invisible wall.");
      play_sfx(mzx_world, SFX_INVIS_WALL);
      break;
    }

    case MINE:
    {
      src_board->level_id[offset] = (char)EXPLOSION;
      src_board->level_param[offset] = param & 240;
      play_sfx(mzx_world, SFX_EXPLOSION);
      break;
    }

    case EYE:
    {
      src_board->level_id[offset] = (char)EXPLOSION;
      src_board->level_param[offset] = (param << 1) & 112;
      break;
    }

    case THIEF:
    {
      dec_counter(mzx_world, "GEMS", (param & 128) >> 7, 0);
      play_sfx(mzx_world, SFX_STOLEN_GEM);
      break;
    }

    case SLIMEBLOB:
    {
      if(param & 64)
        hurt_player(mzx_world, SLIMEBLOB);

      if(param & 128)
        break;

      src_board->level_id[offset] = (char)BREAKAWAY;
      break;
    }

    case GHOST:
    {
      hurt_player(mzx_world, GHOST);

      // Die !?
      if(!(param & 8))
        remove = 1;

      break;
    }

    case DRAGON:
    {
      hurt_player(mzx_world, DRAGON);
      break;
    }

    case FISH:
    {
      if(param & 64)
      {
        hurt_player(mzx_world, FISH);
        remove = 1;
      }
      break;
    }

    case ROBOT:
    {
      int idx = param;

      // update last touched direction
      src_board->robot_list[idx]->last_touch_dir =
       int_to_dir(flip_dir(dir));

      send_robot_def(mzx_world, param, LABEL_TOUCH);
      break;
    }

    case SIGN:
    case SCROLL:
    {
      int idx = param;
      play_sfx(mzx_world, SFX_SCROLL_SIGN);

      m_show();
      scroll_edit(mzx_world, src_board->scroll_list[idx], id & 1);

      if(id == SCROLL)
        remove = 1;
      break;
    }

    default:
      break;
  }

  if(remove == 1)
    offs_remove_id(mzx_world, offset);

  return remove; // Not grabbed
}

static void place_player(struct world *mzx_world, int x, int y, int dir)
{
  struct board *src_board = mzx_world->current_board;
  if((mzx_world->player_x != x) || (mzx_world->player_y != y))
  {
    id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
  }
  id_place(mzx_world, x, y, PLAYER, 0, 0);
  mzx_world->player_x = x;
  mzx_world->player_y = y;
  src_board->player_last_dir =
   (src_board->player_last_dir & 240) | (dir + 1);
}

// Returns 1 if didn't move
int move_player(struct world *mzx_world, int dir)
{
  struct board *src_board = mzx_world->current_board;
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
    if((board_dir == NO_BOARD) ||
     (board_dir >= mzx_world->num_boards) ||
     (!mzx_world->board_list[board_dir]))
    {
      return 1;
    }

    mzx_world->target_board = board_dir;
    mzx_world->target_where = TARGET_POSITION;
    mzx_world->target_x = player_x;
    mzx_world->target_y = player_y;

    switch(dir)
    {
      case 0: // North- Enter south side
      {
        mzx_world->target_y =
         (mzx_world->board_list[board_dir])->board_height - 1;
        break;
      }

      case 1: // South- Enter north side
      {
        mzx_world->target_y = 0;
        break;
      }

      case 2: // East- Enter west side
      {
        mzx_world->target_x = 0;
        break;
      }

      case 3: // West- Enter east side
      {
        mzx_world->target_x =
         (mzx_world->board_list[board_dir])->board_width - 1;
        break;
      }
    }
    src_board->player_last_dir =
     (src_board->player_last_dir & 240) + dir + 1;
    return 0;
  }
  else
  {
    // Not edge
    int d_offset = new_x + (new_y * src_board->board_width);
    enum thing d_id = (enum thing)src_board->level_id[d_offset];
    enum thing u_id = (enum thing)src_board->level_under_id[d_offset];
    int d_flag = flags[(int)d_id];

    if(d_flag & A_SPEC_STOOD)
    {
      // Sensor
      // Activate label and then move player
      int d_param = src_board->level_param[d_offset];
      send_robot(mzx_world,
       (src_board->sensor_list[d_param])->robot_to_mesg,
       "SENSORON", 0);
      place_player(mzx_world, new_x, new_y, dir);
    }
    else

    if(d_flag & A_ENTRANCE)
    {
      // Entrance
      int d_board = src_board->level_param[d_offset];
      play_sfx(mzx_world, SFX_ENTRANCE);
      // Can move?
      if((d_board != mzx_world->current_board_id) &&
       (d_board < mzx_world->num_boards) &&
       (mzx_world->board_list[d_board]))
      {
        // Go to board t1 AFTER showing update
        mzx_world->target_board = d_board;
        mzx_world->target_where = TARGET_ENTRANCE;
        mzx_world->target_color = src_board->level_color[d_offset];
        mzx_world->target_id = d_id;
      }

      place_player(mzx_world, new_x, new_y, dir);
    }
    else

    if((d_flag & A_ITEM) && (d_id != ROBOT_PUSHABLE))
    {
      // Item
      enum thing d_under_id = (enum thing)mzx_world->under_player_id;
      char d_under_color = mzx_world->under_player_color;
      char d_under_param = mzx_world->under_player_param;
      int grab_result = grab_item(mzx_world, d_offset, dir);
      if(grab_result)
      {
        if(d_id == TRANSPORT)
        {
          int player_last_dir = src_board->player_last_dir;
          // Teleporter
          id_remove_top(mzx_world, player_x, player_y);
          mzx_world->under_player_id = (char)d_under_id;
          mzx_world->under_player_color = d_under_color;
          mzx_world->under_player_param = d_under_param;
          src_board->player_last_dir =
           (player_last_dir & 240) + dir + 1;
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
      if(d_id == BULLET)
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
          play_sfx(mzx_world, SFX_HURT);
          set_mesg(mzx_world, "Ouch!");
        }
      }
      else
      {
        dec_counter(mzx_world, "HEALTH", id_dmg[(int)d_id], 0);
        play_sfx(mzx_world, SFX_HURT);
        set_mesg(mzx_world, "Ouch!");

        if(d_flag & A_ENEMY)
        {
          // Kill/move
          id_remove_top(mzx_world, new_x, new_y);

          // Not onto goop.. (under is now top)
          if(u_id != GOOP && !src_board->restart_if_zapped)
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
        if(d_id == ROBOT_PUSHABLE)
          send_robot_def(mzx_world,
           src_board->level_param[d_offset], LABEL_TOUCH);

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

void find_player(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char *level_id = src_board->level_id;
  int dx, dy, offset;

  if(mzx_world->player_x >= board_width)
    mzx_world->player_x = 0;

  if(mzx_world->player_y >= board_height)
    mzx_world->player_y = 0;

  if((enum thing)level_id[mzx_world->player_x +
   (mzx_world->player_y * board_width)] != PLAYER)
  {
    for(dy = 0, offset = 0; dy < board_height; dy++)
    {
      for(dx = 0; dx < board_width; dx++, offset++)
      {
        if((enum thing)level_id[offset] == PLAYER)
        {
          mzx_world->player_x = dx;
          mzx_world->player_y = dy;
          return;
        }
      }
    }

    replace_player(mzx_world);
  }
}
