/**
 * Find external resources in a MZX world. World is parsed semantically,
 * so false positives are theoretically impossible.
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Josh Matthews <josh@joshmatthews.net>
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

#include "../config.h"

#define USAGE \
 "checkres :: MegaZeux " VERSION VERSION_DATE "\n" \
 "Usage: checkres [options] " \
 "mzx/mzb/dir/zip file [-extra path/zip file [-in relative path] ...] ... \n" \
 "\n" \
 "  checkres scans a MZX, MZB, or ZIP file (or a directory) for any file\n"   \
 "  dependencies MegaZeux might encounter. This is useful to check e.g. if\n" \
 "  all files are present before releasing a game. Each required or present\n"\
 "  file may have one of the following statuses:\n\n"                         \
 "    FOUND:     the file is required and was found;\n"                       \
 "    NOT FOUND: the file is required and was not found;\n"                   \
 "    CREATED:   the file is required and wasn't found, but may be created;\n"\
 "    CREATED*:  as above, except more tenuous as this file may be created\n" \
 "               only depending on the result of expressions/interpolation;\n"\
 "    PATTERN*:  the file is specified as part of a command that can only\n"  \
 "               be evaluated while MegaZeux is running, but potential\n"     \
 "               matches can be inferred contextually using wildcards. This\n"\
 "               includes things like DOS filenames (xxxxxx~1.ext).\n"        \
 "    MATCH*:    files potentially matched by a wildcard pattern;\n"          \
 "    UNUSED:    the file is present but is not used in any MZX/MZB file.\n"  \
 "\nGeneral options:\n" \
 " -h   Display this message and exit.\n"                                   \
 "\nStatus options:\n" \
 " -a   Display all found, created, missing, and unused files.\n"           \
 " -C   Only display created files.\n"                                      \
 " -F   Only display found files.\n"                                        \
 " -M   Only display missing files (default).\n"                            \
 " -U   Only display unused files.\n"                                       \
 " -W   Only display wildcard-matched files.\n"                             \
 " -c   Also display created files.\n"                                      \
 " -f   Also display found files.\n"                                        \
 " -m   Also display missing files.\n"                                      \
 " -u   Also display unused files.\n"                                       \
 " -w   Also display wildcard-matched files.\n"                             \
 "\nDetail options:\n" \
 " -s   Summary: unique filenames, status, source, world file (default).\n" \
 " -v   Display all references with sfx#, board#, robot#.\n"                \
 " -vv  Display all references with sfx#, board#, robot#, line#, coords.\n" \
 " -1   Display unique filenames, one per line, with no other info.\n"      \
 "\nSorting options:\n" \
 " -N   Sort by referenced filename, then by location (default).\n"         \
 " -L   Sort by location of reference: world, board#, robot#.\n"            \
 "\n"

#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __WIN32__
#include <strings.h>
#endif

// Defines so checkres builds when this is included.
// This is because khashmzx.h uses the check_alloc functions (CORE_LIBSPEC)
// and memcasecmp.h (which needs platform_endian.h and thus SDL_endian.h).
#define SKIP_SDL
#define CORE_LIBSPEC
#include "../../contrib/khash/khashmzx.h"

// From MZX itself:

// Safe- self sufficient or completely macros/static inlines
#include "../const.h"
#include "../memcasecmp.h"
#include "../memfile.h"
#include "../world_format.h"
#include "../zip.h"

// Contains some CORE_LIBSPEC functions, which should be fine if the object
// is included in linking due to the CORE_LIBSPEC define above. Right now,
// checkres needs fsafeopen.o and util.o.
#include "../fsafeopen.h"
#include "../util.h"
#include "../world.h"

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath"
#endif

// From const.h (copied here for convenience)
#define BOARD_NAME_SIZE 25
#define MAX_BOARDS 250

// From data.h
#define IMAGE_FILE 100

// From sfx.h
#define NUM_SFX 50
#define LEGACY_SFX_SIZE 69

#define LEGACY_WORLD_PROTECTED_OFFSET      BOARD_NAME_SIZE
#define LEGACY_WORLD_GLOBAL_OFFSET_OFFSET  4230

#define MAX_PATH_DEPTH 10

static const char *found_append = "FOUND";
static const char *created_append = "CREATED";
static const char *not_found_append = "NOT FOUND";
static const char *unused_append = "UNUSED";
static const char *pattern_append = "PATTERN*";
static const char *maybe_used_append = "MATCH*";
static const char *maybe_created_append = "CREATED*";

// Status options
static boolean display_not_found = true;
static boolean display_found = false;
static boolean display_created = false;
static boolean display_unused = false;
static boolean display_wildcard = false;

// Detail options
static boolean display_filename_only = false;
static boolean display_first_only = true;
static boolean display_details = false;
static boolean display_all_details = false;

static enum
{
  SORT_BY_LOCATION,
  SORT_BY_FILENAME,
}
sort_by = SORT_BY_FILENAME;

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
  DIRENT_FAILED,
  ZIP_FAILED,
  NO_WORLD,
  MISSING_FILE
};

#define warnhere(...) \
 do{ fprintf(stderr, "At " __FILE__ ":%d: ", __LINE__); \
  fprintf(stderr, "" __VA_ARGS__); fflush(stderr); }while(0)

// MegaZeux's obtuse architecture requires this for the time being.
// This function is used in out_of_memory_check (util.c), and check alloc
// is required by zip.c (as it should be). util.c is also required by the
// directory reading functions in fsafeopen.c on non-Win32 platforms.

int error(const char *message, unsigned int a, unsigned int b, unsigned int c)
{
  fprintf(stderr, "%s\n", message);
  exit(-1);
}

static const char *decode_status(enum status status)
{
  switch(status)
  {
    case INVALID_ARGUMENTS:
      return "Invalid file or argument provided.";
    case CORRUPT_WORLD:
      return "World corruption or truncation detected.";
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
    case DIRENT_FAILED:
      return "Failed to read a required directory.";
    case ZIP_FAILED:
      return "Something is wrong with the zip file.";
    case NO_WORLD:
      return "No world file present.";
    default:
      return "Unknown error.";
  }
}

static void _get_path(char *dest, const char *src)
{
  int i;

  // Copy the file's path to the path string.
  i = strlen(src)-1;
  while((src[i] != '/') && (src[i] != '\\') && i)
    i--;

  strncpy(dest, src, i);
  dest[i] = 0;
}

static void join_path(char *dest, const char *dir, const char *file)
{
  if(dir && dir[0])
  {
    if(file && file[0])
    {
      if(dir[strlen(dir) - 1] != '/')
        snprintf(dest, MAX_PATH, "%s/%s", dir, file);

      else
        snprintf(dest, MAX_PATH, "%s%s", dir, file);
    }

    else
      snprintf(dest, MAX_PATH, "%s", dir);
  }

  else
  {
    snprintf(dest, MAX_PATH, "%s", file);
  }

  dest[MAX_PATH - 1] = 0;
}

static boolean is_simple_path(const char *src, boolean allow_expressions)
{
  size_t len = strlen(src);
  unsigned int i;

  // No empty filename.
  if(len == 0)
    return false;

  // No strings.
  if(len >= 1)
    if(src[0] == '$')
      return false;

  for(i = 0; i < len; i++)
  {
    // No interpolation. A single & at the end of an expression is a
    // valid interpolation due to a longstanding MZX bug.

    if(allow_expressions && src[i] == '&')
      if(i+1 == len || src[i+1] != '&')
        return false;

    // No expressions.
    if(allow_expressions && src[i] == '(')
      return false;
  }

  // No truncated DOS filenames (attempt to wildcard these)
  if(len <= 12)
  {
    const char *tpos = strchr(src, '~');
    if(tpos)
    {
      while(*(++tpos))
      {
        if(isdigit(*tpos)) continue;
        if(*tpos == '.' || *tpos == '\0')
          return false;
      }
    }
  }
  return true;
}

static const char *skip_expression(const char *src)
{
  int level = 0;
  if(*src != '(')
    return src;

  do
  {
    if(*src == '(')
      level++;
    if(*src == ')')
      level--;
    src++;
  }
  while(*src && level > 0);

  if(level == 0)
    return src;

  return NULL;
}

static const char *skip_interpolation(const char *src)
{
  if(*src != '&')
    return src;
  src++;

  if(*src == '&')
    return src;

  // Due to an MZX bug, the end of the string is a valid terminator.
  while(src && *src && *src != '&')
  {
    if(*src == '(')
      src = skip_expression(src);
    else
      src++;
  }
  if(src && *src == '&')
    src++;
  return src;
}

static boolean get_wildcard_path(char dest[MAX_PATH], const char *src)
{
  size_t len = strlen(src);
  size_t i;
  size_t j;

  // No empty filename
  if(len == 0)
    return false;

  // No strings.
  if(len >= 1)
    if(src[0] == '$')
      return false;

  for(i = 0, j = 0; i < len && j < MAX_PATH; i++)
  {
    if(src[i] == '&' && src[i+1] != '&')
    {
      size_t start = i + 1;
      const char *end = skip_interpolation(src + i);
      if(!end)
        return false;

      i = end - src - 1;

      // String (assume interpolation for now)
      if(src[start] == '$')
        dest[j++] = '*';

      // Special: INPUT is also a string when interpolated.
      else
      if(!strncasecmp(src + start, "INPUT", strlen("INPUT")))
        dest[j++] = '*';

      // Counter
      else
        dest[j++] = '#';
    }
    else

    if(src[i] == '(')
    {
      // Expression
      const char *end = skip_expression(src + i);
      if(!end)
        return false;

      i = end - src - 1;
      dest[j++] = '#';
    }
    else

    if(src[i] == '~')
    {
      // Truncated DOS filename--replace ~### with wildcard
      size_t backup = i;
      if(i + 1 < len && isdigit(src[i + 1]))
      {
        while(i + 1 < len && isdigit(src[i + 1])) i++;
        if(i + 1 >= len || src[i + 1] == '.')
        {
          dest[j++] = '*';
          continue;
        }
      }
      i = backup;
      dest[j++] = '~';
    }
    else

    if(src[i] == '*')
    {
      // Escape %
      dest[j++] = '|';
      dest[j++] = '*';
    }
    else

    if(src[i] == '#')
    {
      // Escape #
      dest[j++] = '|';
      dest[j++] = '#';
    }
    else

    if(src[i] == '|')
    {
      // Escape | (can't use backslash because it'll be recognized as a
      // path separator)
      dest[j++] = '|';
      dest[j++] = '|';
    }

    else
      dest[j++] = src[i];
  }
  dest[MIN(j, MAX_PATH - 1)] = 0;
  return true;
}

static boolean check_wildcard_path(const char *path, const char *wildcard)
{
#define MAX_STATE 1024
#define MAX_ITER 1024
  size_t iter = 0;
  struct _state
  {
    uint16_t p;
    uint16_t w;
  }
  state[MAX_STATE] = {{0, 0}};
  int pos = 0;

  size_t wildcard_len = strlen(wildcard);
  size_t path_len = strlen(path);
  size_t i;
  size_t p;
  size_t w;
  char next;

  if(path_len > MAX_PATH)
    return false;

  while(pos >= 0)
  {
    iter++;
    if(iter >= MAX_ITER) return -1;

    p = state[pos].p;
    w = state[pos].w;

    if(p == path_len || w == wildcard_len)
    {
      // Skip trailing wildcards if they exist
      while(wildcard[w] == '*') w++;

      if(p == path_len && w == wildcard_len)
        return true;

      pos--;
      continue;
    }

    next = wildcard[w++];

    switch(next)
    {
      case '*':
      {
        // Skip duplicate wildcards if they exist, pop current state
        while(wildcard[w] == '*') w++;
        pos--;

        for(i = p; i <= path_len; i++)
        {
          if(pos == MAX_STATE - 1) break;
          pos++;
          state[pos].p = i;
          state[pos].w = w;
        }
        break;
      }

      case '#':
      {
        // Numeric
        // Consume '-' if it exists, pop current state
        if(path[p] == '-') p++;
        pos--;

        // Allow up to 10 digits to be consumed
        for(i = p; i < MIN(p + 10, path_len); i++)
        {
          if(pos == MAX_STATE - 1) break;
          if(isdigit(path[i]))
          {
            pos++;
            state[pos].p = i + 1;
            state[pos].w = w;
          }
          else
            break;
        }
        break;
      }

      case '|':
        next = wildcard[w++];
        /* fall-through */

      default:
      {
        if(memtolower(path[p]) == memtolower(next))
        {
          state[pos].p++;
          state[pos].w = w;
        }
        else
          pos--;

        break;
      }
    }
  }
  return false;
}

