/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "world.h"
#include "world_format.h"
#include "legacy_world.h"

#include "board.h"
#include "configure.h"
#include "const.h"
#include "counter.h"
#include "data.h"
#include "error.h"
#include "event.h"
#include "extmem.h"
#include "fsafeopen.h"
#include "game_player.h"
#include "graphics.h"
#include "idput.h"
#include "memfile.h"
#include "robot.h"
#include "sprite.h"
#include "str.h"
#include "util.h"
#include "window.h"
#include "zip.h"

#include "audio/audio.h"
#include "audio/sfx.h"

#ifdef CONFIG_LOADSAVE_METER

static Uint32 last_ticks;

void meter_update_screen(int *curr, int target)
{
  Uint32 ticks = get_ticks();

  (*curr)++;
  meter_interior(*curr, target);

  // Draw updates to the screen at roughly the UI framerate
  if(ticks - last_ticks > UPDATE_DELAY)
  {
    update_screen();
    last_ticks = get_ticks();
  }
}

void meter_restore_screen(void)
{
  restore_screen();
  update_screen();
}

void meter_initial_draw(int curr, int target, const char *title)
{
  save_screen();
  meter(title, curr, target);
  update_screen();
  last_ticks = get_ticks();
}

#else //!CONFIG_LOADSAVE_METER

static inline void meter_update_screen(int *curr, int target) {}
static inline void meter_restore_screen(void) {}
static inline void meter_initial_draw(int curr, int target, const char *title) {}

#endif //CONFIG_LOADSAVE_METER


int world_magic(const char magic_string[3])
{
  if(magic_string[0] == 'M')
  {
    if(magic_string[1] == 'Z')
    {
      switch(magic_string[2])
      {
        case 'X':
          return V100;
        case '2':
          return V200;
        case 'A':
          return V251s1;
      }
    }
    else
    {
      // I hope to God that MZX doesn't last beyond 9.x
      if((magic_string[1] > 1) && (magic_string[1] < 10))
        return ((int)magic_string[1] << 8) + (int)magic_string[2];
    }
  }

  return 0;
}

int save_magic(const char magic_string[5])
{
  if((magic_string[0] == 'M') && (magic_string[1] == 'Z'))
  {
    switch(magic_string[2])
    {
      case 'S':
        if((magic_string[3] == 'V') && (magic_string[4] == '2'))
        {
          return V251;
        }
        else if((magic_string[3] >= 2) && (magic_string[3] <= 10))
        {
          return((int)magic_string[3] << 8 ) + magic_string[4];
        }
        else
        {
          return 0;
        }
      case 'X':
        if((magic_string[3] == 'S') && (magic_string[4] == 'A'))
        {
          return V251s1;
        }
        else
        {
          return 0;
        }
      default:
        return 0;
    }
  }
  else
  {
    return 0;
  }
}

int get_version_string(char buffer[16], enum mzx_version version)
{
  switch(version)
  {
    case V100:
      sprintf(buffer, "1.xx");
      break;

    case V251:
      sprintf(buffer, "2.xx/2.51");
      break;

    case V251s1:
      sprintf(buffer, "2.51s1");
      break;

    case V251s2: // through V261
      sprintf(buffer, "2.51s2/2.61");
      break;

    case VERSION_DECRYPT:
      sprintf(buffer, "<decrypted>");
      break;

    case V262:
      sprintf(buffer, "2.62/2.62b");
      break;

    case V265: // Also 2.68 because the magic wasn't actually incremented.
      sprintf(buffer, "2.65/2.68");
      break;

    case V268:
      sprintf(buffer, "2.68");
      break;

    case V269:
      sprintf(buffer, "2.69");
      break;

    case V269b:
      sprintf(buffer, "2.69b");
      break;

    case V269c:
      sprintf(buffer, "2.69c");
      break;

    case V270:
      sprintf(buffer, "2.70");
      break;

    default:
    {
      if(version < VERSION_PORT)
      {
        sprintf(buffer, "<unknown %4.4Xh>", version);
      }

      else
      {
        buffer[11] = 0;
        snprintf(buffer, 11, "%d.%2.2d",
         (version >> 8) & 0xFF, version & 0xFF);
      }

      break;
    }
  }

  return strlen(buffer);
}


// World info
static inline int save_world_info(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame, int file_version,
 const char *name)
{
  enum mzx_version world_version = mzx_world->version;

  char *buffer;
  size_t buf_size = WORLD_PROP_SIZE;
  struct memfile _mf;
  struct memfile _prop;
  struct memfile *mf = &_mf;
  struct memfile *prop = &_prop;
  int size;
  int i;

  int result;

  if(savegame)
    buf_size += SAVE_PROP_SIZE;

  buffer = cmalloc(buf_size);

  mfopen(buffer, buf_size, mf);

  // Save everything sorted.

  // Header redundant properties
  save_prop_s(WPROP_WORLD_NAME, mzx_world->name, BOARD_NAME_SIZE, 1, mf);
  save_prop_w(WPROP_FILE_VERSION, file_version, mf);

  if(savegame)
  {
    save_prop_w(WPROP_WORLD_VERSION, world_version, mf);
    save_prop_c(WPROP_SAVE_START_BOARD, mzx_world->current_board_id, mf);
    save_prop_c(WPROP_SAVE_TEMPORARY_BOARD, mzx_world->temporary_board, mf);
  }
  else
  {
    save_prop_w(WPROP_WORLD_VERSION, file_version, mf);
  }

  save_prop_c(WPROP_NUM_BOARDS,         mzx_world->num_boards, mf);

  // ID Chars
  save_prop_s(WPROP_ID_CHARS,           id_chars, ID_CHARS_SIZE, 1, mf);
  save_prop_c(WPROP_ID_MISSILE_COLOR,   missile_color, mf);
  save_prop_s(WPROP_ID_BULLET_COLOR,    bullet_color, ID_BULLET_COLOR_SIZE, 1, mf);
  save_prop_s(WPROP_ID_DMG,             id_dmg, ID_DMG_SIZE, 1, mf);

  // Status counters
  save_prop_v(WPROP_STATUS_COUNTERS, COUNTER_NAME_SIZE * NUM_STATUS_COUNTERS,
   prop, mf);

  for(i = 0; i < NUM_STATUS_COUNTERS; i++)
  {
    mfwrite(mzx_world->status_counters_shown[i], COUNTER_NAME_SIZE, 1, prop);
  }

  // Global properties
  save_prop_c(WPROP_EDGE_COLOR,         mzx_world->edge_color, mf);
  save_prop_c(WPROP_FIRST_BOARD,        mzx_world->first_board, mf);
  save_prop_c(WPROP_ENDGAME_BOARD,      mzx_world->endgame_board, mf);
  save_prop_c(WPROP_DEATH_BOARD,        mzx_world->death_board, mf);
  save_prop_w(WPROP_ENDGAME_X,          mzx_world->endgame_x, mf);
  save_prop_w(WPROP_ENDGAME_Y,          mzx_world->endgame_y, mf);
  save_prop_c(WPROP_GAME_OVER_SFX,      mzx_world->game_over_sfx, mf);
  save_prop_w(WPROP_DEATH_X,            mzx_world->death_x, mf);
  save_prop_w(WPROP_DEATH_Y,            mzx_world->death_y, mf);
  save_prop_w(WPROP_STARTING_LIVES,     mzx_world->starting_lives, mf);
  save_prop_w(WPROP_LIVES_LIMIT,        mzx_world->lives_limit, mf);
  save_prop_w(WPROP_STARTING_HEALTH,    mzx_world->starting_health, mf);
  save_prop_w(WPROP_HEALTH_LIMIT,       mzx_world->health_limit, mf);
  save_prop_c(WPROP_ENEMY_HURT_ENEMY,   mzx_world->enemy_hurt_enemy, mf);
  save_prop_c(WPROP_CLEAR_ON_EXIT,      mzx_world->clear_on_exit, mf);
  save_prop_c(WPROP_ONLY_FROM_SWAP,     mzx_world->only_from_swap, mf);

  // SMZX and vlayer are global properties 2.91+
  if(savegame || world_version >= V291)
  {
    save_prop_c(WPROP_SMZX_MODE,        get_screen_mode(), mf);
    save_prop_w(WPROP_VLAYER_WIDTH,     mzx_world->vlayer_width, mf);
    save_prop_w(WPROP_VLAYER_HEIGHT,    mzx_world->vlayer_height, mf);
    save_prop_d(WPROP_VLAYER_SIZE,      mzx_world->vlayer_size, mf);
  }

  if(savegame)
  {
    // Save properties
    size = strlen(mzx_world->real_mod_playing) + 1;
    save_prop_s(WPROP_REAL_MOD_PLAYING, mzx_world->real_mod_playing, size, 1, mf);

    save_prop_c(WPROP_MZX_SPEED,        mzx_world->mzx_speed, mf);
    save_prop_c(WPROP_LOCK_SPEED,       mzx_world->lock_speed, mf);
    save_prop_d(WPROP_COMMANDS,         mzx_world->commands, mf);
    save_prop_d(WPROP_COMMANDS_STOP,    mzx_world->commands_stop, mf);
    save_prop_v(WPROP_SAVED_POSITIONS,  40, prop, mf);

    for(i = 0; i < 8; i++)
    {
      mfputw(mzx_world->pl_saved_x[i], prop);
      mfputw(mzx_world->pl_saved_y[i], prop);
      mfputc(mzx_world->pl_saved_board[i], prop);
    }

    save_prop_v(WPROP_UNDER_PLAYER, 3, prop, mf);

    mfputc(mzx_world->under_player_id, prop);
    mfputc(mzx_world->under_player_color, prop);
    mfputc(mzx_world->under_player_param, prop);

    save_prop_w(WPROP_PLAYER_RESTART_X, mzx_world->player_restart_x, mf);
    save_prop_w(WPROP_PLAYER_RESTART_Y, mzx_world->player_restart_y, mf);
    save_prop_c(WPROP_SAVED_PL_COLOR,   mzx_world->saved_pl_color, mf);
    save_prop_s(WPROP_KEYS,             mzx_world->keys, NUM_KEYS, 1, mf);
    save_prop_c(WPROP_BLIND_DUR,        mzx_world->blind_dur, mf);
    save_prop_c(WPROP_FIREWALKER_DUR,   mzx_world->firewalker_dur, mf);
    save_prop_c(WPROP_FREEZE_TIME_DUR,  mzx_world->freeze_time_dur, mf);
    save_prop_c(WPROP_SLOW_TIME_DUR,    mzx_world->slow_time_dur, mf);
    save_prop_c(WPROP_WIND_DUR,         mzx_world->wind_dur, mf);
    save_prop_c(WPROP_SCROLL_BASE_COLOR,    mzx_world->scroll_base_color, mf);
    save_prop_c(WPROP_SCROLL_CORNER_COLOR,  mzx_world->scroll_corner_color, mf);
    save_prop_c(WPROP_SCROLL_POINTER_COLOR, mzx_world->scroll_pointer_color, mf);
    save_prop_c(WPROP_SCROLL_TITLE_COLOR,   mzx_world->scroll_title_color, mf);
    save_prop_c(WPROP_SCROLL_ARROW_COLOR,   mzx_world->scroll_arrow_color, mf);
    save_prop_c(WPROP_MESG_EDGES,       mzx_world->mesg_edges, mf);
    save_prop_c(WPROP_BI_SHOOT_STATUS,  mzx_world->bi_shoot_status, mf);
    save_prop_c(WPROP_BI_MESG_STATUS,   mzx_world->bi_mesg_status, mf);
    save_prop_c(WPROP_FADED,            get_fade_status(), mf);

    size = strlen(mzx_world->input_file_name) + 1;
    save_prop_s(WPROP_INPUT_FILE_NAME,  mzx_world->input_file_name, size, 1, mf);
    save_prop_d(WPROP_INPUT_POS,        mzx_world->temp_input_pos, mf);
    save_prop_d(WPROP_FREAD_DELIMITER,  mzx_world->fread_delimiter, mf);

    size = strlen(mzx_world->output_file_name) + 1;
    save_prop_s(WPROP_OUTPUT_FILE_NAME, mzx_world->output_file_name, size, 1, mf);
    save_prop_d(WPROP_OUTPUT_POS,       mzx_world->temp_output_pos, mf);
    save_prop_d(WPROP_FWRITE_DELIMITER, mzx_world->fwrite_delimiter, mf);
    save_prop_d(WPROP_MULTIPLIER,       mzx_world->multiplier, mf);
    save_prop_d(WPROP_DIVIDER,          mzx_world->divider, mf);
    save_prop_d(WPROP_C_DIVISIONS,      mzx_world->c_divisions, mf);
    save_prop_d(WPROP_MAX_SAMPLES,      mzx_world->max_samples, mf);
    save_prop_c(WPROP_SMZX_MESSAGE,     mzx_world->smzx_message, mf);
    save_prop_c(WPROP_JOY_SIMULATE_KEYS,mzx_world->joystick_simulate_keys, mf);
  }

  save_prop_eof(mf);

  size = mftell(mf);

  result = zip_write_file(zp, name, buffer, size, ZIP_M_NONE);

  free(buffer);
  return result;
}

