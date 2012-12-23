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
#include "util.h"
#include "validation.h"

static const char magic_code[16] =
 "\xE6\x52\xEB\xF2\x6D\x4D\x4A\xB7\x87\xB2\x92\x88\xDE\x91\x24";

#ifdef CONFIG_LOADSAVE_METER

static void meter_update_screen(int *curr, int target)
{
  (*curr)++;
  meter_interior(*curr, target);
  update_screen();
}

static void meter_restore_screen(void)
{
  restore_screen();
  update_screen();
}

static void meter_initial_draw(int curr, int target, const char *title)
{
  save_screen();
  meter(title, curr, target);
  update_screen();
}

#else // !CONFIG_LOADSAVE_METER

static inline void meter_update_screen(int *curr, int target) {}
static inline void meter_restore_screen(void) {}
static inline void meter_initial_draw(int curr, int target,
 const char *title) {}

#endif // CONFIG_LOADSAVE_METER

// Get 2 bytes

int fgetw(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp);
  if((a == EOF) || (b == EOF))
    return EOF;

  return (b << 8) | a;
}

// Get 4 bytes

int fgetd(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp), c = fgetc(fp), d = fgetc(fp);
  if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
    return EOF;

  return (d << 24) | (c << 16) | (b << 8) | a;
}

// Put 2 bytes

void fputw(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc(src >> 8, fp);
}

// Put 4 bytes

void fputd(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc((src >> 8) & 0xFF, fp);
  fputc((src >> 16) & 0xFF, fp);
  fputc((src >> 24) & 0xFF, fp);
}

static int get_pw_xor_code(char *password, int pro_method)
{
  int work = 85; // Start with 85... (01010101)
  size_t i;
  // Clear pw after first null

  for(i = strlen(password); i < 16; i++)
  {
    password[i] = 0;
  }

  for(i = 0; i < 15; i++)
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
  char password[15];
  char *file_buffer;
  char *src_ptr;
  char backup_name[MAX_PATH];

  int meter_target, meter_curr = 0;

  source = fopen_unsafe(file_name, "rb");
  file_length = ftell_and_rewind(source);

  meter_target = file_length + (file_length - 15) + 4;
 
  meter_initial_draw(meter_curr, meter_target, "Decrypting...");

  file_buffer = cmalloc(file_length);
  src_ptr = file_buffer;
  fread(file_buffer, file_length, 1, source);
  fclose(source);

  meter_curr = file_length - 1;
  meter_update_screen(&meter_curr, meter_target);

  src_ptr += 25;

  strncpy(backup_name, file_name, MAX_PATH - 8);
  strcat(backup_name, ".locked");
  backup = fopen_unsafe(backup_name, "wb");
  fwrite(file_buffer, file_length, 1, backup);
  fclose(backup);

  dest = fopen_unsafe(file_name, "wb");
  if(!dest)
  {
    error("Cannot decrypt write-protected world.", 1, 8, 0x0DD5);
    return;
  }
  pro_method = *src_ptr;
  src_ptr++;

  // Get password
  memcpy(password, src_ptr, 15);
  src_ptr += 18;
  // First, normalize password...
  for(i = 0; i < 15; i++)
  {
    password[i] ^= magic_code[i];
    password[i] -= 0x12 + pro_method;
    password[i] ^= 0x8D;
  }

  // Xor code
  xor_val = get_pw_xor_code(password, pro_method);

  // Copy title
  fwrite(file_buffer, 25, 1, dest);
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

  meter_curr = file_length + (file_length - 15) - 1;
  meter_update_screen(&meter_curr, meter_target);

  // Must fix all the absolute file positions so that they're 15
  // less now
  src_ptr = file_buffer + 4245;
  fseek(dest, 4230, SEEK_SET);
  offset_low_byte = src_ptr[0] ^ xor_val;
  fputc(offset_low_byte - 15, dest);
  if(offset_low_byte < 15)
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
  fseek(dest, (long)(src_ptr - file_buffer - 15), SEEK_SET);

  // Offset boards
  for(i = 0; i < num_boards; i++)
  {
    // Skip length
    src_ptr += 4;
    fseek(dest, 4, SEEK_CUR);

    // Get offset
    offset_low_byte = src_ptr[0] ^ xor_val;
    fputc(offset_low_byte - 15, dest);
    if(offset_low_byte < 15)
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

  free(file_buffer);
  fclose(dest);

  meter_restore_screen();
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

int save_world(struct world *mzx_world, const char *file, int savegame)
{
  int i, num_boards;
  int gl_rob_position, gl_rob_save_position;
  int board_offsets_position, board_begin_position;
  int board_size;
  unsigned int *size_offset_list;
  unsigned char *charset_mem;
  unsigned char r, g, b;
  struct board *cur_board;
  FILE *fp;

  int meter_target = 2 + mzx_world->num_boards, meter_curr = 0;

#ifdef CONFIG_DEBYTECODE
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
          // If it's not a 2.90 world, abort the save
          if(world_magic(tmp) < 0x025A)
          {
            error("Save would overwrite older world. Aborted.", 0, 1, 1337);
            goto exit_close;
          }
        }
      }
      fclose(fp);
    }
  }