static void strcpy_fsafe(char *dest, const char *src)
{
  int result = fsafetranslate(src, dest);

  // SUCCESS - file was actually found physically
  // MATCH_FAILED - no file was found physically, which is fine here
  // MATCHED_DIRECTORY - a physical directory was found, which is fine here

  if(result != FSAFE_SUCCESS && result != -FSAFE_MATCH_FAILED &&
   result != -FSAFE_MATCHED_DIRECTORY)
    dest[0] = 0;
}

static boolean path_search(const char *path_name, size_t base_len, int max_depth,
 void *data, void (*found_fn)(void *data, const char *name, size_t name_len))
{
  DIR *dir;
  struct dirent *d;
  struct stat st;
  boolean join_paths = true;

  if(!strlen(path_name) || !strcmp(path_name, "."))
  {
    dir = opendir(".");
    max_depth = MAX_PATH_DEPTH;
    join_paths = false;
    base_len = 0;
  }
  else
    dir = opendir(path_name);

  if(dir)
  {
    char *_current = cmalloc(MAX_PATH);
    const char *current = NULL;

    while((d = readdir(dir)) != NULL)
    {
      if(strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
      {
        if(join_paths)
        {
          join_path(_current, path_name, d->d_name);
          current = _current;
        }
        else
          current = d->d_name;

        if(!stat(current, &st))
        {
          if(S_ISDIR(st.st_mode))
          {
            // yolo
            if(max_depth > 0)
              path_search(current, base_len, max_depth - 1, data, found_fn);
          }
          else

          if(S_ISREG(st.st_mode))
          {
            // Strip off the base path if requested.
            if(base_len && strlen(current) > base_len)
            {
              current += base_len;
              while(*current == '\\' || *current == '/') current++;
            }

            found_fn(data, current, strlen(current));
          }
        }
      }
    }
    closedir(dir);
    free(_current);
    return true;
  }
  return false;
}

static int16_t robot_xpos[256];
static int16_t robot_ypos[256];

#define PARENT_DEFAULT_LEN 13
#define RESOURCE_DEFAULT_LEN 14
#define DETAILS_MAX_LEN 35
#define DETAILS_SHORT_LEN 12
#define GLOBAL_ROBOT 0
#define DONT_PRINT -255
#define IS_SFX -1
#define IS_BOARD_MOD -3
#define IS_BOARD_CHARSET -2
#define IS_BOARD_PALETTE -1

static int started_table = 0;
static int parent_max_len = PARENT_DEFAULT_LEN;
static int resource_max_len = RESOURCE_DEFAULT_LEN;

static void output(const char *required_by,
 int board_num, int robot_num, int line_num,
 const char *resource_path, const char *status, const char *found_in)
{
  char details[DETAILS_MAX_LEN];
  int details_max_len = display_details ?
   (display_all_details ? DETAILS_MAX_LEN : DETAILS_SHORT_LEN) : 0;

  if(!display_filename_only)
  {
    found_in = found_in ? found_in : "";

    if(!started_table)
    {
      fprintf(stdout, "\n");

      fprintf(stdout, "%-*.*s  %-*.*s%-*.*s  %-10s %s\n",
       parent_max_len, parent_max_len, "Required by",
       details_max_len, details_max_len, "B#    R#    Line     Position",
       resource_max_len, resource_max_len, "Expected file",
       "Status",
       "Found in"
      );

      fprintf(stdout, "%-*.*s  %-*.*s%-*.*s  %-10s %s\n",
       parent_max_len, parent_max_len, "-----------",
       details_max_len, details_max_len, "---   ---   ------   -----------",
       resource_max_len, resource_max_len, "-------------",
       "------",
       "--------"
      );

      started_table = 1;
    }

    if(display_details && board_num != DONT_PRINT)
    {
      char board[6];
      char robot[6];
      char line[8];
      char xy[14];
      snprintf(board, 6, "%d", board_num);
      snprintf(robot, 6, "%d", robot_num);
      snprintf(line, 8, "%d", line_num);

      if(board_num == IS_SFX)
        snprintf(details, DETAILS_MAX_LEN, "sfx   %-3.3s", robot);
      else
      if(robot_num > 0)
      {
        snprintf(xy, 14, "%d, %d",
          robot_xpos[(unsigned char)robot_num],
          robot_ypos[(unsigned char)robot_num]
        );
        snprintf(details, DETAILS_MAX_LEN,
          "b%-3.3s  r%-3.3s  %-6.6s   %-11.11s",
          board, robot, line, xy
        );
      }
      else
      if(board_num == NO_BOARD && robot_num == GLOBAL_ROBOT)
      {
        snprintf(details, DETAILS_MAX_LEN,
         "      gl    %-6.6s   -1, -1", line);
      }
      else
      if(robot_num == IS_BOARD_MOD)
        snprintf(details, DETAILS_MAX_LEN, "b%-3.3s  mod  ", board);
      else
      if(robot_num == IS_BOARD_CHARSET)
        snprintf(details, DETAILS_MAX_LEN, "b%-3.3s  chr  ", board);
      else
      if(robot_num == IS_BOARD_PALETTE)
        snprintf(details, DETAILS_MAX_LEN, "b%-3.3s  pal  ", board);
      else
        snprintf(details, DETAILS_MAX_LEN, "(unknown)");
      details[DETAILS_MAX_LEN - 1] = '\0';
    }
    else
      details[0] = 0;

    fprintf(stdout, "%-*.*s  %-*.*s%-*.*s  %-10s %s\n",
     parent_max_len, parent_max_len, required_by,
     details_max_len, details_max_len, details,
     resource_max_len, resource_max_len, resource_path,
     status,
     found_in
    );
  }
  else
  {
    // Quiet mode- just print the resource paths
    fprintf(stdout, "%s\n", resource_path);
  }
}


/*******************/
/* Data processing */
/*******************/

struct base_path_file
{
  char file_path[MAX_PATH];
  int file_path_len;
  boolean used;
  boolean used_wildcard;
};

KHASH_SET_INIT(BASE_PATH_FILE, struct base_path_file *, file_path, file_path_len)

struct base_path
{
  char actual_path[MAX_PATH];
  char relative_path[MAX_PATH];
  struct zip_archive *zp;
  khash_t(BASE_PATH_FILE) *file_list_table;
};

struct base_file
{
  char file_name[MAX_PATH];
  char relative_path[MAX_PATH];
  int world_version;
};

struct resource
{
  char path[MAX_PATH * 2];
  int path_len;
  int key_len;
  int board_num;
  int robot_num;
  int line_num;
  boolean is_wildcard;
  struct base_file *parent;
};

KHASH_SET_INIT(RESOURCE, struct resource *, path, key_len)

// NULL is important
static khash_t(RESOURCE) *requirement_table = NULL;
static khash_t(RESOURCE) *resource_table = NULL;

static struct resource *add_requirement_ext(const char *src,
 struct base_file *file, int board_num, int robot_num, int line_num,
 boolean allow_expressions)
{
  // A resource file required by a world/board.
  struct resource *req = NULL;
  char fsafe_buffer[MAX_PATH * 2];
  char temporary_buffer[MAX_PATH];
  boolean is_wildcard = false;
  int fsafe_len;

  // Offset the required file's path with the relative path of its parent
  // The file might require wildcard conversion first (if allowed)
  if(!is_simple_path(src, allow_expressions))
  {
    if(!get_wildcard_path(fsafe_buffer, src))
      return NULL;

    join_path(temporary_buffer, file->relative_path, fsafe_buffer);
    is_wildcard = true;
  }
  else
    join_path(temporary_buffer, file->relative_path, src);

  strcpy_fsafe(fsafe_buffer, temporary_buffer);
  fsafe_len = strlen(fsafe_buffer);

  if(!display_first_only)
  {
    // add more info to the key.
    snprintf(fsafe_buffer + fsafe_len + 1, MAX_PATH, "%04x%04x%08x%s",
     (int16_t)board_num, (int16_t)robot_num, line_num, file->file_name);
    fsafe_len += 1 + strlen(fsafe_buffer + fsafe_len + 1);
  }

  KHASH_FIND(RESOURCE, requirement_table, fsafe_buffer, fsafe_len, req);

  if(!req)
  {
    req = cmalloc(sizeof(struct resource));

    // NOTE: might be copying data past the terminator, see key extension above
    memcpy(req->path, fsafe_buffer, fsafe_len + 1);
    req->path_len = strlen(fsafe_buffer);
    req->key_len = fsafe_len;
    req->board_num = board_num;
    req->robot_num = robot_num;
    req->line_num = line_num;
    req->is_wildcard = is_wildcard;
    req->parent = file;

    // Only need to compute these sizes if there's a situation where a
    // requirement might be displayed. The only time this isn't the case
    // currently is when only unused files are being displayed, though.
    if(display_not_found || display_found || display_created || display_wildcard)
    {
      resource_max_len = MAX(resource_max_len, req->path_len);
      parent_max_len = MAX(parent_max_len, (int)strlen(file->file_name));
    }
    KHASH_ADD(RESOURCE, requirement_table, req);
  }
  return req;
}

static struct resource *add_requirement_sfx(const char *src,
 struct base_file *file, int board_num, int robot_num, int line_num)
{
  return add_requirement_ext(src, file, board_num, robot_num, line_num, false);
}

static struct resource *add_requirement_board(const char *src,
 struct base_file *file, int board_num, int resource_type)
{
  if(board_num < 0 || resource_type >= 0) return NULL;
  return add_requirement_ext(src, file, board_num, resource_type, -1, false);
}

static struct resource *add_requirement_robot(const char *src,
 struct base_file *file, int board_num, int robot_num, int line_num)
{
  if(robot_num < 0 || board_num < 0 ||
   (board_num == NO_BOARD && robot_num != GLOBAL_ROBOT))
    return NULL;

  return add_requirement_ext(src, file, board_num, robot_num, line_num, true);
}

static struct resource *add_resource(const char *src, struct base_file *file)
{
  // A filename found in Robotic code that may fulfill a requirement for a file.
  struct resource *res = NULL;
  char fsafe_buffer[MAX_PATH];
  char temporary_buffer[MAX_PATH];
  boolean is_wildcard = false;
  int fsafe_len;

  // Offset the required file's path with the relative path of its parent
  // The file might require wildcard conversion first (if allowed)
  if(!is_simple_path(src, true))
  {
    if(!get_wildcard_path(fsafe_buffer, src))
      return NULL;

    join_path(temporary_buffer, file->relative_path, fsafe_buffer);
    is_wildcard = true;
  }
  else
    join_path(temporary_buffer, file->relative_path, src);

  strcpy_fsafe(fsafe_buffer, temporary_buffer);
  fsafe_len = strlen(fsafe_buffer);

  KHASH_FIND(RESOURCE, resource_table, fsafe_buffer, fsafe_len, res);

  if(!res)
  {
    res = cmalloc(sizeof(struct resource));

    snprintf(res->path, MAX_PATH, "%s", fsafe_buffer);
    res->path_len = fsafe_len;
    res->key_len = fsafe_len;
    res->board_num = -1;
    res->robot_num = -1;
    res->line_num = -1;
    res->is_wildcard = is_wildcard;
    res->parent = NULL;

    // Created resources are never displayed directly so this isn't necessary.
    // However, if this changes, uncomment this.
    /*if(display_created)
    {
      resource_max_len = MAX(resource_max_len, fsafe_len);
      parent_max_len = MAX(parent_max_len, (int)strlen(file->file_name));
    }*/
    KHASH_ADD(RESOURCE, resource_table, res);
  }

  return res;
}

static void add_base_path_file(struct base_path *path,
 const char *file_name, size_t file_name_length)
{
  struct base_path_file *entry;

  if(file_name_length >= MAX_PATH)
    file_name_length = MAX_PATH - 1;

  KHASH_FIND(BASE_PATH_FILE, path->file_list_table, file_name, file_name_length,
    entry);

  if(!entry)
  {
    entry = cmalloc(sizeof(struct base_path_file));

    memcpy(entry->file_path, file_name, file_name_length);
    entry->file_path[file_name_length] = '\0';
    entry->file_path_len = file_name_length;
    entry->used = false;
    entry->used_wildcard = false;

    KHASH_ADD(BASE_PATH_FILE, path->file_list_table, entry);

    if(display_unused || display_wildcard)
      resource_max_len = MAX(resource_max_len, (int)file_name_length);
  }
}

static void add_base_path_file_wr(void *path, const char *file_name,
 size_t file_name_length)
{
  add_base_path_file((struct base_path *)path, file_name, file_name_length);
}

static void build_zip_base_path_table(struct base_path *path,
 struct zip_archive *zp)
{
  struct zip_file_header **files = zp->files;
  struct zip_file_header *fh;
  int num_files = zp->num_files;
  int i;

  for(i = 0; i < num_files; i++)
  {
    fh = files[i];
    add_base_path_file(path, fh->file_name, fh->file_name_length);
  }
}

static void change_base_path_dir(struct base_path *current_path,
 const char *new_relative_path)
{
  size_t len;
  fsafetranslate(new_relative_path, current_path->relative_path);

  len = strlen(current_path->relative_path);
  if(current_path->relative_path[len - 1] != '/' && len < MAX_PATH - 1)
  {
    current_path->relative_path[len++] = '/';
    current_path->relative_path[len] = '\0';
  }
}

static struct base_path *add_base_path(const char *path_name,
 struct base_path ***path_list, int *path_list_size, int *path_list_alloc)
{
  struct base_path *new_path = ccalloc(1, sizeof(struct base_path));
  int alloc = *path_list_alloc;
  int size = *path_list_size;

  size_t path_name_len = strlen(path_name);
  const char *ext = path_name_len >= 4 ? path_name + path_name_len - 4 : NULL;

  if(ext && !strcasecmp(ext, ".ZIP"))
  {
    struct zip_archive *zp = zip_open_file_read(path_name);

    if(!zp)
    {
      free(new_path);
      return NULL;
    }

    build_zip_base_path_table(new_path, zp);
    new_path->zp = zp;
  }
  else
  {
    if(!path_search(path_name, path_name_len, MAX_PATH_DEPTH,
     (void *)new_path, add_base_path_file_wr))
    {
      free(new_path);
      return NULL;
    }
  }

  snprintf(new_path->actual_path, MAX_PATH, "%s", path_name);

  if(size == alloc)
  {
    if(alloc == 0)
    {
      *path_list_alloc = 4;
      alloc = 4;
    }

    *path_list = realloc(*path_list, 2 * alloc * sizeof(struct base_path *));
    *path_list_alloc *= 2;
  }

  (*path_list)[size] = new_path;
  (*path_list_size)++;

  return new_path;
}

static struct base_file *add_base_file(const char *path_name,
 struct base_file ***file_list, int *file_list_size, int *file_list_alloc)
{
  struct base_file *new_file = ccalloc(1, sizeof(struct base_file));
  int alloc = *file_list_alloc;
  int size = *file_list_size;

  snprintf(new_file->file_name, MAX_PATH, "%s", path_name);

  if(size == alloc)
  {
    if(alloc == 0)
    {
      *file_list_alloc = 4;
      alloc = 4;
    }

    *file_list = realloc(*file_list, 2 * alloc * sizeof(struct base_file *));
    *file_list_alloc *= 2;
  }

  (*file_list)[size] = new_file;
  (*file_list_size)++;

  return new_file;
}

struct base_file_list_data
{
  struct base_file ***file_list;
  int *file_list_size;
  int *file_list_alloc;
};

static void base_file_found_fn(void *data, const char *name, size_t name_len)
{
  struct base_file_list_data *d = (struct base_file_list_data *)data;
  const char *ext = name_len >= 4 ? name + name_len - 4 : NULL;

  if(ext && (!strcasecmp(ext, ".MZX") || !strcasecmp(ext, ".MZB")))
  {
    struct base_file *bf =
     add_base_file(name, d->file_list, d->file_list_size, d->file_list_alloc);

    if(bf)
      _get_path(bf->relative_path, name);
  }
}

static boolean add_base_files_from_path(const char *path_name,
 struct base_file ***file_list, int *file_list_size, int *file_list_alloc)
{
  struct base_file_list_data d = { file_list, file_list_size, file_list_alloc };
  return path_search(path_name, strlen(path_name), MAX_PATH_DEPTH,
   (void *)&d, base_file_found_fn);
}

static int req_sort_by_location_fn(const void *A, const void *B)
{
  const struct resource *a = *(struct resource **)A;
  const struct resource *b = *(struct resource **)B;
  int parent_cmp = strcmp(a->parent->file_name, b->parent->file_name);
  return
    parent_cmp ? parent_cmp :
    a->board_num != b->board_num ? a->board_num - b->board_num :
    a->robot_num != b->robot_num ? a->robot_num - b->robot_num :
    a->line_num != b->line_num ? a->line_num - b->line_num :
    strcmp(a->path, b->path);
}

static int req_sort_by_filename_fn(const void *A, const void *B)
{
  const struct resource *a = *(struct resource **)A;
  const struct resource *b = *(struct resource **)B;
  int path_cmp = strcmp(a->path, b->path);
  int parent_cmp = strcmp(a->parent->file_name, b->parent->file_name);
  return
    path_cmp ? path_cmp :
    parent_cmp ? parent_cmp :
    a->board_num != b->board_num ? a->board_num - b->board_num :
    a->robot_num != b->robot_num ? a->robot_num - b->robot_num :
    a->line_num - b->line_num;
}

static int bpf_sort_fn(const void *A, const void *B)
{
  const struct base_path_file *a = *(struct base_path_file **)A;
  const struct base_path_file *b = *(struct base_path_file **)B;
  return
  (a->used_wildcard != b->used_wildcard) ? b->used_wildcard - a->used_wildcard :
   strcmp(a->file_path, b->file_path);
}

static void process_requirements(struct base_path **path_list,
 int path_list_size)
{
#define FOUND_WILDCARD 2
  struct stat stat_info;
  struct base_path *current_path;
  struct base_path_file *bpf;
  struct resource *req;
  struct resource *res;
  char path_buffer[MAX_PATH];
  char *translated_path;
  size_t len;
  boolean found;
  size_t i;
  int j;

  struct resource **req_sorted;
  size_t num_reqs;

  int (*sort_fn)(const void *, const void *);
  switch(sort_by)
  {
    default:
    case SORT_BY_LOCATION:
      sort_fn = req_sort_by_location_fn;
      break;

    case SORT_BY_FILENAME:
      sort_fn = req_sort_by_filename_fn;
      break;
  }

  if(!requirement_table)
    return;

  num_reqs = kh_size(requirement_table);
  req_sorted = cmalloc(num_reqs * sizeof(struct resource *));

  // Build a list of the requirements from the hash table and sort it.
  // This is entirely for the purpose of having more useful output.
  i = 0;
  KHASH_ITER(RESOURCE, requirement_table, req,
  {
    req_sorted[i++] = req;
  });
  qsort(req_sorted, num_reqs, sizeof(struct resource *), sort_fn);

  // Now, actually process the requirements.
  for(i = 0; i < num_reqs; i++)
  {
    req = req_sorted[i];
    found = false;

    for(j = 0; j < path_list_size; j++)
    {
      current_path = path_list[j];
      len = strlen(current_path->relative_path);

      // The required resource's path must start with the relative path
      if(strncasecmp(req->path, current_path->relative_path, len))
        continue;

      translated_path = req->path + len;

      if(current_path->file_list_table)
      {
        if(req->is_wildcard)
        {
          KHASH_ITER(BASE_PATH_FILE, current_path->file_list_table, bpf,
          {
            if(check_wildcard_path(bpf->file_path, translated_path))
            {
              bpf->used_wildcard = true;
              found = FOUND_WILDCARD;

              // If unused/wildcards are being displayed, every single thing
              // this possibly matches needs to be detected. Otherwise, break
              if(!display_unused && !display_wildcard)
                break;
            }
          });

          // See: note above.
          if(found && !display_unused && !display_wildcard)
            break;

          continue;
        }

        // Normal resource: try to find the file in the base path's hash table
        KHASH_FIND(BASE_PATH_FILE, current_path->file_list_table,
         translated_path, strlen(translated_path), bpf);

        if(bpf)
        {
          bpf->used = true;
          found = true;
          break;
        }
      }

      else
      {
        // Try to find an actual file
        join_path(path_buffer, current_path->actual_path, translated_path);

        if(!stat(path_buffer, &stat_info))
        {
          found = true;
          break;
        }
      }
    }

    if(found == FOUND_WILDCARD)
    {
      if(display_wildcard)
        output(req->parent->file_name, req->board_num, req->robot_num,
         req->line_num, req->path, pattern_append, current_path->actual_path);
    }
    else

    if(found)
    {
      if(display_found)
        output(req->parent->file_name, req->board_num, req->robot_num,
         req->line_num, req->path, found_append, current_path->actual_path);
    }
    else
    {
      // Try to find in the created resources table
      KHASH_FIND(RESOURCE, resource_table, req->path, req->path_len, res);

      if(!res && resource_table)
      {
        // There might be wildcard created resources...
        KHASH_ITER(RESOURCE, resource_table, res,
        {
          if(check_wildcard_path(req->path, res->path))
            break;
          res = NULL;
        });
      }

      if(res)
      {
        const char *append =
         (res->is_wildcard ? maybe_created_append : created_append);

        if(display_created)
          output(req->parent->file_name, req->board_num, req->robot_num,
           req->line_num, req->path, append, NULL);
      }
      else
      {
        if(display_not_found)
          output(req->parent->file_name, req->board_num, req->robot_num,
           req->line_num, req->path, not_found_append, NULL);
      }
    }
  }
  free(req_sorted);

  if(display_unused || display_wildcard)
  {
    // Now go through all of the base paths and print the files that aren't
    // actually used by anything... yikes.
    struct base_path_file **bpf_sorted;
    size_t bpf_alloc = 0;

    for(j = 0; j < path_list_size; j++)
      if(path_list[j]->file_list_table)
        bpf_alloc = MAX(bpf_alloc, kh_size(path_list[j]->file_list_table));

    if(bpf_alloc)
    {
      bpf_sorted = cmalloc(bpf_alloc * sizeof(struct base_path_file *));

      for(j = 0; j < path_list_size; j++)
      {
        current_path = path_list[j];

        if(current_path->file_list_table)
        {
          size_t k;
          i = 0;
          KHASH_ITER(BASE_PATH_FILE, current_path->file_list_table, bpf,
          {
            if(!bpf->used)
              bpf_sorted[i++] = bpf;
          });
          qsort(bpf_sorted, i, sizeof(struct base_path_file *), bpf_sort_fn);

          for(k = 0; k < i; k++)
          {
            const char *file_path;
            size_t len;
            bpf = bpf_sorted[k];
            file_path = bpf->file_path;

            // Don't print "unused" MZX/MZB files.
            len = strlen(file_path);
            if(len >= 4 &&
             (!strcasecmp(file_path + len - 4, ".MZX") ||
              !strcasecmp(file_path + len - 4, ".MZB")))
              continue;

            // No "unused" directories either.
            if(len && file_path[len - 1] == '/')
              continue;

            // Want to display this to the user as being in its relative path
            if(path_list[j]->relative_path[0])
            {
              join_path(path_buffer, path_list[j]->relative_path, file_path);
              file_path = path_buffer;
            }

            if(display_unused && !bpf->used_wildcard)
            {
              output("", DONT_PRINT, -1, -1, file_path, unused_append,
               current_path->actual_path);
            }
            else

            if(display_wildcard && bpf->used_wildcard)
            {
              output("", DONT_PRINT, -1, -1, file_path, maybe_used_append,
               current_path->actual_path);
            }
          }
        }
      }
      free(bpf_sorted);
    }
  }

  // Reset these for next time
  started_table = 0;
  parent_max_len = PARENT_DEFAULT_LEN;
  resource_max_len = RESOURCE_DEFAULT_LEN;
}

static void clear_data(struct base_path **path_list,
 int path_list_size, struct base_file **file_list, int file_list_size)
{
  struct base_path *bp;
  struct resource *res;
  struct base_path_file *fp;
  int i;

  KHASH_ITER(RESOURCE, requirement_table, res,
  {
    KHASH_DELETE(RESOURCE, requirement_table, res);
    free(res);
  });
  KHASH_CLEAR(RESOURCE, requirement_table);

  KHASH_ITER(RESOURCE, resource_table, res,
  {
    KHASH_DELETE(RESOURCE, resource_table, res);
    free(res);
  });
  KHASH_CLEAR(RESOURCE, resource_table);

  for(i = 0; i < path_list_size; i++)
  {
    bp = path_list[i];

    KHASH_ITER(BASE_PATH_FILE, bp->file_list_table, fp,
    {
      KHASH_DELETE(BASE_PATH_FILE, bp->file_list_table, fp);
      free(fp);
    });
    KHASH_CLEAR(BASE_PATH_FILE, bp->file_list_table);

    if(bp->zp)
      zip_close(bp->zp, NULL);

    free(bp);
  }
  free(path_list);

  for(i = 0; i < file_list_size; i++)
    free(file_list[i]);

  free(file_list);
}


/**********/
/* Common */
/**********/

// From world.c
static int _world_magic(const unsigned char magic_string[3])
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
      if((magic_string[1] > 1) && (magic_string[1] < 10))
        return ((int)magic_string[1] << 8) + (int)magic_string[2];
    }
  }

  return 0;
}