#define check(id) {                                                           \
  while(next_prop(&prop, &ident, &size, &mf))                                 \
  {                                                                           \
    if(ident == id) break;                                                    \
    else if(ident > id || ident == WPROP_EOF) {                               \
      missing_ident = id;                                                     \
      goto err_free;                                                          \
    }                                                                         \
  }                                                                           \
  last_ident = ident;                                                         \
}

static inline enum val_result validate_world_info(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame, int *file_version)
{
  int world_version;

  char *buffer;
  struct memfile mf;
  struct memfile prop;
  size_t actual_size;

  int missing_ident;
  int last_ident = -1;
  int ident = 0;
  int size = 0;

  zip_get_next_uncompressed_size(zp, &actual_size);

  buffer = cmalloc(actual_size);

  zip_read_file(zp, buffer, actual_size, NULL);

  mfopen(buffer, actual_size, &mf);

  mzx_world->raw_world_info = buffer;
  mzx_world->raw_world_info_size = actual_size;

  // Get the file version out of order.
  check(WPROP_FILE_VERSION);
  *file_version = load_prop_int(size, &prop);

  // Check everything else sorted.
  check(WPROP_WORLD_VERSION);
  world_version = load_prop_int(size, &prop);

  // World data.
  check(WPROP_NUM_BOARDS);
  check(WPROP_ID_CHARS);
  check(WPROP_ID_MISSILE_COLOR);
  check(WPROP_ID_BULLET_COLOR);
  check(WPROP_ID_DMG);
  check(WPROP_STATUS_COUNTERS);
  check(WPROP_EDGE_COLOR);
  check(WPROP_FIRST_BOARD);
  check(WPROP_ENDGAME_BOARD);
  check(WPROP_DEATH_BOARD);
  check(WPROP_ENDGAME_X);
  check(WPROP_ENDGAME_Y);
  check(WPROP_GAME_OVER_SFX);
  check(WPROP_DEATH_X);
  check(WPROP_DEATH_Y);
  check(WPROP_STARTING_LIVES);
  check(WPROP_LIVES_LIMIT);
  check(WPROP_STARTING_HEALTH);
  check(WPROP_HEALTH_LIMIT);
  check(WPROP_ENEMY_HURT_ENEMY);
  check(WPROP_CLEAR_ON_EXIT);
  check(WPROP_ONLY_FROM_SWAP);

  if(!savegame && world_version < V291)
  {
    return VAL_SUCCESS;
  }

  // World data (2.91+), save-only prior.
  check(WPROP_SMZX_MODE);
  check(WPROP_VLAYER_WIDTH);
  check(WPROP_VLAYER_HEIGHT);
  check(WPROP_VLAYER_SIZE);

  if(!savegame)
  {
    return VAL_SUCCESS;
  }

  // Save-only data.
  check(WPROP_REAL_MOD_PLAYING);
  check(WPROP_MZX_SPEED);
  check(WPROP_LOCK_SPEED);
  check(WPROP_COMMANDS);
  check(WPROP_COMMANDS_STOP);
  check(WPROP_SAVED_POSITIONS);
  check(WPROP_UNDER_PLAYER);
  check(WPROP_PLAYER_RESTART_X);
  check(WPROP_PLAYER_RESTART_Y);
  check(WPROP_SAVED_PL_COLOR);
  check(WPROP_KEYS);
  check(WPROP_BLIND_DUR);
  check(WPROP_FIREWALKER_DUR);
  check(WPROP_FREEZE_TIME_DUR);
  check(WPROP_SLOW_TIME_DUR);
  check(WPROP_WIND_DUR);
  check(WPROP_SCROLL_BASE_COLOR);
  check(WPROP_SCROLL_CORNER_COLOR);
  check(WPROP_SCROLL_POINTER_COLOR);
  check(WPROP_SCROLL_TITLE_COLOR);
  check(WPROP_SCROLL_ARROW_COLOR);
  check(WPROP_MESG_EDGES);
  check(WPROP_BI_SHOOT_STATUS);
  check(WPROP_BI_MESG_STATUS);
  check(WPROP_FADED);
  check(WPROP_INPUT_FILE_NAME);
  check(WPROP_INPUT_POS);
  check(WPROP_FREAD_DELIMITER);
  check(WPROP_OUTPUT_FILE_NAME);
  check(WPROP_OUTPUT_POS);
  check(WPROP_FWRITE_DELIMITER);
  check(WPROP_MULTIPLIER);
  check(WPROP_DIVIDER);
  check(WPROP_C_DIVISIONS);

  if(world_version >= V291)
  {
    // Added in 2.91
    check(WPROP_MAX_SAMPLES);
    check(WPROP_SMZX_MESSAGE);
  }

  if(world_version >= V292)
  {
    // Added in 2.92
    check(WPROP_JOY_SIMULATE_KEYS);
  }

  return VAL_SUCCESS;

err_free:
  fprintf(stderr,
   "load_world_info: expected ID %xh not found (found %xh, last %xh)\n",
   missing_ident, ident, last_ident);
  fflush(stderr);

  free(buffer);
  mzx_world->raw_world_info = NULL;
  mzx_world->raw_world_info_size = 0;
  return VAL_INVALID;
}

#define if_savegame         if(!savegame) { break; }
#define if_savegame_or_291  if(!savegame && *file_version < V291) { break; }

