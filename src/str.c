/* MegaZeux
 *
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

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

#include "counter.h"
#include "error.h"
#include "graphics.h"
#include "memcasecmp.h"
#include "rasm.h"
#include "robot.h"
#include "util.h"
#include "world.h"
#include "world_struct.h"

/**
 * TODO: String lookups are currently case-insensitive, which is somewhat of
 * a performance concern. A good future (3.xx) feature might be to lowercase
 * all string names.
 */

#ifdef CONFIG_KHASH
#include <khashmzx.h>
KHASH_SET_INIT(STRING, struct string *, name, name_length)
#endif

// Please only use string literals with this, thanks.

#define special_name(n)                                         \
  ((src_length == (sizeof(n) - 1)) &&                           \
   !strncasecmp(src_value, n, sizeof(n) - 1))                   \

#define special_name_partial(n)                                 \
  ((src_length >= (int)(sizeof(n) - 1)) &&                      \
   !strncasecmp(src_value, n, sizeof(n) - 1))                   \


static unsigned int get_board_x_board_y_offset(struct world *mzx_world, int id)
{
  int board_x = get_counter(mzx_world, "board_x", id);
  int board_y = get_counter(mzx_world, "board_y", id);

  board_x = CLAMP(board_x, 0, mzx_world->current_board->board_width - 1);
  board_y = CLAMP(board_y, 0, mzx_world->current_board->board_height - 1);

  return board_y * mzx_world->current_board->board_width + board_x;
}

static struct robot *get_robot_by_id(struct world *mzx_world, int id)
{
  if(id >= 0 && id <= mzx_world->current_board->num_robots)
    return mzx_world->current_board->robot_list[id];
  else
    return NULL;
}

static struct string *find_string(struct string_list *string_list,
 const char *name, int *next)
{
  struct string *current = NULL;

#if defined(CONFIG_KHASH)
  size_t name_length = strlen(name);
  KHASH_FIND(STRING, string_list->hash_table, name, name_length, current);

  // When reallocing we need to replace the old pointer at
  // its original list index.
  *next = current ? current->list_ind : string_list->num_strings;
  return current;

#else
  struct string **base = string_list->strings;
  int bottom = 0, top = (string_list->num_strings) - 1, middle = 0;
  int cmpval = 0;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else

    if(cmpval < 0)
    {
      top = middle - 1;
    }
    else
    {
      *next = middle;
      return current;
    }
  }

  if(cmpval > 0)
    *next = middle + 1;
  else
    *next = middle;

  return NULL;
#endif
}

static struct string *allocate_new_string(const char *name, int name_length,
 size_t length)
{
  // Allocate a string with room for the name and initial value.
  // Does not initialize the value or the list index.
  struct string *dest = cmalloc(sizeof(struct string) + name_length + length);

  memcpy(dest->name, name, name_length);
  dest->name[name_length] = 0;

  dest->name_length = name_length;
  dest->allocated_length = length;
  dest->length = length;
  dest->value = dest->name + name_length + 1;
  return dest;
}

static struct string *add_string_preallocate(struct string_list *string_list,
 const char *name, size_t length, int position)
{
  int count = string_list->num_strings;
  int allocated = string_list->num_strings_allocated;
  size_t name_length = strlen(name);
  struct string **base = string_list->strings;
  struct string *dest;

  // Need a reallocation?
  if(count == allocated)
  {
    if(allocated)
      allocated *= 2;
    else
      allocated = MIN_STRING_ALLOCATE;

    base = crealloc(base, sizeof(struct string *) * allocated);
    string_list->strings = base;
    string_list->num_strings_allocated = allocated;
  }

  // Doesn't exist, so create it
  // First make space for it if it's not at the top
  if(position != count)
  {
    base += position;
    memmove((char *)(base + 1), (char *)base,
     (count - position) * sizeof(struct string *));
  }

  dest = allocate_new_string(name, name_length, length);

  // Initialize the value to zero.
  if(length > 0)
    memset(dest->value, ' ', length);

  dest->list_ind = position;
  string_list->strings[position] = dest;
  string_list->num_strings = count + 1;

#ifdef CONFIG_KHASH
  KHASH_ADD(STRING, string_list->hash_table, dest);
#endif

  return dest;
}

static struct string *reallocate_string(struct string_list *string_list,
 struct string *src, int pos, size_t length)
{
  // Find the base length (take out the current length)
  int base_length = (int)(src->value - (char *)src);

#ifdef CONFIG_KHASH
  // Delete the string with the same name as src if it exists in the table.
  KHASH_DELETE(STRING, string_list->hash_table, src);
#endif

  src = crealloc(src, base_length + length);
  src->value = (char *)src + base_length;

  // any new bits of the string should be space filled
  // versions up to and including 2.81h used to fill this with garbage
  if(src->allocated_length < length)
  {
    memset(&src->value[src->allocated_length], ' ',
     length - src->allocated_length);
  }

  src->allocated_length = length;

  string_list->strings[pos] = src;

#ifdef CONFIG_KHASH
  KHASH_ADD(STRING, string_list->hash_table, src);
#endif

  return src;
}

