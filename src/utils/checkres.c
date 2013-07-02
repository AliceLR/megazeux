/**
 * Find external resources in a MZX world. World is parsed semantically,
 * so false positives are theoretically impossible.
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Josh Matthews <josh@joshmatthews.net>
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
#include <unistd.h>

#ifdef __WIN32__
#include <strings.h>
#endif

// From MZX itself
#define SKIP_SDL
#include "../fsafeopen.h"
#include "../const.h"
#include "../util.h"

#include "unzip.h"

#define BOARD_NAME_SIZE 25

// "You may now have up to 250 boards." -- port.txt
#define MAX_BOARDS 250

// From sfx.h
#define NUM_SFX 50

#define WORLD_PROTECTED_OFFSET      BOARD_NAME_SIZE
#define WORLD_GLOBAL_OFFSET_OFFSET  4230

#define HASH_TABLE_SIZE 100

#define EOS EOF

enum stream_type
{
  FILE_STREAM,
  BUFFER_STREAM
};

struct stream
{
  enum stream_type type;

  union
  {
    FILE *fp;
    struct
    {
      unsigned long int index;
      unsigned char *buf;
      unsigned long int len;
      unzFile f;
    } buffer;
  } stream;
};

enum status
{
  SUCCESS = 0,
  INVALID_ARGUMENTS,
  CORRUPT_WORLD,
  FOPEN_FAILED,
  GET_PATH_FAILED,
  CHDIR_FAILED,
  FREAD_FAILED,
  FSEEK_FAILED,
  MALLOC_FAILED,
  PROTECTED_WORLD,
  MAGIC_CHECK_FAILED,
  UNZ_FAILED,
  NO_WORLD,
  MISSING_FILE
};

static struct board
{
  unsigned int size;
  unsigned int offset;
}
board_list[MAX_BOARDS];

// FIXME: Fix this better
int error(const char *string, unsigned int type, unsigned int options,
 unsigned int code)
{
  return 0;
}

static char **hash_table[HASH_TABLE_SIZE];

//From http://nothings.org/stb.h, by Sean
static unsigned int stb_hash(char *str)
{
  unsigned int hash = 0;
  while(*str)
    hash = (hash << 7) + (hash >> 25) + *str++;
  return hash + (hash >> 16);
}

static enum status add_to_hash_table(char *stack_str)
{
  unsigned int slot;
  int count = 0;
  size_t len;
  char *str;

  len = strlen(stack_str);
  if(!len)
    return SUCCESS;

  str = malloc(len + 1);
  if(!str)
    return MALLOC_FAILED;

  strncpy(str, stack_str, len);
  str[len] = '\0';

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

static bool is_zip_file(const char *filename)
{
  int signature = 0;
  FILE *fp;

  fp = fopen_unsafe(filename, "rb");
  if(!fp)
    return false;

  signature |= fgetc(fp) << 0;
  signature |= fgetc(fp) << 8;
  signature |= fgetc(fp) << 16;
  signature |= fgetc(fp) << 24;
  fclose(fp);

  // Zip file header obtained from:
  //   http://www.pkware.com/documents/casestudies/APPNOTE.TXT
  return (strcasecmp(filename + strlen(filename) - 3, "zip") == 0 ||
          signature == 0x04034b50);
}

static enum status s_open(const char *filename, const char *mode,
 struct stream **s)
{
  unz_file_info info;
  enum status ret;
  unzFile f;

  *s = malloc(sizeof(struct stream));

  if(!is_zip_file(filename))
  {
    char path[MAX_PATH];
    int path_len;

    /* Move into the world's directory first; this lets us look up
     * resources relative to the world.
     */
    path_len = __get_path(filename, path, MAX_PATH);

    if(path_len < 0)
    {
      ret = GET_PATH_FAILED;
      goto exit_free_stream;
    }

    if(path_len > 0)
    {
      if(chdir(path))
      {
        ret = CHDIR_FAILED;
        goto exit_free_stream;
      }

      filename += path_len + 1;
    }

    (*s)->type = FILE_STREAM;
    (*s)->stream.fp = fopen_unsafe(filename, mode);
    if(!(*s)->stream.fp)
    {
      ret = FOPEN_FAILED;
      goto exit_free_stream;
    }
    return SUCCESS;
  }

  // otherwise, it's a ZIP file, proceed with buffer logic
  (*s)->type = BUFFER_STREAM;
  f = unzOpen(filename);
  if(!f)
  {
    ret = MALLOC_FAILED;
    goto exit_free_stream;
  }

  if(unzGoToFirstFile(f) != UNZ_OK)
  {
    ret = UNZ_FAILED;
    goto exit_free_stream_close;
  }

  while(1)
  {
    char fname[FILENAME_MAX];
    int unzRet;

    // get the filename for the current ZIP file entry
    unzGetCurrentFileInfo(f, &info, fname, FILENAME_MAX, NULL, 0, NULL, 0);

    // not a MZX file; don't care
    if(!strcasecmp(fname + strlen(fname) - 3, "mzx"))
      break;

    // error occurred, or no files left
    unzRet = unzGoToNextFile(f);
    if(unzRet != UNZ_OK)
    {
      if(unzRet == UNZ_END_OF_LIST_OF_FILE)
        ret = NO_WORLD;
      else
        ret = UNZ_FAILED;

      goto exit_free_stream_close;
    }
  }

  if(unzOpenCurrentFile(f) != UNZ_OK)
  {
    ret = UNZ_FAILED;
    goto exit_free_stream_close;
  }

  (*s)->stream.buffer.len = info.uncompressed_size;
  (*s)->stream.buffer.buf = malloc((*s)->stream.buffer.len);
  if(!(*s)->stream.buffer.buf)
  {
    ret = MALLOC_FAILED;
    goto exit_free_stream_close_both;
  }

  if(unzReadCurrentFile(f, (*s)->stream.buffer.buf,
                           (*s)->stream.buffer.len) <= 0)
  {
    ret = UNZ_FAILED;
    goto exit_free_stream_close_both_free_buffer;
  }

  (*s)->stream.buffer.f = f;
  unzCloseCurrentFile(f);
  return SUCCESS;

