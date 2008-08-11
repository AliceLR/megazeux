/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 2004 Gilead Kutnick
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

// game2.cpp - C port of game2.asm. Ye who enter, beware.
// The first run of this port will be fairly faithful to the inner
// mechanics of game2.asm. Optimizations will be applied later.

#include <stdlib.h>
#include <string.h>

#include "robot.h"
#include "game.h"
#include "game2.h"
#include "sfx.h"
#include "counter.h"
#include "data.h"
#include "const.h"
#include "idarray.h"
#include "world.h"

int slow_down = 0;

// For missile turning (directions)

char cwturndir[4] = { 2, 3, 1, 0 };
char ccwturndir[4] = { 3, 2, 0, 1 };

// OPEN DOOR movement directions, use bits 1,2,4,8,16 to index it.
// 0ffh=no movement.

unsigned char open_door_movement[] =
{
  3   , 0   , 2   , 0   , 3   , 1   , 2   , 1   ,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  2   , 1   , 3   , 1   , 2   , 0   , 3   , 0   ,
  1   , 2   , 1   , 3   , 0   , 2   , 0   , 3
};

// Bits for WAIT, in proper bit form, for opening doors. Use bits 1-16.

unsigned char open_door_max_wait[] =
{
  32 , 32 , 32 , 32 , 32 , 32 , 32 , 32 ,
  224, 224, 224, 224, 224, 224, 224, 224,
  224, 224, 224, 224, 224, 224, 224, 224,
  32 , 32 , 32 , 32 , 32 , 32 , 32 , 32
};

void hurt_player_id(World *mzx_world, int id)
{
  Board *src_board = mzx_world->current_board;
  int amount = id_dmg[id];
  dec_counter(mzx_world, "health", amount, 0);
  play_sfx(mzx_world, 21);
  set_mesg(mzx_world, "Ouch!");
}

// Return the seek dir relative to the player.

int find_seek(World *mzx_world, int x, int y)
{
  int dir;
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;

  if(y == player_y)
  {
    dir = 0;                // Go horizontally
  }
  else
  if(x == player_x)
  {
    dir = 1;                // Go vertically
  }
  else
  {
    int r_num = rand();
    dir = r_num & 1;        // Go random horizontal or vertical
  }

  if(dir)
  {
    // Horizontal movement
    if(player_y < y)
    {
      return 0;
    }
    else
    {
      return 1;
    }
  }
  else
  {
    // Vertical movement
    if(player_x < x)
    {
      return 3;
    }
    else
    {
      return 2;
    }
  }
}

int flip_dir(int dir)
{
  dir ^= 1;                 // Toggle the lsb to change direction
  return dir;
}

// Increases current parameter and stores

int inc_param(int param, int max)
{
  if(param == max)
  {
    return 0;
  }
  else
  {
    return param + 1;
  }
}

// Function to take an x/y position and return an array offset (within the board)

int xy2array2(Board *src_board, int x, int y)
{
  return (y * src_board->board_width) + x;
}

// Take an x/y position and a direction and return a new x/y position (in
// what ret_x/ret_y point to)
// Returns -1 if offscreen, 0 otherwise.

int arraydir2(Board *src_board, int x, int y, int *ret_x, int *ret_y,
 int direction)
{
  int new_x = x, new_y = y;

  switch(direction)
  {
    case 0:
    // North
      new_y--;
      break;
    case 1:
    // South
      new_y++;
      break;
    case 2:
    // East
      new_x++;
      break;
    case 3:
    // West
      new_x--;
      break;
  }
  if((new_x == src_board->board_width) ||
   (new_y == src_board->board_height) || (new_x == -1) || (new_y == -1))
  {
    return -1;
  }

  *ret_x = new_x;
  *ret_y = new_y;

  return 0;
}

// This is the big one. Update all of the stuff on the screen..