static void force_string_length(struct string_list *string_list,
 const char *name, int next, struct string **str, size_t *length)
{
  if(*length > MAX_STRING_LEN)
    *length = MAX_STRING_LEN;

  if(!*str)
    *str = add_string_preallocate(string_list, name, *length, next);

  else if(*length > (*str)->allocated_length)
    *str = reallocate_string(string_list, *str, next, *length);

  /* Wipe string if the length has increased but not the allocated memory */
  if(*length > (*str)->length)
    memset(&((*str)->value[(*str)->length]), ' ', (*length) - (*str)->length);
}

static void force_string_splice(struct string_list *string_list,
 const char *name, int next, struct string **str, size_t s_length,
 size_t offset, boolean offset_specified, size_t *size, boolean size_specified)
{
  force_string_length(string_list, name, next, str, &s_length);

  if((*size == 0 && !size_specified) || *size > s_length)
    *size = s_length;

  if((offset == 0 && !offset_specified) || (offset + *size > (*str)->length))
  {
    if(offset + *size > (*str)->length)
    {
      size_t length = offset + *size;
      force_string_length(string_list, name, next, str, &length);
      (*str)->length = length;
    }
    else
      (*str)->length = offset + *size;
  }
}

static void force_string_copy(struct string_list *string_list,
 const char *name, int next, struct string **str, size_t s_length,
 size_t offset, boolean offset_specified, size_t *size, boolean size_specified,
 char *src)
{
  force_string_splice(string_list, name, next, str, s_length,
   offset, offset_specified, size, size_specified);

  if(offset <= (*str)->length - *size)
    memcpy((*str)->value + offset, src, *size);
}

static void force_string_move(struct string_list *string_list,
 const char *name, int next, struct string **str, size_t s_length,
 size_t offset, boolean offset_specified, size_t *size, boolean size_specified,
 char *src)
{
  boolean src_dest_match = false;
  ssize_t off = 0;

  if(*str)
  {
    off = (ssize_t)(src - (*str)->value);
    if(off >= 0 && (unsigned int)off <= (*str)->length)
      src_dest_match = true;
  }

  force_string_splice(string_list, name, next, str, s_length,
   offset, offset_specified, size, size_specified);

  if(src_dest_match)
    src = (*str)->value + off;

  if(offset <= (*str)->length - *size)
    memmove((*str)->value + offset, src, *size);
}

static void get_string_dot_value(char *dot_ptr, int *index,
 size_t *size, boolean *index_specified)
{
  char *error;

  if(dot_ptr[0])
  {
    // Index
    *index = strtol(dot_ptr, &error, 10);

    // Multi-byte index
    if(error[0] == '#' && error[1])
    {
      int result = strtoul(error + 1, &error, 10);

      *size = CLAMP(result, 1, 4);
    }

    if(!error[0])
    {
      *index_specified = true;
    }
  }
}

static boolean get_string_real_index(struct string *src, int index,
 size_t *real)
{
  // Handle negative indexing
  if(index < 0)
  {
    // Negative indexes only work with existing strings
    if(!src)
      return true;

    index += src->length;

    if(index < 0)
    {
      // Invalid index
      return true;
    }
  }

  *real = index;
  return false;
}

static int get_string_numeric_value(struct string *src)
{
  // strtol wants a null terminator and we don't want to duplicate the string,
  // so reimplement a base 10 strtol to use the string length instead. This
  // also seems to run a lot faster than strtol anyway.
  boolean negative = false;
  int overflow = INT_MAX;
  int overflow_d = INT_MAX % 10;
  int value = 0;
  int digit;
  char *end;
  char *pos;

  if(src && src->length)
  {
    pos = src->value;
    end = src->value + src->length;

    // Skip whitespace
    while(pos < end && isspace((int)*pos))
      pos++;

    if(pos >= end)
      return 0;

    if(*pos == '-')
    {
      negative = true;
      overflow = INT_MIN;
      overflow_d = -(INT_MIN % 10);
      pos++;
    }
    else

    if(*pos == '+')
      pos++;

    while(pos < end && isdigit((int)*pos))
    {
      digit = *(pos++) - '0';

      // Overflow
      if(value >= (INT_MAX / 10))
        if(value > (INT_MAX / 10) || digit >= overflow_d)
          return overflow;

      value = value * 10 + digit;
    }

    if(negative)
      value = -value;

    return (int)value;
  }
  return 0;
}

int string_read_as_counter(struct world *mzx_world,
 const char *name, int id)
{
  struct string_list *string_list = &(mzx_world->string_list);
  char *dot_ptr = strchr(name + 1, '.');
  struct string src;

  // User may have provided $str.N or $str.length explicitly
  if(dot_ptr)
  {
    struct string *src;
    int next;

    *dot_ptr = 0;
    src = find_string(string_list, name, &next);

    // Fix the dot because currently stuff like inc/dec/multiply
    // does a read function call followed by a write function call
    *dot_ptr = '.';
    dot_ptr++;

    if(!src)
    {
      return 0;
    }
    else

    if(!strcasecmp(dot_ptr, "length"))
    {
      // str.length
      return (int)src->length;
    }

    else
    {
      // string.[index]
      boolean index_specified = false;
      size_t real_index;
      size_t size = 1;
      int index;
      int value = 0;

      // Check to see if there's a valid string indexing
      get_string_dot_value(dot_ptr, &index, &size, &index_specified);

      if(index_specified)
      {
        // Negative indices and multiple indexing (2.91+)
        if(mzx_world->version >= V291)
        {
          if(get_string_real_index(src, index, &real_index))
            return 0;

          if(src->length < size + real_index)
            size = src->length - real_index;
        }
        else
        {
          real_index = index;
          size = 1;
        }

        // If we're in bounds return the char at this offset
        if(real_index < src->length)
        {
          switch(size)
          {
            case 4: value |= (int)src->value[real_index + 3] << 24; // fallthru
            case 3: value |= (int)src->value[real_index + 2] << 16; // fallthru
            case 2: value |= (int)src->value[real_index + 1] << 8;  // fallthru
            case 1: value |= (int)src->value[real_index];
          }
          return value;
        }
      }
    }
  }
  else

  // Otherwise fall back to looking up a regular string
  if(get_string(mzx_world, name, &src, id))
    return get_string_numeric_value(&src);

  // The string wasn't found or the request was out of bounds
  return 0;
}