exit_free_stream_close_both_free_buffer:
  free((*s)->stream.buffer.buf);
exit_free_stream_close_both:
  unzCloseCurrentFile(f);
exit_free_stream_close:
  unzClose(f);
exit_free_stream:
  free(*s);
  *s = NULL;
  return ret;
}

static int sclose(struct stream *s)
{
  FILE *f;

  switch(s->type)
  {
    case FILE_STREAM:
      f = s->stream.fp;
      free(s);
      return fclose(f);

    case BUFFER_STREAM:
      unzClose(s->stream.buffer.f);
      free(s->stream.buffer.buf);
      free(s);
      return 0;

    default:
      return EOS;
  }
}

static int sgetc(struct stream *s)
{
  switch(s->type)
  {
    case FILE_STREAM:
      return fgetc(s->stream.fp);

    case BUFFER_STREAM:
      if(s->stream.buffer.index == s->stream.buffer.len)
        return EOS;
      else
        return s->stream.buffer.buf[s->stream.buffer.index++];

    default:
      return EOS;
  }
}

static size_t sread(void *ptr, size_t size, size_t count, struct stream *s)
{
  switch(s->type)
  {
    case FILE_STREAM:
      return fread(ptr, size, count, s->stream.fp);

    case BUFFER_STREAM:
      if(s->stream.buffer.index + size * count >= s->stream.buffer.len)
      {
        size_t n = (s->stream.buffer.len - s->stream.buffer.index) / size;
        memcpy(ptr, s->stream.buffer.buf + s->stream.buffer.index, n * size);
        s->stream.buffer.index = s->stream.buffer.len;
        return n;
      }
      else
      {
        memcpy(ptr, s->stream.buffer.buf + s->stream.buffer.index, count * size);
        s->stream.buffer.index += count * size;
        return count;
      }

    default:
      return EOS;
  }
}