static inline void load_world_info(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame, int *file_version, boolean *faded)
{
  char *buffer;
  struct memfile _mf;
  struct memfile _prop;
  struct memfile *mf = &_mf;
  struct memfile *prop = &_prop;
  size_t actual_size;
  int ident = -1;
  int size;
  int v;
  int i;

  // This should absolutely be set, but just in case it isn't...
  if(!mzx_world->raw_world_info)
  {
    zip_get_next_uncompressed_size(zp, &actual_size);

    buffer = cmalloc(actual_size);

    zip_read_file(zp, buffer, actual_size, NULL);
  }

  else
  {
    zip_skip_file(zp);
    buffer = mzx_world->raw_world_info;
    actual_size = mzx_world->raw_world_info_size;
    mzx_world->raw_world_info = NULL;
    mzx_world->raw_world_info_size = 0;
  }

  mfopen(buffer, actual_size, mf);

  while(next_prop(prop, &ident, &size, mf))
  {
    switch(ident)
    {
      case WPROP_EOF:
        mfseek(mf, 0, SEEK_END);
        break;

      // Header redundant properties
      case WPROP_WORLD_NAME:
        mfread(mzx_world->name, size, 1, prop);
        break;

      case WPROP_WORLD_VERSION:
        mzx_world->version = load_prop_int(size, prop);
        break;

      case WPROP_FILE_VERSION:
      {
        // Already read this during validation.
        break;
      }

      case WPROP_SAVE_START_BOARD:
        if(savegame)
        mzx_world->current_board_id = load_prop_int(size, prop);
        break;

      case WPROP_SAVE_TEMPORARY_BOARD:
        if(savegame)
        mzx_world->temporary_board = load_prop_int(size, prop);
        break;

      case WPROP_NUM_BOARDS:
        v = load_prop_int(size, prop);
        mzx_world->num_boards = CLAMP(v, 1, MAX_BOARDS);
        break;

      // ID Chars
      case WPROP_ID_CHARS:
      {
        if(size > ID_CHARS_SIZE)
          size = ID_CHARS_SIZE;

        mfread(id_chars, size, 1, prop);
        memset(id_chars + size, 0, ID_CHARS_SIZE - size);
        break;
      }

      case WPROP_ID_MISSILE_COLOR:
      {
        missile_color = load_prop_int(size, prop);
        break;
      }

      case WPROP_ID_BULLET_COLOR:
      {
        if(size > ID_BULLET_COLOR_SIZE)
          size = ID_BULLET_COLOR_SIZE;

        mfread(bullet_color, size, 1, prop);
        memset(bullet_color + size, 0, ID_BULLET_COLOR_SIZE - size);
        break;
      }

      case WPROP_ID_DMG:
      {
        if(size > ID_DMG_SIZE)
          size = ID_DMG_SIZE;

        mfread(id_dmg, size, 1, prop);
        memset(id_dmg + size, 0, ID_DMG_SIZE - size);
        break;
      }

      // Status counters
      case WPROP_STATUS_COUNTERS:
        for(i = 0; i < NUM_STATUS_COUNTERS; i++)
        {
          mfread(mzx_world->status_counters_shown[i], COUNTER_NAME_SIZE, 1, prop);
        }
        break;

      // Global properties
      case WPROP_EDGE_COLOR:
        mzx_world->edge_color = load_prop_int(size, prop);
        break;

      case WPROP_FIRST_BOARD:
        mzx_world->first_board = load_prop_int(size, prop);
        break;

      case WPROP_ENDGAME_BOARD:
        mzx_world->endgame_board = load_prop_int(size, prop);
        break;

      case WPROP_DEATH_BOARD:
        mzx_world->death_board = load_prop_int(size, prop);
        break;

      case WPROP_ENDGAME_X:
        mzx_world->endgame_x = load_prop_int(size, prop);
        break;

      case WPROP_ENDGAME_Y:
        mzx_world->endgame_y = load_prop_int(size, prop);
        break;

      case WPROP_GAME_OVER_SFX:
        mzx_world->game_over_sfx = load_prop_int(size, prop);
        break;

      case WPROP_DEATH_X:
        mzx_world->death_x = load_prop_int(size, prop);
        break;

      case WPROP_DEATH_Y:
        mzx_world->death_y = load_prop_int(size, prop);
        break;

      case WPROP_STARTING_LIVES:
        mzx_world->starting_lives = load_prop_int(size, prop);
        break;

      case WPROP_LIVES_LIMIT:
        mzx_world->lives_limit = load_prop_int(size, prop);
        break;

      case WPROP_STARTING_HEALTH:
        mzx_world->starting_health = load_prop_int(size, prop);
        break;

      case WPROP_HEALTH_LIMIT:
        mzx_world->health_limit = load_prop_int(size, prop);
        break;

      case WPROP_ENEMY_HURT_ENEMY:
        mzx_world->enemy_hurt_enemy = load_prop_int(size, prop);
        break;

      case WPROP_CLEAR_ON_EXIT:
        mzx_world->clear_on_exit = load_prop_int(size, prop);
        break;

      case WPROP_ONLY_FROM_SWAP:
        mzx_world->only_from_swap = load_prop_int(size, prop);
        break;

      // Global properties II
      case WPROP_SMZX_MODE:
        if_savegame_or_291
        set_screen_mode(load_prop_int(size, prop));
        break;

      case WPROP_VLAYER_WIDTH:
        if_savegame_or_291
        mzx_world->vlayer_width = load_prop_int(size, prop);
        break;

      case WPROP_VLAYER_HEIGHT:
        if_savegame_or_291
        mzx_world->vlayer_height = load_prop_int(size, prop);
        break;

      case WPROP_VLAYER_SIZE:
      {
        unsigned int vlayer_size;
        if_savegame_or_291

        vlayer_size = load_prop_int(size, prop);
        vlayer_size = MAX(1, vlayer_size);

        mzx_world->vlayer_size = vlayer_size;

        if(mzx_world->vlayer_chars)
        {
          mzx_world->vlayer_chars = crealloc(mzx_world->vlayer_chars, vlayer_size);
          mzx_world->vlayer_colors = crealloc(mzx_world->vlayer_colors, vlayer_size);
        }

        else
        {
          mzx_world->vlayer_chars = cmalloc(vlayer_size);
          mzx_world->vlayer_colors = cmalloc(vlayer_size);
        }
        break;
      }

      // Save properties
      case WPROP_REAL_MOD_PLAYING:
        if_savegame
        size = MIN(size, MAX_PATH-1);
        mfread(mzx_world->real_mod_playing, size, 1, prop);
        mzx_world->real_mod_playing[size] = 0;
        break;

      case WPROP_MZX_SPEED:
        if_savegame
        mzx_world->mzx_speed = load_prop_int(size, prop);
        break;

      case WPROP_LOCK_SPEED:
        if_savegame
        mzx_world->lock_speed = load_prop_int(size, prop);
        break;

      case WPROP_COMMANDS:
        if_savegame
        mzx_world->commands = load_prop_int(size, prop);
        break;

      case WPROP_COMMANDS_STOP:
        if_savegame
        mzx_world->commands_stop = load_prop_int(size, prop);
        break;

      case WPROP_SAVED_POSITIONS:
        if_savegame
        if(size >= 40)
        {
          for(i = 0; i < 8; i++)
          {
            mzx_world->pl_saved_x[i] = mfgetw(prop);
            mzx_world->pl_saved_y[i] = mfgetw(prop);
            mzx_world->pl_saved_board[i] = mfgetc(prop);
          }
        }
        break;

      case WPROP_UNDER_PLAYER:
        if_savegame
        if(size >= 3)
        {
          mzx_world->under_player_id = mfgetc(prop);
          mzx_world->under_player_color = mfgetc(prop);
          mzx_world->under_player_param = mfgetc(prop);
        }
        break;

      case WPROP_PLAYER_RESTART_X:
        if_savegame
        mzx_world->player_restart_x = load_prop_int(size, prop);
        break;

      case WPROP_PLAYER_RESTART_Y:
        if_savegame
        mzx_world->player_restart_y = load_prop_int(size, prop);
        break;

      case WPROP_SAVED_PL_COLOR:
        if_savegame
        mzx_world->saved_pl_color = load_prop_int(size, prop);
        break;

      case WPROP_KEYS:
        if_savegame
        mfread(mzx_world->keys, NUM_KEYS, 1, prop);
        break;

      case WPROP_BLIND_DUR:
        if_savegame
        mzx_world->blind_dur = load_prop_int(size, prop);
        break;

      case WPROP_FIREWALKER_DUR:
        if_savegame
        mzx_world->firewalker_dur = load_prop_int(size, prop);
        break;

      case WPROP_FREEZE_TIME_DUR:
        if_savegame
        mzx_world->freeze_time_dur = load_prop_int(size, prop);
        break;

      case WPROP_SLOW_TIME_DUR:
        if_savegame
        mzx_world->slow_time_dur = load_prop_int(size, prop);
        break;

      case WPROP_WIND_DUR:
        if_savegame
        mzx_world->wind_dur = load_prop_int(size, prop);
        break;

      case WPROP_SCROLL_BASE_COLOR:
        if_savegame
        mzx_world->scroll_base_color = load_prop_int(size, prop);
        break;

      case WPROP_SCROLL_CORNER_COLOR:
        if_savegame
        mzx_world->scroll_corner_color = load_prop_int(size, prop);
        break;

      case WPROP_SCROLL_POINTER_COLOR:
        if_savegame
        mzx_world->scroll_pointer_color = load_prop_int(size, prop);
        break;

      case WPROP_SCROLL_TITLE_COLOR:
        if_savegame
        mzx_world->scroll_title_color = load_prop_int(size, prop);
        break;

      case WPROP_SCROLL_ARROW_COLOR:
        if_savegame
        mzx_world->scroll_arrow_color = load_prop_int(size, prop);
        break;

      case WPROP_MESG_EDGES:
        if_savegame
        mzx_world->mesg_edges = load_prop_int(size, prop);
        break;

      case WPROP_BI_SHOOT_STATUS:
        if_savegame
        mzx_world->bi_shoot_status = load_prop_int(size, prop);
        break;

      case WPROP_BI_MESG_STATUS:
        if_savegame
        mzx_world->bi_mesg_status = load_prop_int(size, prop);
        break;

      case WPROP_FADED:
        if_savegame
        *faded = load_prop_int(size, prop);
        break;

      case WPROP_INPUT_FILE_NAME:
        if_savegame
        size = MIN(size, MAX_PATH - 1);
        mfread(mzx_world->input_file_name, size, 1, prop);
        mzx_world->input_file_name[size] = 0;
        break;

      case WPROP_INPUT_POS:
        if_savegame
        mzx_world->temp_input_pos = load_prop_int(size, prop);
        break;

      case WPROP_FREAD_DELIMITER:
        if_savegame
        mzx_world->fread_delimiter = load_prop_int(size, prop);
        break;

      case WPROP_OUTPUT_FILE_NAME:
        if_savegame
        size = MIN(size, MAX_PATH - 1);
        mfread(mzx_world->output_file_name, size, 1, prop);
        mzx_world->output_file_name[size] = 0;
        break;

      case WPROP_OUTPUT_POS:
        if_savegame
        mzx_world->temp_output_pos = load_prop_int(size, prop);
        break;

      case WPROP_FWRITE_DELIMITER:
        if_savegame
        mzx_world->fwrite_delimiter = load_prop_int(size, prop);
        break;

      case WPROP_MULTIPLIER:
        if_savegame
        mzx_world->multiplier = load_prop_int(size, prop);
        break;

      case WPROP_DIVIDER:
        if_savegame
        mzx_world->divider = load_prop_int(size, prop);
        break;

      case WPROP_C_DIVISIONS:
        if_savegame
        mzx_world->c_divisions = load_prop_int(size, prop);
        break;

      // Added in 2.91
      case WPROP_MAX_SAMPLES:
        if_savegame
        if(mzx_world->version >= V291)
        mzx_world->max_samples = load_prop_int(size, prop);
        break;

      case WPROP_SMZX_MESSAGE:
        if_savegame
        if(mzx_world->version >= V291)
        mzx_world->smzx_message = load_prop_int(size, prop);
        break;

      // Added in 2.92
      case WPROP_JOY_SIMULATE_KEYS:
        if_savegame
        if(mzx_world->version >= V292)
          mzx_world->joystick_simulate_keys = !!load_prop_int(size, prop);
        break;

      default:
        break;
    }
  }

  free(buffer);
}


// Global robot
static inline int save_world_global_robot(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame, int file_version, const char *name)
{
  save_robot(mzx_world, &mzx_world->global_robot, zp, savegame,
   file_version, name);

  return 0;
}

static inline int load_world_global_robot(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame, int file_version)
{
  load_robot(mzx_world, &mzx_world->global_robot, zp, savegame, file_version);
  return 0;
}


// SFX
static inline int save_world_sfx(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  // Only save if custom SFX are enabled
  if(mzx_world->custom_sfx_on)
  {
    return zip_write_file(zp, name, mzx_world->custom_sfx, NUM_SFX * SFX_SIZE,
     ZIP_M_DEFLATE);
  }

  return ZIP_SUCCESS;
}

static inline int load_world_sfx(struct world *mzx_world,
 struct zip_archive *zp)
{
  // No custom SFX loaded yet
  if(!mzx_world->custom_sfx_on)
  {
    mzx_world->custom_sfx_on = 1;
    return zip_read_file(zp, mzx_world->custom_sfx, NUM_SFX * SFX_SIZE, NULL);
  }

  // Already loaded custom SFX; skip
  else
  {
    return zip_skip_file(zp);
  }
}


// Charset
static inline int save_world_chars(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame, const char *name)
{
  // Save every charset
  unsigned char *buffer;
  size_t size = PROTECTED_CHARSET_POSITION;
  int result;

  buffer = cmalloc(size * CHAR_SIZE);
  ec_mem_save_set_var(buffer, size * CHAR_SIZE, 0);

  result = zip_write_file(zp, name, buffer, size * CHAR_SIZE, ZIP_M_DEFLATE);

  free(buffer);
  return result;
}

static inline int load_world_chars(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame)
{
  unsigned char *buffer;
  size_t actual_size;
  int result;

  zip_get_next_uncompressed_size(zp, &actual_size);

  buffer = cmalloc(actual_size);
  result = zip_read_file(zp, buffer, actual_size, &actual_size);
  if(result == ZIP_SUCCESS)
  {
    // Load only the first charset (2.90 worlds but not 2.90 saves)
    if(mzx_world->version == V290 && !savegame)
      if(actual_size > CHAR_SIZE * CHARSET_SIZE)
        actual_size = CHAR_SIZE * CHARSET_SIZE;

    ec_clear_set();
    ec_mem_load_set(buffer, actual_size);
  }

  free(buffer);
  return result;
}


// Palette
static inline int save_world_pal(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  unsigned char buffer[SMZX_PAL_SIZE * 3];
  unsigned char *cur = buffer;
  int i;

  for(i = 0; i < SMZX_PAL_SIZE; i++)
  {
    get_rgb(i, cur, cur+1, cur+2);
    cur += 3;
  }

  return zip_write_file(zp, name, buffer, SMZX_PAL_SIZE * 3, ZIP_M_NONE);
}

static inline int load_world_pal(struct world *mzx_world,
 struct zip_archive *zp)
{
  unsigned char buffer[SMZX_PAL_SIZE * 3];
  unsigned char *cur;
  unsigned int i;
  size_t size;
  int result;

  result = zip_read_file(zp, buffer, SMZX_PAL_SIZE*3, &size);
  if(result == ZIP_SUCCESS)
  {
    cur = buffer;
    size /= 3;

    for(i = 0; i < size; i++)
    {
      set_rgb(i, cur[0], cur[1], cur[2]);
      cur += 3;
    }
  }

  return result;
}


// Palette index
static inline int save_world_pal_index(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  int result = ZIP_SUCCESS;

  // Only save these while in the SMZX mode where they matter
  if(get_screen_mode() == 3)
  {
    char *buffer = cmalloc(SMZX_PAL_SIZE * 4);
    save_indices(buffer);

    result = zip_write_file(zp, name, buffer, SMZX_PAL_SIZE * 4, ZIP_M_NONE);

    free(buffer);
  }
  return result;
}

static inline int load_world_pal_index(struct world *mzx_world,
 int file_version, struct zip_archive *zp)
{
  char *buffer = cmalloc(SMZX_PAL_SIZE * 4);
  size_t actual_size;
  int result;

  result = zip_read_file(zp, buffer, SMZX_PAL_SIZE * 4, &actual_size);

  if(result == ZIP_SUCCESS)
  {
    if(file_version >= V291)
      load_indices(buffer, actual_size);

    // 2.90 stored the internal indices instead of user-friendly indices
    else
      load_indices_direct(buffer, actual_size);
  }

  free(buffer);
  return result;
}


// Palette intensities
static inline int save_world_pal_inten(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  char buffer[SMZX_PAL_SIZE];
  char *cur = buffer;
  int i;

  for(i = 0; i < SMZX_PAL_SIZE; i++, cur++)
    *cur = get_color_intensity(i);

  return zip_write_file(zp, name, buffer, SMZX_PAL_SIZE, ZIP_M_NONE);
}

static inline int load_world_pal_inten(struct world *mzx_world,
 struct zip_archive *zp)
{
  char buffer[SMZX_PAL_SIZE];
  char *cur;
  size_t size;
  unsigned int i;
  int result;

  result = zip_read_file(zp, buffer, SMZX_PAL_SIZE, &size);
  if(result == ZIP_SUCCESS)
  {
    cur = buffer;

    for(i = 0; i < size; i++, cur++)
      set_color_intensity(i, *cur);
  }

  return result;
}


// Vlayer colors
static inline int save_world_vco(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  return zip_write_file(zp, name,
   mzx_world->vlayer_colors, mzx_world->vlayer_size, ZIP_M_DEFLATE);
}

static inline int load_world_vco(struct world *mzx_world,
 struct zip_archive *zp)
{
  int vlayer_size = mzx_world->vlayer_size;

  return zip_read_file(zp, mzx_world->vlayer_colors, vlayer_size, NULL);
}