void update_board(World *mzx_world)
{
  int i;
  int x, y;
  int level_offset;
  int last_offset;
  Board *src_board = mzx_world->current_board;
  Robot *cur_robot;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char current_id;
  char current_param;
  char current_color;
  char current_under_id;

  // Toggle slow_down
  slow_down ^= 1;

  // Clear the status code of all robots
  for(i = 0; i < src_board->num_robots_active; i++)
  {
    cur_robot = src_board->robot_list_name_sorted[i];
    cur_robot->status = 0;
  }

  memset(update_done, 0, board_width * board_height);

  // The big update loop
  for(y = 0, level_offset = 0; y < board_height; y++)
  {
    for(x = 0; x < board_width; x++, level_offset++)
    {
      current_id = level_id[level_offset];

      // If the char's update done value is set or the id is < 25
      // (space trough W water) then there's nothing to do here; go to the next one.
      if((update_done[level_offset] & 1) || (current_id < 25) ||
       !(flags[current_id] & A_UPDATE)) continue;

      current_param = level_param[level_offset];

      switch(current_id)
      {
        // Robot
        case 123:
        case 124:
        {
          run_robot(mzx_world, current_param, x, y);

          if(mzx_world->swapped)
          {
            // Swapped world; get out of here
            return;
          }
          break;
        }
        // Ice
        case 25:
        {
          // Start ice animation
          if(current_param == 0)
          {
            int rval = rand() & 0xFF;
            if(rval == 0)
            {
              // Start anim at 1
              level_param[level_offset] = 1;
            }
          }
          else

          // End ice animation
          if(current_param == 3)
          {
            // Reset animation
            level_param[level_offset] = 0;
          }
          else
          {
            // Increase animation
            level_param[level_offset] = current_param + 1;
          }

          break;
        }
        // Lava
        case 26:
        {
          int r_val;

          if(slow_down) break;

          r_val = rand() & 0xFF;

          if(r_val >= 20)
          {
            if(current_param == 2)
            {
              level_param[level_offset] = 0;
            }
            else
            {
              level_param[level_offset] = current_param + 1;
            }
          }
          break;
        }
        // Fire
        case 63:
        {
          // Get a random number
          int rval = rand() & 0xFF;
          int new_x, new_y;
          // If it's less than 20, don't animate
          if(rval >= 20)
          {
            // If not at the end of the animation, increment it
            if(current_param != 5)
            {
              level_param[level_offset] = current_param + 1;
            }
            else
            {
              // Reset the animation
              level_param[level_offset] = 0;
            }
          }

          // Check under for water, ice, or lava; these all kill the fire.

          current_under_id = level_under_id[level_offset];
          if((current_under_id >= 20) && (current_under_id <= 26))
          {
            // Ice also melts into stillwater
            if(current_under_id == 25)
            {
              level_under_id[level_offset] = 20;
              level_under_color[level_offset] = 25;
            }
            id_remove_top(mzx_world, x, y);
            break;
          }

          // Otherwise the fire hurts and spreads and stuff, maybe dies
          // Random number says it might die or spread
          rval = rand() & 0xFF;
          // No spread here
          if(rval > 8) break;
          // If it's 1, it should die, if fire doesn't burn forever
          // You would think dead fire can't spread, but in MZX it can!
          // Makes you wonder what the point of "fire burns forever" is.
          // This should possibly be changed later.
          if((rval == 1) && (src_board->fire_burns != FIRE_BURNS_FOREVER))
          {
            // Fire turns into dark grey ash
            level_id[level_offset] = 15;
            level_color[level_offset] = 8;
          }

          // Put fire all around
          for(i = 3; i >= 0; i--)
          {
            if(!arraydir2(src_board, x, y, &new_x, &new_y, i))
            {
              // Get offset
              int offset = xy2array2(src_board, new_x, new_y);
              int place = 0;
              // Save ID and param there
              unsigned char new_id = level_id[offset];
              unsigned char new_color = level_color[offset];

              // Fire does things to certain ID's
              // Space
              if((new_id == 0) && (src_board->fire_burn_space == 1))
                place = 1;

              // Fake
              if((new_id >= 13) && (new_id <= 19) &&
               (src_board->fire_burn_fakes == 1))
                place = 1;

              if((new_id == 3) &&
               (src_board->fire_burn_trees == 1))
                place = 1;

              // Brown stuff
              // Don't burn scrolls, signs, sensors, robots, or the player
              if((new_color == 6) &&
               (src_board->fire_burn_brown == 1) && (new_id < 122))
                place = 1;

              // Player
              if(new_id == 127)
              {
                // Make sure player can't.. walk on fire.
                if(mzx_world->firewalker_dur == 0)
                {
                  hurt_player_id(mzx_world, 63);
                }
              }

              if(place == 1)
              {
                // Place a new fire
                id_place(mzx_world, new_x, new_y, 63, 0, 0);
              }
            }
          }

          break;
        }
        // Bullet
        case 61:
        {
          int bullettype = current_param >> 2;
          int direction = current_param & 0x03;
          // Erase at the current position
          id_remove_top(mzx_world, x, y);
          // Place a new one
          shoot(mzx_world, x, y, direction, bullettype);

          break;
        }
        // Explosion
        case 38:
        {
          int stage = current_param & 0x0F;
          // Stage zero means climb outwards
          if(!stage)
          {
            // Move the explosion outward if it has any more to go.
            if(current_param & 0xF0)
            {
              int new_x, new_y;
              // Decrease stage
              current_param -= 0x10;
              level_param[level_offset] = current_param;

              // Put explosion at each direction
              for(i = 3; i >= 0; i--)
              {
                if(!arraydir2(src_board, x, y, &new_x, &new_y, i))
                {
                  // Get offset
                  int offset = xy2array2(src_board, new_x, new_y);
                  // Save ID and param there
                  unsigned char new_id = level_id[offset];
                  unsigned char new_param = level_param[offset];
                  // Get the flags for that ID
                  int flag = flags[new_id];

                  // Should a new explosion be placed here?
                  // Also, some further things can happen.
                  // Does what's there..
                  // Blow up (removed by explosion)?
                  // Slimes, ghosts, and dragons might also die if the bit isn't set
                  if((flag & A_BLOW_UP) ||
                   ((new_id == 83) && !(new_param & 0x80)) ||
                   ((new_id == 85) && !(new_param & 0x08)) ||
                   ((new_id == 86) && !(new_param & 0xE0)))
                  {
                    // Remove what's there
                    id_remove_top(mzx_world, new_x, new_y);
                    // Place a new explosion (one level down)
                    id_place(mzx_world, new_x, new_y, 38, 0, current_param);
                    continue;
                  }

                  // Explode? (create a new explosion)
                  if(flag & A_EXPLODE)
                  {
                    // Remove what's there
                    id_remove_top(mzx_world, new_x, new_y);
                    // Place a fresh new explosion
                    id_place(mzx_world, new_x, new_y, 38, 0, 64);
                    continue;
                  }

                  // If it can be pushed under, place an explosion over it
                  if(flag & A_UNDER)
                  {
                    // Place a new explosion over it.
                    id_place(mzx_world, new_x, new_y, 38, 0, current_param);
                    continue;
                  }

                  // Now, see if it's something that reacts to explosions
                  // Player takes damage
                  if(new_id == 127)
                  {
                    hurt_player_id(mzx_world, 38);
                    continue;
                  }

                  // Mine explodes
                  if(new_id == 74)
                  {
                    // Take explosion radius from param
                    // Remove what's there
                    id_remove_top(mzx_world, new_x, new_y);
                    // Place a fresh new explosion
                    id_place(mzx_world, new_x, new_y, 38, 0, new_param & 0xF0);
                    continue;
                  }

                  // Robot gets sent a label
                  if((new_id == 123) || (new_id == 124))
                  {
                    // Send bombed label
                    send_robot_def(mzx_world, new_param, 1);
                    continue;
                  }

                  // Dragon takes damage, if it wasn't already killed
                  if(new_id == 86)
                  {
                    // Decrease HP
                    level_param[offset] = new_param - 0x20;
                    continue;
                  }
                }
              }
            }
          }

          // See if it's at stage 3, if so it's done
          if(stage == 3)
          {
            // Get what's underneath it
            current_under_id = level_under_id[level_offset];
            // Leave space if over goop, water, lava, or ice
            if((current_under_id == 34) ||
             ((current_under_id >= 20) && (current_under_id <= 26)))
            {
              // Leave space
              id_remove_top(mzx_world, x, y);
              break;
            }

            // Is under an entrance? Explosion leaves space? Leave space.
            if((flags[current_under_id] & A_ENTRANCE)
             || src_board->explosions_leave == EXPL_LEAVE_SPACE)
            {
              // Leave space
              id_remove_top(mzx_world, x, y);
              break;
            }
            // Leave ash?
            if(src_board->explosions_leave == EXPL_LEAVE_ASH)
            {
              // Leave ash if the params say so
              level_id[level_offset] = 15;
              level_color[level_offset] = 8;
              break;
            }
            // Otherwise leave fire
            level_id[level_offset] = 63;
            level_param[level_offset] = 0;
          }
          else
          {
            // Otherwise go to the next stage
            level_param[level_offset] = current_param + 1;
          }
          break;
        }
        // Cw/Ccw
        case 45:
        case 46:
        {
          if(slow_down)
            break;
          level_param[level_offset] = inc_param(current_param, 3);
          rotate(mzx_world, x, y, current_id - 45);
          break;
        }
        // Transport
        case 49:
        {
          if(slow_down)
            break;
          // Is it still animating?
          if((current_param & 0x18) != 0x18)
          {
            // Increase animation
            level_param[level_offset] = current_param + 0x08;
          }
          else
          {
            // Otherwise, erase anim bits
            level_param[level_offset] = current_param & 0xE7;
          }
          break;
        }

        // Shooting fire
        case 78:
        {
          int move_status;
          // Get direction
          int direction = current_param >> 1;
          // Flip animation and store
          current_param ^= 1;
          level_param[level_offset] = current_param;
          // Try moving

          move_status = move(mzx_world, x, y, direction,
           4 | 8 | 16 | 128 | 1024 | 2048);

          if(move_status == 2)
          {
            // Hit player; hurt the player and die
            hurt_player_id(mzx_world, 78);
            id_remove_top(mzx_world, x, y);
          }
          else
          {
            if(move_status)
            {
              // Didn't hit the player.. check stuff
              current_under_id = level_under_id[level_offset];
              // See if it hit an entrance
              if((current_under_id == 43) || (current_under_id == 44) ||
              ((current_under_id >= 67) && (current_under_id <= 70)))
              {
                id_remove_top(mzx_world, x, y);
              }
              else
              {
                // Put fire
                level_id[level_offset] = 63;
                level_param[level_offset] = 0;
              }
            }
          }

          break;
        }
        // Missile
        case 62:
        {
          // Param is the direction
          int move_status = move(mzx_world, x, y, current_param,
           4 | 8 | 16 | 128 | 1024);
          // Did it hit something that's not the player?
          if((move_status == 1) || (move_status == 3))
          {
            // Otherwise change direction; try cw then ccw
            int new_direction = cwturndir[current_param];
            level_param[level_offset] = new_direction;
            move_status = move(mzx_world, x, y, new_direction,
             4 | 8 | 16 | 128 | 1024);
            // Did it hit something that's not the player? Try ccw.
            if((move_status == 1) || (move_status == 3))
            {
              new_direction = ccwturndir[current_param];
              level_param[level_offset] = new_direction;
              move_status = move(mzx_world, x, y, new_direction,
               4 | 8 | 16 | 128 | 1024);
              if(move_status) move_status = 2;
            }
          }

          // Did it hit the player?
          if(move_status == 2)
          {
            // If so, leave explosion
            level_id[level_offset] = 38;
            level_param[level_offset] = 48;
            play_sfx(mzx_world, 36);
          }

          break;
        }
        // Seeker
        case 79:
        {
          int seek_dir;

          if(slow_down) break;
          if(current_param == 0)
          {
            id_remove_top(mzx_world, x, y);
          }
          else
          {
            level_param[level_offset] = current_param - 1;
            seek_dir = find_seek(mzx_world, x, y);
            if(move(mzx_world, x, y, seek_dir, 1 | 4 | 8 | 16 | 128 | 1024) == 2)
            {
              hurt_player_id(mzx_world, 79);
              id_remove_top(mzx_world, x, y);
            }
          }
          break;
        }
        // "Lazer"
        case 59:
        {
          // Decrease time until death
          // If it's less than 8, kill it
          current_param -= 8;
          if(current_param < 8)
          {
            id_remove_top(mzx_world, x, y);
          }
          else
          {
            if(slow_down) break;
            // Animate every other cycle
            if(current_param & 0x06)
            {
              // Otherwise, erase anim bits
              level_param[level_offset] = current_param & 0xF9;
            }
            else
            {
              // Increase animation
              level_param[level_offset] = current_param + 2;
            }
          }
          break;
        }
        // Pusher
        case 56:
        {
          if(slow_down) break;
          move(mzx_world, x, y, current_param, 1);

          break;
        }
        // Snake
        case 80:
        {
          int move_status;

          // Flip count and store
          current_param ^= 8;
          level_param[level_offset] = current_param;

          // If the flipflop or fast-movement is set, move
          if((current_param & 0x08) || !(current_param & 0x04))
          {
            // Try move
            move_status = move(mzx_world, x, y, current_param & 0x03, 128);
            // See if it hit the player
            if(move_status == 2)
            {
              // If so, take damage and die
              hurt_player_id(mzx_world, 80);
              id_remove_top(mzx_world, x, y);
              break;
            }

            // Otherwise, if it didn't move, get a new direction
            if(move_status)
            {
              int m_dir;

              int intelligence = (current_param & 0x70) >> 4;
              // Mask out direction
              current_param &= 0xFC;
              // Get number between 0 and 7
              int rval = rand() & 7;

              // If it's less than intelligence, make a "smart" move
              if(rval < intelligence)
              {
                m_dir = find_seek(mzx_world, x, y);
              }
              else
              {
                // Get a number between 0 and 3
                m_dir = rand() & 3;
              }

              // Set direction
              current_param |= m_dir;
              level_param[level_offset] = current_param;
            }
          }
          break;
        }
        // Eye
        case 81:
        {
          // Flip count and store
          current_param ^= 0x80;
          level_param[level_offset] = current_param;

          // If the flipflop or fast-movement is set, move
          if((current_param & 0x80) || !(current_param & 0x40))
          {
            int intelligence = current_param & 7;
            int rval = rand() & 7;
            int m_dir;

            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              // Get a number between 0 and 3
              m_dir = rand() & 3;
            }

            if(move(mzx_world, x, y, m_dir, 4 | 8 | 16 | 128 | 1024) == 2)
            {
              // Hit the player. Get the blast radius.
              int radius = (current_param & 0x38) << 1;
              // Explode (place explosion)
              level_id[level_offset] = 38;
              level_param[level_offset] = radius;
              play_sfx(mzx_world, 36);
            }
          }
          break;
        }
        // Thief
        case 82:
        {
          // Movement rate, 1-4
          int move_speed = current_param & 0x18;
          // Movement counter
          int move_cycle = (current_param & 0x60) >> 2;

          // Should the thief move?
          if(move_cycle == move_speed)
          {
            // Zero out move cycle
            level_param[level_offset] = current_param & 0x9F;
            // Get intelligence
            int intelligence = current_param & 0x07;
            // Get a random number 0 through 7
            int rval = rand() & 7;
            int m_dir;

            // See if a "smart move" should be made
            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = rand() & 3;
            }

            // Move and see if it hit the player
            if(move(mzx_world, x, y, m_dir, 128) == 2)
            {
              // Get amount of gems to take
              int gems_take = ((current_param & 0x80) >> 7) + 1;
              // Take them
              dec_counter(mzx_world, "GEMS", gems_take, 0);
              play_sfx(mzx_world, 44);
            }
          }
          else
          {
            // Increment move cycle
            level_param[level_offset] = current_param + 0x20;
          }

          break;
        }
        // Slime
        case 83:
        {
          if(slow_down) break;

          // This is pretty weird, but I have to go by the ASM for now
          int spread_speed = current_param & 0x03;
          int current_cycle = (current_param &  0x3C) >> 2;

          spread_speed |= spread_speed << 1;

          if(spread_speed == current_cycle)
          {
            int new_x, new_y;
            current_color = level_color[level_offset];
            // Clear cycle
            current_param &= 0xC7;

            // Put slimes all around
            for(i = 3; i >= 0; i--)
            {
              if(!arraydir2(src_board, x, y, &new_x, &new_y, i))
              {
                // Get offset
                int offset = xy2array2(src_board, new_x, new_y);
                // Save ID and param there
                unsigned char new_id = level_id[offset];
                // See if it hits a fake
                if((new_id == 0) || ((new_id >= 13) && (new_id <= 19)))
                {
                  // Put a slime
                  id_place(mzx_world, new_x, new_y, 83, current_color, current_param);
                }
                else
                {
                  // See if it hits the player and hurts the player
                  if((new_id == 127) && (current_param & 0x40))
                  {
                    // Take damage
                    hurt_player_id(mzx_world, 83);
                  }
                }
                // Put a breakaway
                id_place(mzx_world, x, y, 6, current_color, 0);
              }
            }
          }
          else
          {
            // Increase cycle
            level_param[level_offset] = current_param + 4;
          }

          break;
        }
        // Runner
        case 84:
        {
          int speed = current_param & 0x0C;
          int cycle = (current_param & 0x30) >> 2;

          if(cycle == speed)
          {
            // Get direction
            int direction = current_param & 0x03;
            int move_status;
            // Clear cycle
            level_param[level_offset] = current_param & 0xCF;

            move_status = move(mzx_world, x, y, direction, 1 | 2 | 128);
            // Did the move not go through?
            if(move_status)
            {
              // Did it hit the player?
              if(move_status == 2)
              {
                // Hurt the player and die
                hurt_player_id(mzx_world, 84);
                id_remove_top(mzx_world, x, y);
                break;
              }
              else
              {
                // Change direction
                level_param[level_offset] = current_param ^ 1;
              }
            }
          }
          else
          {
            // Increase cycle
            level_param[level_offset] = current_param + 0x10;
          }

          break;
        }
        // Ghost
        case 85:
        {
          int speed = current_param & 0x30;
          int cycle = (current_param & 0xC0) >> 2;

          if(speed == cycle)
          {
            // Get intelligence
            int intelligence = current_param & 0x07;
            int rval = rand() & 7;
            int m_dir;
            // Clear cycle
            level_param[level_offset] = current_param & 0x3F;

            if(rval < intelligence)
            {
              // Smart move
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = rand() & 3;
            }

            // Try move, did it hit the player?
            if(move(mzx_world, x, y, m_dir, 4 | 8 | 16 | 128 | 1024) == 2)
            {
              // Take damage
              hurt_player_id(mzx_world, 85);
              // Only die if not invincible
              if(!(current_param & 0x08))
              {
                id_remove_top(mzx_world, x, y);
                break;
              }
            }
          }
          else
          {
            // Increase cycle
            level_param[level_offset] = current_param + 0x40;
          }

          break;
        }
        // Dragon
        case 86:
        {
          int fire_rate;
          int rval;

          // Can it move?
          if(current_param & 0x04)
          {
            int cycle = current_param & 0x18;
            if(cycle == 0x18)
            {
              int m_dir;
              int move_status;
              // Zero out movement
              level_param[level_offset] = current_param & 0xE7;
              // One out of 8 moves is random

              if(rval & 0x07)
              {
                m_dir = rand() & 0x03;
              }
              else
              {
                // Otherwise make a smart move
                m_dir = find_seek(mzx_world, x, y);
              }

              move_status = move(mzx_world, x, y, m_dir, 4 | 8 | 128);

              // Is it blocked?
              if(move_status)
              {
                // Did it hit the player?
                if(move_status == 2)
                {
                  // Dragons don't die when hit by the player
                  hurt_player_id(mzx_world, 86);
                }

                // Can't shoot, so get out of here
                break;
              }
            }
            else
            {
              level_param[level_offset] = current_param + 8;
            }
          }

          // Shoot; get the fire rate
          fire_rate = current_param & 0x03;
          rval = rand() & 0x0F;

          // Should it fire?
          if(rval < fire_rate)
          {
            // Shoot in the direction of the player
            int fire_dir = find_seek(mzx_world, x, y);
            shoot_fire(mzx_world, x, y, fire_dir);
            play_sfx(mzx_world, 46);
          }

          break;
        }
        // Fish
        case 87:
        {
          unsigned int m_dir;

          // Is the cycle count ready or is fast movement on?
          if((current_param & 0x10) || !(current_param & 0x08))
          {
            m_dir = level_under_id[level_offset] - 21;

            // Toggle cycle flipflop off
            level_param[level_offset] = current_param & 0xEF;

            // Is there not water there or is it not affected by it?
            if(!(current_param & 0x20) || (m_dir > 3))
            {
              int intelligence = current_param & 0x07;
              int rval = rand() & 0x07;
              if(rval < intelligence)
              {
                m_dir = find_seek(mzx_world, x, y);
              }
              else
              {
                m_dir = rand() & 0x03;
              }
            }

            // Move. Did it hit the player and does the player hurt fish?
            if((move(mzx_world, x, y, m_dir, 16 | 128 | 256) == 2) &&
             (current_param & 0x40))
            {
              hurt_player_id(mzx_world, 87);
              id_remove_top(mzx_world, x, y);
            }
          }
          else
          {
            // Toggle cycle flipflop on
            level_param[level_offset] = current_param | 0x10;
          }

          break;
        }
        // Shark
        case 88:
        {
          int intelligence;
          int rval;
          int m_dir;
          int move_status;
          int new_x, new_y;
          int fire_rate;
          int shoot_type;

          if(slow_down) break;

          intelligence = current_param & 0x07;
          rval = rand() & 0x07;

          if(rval < intelligence)
          {
            m_dir = find_seek(mzx_world, x, y);
          }
          else
          {
            m_dir = rand() & 3;
          }

          // Hit player and die
          move_status = move(mzx_world, x, y, m_dir, 512 | 128 | 4);

          if(move_status == 2)
          {
            hurt_player_id(mzx_world, 88);
            id_remove_top(mzx_world, x, y);
            break;
          }

          // Should shoot in the direction it moved.

          // If blocked, keep old x/y, otherwise update
          if(move_status)
          {
            new_x = x;
            new_y = y;
          }
          else
          {
            arraydir2(src_board, x, y, &new_x, &new_y, m_dir);
          }

          // Get shark fire rate
          fire_rate = (current_param & 0xE0) >> 5;
          // Get the type of thing it shoots
          shoot_type = current_param & 0x18;
          // Get a number between 0 and 31
          rval = rand() & 0x1F;

          // Is the random number <= the rate? Also, does it shoot?
          if((rval <= fire_rate) && (shoot_type != 24))
          {
            // Shoot seeker
            if(shoot_type == 8)
            {
              shoot_seeker(mzx_world, new_x, new_y, m_dir);
            }
            else

            // Shoot fire
            if(shoot_type == 16)
            {
              shoot_fire(mzx_world, new_x, new_y, m_dir);
              play_sfx(mzx_world, 46);
            }
            else
            {
              // Shoot bullet
              shoot(mzx_world, new_x, new_y, m_dir, ENEMY_BULLET);
            }
          }

          break;
        }
        // Spider
        case 89:
        {
          // Toggle cycle flipflop
          current_param ^= 0x40;
          level_param[level_offset] = current_param;

          // Is the cycle count ready or is fast movement on?
          if((current_param & 0x40) || !(current_param & 0x20))
          {
            int intelligence = current_param & 0x07;
            int rval = rand() & 7;
            int m_dir;
            // Set these flags
            int flags = 128;

            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = rand() & 3;
            }
            flags |= ((current_param & 0x18) << 2) + 32;

            if(move(mzx_world, x, y, m_dir, flags) == 2)
            {
              hurt_player_id(mzx_world, 89);
              id_remove_top(mzx_world, x, y);
            }
          }

          break;
        }
        // Goblin
        case 90:
        {
          int move_rate;
          int move_cycle;

          if(slow_down) break;

          // Should it pause?
          if(current_param & 0x20)
          {
            move_rate = ((current_param & 0x03) << 1) + 1;
            move_cycle = ((current_param & 0x1C) >> 2);

            if(move_rate == move_cycle)
            {
              // Zero out move cycle and move bit
              level_param[level_offset] = current_param & 0xC3;
            }
            else
            {
              // Increase move cycle
              level_param[level_offset] = current_param + 4;
            }
          }
          else
          {
            int intelligence = (current_param & 0xC0) >> 6;
            int rval = rand() & 3;
            int m_dir;

            // Otherwise, move
            current_param += 4;
            // If the non-pause cycle is maxed out
            if((current_param & 0x1C) == 0x14)
            {
              // Pause next time
              current_param = (current_param & 0xE3) | 0x20;
            }

            level_param[level_offset] = current_param;

            if(rval < intelligence)
            {
              m_dir = find_seek(mzx_world, x, y);
            }
            else
            {
              m_dir = rand() & 3;
            }

            // Try to move, does it hit the player, etc.
            if(move(mzx_world, x, y, m_dir, 16 | 128) == 2)
            {
              hurt_player_id(mzx_world, 90);
              id_remove_top(mzx_world, x, y);
            }
          }

          break;
        }
        // Tiger
        case 91:
        {
          int intelligence;
          int rval;
          int m_dir;
          int move_status;
          int new_x, new_y;
          int fire_rate;
          int shoot_type;

          if(slow_down) break;

          intelligence = current_param & 0x07;
          rval = rand() & 7;

          if(rval < intelligence)
          {
            m_dir = find_seek(mzx_world, x, y);
          }
          else
          {
            m_dir = rand() & 7;
          }

          // Hit player and die
          move_status = move(mzx_world, x, y, m_dir, 16 | 128);

          if(move_status == 2)
          {
            hurt_player_id(mzx_world, 91);
            id_remove_top(mzx_world, x, y);
            break;
          }

          // Should shoot in the direction it moved.

          // If blocked, keep old x/y, otherwise update
          if(move_status)
          {
            new_x = x;
            new_y = y;
          }
          else
          {
            arraydir2(src_board, x, y, &new_x, &new_y, m_dir);
          }

          // Get tiger fire rate
          fire_rate = (current_param & 0xE0) >> 5;
          // Get the type of thing it shoots
          shoot_type = current_param & 0x18;
          // Get a number between 0 and 31
          rval = rand() & 0x1F;

          // Is the random number <= the rate? Also, does it shoot?
          if((rval <= fire_rate) && (shoot_type != 24))
          {
            // Shoot seeker
            if(shoot_type == 8)
            {
              shoot_seeker(mzx_world, new_x, new_y, m_dir);
            }
            else

            // Shoot fire
            if(shoot_type == 16)
            {
              shoot_fire(mzx_world, new_x, new_y, m_dir);
            }
            else
            {
              // Shoot bullet
              shoot(mzx_world, new_x, new_y, m_dir, ENEMY_BULLET);
            }
          }

          break;
        }
        // Spinning gun
        case 93:
        {
          // A spinnning gun is a bulletgun that spins. So it can fall through for
          // now, but this might get changed with refactoring..
          if(slow_down) break;

          // Update animation and direction

          if((current_param & 0x18) == 0x18)
          {
            current_param &= 0xE7;
          }
          else
          {
            current_param += 8;
          }

          level_param[level_offset] = current_param;

          // Fall through
        }
        // Bulletgun
        case 92:
        {
          int shoot_rate;
          int rval;

          if(slow_down) break;

          shoot_rate = current_param & 0x07;
          rval = rand() & 0x0F;

          // Shoot now.. maybe
          if(rval <= shoot_rate)
          {
            int intelligence = current_param & 0x60;
            int should_shoot = 0;
            int direction = (current_param & 0x18) >> 3;
            rval = rand() & 0x60;

            // Is the player aligned with the gun?
            if(find_seek(mzx_world, x, y) == direction)
            {
              // If so, should shoot
              should_shoot = 1;
            }

            // Unless the gun is stupid, then do the opposite
            if(rval > intelligence)
            {
              should_shoot ^= 1;
            }

            if(should_shoot)
            {
              // See what to shoot
              if(current_param & 0x80)
              {
                // Shoot fire
                shoot_fire(mzx_world, x, y, direction);
              }
              else
              {
                // Shoot bullet
                shoot(mzx_world, x, y, direction, ENEMY_BULLET);
              }
            }
          }

          break;
        }
        // Bear
        case 94:
        {
          int move_rate = current_param & 0x18;
          int move_cycle = (current_param & 0x60) >> 2;

          if(move_rate == move_cycle)
          {
            int sensitivity = ((current_param & 0x07) + 1) << 1;
            int player_distance;
            int player_dist_x = mzx_world->player_x - x;
            int player_dist_y = mzx_world->player_y - y;

            // Zero out move cycle
            level_param[level_offset] &= 0x9F;

            if(player_dist_x < 0) player_dist_x = -player_dist_x;
            if(player_dist_y < 0) player_dist_y = -player_dist_y;
            player_distance = player_dist_x + player_dist_y;

            if(player_distance <= sensitivity)
            {
              // Move in the player's direction, always
              if(move(mzx_world, x, y, find_seek(mzx_world, x, y), 16 | 128)
               == 2)
              {
                hurt_player_id(mzx_world, 94);
                id_remove_top(mzx_world, x, y);
              }
            }
          }
          else
          {
            level_param[level_offset] = current_param + 0x20;
          }

          break;
        }
        // Cub
        case 95:
        {
          int switch_rate;
          int switch_cycle;
          int intelligence;
          int rval;
          int m_dir;

          if(slow_down) break;

          switch_rate = ((current_param & 0x0C) >> 1) + 1;
          switch_cycle = (current_param & 0x70) >> 4;

          // Should it switch from chasing/running away from the player?
          if(switch_cycle == switch_rate)
          {
            // Zero out the switch cycle
            current_param &= 0x8F;
            // Toggle the switch
            current_param ^= 0x80;
          }
          else
          {
            // Increase switch time
            current_param += 16;
          }

          level_param[level_offset] = current_param;

          intelligence = current_param & 0x03;
          rval = rand() & 3;

          if(rval < intelligence)
          {
            m_dir = find_seek(mzx_world, x, y);
          }
          else
          {
            m_dir = rand() & 3;
          }

          // Should it run in that direction or the opposite one?
          if(current_param & 0x80)
          {
            m_dir = flip_dir(m_dir);
          }

          if(move(mzx_world, x, y, m_dir, 16 | 128) == 2)
          {
            hurt_player_id(mzx_world, 95);
            id_remove_top(mzx_world, x, y);
          }

          break;
        }
        // Energizer:
        case 33:
        {
          level_param[level_offset] = inc_param(current_param, 7);
          break;
        }
        // Litbomb
        case 37:
        {
          if(slow_down) break;
          if((current_param & 6) == 6)
          {
            // Is it a highbomb?
            if(current_param & 0x80)
            {
              level_param[level_offset] = 0x40;
            }
            else
            {
              level_param[level_offset] = 0x20;
            }

            level_id[level_offset] = 38;
            play_sfx(mzx_world, 36);
          }
          else
          {
            level_param[level_offset] = current_param + 1;
          }
          break;
        }
        // Opendoor
        case 42:
        {
          int curr_wait = current_param & 0xE0;
          // Get the door stage
          int stage = current_param & 0x1F;
          int door_wait = open_door_max_wait[stage];
          int door_move = open_door_movement[stage];

          if(curr_wait == door_wait)
          {
            // Is it at stage 3?
            if((current_param & 0x18) == 0x18)
            {
              // Turn into a regular door
              level_param[level_offset] = current_param & 0x07;
              level_id[level_offset] = 41;
            }
            else
            {
              // Otherwise, add to, reset the wait the stage
              level_param[level_offset] = stage + 8;
            }

            // Only do this if movement is possible
            if(door_move != 0xFF)
            {
              if(move(mzx_world, x, y, door_move, 1 | 4 | 8 | 16))
              {
                // Reset the param and make the door open
                level_id[level_offset] = 42;
                level_param[level_offset] = current_param;
              }
            }
          }
          else
          {
            level_param[level_offset] = current_param + 0x20;
          }
          break;
        }
        // Opengate
        case 48:
        {
          // End of wait? Close it.
          if(current_param == 0)
          {
            // Make it closed gate
            level_id[level_offset] = 47;
            play_sfx(mzx_world, 25);
          }
          else
          {
            // Decrease wait
            level_param[level_offset] = current_param - 1;
          }

          break;
        }
        // Moving wall
        case 51:
        case 52:
        case 53:
        case 54:
        {
          // Get direction
          current_id -= 51;
          // Try the move
          if(move(mzx_world, x, y, current_id, 1 | 2 | 4 | 8 | 16))
          {
            // Can't move; try other direction
            level_id[level_offset] = flip_dir(current_id) + 51;
          }
          break;
        }
        // "Lazer" gun
        case 60:
        {
          // Look at the param
          int start_time = (current_param & 0x1C) >> 2;
          // Must be the correct start time
          if(start_time == src_board->lazwall_start)
          {
            current_color = level_color[level_offset];
            unsigned char length = ((current_param & 0xE0) >> 5) + 1;
            unsigned char direction = (current_param & 0x03);
            shoot_lazer(mzx_world, x, y, direction, length, current_color);
          }
          break;
        }
        // Life
        case 66:
        {
          if(slow_down) break;
          level_param[level_offset] = inc_param(current_param, 3);
          break;
        }
        // Whirlpool
        case 67:
        case 68:
        case 69:
        case 70:
        {
          if(slow_down) break;
          // Is it the last phase?
          if(current_id == 70)
          {
            // Loop back
            level_id[level_offset] = 67;
          }
          else
          {
            // Increase "frame"
            level_id[level_offset] = current_id + 1;
          }

          break;
        }
        // Mine
        case 74:
        {
          // Should it go to the next phase?
          if((current_param & 0x0E) != 0x0E)
          {
            level_param[level_offset] = current_param + 2;
          }
          else
          {
            // Animate
            level_param[level_offset] = (current_param & 0xF1) ^ 1;
          }
          break;
        }
        // Missile gun
        case 97:
        {
          // Apparently, missile guns have intelligence, but this
          // doesn't even affect anything. Hrm...

          int fire_rate;
          int m_dir;
          int rval;

          if(slow_down) break;

          fire_rate = current_param & 0x07;
          rval = rand() & 0x20;
          m_dir = find_seek(mzx_world, x, y);

          // Fire rate of seven means always shoot
          // Only shoot if the gun is facing the player
          if(((rval < fire_rate) || (fire_rate == 7)) &&
           (m_dir == ((current_param >> 3) & 0x03)))
          {
            shoot_missile(mzx_world, x, y, m_dir);
            // Should it die now?
            if(!(current_param & 0x80))
            {
              id_remove_top(mzx_world, x, y);
            }
          }

          break;
        }
      }
    }
  }

  // Run all of the robots _again_, this time in reverse order.
  level_offset = (board_width * board_height) - 1;

  for(y = board_height - 1; y >= 0; y--)
  {
    last_offset = level_offset;
    for(x = board_width - 1; x >= 0; x--)
    {
      current_id = level_id[level_offset];
      if((current_id == 123) || (current_id == 124))
      {
        current_param = level_param[level_offset];
        // May change the source board (with swap world or load game)
        run_robot(mzx_world, -current_param, x, y);
        src_board = mzx_world->current_board;
      }
      level_offset--;
    }
  }

  find_player(mzx_world);
}