static int sseek(struct stream *s, long int offset, int origin)
{
  switch(s->type)
  {
    case FILE_STREAM:
      return fseek(s->stream.fp, offset, origin);

    case BUFFER_STREAM:
      switch(origin)
      {
        case SEEK_SET:
          s->stream.buffer.index = offset;
          break;
        case SEEK_CUR:
          s->stream.buffer.index += offset;
          break;
        case SEEK_END:
          s->stream.buffer.index = s->stream.buffer.len + offset;
          break;
      }

      if(s->stream.buffer.index >= s->stream.buffer.len)
        return EOS;
      return 0;

    default:
      return EOS;
  }
}

static long int stell(struct stream *s)
{
  switch(s->type)
  {
    case FILE_STREAM:
      return ftell(s->stream.fp);
    case BUFFER_STREAM:
      return s->stream.buffer.index;
    default:
      return EOS;
  }
}

static unsigned int sgetud(struct stream *s)
{
  return (sgetc(s) << 0)  | (sgetc(s) << 8) |
         (sgetc(s) << 16) | (sgetc(s) << 24);
}

static unsigned short sgetus(struct stream *s)
{
  return (sgetc(s) << 0) | (sgetc(s) << 8);
}

static int world_magic(const unsigned char magic_string[3])
{
  if(magic_string[0] == 'M')
  {
    if(magic_string[1] == 'Z')
    {
      switch(magic_string[2])
      {
        case '2':
          return 0x0205;
        case 'A':
          return 0x0208;
      }
    }
    else
    {
      if((magic_string[1] > 1) && (magic_string[1] < 10))
        return ((int)magic_string[1] << 8) + (int)magic_string[2];
    }
  }

  return 0;
}

static int board_magic(const unsigned char magic_string[4])
{
  if(magic_string[0] == 0xFF)
  {
    if(magic_string[1] == 'M')
    {
      if(magic_string[2] == 'B' && magic_string[3] == '2')
        return 0x0200;

      if((magic_string[2] > 1) && (magic_string[2] < 10))
        return ((int)magic_string[2] << 8) + (int)magic_string[3];
    }
  }

  return 0;
}

static const char *decode_status(enum status status)
{
  switch(status)
  {
    case CORRUPT_WORLD:
      return "Random world corruption.";
    case FOPEN_FAILED:
      return "Could not open file.";
    case GET_PATH_FAILED:
      return "Failed to obtain path from filename.";
    case CHDIR_FAILED:
      return "Failed to chdir() into parent path.";
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
    case UNZ_FAILED:
      return "Something is wrong with the zip file.";
    case NO_WORLD:
      return "No world file present.";
    default:
      return "Unknown error.";
  }
}

static enum status parse_sfx(char *sfx_buf)
{
  char *start, *end = sfx_buf - 1, str_buf_len;
  enum status ret = SUCCESS;

  str_buf_len = strlen(sfx_buf);

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
    *start = 0;
    *end = 0;

    debug("SFX (class): %s\n", start + 1);
    ret = add_to_hash_table(start + 1);
    if(ret != SUCCESS)
      break;
  }

  return ret;
}

static enum status parse_robot(struct stream *s)
{
  unsigned short robot_size;
  enum status ret = SUCCESS;
  char tmp[256], tmp2[256];
  long start;
  int i;

  // robot's size
  robot_size = sgetus(s);

