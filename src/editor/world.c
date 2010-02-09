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

#include "../decrypt.h"
#include "../error.h"
#include "../graphics.h"
#include "../window.h"
#include "../world.h"
#include "../idput.h"

#include "robot.h"

#include <string.h>

int append_world(World *mzx_world, const char *file)
{
  int version;
  int i, i2;
  int num_boards, old_num_boards = mzx_world->num_boards;
  int last_pos;
  int protection_method;
  int offset;
  int d_flag;
  char magic[3];
  char error_string[80];
  Board *cur_board;
  int board_width, board_height;
  char *level_id, *level_param;

  FILE *fp = fopen(file, "rb");
  if(fp == NULL)
  {
    error("Error loading world", 1, 8, 0x0D01);
    return 1;
  }

  // Name of game - skip it.
  fseek(fp, BOARD_NAME_SIZE, SEEK_CUR);
  // Get protection method
  protection_method = fgetc(fp);

  if(protection_method)
  {
    int do_decrypt;
    error("This world is password protected.", 1, 8, 0x0D02);
    do_decrypt = confirm(mzx_world, "Would you like to decrypt it?");
    if(!do_decrypt)
    {
      int rval;

      fclose(fp);
      decrypt(file);
      // Ah recursion.....
      rval = append_world(mzx_world, file);
      return rval;
    }
    else
    {
      error("Cannot load password protected worlds.", 1, 8, 0x0D02);
      fclose(fp);
      return 1;
    }
  }

  fread(magic, 1, 3, fp);
  version = world_magic(magic);
  if(version == 0)
  {
    sprintf(error_string, "Attempt to load non-MZX world.");
  }
  else

  if(version < 0x0205)
  {
    sprintf(error_string, "World is from old version (%d.%d); use converter",
      (version & 0xFF00) >> 8, version & 0xFF);
    version = 0;
  }
  else

  if(version > WORLD_VERSION)
  {
    sprintf(error_string, "World is from more recent version (%d.%d)",
      (version & 0xFF00) >> 8, version & 0xFF);
    version = 0;
  }

  if(!version)
  {
    error(error_string, 1, 8, 0x0D02);
    fclose(fp);
    return 1;
  }

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
   realloc(mzx_world->board_list,
   sizeof(Board *) * (old_num_boards + num_boards));

  // Append boards
  for(i = old_num_boards; i < old_num_boards + num_boards; i++)
  {
    mzx_world->board_list[i] = load_board_allocate(fp, 0);
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
        cur_board->robot_list[0] = mzx_world->global_robot;
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

  // ...All done!
  fclose(fp);

  return 0;
}

// Create a new, blank, world, suitable for editing.
void create_blank_world(World *mzx_world)
{
  // Make default global data
  // Make a blank board
  int i;

  mzx_world->num_boards = 1;
  mzx_world->num_boards_allocated = 1;
  mzx_world->board_list = malloc(sizeof(Board *));
  mzx_world->board_list[0] = create_blank_board();
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

  create_blank_robot_direct(mzx_world->global_robot, -1, -1);
  mzx_world->current_board->robot_list[0] = mzx_world->global_robot;

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

void set_update_done_current(World *mzx_world)
{
  Board *current_board = mzx_world->current_board;
  int size = current_board->board_width * current_board->board_height;

  if(size > mzx_world->update_done_size)
  {
    if(mzx_world->update_done == NULL)
    {
     mzx_world->update_done = malloc(size);
    }
    else
    {
      mzx_world->update_done =
       realloc(mzx_world->update_done, size);
    }

    mzx_world->update_done_size = size;
  }
}