// From editor/board.c
static int board_magic(const unsigned char magic_string[4])
{
  if(magic_string[0] == 0xFF)
  {
    if(magic_string[1] == 'M')
    {
      if(magic_string[2] == 'B' && magic_string[3] == '2')
        return V251;

      if((magic_string[2] > 1) && (magic_string[2] < 10))
        return ((int)magic_string[2] << 8) + (int)magic_string[3];
    }
  }

  return 0;
}

static enum status parse_sfx(char *sfx_buf, struct base_file *file,
 int board_num, int robot_num, int line_num)
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
    add_requirement_sfx(start + 1, file, board_num, robot_num, line_num);
  }

  return ret;
}

#define match(s, reqv) ((world_version >= reqv) && \
 (fn_len == sizeof(s)-1) && (!strcasecmp(function_counter, s)))

#define match_partial(s, reqv) ((world_version >= reqv) && \
 (fn_len >= sizeof(s)-1) && (!strncasecmp(function_counter, s, fn_len)))

#define TERMINATE(s,slen) \
 do{ if(slen && s[slen - 1] == '\0') slen--; else s[slen]='\0'; }while(0)

static enum status parse_legacy_bytecode(struct memfile *mf,
 unsigned int program_size, struct base_file *file, int board_num, int robot_num)
{
  int world_version = file->world_version;
  enum status ret = SUCCESS;
  int command_length;
  int command;
  int line_num = 0;

  char function_counter[256];
  size_t fn_len;

  char src[256];
  size_t src_len;

  // skip 0xff marker
  if(mfgetc(mf) != 0xff)
  {
    warnhere("Invalid program start byte\n");
    return CORRUPT_WORLD;
  }

  while(1)
  {
    if(mftell(mf) >= (long int)program_size)
    {
      warnhere("Program exceeded expected length\n");
      return CORRUPT_WORLD;
    }

    command_length = mfgetc(mf);
    if(command_length == 0)
      break;

    command = mfgetc(mf);
    line_num++;

    // These constants may eventually change in debytecode versions.
    // Since this is for legacy bytecode, leave the fixed numbers.
    switch(command)
    {
      case 10: // ROBOTIC_CMD_SET
      {
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        fn_len = mfgetc(mf);

        // Int parameter
        if(fn_len == 0)
        {
          mfgetw(mf);
          break;
        }

        // String parameter
        if(mfread(function_counter, 1, fn_len, mf) != fn_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(function_counter, fn_len);

        if(fn_len == 0)
          break;

        if( match("FREAD_OPEN", V260)
         || match("SMZX_PALETTE", V269)
         || match("SMZX_INDICES", V291)
         || match("LOAD_COUNTERS", V290)
         || match("LOAD_GAME", V268)
         || match_partial("LOAD_BC", V270)
         || match_partial("LOAD_ROBOT", V270))
        {
          debug("SET: %s (%s)\n", src, function_counter);
          add_requirement_robot(src, file, board_num, robot_num, line_num);
          break;
        }

        if( match("FWRITE_OPEN", V260)
         || match("FWRITE_APPEND", V260)
         || match("FWRITE_MODIFY", V260)
         || match("SAVE_COUNTERS", V290)
         || match("SAVE_GAME", V268)
         || match_partial("SAVE_BC", V270)
         || match_partial("SAVE_ROBOT", V270))
        {
          debug("SET: %s (%s)\n", src, function_counter);
          add_resource(src, file);
          break;
        }

        if( match("SAVE_WORLD", V269c) )
        {
          // Maximum version
          if(world_version < V290)
          {
            debug("SET: %s (%s)\n", src, function_counter);
            add_resource(src, file);
          }
          break;
        }

        break;
      }

      case 38: // ROBOTIC_CMD_MOD
      {
        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        // ignore MOD "", MOD "*"
        if(!src_len || !strcmp(src, "*"))
          break;

        // Clip filename.mod*
        if(src[src_len - 1] == '*')
          src[src_len - 1] = 0;

        debug("MOD: %s\n", src);
        add_requirement_robot(src, file, board_num, robot_num, line_num);
        break;
      }

      case 39: // ROBOTIC_CMD_SAM
      {
        // Frequency
        src_len = mfgetc(mf);

        if(src_len == 0)
          mfgetw(mf);
        else
          mfseek(mf, src_len, SEEK_CUR);

        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        debug("SAM: %s\n", src);
        add_requirement_robot(src, file, board_num, robot_num, line_num);
        break;
      }

      case 43: // ROBOTIC_CMD_PLAY
      case 45: // ROBOITC_CMD_WAIT_THEN_PLAY
      case 49: // ROBOTIC_CMD_PLAY_IF_SILENT
      {
        // Play string
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        ret = parse_sfx(src, file, board_num, robot_num, line_num);
        if(ret != SUCCESS)
          return ret;
        break;
      }

      case 79: // ROBOTIC_CMD_PUT_XY
      {
        // Filename... maybe.
        src_len = mfgetc(mf);

        if(src_len != 0)
        {
          if(mfread(src, 1, src_len, mf) != src_len)
          {
            warnhere("Truncated command\n");
            return CORRUPT_WORLD;
          }
          // Don't trust null termination from data read in
          TERMINATE(src, src_len);

          fn_len = mfgetc(mf);

          // Thing
          if(fn_len == 0)
          {
            if(mfgetw(mf) == IMAGE_FILE && src[0] == '@')
            {
              debug("PUT @file: %s\n", src + 1);
              add_requirement_robot(src + 1, file, board_num, robot_num,
               line_num);
            }
          }
          else
          {
            mfseek(mf, fn_len, SEEK_CUR);
          }
        }
        else
        {
          mfgetw(mf);

          // Thing
          src_len = mfgetc(mf);
          mfseek(mf, src_len ? src_len : 2, SEEK_CUR);
        }

        // Param
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);

        // x
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);

        // y
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);
        break;
      }

      case 200: // ROBOTIC_CMD_MOD_FADE_IN
      {
        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        // ignore MOD "", MOD "*"
        if(!src_len || !strcmp(src, "*"))
          break;

        // Clip filename.mod*
        if(src[src_len - 1] == '*')
          src[src_len - 1] = 0;

        debug("MOD FADE IN: %s\n", src);
        add_requirement_robot(src, file, board_num, robot_num, line_num);
        break;
      }

      case 201: // ROBOTIC_CMD_COPY_BLOCK
      {
        // x1
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);

        // y1
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);

        // w
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);

        // h
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);

        // x2 or @filename
        src_len = mfgetc(mf);
        if(src_len != 0)
        {
          if(mfread(src, 1, src_len, mf) != src_len)
          {
            warnhere("Truncated command\n");
            return CORRUPT_WORLD;
          }
          // Don't trust null termination from data read in
          TERMINATE(src, src_len);

          if(src[0] == '@')
          {
            debug("COPY BLOCK @file: %s\n", src + 1);
            add_resource(src + 1, file);
          }
        }
        else
        {
          mfseek(mf, src_len ? src_len : 2, SEEK_CUR);
        }

        // y2
        src_len = mfgetc(mf);
        mfseek(mf, src_len ? src_len : 2, SEEK_CUR);
        break;
      }

      case 216: // ROBOTIC_CMD_LOAD_CHAR_SET
      {
        const char *rest = src;

        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        if(src[0] == '+')
        {
          char tempc = src[3];

          src[3] = 0;
          strtol(src + 1, (char **)(&rest), 16);
          src[3] = tempc;
        }

        else if(src[0] == '@')
        {
          char tempc;
          int maxlen;

          if(src[1] == '&')
          {
            rest = skip_interpolation(src + 1);
          }
          else

          if(src[1] == '(')
          {
            rest = skip_expression(src + 1);
          }

          else
          {
            if(world_version < V290)
              maxlen = 3;
            else
              maxlen = 4;

            tempc = src[maxlen+1];
            src[maxlen+1] = 0;
            strtol(src + 1, (char **)(&rest), 10);
            src[maxlen+1] = tempc;
          }
          if(!rest)
            rest = src;
        }

        debug("LOAD CHAR SET: %s\n", rest);
        add_requirement_robot(rest, file, board_num, robot_num, line_num);
        break;
      }

      case 222: // ROBOTIC_CMD_LOAD_PALETTE
      {
        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        debug("LOAD PALETTE: %s\n", src);
        add_requirement_robot(src, file, board_num, robot_num, line_num);
        break;
      }

      case 226: // ROBOTIC_CMD_SWAP_WORLD
      {
        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        // Don't trust null termination from data read in
        TERMINATE(src, src_len);

        debug("SWAP WORLD: %s\n", src);
        add_requirement_robot(src, file, board_num, robot_num, line_num);
        break;
      }

      default:
      {
        if(mfseek(mf, command_length - 1, SEEK_CUR) != 0)
        {
          warnhere("Truncated command\n");
          return CORRUPT_WORLD;
        }
        break;
      }
    }

    if(ret != SUCCESS)
      return ret;

    if(mfgetc(mf) != command_length)
    {
      warnhere("Command start length != end length\n");
      return CORRUPT_WORLD;
    }
  }

  return ret;
}