// Vlayer chars
static inline int save_world_vch(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  return zip_write_file(zp, name,
   mzx_world->vlayer_chars, mzx_world->vlayer_size, ZIP_M_DEFLATE);
}

static inline int load_world_vch(struct world *mzx_world,
 struct zip_archive *zp)
{
  int vlayer_size = mzx_world->vlayer_size;

  return zip_read_file(zp, mzx_world->vlayer_chars, vlayer_size, NULL);
}


// Sprites
static inline int save_world_sprites(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  char *buffer;
  size_t collision_size = mzx_world->collision_count * 4;
  size_t buf_size = SPRITE_PROPS_SIZE + collision_size;
  struct sprite *spr;
  struct memfile mf;
  struct memfile prop;
  int i;

  buffer = cmalloc(buf_size);
  mfopen(buffer, buf_size, &mf);

  // For each
  for(i = 0; i < MAX_SPRITES; i++)
  {
    spr = mzx_world->sprite_list[i];

    save_prop_c(SPROP_SET_ID,             i, &mf);
    save_prop_d(SPROP_X,                  spr->x, &mf);
    save_prop_d(SPROP_Y,                  spr->y, &mf);
    save_prop_d(SPROP_REF_X,              spr->ref_x, &mf);
    save_prop_d(SPROP_REF_Y,              spr->ref_y, &mf);
    save_prop_d(SPROP_COLOR,              spr->color, &mf);
    save_prop_d(SPROP_FLAGS,              spr->flags, &mf);
    save_prop_d(SPROP_WIDTH,              spr->width, &mf);
    save_prop_d(SPROP_HEIGHT,             spr->height, &mf);
    save_prop_d(SPROP_COL_X,              spr->col_x, &mf);
    save_prop_d(SPROP_COL_Y,              spr->col_y, &mf);
    save_prop_d(SPROP_COL_WIDTH,          spr->col_width, &mf);
    save_prop_d(SPROP_COL_HEIGHT,         spr->col_height, &mf);
    save_prop_d(SPROP_TRANSPARENT_COLOR,  spr->transparent_color, &mf);
    save_prop_d(SPROP_CHARSET_OFFSET,     spr->offset, &mf);
    save_prop_d(SPROP_Z,                  spr->z, &mf);
  }

  // Only once
  save_prop_d(SPROP_ACTIVE_SPRITES,       mzx_world->active_sprites, &mf);
  save_prop_d(SPROP_SPRITE_Y_ORDER,       mzx_world->sprite_y_order, &mf);
  save_prop_d(SPROP_COLLISION_COUNT,      mzx_world->collision_count, &mf);
  save_prop_d(SPROP_SPRITE_NUM,           mzx_world->sprite_num, &mf);

  // Collision list
  save_prop_v(SPROP_COLLISION_LIST, collision_size, &prop, &mf);

  for(i = 0; i < mzx_world->collision_count; i++)
    mfputd(mzx_world->collision_list[i], &prop);

  save_prop_eof(&mf);

  zip_write_file(zp, name, buffer, buf_size, ZIP_M_DEFLATE);

  free(buffer);
  return 0;
}

static inline int load_world_sprites(struct world *mzx_world,
 struct zip_archive *zp)
{
  char *buffer;
  size_t actual_size;

  struct sprite *spr = NULL;
  struct memfile mf;
  struct memfile prop;
  int ident;
  int length;
  int value;
  int num_collisions = 0;

  int result;

  result = zip_get_next_uncompressed_size(zp, &actual_size);
  if(result != ZIP_SUCCESS)
    return result;

  buffer = cmalloc(actual_size);

  result = zip_read_file(zp, buffer, actual_size, NULL);
  if(result != ZIP_SUCCESS)
    goto err_free;

  mfopen(buffer, actual_size, &mf);

  while(next_prop(&prop, &ident, &length, &mf))
  {
    // Mostly numeric values here, and anything that isn't can seek back.
    value = load_prop_int(length, &prop);

    switch(ident)
    {
      case SPROP_EOF:
        goto err_free;

      case SPROP_SET_ID:
        if(value >= 0 && value < MAX_SPRITES)
          spr = mzx_world->sprite_list[value];
        else
          spr = NULL;
        break;

      case SPROP_X:
        if(spr) spr->x = value;
        break;

      case SPROP_Y:
        if(spr) spr->y = value;
        break;

      case SPROP_REF_X:
        if(spr) spr->ref_x = value;
        break;

      case SPROP_REF_Y:
        if(spr) spr->ref_y = value;
        break;

      case SPROP_COLOR:
        if(spr) spr->color = value;
        break;

      case SPROP_FLAGS:
        if(spr) spr->flags = value;
        break;

      case SPROP_WIDTH:
        if(spr) spr->width = (unsigned int)value;
        break;

      case SPROP_HEIGHT:
        if(spr) spr->height = (unsigned int)value;
        break;

      case SPROP_COL_X:
        if(spr) spr->col_x = value;
        break;

      case SPROP_COL_Y:
        if(spr) spr->col_y = value;
        break;

      case SPROP_COL_WIDTH:
        if(spr) spr->col_width = (unsigned int)value;
        break;

      case SPROP_COL_HEIGHT:
        if(spr) spr->col_height = (unsigned int)value;
        break;

      case SPROP_TRANSPARENT_COLOR:
        if(spr) spr->transparent_color = value;
        break;

      case SPROP_CHARSET_OFFSET:
        if(spr) spr->offset = value;
        break;

      case SPROP_Z:
        if(mzx_world->version >= V292 && spr) spr->z = value;
        break;

      case SPROP_ACTIVE_SPRITES:
        mzx_world->active_sprites = value;
        break;

      case SPROP_SPRITE_Y_ORDER:
        mzx_world->sprite_y_order = value;
        break;

      case SPROP_COLLISION_COUNT:
        num_collisions = CLAMP(value, 0, MAX_SPRITES);
        mzx_world->collision_count = num_collisions;
        break;

      case SPROP_COLLISION_LIST:
      {
        int collision;

        mfseek(&prop, 0, SEEK_SET);
        if(num_collisions * 4 <= length)
          for(collision = 0; collision < num_collisions; collision++)
            mzx_world->collision_list[collision] = mfgetd(&prop);

        break;
      }

      case SPROP_SPRITE_NUM:
        mzx_world->sprite_num = value;
        break;

      default:
        break;
    }
  }

err_free:
  free(buffer);
  return result;
}


// Counters
static inline int save_world_counters(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  Uint8 buffer[8];
  struct memfile mf;
  struct counter_list *counter_list = &(mzx_world->counter_list);
  struct counter *src_counter;
  size_t name_length;
  int result;
  int i;

  result = zip_write_open_file_stream(zp, name, ZIP_M_NONE);
  if(result != ZIP_SUCCESS)
    return result;

  mfopen(buffer, 8, &mf);
  mfputd(counter_list->num_counters, &mf);
  zwrite(buffer, 4, zp);

  for(i = 0; i < counter_list->num_counters; i++)
  {
    src_counter = counter_list->counters[i];
    name_length = src_counter->name_length;

    mf.current = buffer;
    mfputd(src_counter->value, &mf);
    mfputd(name_length, &mf);
    zwrite(buffer, 8, zp);
    zwrite(src_counter->name, name_length, zp);
  }

  return zip_write_close_stream(zp);
}

static inline int load_world_counters(struct world *mzx_world,
 struct zip_archive *zp)
{
  struct memfile mf;
  char *buffer = NULL;

  char name_buffer[ROBOT_MAX_TR];
  size_t name_length;
  int value;

  struct counter_list *counter_list = &(mzx_world->counter_list);
  int num_prev_allocated;
  int num_counters;
  int i;

  enum zip_error result;
  unsigned int method;
  boolean is_stream = false;

  result = zip_get_next_method(zp, &method);
  if(result != ZIP_SUCCESS)
    return result;

  // If this is an uncompressed memory zip, we can read the memory directly.
  if(zp->is_memory && method == ZIP_M_NONE)
  {
    zip_read_open_mem_stream(zp, &mf);
    is_stream = true;
  }

  else
  {
    size_t actual_size;

    zip_get_next_uncompressed_size(zp, &actual_size);
    buffer = cmalloc(actual_size);
    zip_read_file(zp, buffer, actual_size, &actual_size);

    mfopen(buffer, actual_size, &mf);
  }

  num_counters = mfgetd(&mf);

  num_prev_allocated = counter_list->num_counters_allocated;

  // If there aren't already any counters, allocate manually.
  if(!num_prev_allocated)
  {
    counter_list->num_counters = num_counters;
    counter_list->num_counters_allocated = num_counters;
    counter_list->counters = ccalloc(num_counters, sizeof(struct counter *));
  }

  for(i = 0; i < num_counters; i++)
  {
    value = mfgetd(&mf);
    name_length = mfgetd(&mf);

    if(name_length >= ROBOT_MAX_TR)
      break;

    if(!mfread(name_buffer, name_length, 1, &mf))
      break;

    // If there were already counters, use new_counter to set or add them
    // into the existing counters as-needed.
    if(num_prev_allocated)
    {
      name_buffer[name_length] = 0;
      new_counter(mzx_world, name_buffer, value, -1);
    }

    // Otherwise, put them in the list manually.
    else
    {
      load_new_counter(counter_list, i, name_buffer, name_length, value);
    }
  }

  // If there weren't any previously allocated, the number successfully read is
  // the new number of counters.
  if(!num_prev_allocated)
    counter_list->num_counters = i;

#ifndef CONFIG_KHASH
  // Versions without the hash table require this to be sorted at all times
  sort_counter_list(counter_list);
#endif

  if(is_stream)
    zip_read_close_stream(zp);

  free(buffer);
  return ZIP_SUCCESS;
}


// Strings
static inline int save_world_strings(struct world *mzx_world,
 struct zip_archive *zp, const char *name)
{
  Uint8 buffer[8];
  struct memfile mf;
  struct string_list *string_list = &(mzx_world->string_list);
  struct string *src_string;
  size_t name_length;
  size_t str_length;
  int result;
  int i;

  result = zip_write_open_file_stream(zp, name, ZIP_M_NONE);
  if(result != ZIP_SUCCESS)
    return result;

  mfopen(buffer, 8, &mf);
  mfputd(string_list->num_strings, &mf);
  zwrite(buffer, 4, zp);

  for(i = 0; i < string_list->num_strings; i++)
  {
    src_string = string_list->strings[i];
    name_length = src_string->name_length;
    str_length = src_string->length;

    mf.current = buffer;
    mfputd(name_length, &mf);
    mfputd(str_length, &mf);
    zwrite(buffer, 8, zp);
    zwrite(src_string->name, name_length, zp);
    zwrite(src_string->value, str_length, zp);
  }

  return zip_write_close_stream(zp);
}

static inline int load_world_strings_mem(struct world *mzx_world,
 struct zip_archive *zp, int method)
{
  // In unusual cases (DEFLATEd strings, loading from memory), use this
  // implementation.
  struct memfile mf;
  char *buffer = NULL;
  boolean is_stream = false;

  struct string_list *string_list = &(mzx_world->string_list);
  struct string *src_string;
  char name_buffer[ROBOT_MAX_TR];
  size_t name_length;
  size_t str_length;

  int num_prev_allocated;
  int num_strings;
  int i;

  // If this is an uncompressed memory zip, we can read the memory directly.
  if(zp->is_memory && method == ZIP_M_NONE)
  {
    zip_read_open_mem_stream(zp, &mf);
    is_stream = true;
  }

  else
  {
    size_t actual_size;

    zip_get_next_uncompressed_size(zp, &actual_size);
    buffer = cmalloc(actual_size);
    zip_read_file(zp, buffer, actual_size, &actual_size);

    mfopen(buffer, actual_size, &mf);
  }

  num_strings = mfgetd(&mf);

  num_prev_allocated = string_list->num_strings_allocated;

  // If there aren't already any strings, allocate manually.
  if(!num_prev_allocated)
  {
    string_list->num_strings = num_strings;
    string_list->num_strings_allocated = num_strings;
    string_list->strings = ccalloc(num_strings, sizeof(struct string *));
  }