void string_write_as_counter(struct world *mzx_world,
 const char *name, int value, int id)
{
  struct string_list *string_list = &(mzx_world->string_list);
  char *dot_ptr = strrchr(name + 1, '.');

  // User may have provided $str.N notation "write char at offset"
  if(dot_ptr)
  {
    struct string *src;
    boolean index_specified = false;
    size_t old_length;
    size_t new_length;
    size_t alloc_length;
    size_t write_size = 1;
    size_t write_index = 0;
    int next;

    *dot_ptr = 0;
    dot_ptr++;

    /* As a special case, alter the string's length if '$str.length'
     * is written to.
     */
    if(!strcasecmp(dot_ptr, "length"))
    {
      // Ignore impossible lengths
      if(value < 0)
        return;

      // Writing to length from a non-existent string has no effect
      src = find_string(string_list, name, &next);
      if(!src)
        return;

      new_length = value;
      alloc_length = new_length + 1;
    }

    else
    {
      int index;

      // Check to see if there's a valid string indexing
      get_string_dot_value(dot_ptr, &index, &write_size, &index_specified);
      if(!index_specified)
        return;

      src = find_string(string_list, name, &next);

      // Negative indices (2.91+)
      if(index < 0 && mzx_world->version < V291)
        return;

      if(get_string_real_index(src, index, &write_index))
        return;

      // Multiple size (2.91+)
      if(mzx_world->version < V291)
        write_size = 1;

      // Tentatively increase the string's length to cater for this write
      new_length = write_index + write_size;
      alloc_length = new_length;
    }

    if(new_length > MAX_STRING_LEN)
      return;

    /* As a kind of unnecessary optimisation, if the string already exists
     * and we're asking to extend its length, increase the length by a power
     * of two rather than just by the amount necessary.
     */

    if(src != NULL)
    {
      old_length = src->allocated_length;

      if(alloc_length > old_length)
      {
        unsigned int i;

        for(i = 1 << 31; i != 0; i >>= 1)
          if(alloc_length & i)
            break;

        alloc_length = i << 1;
      }
    }

    force_string_length(string_list, name, next, &src, &alloc_length);

    if(index_specified)
    {
      switch(write_size)
      {
        case 4: src->value[write_index + 3] = value >> 24; // fallthru
        case 3: src->value[write_index + 2] = value >> 16; // fallthru
        case 2: src->value[write_index + 1] = value >> 8; // fallthru
        case 1: src->value[write_index] = value;
      }
      if(src->length < new_length)
        src->length = new_length;
    }
    else
    {
      src->value[new_length] = 0;
      src->length = new_length;

      // Before 2.84, the length would be set to the alloc_length
      if(mzx_world->version < V284)
        src->length = alloc_length;
    }
  }
  else
  {
    struct string src_str;
    char n_buffer[16];
    snprintf(n_buffer, 16, "%d", value);

    src_str.value = n_buffer;
    src_str.length = strlen(n_buffer);
    set_string(mzx_world, name, &src_str, id);
  }
}

static void add_string(struct string_list *string_list, const char *name,
 struct string *src, int position)
{
  struct string *dest = add_string_preallocate(string_list, name,
   src->length, position);

  memcpy(dest->value, src->value, src->length);
}

static boolean get_string_size_offset(const char *name, size_t *ssize,
 boolean *size_specified, int *soffset, boolean *offset_specified)
{
  char *offset_position = strchr(name, '+');
  char *size_position = strchr(name, '#');
  char *error;

  if(size_position)
    *size_position = 0;

  if(offset_position)
    *offset_position = 0;

  if(size_position)
  {
    size_t ret = strtoul(size_position + 1, &error, 10);

    if(error[0])
      return true;

    *size_specified = true;
    *ssize = ret;
  }

  if(offset_position)
  {
    int ret = strtol(offset_position + 1, &error, 10);

    if(error[0])
      return true;

    *offset_specified = true;
    *soffset = ret;
  }

  return false;
}

static int load_string_board_direct(char *dest_chars, int dest_size,
 char *src_chars, int src_width, int block_width, int block_height,
 char terminator)
{
  char *dest_pos = dest_chars;
  int dest_height = dest_size / block_width;
  int dest_left = dest_size % block_width;
  int i;

  for(i = 0; i < dest_height; i++)
  {
    memcpy(dest_pos, src_chars, block_width);
    dest_pos += block_width;
    src_chars += src_width;
  }

  if(dest_left)
    memcpy(dest_pos, src_chars, dest_left);

  if(terminator)
  {
    for(i = 0; i < dest_size; i++)
    {
      if(dest_chars[i] == terminator)
        break;
    }

    return i;
  }

  return dest_size;
}

