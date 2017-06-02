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

#include "configure.h"
#include "helpsys.h"
#include "sfx.h"
#include "scrdisp.h"
#include "error.h"
#include "window.h"
#include "const.h"
#include "counter.h"
#include "sprite.h"
#include "counter.h"
#include "graphics.h"
#include "event.h"
#include "world.h"
#include "data.h"
#include "idput.h"
#include "fsafeopen.h"
#include "game.h"
#include "audio.h"
#include "extmem.h"
#include "legacy_world.h"
#include "zip.h"
#include "util.h"


#ifdef CONFIG_LOADSAVE_METER

void meter_update_screen(int *curr, int target)
{
  (*curr)++;
  meter_interior(*curr, target);
  update_screen();
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
}

#else //!CONFIG_LOADSAVE_METER

inline void meter_update_screen(int *curr, int target) {}
inline void meter_restore_screen(void) {}
inline void meter_initial_draw(int curr, int target, const char *title) {}

#endif //CONFIG_LOADSAVE_METER


#define PROP_HEADER_SIZE 4

// IF YOU ADD ANYTHING, MAKE SURE THIS GETS UPDATED!
#define COUNT_WORLD_PROPS ( 2 +   5 + 16)
#define BOUND_WORLD_PROPS (27 + 455 + 24)

#define COUNT_SAVE_PROPS  ( 2 +   4 +   24)
#define BOUND_SAVE_PROPS  ( 3 +   9 + 1632 + NUM_KEYS)

enum world_prop
{
  WPROP_EOF                       = 0x0000, // Size bound (save)

  // Header redundant properties      2 (4)     27  (30)
  WPROP_WORLD_NAME                = 0x0001, //  25
  WPROP_WORLD_VERSION             = 0x0002, //   2
  WPROP_SAVE_VERSION              = 0x0003, //       (2)
  WPROP_SAVE_START_BOARD          = 0x0004, //       (1)

  // ID Chars                            4     455
  WPROP_ID_CHARS                  = 0x0010, // 323
  WPROP_ID_MISSILE_COLOR          = 0x0011, //   1
  WPROP_ID_BULLET_COLOR           = 0x0012, //   3
  WPROP_ID_DMG                    = 0x0013, // 128

  // Global properties                  16      24
  WPROP_EDGE_COLOR                = 0x0020, //   1
  WPROP_FIRST_BOARD               = 0x0021, //   1
  WPROP_ENDGAME_BOARD             = 0x0022, //   1
  WPROP_DEATH_BOARD               = 0x0023, //   1
  WPROP_ENDGAME_X                 = 0x0024, //   2
  WPROP_ENDGAME_Y                 = 0x0025, //   2
  WPROP_GAME_OVER_SFX             = 0x0026, //   1
  WPROP_DEATH_X                   = 0x0027, //   2
  WPROP_DEATH_Y                   = 0x0028, //   2
  WPROP_STARTING_LIVES            = 0x0029, //   2
  WPROP_LIVES_LIMIT               = 0x002A, //   2
  WPROP_STARTING_HEALTH           = 0x002B, //   2
  WPROP_HEALTH_LIMIT              = 0x002C, //   2
  WPROP_ENEMY_HURT_ENEMY          = 0x002D, //   1
  WPROP_CLEAR_ON_EXIT             = 0x002E, //   1
  WPROP_ONLY_FROM_SWAP            = 0x002F, //   1

  // Temporarily save-only               4       9
  WPROP_SMZX_MODE                 = 0x0030, //   1
  WPROP_VLAYER_WIDTH              = 0x0031, //   2
  WPROP_VLAYER_HEIGHT             = 0x0032, //   2
  WPROP_VLAYER_SIZE               = 0x0033, //   4