  for(i = 0; i < num_strings; i++)
  {
    name_length = mfgetd(&mf);
    str_length = mfgetd(&mf);

    if(name_length >= ROBOT_MAX_TR || str_length > MAX_STRING_LEN)
      break;

    if(!mfread(name_buffer, name_length, 1, &mf))
      break;

    // If there were already strings, use new_string to set or add them
    // into the existing strings as-needed.
    if(num_prev_allocated)
    {
      name_buffer[name_length] = 0;
      src_string = new_string(mzx_world, name_buffer, str_length, -1);
    }

    // Otherwise, put them in the list manually.
    else
    {
      src_string = load_new_string(string_list, i,
       name_buffer, name_length, str_length);
    }

    mfread(src_string->value, str_length, 1, &mf);
    src_string->length = str_length;
  }

  // If there weren't any previously allocated, the number successfully read is
  // the new number of strings.
  if(!num_prev_allocated)
    string_list->num_strings = i;

#ifndef CONFIG_KHASH
  // Versions without the hash table require this to be sorted at all times
  sort_string_list(string_list);
#endif

  if(is_stream)
    zip_read_close_stream(zp);

  free(buffer);
  return ZIP_SUCCESS;
}

static inline int load_world_strings(struct world *mzx_world,
 struct zip_archive *zp)
{
  Uint8 buffer[8];
  struct memfile mf;
  struct string_list *string_list = &(mzx_world->string_list);
  struct string *src_string;
  char name_buffer[ROBOT_MAX_TR];
  size_t name_length;
  size_t str_length;

  int num_prev_allocated;
  int num_strings;
  int i;

  enum zip_error result;
  unsigned int method;

  result = zip_get_next_method(zp, &method);
  if(result != ZIP_SUCCESS)
    return result;

  if(zp->is_memory || method == ZIP_M_DEFLATE)
    return load_world_strings_mem(mzx_world, zp, method);

  // Stream the strings out of the file.
  zip_read_open_file_stream(zp, NULL);

  mfopen(buffer, 8, &mf);
  zread(buffer, 4, zp);

  num_strings = mfgetd(&mf);
  num_prev_allocated = string_list->num_strings_allocated;

  // If there aren't already any strings, allocate manually.
  if(!num_prev_allocated)
  {
    string_list->num_strings = num_strings;
    string_list->num_strings_allocated = num_strings;
    string_list->strings = ccalloc(num_strings, sizeof(struct string *));
  }

  for(i = 0; i < num_strings; i++)
  {
    zread(buffer, 8, zp);
    mf.current = buffer;
    name_length = mfgetd(&mf);
    str_length = mfgetd(&mf);

    if(name_length >= ROBOT_MAX_TR || str_length > MAX_STRING_LEN)
      break;

    if(ZIP_SUCCESS != zread(name_buffer, name_length, zp))
      break;

    // If there were already string, use new_string to set or add them
    // into the existing strings as-needed.
    if(num_prev_allocated)
    {
      name_buffer[name_length] = 0;
      src_string = new_string(mzx_world, name_buffer, str_length, -1);
    }

    // Otherwise, put them in the list manually.
    else
    {
      src_string = load_new_string(string_list, i,
       name_buffer, name_length, str_length);
    }

    zread(src_string->value, str_length, zp);
    src_string->length = str_length;
  }

  // If there weren't any previously allocated, the number successfully read is
  // the new number of strings.
  if(!num_prev_allocated)
    string_list->num_strings = i;

#ifndef CONFIG_KHASH
  // Versions without the hash table require this to be sorted at all times
  sort_string_list(string_list);
#endif

  return zip_read_close_stream(zp);
}


void save_counters_file(struct world *mzx_world, const char *file)
{
  FILE *fp = fopen_unsafe(file, "wb");
  struct zip_archive *zp;

  if(!fp)
    return;

  // TODO temporary fix to improve save times on the embedded platforms.
  setvbuf(fp, NULL, _IOFBF, 16384);

  if(!fwrite("COUNTERS", 8, 1, fp))
    goto err;

  zp = zip_open_fp_write(fp);
  if(!zp)
    goto err;

  save_world_counters(mzx_world, zp,      "counter");
  save_world_strings(mzx_world, zp,       "string");

  zip_close(zp, NULL);
  return;

err:
  fclose(fp);
  return;
}


int load_counters_file(struct world *mzx_world, const char *file)
{
  FILE *fp = fopen_unsafe(file, "rb");
  struct zip_archive *zp;
  char magic[8];

  unsigned int prop_id;

  if(!fp)
  {
    error_message(E_FILE_DOES_NOT_EXIST, 0, NULL);
    return -1;
  }

  // TODO temporary fix to improve load times on the embedded platforms.
  setvbuf(fp, NULL, _IOFBF, 16384);

  if(!fread(magic, 8, 1, fp))
    goto err_close_file;

  if(memcmp(magic, "COUNTERS", 8))
    goto err_close_file;

  zp = zip_open_fp_read(fp);

  if(!zp)
    goto err_close_zip;

  assign_fprops(zp, 0);

  while(ZIP_SUCCESS == zip_get_next_prop(zp, &prop_id, NULL, NULL))
  {
    switch(prop_id)
    {
      case FPROP_WORLD_COUNTERS:
        load_world_counters(mzx_world, zp);
        break;

      case FPROP_WORLD_STRINGS:
        load_world_strings(mzx_world, zp);
        break;

      default:
        zip_skip_file(zp);
        break;
    }
  }

  zip_close(zp, NULL);
  return 0;

err_close_zip:
  zip_close(zp, NULL);
  fp = NULL;

err_close_file:
  if(fp)
    fclose(fp);
  error_message(E_SAVE_FILE_INVALID, 0, NULL);
  return -1;
}


static enum val_result validate_world_zip(struct world *mzx_world,
 struct zip_archive *zp, boolean savegame, int *file_version)
{
  unsigned int file_id;
  int result;

  int has_world = 0;
  int has_chars = 0;
  int has_pal = 0;
  int has_counter = 0;
  int has_string = 0;

  // The directory has already been read by this point.
  assign_fprops(zp, 0);

  // Step through the directory and make sure the mandatory files exist.
  while(ZIP_SUCCESS == zip_get_next_prop(zp, &file_id, NULL, NULL))
  {
    // Can we stop early?
    if((!savegame && has_pal) || has_string)
      break;

    switch(file_id)
    {
      // Everything needs this, no negotiations.
      case FPROP_WORLD_INFO:
        result = validate_world_info(mzx_world, zp, savegame, file_version);
        if(result != VAL_SUCCESS)
          return result;
        // Continue so it doesn't skip the file.
        has_world = 1;
        continue;

      // These are pretty much the bare minimum of what counts as a world
      case FPROP_WORLD_CHARS:
        has_chars = 1;
        break;

      case FPROP_WORLD_PAL:
        has_pal = 1;
        break;

      // These are pretty much the bare minimum of what counts as a save
      case FPROP_WORLD_COUNTERS:
        has_counter = 1;
        break;

      case FPROP_WORLD_STRINGS:
        has_string = 1;
        break;

      // Mandatory, but we can recover from not having them.
      case FPROP_WORLD_GLOBAL_ROBOT:
      case FPROP_WORLD_PAL_INDEX:
      case FPROP_WORLD_PAL_INTENSITY:
      case FPROP_WORLD_VCO:
      case FPROP_WORLD_VCH:
      case FPROP_WORLD_SPRITES:
        break;

      // Completely optional.
      case FPROP_WORLD_SFX:
        break;

      // Everything else: who knows
      default:
        break;
    }
    zip_skip_file(zp);
  }

  if(!(has_world && has_pal && has_chars))
    goto err_out;

  if(savegame && !(has_counter && has_string))
    goto err_out;

  return VAL_SUCCESS;

err_out:
  if(has_world)
  {
    free(mzx_world->raw_world_info);
    mzx_world->raw_world_info = NULL;
    mzx_world->raw_world_info_size = 0;
  }

  return VAL_MISSING;
}


static int save_world_zip(struct world *mzx_world, const char *file,
 boolean savegame, int file_version)
{
  FILE *fp;
  struct zip_archive *zp = NULL;
  struct board *cur_board;
  int i;

  int meter_curr = 0;
  int meter_target = 2 + mzx_world->num_boards + mzx_world->temporary_board;

  meter_initial_draw(meter_curr, meter_target, "Saving...");

  fp = fopen_unsafe(file, "wb");
  if(!fp)
    goto err;

  // TODO temporary fix to improve save times on the embedded platforms.
  setvbuf(fp, NULL, _IOFBF, 16384);

  // Header
  if(!savegame)
  {
    // World name
    if(!fwrite(mzx_world->name, BOARD_NAME_SIZE, 1, fp))
      goto err_close;

    // Protection method -- always zero
    fputc(0, fp);

    // Version string
    fputc('M', fp);
    fputc((file_version >> 8) & 0xFF, fp);
    fputc(file_version & 0xFF, fp);
  }
  else
  {
    // Version string
    if(!fwrite("MZS", 3, 1, fp))
      goto err_close;

    fputc((file_version >> 8) & 0xFF, fp);
    fputc(file_version & 0xFF, fp);

    // MZX world version
    fputw(mzx_world->version, fp);

    // Current board ID
    fputc(mzx_world->current_board_id, fp);
  }

  zp = zip_open_fp_write(fp);
  if(!zp)
    goto err_close;

  if(save_world_info(mzx_world, zp, savegame, file_version, "world"))
    goto err_close;

  if(save_world_global_robot(mzx_world, zp, savegame, file_version, "gr"))
    goto err_close;

  if(save_world_sfx(mzx_world, zp,              "sfx"))     goto err_close;
  if(save_world_chars(mzx_world, zp, savegame,  "chars"))   goto err_close;
  if(save_world_pal(mzx_world, zp,              "pal"))     goto err_close;
  if(save_world_pal_index(mzx_world, zp,        "palidx"))  goto err_close;
  if(save_world_vco(mzx_world, zp,              "vco"))     goto err_close;
  if(save_world_vch(mzx_world, zp,              "vch"))     goto err_close;

  if(savegame)
  {
    if(save_world_pal_inten(mzx_world, zp,     "palint"))   goto err_close;
    if(save_world_sprites(mzx_world, zp,       "spr"))      goto err_close;
    if(save_world_counters(mzx_world, zp,      "counter"))  goto err_close;
    if(save_world_strings(mzx_world, zp,       "string"))   goto err_close;
  }

  meter_update_screen(&meter_curr, meter_target);

  for(i = 0; i < mzx_world->num_boards; i++)
  {
    cur_board = mzx_world->board_list[i];

    if(cur_board)
      if(save_board(mzx_world, cur_board, zp, savegame, file_version, i))
        goto err_close;

    meter_update_screen(&meter_curr, meter_target);
  }

  if(mzx_world->temporary_board)
  {
    if(save_board(mzx_world, mzx_world->current_board, zp, savegame,
     file_version, TEMPORARY_BOARD))
      goto err_close;

    meter_update_screen(&meter_curr, meter_target);
  }

  meter_update_screen(&meter_curr, meter_target);

  meter_restore_screen();

  zip_close(zp, NULL);
  return 0;

err_close:
  if(zp)
    zip_close(zp, NULL);
  else
    fclose(fp);

err:
  error_message(E_WORLD_IO_SAVING, 0, NULL);
  meter_restore_screen();
  return -1;
}

#undef if_savegame
#undef if_savegame_or_291

#define if_savegame         if(!savegame) { zip_skip_file(zp); break; }
#define if_savegame_or_291  if(!savegame && mzx_world->version < V291) \
                             { zip_skip_file(zp); break; }