void load_string_board(struct world *mzx_world, const char *name,
 char *src_chars, int src_width, int block_width, int block_height,
 char terminator)
{
  struct string_list *string_list = &(mzx_world->string_list);
  boolean offset_specified = false;
  boolean size_specified = false;
  size_t dest_size = (size_t)(block_width * block_height);
  size_t dest_offset = 0;
  int input_offset = 0;
  struct string *dest;
  char *copy_buffer;
  size_t copy_size;
  int next;

  boolean error = get_string_size_offset(name, &dest_size, &size_specified,
   &input_offset, &offset_specified);

  if(error)
    return;

  // Size/offset support (2.91+)
  if(mzx_world->version < V291 && (offset_specified || size_specified))
    return;

  dest = find_string(string_list, name, &next);

  if(get_string_real_index(dest, input_offset, &dest_offset))
    return;

  // Copying to a buffer makes this slower but consistent with set.
  copy_buffer = cmalloc(dest_size);

  copy_size = load_string_board_direct(copy_buffer, dest_size,
   src_chars, src_width, block_width, block_height, terminator);

  force_string_move(string_list, name, next, &dest, copy_size,
   dest_offset, offset_specified, &dest_size, size_specified, copy_buffer);

  free(copy_buffer);
}

int set_string(struct world *mzx_world, const char *name, struct string *src,
 int id)
{
  struct string_list *string_list = &(mzx_world->string_list);
  boolean offset_specified = false;
  boolean size_specified = false;
  size_t offset = 0;
  size_t size = 0;
  int input_offset = 0;
  size_t src_length = src->length;
  char *src_value = src->value;
  struct string *dest;
  int next = 0;

  boolean error = get_string_size_offset(name, &size, &size_specified,
   &input_offset, &offset_specified);

  if(error)
    return 0;

  dest = find_string(string_list, name, &next);

  // Negative offsets (2.91+)
  if(input_offset < 0 && mzx_world->version < V291)
    return 0;

  if(get_string_real_index(dest, input_offset, &offset))
    return 0;

  if(special_name_partial("fread") &&
   !mzx_world->input_is_dir && mzx_world->input_file)
  {
    FILE *input_file = mzx_world->input_file;

    if(src_length > 5)
    {
      unsigned int read_count = strtoul(src_value + 5, NULL, 10);
      long current_pos, file_size;
      size_t actual_read;

      // You know what would be great, is not trying to allocate 4GB of memory
      read_count = MIN(read_count, MAX_STRING_LEN);

      /* This is hacky, but we don't want to prematurely allocate more space
       * to the string than can possibly be read from the file. So we save the
       * old file pointer, figure out the length of the current input file,
       * and put it back where we found it.
       */
      current_pos = ftell(input_file);
      file_size = ftell_and_rewind(input_file);
      fseek(input_file, current_pos, SEEK_SET);

      /* We then truncate the user read to the maximum difference between the
       * current position and the file end; this won't affect normal reads,
       * it just prevents people from crashing MZX by reading 2G from a 3K file.
       */
      if(current_pos + read_count > (unsigned int)file_size)
        read_count = file_size - current_pos;

      force_string_splice(string_list, name, next, &dest,
       read_count, offset, offset_specified, &size, size_specified);

      actual_read = fread(dest->value + offset, 1, read_count, input_file);
      if(offset == 0 && !offset_specified)
        dest->length = actual_read;
    }
    else
    {
      const char terminate_char = mzx_world->fread_delimiter;
      char *dest_value;
      int current_char = 0;
      unsigned int read_pos = 0;
      unsigned int read_allocate;
      unsigned int allocated = 32;
      unsigned int new_allocated = allocated;

      force_string_splice(string_list, name, next, &dest,
       allocated, offset, offset_specified, &size, size_specified);
      dest_value = dest->value;

      while(1)
      {
        for(read_allocate = 0; read_allocate < new_allocated;
         read_allocate++, read_pos++)
        {
          current_char = fgetc(input_file);

          if((current_char == terminate_char) || (current_char == EOF) ||
           (read_pos + offset == MAX_STRING_LEN))
          {
            if(offset == 0 && !offset_specified)
              dest->length = read_pos;
            return 0;
          }

          dest_value[read_pos + offset] = current_char;
        }

        new_allocated = allocated;

        if((allocated *= 2) > MAX_STRING_LEN)
          allocated = MAX_STRING_LEN;

        dest = reallocate_string(string_list, dest, next, allocated);
        dest_value = dest->value;
      }
    }
  }
  else

  if(special_name_partial("fread") && mzx_world->input_is_dir)
  {
    char entry[PATH_BUF_LEN];

    while(1)
    {
      // Read entries until there are none left
      if(!dir_get_next_entry(&mzx_world->input_directory, entry, NULL))
        break;

      // Ignore . and ..
      if(!strcmp(entry, ".") || !strcmp(entry, ".."))
        continue;

      // And ignore anything with '*' or '/' in the name
      if(strchr(entry, '*') || strchr(entry, '/'))
        continue;

      break;
    }

    force_string_copy(string_list, name, next, &dest, strlen(entry),
     offset, offset_specified, &size, size_specified, entry);
  }
  else

  if(special_name("board_name"))
  {
    char *board_name = mzx_world->current_board->board_name;
    size_t str_length = strlen(board_name);
    force_string_copy(string_list, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, board_name);
  }
  else

  if(special_name("robot_name"))
  {
    char *robot_name = (mzx_world->current_board->robot_list[id])->robot_name;
    size_t str_length = strlen(robot_name);
    force_string_copy(string_list, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, robot_name);
  }
  else

  if(special_name("mod_name"))
  {
    char *mod_name = mzx_world->real_mod_playing;
    size_t str_length = strlen(mod_name);
    force_string_copy(string_list, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, mod_name);
  }
  else

  if(special_name("input"))
  {
    char *input_string = mzx_world->current_board->input_string;
    size_t str_length = strlen(input_string);
    force_string_copy(string_list, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, input_string);
  }
  else

  // Okay, I implemented this stupid thing, you can all die.
  if(special_name("board_scan"))
  {
    struct board *src_board = mzx_world->current_board;
    unsigned int board_width, board_size, board_pos;
    size_t read_length = 63;

    board_width = src_board->board_width;
    board_size = board_width * src_board->board_height;
    board_pos = get_board_x_board_y_offset(mzx_world, id);

    force_string_length(string_list, name, next, &dest, &read_length);

    if(board_pos < board_size)
    {
      if((board_pos + read_length) > board_size)
        read_length = board_size - board_pos;

      dest->length = load_string_board_direct(dest->value,
       (int)read_length, src_board->level_param + board_pos, board_width,
       (int)read_length, 1, '*');
    }
  }
  else

  // This isn't a read (set), it's a write but it fits here now.

  if(special_name_partial("fwrite") && mzx_world->output_file)
  {
    // When no length argument is specified, always write a delimiter.
    boolean write_delimiter = (src_length == 6);

    /* You can't write a string that doesn't exist, or a string
     * of zero length (the file will still be created, of course).
     */
    if(dest != NULL && dest->length > 0)
    {
      FILE *output_file = mzx_world->output_file;
      char *dest_value = dest->value;
      size_t dest_length = dest->length;

      if(src_length > 6)
        size = strtol(src_value + 6, NULL, 10);

      if(size == 0)
        size = dest_length;

      if(offset >= dest_length)
        offset = dest_length - 1;

      if(offset + size > dest_length)
        size = dest_length - offset;

      fwrite(dest_value + offset, size, 1, output_file);
    }
    else
    {
      // 2.80X through 2.91X wouldn't write delimiters for unset strings.
      if((mzx_world->version >= V280) && (mzx_world->version <= V291) && !dest)
        write_delimiter = false;

      // 2.82b through 2.91X wouldn't write them for 0-length strings either.
      if((mzx_world->version >= V282) && (mzx_world->version <= V291))
        write_delimiter = false;
    }

    if(write_delimiter)
      fputc(mzx_world->fwrite_delimiter, mzx_world->output_file);
  }
  else

  // Load SMZX indices from a string

  if(special_name("smzx_indices"))
  {
    if(dest && dest->length > 0)
      load_indices(dest->value, dest->length);
  }
  else

  // Load source code from a string

#ifdef CONFIG_DEBYTECODE
  if(special_name_partial("load_robot") && mzx_world->version >= V290)
  {
    // Load legacy source code (2.90+)

    struct robot *cur_robot;
    int load_id = id;

    // If there's a number at the end, we're loading to another robot.
    if(src_length > 10)
      load_id = strtol(src_value + 10, NULL, 10);

    cur_robot = get_robot_by_id(mzx_world, load_id);

    if(cur_robot && dest && dest->length)
    {
      int new_length = 0;
      char *new_source;

      // Source world? Assume new source. Otherwise, assume old source.
      // TODO issues caused by this will be resolved when these counters get
      // translated into actual commands eventually.
      if(mzx_world->version >= VERSION_SOURCE)
      {
        new_length = dest->length;
        new_source = cmalloc(new_length + 1);
        memcpy(new_source, dest->value, dest->length);
        new_source[new_length] = 0;
      }
      else
        new_source = legacy_convert_program(dest->value, dest->length,
         &new_length, SAVE_ROBOT_DISASM_EXTRAS, SAVE_ROBOT_DISASM_BASE);

      if(new_source)
      {
        if(cur_robot->program_source)
          free(cur_robot->program_source);

        cur_robot->program_source = new_source;
        cur_robot->program_source_length = new_length;

        // TODO: Move this outside of here.
        if(cur_robot->program_bytecode)
        {
          free(cur_robot->program_bytecode);
          cur_robot->program_bytecode = NULL;
        }
        cur_robot->stack_pointer = 0;
        cur_robot->cur_prog_line = 1;
        prepare_robot_bytecode(mzx_world, cur_robot);

        // Restart this robot if either it was just a LOAD_ROBOT
        // OR LOAD_ROBOTn was used where n is &robot_id&.
        if(load_id == id)
          return 1;
      }
    }
  }
  else

  if(special_name_partial("save_robot") && mzx_world->version >= V292)
  {
    // Save robot source to string (2.92+)
    // FIXME old worlds will save new source. Fixing this would ideally
    // involve allowing new source, old source, or old bytecode to all be
    // considered the current program "source", but doing this in a clean
    // way probably relies on separating programs from robots internally.

    struct robot *cur_robot;
    int load_id = id;

    if(src_length > 10)
      load_id = strtol(src_value + 10, NULL, 10);

    cur_robot = get_robot_by_id(mzx_world, load_id);

    if(cur_robot)
    {
      force_string_copy(string_list, name, next, &dest,
       cur_robot->program_source_length, offset, offset_specified,
       &size, size_specified, cur_robot->program_source);
    }
  }

#else //!CONFIG_DEBYTECODE
  if(special_name_partial("load_robot") && mzx_world->version >= V290)
  {
    // Load robot from string (2.90+)

    char *new_program = NULL;
    int new_size;

    if(dest && dest->length)
      new_program = assemble_program(dest->value, dest->length, &new_size);

    if(new_program)
    {
      struct robot *cur_robot;
      int load_id = id;

      // If there's a number at the end, we're loading to another robot.
      if(src_length > 10)
        load_id = strtol(src_value + 10, NULL, 10);

      cur_robot = get_robot_by_id(mzx_world, load_id);

      if(cur_robot)
      {
        reallocate_robot(cur_robot, new_size);
        clear_label_cache(cur_robot);

        memcpy(cur_robot->program_bytecode, new_program, new_size);
        cur_robot->stack_pointer = 0;
        cur_robot->cur_prog_line = 1;
        cache_robot_labels(cur_robot);

        // Free the robot's source and command map
        free(cur_robot->program_source);
        cur_robot->program_source = NULL;
        cur_robot->program_source_length = 0;
#ifdef CONFIG_EDITOR
        free(cur_robot->command_map);
        cur_robot->command_map_length = 0;
        cur_robot->command_map = NULL;
#endif

        // Restart this robot if either it was just a LOAD_ROBOT
        // OR LOAD_ROBOTn was used where n is &robot_id&.
        if(load_id == id)
        {
          free(new_program);
          return 1;
        }
      }

      free(new_program);
    }
  }
  else

  if(special_name_partial("save_robot") && mzx_world->version >= V292)
  {
    // Save robot to string (2.92+)
    struct robot *cur_robot;
    char *robot_source = NULL;
    int robot_source_length;
    int load_id = id;

    if(src_length > 10)
      load_id = strtol(src_value + 10, NULL, 10);

    cur_robot = get_robot_by_id(mzx_world, load_id);

    if(cur_robot)
    {
      disassemble_program(cur_robot->program_bytecode,
       cur_robot->program_bytecode_length,
       &robot_source, &robot_source_length, NULL, NULL);

      if(robot_source)
      {
        force_string_copy(string_list, name, next, &dest, robot_source_length,
         offset, offset_specified, &size, size_specified, robot_source);
        free(robot_source);
      }
    }
  }
#endif

  else
  {
    // Just a normal string here.
    force_string_move(string_list, name, next, &dest, src_length,
     offset, offset_specified, &size, size_specified, src_value);
  }

  return 0;
}

