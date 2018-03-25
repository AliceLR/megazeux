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

#define USAGE \
 "Usage: checkres [-a -A -h -q] " \
 "mzx/mzb/zip file [-extra path/zip file [-in relative path] ...] ... \n" \
 "  -a  Display all found and missing files.\n"                           \
 "  -A  Display missing files and files that may be created by Robotic.\n"\
 "  -h  Display this message.\n"                                          \
 "  -q  Display only file paths i.e. no extra info.\n"                    \
 "\n"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __WIN32__
#include <strings.h>
#endif

#include <utcasehash.h>

// From MZX itself:
#define SKIP_SDL

// Safe- self sufficient or completely macros/static inlines
#include "../const.h"
#include "../memfile.h"
#include "../world_prop.h"
#include "../zip.h"

// Avoid CORE_LIBSPEC functions
#include "../fsafeopen.h"
#include "../util.h"
#include "../world.h"

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

static const char *found_append = "FOUND";
static const char *created_append = "CREATED";
static const char *not_found_append = "NOT FOUND";

static int quiet_mode = 0;
static int display_found = 0;
static int display_created = 0;

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
  ZIP_FAILED,
  NO_WORLD,
  MISSING_FILE
};

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
      strcpy(dest, dir);
  }

  else
  {
    strcpy(dest, file);
  }

  dest[MAX_PATH - 1] = 0;
}

static bool is_simple_path(char *src)
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

    if(src[i] == '&')
      if(i+1 == len || src[i+1] != '&')
        return false;

    // No expressions.
    if(src[i] == '(')
      return false;
  }

  return true;
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

static int started_table = 0;
static unsigned int parent_max_len = 12;
static unsigned int resource_max_len = 16;