/*****************/
/* Legacy Worlds */
/*****************/

static enum status parse_legacy_robot(struct memfile *mf,
 struct base_file *file, int board_num, int robot_num)
{
  unsigned int program_size;
  struct memfile prog;

  enum status ret = SUCCESS;

  // program_length (2),
  program_size = mfgetw(mf);

  // program_location (2), robot_name (15), robot_char (1),
  // cur_prog_line (2), pos_within_line (1), robot_cycle (1), cycle_count (1),
  // bullet_type (1), is_locked (1), can_lavawalk (1), walk_dir (1),
  // last_touch_dir (1), last_shot_dir (1), xpos (2), ypos (2),
  // status (1), blank (2), used (1), loop_count (2),
  if(mfseek(mf, 41 - 2, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  if(program_size)
  {
    mfopen(mf->current, program_size, &prog);

    ret = parse_legacy_bytecode(&prog, program_size, file, board_num, robot_num);
  }

  mfseek(mf, program_size, SEEK_CUR);
  return ret;
}

static boolean skip_rle(struct memfile *mf)
{
  uint16_t w = mfgetw(mf);
  uint16_t h = mfgetw(mf);
  int pos = 0;

  /* RLE "decoder"; just to skip stuff */
  while(pos < w * h && mf->current < mf->end)
  {
    unsigned char c = (unsigned char)mfgetc(mf);

    if(!(c & 0x80))
      pos++;
    else
    {
      c &= ~0x80;
      pos += c;
      mfgetc(mf);
    }
  }
  return (mf->current < mf->end);
}

static boolean load_rle(char **dest, uint16_t *width, uint16_t *height,
 struct memfile *mf)
{
  uint16_t w = mfgetw(mf);
  uint16_t h = mfgetw(mf);
  size_t size = w * h;
  size_t runsize;
  size_t i;
  char *plane;

  if(size > MAX_BOARD_SIZE)
    return false;

  plane = cmalloc(size);

  for(i = 0; i < size && mf->current < mf->end; i++)
  {
    unsigned char c = (unsigned char)mfgetc(mf);
    if(!(c & 0x80))
    {
      // Regular character
      plane[i] = (char)c;
    }
    else
    {
      // A run
      runsize = (c & 0x7F);
      if((i + runsize) > size)
      {
        free(plane);
        return false;
      }

      memset(plane + i, mfgetc(mf), runsize);
      i += (runsize - 1);
    }
  }

  if(mf->current < mf->end)
  {
    *dest = plane;
    *width = w;
    *height = h;
    return true;
  }

  free(plane);
  return false;
}

static enum status parse_legacy_board(struct memfile *mf,
 struct base_file *file, int board_num)
{
  int i, num_robots, skip_bytes;
  unsigned short board_mod_len;
  enum status ret = SUCCESS;
  // NOTE: the higher of the two possible MAX_PATH values that might have been
  // used to store this.
  char board_mod[512];
  char *level_id = NULL;
  char *level_param = NULL;
  uint16_t width;
  uint16_t height;

  // junk the undocumented (and unused) board_mode
  mfgetc(mf);

  // whether the overlay is enabled or not
  if(mfgetc(mf))
  {
    // not enabled, so rewind this last read
    if(mfseek(mf, -1, SEEK_CUR) != 0)
      return FSEEK_FAILED;
  }
  else
  {
    // junk overlay_mode
    mfgetc(mf);

    // Skip overlay char and color RLE blocks.
    if(!skip_rle(mf) || !skip_rle(mf))
    {
      warnhere("Failed to unpack overlay RLE\n");
      return CORRUPT_WORLD;
    }
  }

  // NOTE: Robot xpos and ypos variables were always set to 0 in DOS-era worlds
  // and can't be trusted. Instead, the level_id/level_param arrays need to be
  // unpacked and scanned.

  // level_id, level_color, level_param,
  // level_under_id, level_under_color, level_under_param
  if( !load_rle(&level_id, &width, &height, mf)
   || !skip_rle(mf)
   || !load_rle(&level_param, &width, &height, mf)
   || !skip_rle(mf)
   || !skip_rle(mf)
   || !skip_rle(mf))
  {
    warnhere("Failed to unpack board RLE\n");
    free(level_id);
    free(level_param);
    return CORRUPT_WORLD;
  }

  if(level_id && level_param)
  {
    for(i = 0; i < (int)(width * height); i++)
    {
      if(is_robot((enum thing)level_id[i]))
      {
        unsigned char param = (unsigned char)level_param[i];
        robot_xpos[param] = (i % width);
        robot_ypos[param] = (i / width);
      }
    }
  }
  free(level_id);
  free(level_param);

  // get length of board MOD string
  if(file->world_version < V283)
    board_mod_len = 13;
  else
    board_mod_len = mfgetw(mf);

  // In practice, the board mod could never be longer than this.
  // If it is, something's wrong with the world file (e.g. 2.83 beta worlds
  // have the 2.83 magic but expect a length of 12)
  if(board_mod_len >= sizeof(board_mod))
  {
    warnhere("Board mod length invalid (%d)\n", board_mod_len);
    return CORRUPT_WORLD;
  }

  // grab board's default MOD
  if(mfread(board_mod, 1, board_mod_len, mf) != board_mod_len)
  {
    warnhere("Failed to read board mod (truncated)\n");
    return CORRUPT_WORLD;
  }

  board_mod[board_mod_len] = '\0';

  // check the board MOD exists
  if(strlen(board_mod) > 0 && strcmp(board_mod, "*"))
  {
    debug("BOARD MOD: %s\n", board_mod);
    add_requirement_board(board_mod, file, board_num, IS_BOARD_MOD);
  }

  if(file->world_version < V283)
    skip_bytes = 207;
  else
    skip_bytes = 25;

  // skip to the robot count
  if(mfseek(mf, skip_bytes, SEEK_CUR) != 0)
  {
    warnhere("Failed to seek to start of robots\n");
    return CORRUPT_WORLD;
  }

  // walk the robot list, scan the robotic
  num_robots = mfgetc(mf);
  for(i = 0; i < num_robots; i++)
  {
    ret = parse_legacy_robot(mf, file, board_num, i + 1);
    if(ret != SUCCESS)
    {
      warnhere("Failed processing robot %d\n", i + 1);
      break;
    }
  }

  return ret;
}

struct legacy_board {
  int offset;
  int size;
};

static enum status parse_legacy_world(struct memfile *mf,
 struct base_file *file)
{
  int global_robot_offset;
  struct legacy_board board_list[MAX_BOARDS];
  int num_boards;
  int i;

  enum status ret = SUCCESS;

  // Jump to the global robot offset
  if(mfseek(mf, LEGACY_WORLD_GLOBAL_OFFSET_OFFSET, SEEK_SET) != 0)
  {
    warnhere("couldn't seek to global robot position (truncated)\n");
    return CORRUPT_WORLD;
  }

  // Absolute offset (in bytes) of global robot
  global_robot_offset = mfgetd(mf);

  // num_boards doubles as the check for custom sfx, if zero
  num_boards = mfgetc(mf);

  // if we have custom sfx, check them for resources
  if(num_boards == 0)
  {
    unsigned short sfx_len_total = mfgetw(mf);
    char sfx_buf[LEGACY_SFX_SIZE + 1];
    int sfx_len;

    debug("SFX table (input length: %d)\n", sfx_len_total);

    for(i = 0; i < NUM_SFX; i++)
    {
      sfx_len = mfgetc(mf);
      if(sfx_len > LEGACY_SFX_SIZE)
      {
        warnhere("invalid SFX length of %d\n", sfx_len);
        return CORRUPT_WORLD;
      }

      if(sfx_len > 0)
      {
        if(mfread(sfx_buf, 1, sfx_len, mf) != (size_t)sfx_len)
        {
          warnhere("couldn't read SFX %d (truncated)\n", i);
          return CORRUPT_WORLD;
        }
        sfx_buf[sfx_len] = 0;

        ret = parse_sfx(sfx_buf, file, IS_SFX, i, -1);
        if(ret != SUCCESS)
        {
          warnhere("error parsing SFX %d\n", i);
          return ret;
        }
      }

      // 1 for length byte + sfx string
      sfx_len_total -= 1 + sfx_len;
    }

    // better check we moved by whole amount
    if(sfx_len_total != 0)
    {
      warnhere("Failed sfx total check: remaining is %d\n", sfx_len_total);
      return CORRUPT_WORLD;
    }

    // read the "real" num_boards
    num_boards = mfgetc(mf);
  }

  // skip board names; we simply don't care
  if(mfseek(mf, num_boards * BOARD_NAME_SIZE, SEEK_CUR) != 0)
  {
    warnhere("Failed to skip board names (truncated)\n");
    return CORRUPT_WORLD;
  }

  // grab the board sizes/offsets
  for(i = 0; i < num_boards; i++)
  {
    board_list[i].size = mfgetd(mf);
    board_list[i].offset = mfgetd(mf);
  }

  // walk all boards
  for(i = 0; i < num_boards; i++)
  {
    struct legacy_board *board = &board_list[i];

    debug("Board %d\n", i);

    // don't care about deleted boards
    if(board->size == 0)
      continue;

    // seek to board offset within world
    if(mfseek(mf, board->offset, SEEK_SET) != 0)
    {
      warnhere("Failed to seek to position of board %d\n", i);
      return CORRUPT_WORLD;
    }

    // parse this board atomically
    ret = parse_legacy_board(mf, file, i);
    if(ret != SUCCESS)
    {
      warnhere("Failed processing board %d\n", i);
      goto err_out;
    }
  }

  debug("Global robot\n");

  // Do the global robot too..
  if(mfseek(mf, global_robot_offset, SEEK_SET) != 0)
  {
    warnhere("Failed to seek to global robot position\n");
    return CORRUPT_WORLD;
  }

  ret = parse_legacy_robot(mf, file, NO_BOARD, GLOBAL_ROBOT);
  if(ret != SUCCESS)
    warnhere("Failed processing global robot\n");

err_out:
  return ret;
}


/*****************/
/* Modern Worlds */
/*****************/

static enum status parse_robot_info(struct memfile *mf, struct base_file *file,
 int board_num, int robot_num)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, mf))
  {
    switch(ident)
    {
      // These vars are more reliable in ZIP worlds than they were prior,
      // and can be trusted instead of scanning the board data.
      case RPROP_XPOS:
        robot_xpos[(unsigned char)robot_num] = load_prop_int(len, &prop);
        break;

      case RPROP_YPOS:
        robot_ypos[(unsigned char)robot_num] = load_prop_int(len, &prop);
        break;

      case RPROP_PROGRAM_BYTECODE:
        return parse_legacy_bytecode(&prop, (unsigned int)len, file,
         board_num, robot_num);
    }
  }

  return SUCCESS;
}