// Creates a new string if it doesn't already exist; otherwise, resizes
// the string to the provided length
struct string *new_string(struct world *mzx_world, const char *name,
 size_t length, int id)
{
  struct string_list *string_list = &(mzx_world->string_list);
  struct string *str;
  int next = 0;

  str = find_string(string_list, name, &next);
  force_string_length(string_list, name, next, &str, &length);
  str->length = length;
  return str;
}

int get_string(struct world *mzx_world, const char *name, struct string *dest,
 int id)
{
  struct string_list *string_list = &(mzx_world->string_list);
  boolean error;
  boolean offset_specified = false;
  boolean size_specified = false;
  size_t size = 0;
  size_t offset = 0;
  int input_offset = 0;
  struct string *src;
  char *trimmed_name;
  int next;

  dest->length = 0;

  trimmed_name = malloc(strlen(name) + 1);
  memcpy(trimmed_name, name, strlen(name) + 1);

  error = get_string_size_offset(trimmed_name, &size, &size_specified,
   &input_offset, &offset_specified);

  if(error)
  {
    free(trimmed_name);
    return 0;
  }

  src = find_string(string_list, trimmed_name, &next);
  free(trimmed_name);

  if(src)
  {
    // Negative offsets (2.91+)
    if(input_offset < 0 && mzx_world->version < V291)
      return 0;

    if(get_string_real_index(src, input_offset, &offset))
      return 0;

    if((size == 0 && !size_specified) || size > src->length)
      size = src->length;

    if(offset > src->length)
      offset = src->length;

    if(offset + size > src->length)
      size = src->length - offset;

    dest->list_ind = src->list_ind;
    dest->value = src->value + offset;
    dest->length = size;
    return 1;
  }

  return 0;
}

