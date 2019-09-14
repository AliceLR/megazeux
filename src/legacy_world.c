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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "legacy_world.h"
#include "legacy_board.h"
#include "legacy_robot.h"

#include "const.h"
#include "counter.h"
#include "error.h"
#include "extmem.h"
#include "fsafeopen.h"
#include "graphics.h"
#include "idput.h"
#include "robot.h"
#include "sprite.h"
#include "str.h"
#include "window.h"
#include "world.h"
#include "util.h"

#include "audio/sfx.h"

#ifndef CONFIG_LOADSAVE_METER

static inline void meter_update_screen(int *curr, int target) {}
static inline void meter_restore_screen(void) {}
static inline void meter_initial_draw(int curr, int target, const char *title) {}

#endif //!CONFIG_LOADSAVE_METER


static char name_buffer[ROBOT_MAX_TR];

static inline boolean legacy_load_counter(struct world *mzx_world,
 FILE *fp, struct counter_list *counter_list, int index)
{
  int value = fgetd(fp);
  int name_length = fgetd(fp);

  name_buffer[0] = 0;
  if(name_length && !fread(name_buffer, name_length, 1, fp))
    return false;

  // Stupid legacy hacks
  if(!strncasecmp(name_buffer, "mzx_speed", name_length))
  {
    mzx_world->mzx_speed = value;
    return false;
  }

  if(!strncasecmp(name_buffer, "_____lock_speed", name_length))
  {
    mzx_world->lock_speed = value;
    return false;
  }

  load_new_counter(counter_list, index, name_buffer, name_length, value);
  return true;
}

static inline void legacy_load_string(FILE *fp,
 struct string_list *string_list, int index)
{
  int name_length = fgetd(fp);
  int str_length = fgetd(fp);

  struct string *src_string;

  name_buffer[0] = 0;
  if(name_length && !fread(name_buffer, name_length, 1, fp))
    return;

  src_string = load_new_string(string_list, index,
   name_buffer, name_length, str_length);

  if(str_length && !fread(src_string->value, str_length, 1, fp))
    return;
}

static const char magic_code[16] =
 "\xE6\x52\xEB\xF2\x6D\x4D\x4A\xB7\x87\xB2\x92\x88\xDE\x91\x24";

#define MAX_PASSWORD_LENGTH 15

static int get_pw_xor_code(char *password, int pro_method)
{
  int work = 85; // Start with 85... (01010101)
  size_t i;
  // Clear pw after first null

  for(i = strlen(password); i < MAX_PASSWORD_LENGTH; i++)
  {
    password[i] = 0;
  }

  for(i = 0; i < MAX_PASSWORD_LENGTH; i++)
  {
    //For each byte, roll once to the left and xor in pw byte if it
    //is an odd character, or add in pw byte if it is an even character.
    work <<= 1;
    if(work > 255)
      work ^= 257; // Wraparound from roll

    if(i & 1)
    {
      work += (signed char)password[i]; // Add (even byte)
      if(work > 255)
        work ^= 257; // Wraparound from add
    }
    else
    {
      work ^= (signed char)password[i]; // XOR (odd byte);
    }
  }
  // To factor in protection method, add it in and roll one last time
  work += pro_method;
  if(work > 255)
    work ^= 257;

  work <<= 1;
  if(work > 255)
    work ^= 257;

  // Can't be 0-
  if(work == 0)
    work = 86; // (01010110)
  // Done!
  return work;
}