void shoot_lazer(World *mzx_world, int x, int y, int dir, int length,
 int color)
{
  Board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_color = src_board->level_color;
  char *level_param = src_board->level_param;

  int lx = x;
  int ly = y;
  int offset;
  unsigned char id;
  unsigned char param;

  while(1)
  {
    if(arraydir2(src_board, lx, ly, &lx, &ly, dir))
    {
      // Out of bounds
      return;
    }

    offset = xy2array2(src_board, lx, ly);
    id = level_id[offset];

    // Player
    if(id == 127)
    {
      hurt_player_id(mzx_world, 59);

      // Restart if zapped?
      if(src_board->restart_if_zapped != 1)
      {
        int p_dir = (src_board->player_last_dir) & 0x0F;
        if(p_dir != 0)
        {
          // Move the player in the opposite direction
          move(mzx_world, lx, ly, flip_dir((p_dir - 1)), 1 | 2 | 16);
        }
      }

      break;
    }
    else

    // Robot
    if((id == 123) || (id == 124))
    {
      param = level_param[offset];
      // Send the robot the lazer label
      send_robot_def(mzx_world, param, 8);

      break;
    }
    else
    // Otherwise...
    {
      int flag = flags[id];
      unsigned char n_param;
      // Blocked. Get out of here
      if(!(flag & A_UNDER)) return;

      // Combine length and dir in lazer param
      n_param = length << 3;

      // Check vertical/horizontal
      if(dir <= 1)
      {
        n_param |= 1;
      }

      id_place(mzx_world, lx, ly, 59, color, n_param);
    }
  }
}