// You can't increment spliced strings (it's not really useful and
// would introduce a world of problems..)

void inc_string(struct world *mzx_world, const char *name, struct string *src,
 int id)
{
  struct string_list *string_list = &(mzx_world->string_list);
  struct string *dest;
  int next;

  dest = find_string(string_list, name, &next);

  if(dest)
  {
    size_t new_length = src->length + dest->length;
    if(new_length > MAX_STRING_LEN)
      return;

    // Concatenate
    if(new_length > dest->allocated_length)
    {
      // Handle collisions, for incrementing something by a splice
      // of itself, which could relocate the string and the value...
      char *src_end = src->value + src->length;
      if((src_end <= (dest->value + dest->length)) &&
       (src_end >= dest->value))
      {
        char *old_dest_value = dest->value;
        dest = reallocate_string(string_list, dest, next, new_length);
        src->value += (dest->value - old_dest_value);
      }
      else
      {
        dest = reallocate_string(string_list, dest, next, new_length);
      }
    }

    memcpy(dest->value + dest->length, src->value, src->length);
    dest->length = new_length;
  }
  else
  {
    // Make sure this isn't a splice, malformed or otherwise.
    boolean offset_specified = false;
    boolean size_specified = false;
    size_t size;
    int offset;

    boolean error = get_string_size_offset(name, &size,
     &size_specified, &offset, &offset_specified);

    if(error || offset_specified || size_specified)
      return;

    add_string(string_list, name, src, next);
  }
}