static int load_world_zip(struct world *mzx_world, struct zip_archive *zp,
 boolean savegame, int file_version, boolean *faded)
{
  unsigned int file_id;
  unsigned int board_id;
  enum zip_error err;

  int loaded_global_robot = 0;

  int meter_curr = 0;
  int meter_target = 2;

  meter_initial_draw(meter_curr, meter_target, "Loading...");

  // The directory has already been read by this point, and we're at the start.

  while(ZIP_SUCCESS == zip_get_next_prop(zp, &file_id, &board_id, NULL))
  {
    err = ZIP_SUCCESS;

    switch(file_id)
    {
      case FPROP_NONE:
      default:
        zip_skip_file(zp);
        break;

      case FPROP_WORLD_INFO:
      {
        load_world_info(mzx_world, zp, savegame, &file_version, faded);

        mzx_world->num_boards_allocated = mzx_world->num_boards;
        mzx_world->board_list =
         ccalloc(mzx_world->num_boards, sizeof(struct board *));

        meter_target += mzx_world->num_boards + mzx_world->temporary_board;
        meter_update_screen(&meter_curr, meter_target);
        break;
      }

      case FPROP_WORLD_GLOBAL_ROBOT:
        err = load_world_global_robot(mzx_world, zp, savegame, file_version);
        loaded_global_robot = 1;
        break;

      case FPROP_WORLD_SFX:
        err = load_world_sfx(mzx_world, zp);
        break;

      case FPROP_WORLD_CHARS:
        err = load_world_chars(mzx_world, zp, savegame);
        break;

      case FPROP_WORLD_PAL:
        err = load_world_pal(mzx_world, zp);
        break;

      case FPROP_WORLD_PAL_INDEX:
        if_savegame_or_291
        err = load_world_pal_index(mzx_world, file_version, zp);
        break;

      case FPROP_WORLD_PAL_INTENSITY:
        if_savegame
        err = load_world_pal_inten(mzx_world, zp);
        break;

      case FPROP_WORLD_VCO:
        if_savegame_or_291
        err = load_world_vco(mzx_world, zp);
        break;

      case FPROP_WORLD_VCH:
        if_savegame_or_291
        err = load_world_vch(mzx_world, zp);
        break;

      case FPROP_WORLD_SPRITES:
        if_savegame
        err = load_world_sprites(mzx_world, zp);
        break;

      case FPROP_WORLD_COUNTERS:
        if_savegame
        err = load_world_counters(mzx_world, zp);
        break;

      case FPROP_WORLD_STRINGS:
        if_savegame
        err = load_world_strings(mzx_world, zp);
        break;

      // Defer to the board loader.
      case FPROP_BOARD_INFO:
      {
        if((int)board_id < mzx_world->num_boards)
        {
          mzx_world->board_list[board_id] =
           load_board_allocate(mzx_world, zp, savegame, file_version, board_id);

          store_board_to_extram(mzx_world->board_list[board_id]);
          meter_update_screen(&meter_curr, meter_target);
        }
        else

        // Load the temporary board
        if(mzx_world->temporary_board &&
         board_id == TEMPORARY_BOARD)
        {
          mzx_world->current_board =
           load_board_allocate(mzx_world, zp, savegame, file_version, board_id);

          meter_update_screen(&meter_curr, meter_target);
        }
        break;
      }

      case FPROP_BOARD_BID:
      case FPROP_BOARD_BPR:
      case FPROP_BOARD_BCO:
      case FPROP_BOARD_UID:
      case FPROP_BOARD_UPR:
      case FPROP_BOARD_UCO:
      case FPROP_BOARD_OCH:
      case FPROP_BOARD_OCO:
      case FPROP_ROBOT:
      case FPROP_SCROLL:
      case FPROP_SENSOR:
        // Should never, ever encounter these here.
        zip_skip_file(zp);
        break;
    }

    // File failed to load? Skip it
    if(err != ZIP_SUCCESS)
    {
      zip_skip_file(zp);
      fprintf(stderr, "ERROR - Read error @ file ID %u\n", file_id);
      fflush(stderr);
    }
  }

  // Check for missing global robot
  if(!loaded_global_robot)
  {
    error_message(E_ZIP_ROBOT_MISSING_FROM_DATA, 0, "gr");

    create_blank_robot(&mzx_world->global_robot);
    create_blank_robot_program(&mzx_world->global_robot);
  }

  // Check for no title screen; make a dummy board if it's missing.
  if(!mzx_world->board_list[0])
  {
    struct board *dummy = cmalloc(sizeof(struct board));
    dummy_board(dummy);

    dummy->board_name[0] = 0;
    dummy->robot_list[0] = &mzx_world->global_robot;

    mzx_world->board_list[0] = dummy;

    error_message(E_WORLD_BOARD_MISSING, 0, NULL);
  }

  meter_update_screen(&meter_curr, meter_target);

  meter_restore_screen();

  zip_close(zp, NULL);
  return 0;
}


int save_world(struct world *mzx_world, const char *file, boolean savegame,
 int world_version)
{
#ifdef CONFIG_DEBYTECODE
  FILE *fp;

  // TODO we'll cross this bridge when we need to. That shouldn't be
  // until debytecode gets an actual release, though.

  if(world_version == MZX_VERSION_PREV)
  {
    error("Downver. not currently supported by debytecode.",
     ERROR_T_ERROR, ERROR_OPT_OK, 0x0000);
    return -1;
  }

  if(!savegame)
  {
    fp = fopen_unsafe(file, "rb");
    if(fp)
    {
      if(!fseek(fp, 0x1A, SEEK_SET))
      {
        char tmp[3];
        if(fread(tmp, 1, 3, fp) == 3)
        {
          int old_version = world_magic(tmp);

          // If it's non-debytecode, abort
          if(old_version < VERSION_SOURCE)
          {
            error_message(E_DBC_WORLD_OVERWRITE_OLD, old_version, NULL);
            fclose(fp);
            return -1;
          }
        }
      }
      fclose(fp);
    }
  }
#endif /* CONFIG_DEBYTECODE */

  // Prepare input pos
  if(!mzx_world->input_is_dir && mzx_world->input_file)
  {
    mzx_world->temp_input_pos = ftell(mzx_world->input_file);
  }
  else if(mzx_world->input_is_dir)
  {
    mzx_world->temp_input_pos = dir_tell(&mzx_world->input_directory);
  }
  else
  {
    mzx_world->temp_input_pos = 0;
  }

  // Prepare output pos
  if(mzx_world->output_file)
  {
    mzx_world->temp_output_pos = ftell(mzx_world->output_file);
  }
  else
  {
    mzx_world->temp_output_pos = 0;
  }

#ifdef CONFIG_EDITOR
  if(world_version == MZX_VERSION_PREV)
  {
    // Temporarily replace the world version with the previous version
    int actual_world_version = mzx_world->version;
    int ret_val;
    mzx_world->version = MZX_VERSION_PREV;

    ret_val = save_world_zip(mzx_world, file, savegame, MZX_VERSION_PREV);

    mzx_world->version = actual_world_version;
    return ret_val;
  }
  else
#endif

  if(world_version == MZX_VERSION)
  {
    return save_world_zip(mzx_world, file, savegame, MZX_VERSION);
  }

  else
  {
    fprintf(stderr,
     "ERROR: Attempted to save incompatible world version %d.%d! Aborting!\n",
     (world_version >> 8) & 0xFF, world_version & 0xFF);
    fflush(stderr);

    return -1;
  }
}

__editor_maybe_static
void set_update_done(struct world *mzx_world)
{
  struct board **board_list = mzx_world->board_list;
  struct board *cur_board;
  int max_size = 0;
  int cur_size;
  int i;

  for(i = 0; i < mzx_world->num_boards; i++)
  {
    cur_board = board_list[i];
    cur_size = cur_board->board_width * cur_board->board_height;

    if(cur_size > max_size)
      max_size = cur_size;
  }

  if(max_size > mzx_world->update_done_size)
  {
    if(mzx_world->update_done == NULL)
    {
      mzx_world->update_done = cmalloc(max_size);
    }
    else
    {
      mzx_world->update_done =
       crealloc(mzx_world->update_done, max_size);
    }

    mzx_world->update_done_size = max_size;
  }
}

__editor_maybe_static
void refactor_board_list(struct world *mzx_world,
 struct board **new_board_list, int new_list_size,
 int *board_id_translation_list)
{
  int i;
  int i2;
  int offset;
  char *level_id;
  char *level_param;
  int d_param, d_flag;
  int board_width;
  int board_height;
  int new_current_id = NO_BOARD;

  int num_boards = mzx_world->num_boards;
  struct board **board_list = mzx_world->board_list;
  struct board *cur_board;

  if(board_list[mzx_world->current_board_id])
    new_current_id = board_id_translation_list[mzx_world->current_board_id];

  free(board_list);
  board_list =
   crealloc(new_board_list, sizeof(struct board *) * new_list_size);

  mzx_world->num_boards = new_list_size;
  mzx_world->num_boards_allocated = new_list_size;

  // Fix all entrances and exits in each board
  for(i = 0; i < new_list_size; i++)
  {
    cur_board = board_list[i];
    board_width = cur_board->board_width;
    board_height = cur_board->board_height;

    if(i != new_current_id)
      retrieve_board_from_extram(cur_board);

    level_id = cur_board->level_id;
    level_param = cur_board->level_param;

    // Fix entrances
    for(offset = 0; offset < board_width * board_height; offset++)
    {
      d_flag = flags[(int)level_id[offset]];

      if(d_flag & A_ENTRANCE)
      {
        d_param = level_param[offset];
        if(d_param < num_boards)
          level_param[offset] = board_id_translation_list[d_param];
        else
          level_param[offset] = NO_BOARD;
      }
    }

    // Fix exits
    for(i2 = 0; i2 < 4; i2++)
    {
      d_param = cur_board->board_dir[i2];

      if(d_param < num_boards)
        cur_board->board_dir[i2] = board_id_translation_list[d_param];
      else
        cur_board->board_dir[i2] = NO_BOARD;
    }

    if(i != new_current_id)
      store_board_to_extram(cur_board);
  }

  // Fix current board
  if(new_current_id != NO_BOARD)
  {
    d_param = mzx_world->current_board_id;
    d_param = board_id_translation_list[d_param];
    mzx_world->current_board_id = d_param;

    if(!mzx_world->temporary_board)
      mzx_world->current_board = board_list[d_param];
  }

  d_param = mzx_world->first_board;
  if(d_param >= num_boards)
    d_param = num_boards - 1;

  d_param = board_id_translation_list[d_param];
  mzx_world->first_board = d_param;

  d_param = mzx_world->endgame_board;

  if(d_param != NO_BOARD)
  {
    if(d_param >= num_boards)
      d_param = num_boards - 1;

    d_param = board_id_translation_list[d_param];
    mzx_world->endgame_board = d_param;
  }

  d_param = mzx_world->death_board;

  if((d_param != NO_BOARD) && (d_param != DEATH_SAME_POS))
  {
    if(d_param >= num_boards)
      d_param = num_boards - 1;

    d_param = board_id_translation_list[d_param];
    mzx_world->death_board = d_param;
  }

  mzx_world->board_list = board_list;
}

__editor_maybe_static
void optimize_null_boards(struct world *mzx_world)
{
  // Optimize out null objects while keeping a translation list mapping
  // board numbers from the old list to the new list.
  int num_boards = mzx_world->num_boards;
  struct board **board_list = mzx_world->board_list;
  struct board **optimized_board_list =
   ccalloc(num_boards, sizeof(struct board *));
  int *board_id_translation_list = ccalloc(num_boards, sizeof(int));

  struct board *cur_board;
  int i, i2;

  for(i = 0, i2 = 0; i < num_boards; i++)
  {
    cur_board = board_list[i];
    if(cur_board != NULL)
    {
      optimized_board_list[i2] = cur_board;
      board_id_translation_list[i] = i2;
      i2++;
    }
    else
    {
      board_id_translation_list[i] = NO_BOARD;
    }
  }

  // Might need to fix these first, to correct old MZX games.
  if(mzx_world->first_board >= num_boards)
    mzx_world->first_board = 0;

  if((mzx_world->death_board >= num_boards) &&
   (mzx_world->death_board < DEATH_SAME_POS))
    mzx_world->death_board = NO_BOARD;

  if(mzx_world->endgame_board >= num_boards)
    mzx_world->endgame_board = NO_BOARD;

  if(i2 < num_boards)
  {
    refactor_board_list(mzx_world, optimized_board_list, i2,
     board_id_translation_list);
  }
  else
  {
    free(optimized_board_list);
  }

  free(board_id_translation_list);
}

#ifdef CONFIG_DEBYTECODE

static void convert_sfx_strs(char *sfx_buf)
{
  char *start, *end = sfx_buf - 1, str_buf_len = strlen(sfx_buf);

  while(true)
  {
    // no starting & was found
    start = strchr(end + 1, '&');
    if(!start || start - sfx_buf + 1 > str_buf_len)
      break;

    // no ending & was found
    end = strchr(start + 1, '&');
    if(!end || end - sfx_buf + 1 > str_buf_len)
      break;

    // Wipe out the &s to get a token
    *start = '(';
    *end = ')';
  }
}