// Push/transport currently have no recursion protection. As far as
// I'm aware, there's only so large you can make a push/transport chain
// (at most around 400). This is probably too much for an MS-DOS stack,
// but any decent 32bit OS should be able to tolerate it.

// See if a transport can be made to that x/y position
// This is an additional helper function to try to sanitize transport
// Returns 1 if successful, 0 if not

int try_transport(World *mzx_world, int x, int y, int push_status,
 int can_push, int id, int dir)
{
  Board *src_board = mzx_world->current_board;
  int d_offset = xy2array2(src_board, x, y);
  int d_id = src_board->level_id[d_offset];

  // Not gonna be happening with goop or a transport
  if((d_id != 34) && (d_id != 49))
  {
    // Get flags for the ID
    int d_flag = flags[d_id];

    // Can it be moved under? It's a good destination then
    if(d_flag & A_UNDER)
      return 1;

    // Is it pushable?
    if(d_flag & (push_status | A_SPEC_PUSH))
    {
      // Player can move onto sensor.. sensor is not under
      // but pushable.
      if((d_id == 122) && (id == 127))
      {
        return 1;
      }
      else
      {
        // Okay, try pushing it.. if it can be pushed
        if(can_push == 1)
        {
          int px, py;
          arraydir2(src_board, x, y, &px, &py, flip_dir(dir));
          // Try to push it
          if(push(mzx_world, px, py, dir, 0) == 0)
            return 1;
        }
      }
    }
  }

  return 0;
}