void dec_string_int(struct world *mzx_world, const char *name, int value,
 int id)
{
  struct string_list *string_list = &(mzx_world->string_list);
  struct string *dest;
  int next;

  dest = find_string(string_list, name, &next);

  if(dest)
  {
    // Simply decrease the length
    if((int)dest->length - value < 0)
      dest->length = 0;
    else
      dest->length -= value;
  }
}

boolean is_string(char *buffer)
{
  size_t namelen, i;

  // String doesn't start with $, that's an immediate reject
  if(buffer[0] != '$')
    return false;

  // We need to stub out any part of the buffer that describes a
  // string offset or size constraint. This is because after the
  // offset or size characters, there may be an expression which
  // may use the .length operator on the same (or different)
  // string. We must not consider such composites to be invalid.
  namelen = strcspn(buffer, "#+");

  // For something to be a string it must not have a . in its name
  for(i = 0; i < namelen; i++)
    if(buffer[i] == '.')
      return false;

  // Valid string
  return true;
}

static boolean wildcard_char_is_escapable(unsigned char c)
{
  return (c == '%') || (c == '?') || (c == '\\');
}

// Slower wildcard compare algorithm that manipulates an array of booleans to
// determine the match state of the entire source string at once.
// This approach seems to perform a bit better than a stack.
//
// Example pattern:
//      z a b c d e f g
//    Y
// z  Y Y - - - - - - -
// ?  Y - Y - - - - - -
// %  Y - Y Y Y Y Y Y Y (fill left to end with Y)
// c  Y - - N Y N N N N (attempt from left to right)
// %  Y - - - Y Y Y Y Y (fill left to end with Y)
// g  Y - - - - N N N Y
static int compare_wildcard_slow(const char *str, size_t str_len,
 const char *pat, size_t pat_len, boolean exact_case)
{
  char *str_matched = ccalloc(1, str_len + 1);
  size_t left = 1;
  size_t right = 1;
  size_t new_left = 1;
  size_t new_right;
  size_t i = 0;
  size_t j;
  char next = 0;
  int res = -1;

  //info("Slow: %.*s ?=%s %.*s\n", str_len, str, exact_case?"=":"", pat_len, pat);

  str_matched[0] = 1;

  while(i < pat_len && left <= str_len)
  {
    next = exact_case ? pat[i] : memtolower(pat[i]);
    i++;

    switch(next)
    {
      case '?':
      case '%':
      {
        // ? matches any character.
        // % can match the entire string or nothing.
        // Consume all wildcards in a block to reduce the number of calls.
        boolean match_any = false;
        new_left = left;
        new_right = right;
        while(true)
        {
          if(next == '%')
          {
            new_right = str_len;
            match_any = true;
          }
          else

          if(next == '?')
          {
            new_left++;
            if(new_right < str_len)
              new_right++;
          }
          else
          {
            i--;
            break;
          }

          if(i >= pat_len)
            break;

          next = pat[i++];
        }

        if(match_any)
        {
          memset(str_matched + left, 1, str_len - left + 1);
        }
        else
          memmove(str_matched + new_left - 1, str_matched + left - 1,
           new_right - new_left + 1);

        break;
      }

      case '\\':
      {
        // Might be an escaped character
        if(i < pat_len)
        {
          if(wildcard_char_is_escapable(pat[i]))
          {
            next = pat[i];
            i++;
          }
        }
      }

      /* fallthrough */

      default:
      {
        // Matches next character if previous character matched and
        // the current character is the same as the pattern. If no matches are
        // found, left will be set >str_len and terminate the search.
        new_left = (size_t)-1;
        new_right = 0;

        if(exact_case)
        {
          for(j = right; j >= left; j--)
          {
            str_matched[j] = str_matched[j-1] && (str[j-1] == next);
            if(str_matched[j])
            {
              new_left = j+1;
              if(!new_right)
                new_right = j+(j < str_len);
            }
          }
        }
        else
        {
          for(j = right; j >= left; j--)
          {
            str_matched[j] = str_matched[j-1] && memtolower(str[j-1]) == next;
            if(str_matched[j])
            {
              new_left = j+1;
              if(!new_right)
                new_right = j+(j < str_len);
            }
          }
        }
        break;
      }
    }

    left = new_left;
    right = new_right;
  }

  // Consume any trailing multiple wildcards
  while(i < pat_len && pat[i] == '%')
    i++;

  if(str_matched[str_len] && i == pat_len)
    res = 0;

  free(str_matched);
  return res;
}