static void decrypt(const char *file_name)
{
  FILE *source;
  FILE *backup;
  FILE *dest;
  int file_length;
  int pro_method;
  int i;
  int len;
  char num_boards;
  char offset_low_byte;
  char xor_val;
  char password[MAX_PASSWORD_LENGTH + 1];
  char *file_buffer;
  char *src_ptr;
  char backup_name[MAX_PATH];
  int count;

  int meter_target, meter_curr = 0;

  source = fopen_unsafe(file_name, "rb");
  file_length = ftell_and_rewind(source);

  meter_target = file_length + (file_length - 15) + 4;

  meter_initial_draw(meter_curr, meter_target, "Decrypting...");

  file_buffer = cmalloc(file_length);
  src_ptr = file_buffer;
  count = fread(file_buffer, file_length, 1, source);
  fclose(source);
  if(!count)
    goto err_free;

  meter_curr = file_length - 1;
  meter_update_screen(&meter_curr, meter_target);

  src_ptr += 25;

  snprintf(backup_name, MAX_PATH, "%.*s.locked", MAX_PATH - 8, file_name);

  backup = fopen_unsafe(backup_name, "wb");
  count = fwrite(file_buffer, file_length, 1, backup);
  fclose(backup);
  if(!count)
    goto err_free;

  dest = fopen_unsafe(file_name, "wb");
  if(!dest)
  {
    error_message(E_WORLD_DECRYPT_WRITE_PROTECTED, 0, NULL);
    return;
  }
  pro_method = *src_ptr;
  src_ptr++;

  // Get password
  memcpy(password, src_ptr, MAX_PASSWORD_LENGTH);
  password[MAX_PASSWORD_LENGTH] = '\0';
  src_ptr += 18;
  // First, normalize password...
  for(i = 0; i < MAX_PASSWORD_LENGTH; i++)
  {
    password[i] ^= magic_code[i];
    password[i] -= 0x12 + pro_method;
    password[i] ^= 0x8D;
  }

  // Xor code
  xor_val = get_pw_xor_code(password, pro_method);

  // Copy title
  if(!fwrite(file_buffer, 25, 1, dest))
    goto err_close;
  fputc(0, dest);
  fputs("M\x02\x11", dest);

  meter_curr += 25 + 1 + 3 - 1;
  meter_update_screen(&meter_curr, meter_target);

  len = file_length - 44;
  for(; len > 0; len--)
  {
    fputc((*src_ptr) ^ xor_val, dest);
    src_ptr++;

    if((len % 1000) == 0)
    {
      meter_curr += 999;
      meter_update_screen(&meter_curr, meter_target);
    }
  }

  meter_curr = file_length + (file_length - MAX_PASSWORD_LENGTH) - 1;
  meter_update_screen(&meter_curr, meter_target);

  // Must fix all the absolute file positions so that they're MAX_PASSWORD_LENGTH
  // less now
  src_ptr = file_buffer + 4245;
  fseek(dest, 4230, SEEK_SET);
  offset_low_byte = src_ptr[0] ^ xor_val;
  fputc(offset_low_byte - MAX_PASSWORD_LENGTH, dest);
  if(offset_low_byte < MAX_PASSWORD_LENGTH)
  {
    fputc((src_ptr[1] ^ xor_val) - 1, dest);
  }
  else
  {
    fputc(src_ptr[1] ^ xor_val, dest);
  }

  fputc(src_ptr[2] ^ xor_val, dest);
  fputc(src_ptr[3] ^ xor_val, dest);

  meter_curr += 4 - 1;
  meter_update_screen(&meter_curr, meter_target);

  src_ptr += 4;

  num_boards = ((*src_ptr) ^ xor_val);
  src_ptr++;

  // If custom SFX is there, run through and skip it
  if(!num_boards)
  {
    int sfx_length = (char)(src_ptr[0] ^ xor_val);
    sfx_length |= ((char)(src_ptr[1] ^ xor_val)) << 8;
    src_ptr += sfx_length + 2;
    num_boards = (*src_ptr) ^ xor_val;
    src_ptr++;
  }

  meter_target += num_boards * 4;
  meter_curr--;
  meter_update_screen(&meter_curr, meter_target);

  // Skip titles
  src_ptr += (25 * num_boards);
  // Synchronize source and dest positions
  fseek(dest, (long)(src_ptr - file_buffer - MAX_PASSWORD_LENGTH), SEEK_SET);

  // Offset boards
  for(i = 0; i < num_boards; i++)
  {
    // Skip length
    src_ptr += 4;
    fseek(dest, 4, SEEK_CUR);

    // Get offset
    offset_low_byte = src_ptr[0] ^ xor_val;
    fputc(offset_low_byte - MAX_PASSWORD_LENGTH, dest);
    if(offset_low_byte < MAX_PASSWORD_LENGTH)
    {
      fputc((src_ptr[1] ^ xor_val) - 1, dest);
    }
    else
    {
      fputc(src_ptr[1] ^ xor_val, dest);
    }
    fputc(src_ptr[2] ^ xor_val, dest);
    fputc(src_ptr[3] ^ xor_val, dest);
    src_ptr += 4;

    meter_target += 4 - 1;
    meter_update_screen(&meter_curr, meter_target);
  }

err_close:
  fclose(dest);

err_free:
  free(file_buffer);

  meter_restore_screen();
}


