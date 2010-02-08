/**
 * Find external resources in a MZX world. World is parsed semantically,
 * so false positives are theoretically impossible.
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// From MZX itself
#include <const.h>
#include <fsafeopen.h>

#undef DEBUG

#ifdef DEBUG
#define debug(...) fprintf(stdout, __VA_ARGS__)
#else
#define debug(...)
#endif

#define BOARD_NAME_SIZE         25

// "You may now have up to 250 boards." -- port.txt
#define MAX_BOARDS              250

#define WORLD_PROTECTED_OFFSET  BOARD_NAME_SIZE
#define WORLD_NUM_BOARDS_OFFSET 4234

#define HASH_TABLE_SIZE         100

typedef enum status
{
  SUCCESS = 0,
  INVALID_ARGUMENTS,
  CORRUPT_WORLD,
  FOPEN_FAILED,
  FREAD_FAILED,
  FSEEK_FAILED,
  MALLOC_FAILED,
  PROTECTED_WORLD,
  MAGIC_CHECK_FAILED,
}
status_t;

static struct board
{
  unsigned int size;
  unsigned int offset;
}
board_list[MAX_BOARDS];

static char **hash_table[HASH_TABLE_SIZE];

//From http://nothings.org/stb.h, by Sean
static unsigned int stb_hash(char *str)
{
   unsigned int hash = 0;
   while(*str)
      hash = (hash << 7) + (hash >> 25) + *str++;
   return hash + (hash >> 16);
}

static status_t add_to_hash_table(char *stack_str)
{
  unsigned int slot;
  int count = 0;
  size_t len;
  char *str;

  len = strlen(stack_str);
  if(!len)
    return MALLOC_FAILED;

  str = malloc(len + 1);
  if(!str)
    return MALLOC_FAILED;

  strcpy(str, stack_str);

  slot = stb_hash(stack_str) % HASH_TABLE_SIZE;

  if(!hash_table[slot])
  {
    hash_table[slot] = malloc(sizeof(char *) * 2);
    if(!hash_table[slot])
      return MALLOC_FAILED;
  }
  else
  {
    while(hash_table[slot][count])
    {
      if(!strcasecmp(stack_str, hash_table[slot][count]))
      {
        free(str);
        return SUCCESS;
      }
      count++;
    }

    hash_table[slot] = realloc(hash_table[slot], sizeof(char *) * (count + 2));
  }

  hash_table[slot][count] = str;
  hash_table[slot][count + 1] = NULL;

  return SUCCESS;
}

static unsigned int fgetud(FILE *f)
{
  return (fgetc(f) << 0)  | (fgetc(f) << 8) |
         (fgetc(f) << 16) | (fgetc(f) << 24);
}

static unsigned short fgetus(FILE *f)
{
  return (fgetc(f) << 0) | (fgetc(f) << 8);
}

static const char *decode_status(status_t status)
{
  switch(status)
  {
    case CORRUPT_WORLD:
      return "Random world corruption.";
    case FOPEN_FAILED:
      return "Could not open file.";
    case FREAD_FAILED:
      return "File read failed or was short (corrupt file?).";
    case FSEEK_FAILED:
      return "Seeking in file failed (too short?).";
    case MALLOC_FAILED:
      return "Memory allocation failed or was zero.";
    case PROTECTED_WORLD:
      return "Protected worlds currently unsupported.";
    case MAGIC_CHECK_FAILED:
      return "File magic not consistent with 2.00 world or board.";
    default:
      return "Unknown error.";
  }
}

static status_t parse_board_direct(FILE *f)
{
  int i, j, num_robots, skip_rle_blocks = 6;
  char tmp[256], tmp2[256], *str;
  status_t ret;

  // junk the undocument (and unused) board_mode
  fgetc(f);

  // get whether the overlay is enabled or not
  if(fgetc(f))
  {
    // not enabled, so rewind this last read
    if(fseek(f, -1, SEEK_CUR) != 0)
      return FSEEK_FAILED;
  }
  else
  {
    // junk overlay_mode
    fgetc(f);
    skip_rle_blocks += 2;
  }

  // this skips either 6 blocks (with no overlay)
  // ..or 8 blocks (with overlay enabled on board)
  for(i = 0; i < skip_rle_blocks; i++)
  {
    unsigned short w = fgetus(f);
    unsigned short h = fgetus(f);
    int pos = 0;

    /* RLE "decoder"; just to skip stuff */
    while(pos < w * h)
    {
      unsigned char c = (unsigned char)fgetc(f);

      if(!(c & 0x80))
        pos++;
      else
      {
        c &= ~0x80;
        pos += c;
        fgetc(f);
      }
    }
  }

  // grab board's default MOD
  if(fread(tmp, 1, 12, f) != 12)
    return FREAD_FAILED;
  tmp[12] = '\0';

  // check the board MOD exists
  if(strlen(tmp) > 0)
  {
    debug("BOARD MOD: %s\n", tmp);
    ret = add_to_hash_table(tmp);
    if (ret != SUCCESS)
      return ret;
  }

  // skip to the robot count
  if(fseek(f, 220 - 12, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  // walk the robot list, scan the robotic
  num_robots = fgetc(f);
  for(i = 0; i < num_robots; i++)
  {
    unsigned short robot_size = fgetus(f);
    long start;

    // skip to robot code
    if(fseek(f, 40 - 2 + 1, SEEK_CUR) != 0)
      return FSEEK_FAILED;

    start = ftell(f);

    // skip 0xff marker
    if(fgetc(f) != 0xff)
      return CORRUPT_WORLD;

    while(1)
    {
      int command, command_length, str_len;

      if(ftell(f) - start >= robot_size)
        return CORRUPT_WORLD;

      command_length = fgetc(f);
      if(command_length == 0)
        break;

      command = fgetc(f);
      switch(command)
      {
        case 0xa:
          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          str_len = fgetc(f);

          if(str_len == 0)
          {
            fgetus(f);
            break;
          }

          if(fread(tmp2, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          if(!strcasecmp(tmp2, "FREAD_OPEN")
            || !strcasecmp(tmp2, "SMZX_PALETTE")
            || !strcasecmp(tmp2, "LOAD_GAME")
            || !strcasecmp(tmp2, "LOAD_BC")
            || !strcasecmp(tmp2, "LOAD_ROBOT")
            || !strncasecmp(tmp2, "LOAD_BC", 7)
            || !strncasecmp(tmp2, "LOAD_ROBOT", 10))
          {
            debug("SET: %s (%s)\n", tmp, tmp2);
            ret = add_to_hash_table(tmp);
            if (ret != SUCCESS)
              return ret;
          }
          break;

        case 0x26:
          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          // FIXME: Should only match pairs?
          if(strstr(tmp, "&"))
              break;

          debug("MOD: %s\n", tmp);
          ret = add_to_hash_table(tmp);
          if (ret != SUCCESS)
            return ret;
          break;

        case 0x27:
          str_len = fgetc(f);

          if(str_len == 0)
            fgetus(f);
          else
          {
            for(j = 0; j < str_len; j++)
              fgetc(f);
          }

          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          // FIXME: Should only match pairs?
          if(strstr(tmp, "&"))
              break;

          debug("SAM: %s\n", tmp);
          ret = add_to_hash_table(tmp);
          if (ret != SUCCESS)
            return ret;
          break;

        case 0x2b:
        case 0x2d:
        case 0x31:
          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          str = strtok(tmp, "&");
          while(str)
          {
            debug("PLAY (class): %s\n", str);
            ret = add_to_hash_table(str);
            if (ret != SUCCESS)
              return ret;

            // tokenise twice, because we only care about
            // every _other '&'; e.g. &sam.sam&ff&sam2.sam&ee
            str = strtok(NULL, "&");
            str = strtok(NULL, "&");
          }
          break;

        case 0xc8:
          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          debug("MOD FADE IN: %s\n", tmp);
          ret = add_to_hash_table(tmp);
          if (ret != SUCCESS)
            return ret;
          break;

        case 0xd8:
          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          debug("LOAD CHAR SET: %s\n", tmp);
          ret = add_to_hash_table(tmp);
          if (ret != SUCCESS)
            return ret;
          break;

        case 0xde:
          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          debug("LOAD PALETTE: %s\n", tmp);
          ret = add_to_hash_table(tmp);
          if (ret != SUCCESS)
            return ret;
          break;

        case 0xe2:
          str_len = fgetc(f);

          if(fread(tmp, 1, str_len, f) != (size_t)str_len)
            return FREAD_FAILED;

          debug("SWAP WORLD: %s\n", tmp);
          ret = add_to_hash_table(tmp);
          if (ret != SUCCESS)
            return ret;
          break;

        default:
          if(fseek(f, command_length - 1, SEEK_CUR) != 0)
            return FSEEK_FAILED;
          break;
      }

      if(fgetc(f) != command_length)
        return CORRUPT_WORLD;
    }
  }

  return SUCCESS;
}

// same as internal boards except for a 4 byte magic header

static status_t parse_board(FILE *f)
{
  int c;

  if(fgetc(f) != 0xff)
    return MAGIC_CHECK_FAILED;

  if(fgetc(f) != 'M')
    return MAGIC_CHECK_FAILED;

  c = fgetc(f);
  if (c != 'B' && c != '\x2')
    return MAGIC_CHECK_FAILED;

  fgetc(f);

  return parse_board_direct(f);
}

static status_t parse_world(FILE *f)
{
  status_t ret = SUCCESS;
  int i, c, num_boards;

  // skip to protected byte; don't care about world name
  if(fseek(f, WORLD_PROTECTED_OFFSET, SEEK_SET) != 0)
    return FSEEK_FAILED;

  // can't yet support protected worlds
  if(fgetc(f) != 0)
    return PROTECTED_WORLD;

  // can only support 2.00+ versioned worlds
  if(fgetc(f) != 'M')
    return MAGIC_CHECK_FAILED;

  c = fgetc(f);
  if(c != '\x2' && c != 'Z')
    return MAGIC_CHECK_FAILED;

  // num_boards doubles as the check for custom sfx, if zero
  if(fseek(f, WORLD_NUM_BOARDS_OFFSET, SEEK_SET) != 0)
    return FSEEK_FAILED;

  // so read it, and..
  num_boards = fgetc(f);

  // if we have sfx, they must be skipped
  if(num_boards == 0)
  {
    unsigned short sfx_len = fgetus(f);

    if(fseek(f, sfx_len, SEEK_CUR) != 0)
      return FSEEK_FAILED;

    // read the "real" num_boards
    num_boards = fgetc(f);
  }

  // skip board names; we simply don't care
  if(fseek(f, num_boards * BOARD_NAME_SIZE, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  // grab the board sizes/offsets
  for(i = 0; i < num_boards; i++)
  {
    board_list[i].size = fgetud(f);
    board_list[i].offset = fgetud(f);
  }

  // walk all boards
  for(i = 0; i < num_boards; i++)
  {
    struct board *board = &board_list[i];

    // don't care about deleted boards
    if(board->size == 0)
      continue;

    // seek to board offset within world
    if(fseek(f, board->offset, SEEK_SET) != 0)
      return FSEEK_FAILED;

    // parse this board atomically
    ret = parse_board_direct(f);
  }

  return ret;
}

int main(int argc, char *argv[])
{
  const char *found_append = " - FOUND", *not_found_append = " - NOT FOUND";
  int i, len, print_all_files = 0, got_world = 0;
  status_t ret;
  FILE *f;

  if(argc < 2)
  {
    fprintf(stderr, "usage: %s [-q] [-a] [mzx file]\n\n", argv[0]);
    fprintf(stderr, "  -q  Do not print summary \"FOUND\"/\"NOT FOUND\".\n");
    fprintf(stderr, "  -a  Print found files as well as missing files.\n");
    fprintf(stderr, "\n");
    return INVALID_ARGUMENTS;
  }

  if((argc > 2 && !strcmp(argv[1], "-q"))
   ||(argc > 3 && !strcmp(argv[2], "-q")))
  {
    found_append = "";
    not_found_append = "";
  }

  if((argc > 2 && !strcmp(argv[1], "-a"))
   ||(argc > 3 && !strcmp(argv[2], "-a")))
    print_all_files = 1;

  len = strlen(argv[argc - 1]);
  if(len < 4)
  {
    fprintf(stderr, "\"%s\" is not a valid input filename.\n", argv[argc - 1]);
    return INVALID_ARGUMENTS;
  }

  if(!strcasecmp(&argv[argc - 1][len - 4], ".MZX"))
    got_world = 1;
  else if(!strcasecmp(&argv[argc - 1][len - 4], ".MZB"))
    got_world = 0;
  else
  {
    fprintf(stderr, "\"%s\" is not a .MZX (world) or .MZB (board) file.\n",
      argv[argc - 1]);
    return INVALID_ARGUMENTS;
  }

  f = fopen(argv[argc - 1], "r");
  if(f)
  {
    if (got_world)
      ret = parse_world(f);
    else
      ret = parse_board(f);

    fclose(f);

    for(i = 0; i < HASH_TABLE_SIZE; i++)
    {
      char **p = hash_table[i];

      if(!p)
        continue;

      while(*p)
      {
        if(ret == SUCCESS)
        {
          char newpath[MAX_PATH];

          if(fsafetranslate(*p, newpath) == FSAFE_SUCCESS)
          {
            if(print_all_files)
              fprintf(stdout, "%s%s\n", newpath, found_append);
          }
          else
            fprintf(stdout, "%s%s\n", *p, not_found_append);
        }

        free(*p);
        p++;
      }

      free(hash_table[i]);
    }
  }
  else
    ret = FOPEN_FAILED;

  if(ret != SUCCESS)
    fprintf(stderr, "ERROR: %s\n", decode_status(ret));

  return ret;
}