  // skip to robot code
  if(sseek(s, 40 - 2 + 1, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  start = stell(s);

  // skip 0xff marker
  if(sgetc(s) != 0xff)
    return CORRUPT_WORLD;

  while(1)
  {
    int command, command_length, str_len;

    if(stell(s) - start >= robot_size)
      return CORRUPT_WORLD;

    command_length = sgetc(s);
    if(command_length == 0)
      break;

    command = sgetc(s);
    switch(command)
    {
      case 0xa:
        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        str_len = sgetc(s);

        if(str_len == 0)
        {
          sgetus(s);
          break;
        }

        if(sread(tmp2, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        if(!strcasecmp(tmp2, "FREAD_OPEN")
          || !strcasecmp(tmp2, "SMZX_PALETTE")
          || !strcasecmp(tmp2, "LOAD_GAME")
          || !strncasecmp(tmp2, "LOAD_BC", 7)
          || !strncasecmp(tmp2, "LOAD_ROBOT", 10))
        {
          debug("SET: %s (%s)\n", tmp, tmp2);
          ret = add_to_hash_table(tmp);
          if(ret != SUCCESS)
            return ret;
        }
        break;

      case 0x26:
        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        // ignore MOD *
        if(!strcmp(tmp, "*"))
          break;

        // FIXME: Should only match pairs?
        if(strstr(tmp, "&"))
          break;

        debug("MOD: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0x27:
        str_len = sgetc(s);

        if(str_len == 0)
          sgetus(s);
        else
        {
          for(i = 0; i < str_len; i++)
            sgetc(s);
        }

        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        // FIXME: Should only match pairs?
        if(strstr(tmp, "&"))
            break;

        debug("SAM: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0x2b:
      case 0x2d:
      case 0x31:
        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        ret = parse_sfx(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0xc8:
        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        debug("MOD FADE IN: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0xd8:
        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        debug("LOAD CHAR SET: %s\n", tmp);

        if(tmp[0] == '+')
        {
          char *rest, tempc = tmp[3];
          tmp[3] = 0;
          strtol(tmp + 1, &rest, 16);
          tmp[3] = tempc;
          memmove(tmp, rest, str_len - (rest - tmp));
        }
        else if(tmp[0] == '@')
        {
          char *rest, tempc = tmp[4];
          tmp[4] = 0;
          strtol(tmp + 1, &rest, 10);
          tmp[4] = tempc;
          memmove(tmp, rest, str_len - (rest - tmp));
        }

        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0xde:
        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        debug("LOAD PALETTE: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0xe2:
        str_len = sgetc(s);

        if(sread(tmp, 1, str_len, s) != (size_t)str_len)
          return FREAD_FAILED;

        debug("SWAP WORLD: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      default:
        if(sseek(s, command_length - 1, SEEK_CUR) != 0)
          return FSEEK_FAILED;
        break;
    }

    if(sgetc(s) != command_length)
      return CORRUPT_WORLD;
  }

  return ret;
}

static enum status parse_board_direct(struct stream *s, int version)
{
  int i, num_robots, skip_rle_blocks = 6, skip_bytes;
  unsigned short board_mod_len;
  enum status ret = SUCCESS;
  char tmp[MAX_PATH];

  // junk the undocumented (and unused) board_mode
  sgetc(s);

  // whether the overlay is enabled or not
  if(sgetc(s))
  {
    // not enabled, so rewind this last read
    if(sseek(s, -1, SEEK_CUR) != 0)
      return FSEEK_FAILED;
  }
  else
  {
    // junk overlay_mode
    sgetc(s);
    skip_rle_blocks += 2;
  }

  // this skips either 6 blocks (with no overlay)
  // ..or 8 blocks (with overlay enabled on board)
  for(i = 0; i < skip_rle_blocks; i++)
  {
    unsigned short w = sgetus(s);
    unsigned short h = sgetus(s);
    int pos = 0;

    /* RLE "decoder"; just to skip stuff */
    while(pos < w * h)
    {
      unsigned char c = (unsigned char)sgetc(s);

      if(!(c & 0x80))
        pos++;
      else
      {
        c &= ~0x80;
        pos += c;
        sgetc(s);
      }
    }
  }

  // get length of board MOD string
  if(version < 0x0253)
    board_mod_len = 12;
  else
    board_mod_len = sgetus(s);

  // grab board's default MOD
  if(sread(tmp, 1, board_mod_len, s) != board_mod_len)
      return FREAD_FAILED;
  tmp[board_mod_len] = '\0';

  // check the board MOD exists
  if(strlen(tmp) > 0 && strcmp(tmp, "*"))
  {
    debug("BOARD MOD: %s\n", tmp);
    ret = add_to_hash_table(tmp);
    if(ret != SUCCESS)
      return ret;
  }

  if(version < 0x0253)
    skip_bytes = 208;
  else
    skip_bytes = 25;

  // skip to the robot count
  if(sseek(s, skip_bytes, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  // walk the robot list, scan the robotic
  num_robots = sgetc(s);
  for(i = 0; i < num_robots; i++)
  {
    ret = parse_robot(s);
    if(ret != SUCCESS)
      break;
  }

  return ret;
}

// same as internal boards except for a 4 byte magic header

static enum status parse_board(struct stream *s)
{
  unsigned char magic[4];
  int version;

  if(sread(magic, 1, 4, s) != (size_t)4)
    return FREAD_FAILED;

  version = board_magic(magic);
  if(version <= 0)
    return MAGIC_CHECK_FAILED;

  return parse_board_direct(s, version);
}

static enum status parse_world(struct stream *s)
{
  int version, i, num_boards, global_robot_offset;
  enum status ret = SUCCESS;
  unsigned char magic[3];

  // skip to protected byte; don't care about world name
  if(sseek(s, WORLD_PROTECTED_OFFSET, SEEK_SET) != 0)
    return FSEEK_FAILED;

  // can't yet support protected worlds
  if(sgetc(s) != 0)
    return PROTECTED_WORLD;

  // read in world magic (version)
  if(sread(magic, 1, 3, s) != (size_t)3)
    return FREAD_FAILED;

  // can only support 2.00+ versioned worlds
  version = world_magic(magic);
  if(version <= 0)
    return MAGIC_CHECK_FAILED;

  // Jump to the global robot offset
  if(sseek(s, WORLD_GLOBAL_OFFSET_OFFSET, SEEK_SET) != 0)
    return FSEEK_FAILED;

  // Absolute offset (in bytes) of global robot
  global_robot_offset = sgetud(s);

  // num_boards doubles as the check for custom sfx, if zero
  num_boards = sgetc(s);

  // if we have custom sfx, check them for resources
  if(num_boards == 0)
  {
    unsigned short sfx_len_total = sgetus(s);

    for(i = 0; i < NUM_SFX; i++)
    {
      unsigned short sfx_len;
      char sfx_buf[70];

      sfx_len = sgetc(s);
      if(sfx_len > 69)
        return CORRUPT_WORLD;

      if(sfx_len > 0)
      {
        if(sread(sfx_buf, 1, sfx_len, s) != (size_t)sfx_len)
          return FREAD_FAILED;
        sfx_buf[sfx_len] = 0;

        ret = parse_sfx(sfx_buf);
        if(ret != SUCCESS)
          return ret;
      }

      // 1 for length byte + sfx string
      sfx_len_total -= 1 + sfx_len;
    }

    // better check we moved by whole amount
    if(sfx_len_total != 0)
      return CORRUPT_WORLD;

    // read the "real" num_boards
    num_boards = sgetc(s);
  }

  // skip board names; we simply don't care
  if(sseek(s, num_boards * BOARD_NAME_SIZE, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  // grab the board sizes/offsets
  for(i = 0; i < num_boards; i++)
  {
    board_list[i].size = sgetud(s);
    board_list[i].offset = sgetud(s);
  }

  // walk all boards
  for(i = 0; i < num_boards; i++)
  {
    struct board *board = &board_list[i];

    // don't care about deleted boards
    if(board->size == 0)
      continue;

    // seek to board offset within world
    if(sseek(s, board->offset, SEEK_SET) != 0)
      return FSEEK_FAILED;

    // parse this board atomically
    ret = parse_board_direct(s, version);
    if(ret != SUCCESS)
      goto err_out;
  }

  // Do the global robot too..
  if(sseek(s, global_robot_offset, SEEK_SET) != 0)
    return FSEEK_FAILED;
  ret = parse_robot(s);

err_out:
  return ret;
}

static enum status file_exists(const char *file, struct stream *s)
{
  char newpath[MAX_PATH];

  switch(s->type)
  {
    case FILE_STREAM:
      if(fsafetranslate(file, newpath) == FSAFE_SUCCESS)
        return SUCCESS;
      return MISSING_FILE;

    case BUFFER_STREAM:
      if(unzLocateFile(s->stream.buffer.f, file, 2) == UNZ_OK)
        return SUCCESS;
      return MISSING_FILE;

    default:
      return MISSING_FILE;
  }
}

int main(int argc, char *argv[])
{
  const char *found_append = " - FOUND", *not_found_append = " - NOT FOUND";
  int i, len, print_all_files = 0, got_world = 0, quiet_mode = 0;
  struct stream *s;
  enum status ret;

  if(argc < 2)
  {
    fprintf(stderr, "usage: %s [-q] [-a] [mzx/mzb/zip file]\n\n", argv[0]);
    fprintf(stderr, "  -q  Do not print summary \"FOUND\"/\"NOT FOUND\".\n");
    fprintf(stderr, "  -a  Print found files as well as missing files.\n");
    fprintf(stderr, "\n");
    return INVALID_ARGUMENTS;
  }

  for(i = 1; i < argc - 1; i++)
  {
    if(!strcmp(argv[i], "-q"))
    {
      quiet_mode = 1;
      found_append = "";
      not_found_append = "";
    }
    else if(!strcmp(argv[i], "-a"))
      print_all_files = 1;
  }

  len = strlen(argv[argc - 1]);
  if(len < 4)
  {
    fprintf(stderr, "'%s' is not a valid input filename.\n", argv[argc - 1]);
    return INVALID_ARGUMENTS;
  }

  if(!strcasecmp(&argv[argc - 1][len - 4], ".MZX"))
    got_world = 1;
  else if(!strcasecmp(&argv[argc - 1][len - 4], ".MZB"))
    got_world = 0;
  else if(!strcasecmp(&argv[argc -1][len -4], ".ZIP"))
    got_world = 1;
  else
  {
    fprintf(stderr, "'%s' is not a .MZX (world), .MZB (board) "
                    "or .ZIP (archive) file.\n", argv[argc - 1]);
    return INVALID_ARGUMENTS;
  }

  ret = s_open(argv[argc - 1], "rb", &s);
  if(s)
  {
    if(got_world)
      ret = parse_world(s);
    else
      ret = parse_board(s);

    for(i = 0; i < HASH_TABLE_SIZE; i++)
    {
      char **p = hash_table[i];

      if(!p)
        continue;

      while(*p)
      {
        if(ret == SUCCESS)
        {
          if(file_exists(*p, s) == SUCCESS)
          {
            if(print_all_files)
              fprintf(stdout, "%s%s\n", *p, found_append);
          }
          else
            fprintf(stdout, "%s%s\n", *p, not_found_append);
        }

        free(*p);
        p++;
      }

      free(hash_table[i]);
    }

    sclose(s);
  }

  if(ret != SUCCESS)
    fprintf(stderr, "ERROR: %s\n", decode_status(ret));
  else
  {
    if(!quiet_mode)
      fprintf(stdout, "Finished processing '%s'.\n", argv[argc - 1]);
  }

  return ret;
}