static void output(const char *required_by, const char *resource_path,
 const char *status, const char *found_in)
{
  if(!quiet_mode)
  {
    char *req_by = malloc(parent_max_len + 1);
    char *res_path = malloc(resource_max_len + 1);

    req_by[parent_max_len] = 0;
    res_path[resource_max_len] = 0;

    found_in = found_in ? found_in : "";

    if(!started_table)
    {
      fprintf(stdout, "\n");

      memset(req_by, 32, parent_max_len);
      memset(res_path, 32, resource_max_len);

      strncpy(req_by, "Required by", strlen("Required by"));
      strncpy(res_path, "Resource path", strlen("Resource path"));

      fprintf(stdout, "%s  %s  %-10s %s\n",
       req_by, res_path, "Status", "Found in");

      memset(req_by, 32, parent_max_len);
      memset(res_path, 32, resource_max_len);

      strncpy(req_by, "-----------", strlen("-----------"));
      strncpy(res_path, "-------------", strlen("-------------"));

      fprintf(stdout, "%s  %s  %-10s %s\n",
       req_by, res_path, "------", "--------");

      started_table = 1;
    }

    memset(req_by, 32, parent_max_len);
    memset(res_path, 32, resource_max_len);

    strncpy(req_by, required_by, strlen(required_by));
    strncpy(res_path, resource_path, strlen(resource_path));

    fprintf(stdout, "%s  %s  %-10s %s\n",
     req_by, res_path, status, found_in);
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

struct zip_file_path {
  char file_path[MAX_PATH];
  UT_hash_handle hh;
};

struct base_path {
  char actual_path[MAX_PATH];
  char relative_path[MAX_PATH];
  struct zip_archive *zp;
  struct zip_file_path *file_list_head;
};

struct base_file {
  char file_name[MAX_PATH];
  char relative_path[MAX_PATH];
  int world_version;
};

struct resource {
  char path[MAX_PATH];
  struct base_file *parent;
  UT_hash_handle hh;
};

// NULL is important
static struct resource *requirement_head = NULL;
static struct resource *resource_head = NULL;

static struct resource *add_requirement(char *src, struct base_file *file)
{
  // A resource file required by a world/board.

  struct resource *req = NULL;
  char file_buffer[MAX_PATH];

  // Offset the required file's path with the relative path of its parent
  join_path(file_buffer, file->relative_path, src);

  HASH_FIND_STR(requirement_head, file_buffer, req);

  if(!req && is_simple_path(src))
  {
    req = malloc(sizeof(struct resource));

    strcpy_fsafe(req->path, file_buffer);
    req->parent = file;

    // Calculate these values for the output table as we go along
    resource_max_len = MAX(resource_max_len, strlen(file_buffer));
    parent_max_len = MAX(parent_max_len, strlen(file->file_name));

    HASH_ADD_STR(requirement_head, path, req);
  }

  return req;
}

static struct resource *add_resource(char *src, struct base_file *file)
{
  // A filename found in Robotic code that may fulfill a requirement for a file.

  struct resource *res = NULL;
  char file_buffer[MAX_PATH];

  // Offset the required file's path with the relative path of its parent
  join_path(file_buffer, file->relative_path, src);

  HASH_FIND_STR(resource_head, file_buffer, res);

  if(!res && is_simple_path(src))
  {
    res = malloc(sizeof(struct resource));

    strcpy_fsafe(res->path, file_buffer);

    // Calculate these values for the output table as we go along
    resource_max_len = MAX(resource_max_len, strlen(file_buffer));
    parent_max_len = MAX(parent_max_len, strlen(file->file_name));

    HASH_ADD_STR(resource_head, path, res);
  }

  return res;
}

static void build_base_path_table(struct base_path *path,
 struct zip_archive *zp)
{
  struct zip_file_path *entry;
  struct zip_file_path *has_entry;

  struct zip_file_header **files = zp->files;
  struct zip_file_header *fh;
  int num_files = zp->num_files;
  int i;

  for(i = 0; i < num_files; i++)
  {
    fh = files[i];
    entry = malloc(sizeof(struct zip_file_path));
    strncpy(entry->file_path, fh->file_name, MAX_PATH);
    entry->file_path[MAX_PATH - 1] = 0;

    HASH_FIND_STR(path->file_list_head, entry->file_path, has_entry);

    if(!has_entry)
      HASH_ADD_STR(path->file_list_head, file_path, entry);
  }
}

static void change_base_path_dir(struct base_path *current_path,
 const char *new_relative_path)
{
  fsafetranslate(new_relative_path, current_path->relative_path);
}

static struct base_path *add_base_path(const char *path_name,
 struct base_path ***path_list, int *path_list_size, int *path_list_alloc)
{
  struct base_path *new_path = calloc(1, sizeof(struct base_path));
  int alloc = *path_list_alloc;
  int size = *path_list_size;

  if(!strcasecmp(path_name + strlen(path_name) - 4, ".ZIP"))
  {
    struct zip_archive *zp = zip_open_file_read(path_name);
    int result = zip_read_directory(zp);

    if(result != ZIP_SUCCESS)
    {
      free(new_path);
      return NULL;
    }

    build_base_path_table(new_path, zp);
    new_path->zp = zp;
  }

  strcpy(new_path->actual_path, path_name);

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
  struct base_file *new_file = calloc(1, sizeof(struct base_file));
  int alloc = *file_list_alloc;
  int size = *file_list_size;

  strcpy(new_file->file_name, path_name);

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

static void process_requirements(struct base_path **path_list,
 int path_list_size)
{
  struct stat stat_info;
  struct base_path *current_path;
  struct zip_file_path *zfp;
  struct resource *req;
  struct resource *res;
  struct resource *tmp;
  char path_buffer[MAX_PATH];
  char *translated_path;
  size_t len;
  int found;
  int i;

  // Now actually process the requirements
  HASH_ITER(hh, requirement_head, req, tmp)
  {
    found = 0;

    for(i = 0; i < path_list_size; i++)
    {
      current_path = path_list[i];
      len = strlen(current_path->relative_path);

      // The required resource's path must start with the relative path
      if(strncasecmp(req->path, current_path->relative_path, len))
        continue;

      translated_path = req->path + len;

      if(current_path->zp)
      {
        // Try to find the file in the zip's hash table
        HASH_FIND_STR(current_path->file_list_head, translated_path, zfp);

        if(zfp)
        {
          found = 1;
          break;
        }
      }

      else
      {
        // Try to find an actual file
        join_path(path_buffer, current_path->actual_path, translated_path);

        if(!stat(path_buffer, &stat_info))
        {
          found = 1;
          break;
        }
      }
    }

    if(found)
    {
      if(display_found)
        output(req->parent->file_name, req->path, found_append,
         current_path->actual_path);
    }
    else
    {
      // Try to find in the created resources table
      HASH_FIND_STR(resource_head, req->path, res);

      if(res)
      {
        if(display_created)
          output(req->parent->file_name, req->path, created_append, NULL);
      }
      else
      {
        output(req->parent->file_name, req->path, not_found_append, NULL);
      }
    }
  }

  // Reset these for next time
  started_table = 0;
  parent_max_len = 12;
  resource_max_len = 16;
}

static void clear_data(struct base_path **path_list,
 int path_list_size, struct base_file **file_list, int file_list_size)
{
  struct base_path *bp;
  struct resource *res;
  struct resource *tmp;
  struct zip_file_path *fp;
  struct zip_file_path *tmpfp;
  int i;

  HASH_ITER(hh, requirement_head, res, tmp)
  {
    HASH_DELETE(hh, requirement_head, res);
    free(res);
  }
  requirement_head = NULL;

  HASH_ITER(hh, resource_head, res, tmp)
  {
    HASH_DELETE(hh, resource_head, res);
    free(res);
  }
  resource_head = NULL;

  for(i = 0; i < path_list_size; i++)
  {
    bp = path_list[i];

    HASH_ITER(hh, bp->file_list_head, fp, tmpfp)
    {
      HASH_DELETE(hh, bp->file_list_head, fp);
      free(fp);
    }

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
          return V251;
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

static enum status parse_sfx(char *sfx_buf, struct base_file *file)
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

    add_requirement(start + 1, file);
  }

  return ret;
}

#define match(s, reqv) ((world_version >= reqv) && \
 (fn_len == sizeof(s)-1) && (!strcasecmp(function_counter, s)))

#define match_partial(s, reqv) ((world_version >= reqv) && \
 (fn_len >= sizeof(s)-1) && (!strncasecmp(function_counter, s, fn_len)))


static enum status parse_legacy_bytecode(struct memfile *mf,
 unsigned int program_size, struct base_file *file)
{
  int world_version = file->world_version;
  enum status ret = SUCCESS;
  int command_length;
  int command;

  char function_counter[256];
  size_t fn_len;

  char src[256];
  size_t src_len;
  
  // skip 0xff marker
  if(mfgetc(mf) != 0xff)
    return CORRUPT_WORLD;

  while(1)
  {
    if(mftell(mf) >= (long int)program_size)
      return CORRUPT_WORLD;

    command_length = mfgetc(mf);
    if(command_length == 0)
      break;

    command = mfgetc(mf);

    // These constants may eventually change in debytecode versions.
    // Since this is for legacy bytecode, leave the fixed numbers.
    switch(command)
    {
      case 10: // ROBOTIC_CMD_SET
      {
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
          return FREAD_FAILED;

        fn_len = mfgetc(mf);

        // Int parameter
        if(fn_len == 0)
        {
          mfgetw(mf);
          break;
        }

        // String parameter
        if(mfread(function_counter, 1, fn_len, mf) != fn_len)
          return FREAD_FAILED;

        // Subtract off the null terminator
        fn_len--;

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
          add_requirement(src, file);
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
          return FREAD_FAILED;

        // ignore MOD *
        if(!strcmp(src, "*"))
          break;

        // Clip filename.mod*
        if(src[src_len - 1] == '*')
          src[src_len - 1] = 0;

        debug("MOD: %s\n", src);
        add_requirement(src, file);
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
          return FREAD_FAILED;

        debug("SAM: %s\n", src);
        add_requirement(src, file);
        break;
      }

      case 43: // ROBOTIC_CMD_PLAY
      case 45: // ROBOITC_CMD_WAIT_THEN_PLAY
      case 49: // ROBOTIC_CMD_PLAY_IF_SILENT
      {
        // Play string
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
          return FREAD_FAILED;

        ret = parse_sfx(src, file);
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
            return FREAD_FAILED;

          fn_len = mfgetc(mf);

          // Thing
          if(fn_len == 0)
          {
            if(mfgetw(mf) == IMAGE_FILE && src[0] == '@')
            {
              debug("PUT @file: %s\n", src + 1);
              add_requirement(src + 1, file);
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
          return FREAD_FAILED;

        debug("MOD FADE IN: %s\n", src);
        add_requirement(src, file);
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
            return FREAD_FAILED;

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
        char *rest = src;

        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
          return FREAD_FAILED;

        if(src[0] == '+')
        {
          char tempc = src[3];

          src[3] = 0;
          strtol(src + 1, &rest, 16);
          src[3] = tempc;
        }

        else if(src[0] == '@')
        {
          char tempc;
          int maxlen;

          if(world_version < V290)
            maxlen = 3;
          else
            maxlen = 4;

          tempc = src[maxlen+1];
          src[maxlen+1] = 0;
          strtol(src + 1, &rest, 10);
          src[maxlen+1] = tempc;
        }

        debug("LOAD CHAR SET: %s\n", rest);
        add_requirement(rest, file);
        break;
      }

      case 222: // ROBOTIC_CMD_LOAD_PALETTE
      {
        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
          return FREAD_FAILED;

        debug("LOAD PALETTE: %s\n", src);
        add_requirement(src, file);
        break;
      }

      case 226: // ROBOTIC_CMD_SWAP_WORLD
      {
        // Filename
        src_len = mfgetc(mf);

        if(mfread(src, 1, src_len, mf) != src_len)
          return FREAD_FAILED;

        debug("SWAP WORLD: %s\n", src);
        add_requirement(src, file);
        break;
      }

      default:
      {
        if(mfseek(mf, command_length - 1, SEEK_CUR) != 0)
          return FSEEK_FAILED;
        break;
      }
    }

    if(ret != SUCCESS)
      return ret;

    if(mfgetc(mf) != command_length)
      return CORRUPT_WORLD;
  }

  return ret;
}


/*****************/
/* Legacy Worlds */
/*****************/

static enum status parse_legacy_robot(struct memfile *mf,
 struct base_file *file)
{
  unsigned int program_size;
  struct memfile prog;

  enum status ret = SUCCESS;

  // robot's size
  program_size = mfgetw(mf);

  // skip to robot code
  if(mfseek(mf, 40 - 2 + 1, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  if(program_size)
  {
    mfopen_static(mf->current, program_size, &prog);

    ret = parse_legacy_bytecode(&prog, program_size, file);
  }

  mfseek(mf, program_size, SEEK_CUR);
  return ret;
}

static enum status parse_legacy_board(struct memfile *mf,
 struct base_file *file)
{
  int i, num_robots, skip_rle_blocks = 6, skip_bytes;
  unsigned short board_mod_len;
  enum status ret = SUCCESS;
  char board_mod[MAX_PATH];

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
    skip_rle_blocks += 2;
  }

  // this skips either 6 blocks (with no overlay)
  // ..or 8 blocks (with overlay enabled on board)
  for(i = 0; i < skip_rle_blocks; i++)
  {
    unsigned short w = mfgetw(mf);
    unsigned short h = mfgetw(mf);
    int pos = 0;

    /* RLE "decoder"; just to skip stuff */
    while(pos < w * h)
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
  }

  // get length of board MOD string
  if(file->world_version < V283)
    board_mod_len = 12;
  else
    board_mod_len = mfgetw(mf);

  // grab board's default MOD
  if(mfread(board_mod, 1, board_mod_len, mf) != board_mod_len)
    return FREAD_FAILED;

  board_mod[board_mod_len] = '\0';

  // check the board MOD exists
  if(strlen(board_mod) > 0 && strcmp(board_mod, "*"))
  {
    debug("BOARD MOD: %s\n", board_mod);
    add_requirement(board_mod, file);
  }

  if(file->world_version < V283)
    skip_bytes = 208;
  else
    skip_bytes = 25;

  // skip to the robot count
  if(mfseek(mf, skip_bytes, SEEK_CUR) != 0)
    return FSEEK_FAILED;

  // walk the robot list, scan the robotic
  num_robots = mfgetc(mf);
  for(i = 0; i < num_robots; i++)
  {
    ret = parse_legacy_robot(mf, file);
    if(ret != SUCCESS)
      break;
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
    return FSEEK_FAILED;

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
        return CORRUPT_WORLD;

      if(sfx_len > 0)
      {
        if(mfread(sfx_buf, 1, sfx_len, mf) != (size_t)sfx_len)
          return FREAD_FAILED;
        sfx_buf[sfx_len] = 0;

        ret = parse_sfx(sfx_buf, file);
        if(ret != SUCCESS)
          return ret;
      }

      // 1 for length byte + sfx string
      sfx_len_total -= 1 + sfx_len;
    }

    // better check we moved by whole amount
    if(sfx_len_total != 0)
    {
      debug("Failed sfx total check: remaining is %d\n", sfx_len_total);
      return CORRUPT_WORLD;
    }

    // read the "real" num_boards
    num_boards = mfgetc(mf);
  }

  // skip board names; we simply don't care
  if(mfseek(mf, num_boards * BOARD_NAME_SIZE, SEEK_CUR) != 0)
    return FSEEK_FAILED;

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
      return FSEEK_FAILED;

    // parse this board atomically
    ret = parse_legacy_board(mf, file);
    if(ret != SUCCESS)
      goto err_out;
  }

  debug("Global robot\n");

  // Do the global robot too..
  if(mfseek(mf, global_robot_offset, SEEK_SET) != 0)
    return FSEEK_FAILED;

  ret = parse_legacy_robot(mf, file);

err_out:
  return ret;
}


/*****************/
/* Modern Worlds */
/*****************/

static enum status parse_robot_info(struct memfile *mf, struct base_file *file)
{
  struct memfile prop;
  int ident;
  int len;

  while(next_prop(&prop, &ident, &len, mf))
  {
    if(ident == RPROP_PROGRAM_BYTECODE)
      return parse_legacy_bytecode(&prop, (unsigned int)len, file);
  }

  return SUCCESS;
}

static enum status parse_board_info(struct memfile *mf, struct base_file *file)
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

          add_requirement(buffer, file);
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

          add_requirement(buffer, file);
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

    ret = parse_sfx(sfx_buf, file);
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

  char *buffer;
  size_t actual_size;
  struct memfile buf_file;
  unsigned int file_id;

  enum status ret = SUCCESS;

  if(ZIP_SUCCESS != zip_read_directory(zp))
  {
    ret = CORRUPT_WORLD;
    goto err_close;
  }

  assign_fprops(zp, not_a_world);

  while(ZIP_SUCCESS == zip_get_next_prop(zp, &file_id, NULL, NULL))
  {
    switch(file_id)
    {
      case FPROP_BOARD_INFO:
      case FPROP_ROBOT:
      case FPROP_WORLD_SFX:
      {
        zip_get_next_uncompressed_size(zp, &actual_size);
        buffer = malloc(actual_size);
        zip_read_file(zp, buffer, actual_size, &actual_size);
        mfopen_static(buffer, actual_size, &buf_file);

        if(file_id == FPROP_BOARD_INFO)
          ret = parse_board_info(&buf_file, file);

        else if(file_id == FPROP_ROBOT)
          ret = parse_robot_info(&buf_file, file);

        else if(file_id == FPROP_WORLD_SFX)
          ret = parse_sfx_file(&buf_file, file);

        free(buffer);
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
    return parse_legacy_board(mf, file);

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

  buffer = malloc(*buf_size);
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

  int len = strlen(file_name);
  char *ext;

  struct memfile mf;
  char *buffer = NULL;
  size_t buf_size;
  FILE *fp;

  enum status ret = SUCCESS;

  if(len < 4)
  {
    fprintf(stderr, "'%s' is not a valid input filename.\n", file_name);
    return INVALID_ARGUMENTS;
  }

  fp = fopen_unsafe(file_name, "rb");

  if(!fp)
  {
    fprintf(stderr, "'%s' could not be opened.\n", file_name);
    return FOPEN_FAILED;
  }

  len = strlen(file_name);
  ext = (char *)file_name + len - 4;

  _get_path(file_dir, file_name);

  if(!strcasecmp(ext, ".MZX"))
  {
    buffer = load_file(fp, &buf_size);
    fclose(fp);

    // Add the containing directory as a base path
    add_base_path(file_dir, &path_list, &path_list_size, &path_list_alloc);

    current_file = add_base_file(file_name,
     &file_list, &file_list_size, &file_list_alloc);
    mfopen_static(buffer, buf_size, &mf);

    ret = parse_world_file(&mf, current_file);
    free(buffer);
  }
  else

  if(!strcasecmp(ext, ".MZB"))
  {
    buffer = load_file(fp, &buf_size);
    fclose(fp);

    // Add the containing directory as a base path
    add_base_path(file_dir, &path_list, &path_list_size, &path_list_alloc);

    current_file = add_base_file(file_name,
     &file_list, &file_list_size, &file_list_alloc);
    mfopen_static(buffer, buf_size, &mf);

    ret = parse_board_file(&mf, current_file);
    free(buffer);
  }
  else

  if(!strcasecmp(ext, ".ZIP"))
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

    zp = zip_base->zp;

    // Iterate the ZIP
    while(ZIP_SUCCESS == zip_get_next_uncompressed_size(zp, &actual_size))
    {
      zip_get_next_name(zp, name_buffer, MAX_PATH-1);

      len = strlen(name_buffer);
      ext = (char *)name_buffer + len - 4;

      if(!strcasecmp(ext, ".MZX"))
      {
        buffer = malloc(actual_size);
        zip_read_file(zp, buffer, actual_size, &actual_size);

        current_file = add_base_file(name_buffer,
         &file_list, &file_list_size, &file_list_alloc);

        // Files in zips need a relative path.
        _get_path(current_file->relative_path, name_buffer);

        mfopen_static(buffer, actual_size, &mf);

        // FIXME do something with ret
        ret = parse_world_file(&mf, current_file);
        free(buffer);
      }
      else

      if(!strcasecmp(ext, ".MZB"))
      {
        buffer = malloc(actual_size);
        zip_read_file(zp, buffer, actual_size, &actual_size);

        current_file = add_base_file(name_buffer,
         &file_list, &file_list_size, &file_list_alloc);

        // Files in zips need a relative path.
        _get_path(current_file->relative_path, name_buffer);

        mfopen_static(buffer, actual_size, &mf);

        // FIXME do something with ret
        ret = parse_board_file(&mf, current_file);
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
  {
    fclose(fp);
    fprintf(stderr, "'%s' is not a .MZX (world), .MZB (board) "
                    "or .ZIP (archive) file.\n", file_name);
    return INVALID_ARGUMENTS;
  }

  process_requirements(path_list, path_list_size);

  clear_data(path_list, path_list_size,
   file_list, file_list_size);

  if(!quiet_mode)
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
      if(!strcmp(param, "q"))
      {
        quiet_mode = 1;
      }
      else
      if(!strcmp(param, "a"))
      {
        display_found = 1;
        display_created = 1;
      }
      else
      if(!strcmp(param, "A"))
      {
        display_found = 0;
        display_created = 1;
      }
      else
      if(!strcmp(param, "h"))
      {
        if(i == 1)
        {
          fprintf(stderr, USAGE);
          return SUCCESS;
        }
      }
      else
      {
        fprintf(stderr, "Unrecognized argument: -%s\n", param);
        return INVALID_ARGUMENTS;
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

  return ret;
}