static enum status parse_board_info(struct memfile *mf, struct base_file *file,
 int board_num)
{
  struct memfile prop;
  int ident;
  int len;

  char buffer[MAX_PATH+1];

  while(next_prop(&prop, &ident, &len, mf))
  {
    switch(ident)
    {
      case BPROP_MOD_PLAYING:
      {
        // No * for the mod
        if(len && *(prop.start) != '*')
        {
          mfread(buffer, len, 1, &prop);
          buffer[len] = 0;

          debug("BOARD MOD: %s\n", buffer);
          add_requirement_board(buffer, file, board_num, IS_BOARD_MOD);
        }
        break;
      }
      case BPROP_CHARSET_PATH:
      case BPROP_PALETTE_PATH:
      {
        if(len)
        {
          mfread(buffer, len, 1, &prop);
          buffer[len] = 0;

          debug("BOARD CHR/PAL: %s\n", buffer);
          add_requirement_board(buffer, file, board_num,
           (ident == BPROP_CHARSET_PATH) ? IS_BOARD_CHARSET : IS_BOARD_PALETTE);
        }
        break;
      }
    }
  }

  return SUCCESS;
}

static enum status parse_sfx_file(struct memfile *mf, struct base_file *file)
{
  char sfx_buf[LEGACY_SFX_SIZE + 1];
  int i;

