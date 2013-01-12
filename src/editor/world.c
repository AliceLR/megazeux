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

#include "world.h"

#include "../error.h"
#include "../graphics.h"
#include "../window.h"
#include "../world.h"
#include "../idput.h"

#include "board.h"
#include "robot.h"

#include <string.h>

static const unsigned char def_id_chars[455] =
{
  /* id_chars */
  32,178,219,6,0,255,177,255,233,254,255,254,255,178,177,176, /* 0-15 */
  254,255,0,0,176,24,25,26,27,0,0,160,4,4,3,9,                /* 16-31 */
  150,7,176,0,11,0,177,12,10,0,0,162,161,0,0,22,              /* 32-47 */
  196,0,7,255,255,255,255,159,0,18,29,0,206,0,0,0,            /* 48-63 */
  '?',178,0,151,152,153,154,32,0,42,0,0,255,255,0,0,          /* 64-79 */
  235,236,5,42,2,234,21,224,127,149,5,227,0,0,172,173,        /* 80-95 */
  '?',0, /* 96-97 */
  '?','?','?','?','?','?','?','?', /* 98-121 */
  '?','?','?','?','?','?','?','?',
  '?','?','?','?','?','?','?','?',
  0,0,0,226,232,0, /* 122-127 */
  /* thin_line */
  249,179,179,179, /* None, N, S, NS */
  196,192,218,195, /* E, NE, SE, NSE */
  196,217,191,180, /* W, NW, SW, NSW */
  196,193,194,197, /* EW, NEW, SEW, NSEW */
  /* thick_line */
  249,186,186,186, /* None, N, S, NS */
  205,200,201,204, /* E, NE, SE, NSE */
  205,188,187,185, /* W, NW, SW, NSW */
  205,202,203,206, /* EW, NEW, SEW, NSEW */
  /* ice_anim */
  32,252,253,255, /* Ice animation table */
  /* lava_anim */
  176,177,178, /* Lava animation table */
  /* low_ammo, hi_ammo */
  163, /* < 10 ammunition pic */
  164, /* > 9 ammunition pic */
  /* lit_bomb */
  171,170,169,168,167,166,165, /* Lit bomb animation */
  /* energizer_glow */
  1,9,3,11,15,11,3,9, /* Energizer Glow */
  /* explosion_colors */
  239,206,76,4, /* Explosion color table */
  /* horiz_door, vert_door */
  196, /* Horizontal door pic */
  179, /* Vertical door pic */
  /* cw_anim, ccw_anim */
  47,196,92,179, /* CW animation table */
  47,179,92,196, /* CCW animation table */
  /* open_door */
  47,47, /* Open 1/2 of type 0 */
  92,92, /* Open 1/2 of type 1 */
  92,92, /* Open 1/2 of type 2 */
  47,47, /* Open 1/2 of type 3 */
  179,196,179,196,179,196,179,196, /* Open full of all types */
  179,196,179,196,179,196,179,196, /* Open full of all types */
  47,47, /* Close 1/2 of type 0 */
  92,92, /* Close 1/2 of type 1 */
  92,92, /* Close 1/2 of type 2 */
  47,47, /* Close 1/2 of type 3 */
  /* transport_anims */
  45,94,126,94, /* North */
  86,118,95,118, /* South */
  93,41,62,41, /* East */
  91,40,60,40, /* West */
  94,62,118,60, /* All directions */
  /* thick_arrow, thin_arrow */
  30,31,16,17, /* Thick arrows (pusher-style) NSEW */
  24,25,26,27, /* Thin arrows (gun-style) NSEW */
  /* horiz_lazer, vert_lazer */
  130,196,131,196, /* Horizontal Lazer Anim Table */
  128,179,129,179, /* Vertical Lazer Anim Table */
  /* fire_anim, fire_colors */
  177,178,178,178,177,176, /* Fire animation */
  4,6,12,14,12,6, /* Fire colors */
  /* life_anim, life_colors */
  155,156,157,158, /* Free life animation */
  15,11,3,11, /* Free life colors */
  /* ricochet_panels */
  28,23, /* Ricochet pics */
  /* mine_anim */
  143,144, /* Mine animation */
  /* shooting_fire_anim, shooting_fire_colors */
  15,42, /* Shooting Fire Anim */
  12,14, /* Shooting Fire Colors */
  /* seeker_anim, seeker_colors */
  '/','-','\\','|', /* Seeker animation */
  11,14,10,12, /* Seeker colors */
  /* whirlpool_glow */
  11,3,9,15, /* Whirlpool colors (this ends at 306 bytes, where ver */
  /* 1.02 used to end) */
  /* bullet_char */
  145,146,147,148, /* Player */
  145,146,147,148, /* Neutral */
  145,146,147,148, /* Enemy */
  /* player_char */
  2, 2, 2, 2, /*N S E W */
  /* player_color, missile_color, bullet_color */
  27,
  8,
  15, /* Player */
  15, /* Neutral */
  15, /* Enemy */
  /* id_dmg */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0-15 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100,  0, 0, 0, 0, 0, /* 16-31 */
  0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 32-47 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 10, 5, 5, /* 48-63 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 10, 10, /* 64-79 */
  10, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 10, 10, /* 80-95 */
  0, 0, /* 96-97 */
  0, 0, 0, 0, 0, 0, 0, 0, /* 98-121 */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0  /* 122-127 */
};