#define WORLD_GLOBAL_OFFSET_OFFSET 4230
#define WORLD_BLOCK_1_SIZE 4129
#define WORLD_BLOCK_2_SIZE 72

/* This is a lot like try_load_world but much more thorough, and doesn't
 * pass data through or leave a file open.  This needs to be done before
 * any data is ever loaded, so that Megazeux can cleanly abort if there
 * is an issue.
 *
 * There are a few redundant checks here with try_load_world, but that's ok.
 */

static enum val_result __validate_legacy_world_file(const char *file,
 boolean savegame)
{
  enum val_result result = VAL_SUCCESS;

  struct stat stat_result;
  int stat_op_result;
  FILE *f;
  char magic[15];
  int num_boards;
  int board_name_offset;
  int v, i;

  /* TEST 1: make sure it's even a file */
  stat_op_result = stat(file, &stat_result);

  if(stat_op_result ||
   !S_ISREG(stat_result.st_mode) ||
   !(f = fopen_unsafe(file, "rb")))
  {
    error_message(E_FILE_DOES_NOT_EXIST, 0, NULL);
    result = VAL_MISSING;
    goto err_out;
  }

  /* TEST 2:  Is it a save file? */
  if(savegame)
  {
    int screen_mode, num_counters, num_strings, len;

    if(fread(magic, 5, 1, f) != 1)
      goto err_invalid;

    v = save_magic(magic);
    if(!v)
      goto err_invalid;

    else if(v > MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_SAVE_VERSION_TOO_RECENT, v, NULL);
      result = VAL_VERSION;
      goto err_close;
    }

    // This enables 2.84 save loading.
    // If we ever want to remove this, change this check.
    //else if (v < MZX_VERSION)
    else if(v < MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_SAVE_VERSION_OLD, v, NULL);
      result = VAL_VERSION;
      goto err_close;
    }

    /* TEST 3:  Check for truncation, savegame style, hope this
     * doesn't explode :erm:
     */
    if(
     fseek(f, 8, SEEK_SET) ||
     fseek(f, WORLD_BLOCK_1_SIZE, SEEK_CUR) ||
     fseek(f, 71, SEEK_CUR) ||
     fseek(f, (len = fgetw(f)), SEEK_CUR) ||
     (len < 0) ||
     fseek(f, WORLD_BLOCK_2_SIZE, SEEK_CUR) ||
     fseek(f, 24, SEEK_CUR))
    {
      debug("pre-counters\n");
      goto err_invalid;
    }

    //do counters - vvvvnnnn(name)
    num_counters = fgetd(f);
    if(num_counters < 0)
    {
      debug("counter num\n");
      goto err_invalid;
    }

    for(i = 0; i < num_counters; i++)
    {
      if(
       fseek(f, 4, SEEK_CUR) || //value
       fseek(f, (len = fgetd(f)), SEEK_CUR) ||
       (len < 0))
      {
        debug("counters\n");
        goto err_invalid;
      }
    }

    //do strings-   nnnnllll(name)(value)
    num_strings = fgetd(f);
    if(num_strings < 0)
    {
      debug("string num\n");
      goto err_invalid;
    }

    for(i = 0; i < num_strings; i++)
    {
      int name_length = fgetd(f);
      int value_length = fgetd(f);
      if(
       (name_length < 0) ||
       (value_length < 0) ||
       fseek(f, name_length, SEEK_CUR) ||
       fseek(f, value_length, SEEK_CUR))
      {
        debug("strings\n");
        goto err_invalid;
      }
    }

    if(
     fseek(f, 4612, SEEK_CUR) || //sprites
     fseek(f, 12, SEEK_CUR) || //misc
     fseek(f, fgetw(f), SEEK_CUR) || //fread_open
     fseek(f, 4, SEEK_CUR) || //fread_pos
     fseek(f, fgetw(f), SEEK_CUR) || //fwrite_open
     fseek(f, 4, SEEK_CUR)) //fwrite_pos
    {
      debug("post strings\n");
      goto err_invalid;
    }

    screen_mode = fgetw(f);
    if((screen_mode > 3) || (screen_mode > 1 &&
     fseek(f, 768, SEEK_CUR))) //smzx palette
    {
      debug("smzx palette\n");
      goto err_invalid;
    }

    if(
     fseek(f, 4, SEEK_CUR) || //commands
     ((len = fgetd(f)) < 0) || //vlayer size
     fseek(f, 4, SEEK_CUR) || // width & height
     fseek(f, len, SEEK_CUR) || //chars
     fseek(f, len, SEEK_CUR)) //colors
    {
      debug("vlayer\n");
      goto err_invalid;
    }

    /* now we should be at the global robot pointer! */
  }

  else /* !savegame */
  {
    int protection_method;

    /* TEST 3:  Check for truncation */
    if(fseek(f, WORLD_GLOBAL_OFFSET_OFFSET, SEEK_SET))
      goto err_invalid;

    fseek(f, BOARD_NAME_SIZE, SEEK_SET);

    /* TEST 4:  If we think it's locked, try to decrypt it. */
    protection_method = fgetc(f);
    if(protection_method > 0)
    {
      if(protection_method > 3)
        goto err_invalid;

      result = VAL_PROTECTED;
      goto err_close;
    }

    /* TEST 5:  Test the magic */
    if(!fread(magic, 3, 1, f))
      goto err_invalid;

    v = world_magic(magic);
    if(v == 0)
      goto err_invalid;

    else if(v < V251)
    {
      error_message(E_WORLD_FILE_VERSION_OLD, v, NULL);
      result = VAL_VERSION;
      goto err_close;
    }

    else if(v > MZX_LEGACY_FORMAT_VERSION)
    {
      error_message(E_WORLD_FILE_VERSION_TOO_RECENT, v, NULL);
      result = VAL_VERSION;
      goto err_close;
    }

    /* TEST 6:  Attempt to eliminate invalid files by
     * testing the palette for impossible values.
     */
    fseek(f, WORLD_GLOBAL_OFFSET_OFFSET - 48, SEEK_SET);
    for(i = 0; i<48; i++)
    {
      int val = fgetc(f);
      if((val < 0) || (val > 63))
        goto err_invalid;
    }

    /* now we should be at the global robot pointer! */
  }

  /* TEST 7:  Either branch should be at the global robot pointer now.
   * Test for valid SFX structure, if applicable, and board information.
   */
  fseek(f, 4, SEEK_CUR);

  // Do the sfx
  num_boards = fgetc(f);
  if(num_boards == 0)
  {
    int sfx_size = fgetw(f);
    int sfx_off = ftell(f);

    for(i = 0; i < NUM_SFX; i++)
    {
      if(fseek(f, fgetc(f), SEEK_CUR))
        break;
    }

    if((i != NUM_SFX) || ((ftell(f) - sfx_off) != sfx_size))
      goto err_invalid;

    num_boards = fgetc(f);
  }
  if(num_boards == 0)
    goto err_invalid;

  board_name_offset = ftell(f);

  //Make sure board name and pointer data exists
  if(
   fseek(f, num_boards * BOARD_NAME_SIZE, SEEK_CUR) ||
   fseek(f, num_boards * 8, SEEK_CUR) ||
   ((ftell(f) - board_name_offset) != num_boards * (BOARD_NAME_SIZE + 8)))
    goto err_invalid;

  //todo: maybe have a complete fail when N number of pointers fail?

  goto err_close;