#endif /* CONFIG_DEBYTECODE */


static void load_world(struct world *mzx_world, struct zip_archive *zp,
 FILE *fp, const char *file, boolean savegame, int file_version, char *name,
 boolean *faded)
{
  size_t file_name_len = strlen(file) - 4;
  char config_file_name[MAX_PATH];
  char current_dir[MAX_PATH];
  char file_path[MAX_PATH];
  struct stat file_info;

  get_path(file, file_path, MAX_PATH);

  // chdir to game directory
  if(file_path[0])
  {
    getcwd(current_dir, MAX_PATH);

    if(strcmp(current_dir, file_path))
      chdir(file_path);
  }

  // load world config file
  memcpy(config_file_name, file, file_name_len);
  strncpy(config_file_name + file_name_len, ".cnf", 5);

  if(stat(config_file_name, &file_info) >= 0)
    set_config_from_file(config_file_name);

  // Some initial setting(s)
  mzx_world->custom_sfx_on = 0;
  mzx_world->max_samples = -1;
  mzx_world->joystick_simulate_keys = true;

  // If we're here, there's either a zip (regular) or a file (legacy).
  if(zp)
  {
    load_world_zip(mzx_world, zp, savegame, file_version, faded);
  }
  else
  {
    legacy_load_world(mzx_world, fp, file, savegame, file_version, name, faded);
  }

  update_palette();

  initialize_gateway_functions(mzx_world);

#ifdef CONFIG_DEBYTECODE
  // Convert SFX strings if needed
  if(file_version < VERSION_SOURCE)
  {
    char *sfx_offset = mzx_world->custom_sfx;
    int i;

    for(i = 0; i < NUM_SFX; i++, sfx_offset += SFX_SIZE)
      convert_sfx_strs(sfx_offset);
  }
#endif

  // Open input file
  if(mzx_world->input_file_name[0])
  {
    char translated_path[MAX_PATH];
    int err;

    mzx_world->input_is_dir = false;

    err = fsafetranslate(mzx_world->input_file_name, translated_path);
    if(err == -FSAFE_MATCHED_DIRECTORY)
    {
      if(dir_open(&mzx_world->input_directory, translated_path))
      {
        dir_seek(&mzx_world->input_directory, mzx_world->temp_input_pos);
        mzx_world->input_is_dir = true;
      }
    }
    else if(err == -FSAFE_SUCCESS)
    {
      mzx_world->input_file = fopen_unsafe(translated_path, "rb");
      if(mzx_world->input_file)
        fseek(mzx_world->input_file, mzx_world->temp_input_pos, SEEK_SET);
    }
  }

  // Open output file
  if(mzx_world->output_file_name[0])
  {
    mzx_world->output_file =
     fsafeopen(mzx_world->output_file_name, "ab");

    if(mzx_world->output_file)
    {
      fseek(mzx_world->output_file, mzx_world->temp_output_pos, SEEK_SET);
    }
  }

  // This pointer is now invalid. Clear it before we try to
  // send it back to extra RAM.
  if(!mzx_world->temporary_board)
  {
    mzx_world->current_board = NULL;
    set_current_board_ext(mzx_world,
     mzx_world->board_list[mzx_world->current_board_id]);
  }

  // If this is a pre-port world, limit the number of samples
  // playing to 4 (the maximum number in pre-port MZX)
  if(mzx_world->version < VERSION_PORT)
    mzx_world->max_samples = 4;

  // This will be -1 (no limit) or whatever was loaded from a save
  audio_set_max_samples(mzx_world->max_samples);

  // This will generally be 'true' unless it was different in the save.
  joystick_set_game_bindings(mzx_world->joystick_simulate_keys);

  mzx_world->active = 1;

  // Remove any null boards
  optimize_null_boards(mzx_world);

  // Resize this array if necessary
  set_update_done(mzx_world);

  // Find the player
  find_player(mzx_world);
}


/**
 * Open the world file and attempt to read the world header.
 * Doesn't validate any read info; only return NULL if there was an IO error.
 */

static FILE *try_open_world(const char *file, boolean savegame,
 int *file_version, int *protected, char *name)
{
  char magic[5];
  int pr = 0;
  int v;

  FILE *fp = fopen_unsafe(file, "rb");
  if(!fp)
    return NULL;

  // TODO temporary fix to improve load times on the embedded platforms.
  setvbuf(fp, NULL, _IOFBF, 16384);

  if(savegame)
  {
    if(!fread(magic, 5, 1, fp))
      goto err_close;

    v = save_magic(magic);
  }

  else
  {
    if(!fread(name, BOARD_NAME_SIZE, 1, fp))
      goto err_close;

    // Protection byte
    pr = fgetc(fp);
    if(protected)
      *protected = pr;

    if(!fread(magic, 3, 1, fp))
      goto err_close;

    v = world_magic(magic);
  }

  if(protected) *protected = pr;
  if(file_version) *file_version = v;
  return fp;

err_close:
  fclose(fp);
  return NULL;
}

static struct zip_archive *try_load_zip_world(struct world *mzx_world,
 const char *file, boolean savegame, int *file_version, int *protected,
 char *name)
{
  struct zip_archive *zp = NULL;
  FILE *fp;
  int pr = 0;
  int v;

  int result;

  fp = try_open_world(file, savegame, &v, &pr, name);
  if(!fp)
    return NULL;

  *file_version = v;

  /* If we got something useful from the version number, and it's legacy:
   * - and it's a save, then it's not a headerless zip by default
   * - and it's a regular world, then the first file name would have to be
   *   extremely long to produce a valid version number from a headerless zip
   *
   * So we can just fail safely and let the legacy loader pick it up.
   */

  if(pr > 0 && pr <= 3 && strncmp(name, "PK", 2))
    goto err_protected;

  if(v > 0 && v <= MZX_LEGACY_FORMAT_VERSION)
    goto err_close;

  // Get the actual file version out of the world metadata
  *file_version = 0;
  v = 0;

  zp = zip_open_fp_read(fp);

  if(!zp)
  {
    fp = NULL;
    goto err_close;
  }

  result = validate_world_zip(mzx_world, zp, savegame, file_version);

  if(result != VAL_SUCCESS)
    goto err_close;

  // Should never happen, but if it did, it's totally invalid.
  if(*file_version <= MZX_LEGACY_FORMAT_VERSION)
    goto err_close;

  v = *file_version;
  if(v > MZX_VERSION)
    goto err_close;

  zip_rewind(zp);

  reset_error_suppression();
  return zp;

err_close:

  // Display an appropriate error.
  if(savegame)
  {
    if(v > MZX_VERSION)
    {
      error_message(E_SAVE_VERSION_TOO_RECENT, v, NULL);
    }
    else

    // Allow the last pre-ZIP version, but none before it
    if(v > 0 && v < MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_SAVE_VERSION_OLD, v, NULL);
    }
    else

    if(v != MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_SAVE_FILE_INVALID, 0, NULL);
    }
  }

  else
  {
    if(v > MZX_VERSION)
    {
      error_message(E_WORLD_FILE_VERSION_TOO_RECENT, v, NULL);
    }
    else

    if(v > 0 && v < V251)
    {
      error_message(E_WORLD_FILE_VERSION_OLD, v, NULL);
    }
    else

    if(v == 0 || v > MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_WORLD_FILE_INVALID, 0, NULL);
    }
  }

err_protected:
  *protected = pr;
  if(zp)
  {
    zip_close(zp, NULL);
  }
  else

  if(fp)
    fclose(fp);

  return NULL;
}

static FILE *try_load_legacy_world(struct world *mzx_world, const char *file,
 boolean savegame, int *file_version, char *name)
{
  FILE *fp;
  enum val_result result;

  // Validate the legacy world file and attempt decryption as needed.
  result = validate_legacy_world_file(mzx_world, file, savegame);

  if(result != VAL_SUCCESS)
    return NULL;

  // We don't care about the value of the protected byte since the world
  // should be decrypted if we've made it this far.
  fp = try_open_world(file, savegame, file_version, NULL, name);

  reset_error_suppression();
  return fp;
}

__editor_maybe_static
void try_load_world(struct world *mzx_world, struct zip_archive **zp,
 FILE **fp, const char *file, boolean savegame, int *file_version, char *name)
{
  // Regular worlds use a zip_archive. Legacy worlds use a FILE.
  struct zip_archive *_zp = NULL;
  FILE *_fp = NULL;
  int protected = 0;
  int v = 0;

  _zp = try_load_zip_world(mzx_world, file, savegame, &v, &protected, name);

  if(!_zp)
    if(protected || (v >= V251 && v <= MZX_LEGACY_FORMAT_VERSION))
      _fp = try_load_legacy_world(mzx_world, file, savegame, &v, name);

  *zp = _zp;
  *fp = _fp;
  *file_version = v;
}


void change_board(struct world *mzx_world, int board_id)
{
  // Set the current board during gameplay.
  struct board *cur_board = mzx_world->current_board;

  // Is this board temporary? Clear it
  if(mzx_world->temporary_board)
  {
    assert(cur_board != NULL);
    assert(cur_board->reset_on_entry != 0);

    clear_board(cur_board);
    mzx_world->current_board = NULL;
    mzx_world->temporary_board = 0;
  }

  mzx_world->current_board_id = board_id;
  set_current_board_ext(mzx_world, mzx_world->board_list[board_id]);

  cur_board = mzx_world->current_board;

  // Does this board need a duplicate? (2.90+)
  if(mzx_world->version >= V290 && cur_board->reset_on_entry)
  {
    struct board *dup_board = duplicate_board(mzx_world, cur_board);
    store_board_to_extram(cur_board);

    mzx_world->current_board = dup_board;
    mzx_world->temporary_board = 1;
  }
}

void change_board_set_values(struct world *mzx_world)
{
  // The sequel to change_board; set special values on board (re)entry.
  struct board *cur_board = mzx_world->current_board;
  struct player *player = &mzx_world->players[0];

  // Set the timer.
  set_counter(mzx_world, "TIME", cur_board->time_limit, 0);

  // Set the player restart position.
  find_player(mzx_world);
  mzx_world->player_restart_x = player->x;
  mzx_world->player_restart_y = player->y;
}

void change_board_load_assets(struct world *mzx_world)
{
  // The sequel to change_board; if there's a fadeout, use this after.
  struct board *cur_board = mzx_world->current_board;
  char translated_name[MAX_PATH];

  // Does this board need a char set loaded? (2.90+)
  if(mzx_world->version >= V290 && cur_board->charset_path[0])
  {
    if(fsafetranslate(cur_board->charset_path, translated_name) == FSAFE_SUCCESS)
    {
      // Bug: ec_load_set cleared the extended chars prior to 2.91e
      if(mzx_world->version < V291)
        ec_clear_set();

      ec_load_set(translated_name);
    }
  }

  // Does this board need a palette loaded? (2.90+)
  if(mzx_world->version >= V290 && cur_board->palette_path[0])
  {
    if(fsafetranslate(cur_board->palette_path, translated_name) == FSAFE_SUCCESS)
      load_palette(translated_name);
  }
}

// This needs to happen before a world is loaded if clear_global_data was used.

static void default_sprite_data(struct world *mzx_world)
{
  int i;

  // Allocate space for sprites and clist
  mzx_world->num_sprites = MAX_SPRITES;
  mzx_world->sprite_list = ccalloc(MAX_SPRITES, sizeof(struct sprite *));

  for(i = 0; i < MAX_SPRITES; i++)
  {
    mzx_world->sprite_list[i] = ccalloc(1, sizeof(struct sprite));
  }

  mzx_world->collision_list = ccalloc(MAX_SPRITES, sizeof(int));
  mzx_world->sprite_num = 0;
}

// This also needs to happen before a world is loaded.

__editor_maybe_static void default_vlayer(struct world *mzx_world)
{
  // Allocate space for vlayer.
  mzx_world->vlayer_size = 0x8000;
  mzx_world->vlayer_width = 256;
  mzx_world->vlayer_height = 128;
  mzx_world->vlayer_chars = cmalloc(0x8000);
  mzx_world->vlayer_colors = cmalloc(0x8000);
  memset(mzx_world->vlayer_chars, 32, 0x8000);
  memset(mzx_world->vlayer_colors, 7, 0x8000);
}