  enum status ret = SUCCESS;

  debug("SFX table found\n");

  for(i = 0; i < NUM_SFX; i++)
  {
    if(!mfread(sfx_buf, LEGACY_SFX_SIZE, 1, mf))
      return FREAD_FAILED;

    sfx_buf[LEGACY_SFX_SIZE] = 0;

    ret = parse_sfx(sfx_buf, file, IS_SFX, i, -1);
    if(ret != SUCCESS)
      return ret;
  }

  return SUCCESS;
}

static enum status parse_world(struct memfile *mf, struct base_file *file,
 int not_a_world)
{
  struct zip_archive *zp = zip_open_mem_read(mf->start,
   mf->end - mf->start);

  char *buffer = NULL;
  size_t actual_size;
  size_t allocated_size = 0;
  struct memfile buf_file;
  unsigned int file_id;
  unsigned int board_id;
  unsigned int robot_id;

  enum status ret = SUCCESS;

  if(!zp)
  {
    ret = CORRUPT_WORLD;
    goto err_close;
  }

  assign_fprops(zp, not_a_world);

  while(ZIP_SUCCESS == zip_get_next_prop(zp, &file_id, &board_id, &robot_id))
  {
    switch(file_id)
    {
      case FPROP_WORLD_GLOBAL_ROBOT:
      case FPROP_BOARD_INFO:
      case FPROP_ROBOT:
      case FPROP_WORLD_SFX:
      {
        zip_get_next_uncompressed_size(zp, &actual_size);
        if(allocated_size < actual_size)
        {
          allocated_size = actual_size;
          buffer = crealloc(buffer, allocated_size);
        }

        zip_read_file(zp, buffer, actual_size, &actual_size);
        mfopen(buffer, actual_size, &buf_file);

        if(file_id == FPROP_BOARD_INFO)
        {
          debug("Board %u\n", board_id);
          ret = parse_board_info(&buf_file, file, (int)board_id);
        }
        else

        if(file_id == FPROP_ROBOT)
        {
          ret = parse_robot_info(&buf_file, file, (int)board_id, (int)robot_id);
        }
        else

        if(file_id == FPROP_WORLD_GLOBAL_ROBOT)
        {
          debug("Global robot\n");
          ret = parse_robot_info(&buf_file, file, NO_BOARD, GLOBAL_ROBOT);
        }
        else

        if(file_id == FPROP_WORLD_SFX)
        {
          debug("SFX table\n");
          ret = parse_sfx_file(&buf_file, file);
        }

        break;
      }

      default:
      {
        zip_skip_file(zp);
        break;
      }
    }

    if(ret != SUCCESS)
      goto err_close;
  }

err_close:
  zip_close(zp, NULL);
  free(buffer);
  return ret;
}