#endif

  fp = fopen_unsafe(file, "wb");
  if(!fp)
  {
    error("Error saving world", 1, 8, 0x0D01);
    return -1;
  }

  meter_initial_draw(meter_curr, meter_target, "Saving...");

  if(savegame)
  {
    // Write this MZX's version string
    fputs("MZS", fp);
    fputc((WORLD_VERSION >> 8) & 0xff, fp);
    fputc(WORLD_VERSION & 0xff, fp);

    // Write the version of the loaded world for this SAV
    fputw(mzx_world->version, fp);

    fputc(mzx_world->current_board_id, fp);
  }
  else
  {
    fwrite(mzx_world->name, BOARD_NAME_SIZE, 1, fp);

    // No protection
    fputc(0, fp);

    // Write this MZX's version string
    fputc('M', fp);
    fputc((WORLD_VERSION >> 8) & 0xff, fp);
    fputc(WORLD_VERSION & 0xff, fp);
  }

  // Save charset
  charset_mem = cmalloc(3584);
  ec_mem_save_set(charset_mem);
  fwrite(charset_mem, 3584, 1, fp);
  free(charset_mem);

  // Save idchars array.
  fwrite(id_chars, 323, 1, fp);
  fputc(missile_color, fp);
  fwrite(bullet_color, 3, 1, fp);
  fwrite(id_dmg, 128, 1, fp);

  // Save status counters.
  fwrite((char *)mzx_world->status_counters_shown, COUNTER_NAME_SIZE,
   NUM_STATUS_COUNTERS, fp);

  /* Older MZX sources refer to SAVE_INDIVIDUAL, but it has always been
   * defined. Exo eventually removed the conditional code in 2.80.
   * We don't need to think about it.
   */

  if(savegame)
  {
    fwrite(mzx_world->keys, NUM_KEYS, 1, fp);
    fputc(mzx_world->blind_dur, fp);
    fputc(mzx_world->firewalker_dur, fp);
    fputc(mzx_world->freeze_time_dur, fp);
    fputc(mzx_world->slow_time_dur, fp);
    fputc(mzx_world->wind_dur, fp);

    for(i = 0; i < 8; i++)
    {
      fputw(mzx_world->pl_saved_x[i], fp);
    }

    for(i = 0; i < 8; i++)
    {
      fputw(mzx_world->pl_saved_y[i], fp);
    }

    fwrite(mzx_world->pl_saved_board, 8, 1, fp);
    fputc(mzx_world->saved_pl_color, fp);
    fputc(mzx_world->under_player_id, fp);
    fputc(mzx_world->under_player_color, fp);
    fputc(mzx_world->under_player_param, fp);
    fputc(mzx_world->mesg_edges, fp);
    fputc(mzx_world->scroll_base_color, fp);
    fputc(mzx_world->scroll_corner_color, fp);
    fputc(mzx_world->scroll_pointer_color, fp);
    fputc(mzx_world->scroll_title_color, fp);
    fputc(mzx_world->scroll_arrow_color, fp);

    {
      size_t len = strlen(mzx_world->real_mod_playing);
      fputw((int)len, fp);
      if(len)
        fwrite(mzx_world->real_mod_playing, len, 1, fp);
    }
  }

  fputc(mzx_world->edge_color, fp);
  fputc(mzx_world->first_board, fp);
  fputc(mzx_world->endgame_board, fp);
  fputc(mzx_world->death_board, fp);
  fputw(mzx_world->endgame_x, fp);
  fputw(mzx_world->endgame_y, fp);
  fputc(mzx_world->game_over_sfx, fp);
  fputw(mzx_world->death_x, fp);
  fputw(mzx_world->death_y, fp);
  fputw(mzx_world->starting_lives, fp);
  fputw(mzx_world->lives_limit, fp);
  fputw(mzx_world->starting_health, fp);
  fputw(mzx_world->health_limit, fp);
  fputc(mzx_world->enemy_hurt_enemy, fp);
  fputc(mzx_world->clear_on_exit, fp);
  fputc(mzx_world->only_from_swap, fp);

  // Palette...
  for(i = 0; i < 16; i++)
  {
    get_rgb(i, &r, &g, &b);
    fputc(r, fp);
    fputc(g, fp);
    fputc(b, fp);
  }

  if(savegame)
  {
    struct counter *mzx_speed, *lock_speed;
    int vlayer_size;

    for(i = 0; i < 16; i++)
    {
      fputc(get_color_intensity(i), fp);
    }
    fputc(get_fade_status(), fp);

    fputw(mzx_world->player_restart_x, fp);
    fputw(mzx_world->player_restart_y, fp);
    fputc(mzx_world->under_player_id, fp);
    fputc(mzx_world->under_player_color, fp);
    fputc(mzx_world->under_player_param, fp);

    // Write regular counters + mzx_speed
    fputd(mzx_world->num_counters + 2, fp);
    for(i = 0; i < mzx_world->num_counters; i++)
    {
      save_counter(fp, mzx_world->counter_list[i]);
    }

    mzx_speed = malloc(sizeof(struct counter) + sizeof("mzx_speed") - 1);
    mzx_speed->value = mzx_world->mzx_speed;
    strcpy(mzx_speed->name, "mzx_speed");
    save_counter(fp, mzx_speed);
    free(mzx_speed);
    lock_speed = malloc(sizeof(struct counter) + sizeof("_____lock_speed") - 1);
    lock_speed->value = mzx_world->lock_speed;
    strcpy(lock_speed->name, "_____lock_speed");
    save_counter(fp, lock_speed);
    free(lock_speed);

    // Write strings
    fputd(mzx_world->num_strings, fp);

    for(i = 0; i < mzx_world->num_strings; i++)
    {
      save_string(fp, mzx_world->string_list[i]);
    }

    // Sprite data
    for(i = 0; i < MAX_SPRITES; i++)
    {
      fputw((mzx_world->sprite_list[i])->x, fp);
      fputw((mzx_world->sprite_list[i])->y, fp);
      fputw((mzx_world->sprite_list[i])->ref_x, fp);
      fputw((mzx_world->sprite_list[i])->ref_y, fp);
      fputc((mzx_world->sprite_list[i])->color, fp);
      fputc((mzx_world->sprite_list[i])->flags, fp);
      fputc((mzx_world->sprite_list[i])->width, fp);
      fputc((mzx_world->sprite_list[i])->height, fp);
      fputc((mzx_world->sprite_list[i])->col_x, fp);
      fputc((mzx_world->sprite_list[i])->col_y, fp);
      fputc((mzx_world->sprite_list[i])->col_width, fp);
      fputc((mzx_world->sprite_list[i])->col_height, fp);
    }
    // total sprites
    fputc(mzx_world->active_sprites, fp);
    // y order flag
    fputc(mzx_world->sprite_y_order, fp);
    // collision info
    fputw(mzx_world->collision_count, fp);

    for(i = 0; i < MAX_SPRITES; i++)
    {
      fputw(mzx_world->collision_list[i], fp);
    }

    // Multiplier
    fputw(mzx_world->multiplier, fp);
    // Divider
    fputw(mzx_world->divider, fp);
    // Circle divisions
    fputw(mzx_world->c_divisions, fp);
    // String FREAD and FWRITE Delimiters
    fputw(mzx_world->fread_delimiter, fp);
    fputw(mzx_world->fwrite_delimiter, fp);
    // Builtin shooting/message status
    fputc(mzx_world->bi_shoot_status, fp);
    fputc(mzx_world->bi_mesg_status, fp);

    // Write input file name and if open, position
    {
      size_t len = strlen(mzx_world->input_file_name);
      fputw((int)len, fp);
      if(len)
        fwrite(mzx_world->input_file_name, len, 1, fp);
    }

    if(!mzx_world->input_is_dir && mzx_world->input_file)
    {
      fputd(ftell(mzx_world->input_file), fp);
    }
    else if(mzx_world->input_is_dir)
    {
      fputd(dir_tell(&mzx_world->input_directory), fp);
    }
    else
    {
      fputd(0, fp);
    }

    // Write output file name and if open, position
    {
      size_t len = strlen(mzx_world->output_file_name);
      fputw((int)len, fp);
      if(len)
        fwrite(mzx_world->output_file_name, len, 1, fp);
    }

    if(mzx_world->output_file)
    {
      fputd(ftell(mzx_world->output_file), fp);
    }
    else
    {
      fputd(0, fp);
    }

    fputw(get_screen_mode(), fp);

    if(get_screen_mode() > 1)
    {
      // Put SMZX mode 2 palette
      for(i = 0; i < 256; i++)
      {
        get_rgb(i, &r, &g, &b);
        fputc(r, fp);
        fputc(g, fp);
        fputc(b, fp);
      }
    }

    fputd(mzx_world->commands, fp);

    vlayer_size = mzx_world->vlayer_size;
    fputd(vlayer_size, fp);
    fputw(mzx_world->vlayer_width, fp);
    fputw(mzx_world->vlayer_height, fp);

    fwrite(mzx_world->vlayer_chars, 1, vlayer_size, fp);
    fwrite(mzx_world->vlayer_colors, 1, vlayer_size, fp);
  }

  // Put position of global robot later
  gl_rob_save_position = ftell(fp);
  // Put some 0's
  fputd(0, fp);

  // Put custom fx?
  if(mzx_world->custom_sfx_on == 1)
  {
    int offset = 0;
    size_t sfx_len;
    int length_slot_pos, next_pos, total_len;
    fputc(0, fp);
    length_slot_pos = ftell(fp);
    fputw(0, fp);
    for(i = 0; i < NUM_SFX; i++, offset += 69)
    {
      sfx_len = strlen(mzx_world->custom_sfx + offset);
      fputc((int)sfx_len, fp);
      fwrite(mzx_world->custom_sfx + offset, sfx_len, 1, fp);
    }
    // Get size of the block
    next_pos = ftell(fp);
    total_len = (next_pos - length_slot_pos) - 2;
    fseek(fp, length_slot_pos, SEEK_SET);
    fputw(total_len, fp);
    fseek(fp, next_pos, SEEK_SET);
  }

  meter_update_screen(&meter_curr, meter_target);

  num_boards = mzx_world->num_boards;
  fputc(num_boards, fp);

  // Put the names
  for(i = 0; i < num_boards; i++)
  {
    fwrite((mzx_world->board_list[i])->board_name, 25, 1, fp);
  }

  /* Due to some bugs in the NDS's libfat library, seeking backwards
   * from the end results in data corruption. To prevent this, waste
   * a little bit of memory caching the offsets of the board data so
   * we can rewrite the size/offset list with less seeking later.
   */
  size_offset_list = cmalloc(8 * num_boards);
  board_offsets_position = ftell(fp);
  fseek(fp, 8 * num_boards, SEEK_CUR);

  for(i = 0; i < num_boards; i++)
  {
    cur_board = mzx_world->board_list[i];

    // Before messing with the board, make sure the board is
    // rid of any gaps in the object lists...
    optimize_null_objects(cur_board);

    // First save the offset of where the board will be placed
    board_begin_position = ftell(fp);
    // Now save the board and get the size
    board_size = save_board(cur_board, fp, savegame, WORLD_VERSION);
    // board_end_position, unused
    ftell(fp);
    // Record size/offset information.
    size_offset_list[2 * i] = board_size;
    size_offset_list[2 * i + 1] = board_begin_position;

    meter_update_screen(&meter_curr, meter_target);
  }

  // Save for global robot position
  gl_rob_position = ftell(fp);
  save_robot(&mzx_world->global_robot, fp, savegame, WORLD_VERSION);

  meter_update_screen(&meter_curr, meter_target);

  // Go back to where the global robot position should be saved
  fseek(fp, gl_rob_save_position, SEEK_SET);
  fputd(gl_rob_position, fp);

  // Go back to offsets/size list
  fseek(fp, board_offsets_position, SEEK_SET);
  for(i = 0; i < num_boards; i++)
  {
    fputd(size_offset_list[2 * i  ], fp);
    fputd(size_offset_list[2 * i + 1], fp);
  }
  free(size_offset_list);

  meter_restore_screen();