int transport(World *mzx_world, int x, int y, int dir, int id, int param,
 int color, int can_push)
{
  Board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  // East/west or north/south
  int push_status = A_PUSHEW;
  int offset = xy2array2(src_board, x, y);
  int dx, dy, d_offset;
  char t_param = level_param[offset];
  int found = 0;

  // Get the direction of the transporter
  int t_dir = t_param & 0x07;

  // The direction must either be the given one or anydir
  if((t_dir != dir) && (t_dir != 4))
    return 1;

  if(dir < 2)
  {
    push_status = A_PUSHNS;
  }

  if(arraydir2(src_board, x, y, &dx, &dy, dir))
    return 1;

  // See if the first one will work

  found = try_transport(mzx_world, dx, dy, push_status, can_push, id, dir);

  // Until found or exited, loop on
  while(!found)
  {
    // The next one must be a transporter
    d_offset = xy2array2(src_board, dx, dy);
    // Get next dir

    if(arraydir2(src_board, dx, dy, &dx, &dy, dir))
      return 1;

    if(level_id[d_offset] == 49)
    {
      t_dir = level_param[d_offset] & 0x07;
      if((t_dir == 4) || (t_dir == flip_dir(dir)))
      {
        // Try the new location, after the transport
        found = try_transport(mzx_world, dx, dy, push_status, can_push, id, dir);
      }
    }
  }

  // Okay, can put it at dx, dy.. if we're not just checking
  if(id != 0)
  {
    // Place it
    play_sfx(mzx_world, 27);
    id_place(mzx_world, dx, dy, id, color, param);
  }

  // Successful return..
  return 0;
}