/******************/
/* Common Formats */
/******************/

// same as internal boards except for a 4 byte magic header

static enum status parse_board_file(struct memfile *mf, struct base_file *file)
{
  unsigned char magic[4];
  int file_version;

  if(mfread(magic, 1, 4, mf) != 4)
    return FREAD_FAILED;

  file_version = board_magic(magic);
  if(file_version <= 0)
    return MAGIC_CHECK_FAILED;

  file->world_version = file_version;

  if(file_version <= MZX_LEGACY_FORMAT_VERSION)
    return parse_legacy_board(mf, file, -1);

  if(file_version <= MZX_VERSION)
    return parse_world(mf, file, 1);

  return MAGIC_CHECK_FAILED;
}

static enum status parse_world_file(struct memfile *mf, struct base_file *file)
{
  unsigned char magic[3];
  int file_version;

  // skip to protected byte; don't care about world name
  if(mfseek(mf, LEGACY_WORLD_PROTECTED_OFFSET, SEEK_SET) != 0)
    return FSEEK_FAILED;

  // can't yet support protected worlds
  if(mfgetc(mf) != 0)
    return PROTECTED_WORLD;

  // read in world magic (version)
  if(mfread(magic, 1, 3, mf) != 3)
    return FREAD_FAILED;

  // can only support 2.00+ versioned worlds
  file_version = _world_magic(magic);
  if(file_version <= 0)
    return MAGIC_CHECK_FAILED;

  file->world_version = file_version;

  if(file_version <= MZX_LEGACY_FORMAT_VERSION)
    return parse_legacy_world(mf, file);

  if(file_version <= MZX_VERSION)
    return parse_world(mf, file, 0);

  return MAGIC_CHECK_FAILED;
}

static char *load_file(FILE *fp, size_t *buf_size)
{
  char *buffer;

  fseek(fp, 0, SEEK_END);
  *buf_size = ftell(fp);
  rewind(fp);

  buffer = cmalloc(*buf_size);
  *buf_size = fread(buffer, 1, *buf_size, fp);

  return buffer;
}

static enum status parse_file(const char *file_name,
 struct base_path **path_list, int path_list_size)
{
  char file_dir[MAX_PATH];

  int path_list_alloc = path_list_size;

  struct base_file **file_list = NULL;
  struct base_file *current_file;
  int file_list_alloc = 0;
  int file_list_size = 0;

  int len;
  char *ext;

  struct memfile mf;
  char *buffer = NULL;
  size_t buf_size;
  FILE *fp;
  int i;

  enum status ret = SUCCESS;

  fp = fopen_unsafe(file_name, "rb");
  len = strlen(file_name);
  ext = len >= 4 ? (char *)file_name + len - 4 : NULL;

  _get_path(file_dir, file_name);

  if(fp && ext && !strcasecmp(ext, ".MZX"))
  {
    buffer = load_file(fp, &buf_size);
    fclose(fp);

    // Add the containing directory as a base path
    add_base_path(file_dir, &path_list, &path_list_size, &path_list_alloc);

    current_file = add_base_file(file_name,
     &file_list, &file_list_size, &file_list_alloc);
    mfopen(buffer, buf_size, &mf);

    ret = parse_world_file(&mf, current_file);
    free(buffer);
  }
  else