// Basic wildcard match-- supports anything but % followed by literals.
// This is a lot faster than the older algorithm, but if the aforementioned
// case is encountered, it has to call the old algorithm instead.
static int compare_wildcard(const char *str, size_t str_len,
 const char *pat, size_t pat_len, boolean exact_case)
{
  size_t s = 0;
  size_t w = 0;

  //info("Fast: %.*s ?=%s %.*s\n", str_len, str, exact_case?"=":"", pat_len, pat);

  // Pattern is length 0: match if str is length 0, otherwise not a match.
  if(pat_len == 0)
    return (str_len == 0) ? 0 : 1;

  while(s < str_len && w < pat_len)
  {
    switch(pat[w])
    {
      case '%':
      {
        // Consume extra wildcards.
        size_t oldw = w;
        size_t olds = s;
        char lookahead;
        w++;
        while(w < pat_len)
        {
          if(pat[w] == '%')
          {
            w++;
          }
          else

          if(pat[w] == '?')
          {
            w++;
            s++;
          }
          else
            break;
        }

        // Not enough source characters? Not a match.
        if(s > str_len)
          return 1;

        // End of the pattern and >=0 characters left? Match.
        if(w == pat_len)
          return 0;

        // Lookahead char not present anywhere in the source? Not a match.
        // This is a good opportunity to reduce the size of the source if it
        // has to go to the old search algorithm too.
        lookahead = pat[w];
        if(lookahead == '\\' && w+1 < pat_len)
          if(wildcard_char_is_escapable(pat[w+1]))
            lookahead = pat[w+1];

        while(s < str_len)
        {
          if(str[s] == lookahead)
            break;
          olds++;
          s++;
        }
        if(s >= str_len)
          return 1;

        // Bring out the slow search algorithm :(
        str += olds;
        str_len -= olds;
        pat += oldw;
        pat_len -= oldw;
        return compare_wildcard_slow(str, str_len, pat, pat_len, exact_case);
      }

      case '?':
      {
        // Consume extra wildcards.
        w++;
        s++;
        while(w < pat_len && pat[w] == '?')
        {
          w++;
          s++;
        }
        continue;
      }

      case '\\':
      {
        if(wildcard_char_is_escapable(pat[w+1]))
          w++;
        if(w >= pat_len)
          return 1;
      }

      /* fall-through */

      default:
      {
        if(exact_case)
        {
          if(str[s] != pat[w])
            return 1;
        }
        else
        {
          if(memtolower(str[s]) != memtolower(pat[w]))
            return 1;
        }
        w++;
        s++;
        continue;
      }
    }
  }
  // Skip trailing wildcards if they exist
  while(w < pat_len && pat[w] == '%') w++;

  // Both exactly consumed--successful match.
  if(s == str_len && w == pat_len)
    return 0;

  return 1;
}

int compare_strings(struct string *A, struct string *B, boolean exact_case,
 boolean allow_wildcards)
{
  size_t cmp_length = (A->length < B->length) ? A->length : B->length;
  int res = 0;

  if(!allow_wildcards)
  {
    if(exact_case)
    {
      res = memcmp(A->value, B->value, cmp_length);
    }
    else
    {
      res = memcasecmp(A->value, B->value, cmp_length);
    }

    // NOTE: Versions prior to 2.91e have a string ordering bug.
    // If it's necessary to emulate this bug,
    // then passing mzx_world into this function
    // and using this instead will suffice for most affected versions:
    //if(res != 0 && mzx_world->version < V291)
    if(res != 0)
      return res;

    if(A->length > B->length)
      return 1;

    if(A->length < B->length)
      return -1;

    return res;
  }

  return compare_wildcard(A->value, A->length, B->value, B->length, exact_case);
}

// Create a new string from loading a save file. This skips find_string.
struct string *load_new_string(struct string_list *string_list, int index,
 const char *name, int name_length, int str_length)
{
  struct string *dest = allocate_new_string(name, name_length, str_length);

  dest->list_ind = index;
  string_list->strings[index] = dest;

#ifdef CONFIG_KHASH
  KHASH_ADD(STRING, string_list->hash_table, dest);
#endif

  return dest;
}

static int string_sort_fcn(const void *a, const void *b)
{
  return strcasecmp(
   (*(const struct string **)a)->name,
   (*(const struct string **)b)->name);
}

void sort_string_list(struct string_list *string_list)
{
  int i;

  qsort(string_list->strings, (size_t)string_list->num_strings,
   sizeof(struct string *), string_sort_fcn);

  // IMPORTANT: Make sure we reset each string's list_ind since we just
  // sorted them, it's only polite (and also will make MZX not crash)
  for(i = 0; i < string_list->num_strings; i++)
    string_list->strings[i]->list_ind = i;
}

void clear_string_list(struct string_list *string_list)
{
  int i;

#ifdef CONFIG_KHASH
  KHASH_CLEAR(STRING, string_list->hash_table);
  string_list->hash_table = NULL;
#endif

  for(i = 0; i < string_list->num_strings; i++)
    free(string_list->strings[i]);

  free(string_list->strings);

  string_list->num_strings = 0;
  string_list->num_strings_allocated = 0;
  string_list->strings = NULL;
}