  // Save properties                    24    1632 + NUM_KEYS
  WPROP_REAL_MOD_PLAYING          = 0x0040, // 512
  WPROP_MZX_SPEED                 = 0x0041, //   1
  WPROP_LOCK_SPEED                = 0x0042, //   1
  WPROP_COMMANDOS                 = 0x0043, //   4
  WPROP_SAVED_POSITIONS           = 0x0048, //  40 (2+2+1)*8
  WPROP_PLAYER_RESTART_X          = 0x0049, //   2
  WPROP_PLAYER_RESTARY_Y          = 0x004A, //   2
  WPROP_SAVED_PL_COLOR            = 0x004B, //   1
  WPROP_UNDER_PLAYER              = 0x004C, //   3 (1+1+1)
  WPROP_KEYS                      = 0x004D, // NUM_KEYS
  WPROP_DURATIONS                 = 0x0050, //   5 (1+1+1+1+1)
  WPROP_SCROLL_COLS               = 0x0051, //   5 (1+1+1+1+1)
  WPROP_MESG_EDGES                = 0x0052, //   1
  WPROP_BI_SHOOT_STATUS           = 0x0053, //   1
  WPROP_BI_MESG_STATUS            = 0x0054, //   1
  WPROP_FADED                     = 0x0055, //   1
  WPROP_INPUT_FILE_NAME           = 0x0060, // 512
  WPROP_INPUT_POS                 = 0x0064, //   4
  WPROP_FREAD_DELIMITER           = 0x0065, //   4
  WPROP_OUTPUT_FILE_NAME          = 0x0068, // 512
  WPROP_OUTPUT_POS                = 0x006C, //   4
  WPROP_FWRITE_DELIMITER          = 0x006D, //   4
  WPROP_MULTIPLIER                = 0x0070, //   4
  WPROP_DIVIDER                   = 0x0071, //   4
  WPROP_C_DIVISIONS               = 0x0072, //   4
};

static inline int world_prop_file_size(void)
{
  return (BOUND_WORLD_PROPS + PROP_HEADER_SIZE * COUNT_WORLD_PROPS) + 2;
}

static inline int save_prop_extra_file_size(void)
{
  return (BOUND_SAVE_PROPS + PROP_HEADER_SIZE * COUNT_SAVE_PROPS) + 2;
}


#define COUNT_SPRITE_PROPS 13
#define BOUND_SPRITE_PROPS 20

enum sprite_prop
{
  SPROP_EOF               = 0x00,
  SPROP_X                 = 0x01, // 2
  SPROP_Y                 = 0x02, // 2
  SPROP_REF_X             = 0x03, // 2
  SPROP_REF_Y             = 0x04, // 2
  SPROP_COLOR             = 0x05, // 1
  SPROP_FLAGS             = 0x06, // 1
  SPROP_WIDTH             = 0x07, // 1
  SPROP_HEIGHT            = 0x08, // 1
  SPROP_COL_X             = 0x09, // 1
  SPROP_COL_Y             = 0x0A, // 1
  SPROP_COL_WIDTH         = 0x0B, // 1
  SPROP_COL_HEIGHT        = 0x0C, // 1
  SPROP_TRANSPARENT_COLOR = 0x0D, // 4
};

static inline int sprite_prop_file_size(void)
{
  return
   (BOUND_SPRITE_PROPS + PROP_HEADER_SIZE * COUNT_SPRITE_PROPS)
   * MAX_SPRITES + 2;
}


// This function is used to save properties files in world loading.
// There are no safety checks here. USE THE BOUNDING FUNCTIONS WHEN ALLOCATING.
static inline void save_prop_eof(struct memfile *mf)
{
  mfputw(0, mf);
}

static inline void save_prop_c(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputw(1, mf);
  mfputc(value, mf);
}

static inline void save_prop_w(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputw(2, mf);
  mfputw(value, mf);
}

static inline void save_prop_d(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputw(4, mf);
  mfputd(value, mf);
}

static inline void save_prop_m(int ident, void *src, size_t len, size_t count,
 struct memfile *mf)
{
  mfputw(ident, mf);
  mfputw(len * count, mf);
  mfwrite(src, len, count, mf);
}

// This function is used to read properties files in world loading.
int next_prop(struct memfile *prop, int *ident, int *length,
 struct memfile *mf)
{
  char *end = mf->end;
  char *cur;
  int len;
  int id;

  if((end - mf->current)<PROP_HEADER_SIZE)
  {
    *ident = 0;
    return 0;
  }

  id = mfgetw(mf);
  len = mfgetw(mf);
  cur = mf->current;

  if((end - cur)<len)
  {
    *ident = 0;
    return 0;
  }

  *ident = id;
  *length = len;
  prop->current = cur;
  prop->start = cur;
  prop->end = cur + len;

  mf->current += len;
  return 1;
}