  if(fp && ext && !strcasecmp(ext, ".MZB"))
  {
    buffer = load_file(fp, &buf_size);
    fclose(fp);

    // Add the containing directory as a base path
    add_base_path(file_dir, &path_list, &path_list_size, &path_list_alloc);

    current_file = add_base_file(file_name,
     &file_list, &file_list_size, &file_list_alloc);
    mfopen(buffer, buf_size, &mf);

    ret = parse_board_file(&mf, current_file);
    free(buffer);
  }
  else

  if(fp && ext && !strcasecmp(ext, ".ZIP"))
  {
    struct base_path *zip_base;
    struct zip_archive *zp;

    size_t actual_size;
    char name_buffer[MAX_PATH];

    fclose(fp);

    // Add the zip as a base path
    // This will open the zip and read its directory

    zip_base = add_base_path(file_name, &path_list,
     &path_list_size, &path_list_alloc);

    if(!zip_base)
      return ZIP_FAILED;

    zp = zip_base->zp;

    // Iterate the ZIP
    while(ZIP_SUCCESS == zip_get_next_uncompressed_size(zp, &actual_size))
    {
      zip_get_next_name(zp, name_buffer, MAX_PATH-1);

      len = strlen(name_buffer);
      ext = len >= 4 ? (char *)name_buffer + len - 4 : NULL;

      if(ext && (!strcasecmp(ext, ".MZX") || !strcasecmp(ext, ".MZB")))
      {
        buffer = ccalloc(1, actual_size);
        if(ZIP_SUCCESS != zip_read_file(zp, buffer, actual_size, &actual_size))
        {
          warn("Error processing '%s': %s\n\n", name_buffer,
           decode_status(ZIP_FAILED));
          free(buffer);
          continue;
        }

        current_file = add_base_file(name_buffer,
         &file_list, &file_list_size, &file_list_alloc);

        // Files in zips need a relative path.
        _get_path(current_file->relative_path, name_buffer);

        mfopen(buffer, actual_size, &mf);

        if(!strcasecmp(ext, ".MZX"))
          ret = parse_world_file(&mf, current_file);
        else
          ret = parse_board_file(&mf, current_file);

        if(ret != SUCCESS)
        {
          // Keep going; other files in the archive may not be corrupt.
          warn("Error processing '%s': %s\n\n", name_buffer, decode_status(ret));
          ret = SUCCESS;
        }
        free(buffer);
      }

      else
      {
        zip_skip_file(zp);
      }
    }

    // The file and zip will be closed in clear_data().
  }
  else

  // Try to open the file as a directory.
  if(add_base_files_from_path(file_name, &file_list, &file_list_size,
   &file_list_alloc) && file_list_size)
  {
    struct base_path *dir_base = add_base_path(file_name, &path_list,
     &path_list_size, &path_list_alloc);
    char name_buffer[MAX_PATH];

    if(fp) fclose(fp);
    if(!dir_base)
      return DIRENT_FAILED;

    for(i = 0; i < file_list_size; i++)
    {
      current_file = file_list[i];
      join_path(name_buffer, file_name, current_file->file_name);

      fp = fopen_unsafe(name_buffer, "rb");
      len = strlen(current_file->file_name);
      ext = len >= 4 ? current_file->file_name + len - 4 : NULL;
      if(fp)
      {
        // NOTE: the relative paths of these are automatically added in
        // add_base_files_from_path. The base file itself doesn't need a
        // relative path because it's being created as the cwd.
        buffer = load_file(fp, &buf_size);
        fclose(fp);

        mfopen(buffer, buf_size, &mf);

        if(ext && !strcasecmp(ext, ".MZX"))
          ret = parse_world_file(&mf, current_file);
        else

        if(ext && !strcasecmp(ext, ".MZB"))
          ret = parse_board_file(&mf, current_file);

        free(buffer);

        if(ret != SUCCESS)
        {
          // Keep going; other files in the path may not be corrupt.
          warn("Error processing '%s': %s\n", current_file->file_name,
           decode_status(ret));
          ret = SUCCESS;
        }
      }
      else
        warn("Failed to open '%s' for reading\n", current_file->file_name);
    }
  }

  else
  {
    if(fp) fclose(fp);
    fprintf(stderr,
      "'%s' is not a .MZX (world), .MZB (board),"
      "directory containing .MZX/.MZB files, or .ZIP (archive) file.\n",
      file_name
    );
    return INVALID_ARGUMENTS;
  }

  process_requirements(path_list, path_list_size);

  clear_data(path_list, path_list_size,
   file_list, file_list_size);

  if(!display_filename_only)
   fprintf(stdout, "\nFinished processing '%s'.\n\n", file_name);

  fflush(stdout);

  return ret;
}

int main(int argc, char *argv[])
{
  struct base_path **path_list = NULL;
  int path_list_alloc = 0;
  int path_list_size = 0;
  enum status ret = SUCCESS;
  int i;

  struct base_path *current_path = NULL;
  char *file_name = NULL;
  char *param;

#ifdef CONFIG_PLEDGE_UTILS
  // Hard to predict where this will read ahead of time, so no unveil right now.
  if(pledge(PROMISES, ""))
  {
    fprintf(stderr, "ERROR: Failed pledge!\n");
    return 1;
  }
#endif

  if(argc < 2)
  {
    fprintf(stderr, USAGE);
    return INVALID_ARGUMENTS;
  }

  for(i = 1; i < argc; i++)
  {
    if(argv[i][0] == '-')
    {
      param = argv[i] + 1;

      if(!strcmp(param, "extra"))
      {
        if(file_name)
        {
          i++;
          if(i < argc)
          {
            current_path = add_base_path(argv[i], &path_list, &path_list_size,
             &path_list_alloc);
          }
          else
          {
            fprintf(stderr, "ERROR: expected path or zip file\n");
            return INVALID_ARGUMENTS;
          }
        }
        else
        {
          fprintf(stderr, "ERROR: -extra must follow base file\n");
          return INVALID_ARGUMENTS;
        }
      }
      else
      if(!strcmp(param, "in"))
      {
        if(current_path)
        {
          i++;
          if(i < argc)
          {
            change_base_path_dir(current_path, argv[i]);
            current_path = NULL;
          }
          else
          {
            fprintf(stderr, "ERROR: expected relative path\n");
            return INVALID_ARGUMENTS;
          }
        }
        else
        {
          fprintf(stderr, "ERROR: -in must follow extra\n");
          return INVALID_ARGUMENTS;
        }
      }
      else

      while(*param)
      {
        switch(*param)
        {
          case 'h':
            fprintf(stderr, USAGE);
            return SUCCESS;

          case 'A': // Legacy equivalent of -Mc
            display_not_found = true;
            display_found = false;
            display_created = true;
            display_unused = false;
            display_wildcard = false;
            break;

          case 'C':
            display_not_found = false;
            display_found = false;
            display_created = true;
            display_unused = false;
            display_wildcard = false;
            break;

          case 'F':
            display_not_found = false;
            display_found = true;
            display_created = false;
            display_unused = false;
            display_wildcard = false;
            break;

          case 'M':
            display_not_found = true;
            display_found = false;
            display_created = false;
            display_unused = false;
            display_wildcard = false;
            break;

          case 'U':
            display_not_found = false;
            display_found = false;
            display_created = false;
            display_unused = true;
            display_wildcard = false;
            break;

          case 'W':
            display_not_found = false;
            display_found = false;
            display_created = false;
            display_unused = false;
            display_wildcard = true;
            break;

          case 'a':
            display_not_found = true;
            display_found = true;
            display_created = true;
            display_unused = true;
            display_wildcard = true;
            break;

          case 'c':
            display_created = true;
            break;

          case 'f':
            display_found = true;
            break;

          case 'm':
            display_not_found = true;
            break;

          case 'u':
            display_unused = true;
            break;

          case 'w':
            display_wildcard = true;
            break;

          case 's':
            display_filename_only = false;
            display_first_only = true;
            display_details = false;
            display_all_details = false;
            break;

          case 'v':
          {
            if(param[1] == 'v')
            {
              display_all_details = true;
              param++;
            }
            else
              display_all_details = false;

            display_filename_only = false;
            display_first_only = false;
            display_details = true;
            break;
          }

          case '1':
          case 'q': // Legacy equivalent of -1
            display_first_only = true;
            display_filename_only = true;
            display_details = false;
            display_all_details = false;
            break;

          case 'L':
            sort_by = SORT_BY_LOCATION;
            break;

          case 'N':
            sort_by = SORT_BY_FILENAME;
            break;

          default:
            fprintf(stderr, "Unrecognized argument '%c'\n", *param);
            return INVALID_ARGUMENTS;
        }
        param++;
      }
    }

    else
    {
      // Parse the previous base file.
      if(file_name)
      {
        ret = parse_file(file_name, path_list, path_list_size);

        path_list = NULL;
        path_list_size = 0;
        path_list_alloc = 0;

        if(ret != SUCCESS)
          break;
      }
      file_name = argv[i];
      current_path = NULL;
    }
  }

  // Parse the final base file
  if(file_name && ret == SUCCESS)
  {
    ret = parse_file(file_name, path_list, path_list_size);
  }

  if(ret != SUCCESS)
  {
    fprintf(stderr, "ERROR: %s\n", decode_status(ret));
  }

  fflush(stderr);
  return ret;
}