// After loading, use this to get default values. Use
// for loading of worlds (as opposed to save games).

__editor_maybe_static void default_global_data(struct world *mzx_world)
{
  int i;

  // This might be a new world, so make sure we have sprites.
  if(!mzx_world->sprite_list)
    default_sprite_data(mzx_world);

  // Set some default counter values
  // The others have to be here so their gateway functions will stick
  set_counter(mzx_world, "AMMO", 0, 0);
  set_counter(mzx_world, "COINS", 0, 0);
  set_counter(mzx_world, "ENTER_MENU", 1, 0);
  set_counter(mzx_world, "ESCAPE_MENU", 1, 0);
  set_counter(mzx_world, "F2_MENU", 1, 0);
  set_counter(mzx_world, "GEMS", 0, 0);
  set_counter(mzx_world, "HEALTH", mzx_world->starting_health, 0);
  set_counter(mzx_world, "HELP_MENU", 1, 0);
  set_counter(mzx_world, "HIBOMBS", 0, 0);
  set_counter(mzx_world, "INVINCO", 0, 0);
  set_counter(mzx_world, "LIVES", mzx_world->starting_lives, 0);
  set_counter(mzx_world, "LOAD_MENU", 1, 0);
  set_counter(mzx_world, "LOBOMBS", 0, 0);
  set_counter(mzx_world, "SCORE", 0, 0);
  set_counter(mzx_world, "TIME", 0, 0);

  // Setup their gateways
  initialize_gateway_functions(mzx_world);

  mzx_world->multiplier = 10000;
  mzx_world->divider = 10000;
  mzx_world->c_divisions = 360;
  mzx_world->fread_delimiter = '*';
  mzx_world->fwrite_delimiter = '*';
  mzx_world->bi_shoot_status = 1;
  mzx_world->bi_mesg_status = 1;

  // Default values for global params
  memset(mzx_world->keys, NO_KEY, NUM_KEYS);
  mzx_world->mesg_edges = 1;
  mzx_world->real_mod_playing[0] = 0;
  mzx_world->smzx_message = 1;
  // In 2.90X, due to a bug the message could only display in mode 0.
  if(mzx_world->version == V290)
    mzx_world->smzx_message = 0;

  mzx_world->blind_dur = 0;
  mzx_world->firewalker_dur = 0;
  mzx_world->freeze_time_dur = 0;
  mzx_world->slow_time_dur = 0;
  mzx_world->wind_dur = 0;

  for(i = 0; i < 8; i++)
  {
    mzx_world->pl_saved_x[i] = 0;
    mzx_world->pl_saved_y[i] = 0;
    mzx_world->pl_saved_board[i] = 0;
  }

  mzx_world->saved_pl_color = 27;
  mzx_world->player_restart_x = 0;
  mzx_world->player_restart_y = 0;
  mzx_world->under_player_id = 0;
  mzx_world->under_player_color = 7;
  mzx_world->under_player_param = 0;

  mzx_world->commands = 40;
  mzx_world->commands_stop = 2000000;

  default_scroll_values(mzx_world);

  scroll_color = 15;

  mzx_world->lock_speed = 0;
  mzx_world->mzx_speed = get_config()->mzx_speed;

  assert(mzx_world->input_file == NULL);
  assert(mzx_world->output_file == NULL);
  assert(mzx_world->input_is_dir == false);

  mzx_world->target_where = TARGET_NONE;
}

boolean reload_world(struct world *mzx_world, const char *file, boolean *faded)
{
  char name[BOARD_NAME_SIZE];
  int version;

  struct zip_archive *zp;
  FILE *fp;

  try_load_world(mzx_world, &zp, &fp, file, false, &version, name);

  if(!zp && !fp)
    return false;

  if(mzx_world->active)
  {
    clear_world(mzx_world);
    clear_global_data(mzx_world);
  }

  // Always switch back to regular mode before loading the world,
  // because we want the world's intrinsic palette to be applied.
  set_screen_mode(0);
  smzx_palette_loaded(0);
  set_palette_intensity(100);

  default_sprite_data(mzx_world);
  default_vlayer(mzx_world);

  load_world(mzx_world, zp, fp, file, false, version, name, faded);
  default_global_data(mzx_world);
  *faded = false;

  // Now that the world's loaded, fix the save path.
  {
    char save_name[MAX_PATH];
    char current_dir[MAX_PATH];
    getcwd(current_dir, MAX_PATH);

    split_path_filename(curr_sav, NULL, 0, save_name, MAX_PATH);
    join_path_names(curr_sav, MAX_PATH, current_dir, save_name);
  }

  return true;
}

boolean reload_savegame(struct world *mzx_world, const char *file,
 boolean *faded)
{
  char ignore[BOARD_NAME_SIZE];
  int version;

  struct zip_archive *zp;
  FILE *fp;

  try_load_world(mzx_world, &zp, &fp, file, true, &version, ignore);

  if(!zp && !fp)
    return false;

  // It is, so wipe the old world
  if(mzx_world->active)
  {
    clear_world(mzx_world);
    clear_global_data(mzx_world);
  }

  default_sprite_data(mzx_world);
  default_vlayer(mzx_world);

  // And load the new one
  load_world(mzx_world, zp, fp, file, true, version, NULL, faded);
  return true;
}

boolean reload_swap(struct world *mzx_world, const char *file, boolean *faded)
{
  char name[BOARD_NAME_SIZE];
  char full_path[MAX_PATH];
  char file_name[MAX_PATH];
  int version;

  struct zip_archive *zp;
  FILE *fp;

  try_load_world(mzx_world, &zp, &fp, file, false, &version, name);

  if(!zp && !fp)
    return false;

  if(mzx_world->active)
    clear_world(mzx_world);

  load_world(mzx_world, zp, fp, file, false, version, name, faded);

  // Change to the first board of the new world.
  change_board(mzx_world, mzx_world->first_board);
  change_board_set_values(mzx_world);
  change_board_load_assets(mzx_world);

  // Give curr_file a full path
  getcwd(full_path, MAX_PATH);
  split_path_filename(file, NULL, 0, file_name, MAX_PATH);
  join_path_names(curr_file, MAX_PATH, full_path, file_name);

  return true;
}

// This only clears boards, no global data. Useful for swap world,
// when you want to maintain counters and sprites and all that.

void clear_world(struct world *mzx_world)
{
  // Do this before loading, when there's a world

  int i;
  int player_id;
  int num_boards = mzx_world->num_boards;
  struct board **board_list = mzx_world->board_list;

  for(i = 0; i < num_boards; i++)
  {
    if(mzx_world->current_board_id != i)
      retrieve_board_from_extram(board_list[i]);
    clear_board(board_list[i]);
  }
  free(board_list);

  // Make sure we nuke the duplicate too, if it exists.
  if(mzx_world->temporary_board)
  {
    clear_board(mzx_world->current_board);
  }

  mzx_world->temporary_board = 0;
  mzx_world->current_board_id = 0;
  mzx_world->current_board = NULL;
  mzx_world->board_list = NULL;

  clear_robot_contents(&mzx_world->global_robot);

  if(!mzx_world->input_is_dir && mzx_world->input_file)
  {
    fclose(mzx_world->input_file);
    mzx_world->input_file = NULL;
  }
  else if(mzx_world->input_is_dir)
  {
    dir_close(&mzx_world->input_directory);
    mzx_world->input_is_dir = false;
  }

  if(mzx_world->output_file)
  {
    fclose(mzx_world->output_file);
    mzx_world->output_file = NULL;
  }

  mzx_world->current_cycle_odd = false;
  mzx_world->current_cycle_frozen = false;
  for(player_id = 0; player_id < NUM_PLAYERS; player_id++)
  {
    struct player *player = &mzx_world->players[player_id];
    player->shoot_cooldown = 0;
  }
  mzx_world->active = 0;

  audio_end_sample();
}

// This clears the rest of the stuff.

void clear_global_data(struct world *mzx_world)
{
  int i;
  struct sprite **sprite_list = mzx_world->sprite_list;

  free(mzx_world->vlayer_chars);
  free(mzx_world->vlayer_colors);
  mzx_world->vlayer_chars = NULL;
  mzx_world->vlayer_colors = NULL;

  // Clear all counters out of the counter list
  clear_counter_list(&(mzx_world->counter_list));

  // Clear all strings out of the string list
  clear_string_list(&(mzx_world->string_list));

  for(i = 0; i < MAX_SPRITES; i++)
  {
    free(sprite_list[i]);
  }

  free(sprite_list);
  mzx_world->sprite_list = NULL;
  mzx_world->num_sprites = 0;

  free(mzx_world->collision_list);
  mzx_world->collision_list = NULL;
  mzx_world->collision_count = 0;

  mzx_world->active_sprites = 0;
  mzx_world->sprite_y_order = 0;

  mzx_world->vlayer_size = 0;
  mzx_world->vlayer_width = 0;
  mzx_world->vlayer_height = 0;

  mzx_world->output_file_name[0] = 0;
  mzx_world->input_file_name[0] = 0;

  mzx_world->robotic_save_type = SAVE_NONE;

  memset(mzx_world->custom_sfx, 0, NUM_SFX * SFX_SIZE);
  mzx_world->custom_sfx_on = 0;

  mzx_world->max_samples = -1;
  audio_set_max_samples(mzx_world->max_samples);

  mzx_world->joystick_simulate_keys = true;
  joystick_set_game_bindings(mzx_world->joystick_simulate_keys);

  mzx_world->bomb_type = 1;
  mzx_world->dead = false;
}

void default_scroll_values(struct world *mzx_world)
{
  mzx_world->scroll_base_color = 143;
  mzx_world->scroll_corner_color = 135;
  mzx_world->scroll_pointer_color = 128;
  mzx_world->scroll_title_color = 143;
  mzx_world->scroll_arrow_color = 142;
}

void remap_vlayer(struct world *mzx_world,
 int new_width, int new_height)
{
  // Given a new width and height, translate the vlayer such that the area
  // where the old dimensions and the new dimensions overlap is the same, and
  // everything else is blank.

  char *vlayer_chars = mzx_world->vlayer_chars;
  char *vlayer_colors = mzx_world->vlayer_colors;
  int vlayer_size = mzx_world->vlayer_size;

  int old_width = mzx_world->vlayer_width;
  int old_height = mzx_world->vlayer_height;
  int new_pos = 0;
  int old_pos = 0;
  int i;

  if(old_width * old_height > vlayer_size)
  {
    old_height = vlayer_size / old_width;
  }

  if(new_width < old_width)
  {
    // Decreased width -- go from start to end

    for(i = 0; i < old_height; i++)
    {
      memmove(vlayer_chars + new_pos, vlayer_chars + old_pos, new_width);
      memmove(vlayer_colors + new_pos, vlayer_colors + old_pos, new_width);

      old_pos += old_width;
      new_pos += new_width;
    }

    // Clear everything else
    memset(vlayer_chars + new_pos, 32, vlayer_size - new_pos);
    memset(vlayer_colors + new_pos, 7, vlayer_size - new_pos);
  }
  else

  if(new_width > old_width)
  {
    // Increased width -- go from end to start
    // Clear blank areas after every copy
    int clear_width = new_width - old_width;

    new_pos = new_width * (new_height - 1);
    old_pos = old_width * (new_height - 1);

    for(i = 0; i < new_height; i++)
    {
      memmove(vlayer_chars + new_pos, vlayer_chars + old_pos, new_width);
      memmove(vlayer_colors + new_pos, vlayer_colors + old_pos, new_width);

      // Clear blank area
      memset(vlayer_chars + new_pos + old_width, 32, clear_width);
      memset(vlayer_colors + new_pos + old_width, 7, clear_width);

      old_pos -= old_width;
      new_pos -= new_width;
    }

    // Clear anything after the new end
    new_pos = new_width * new_height;
    memset(vlayer_chars + new_pos, 32, vlayer_size - new_pos);
    memset(vlayer_colors + new_pos, 7, vlayer_size - new_pos);
  }

  mzx_world->vlayer_width = new_width;
  mzx_world->vlayer_height = new_height;
}