err_invalid:
  result = VAL_INVALID;
  if(savegame)
    error_message(E_SAVE_FILE_INVALID, 0, NULL);
  else
    error_message(E_WORLD_FILE_INVALID, 0, NULL);
err_close:
  fclose(f);
err_out:
  return result;
}

enum val_result validate_legacy_world_file(struct world *mzx_world,
 const char *file, boolean savegame)
{
  enum val_result res = __validate_legacy_world_file(file, savegame);

  if(res == VAL_PROTECTED)
  {
    if(!has_video_initialized() ||
     !confirm(mzx_world, "This world may be password protected. Decrypt it?"))
    {
      // Decrypt and try again
      decrypt(file);
      res = __validate_legacy_world_file(file, savegame);

      // If the world is still protected, abort.
      if(res != VAL_PROTECTED)
        return res;
    }

    error_message(E_WORLD_LOCKED, 0, NULL);
    return VAL_ABORTED;
  }

  return res;
}

void legacy_load_world(struct world *mzx_world, FILE *fp, const char *file,
 boolean savegame, int file_version, char *name, boolean *faded)
{
  int i;
  int num_boards;
  int global_robot_pos, board_names_pos;
  unsigned char *charset_mem;
  unsigned char r, g, b;
  struct counter_list *counter_list;
  struct string_list *string_list;
  struct board *cur_board;
  unsigned int *board_offsets;
  unsigned int *board_sizes;

  int meter_target = 2, meter_curr = 0;

  if(savegame)
  {
    fseek(fp, 5, SEEK_SET);
    mzx_world->version = fgetw(fp);
    mzx_world->current_board_id = fgetc(fp);
  }
  else
  {
    fseek(fp, 29, SEEK_SET);
    strcpy(mzx_world->name, name);
    mzx_world->version = file_version;
    mzx_world->current_board_id = 0;
  }

  meter_initial_draw(meter_curr, meter_target, "Loading...");

  charset_mem = cmalloc(CHAR_SIZE * CHARSET_SIZE);
  if(!fread(charset_mem, CHAR_SIZE * CHARSET_SIZE, 1, fp))
    goto err_close;
  ec_clear_set();
  ec_mem_load_set(charset_mem, CHAR_SIZE * CHARSET_SIZE);
  free(charset_mem);

  // Idchars array...
  memset(id_chars, 0, ID_CHARS_SIZE);
  memset(id_dmg, 0, ID_DMG_SIZE);
  memset(bullet_color, 0, ID_BULLET_COLOR_SIZE);

  if(!fread(id_chars, LEGACY_ID_CHARS_SIZE, 1, fp))
    goto err_close;
  missile_color = fgetc(fp);
  if(!fread(bullet_color, LEGACY_ID_BULLET_COLOR_SIZE, 1, fp))
    goto err_close;
  if(!fread(id_dmg, LEGACY_ID_DMG_SIZE, 1, fp))
    goto err_close;

  // Status counters...
  if(fread((char *)mzx_world->status_counters_shown, COUNTER_NAME_SIZE,
   NUM_STATUS_COUNTERS, fp) != NUM_STATUS_COUNTERS)
    goto err_close;

  if(savegame)
  {
    if(!fread(mzx_world->keys, NUM_KEYS, 1, fp))
      goto err_close;
    mzx_world->blind_dur = fgetc(fp);
    mzx_world->firewalker_dur = fgetc(fp);
    mzx_world->freeze_time_dur = fgetc(fp);
    mzx_world->slow_time_dur = fgetc(fp);
    mzx_world->wind_dur = fgetc(fp);

    for(i = 0; i < 8; i++)
    {
      mzx_world->pl_saved_x[i] = fgetw(fp);
    }

    for(i = 0; i < 8; i++)
    {
      mzx_world->pl_saved_y[i] = fgetw(fp);
    }

    if(!fread(mzx_world->pl_saved_board, 8, 1, fp))
      goto err_close;
    mzx_world->saved_pl_color = fgetc(fp);
    mzx_world->under_player_id = fgetc(fp);
    mzx_world->under_player_color = fgetc(fp);
    mzx_world->under_player_param = fgetc(fp);
    mzx_world->mesg_edges = fgetc(fp);
    mzx_world->scroll_base_color = fgetc(fp);
    mzx_world->scroll_corner_color = fgetc(fp);
    mzx_world->scroll_pointer_color = fgetc(fp);
    mzx_world->scroll_title_color = fgetc(fp);
    mzx_world->scroll_arrow_color = fgetc(fp);

    {
      size_t len = fgetw(fp);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      if(len && !fread(mzx_world->real_mod_playing, len, 1, fp))
        goto err_close;
      mzx_world->real_mod_playing[len] = 0;
    }
  }

  mzx_world->edge_color = fgetc(fp);
  mzx_world->first_board = fgetc(fp);
  mzx_world->endgame_board = fgetc(fp);
  mzx_world->death_board = fgetc(fp);
  mzx_world->endgame_x = fgetw(fp);
  mzx_world->endgame_y = fgetw(fp);
  mzx_world->game_over_sfx = fgetc(fp);
  mzx_world->death_x = fgetw(fp);
  mzx_world->death_y = fgetw(fp);
  mzx_world->starting_lives = fgetw(fp);
  mzx_world->lives_limit = fgetw(fp);
  mzx_world->starting_health = fgetw(fp);
  mzx_world->health_limit = fgetw(fp);
  mzx_world->enemy_hurt_enemy = fgetc(fp);
  mzx_world->clear_on_exit = fgetc(fp);
  mzx_world->only_from_swap = fgetc(fp);

  // Palette...
  for(i = 0; i < 16; i++)
  {
    r = fgetc(fp);
    g = fgetc(fp);
    b = fgetc(fp);

    set_rgb(i, r, g, b);
  }

  if(savegame)
  {
    int vlayer_size;
    int num_counters, num_strings;
    int screen_mode;
    int j;

    for(i = 0; i < 16; i++)
    {
      set_color_intensity(i, fgetc(fp));
    }

    *faded = fgetc(fp);

    mzx_world->player_restart_x = fgetw(fp);
    mzx_world->player_restart_y = fgetw(fp);
    mzx_world->under_player_id = fgetc(fp);
    mzx_world->under_player_color = fgetc(fp);
    mzx_world->under_player_param = fgetc(fp);

    // Read counters
    num_counters = fgetd(fp);
    counter_list = &(mzx_world->counter_list);
    counter_list->num_counters = num_counters;
    counter_list->num_counters_allocated = num_counters;
    counter_list->counters = ccalloc(num_counters, sizeof(struct counter *));

    for(i = 0, j = 0; i < num_counters; i++)
    {
      boolean counter = legacy_load_counter(mzx_world, fp, counter_list, j);

      /* We loaded a special counter, this doesn't need to be
       * loaded into the regular list.
       */
      if(!counter)
      {
        counter_list->num_counters--;
        continue;
      }

      j++;
    }

    // Read strings
    num_strings = fgetd(fp);
    string_list = &(mzx_world->string_list);
    string_list->num_strings = num_strings;
    string_list->num_strings_allocated = num_strings;
    string_list->strings = ccalloc(num_strings, sizeof(struct string *));

    for(i = 0; i < num_strings; i++)
    {
      legacy_load_string(fp, string_list, i);
    }

#ifndef CONFIG_KHASH
    // Versions without the hash table require these to be sorted at all times
    sort_counter_list(counter_list);
    sort_string_list(string_list);
#endif

    // Sprite data
    for(i = 0; i < MAX_SPRITES; i++)
    {
      (mzx_world->sprite_list[i])->x = fgetw(fp);
      (mzx_world->sprite_list[i])->y = fgetw(fp);
      (mzx_world->sprite_list[i])->ref_x = fgetw(fp);
      (mzx_world->sprite_list[i])->ref_y = fgetw(fp);
      (mzx_world->sprite_list[i])->color = fgetc(fp);
      (mzx_world->sprite_list[i])->flags = fgetc(fp);
      (mzx_world->sprite_list[i])->width = fgetc(fp);
      (mzx_world->sprite_list[i])->height = fgetc(fp);
      (mzx_world->sprite_list[i])->col_x = fgetc(fp);
      (mzx_world->sprite_list[i])->col_y = fgetc(fp);
      (mzx_world->sprite_list[i])->col_width = fgetc(fp);
      (mzx_world->sprite_list[i])->col_height = fgetc(fp);
    }

    // total sprites
    mzx_world->active_sprites = fgetc(fp);
    // y order flag
    mzx_world->sprite_y_order = fgetc(fp);
    // collision info
    mzx_world->collision_count = fgetw(fp);

    for(i = 0; i < MAX_SPRITES; i++)
    {
      mzx_world->collision_list[i] = fgetw(fp);
    }

    // Multiplier
    mzx_world->multiplier = fgetw(fp);
    // Divider
    mzx_world->divider = fgetw(fp);
    // Circle divisions
    mzx_world->c_divisions = fgetw(fp);
    // String FREAD and FWRITE Delimiters
    mzx_world->fread_delimiter = fgetw(fp);
    mzx_world->fwrite_delimiter = fgetw(fp);
    // Builtin shooting/message status
    mzx_world->bi_shoot_status = fgetc(fp);
    mzx_world->bi_mesg_status = fgetc(fp);

    // Load input file name, open later
    {
      size_t len = fgetw(fp);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      if(len && !fread(mzx_world->input_file_name, len, 1, fp))
        goto err_close;
      mzx_world->input_file_name[len] = 0;
    }
    mzx_world->temp_input_pos = fgetd(fp);

    // Load output file name, open later
    {
      size_t len = fgetw(fp);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      if(len && !fread(mzx_world->output_file_name, len, 1, fp))
        goto err_close;
      mzx_world->output_file_name[len] = 0;
    }
    mzx_world->temp_output_pos = fgetd(fp);

    screen_mode = fgetw(fp);

    // If it's at SMZX mode 2, set default palette as loaded
    // so the .sav one doesn't get overwritten
    if(screen_mode == 2)
    {
      smzx_palette_loaded(1);
    }
    set_screen_mode(screen_mode);

    // Also get the palette
    if(screen_mode > 1)
    {
      for(i = 0; i < 256; i++)
      {
        r = fgetc(fp);
        g = fgetc(fp);
        b = fgetc(fp);

        set_rgb(i, r, g, b);
      }
    }

    mzx_world->commands = fgetd(fp);

    vlayer_size = fgetd(fp);
    mzx_world->vlayer_width = fgetw(fp);
    mzx_world->vlayer_height = fgetw(fp);
    mzx_world->vlayer_size = vlayer_size;

    // This might have been allocated already...
    mzx_world->vlayer_chars = crealloc(mzx_world->vlayer_chars, vlayer_size);
    mzx_world->vlayer_colors = crealloc(mzx_world->vlayer_colors, vlayer_size);

    if(vlayer_size &&
     (!fread(mzx_world->vlayer_chars, vlayer_size, 1, fp) ||
      !fread(mzx_world->vlayer_colors, vlayer_size, 1, fp)))
      goto err_close;
  }

  // Get position of global robot...
  global_robot_pos = fgetd(fp);
  // Get number of boards
  num_boards = fgetc(fp);

  if(num_boards == 0)
  {
    int sfx_size;
    char *sfx_offset = mzx_world->custom_sfx;
    // Sfx
    mzx_world->custom_sfx_on = 1;
    fgetw(fp); // Skip word size

    //Read sfx
    for(i = 0; i < NUM_SFX; i++, sfx_offset += LEGACY_SFX_SIZE)
    {
      sfx_size = fgetc(fp);
      if(sfx_size && !fread(sfx_offset, sfx_size, 1, fp))
        goto err_close;
    }
    num_boards = fgetc(fp);
  }
  else
  {
    mzx_world->custom_sfx_on = 0;
  }

  meter_target += num_boards;
  meter_update_screen(&meter_curr, meter_target);

  mzx_world->num_boards = num_boards;
  mzx_world->num_boards_allocated = num_boards;
  mzx_world->board_list = cmalloc(sizeof(struct board *) * num_boards);

  // Skip the names for now
  board_names_pos = ftell(fp);
  fseek(fp, num_boards * BOARD_NAME_SIZE, SEEK_CUR);

  // Read the board offsets/sizes preemptively to reduce the amount of seeking.
  board_offsets = cmalloc(sizeof(int) * num_boards);
  board_sizes = cmalloc(sizeof(int) * num_boards);
  for(i = 0; i < num_boards; i++)
  {
    board_sizes[i] = fgetd(fp);
    board_offsets[i] = fgetd(fp);
  }

  for(i = 0; i < num_boards; i++)
  {
    mzx_world->board_list[i] =
     legacy_load_board_allocate(mzx_world, fp, board_offsets[i], board_sizes[i],
      savegame, file_version);

    if(mzx_world->board_list[i])
    {
      // Also patch a pointer to the global robot
      if(mzx_world->board_list[i]->robot_list)
        (mzx_world->board_list[i])->robot_list[0] = &mzx_world->global_robot;

      // Also optimize out null objects
      optimize_null_objects(mzx_world->board_list[i]);

      store_board_to_extram(mzx_world->board_list[i]);
    }

    meter_update_screen(&meter_curr, meter_target);
  }

  free(board_offsets);
  free(board_sizes);

  // Read global robot
  fseek(fp, global_robot_pos, SEEK_SET); //don't worry if this fails
  legacy_load_robot(mzx_world, &mzx_world->global_robot, fp, savegame,
   file_version);

  // Some old worlds have the global_robot marked unused. Always mark it used.
  mzx_world->global_robot.used = 1;

  // Go back to where the names are
  fseek(fp, board_names_pos, SEEK_SET);
  for(i = 0; i < num_boards; i++)
  {
    cur_board = mzx_world->board_list[i];
    // Look at the name, width, and height of the just loaded board
    if(cur_board)
    {
      if(!fread(cur_board->board_name, BOARD_NAME_SIZE, 1, fp))
        cur_board->board_name[0] = 0;
    }
    else
    {
      char ignore[BOARD_NAME_SIZE];
      fread(ignore, BOARD_NAME_SIZE, 1, fp);
    }
  }

  meter_update_screen(&meter_curr, meter_target);
  meter_restore_screen();
  fclose(fp);
  return;

err_close:
  // Note that this file had already been successfully validated for length
  // and opened with no issue before this error occurred, and that the only
  // way to reach this error is a failed fread before any board/robot data
  // was loaded. Something seriously went wrong somewhere.

  error_message(E_IO_READ, 0, NULL);
  meter_restore_screen();
  fclose(fp);
}