void push_player_sensor(World *mzx_world, int p_offset,
 int d_offset, unsigned char param, unsigned char color)
{
  Board *src_board = mzx_world->current_board;
  char *level_under_id = src_board->level_under_id;
  char *level_under_color = src_board->level_under_color;
  char *level_under_param = src_board->level_under_param;

  // Restore the previous under with this stuff
  level_under_id[p_offset] = mzx_world->under_player_id;
  level_under_color[p_offset] = mzx_world->under_player_color;
  level_under_param[p_offset] = mzx_world->under_player_param;

  // Get new under stuff
  mzx_world->under_player_id = level_under_id[d_offset];
  mzx_world->under_player_color = level_under_color[d_offset];
  mzx_world->under_player_param = level_under_param[d_offset];

  // Move the sensor under the player
  level_under_id[d_offset] = 122;
  level_under_color[d_offset] = color;
  level_under_param[d_offset] = param;

  push_sensor(mzx_world, level_under_param[d_offset]);
}

int push(World *mzx_world, int x, int y, int dir, int checking)
{
  Board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;

  int push_status = A_PUSHEW;
  int dx = x, dy = y, d_offset, d_flag;
  unsigned char d_id = 0xFF, d_param, d_color, d_under_id;
  unsigned char p_id = 0xFF, p_param = 0xFF, p_color = 0xFF;
  unsigned char p_under_id = 0xFF, sensor_param, sensor_color;
  int p_offset = -1;

  if(dir < 2)
  {
    push_status = A_PUSHNS;
  }

  // See if the block can be pushed; if not, exit.
  while(1)
  {
    // Previous ID
    p_id = d_id;
    // Get the next in the dir, out of bounds returns 1
    if(arraydir2(src_board, dx, dy, &dx, &dy, dir))
      return 1;
    d_offset = xy2array2(src_board, dx, dy);
    d_id = level_id[d_offset];
    d_flag = flags[d_id];

    // If a push can be made here, the destination has been found

    if(d_flag & A_UNDER)
    {
      // FIXME - In game2.asm you can't push things over water, even though
      // it still works somehow. Look into this.
      // Lava and goop are automatic failures even though it's "underable"
      if((d_id == 26) || (d_id == 34))
        return 1;
      // Otherwise, we're good
      break;
    }
    else
    {
      // The search may continue... if it's pushable in that dir, continue
      if(!(d_flag & push_status))
      {
        // "spec push" can be pushed too, otherwise no push
        if(!(d_flag & A_SPEC_PUSH))
          return 1;
        // Player can be pushed onto sensor, otherwise it's pushable
        if((d_id == 122) && (p_id == 127))
          break;
        else

        // Can be pushed onto a transport if transport checks out
        if(d_id == 49)
        {
          if(transport(mzx_world, dx, dy, dir, 0, 0, 0, 1))
            return 1;
          else
            break;
        }
      }
    }
  }

  // The push can be made now, make sure we're not just checking though
  if(!checking)
  {
    // Start back at the beginning...
    dx = x;
    dy = y;
    p_id = 0xFF;

    while(1)
    {
      // Get thing at next location
      arraydir2(src_board, dx, dy, &dx, &dy, dir);
      d_offset = xy2array2(src_board, dx, dy);
      d_id = level_id[d_offset];
      d_param = level_param[d_offset];
      d_color = level_color[d_offset];
      d_under_id = level_under_id[d_offset];
      d_flag = flags[d_id];

      // Sensor? Mark it
      if(d_under_id == 122)
      {
        sensor_color = level_under_color[d_offset];
        sensor_param = level_under_param[d_offset];
      }

      // Can the destination be moved under and thus pushed onto?
      // A sensor will also work if the player is what's being pushed onto it.
      if((d_flag & A_UNDER) || ((p_id == 127) && (d_id == 122)))
      {
        // Place the previous thing here
        id_place(mzx_world, dx, dy, p_id, p_color, p_param);
        if((p_id == 127) && (p_under_id == 122))
        {
          push_player_sensor(mzx_world, p_offset, d_offset, sensor_param,
           sensor_color);
        }
        // If this is a sensor, the player was pushed onto it
        if(d_id == 122)
        {
          // The player is just stepping on it.. alert sensor
          step_sensor(mzx_world, d_param);
        }
        // All under can stop
        break;
      }

      // Is it pushable?
      if(d_flag & (push_status | A_SPEC_PUSH))
      {
        if(d_id == 49)
        {
          // If it's a transport, transport it
          transport(mzx_world, dx, dy, dir, p_id, p_param, p_color, 1);
          break;
        }

        // No previous ID yet?
        if(p_id == 0xFF)
        {
          // Remove what was there
          id_remove_top(mzx_world, dx, dy);
        }
        else
        {
          // Otherwise, put the last one in the new one, and make
          // the current one the new last one.
          level_id[d_offset] = p_id;
          level_param[d_offset] = p_param;
          level_color[d_offset] = p_color;

          // How about a pushable robot?
          if(d_id == 123)
          {
            // Send the default label for pushing
            send_robot_def(mzx_world, d_param, 3);
          }

          // Was a player/sensor sandwich pushed?
          if((p_id == 127) && (p_under_id == 122))
          {
            push_player_sensor(mzx_world, p_offset, d_offset,
             sensor_param, sensor_color);
          }
        }

        // Load current into previous
        p_id = d_id;
        p_under_id = d_under_id;
        p_param = d_param;
        p_color = d_color;
        p_offset = d_offset;


        // Is it a sensor that was pushed? Flag it.
        if(d_id == 122)
        {
          push_sensor(mzx_world, d_param);
        }
      }
    }
  }

  play_sfx(mzx_world, 26);

  return 0;
}