#ifdef CONFIG_DEBYTECODE
exit_close:
#endif
  fclose(fp);
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

__editor_maybe_static void set_update_done(struct world *mzx_world)
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

__editor_maybe_static FILE *try_load_world(const char *file,
 bool savegame, int *version, char *name)
{
  FILE *fp;
  char magic[5];
  int v;

  enum val_result status = validate_world_file(file, savegame, NULL, 0);

  if(VAL_NEED_UNLOCK == status)
  {
    decrypt(file);
    status = validate_world_file(file, savegame, NULL, 1);
  }
  if(VAL_SUCCESS != status)
    goto err_out;

  // Validation succeeded so this should be a breeze.
  fp = fopen_unsafe(file, "rb");
  if(!fp)
  {
    error("Post validation IO error occurred", 1, 8, 0x0D01);
    goto err_out;
  }

  if(savegame)
  {
    fread(magic, 5, 1, fp);

    v = save_magic(magic);
  }
  else
  {
    if(name)
      fread(name, BOARD_NAME_SIZE, 1, fp);
    else
      fseek(fp, BOARD_NAME_SIZE, SEEK_CUR);

    fseek(fp, 1, SEEK_CUR); // Skip protection byte.

    fread(magic, 1, 3, fp);

    v = world_magic(magic);
  }

  if(version)
    *version = v;
  return fp;

err_out:
  return NULL;
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

// Loads a world into a struct world

static void load_world(struct world *mzx_world, FILE *fp, const char *file,
 bool savegame, int version, char *name, int *faded)
{
  int i;
  int num_boards;
  int gl_rob, last_pos;
  unsigned char *charset_mem;
  unsigned char r, g, b;
  struct board *cur_board;
  char *config_file_name;
  size_t file_name_len = strlen(file) - 4;
  struct stat file_info;
  char *file_path;
  char *current_dir;

  int meter_target = 2, meter_curr = 0;

  if(savegame)
  {
    mzx_world->version = fgetw(fp);
    mzx_world->current_board_id = fgetc(fp);
  }
  else
  {
    strcpy(mzx_world->name, name);
    mzx_world->version = version;
    mzx_world->current_board_id = 0;
  }

  meter_initial_draw(meter_curr, meter_target, "Loading...");

  file_path = cmalloc(MAX_PATH);
  current_dir = cmalloc(MAX_PATH);
  config_file_name = cmalloc(MAX_PATH);

  get_path(file, file_path, MAX_PATH);

  if(file_path[0])
  {
    getcwd(current_dir, MAX_PATH);

    if(strcmp(current_dir, file_path))
      chdir(file_path);
  }

  memcpy(config_file_name, file, file_name_len);
  strncpy(config_file_name + file_name_len, ".cnf", 5);

  if(stat(config_file_name, &file_info) >= 0)
  {
    set_config_from_file(&(mzx_world->conf), config_file_name);
  }

  free(config_file_name);
  free(current_dir);
  free(file_path);

  charset_mem = cmalloc(3584);
  fread(charset_mem, 3584, 1, fp);
  ec_mem_load_set(charset_mem);
  free(charset_mem);

  // Idchars array...
  fread(id_chars, 323, 1, fp);
  missile_color = fgetc(fp);
  fread(bullet_color, 3, 1, fp);
  fread(id_dmg, 128, 1, fp);

  // Status counters...
  fread((char *)mzx_world->status_counters_shown, COUNTER_NAME_SIZE,
   NUM_STATUS_COUNTERS, fp);

  if(savegame)
  {
    fread(mzx_world->keys, NUM_KEYS, 1, fp);
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

    fread(mzx_world->pl_saved_board, 8, 1, fp);
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

      fread(mzx_world->real_mod_playing, len, 1, fp);
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
    mzx_world->num_counters = num_counters;
    mzx_world->num_counters_allocated = num_counters;
    mzx_world->counter_list = ccalloc(num_counters, sizeof(struct counter *));

    for(i = 0, j = 0; i < num_counters; i++)
    {
      struct counter *counter = load_counter(mzx_world, fp);

      /* We loaded a special counter, this doesn't need to be
       * loaded into the regular list.
       */
      if(!counter)
      {
        mzx_world->num_counters--;
        continue;
      }

      mzx_world->counter_list[j] = counter;
      j++;
    }

    // Setup gateway functions
    initialize_gateway_functions(mzx_world);

    // Read strings
    num_strings = fgetd(fp);
    mzx_world->num_strings = num_strings;
    mzx_world->num_strings_allocated = num_strings;
    mzx_world->string_list = ccalloc(num_strings, sizeof(struct string *));

    for(i = 0; i < num_strings; i++)
    {
      mzx_world->string_list[i] = load_string(fp);
      mzx_world->string_list[i]->list_ind = i;
    }

    // Allocate space for sprites and clist
    mzx_world->num_sprites = MAX_SPRITES;
    mzx_world->sprite_list = ccalloc(MAX_SPRITES, sizeof(struct sprite *));

    for(i = 0; i < MAX_SPRITES; i++)
    {
      mzx_world->sprite_list[i] = ccalloc(1, sizeof(struct sprite));
    }

    mzx_world->collision_list = ccalloc(MAX_SPRITES, sizeof(int));
    mzx_world->sprite_num = 0;

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

    {
      size_t len = fgetw(fp);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      fread(mzx_world->input_file_name, len, 1, fp);
      mzx_world->input_file_name[len] = 0;
    }

    if(mzx_world->input_file_name[0])
    {
      char *translated_path = cmalloc(MAX_PATH);
      int err;

      mzx_world->input_is_dir = false;

      err = fsafetranslate(mzx_world->input_file_name, translated_path);
      if(err == -FSAFE_MATCHED_DIRECTORY)
      {
        if(dir_open(&mzx_world->input_directory, translated_path))
        {
          dir_seek(&mzx_world->input_directory, fgetd(fp));
          mzx_world->input_is_dir = true;
        }
        else
          fseek(fp, 4, SEEK_CUR);
      }
      else if(err == -FSAFE_SUCCESS)
      {
        mzx_world->input_file = fopen_unsafe(translated_path, "rb");
        if(mzx_world->input_file)
          fseek(mzx_world->input_file, fgetd(fp), SEEK_SET);
        else
          fseek(fp, 4, SEEK_CUR);
      }

      free(translated_path);
    }
    else
    {
      fseek(fp, 4, SEEK_CUR);
    }

    // Load ouput file name, open
    {
      size_t len = fgetw(fp);
      if(len >= MAX_PATH)
        len = MAX_PATH - 1;

      fread(mzx_world->output_file_name, len, 1, fp);
      mzx_world->output_file_name[len] = 0;
    }

    if(mzx_world->output_file_name[0])
    {
      mzx_world->output_file =
       fsafeopen(mzx_world->output_file_name, "ab");

      if(mzx_world->output_file)
      {
        fseek(mzx_world->output_file, fgetd(fp), SEEK_SET);
      }
      else
      {
        fseek(fp, 4, SEEK_CUR);
      }
    }
    else
    {
      fseek(fp, 4, SEEK_CUR);
    }

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

    mzx_world->vlayer_chars = cmalloc(vlayer_size);
    mzx_world->vlayer_colors = cmalloc(vlayer_size);

    fread(mzx_world->vlayer_chars, 1, vlayer_size, fp);
    fread(mzx_world->vlayer_colors, 1, vlayer_size, fp);
  }

  update_palette();

  // Get position of global robot...
  gl_rob = fgetd(fp);
  // Get number of boards
  num_boards = fgetc(fp);

  if(num_boards == 0)
  {
    int sfx_size;
    char *sfx_offset = mzx_world->custom_sfx;
    // Sfx
    mzx_world->custom_sfx_on = 1;
    fseek(fp, 2, SEEK_CUR);     // Skip word size

    //Read sfx
    for(i = 0; i < NUM_SFX; i++, sfx_offset += 69)
    {
      sfx_size = fgetc(fp);
      fread(sfx_offset, sfx_size, 1, fp);
#ifdef CONFIG_DEBYTECODE
      if(version < 0x025A)
        convert_sfx_strs(sfx_offset);
#endif
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
  // Gonna wanna come back to here
  last_pos = ftell(fp);
  fseek(fp, num_boards * BOARD_NAME_SIZE, SEEK_CUR);

  for(i = 0; i < num_boards; i++)
  {
    mzx_world->board_list[i] =
     load_board_allocate(fp, savegame, version, mzx_world->version);
    store_board_to_extram(mzx_world->board_list[i]);
    meter_update_screen(&meter_curr, meter_target);
  }

  // Read global robot
  fseek(fp, gl_rob, SEEK_SET); //don't worry if this fails
  load_robot(&mzx_world->global_robot, fp, savegame, version);

  // Go back to where the names are
  fseek(fp, last_pos, SEEK_SET);
  for(i = 0; i < num_boards; i++)
  {
    cur_board = mzx_world->board_list[i];
    // Look at the name, width, and height of the just loaded board
    if(cur_board)
    {
      fread(cur_board->board_name, BOARD_NAME_SIZE, 1, fp);

      // Also patch a pointer to the global robot
      if(cur_board->robot_list)
        (mzx_world->board_list[i])->robot_list[0] = &mzx_world->global_robot;

      // Also optimize out null objects
      retrieve_board_from_extram(mzx_world->board_list[i]);
      optimize_null_objects(mzx_world->board_list[i]);
      store_board_to_extram(mzx_world->board_list[i]);
    }
    else
    {
      fseek(fp, BOARD_NAME_SIZE, SEEK_CUR);
    }
  }

  meter_update_screen(&meter_curr, meter_target);

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

  meter_restore_screen();

  fclose(fp);
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

// FIXME: This function should probably die. It's unsafe.
void add_ext(char *src, const char *ext)
{
  size_t len = strlen(src);

  if((len < 4) || ((src[len - 4] != '.') && (src[len - 3] != '.')
   && (src[len - 2] != '.')))
  {
    strncat(src, ext, 4);
  }
}