static inline void save_world_info(struct world *mzx_world,
 struct zip_archive *zp, int savegame, const char *name, enum file_prop file_id)
{
}

static inline int load_world_info(struct world *mzx_world,
 struct zip_archive *zp, int savegame)
{
  return 0;
}

static inline void save_world_global_robot(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_global_robot(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_stat_counters(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_stat_counters(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_sfx(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_sfx(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_board_names(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_board_names(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_chars(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_chars(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_pal(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_pal(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_pal_index(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_pal_index(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_pal_inten(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_pal_inten(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_vco(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_vco(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_vch(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_vch(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_sprites(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
}

static inline void load_world_sprites(struct world *mzx_world,
 struct zip_archive *zp)
{
}

static inline void save_world_counters(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
  struct counter *src_counter;
  size_t name_length;
  int i;

  enum zip_error result;

  if(ZIP_SUCCESS !=
   zip_write_open_file_stream(zp, name, ZIP_M_NONE, file_id, 0, 0))
    return;

  zputd(mzx_world->num_counters, zp);

  for(i = 0; i < mzx_world->num_counters; i++)
  {
    src_counter = mzx_world->counter_list[i];
    name_length = strlen(src_counter->name);

    zputd(src_counter->value, zp);
    zputd(name_length, zp);
    result = zwrite(src_counter->name, name_length, zp);
    if(result != ZIP_SUCCESS)
      break;
  }

  zip_write_close_stream(zp);
}

static inline void load_world_counters(struct world *mzx_world,
 struct zip_archive *zp)
{
  char name_buffer[ROBOT_MAX_TR];
  size_t name_length;
  int value;

  int num_prev_allocated;
  int num_counters;
  int i;

  enum zip_error result;

  if(ZIP_SUCCESS !=
   zip_read_open_file_stream(zp, NULL, 0, NULL))
    return;

  num_counters = zgetd(zp, &result);
  num_prev_allocated = mzx_world->num_counters_allocated;

  // If there aren't already any counters, allocate manually.
  if(!num_prev_allocated)
  {
    mzx_world->num_counters = num_counters;
    mzx_world->num_counters_allocated = num_counters;
    mzx_world->counter_list = ccalloc(num_counters, sizeof(struct counter *));
  }

  for(i = 0; i < num_counters; i++)
  {
    value = zgetd(zp, &result);
    name_length = zgetd(zp, &result);

    if(name_length >= ROBOT_MAX_TR)
      break;

    if(ZIP_SUCCESS != zread(name_buffer, name_length, zp))
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
      mzx_world->counter_list[i] =
       load_new_counter(name_buffer, name_length, value);
    }
  }

  // If there weren't any previously allocated, the number successfully read is
  // the new number of counters.
  if(!num_prev_allocated)
    mzx_world->num_counters = i;

  zip_read_close_stream(zp);
}

static inline void save_world_strings(struct world *mzx_world,
 struct zip_archive *zp, const char *name, enum file_prop file_id)
{
  struct string *src_string;
  size_t name_length;
  size_t str_length;
  int i;

  enum zip_error result;

  if(ZIP_SUCCESS !=
   zip_write_open_file_stream(zp, name, ZIP_M_NONE, file_id, 0, 0))
    return;

  zputd(mzx_world->num_strings, zp);

  for(i = 0; i < mzx_world->num_strings; i++)
  {
    src_string = mzx_world->string_list[i];
    name_length = strlen(src_string->name);
    str_length = src_string->length;

    zputd(name_length, zp);
    zputd(str_length, zp);
    zwrite(src_string->name, name_length, zp);
    result = zwrite(src_string->value, str_length, zp);
    if(result != ZIP_SUCCESS)
      break;
  }

  zip_write_close_stream(zp);
}

static inline void load_world_strings(struct world *mzx_world,
 struct zip_archive *zp)
{
  struct string *src_string;
  char name_buffer[ROBOT_MAX_TR];
  size_t name_length;
  size_t str_length;

  int num_prev_allocated;
  int num_strings;
  int i;

  enum zip_error result;

  if(ZIP_SUCCESS !=
   zip_read_open_file_stream(zp, NULL, 0, NULL))
    return;

  num_strings = zgetd(zp, &result);
  num_prev_allocated = mzx_world->num_counters_allocated;

  // If there aren't already any strings, allocate manually.
  if(!num_prev_allocated)
  {
    mzx_world->num_strings = num_strings;
    mzx_world->num_strings_allocated = num_strings;
    mzx_world->string_list = ccalloc(num_strings, sizeof(struct string *));
  }

  for(i = 0; i < num_strings; i++)
  {
    name_length = zgetd(zp, &result);
    str_length = zgetd(zp, &result);

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
      src_string = load_new_string(name_buffer, name_length, str_length);
      mzx_world->string_list[i] = src_string;
    }

    zread(src_string->value, str_length, zp);
    src_string->length = str_length;
  }

  // If there weren't any previously allocated, the number successfully read is
  // the new number of strings.
  if(!num_prev_allocated)
    mzx_world->num_strings = i;

  zip_read_close_stream(zp);
}


static void save_world_zip(struct world *mzx_world, const char *file,
 int savegame, int file_version)
{
  struct zip_archive *zp = zip_open_file_write(file);

  if(!zp)
    return;

  // Header
  if(!savegame)
  {
    // World name
    zwrite(mzx_world->name, BOARD_NAME_SIZE, zp);

    // Protection method -- always zero
    zputc(0, zp);

    // Version string
    zputc('M', zp);
    zputc((file_version >> 8) & 0xFF, zp);
    zputc(file_version & 0xFF, zp);
  }
  else
  {
    // FIXME fail if mzx_world->version > file_version

    // Version string
    zwrite("MZS", 3, zp);
    zputc((file_version >> 8) & 0xFF, zp);
    zputc(file_version & 0xFF, zp);

    // MZX world version
    zputw(mzx_world->version, zp);

    // Current board ID
    zputc(mzx_world->current_board_id, zp);
  }

  save_world_info(mzx_world, zp, savegame, "_info", FPROP_WORLD_INFO);

  save_world_global_robot(mzx_world, zp,  "gr",     FPROP_WORLD_GLOBAL_ROBOT);
  save_world_stat_counters(mzx_world, zp, "status", FPROP_WORLD_STAT_COUNTERS);
  save_world_sfx(mzx_world, zp,           "sfx",    FPROP_WORLD_SFX);
  save_world_board_names(mzx_world, zp,   "boards", FPROP_WORLD_BOARD_NAMES);
  save_world_chars(mzx_world, zp,         "chars",  FPROP_WORLD_CHARS);
  save_world_pal(mzx_world, zp,           "pal",    FPROP_WORLD_PAL);

  if(savegame)
  {
    save_world_pal_index(mzx_world, zp,   "palidx", FPROP_WORLD_PAL_INDEX);
    save_world_pal_inten(mzx_world, zp,   "palint", FPROP_WORLD_PAL_INTENSITY);
    save_world_vco(mzx_world, zp,         "vco",    FPROP_WORLD_VCO);
    save_world_vch(mzx_world, zp,         "vch",    FPROP_WORLD_VCH);
    save_world_sprites(mzx_world, zp,     "spr",    FPROP_WORLD_SPRITES);
    save_world_counters(mzx_world, zp,    "counter",FPROP_WORLD_COUNTERS);
    save_world_strings(mzx_world, zp,     "string", FPROP_WORLD_STRINGS);
  }

  // FIXME boards

  zip_close(zp, NULL);
}


void save_counters_file(struct world *mzx_world, const char *file)
{
  struct zip_archive *zp = zip_open_file_write(file);

  if(!zp)
    return;

  zwrite("COUNTERS", 8, zp);

  save_world_counters(mzx_world, zp,      "counter",FPROP_WORLD_COUNTERS);
  save_world_strings(mzx_world, zp,       "string", FPROP_WORLD_STRINGS);

  zip_close(zp, NULL);
}


static void load_world_zip(struct world *mzx_world, FILE *fp,
 int savegame, int file_version, int *faded)
{
}


int load_counters_file(struct world *mzx_world, const char *file)
{
  struct zip_archive *zp = zip_open_file_read(file);
  char magic[9];

  unsigned int prop_id;

  if(!zp)
  {
    error_message(E_FILE_DOES_NOT_EXIST, 0, file);
    return 0;
  }

  magic[8] = 0;
  if(ZIP_SUCCESS != zread(magic, 8, zp))
    goto err;

  if(strcmp(magic, "COUNTERS"))
    goto err;

  if(ZIP_SUCCESS != zip_read_directory(zp))
    goto err;

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
  return 1;

err:
  error_message(E_SAVE_FILE_INVALID, 0, file);
  zip_close(zp, NULL);
  return 0;
}


int world_magic(const char magic_string[3])
{
  if(magic_string[0] == 'M')
  {
    if(magic_string[1] == 'Z')
    {
      switch(magic_string[2])
      {
        case 'X':
          return 0x0100;
        case '2':
          return 0x0205;
        case 'A':
          return 0x0208;
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
          return 0x0205;
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
          return 0x0208;
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

int save_world(struct world *mzx_world, const char *file, int savegame)
{
#ifdef CONFIG_DEBYTECODE
  FILE *fp;

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
          if(old_version < VERSION_PROGRAM_SOURCE)
          {
            error_message(E_DBC_WORLD_OVERWRITE_OLD, old_version, file);
            fclose(fp);
            return -1;
          }
        }
      }
      fclose(fp);
    }
  }
#endif

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

  // FIXME save_world_zip
  return legacy_save_world(mzx_world, file, savegame);
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
  int relocate_current = 1;

  int num_boards = mzx_world->num_boards;
  struct board **board_list = mzx_world->board_list;
  struct board *cur_board;

/* SAVED POSITIONS
#ifdef CONFIG_EDITOR
  refactor_saved_positions(mzx_world, board_id_translation_list);
#endif
*/

  if(board_list[mzx_world->current_board_id] == NULL)
    relocate_current = 0;

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

      if(d_param < new_list_size)
        cur_board->board_dir[i2] = board_id_translation_list[d_param];
      else
        cur_board->board_dir[i2] = NO_BOARD;
    }
  }

  // Fix current board
  if(relocate_current)
  {
    d_param = mzx_world->current_board_id;
    d_param = board_id_translation_list[d_param];
    mzx_world->current_board_id = d_param;
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


static void load_world(struct world *mzx_world, FILE *fp, const char *file,
 bool savegame, int version, char *name, int *faded)
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
  {
    set_config_from_file(&(mzx_world->conf), config_file_name);
  }

  // FIXME load_world_zip
  legacy_load_world(mzx_world, fp, file, savegame, version, name, faded);

  // Update the palette  
  update_palette();

  // Setup gateway functions
  initialize_gateway_functions(mzx_world);

#ifdef CONFIG_DEBYTECODE
  // Convert SFX strings if needed
  if(version < VERSION_PROGRAM_SOURCE)
  {
    char *sfx_offset = mzx_world->custom_sfx;
    int i;

    for(i = 0; i < NUM_SFX; i++, sfx_offset += 69)
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
  mzx_world->current_board = NULL;
  set_current_board_ext(mzx_world,
   mzx_world->board_list[mzx_world->current_board_id]);

  mzx_world->active = 1;

  // Remove any null boards
  optimize_null_boards(mzx_world);

  // Resize this array if necessary
  set_update_done(mzx_world);

  // Find the player
  find_player(mzx_world);
}

static FILE *try_open_world(const char *file)
{
  struct stat stat_result;
  int stat_op_result;
  FILE *fp;

  stat_op_result = stat(file, &stat_result);

  if(stat_op_result ||
   !S_ISREG(stat_result.st_mode) ||
   !(fp = fopen_unsafe(file, "rb")))
  {
    error_message(E_FILE_DOES_NOT_EXIST, 0, file);
    return NULL;
  }

  return fp;
}

__editor_maybe_static FILE *try_load_world(const char *file,
 bool savegame, int *version, char *name)
{
  FILE *fp = try_open_world(file);
  char magic[5];
  int v;

  // If we couldn't open the world, fail.
  if(!fp)
  {
    goto err_out;
  }

  // For now, a magic failure here means we can't load the world.
  // FIXME eventually we can try to get the world name and magic out of the
  // zip world metadata.

  // Try to get the savegame magic
  if(savegame)
  {
    if(!fread(magic, 5, 1, fp))
      goto err_out;

    v = save_magic(magic);

    if(v == 0)
    {
      error_message(E_SAVE_FILE_INVALID, 0, file);
      goto err_close;
    }
    else

    if(v > WORLD_VERSION)
    {
      error_message(E_SAVE_VERSION_TOO_RECENT, v, file);
      goto err_close;
    }
    else

    if (v < WORLD_LEGACY_FORMAT_VERSION)
    {
      error_message(E_SAVE_VERSION_OLD, v, file);
      goto err_close;
    }
  }

  // Try to get the world name and world magic
  else
  {
    if(name)
    {
      if(!fread(name, BOARD_NAME_SIZE, 1, fp))
        goto err_out;
    }

    else
    {
      if(fseek(fp, BOARD_NAME_SIZE, SEEK_CUR))
        goto err_out;
    }

    // Skip protection byte for now.
    fseek(fp, 1, SEEK_CUR); 

    fread(magic, 1, 3, fp);

    v = world_magic(magic);

    if(v == 0)
    {
      error_message(E_WORLD_FILE_INVALID, 0, file);
      goto err_close;
    }
    else

    if (v < 0x0205)
    {
      error_message(E_WORLD_FILE_VERSION_OLD, v, file);
      goto err_close;
    }
    else

    if (v > WORLD_VERSION)
    {
      error_message(E_WORLD_FILE_VERSION_TOO_RECENT, v, file);
      goto err_close;
    }
  }

  // FIXME
  //if(v == WORLD_LEGACY_FORMAT_VERSION)
  {
    enum val_result status;

    // We need to close the file so we can run the validator
    fclose(fp);
    fp = NULL;

    status = validate_legacy_world_file(file, savegame, NULL, 0);

    if(status != VAL_SUCCESS)
      goto err_out;

    // Reopen the file
    fp = try_open_world(file);
  }

  // FIXME verify newer worlds are valid zips

  if(version)
    *version = v;

  // Success: reset suppression for the world
  reset_error_suppression();

  return fp;

err_close:
  fclose(fp);
err_out:
  return NULL;
}

// After clearing the above, use this to get default values. Use
// for loading of worlds (as opposed to save games).

__editor_maybe_static void default_global_data(struct world *mzx_world)
{
  int i;

  // Allocate space for sprites and give them default values (all 0's)
  mzx_world->num_sprites = MAX_SPRITES;
  mzx_world->sprite_list = ccalloc(MAX_SPRITES, sizeof(struct sprite *));

  for(i = 0; i < MAX_SPRITES; i++)
  {
    mzx_world->sprite_list[i] = ccalloc(1, sizeof(struct sprite));
  }

  mzx_world->collision_list = ccalloc(MAX_SPRITES, sizeof(int));
  mzx_world->sprite_num = 0;

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

  // And vlayer
  // Allocate space for vlayer.
  mzx_world->vlayer_size = 0x8000;
  mzx_world->vlayer_width = 256;
  mzx_world->vlayer_height = 128;
  mzx_world->vlayer_chars = cmalloc(0x8000);
  mzx_world->vlayer_colors = cmalloc(0x8000);
  memset(mzx_world->vlayer_chars, 32, 0x8000);
  memset(mzx_world->vlayer_colors, 7, 0x8000);

  // Default values for global params
  memset(mzx_world->keys, NO_KEY, NUM_KEYS);
  mzx_world->mesg_edges = 1;
  mzx_world->real_mod_playing[0] = 0;

  mzx_world->blind_dur = 0;
  mzx_world->firewalker_dur = 0;
  mzx_world->freeze_time_dur = 0;
  mzx_world->slow_time_dur = 0;
  mzx_world->wind_dur = 0;

  for(i = 0; i < 8; i++)
  {
    mzx_world->pl_saved_x[i] = 0;
  }

  for(i = 0; i < 8; i++)
  {
    mzx_world->pl_saved_y[i] = 0;
  }
  memset(mzx_world->pl_saved_board, 0, 8);
  mzx_world->saved_pl_color = 27;
  mzx_world->player_restart_x = 0;
  mzx_world->player_restart_y = 0;
  mzx_world->under_player_id = 0;
  mzx_world->under_player_color = 7;
  mzx_world->under_player_param = 0;

  mzx_world->commands = 40;

  default_scroll_values(mzx_world);

  scroll_color = 15;

  mzx_world->lock_speed = 0;
  mzx_world->mzx_speed = mzx_world->default_speed;

  assert(mzx_world->input_file == NULL);
  assert(mzx_world->output_file == NULL);
  assert(mzx_world->input_is_dir == false);

  mzx_world->target_where = TARGET_NONE;
}

bool reload_world(struct world *mzx_world, const char *file, int *faded)
{
  char name[BOARD_NAME_SIZE];
  int version;
  FILE *fp;

  fp = try_load_world(file, false, &version, name);
  if(!fp)
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

  load_world(mzx_world, fp, file, false, version, name, faded);
  default_global_data(mzx_world);
  *faded = 0;

  // Now that the world's loaded, fix the save path.
  {
    char save_name[MAX_PATH];
    char current_dir[MAX_PATH];
    getcwd(current_dir, MAX_PATH);

    split_path_filename(curr_sav, NULL, 0, save_name, MAX_PATH);
    snprintf(curr_sav, MAX_PATH, "%s%s%s",
     current_dir, DIR_SEPARATOR, save_name);
  }

  return true;
}

bool reload_savegame(struct world *mzx_world, const char *file, int *faded)
{
  int version;
  FILE *fp;

  // Check this SAV is actually loadable
  fp = try_load_world(file, true, &version, NULL);
  if(!fp)
    return false;

  // It is, so wipe the old world
  if(mzx_world->active)
  {
    clear_world(mzx_world);
    clear_global_data(mzx_world);
  }

  // And load the new one
  load_world(mzx_world, fp, file, true, version, NULL, faded);
  return true;
}

bool reload_swap(struct world *mzx_world, const char *file, int *faded)
{
  char name[BOARD_NAME_SIZE];
  char full_path[MAX_PATH];
  char file_name[MAX_PATH];
  int version;
  FILE *fp;

  fp = try_load_world(file, false, &version, name);
  if(!fp)
    return false;

  if(mzx_world->active)
    clear_world(mzx_world);

  load_world(mzx_world, fp, file, false, version, name, faded);

  mzx_world->current_board_id = mzx_world->first_board;
  set_current_board_ext(mzx_world,
   mzx_world->board_list[mzx_world->current_board_id]);

  // Give curr_file a full path
  getcwd(full_path, MAX_PATH);
  split_path_filename(file, NULL, 0, file_name, MAX_PATH);
  snprintf(curr_file, MAX_PATH, "%s%s%s",
   full_path, DIR_SEPARATOR, file_name);

  return true;
}

// This only clears boards, no global data. Useful for swap world,
// when you want to maintain counters and sprites and all that.

void clear_world(struct world *mzx_world)
{
  // Do this before loading, when there's a world

  int i;
  int num_boards = mzx_world->num_boards;
  struct board **board_list = mzx_world->board_list;

  for(i = 0; i < num_boards; i++)
  {
    if(mzx_world->current_board_id != i)
      retrieve_board_from_extram(board_list[i]);
    clear_board(board_list[i]);
  }
  free(board_list);
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

  mzx_world->active = 0;

  end_sample();
}

// This clears the rest of the stuff.

void clear_global_data(struct world *mzx_world)
{
  int i;
  int num_counters = mzx_world->num_counters;
  int num_strings = mzx_world->num_strings;
  struct counter **counter_list = mzx_world->counter_list;
  struct string **string_list = mzx_world->string_list;
  struct sprite **sprite_list = mzx_world->sprite_list;

  free(mzx_world->vlayer_chars);
  free(mzx_world->vlayer_colors);
  mzx_world->vlayer_chars = NULL;
  mzx_world->vlayer_colors = NULL;

  // Let counter.c handle this
  free_counter_list(counter_list, num_counters);
  mzx_world->counter_list = NULL;

  // Let counter.c handle this
  free_string_list(string_list, num_strings);
  mzx_world->string_list = NULL;

  mzx_world->num_counters = 0;
  mzx_world->num_counters_allocated = 0;
  mzx_world->num_strings = 0;
  mzx_world->num_strings_allocated = 0;

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

  memset(mzx_world->custom_sfx, 0, NUM_SFX * 69);

  mzx_world->bomb_type = 1;
  mzx_world->dead = 0;
}

void default_scroll_values(struct world *mzx_world)
{
  mzx_world->scroll_base_color = 143;
  mzx_world->scroll_corner_color = 135;
  mzx_world->scroll_pointer_color = 128;
  mzx_world->scroll_title_color = 143;
  mzx_world->scroll_arrow_color = 142;
}