void shoot(World *mzx_world, int x, int y, int dir, int type)
{
  int dx, dy, d_offset, d_flag;
  unsigned char d_id, d_param;
  Board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;

  // If enemy hurts enemy then enemy bullet becomes neutral
  if((mzx_world->enemy_hurt_enemy == 1) && (type == ENEMY_BULLET))
  {
    type = NEUTRAL_BULLET;
  }

  if(arraydir2(src_board, x, y, &dx, &dy, dir))
  {
    return;
  }

  d_offset = xy2array2(src_board, dx, dy);
  d_id = src_board->level_id[d_offset];
  d_param = src_board->level_param[d_offset];
  d_flag = flags[d_id];

  // If it's a player bullet maybe give some points
  // Has to be an enemy ID 80-95 but not 83, 85, 92, or 93
  if((type == PLAYER_BULLET) && (d_id >= 80) && (d_id <= 95) &&
   (d_id != 83) && (d_id != 85) && (d_id != 92) && (d_id != 93))
  {
    // Score isn't supposed to overflow.. but the overflow
    // detection in game2.asm is broken anyway....
    // Just let it wrap around for now.
    mzx_world->score += 3;
  }

  // If it's underable the bullet can move here
  if(d_flag & A_UNDER)
  {
    unsigned char n_param = (type << 2) | dir;
    id_place(mzx_world, dx, dy, 61, bullet_color[type], n_param);
    return;
  }

  // Is it shootable?
  if(d_flag & A_SHOOTABLE)
  {
    // However, enemies can only shoot these things:
    if((type != ENEMY_BULLET) || (d_id == 61) || (d_id == 28) ||
     (d_id == 29) || (d_id == 6) || (d_id == 7))
    {
      // Can be removed
      id_remove_top(mzx_world, dx, dy);
      play_sfx(mzx_world, 29);
    }
    return;
  }

  // Must be a special shot for anything to happen now
  if(d_flag & A_SPEC_SHOT)
  {
    // And these are the ones
    switch(d_id)
    {
      // Panel
      case 72:
      {
        play_sfx(mzx_world, 31);
        // Fanangle the panel orientation into a shoot direction
        if(d_param == 1)
        {
          dir ^= 2;
        }
        else
        {
          dir ^= 3;
        }
        shoot(mzx_world, dx, dy, dir, NEUTRAL_BULLET);
        break;
      }
      // Ricochet
      case 73:
      {
        int pdir = flip_dir(dir);
        play_sfx(mzx_world, 31);
        // Shoot backwards
        shoot(mzx_world, dx, dy, pdir, NEUTRAL_BULLET);
        break;
      }
      // Mine
      case 74:
      {
        // Turn into explosion
        level_id[d_offset] = 38;
        // Get rid of count and anim fields in param
        level_param[d_offset] = d_param & 0xF0;
        play_sfx(mzx_world, 36);
        break;
      }
      // Player
      case 127:
      {
        // Player bullets don't hurt player
        if(type == PLAYER_BULLET) break;
        hurt_player_id(mzx_world, 61);
        send_robot_def(mzx_world, 0, 7);
        break;
      }
      // Robot
      case 123:
      case 124:
      {
        // Send the current shot label to the robot
        send_robot_def(mzx_world, d_param, type + 4);
        // Set its last shot dir..
        (src_board->robot_list[d_param])->last_shot_dir = flip_dir(dir) + 1;
        break;
      }
      // Eye
      case 81:
      {
        // Can't be shot by enemy bullet
        if(type == ENEMY_BULLET) break;
        // Turn into explosion
        level_id[d_offset] = 38;
        level_param[d_offset] = (d_param & 0x38) << 1;
        play_sfx(mzx_world, 36);
        break;
      }
      // Slime
      case 83:
      {
        // Can't be shot by enemy bullet
        if(type == ENEMY_BULLET) break;
        // Check if not invincible
        if(!(d_param & 0x80))
        {
          // Kill it
          id_remove_top(mzx_world, dx, dy);
          play_sfx(mzx_world, 29);
        }
        break;
      }
      // Runner
      case 84:
      {
        // Can't be shot by enemy bullet
        if(type == ENEMY_BULLET) break;
        // Check if it has 0 HP
        if(d_param & 0xC0)
        {
          // Otherwise, take an HP
          level_param[d_offset] = d_param - 0x40;
          play_sfx(mzx_world, 45);
        }
        else
        {
          // Kill it
          id_remove_top(mzx_world, dx, dy);
          play_sfx(mzx_world, 29);
        }

        break;
      }
      // Ghost
      case 85:
      {
        // Can't be shot by enemy bullet
        if(type == ENEMY_BULLET) break;
        // Is it not invincible?
        if(!(d_param & 0x08))
        {
          // Kill it
          id_remove_top(mzx_world, dx, dy);
          play_sfx(mzx_world, 29);
        }

        break;
      }
      // Dragon
      case 86:
      {
        // Can't be shot by enemy bullet
        if(type == ENEMY_BULLET) break;

        // Out of HP?
        if(!(d_param & 0xE0))
        {
          // Kill it
          id_remove_top(mzx_world, dx, dy);
          play_sfx(mzx_world, 29);
        }
        else
        {
          // Take away a hit point
          level_param[d_offset] = d_param - 0x20;
          play_sfx(mzx_world, 45);
        }

        break;
      }
      // Fish/Spider/Bear
      case 87:
      case 89:
      case 94:
      {
        // Zero HP?
        if(!(d_param & 0x80))
        {
          // Kill it
          id_remove_top(mzx_world, dx, dy);
          play_sfx(mzx_world, 29);
        }
        else
        {
          // Remove HP
          level_param[d_offset] = d_param ^ 0x80;
          play_sfx(mzx_world, 45);
        }
        break;
      }
    }
    return;
  }
}

void shoot_fire(World *mzx_world, int x, int y, int dir)
{
  Board *src_board = mzx_world->current_board;
  int dx, dy, d_offset;
  unsigned char d_id, d_param, d_flag;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;

  if(arraydir2(src_board, x, y, &dx, &dy, dir))
  {
    return;
  }
  d_offset = xy2array2(src_board, dx, dy);
  d_id = level_id[d_offset];
  d_param = level_param[d_offset];
  d_flag = flags[d_id];

  // If it can be moved under, place it
  if(d_flag & A_UNDER)
  {
    id_place(mzx_world, dx, dy, 78, 0, dir << 1);
  }
  else

  // Did it hit a robot?
  if((d_id == 123) || (d_id == 124))
  {
    // Send the spitfire label
    send_robot_def(mzx_world, d_param, 9);
  }
  else

  // Did it hit the player?
  if(d_id == 127)
  {
    hurt_player_id(mzx_world, 78);
  }
}