bool append_world(struct world *mzx_world, const char *file)
{
  int i, i2;
  int num_boards, old_num_boards = mzx_world->num_boards;
  int last_pos;
  int offset;
  int d_flag;
  struct board *cur_board;
  int board_width, board_height;
  char *level_id, *level_param;
  int input_version;
  FILE *fp;

  fp = try_load_world(file, false, &input_version, NULL);
  if(!fp)
    return false;

  fseek(fp, 4234, SEEK_SET);

  num_boards = fgetc(fp);

  if(num_boards == 0)
  {
    int sfx_size;
    // Sfx skip word size
    fseek(fp, 2, SEEK_CUR);

    // Skip sfx
    for(i = 0; i < NUM_SFX; i++)
    {
      sfx_size = fgetc(fp);
      fseek(fp, sfx_size, SEEK_CUR);
    }
    num_boards = fgetc(fp);
  }

  // Skip the names for now
  // Gonna wanna come back to here
  last_pos = ftell(fp);
  fseek(fp, num_boards * BOARD_NAME_SIZE, SEEK_CUR);

  if(num_boards + old_num_boards >= MAX_BOARDS)
    num_boards = MAX_BOARDS - old_num_boards;

  mzx_world->num_boards += num_boards;
  mzx_world->num_boards_allocated += num_boards;
  mzx_world->board_list =
   crealloc(mzx_world->board_list,
   sizeof(struct board *) * (old_num_boards + num_boards));

  // Append boards
  for(i = old_num_boards; i < old_num_boards + num_boards; i++)
  {
    mzx_world->board_list[i] =
     load_board_allocate(fp, 0, input_version, mzx_world->version);
    cur_board = mzx_world->board_list[i];
  }

  // Go back to where the names are
  fseek(fp, last_pos, SEEK_SET);
  for(i = old_num_boards; i < old_num_boards + num_boards; i++)
  {
    cur_board = mzx_world->board_list[i];
    fread(cur_board->board_name, BOARD_NAME_SIZE, 1, fp);

    if(cur_board)
    {
      // Also patch a pointer to the global robot
      if(cur_board->robot_list)
      {
        cur_board->robot_list[0] = &mzx_world->global_robot;
      }
      // Also optimize out null objects
      optimize_null_objects(cur_board);

      board_width = cur_board->board_width;
      board_height = cur_board->board_height;
      level_id = cur_board->level_id;
      level_param = cur_board->level_param;

      // ALSO offset all entrances and exits
      for(offset = 0; offset < board_width * board_height; offset++)
      {
        d_flag = flags[(int)level_id[offset]];

        if((d_flag & A_ENTRANCE) && (level_param[offset] != NO_BOARD))
        {
          level_param[offset] += old_num_boards;
        }
      }

      for(i2 = 0; i2 < 4; i2++)
      {
        if(cur_board->board_dir[i2] != NO_BOARD)
          cur_board->board_dir[i2] += old_num_boards;
      }
    }
  }

  // Remove any null boards
  optimize_null_boards(mzx_world);

  fclose(fp);
  return true;
}