void shoot_seeker(World *mzx_world, int x, int y, int dir)
{
  Board *src_board = mzx_world->current_board;
  int dx, dy, d_offset;
  unsigned char d_id, d_param, d_flag;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;

  if(arraydir2(src_board, x, y, &dx, &dy, dir))
  {
    return;
  }
  d_offset = xy2array2(src_board, dx, dy);
  d_id = level_id[d_offset];
  d_param = level_param[d_offset];
  d_flag = flags[d_id];

  // If it can be moved under, place it
  if(d_flag & A_UNDER)
  {
    id_place(mzx_world, dx, dy, 79, 0, 127);
  }
  else

  // Did it hit the player?
  if(d_id == 127)
  {
    hurt_player_id(mzx_world, 79);
  }
}

void shoot_missile(World *mzx_world, int x, int y, int dir)
{
  Board *src_board = mzx_world->current_board;
  int dx, dy, d_offset;
  unsigned char d_id, d_param, d_flag;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;


  if(arraydir2(src_board, x, y, &dx, &dy, dir))
  {
    return;
  }
  d_offset = xy2array2(src_board, dx, dy);
  d_id = level_id[d_offset];
  d_param = level_param[d_offset];
  d_flag = flags[d_id];

  // If it can be moved under, place it
  if(d_flag & A_UNDER)
  {
    id_place(mzx_world, dx, dy, 62, missile_color, dir);
  }
  else

  // Did it hit the player?
  if(d_id == 127)
  {
    hurt_player_id(mzx_world, 62);
  }
}

#define MOVE_CAN_PUSH           0x001
#define MOVE_CAN_TRANSPORT      0x002
#define MOVE_CAN_LAVAWALK       0x004
#define MOVE_CAN_FIREWALK       0x008
#define MOVE_CAN_WATERWALK      0x010
#define MOVE_MUST_WEB           0x020
#define MOVE_MUST_THICKWEB      0x040
#define MOVE_REACT_PLAYER       0x080
#define MOVE_MUST_WATER         0x100
#define MOVE_MUST_LAVAGOOP      0x200
#define MOVE_CAN_GOOPWALK       0x400
#define MOVE_SPITFIRE           0x800

int move(World *mzx_world, int x, int y, int dir, int move_flags)
{
  Board *src_board = mzx_world->current_board;
  int dx, dy, d_offset, d_flag;
  unsigned char d_id, d_param;
  int p_offset = xy2array2(src_board, x, y);
  unsigned char p_id, p_param, p_color;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;

  if(arraydir2(src_board, x, y, &dx, &dy, dir))
  {
    // Hit edge
    return 3;
  }
  d_offset = xy2array2(src_board, dx, dy);
  d_id = level_id[d_offset];
  d_param = level_param[d_offset];
  d_flag = flags[d_id];

  p_id = level_id[p_offset];
  p_param = level_param[p_offset];
  p_color = level_color[p_offset];

  // Only if the player shouldn't be checked yet
  if((move_flags & MOVE_REACT_PLAYER) && (d_id == 127))
  {
    // Hit the player
    return 2;
  }

  // Is it a spitfire? Is it moving onto a robot?
  if((move_flags & MOVE_SPITFIRE) && ((d_id == 123) || (d_id == 124)))
  {
    // Send the label
    send_robot_def(mzx_world, d_param, 9);
    return 1;
  }

  // Can move? However, flags may prohibit the move.
  if(d_flag & A_UNDER)
  {
    int web_flags = (move_flags & (MOVE_MUST_WEB | MOVE_MUST_THICKWEB)) >> 5;
    int web = d_id - 17;
    if((web < 1) || (web > 2)) web = 0;

    if(web_flags)
    {
      // Must be one of these
      if(!(web_flags & web))
        return 1;
    }

    // Must be water?
    if((move_flags & MOVE_MUST_WATER) && ((d_id < 20) || (d_id > 24)))
      return 1;

    // Must be lava or goop?
    if((move_flags & MOVE_MUST_LAVAGOOP) && (d_id != 26) && (d_id != 34))
      return 1;

    // Now CAN it move to lava if there's lava?
    if((d_id == 26) && !(move_flags & MOVE_CAN_LAVAWALK))
      return 1;

    // Same for fire
    if((d_id == 63) && !(move_flags & MOVE_CAN_FIREWALK))
      return 1;

    // Same for water
    if((d_id >= 20) && (d_id <= 24) && !(move_flags & MOVE_CAN_WATERWALK))
      return 1;

    // Only must goop can go on goop
    if((d_id == 34) && !(move_flags & MOVE_MUST_LAVAGOOP))
      return 1;
  }
  else

  // Moving onto a transporter?
  if(d_id == 49)
  {
    // Can it transport?
    if(move_flags & MOVE_CAN_TRANSPORT)
    {
      int can_push = 0;

      // Transport it
      if(move_flags & MOVE_CAN_PUSH)
        can_push = 1;

      // If it can transport, do it
      if(!transport(mzx_world, dx, dy, dir, p_id, p_param, p_color, can_push))
      {
        id_remove_top(mzx_world, x, y);
        return 0;
      }
    }
    // Otherwise blocked
    return 1;
  }
  else

  // Is it pushable and can it be pushed?
  if((d_flag & (A_PUSHNS | A_PUSHEW | A_SPEC_PUSH)) &&
   (move_flags & MOVE_CAN_PUSH))
  {
    // If it can't push, exit
      if(push(mzx_world, x, y, dir, 0))
        return 1;
  }
  else
  {
    return 1;
  }

  // Made it this far - actually move the thing.
  // Remove what was there
  id_remove_top(mzx_world, x, y);
  // Place a new one at the destination
  id_place(mzx_world, dx, dy, p_id, p_color, p_param);

  // Successfully return
  return 0;
}

int parsedir(World *mzx_world, int old_dir, int x, int y, int flow_dir,
 int bln, int bls, int ble, int blw)
{
  int n_dir = old_dir & 0x0F;

  switch(n_dir)
  {
    // idle, beneath, anydir, nodir don't get modified
    case 0:
    case 11:
    case 12:
    case 14:
      return n_dir;

    // Cardinal direction
    case 1:
    case 2:
    case 3:
    case 4:
      n_dir--;
      break;

    // randns
    case 5:
      n_dir = rand() & 1;
      break;

    // randew
    case 6:
      n_dir = (rand() & 1) + 2;
      break;

    // randne
    case 7:
      n_dir = (rand() & 2);
      break;

    // randnb
    case 8:
      bln ^= 1;
      bls ^= 1;
      ble ^= 1;
      blw ^= 1;
    // randb
    case 15:
    {
      int bl_sum = bln + bls + ble + blw;
      if(bl_sum)
      {
        n_dir = rand() % bl_sum;
        if(!bln)
          n_dir++;
        if(!bls && n_dir)
          n_dir++;
        if(!ble && (n_dir > 1))
          n_dir++;
      }
      else
      {
        return 14;
      }
      break;
    }

    // seek
    case 9:
      n_dir = find_seek(mzx_world, x, y);
      break;

    // randany
    case 10:
      n_dir = rand() & 3;
      break;

    // flow
    case 13:
      if(flow_dir)
        n_dir = flow_dir - 1;
      else
        return 0;
      break;
  }

  // Rotate clockwise
  if(old_dir & 0x20)
  {
    if(n_dir >= 2)
      n_dir ^= 1;
    n_dir ^= 2;
  }
  // Move opposite
  if(old_dir & 0x40)
  {
    n_dir = (n_dir ^ 1);
  }
  // Randp
  if(old_dir & 0x10)
  {
    int rval = rand();
    n_dir ^= 2;
    if(rval & 1)
    {
      n_dir ^= 1;
    }
  }
  // Randnot
  if(old_dir & 0x80)
  {
    int rval = (rand() & 3);
    if(n_dir != rval)
    {
      n_dir = rval;
    }
    else
    {
      n_dir = 4;
    }
  }

  return n_dir + 1;
}