// Create a new, blank, world, suitable for editing.
void create_blank_world(struct world *mzx_world)
{
  // Make default global data
  // Make a blank board
  int i;

  mzx_world->num_boards = 1;
  mzx_world->num_boards_allocated = 1;
  mzx_world->board_list = cmalloc(sizeof(struct board *));
  mzx_world->board_list[0] = create_blank_board(&(mzx_world->editor_conf));
  mzx_world->current_board_id = 0;
  mzx_world->current_board = mzx_world->board_list[0];

  mzx_world->edge_color = 8;
  mzx_world->first_board = 0;
  mzx_world->endgame_board = NO_ENDGAME_BOARD;
  mzx_world->death_board = NO_DEATH_BOARD;
  mzx_world->endgame_x = 0;
  mzx_world->endgame_y = 0;
  mzx_world->game_over_sfx = 1;
  mzx_world->death_x = 0;
  mzx_world->death_y = 0;
  mzx_world->starting_lives = 7;
  mzx_world->lives_limit = 99;
  mzx_world->starting_health = 100;
  mzx_world->health_limit = 200;
  mzx_world->enemy_hurt_enemy = 0;
  mzx_world->clear_on_exit = 0;
  mzx_world->only_from_swap = 0;
  memcpy(id_chars, def_id_chars, 324);
  memcpy(bullet_color, def_id_chars + 324, 3);
  memcpy(id_dmg, def_id_chars + 327, 128);

  create_blank_robot_direct(&mzx_world->global_robot, -1, -1);
  mzx_world->current_board->robot_list[0] = &mzx_world->global_robot;

  for(i = 0; i < 6; i++)
  {
    mzx_world->status_counters_shown[i][0] = 0;
  }

  mzx_world->name[0] = 0;

  set_update_done(mzx_world);

  set_screen_mode(0);
  smzx_palette_loaded(0);
  set_palette_intensity(100);

  ec_load_mzx();
  default_palette();
  default_global_data(mzx_world);
}

void set_update_done_current(struct world *mzx_world)
{
  struct board *current_board = mzx_world->current_board;
  int size = current_board->board_width * current_board->board_height;

  if(size > mzx_world->update_done_size)
  {
    if(mzx_world->update_done == NULL)
    {
     mzx_world->update_done = cmalloc(size);
    }
    else
    {
      mzx_world->update_done =
       crealloc(mzx_world->update_done, size);
    }

    mzx_world->update_done_size = size;
  }
}

void move_current_board(struct world *mzx_world, int new_position)
{
  int i, i2;
  int num_boards = mzx_world->num_boards;
  int old_position = mzx_world->current_board_id;
  struct board **board_list = mzx_world->board_list;
  struct board **new_board_list = ccalloc(num_boards, sizeof(struct board *));
  int *board_id_translation_list = ccalloc(num_boards, sizeof(int));

  // Copy the list and shift all boards necessary
  for(i = 0; i < num_boards; i++)
  {
    // This works easier if we start with the new table and
    // figure out where to copy from the old table.
    i2 = i - (i >= new_position) +
     (i >= (old_position + (old_position > new_position)));

    // As it turns out, we'll always have a duplicate of something
    // else where the new position is this way, ignore it so we
    // don't mess up the translation table.
    if(i != new_position)
    {
      board_id_translation_list[i2] = i;
      new_board_list[i] = board_list[i2];
    }
  }

  // Insert the old board
  board_id_translation_list[old_position] = new_position;
  new_board_list[new_position] = board_list[old_position];

  refactor_board_list(mzx_world, new_board_list, num_boards,
   board_id_translation_list);

  free(board_id_translation_list);
}

char get_default_id_char(int id)
{
  return def_id_chars[id];
}
