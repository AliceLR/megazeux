/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2012 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

// Must include after <time.h> (MSVC bug)
#include "util.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "const.h" // for MAX_PATH
#include "error.h"
#include "memcasecmp.h" // memtolower

struct mzx_resource
{
  const char *const base_name;
  char *path;

  /* So mzxrun requires fewer files even in CONFIG_EDITOR=1 build */
  boolean editor_only;

  /* Optional resource--do not abort if not found, but print a warning. */
  boolean optional;
};

/* Using C99 initializers would be nicer here, but MSVC doesn't support
 * them and we try to allow compilation with that compiler.
 *
 * As a result, these must be in exactly the same order as the
 * enum resource_id enumeration defines them.
 */
static struct mzx_resource mzx_res[] =
{
#define ASSETS "assets/"
  { CONFFILE,                           NULL, false, false },
  { ASSETS "default.chr",               NULL, false, false },
  { ASSETS "edit.chr",                  NULL, false, false },
  { ASSETS "smzx.pal",                  NULL, false, false },
#ifdef CONFIG_EDITOR
  { ASSETS "ascii.chr",                 NULL, true,  false },
  { ASSETS "blank.chr",                 NULL, true,  false },
  { ASSETS "smzx.chr",                  NULL, true,  false },
#endif
#ifdef CONFIG_HELPSYS
  { ASSETS "help.fil",                  NULL, false, true },
#endif
#ifdef CONFIG_RENDER_GL_PROGRAM
#define GLSL_SHADERS ASSETS "glsl/"
#define GLSL_SCALERS GLSL_SHADERS "scalers/"
  { GLSL_SCALERS,                       NULL, false, false },
  { GLSL_SHADERS "scaler.vert",         NULL, false, false },
  { GLSL_SCALERS "semisoft.frag",       NULL, false, false },
  { GLSL_SHADERS "tilemap.vert",        NULL, false, false },
  { GLSL_SHADERS "tilemap.frag",        NULL, false, false },
  { GLSL_SHADERS "tilemap.smzx.frag",   NULL, false, false },
  { GLSL_SHADERS "mouse.vert",          NULL, false, false },
  { GLSL_SHADERS "mouse.frag",          NULL, false, false },
  { GLSL_SHADERS "cursor.vert",         NULL, false, false },
  { GLSL_SHADERS "cursor.frag",         NULL, false, false },
#endif
#ifdef CONFIG_GAMECONTROLLERDB
  { ASSETS "gamecontrollerdb.txt",      NULL, false, true },
#endif
};

#ifdef CONFIG_CHECK_ALLOC

static void out_of_memory_check(void *p, const char *file, int line)
{
  char msgbuf[128];
  if(!p)
  {
    snprintf(msgbuf, sizeof(msgbuf), "Out of memory in %s:%d", file, line);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    error(msgbuf, ERROR_T_FATAL, ERROR_OPT_EXIT|ERROR_OPT_NO_HELP, 0);
  }
}

void *check_calloc(size_t nmemb, size_t size, const char *file, int line)
{
  void *result = calloc(nmemb, size);
  out_of_memory_check(result, file, line);
  return result;
}

void *check_malloc(size_t size, const char *file, int line)
{
  void *result = malloc(size);
  out_of_memory_check(result, file, line);
  return result;
}

void *check_realloc(void *ptr, size_t size, const char *file, int line)
{
  void *result = realloc(ptr, size);
  out_of_memory_check(result, file, line);
  return result;
}

#endif /* CONFIG_CHECK_ALLOC */

int mzx_res_init(const char *argv0, boolean editor)
{
  size_t i, bin_path_len = 0;
  struct stat file_info;
  char *full_path_base;
  char *full_path;
  char *bin_path;
  ssize_t g_ret;
  char *p_dir;
  int ret = 0;

  full_path_base = cmalloc(MAX_PATH);
  bin_path = cmalloc(MAX_PATH);
  p_dir = cmalloc(MAX_PATH);

  g_ret = get_path(argv0, bin_path, MAX_PATH);
  if(g_ret > 0)
  {
    // move and convert to absolute path style
    chdir(bin_path);
    getcwd(bin_path, MAX_PATH);
    bin_path_len = strlen(bin_path);

    // append the trailing '/'
    bin_path[bin_path_len++] = '/';
    bin_path[bin_path_len] = 0;
  }
  else
  {
    free(bin_path);
    bin_path = NULL;
  }

  /* Always try to use SHAREDIR/CONFDIR first, if possible. On some
   * platforms, such as Linux, this will be something other than
   * the current working directory (i.e. '.'). If this fails, try
   * to look up the resources relative to the binary's install
   * location, which should allow the MegaZeux binary to be more
   * "portable".
   */
  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
    size_t base_name_len = strlen(mzx_res[i].base_name);
    size_t p_dir_len;

    if(i == CONFIG_TXT)
      chdir(CONFDIR);
    else
      chdir(SHAREDIR);

    getcwd(p_dir, MAX_PATH);
    p_dir_len = strlen(p_dir);

    // if we can't add the path we should really fail hard
    if(p_dir_len + base_name_len + 1 >= MAX_PATH)
      continue;

    // append the trailing '/'
    p_dir[p_dir_len++] = '/';
    p_dir[p_dir_len] = 0;

    memcpy(full_path_base, p_dir, p_dir_len);
    memcpy(full_path_base + p_dir_len, mzx_res[i].base_name, base_name_len);
    full_path_base[p_dir_len + base_name_len] = 0;

    full_path = cmalloc(MAX_PATH);
    clean_path_slashes(full_path_base, full_path, MAX_PATH);

    // Attempt to load it from this new path
    if(!stat(full_path, &file_info))
    {
      mzx_res[i].path = full_path;
      continue;
    }

    free(full_path);

    // Try to locate the resource relative to the bin_path
    if(bin_path)
    {
      chdir(bin_path);
      if(!stat(mzx_res[i].base_name, &file_info))
      {
        mzx_res[i].path = cmalloc(bin_path_len + base_name_len + 1);
        memcpy(mzx_res[i].path, bin_path, bin_path_len);
        memcpy(mzx_res[i].path + bin_path_len,
         mzx_res[i].base_name, base_name_len);
        mzx_res[i].path[bin_path_len + base_name_len] = 0;
      }
    }
  }

  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
    /* Skip editor resources if this isn't the editor. */
    if(!editor && mzx_res[i].editor_only)
      continue;

    if(!mzx_res[i].path)
    {
      if(mzx_res[i].optional)
      {
        warn("Failed to locate non-critical resource '%s'\n",
         mzx_res[i].base_name);
        continue;
      }

      warn("Failed to locate critical resource '%s'.\n",
       mzx_res[i].base_name);
      ret = 1;
    }
  }

  free(p_dir);
  free(bin_path);
  free(full_path_base);
  return ret;
}

void mzx_res_free(void)
{
  int i;
  for(i = 0; i < END_RESOURCE_ID_T; i++)
    if(mzx_res[i].path)
      free(mzx_res[i].path);
}

#ifdef USERCONFFILE
#define COPY_BUFFER_SIZE  4096
static unsigned char copy_buffer[COPY_BUFFER_SIZE];
#endif

char *mzx_res_get_by_id(enum resource_id id)
{
#ifdef USERCONFFILE
  static char userconfpath[MAX_PATH];
  if(id == CONFIG_TXT)
  {
    FILE *fp;

    // Special handling for CONFIG_TXT to allow for user
    // configuration files
    snprintf(userconfpath, MAX_PATH, "%s/%s", getenv("HOME"), USERCONFFILE);

    // Check if the file can be opened for reading
    fp = fopen_unsafe(userconfpath, "rb");

    if(fp)
    {
      fclose(fp);
      return (char *)userconfpath;
    }
    // Otherwise, try to open the file for writing
    fp = fopen_unsafe(userconfpath, "wb");
    if(fp)
    {
      FILE *original = fopen_unsafe(mzx_res[id].path, "rb");
      if(original)
      {
        size_t bytes_read;
        for(;;)
        {
          bytes_read = fread(copy_buffer, 1, COPY_BUFFER_SIZE, original);
          if(bytes_read)
            fwrite(copy_buffer, 1, bytes_read, fp);
          else
            break;
        }
        fclose(fp);
        fclose(original);
        return (char *)userconfpath;
      }
      fclose(fp);
    }

    // If that's no good, just read the normal config file
  }
#endif /* USERCONFFILE */
  return mzx_res[id].path;
}

/**
 * Some platforms may not be able to display console output without extra work.
 * On these platforms redirect STDIO to files so the console output is easier
 * to read.
 */
boolean redirect_stdio(const char *base_path, boolean require_conf)
{
  char clean_path[MAX_PATH];
  char dest_path[MAX_PATH];
  FILE *fp_wr;
  uint64_t t;

  if(!base_path)
    return false;

  clean_path_slashes(base_path, clean_path, MAX_PATH);

  if(require_conf)
  {
    // If the config file is required, attempt to stat it.
    struct stat stat_info;

    join_path_names(dest_path, MAX_PATH, clean_path, "config.txt");
    if(stat(dest_path, &stat_info))
      return false;
  }

  // Test directory for write access.
  join_path_names(dest_path, MAX_PATH, clean_path, "stdout.txt");
  fp_wr = fopen_unsafe(dest_path, "w");
  if(fp_wr)
  {
    t = (uint64_t)time(NULL);

    // Redirect stdout to stdout.txt.
    fclose(fp_wr);
    fprintf(stdout, "Redirecting logs to '%s'...\n", dest_path);
    if(freopen(dest_path, "w", stdout))
      fprintf(stdout, "MegaZeux: Logging to '%s' (%" PRIu64 ")\n", dest_path, t);
    else
      fprintf(stdout, "Failed to redirect stdout\n");

    // Redirect stderr to stderr.txt.
    join_path_names(dest_path, MAX_PATH, clean_path, "stderr.txt");
    fprintf(stderr, "Redirecting logs to '%s'...\n", dest_path);
    if(freopen(dest_path, "w", stderr))
      fprintf(stderr, "MegaZeux: Logging to '%s' (%" PRIu64 ")\n", dest_path, t);
    else
      fprintf(stderr, "Failed to redirect stderr\n");

    return true;
  }
  return false;
}

// Get 2 bytes, little endian

int fgetw(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp);
  if((a == EOF) || (b == EOF))
    return EOF;

  return (b << 8) | a;
}

// Get 4 bytes, little endian

int fgetd(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp), c = fgetc(fp), d = fgetc(fp);
  if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
    return EOF;

  return (d << 24) | (c << 16) | (b << 8) | a;
}

// Put 2 bytes, little endian

void fputw(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc(src >> 8, fp);
}

// Put 4 bytes, little endian

void fputd(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc((src >> 8) & 0xFF, fp);
  fputc((src >> 16) & 0xFF, fp);
  fputc((src >> 24) & 0xFF, fp);
}

// Determine file size of an open FILE and rewind it

long ftell_and_rewind(FILE *f)
{
  long size;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);

  return size;
}

// Random function, returns an integer [0-range)

static uint64_t rng_state;

// Seed the RNG from system time on startup
void rng_seed_init(void)
{
  uint64_t seed = (((uint64_t)time(NULL)) << 32) | clock();
  rng_set_seed(seed);
}

uint64_t rng_get_seed(void)
{
  return rng_state;
}

void rng_set_seed(uint64_t seed)
{
  rng_state = seed;
}

// xorshift*
// Implementation from https://en.wikipedia.org/wiki/Xorshift
unsigned int Random(uint64_t range)
{
  uint64_t x = rng_state;
  if(x == 0) x = 1;
  x ^= x >> 12; // a
  x ^= x << 25; // b
  x ^= x >> 27; // c
  rng_state = x;
  return ((x * 0x2545F4914F6CDD1D) >> 32) * range / 0xFFFFFFFF;
}

// FIXME: This function should probably die. It's unsafe.
void add_ext(char *src, const char *ext)
{
  size_t src_len = strlen(src);
  size_t ext_len = strlen(ext);

  if((src_len < ext_len) || (src[src_len - ext_len] != '.') ||
   strcasecmp(src + src_len - ext_len, ext))
  {
    if(src_len + ext_len >= MAX_PATH)
      src_len = MAX_PATH - ext_len - 1;

    snprintf(src + src_len, MAX_PATH - src_len, "%s", ext);
    src[MAX_PATH - 1] = '\0';
  }
}

int get_ext_pos(const char *filename)
{
  int filename_length = strlen(filename);
  int ext_pos;

  for(ext_pos = filename_length - 1; ext_pos >= 0; ext_pos--)
    if(filename[ext_pos] == '.')
      break;

  return ext_pos;
}

__utils_maybe_static ssize_t __get_path(const char *file_name, char *dest,
 unsigned int buf_len)
{
  ssize_t c = (ssize_t)strlen(file_name) - 1;

  // no path, or it's too long to store
  if(c == -1 || c > (int)buf_len)
  {
    if(buf_len > 0)
      dest[0] = 0;
    return -1;
  }

  while((file_name[c] != '/') && (file_name[c] != '\\') && c)
    c--;

  if(c > 0)
    memcpy(dest, file_name, c);
  dest[c] = 0;

  return c;
}

ssize_t get_path(const char *file_name, char *dest, unsigned int buf_len)
{
  return __get_path(file_name, dest, buf_len);
}

static int isslash(char n)
{
  return n=='\\' || n=='/';
}

void clean_path_slashes(const char *src, char *dest, size_t buf_size)
{
  unsigned int i = 0;
  unsigned int p = 0;
  size_t src_len = strlen(src);

  while((i < src_len) && (p < buf_size-1))
  {
    if(isslash(src[i]))
    {
      while(isslash(src[i]))
        i++;

      dest[p] = DIR_SEPARATOR_CHAR;
      p++;
    }
    else
    {
      dest[p] = src[i];
      i++;
      p++;
    }
  }
  dest[p] = '\0';

  if((p >= 2) && (dest[p-1] == DIR_SEPARATOR_CHAR) && (dest[p-2] != ':'))
    dest[p-1] = '\0';
}

void split_path_filename(const char *source,
 char *destpath, unsigned int dest_buffer_len,
 char *destfile, unsigned int file_buffer_len)
{
  char temppath[MAX_PATH];
  struct stat path_info;
  int stat_res = stat(source, &path_info);

  // If the entirety of source is a directory
  if((stat_res >= 0) && S_ISDIR(path_info.st_mode))
  {
    if(dest_buffer_len)
      clean_path_slashes(source, destpath, dest_buffer_len);

    if(file_buffer_len)
      destfile[0] = '\0';
  }
  else
  // If source has a directory and a file
  if((source[0] != '\0') && get_path(source, temppath, MAX_PATH))
  {
    // get_path leaves off trailing /, add 1 to offset.
    if(dest_buffer_len)
      clean_path_slashes(temppath, destpath, dest_buffer_len);

    if(file_buffer_len)
      strncpy(destfile, &(source[strlen(temppath) + 1]), file_buffer_len);
  }
  // Source is just a file or blank.
  else
  {
    if(dest_buffer_len)
      destpath[0] = '\0';

    if(file_buffer_len)
      strncpy(destfile, source, file_buffer_len);
  }
}

void join_path_names(char* target, int max_len, const char* path1, const char* path2)
{
  if(path1[strlen(path1)-1] == DIR_SEPARATOR_CHAR)
    snprintf(target, max_len, "%s%s", path1, path2);
  else
    snprintf(target, max_len, "%s%s%s", path1, DIR_SEPARATOR, path2);
}

int create_path_if_not_exists(const char *filename)
{
  struct stat stat_info;
  char parent_directory[MAX_PATH];

  if(!get_path(filename, parent_directory, MAX_PATH))
    return 1;

  if(!stat(parent_directory, &stat_info))
    return 2;

  create_path_if_not_exists(parent_directory);

  if(mkdir(parent_directory, 0755))
    return 3;

  return 0;
}

// Navigate a path name.
int change_dir_name(char *path_name, const char *dest)
{
  struct stat stat_info;
  char path_temp[MAX_PATH];
  char path[MAX_PATH];
  const char *current;
  const char *next;
  const char *end;
  char current_char;
  size_t len;

  if(!dest || !dest[0])
    return -1;

  if(!path_name)
    return -1;

  current = dest;
  end = dest + strlen(dest);

  next = strchr(dest, ':');
  if(next)
  {
    /**
     * Destination starts with a Windows-style root directory.
     * Aside from Windows, these are often used by console SDKs (albeit with /
     * instead of \) to distinguish SD cards and the like.
     */
    if(next[1] != DIR_SEPARATOR_CHAR && next[1] != 0)
      return -1;

    snprintf(path, MAX_PATH, "%.*s" DIR_SEPARATOR, (int)(next - dest + 1),
     dest);
    path[MAX_PATH - 1] = '\0';

    if(stat(path, &stat_info) < 0)
      return -1;

    current = next + 1;
    if(current[0] == DIR_SEPARATOR_CHAR)
      current++;
  }
  else

  if(dest[0] == DIR_SEPARATOR_CHAR)
  {
    /**
     * Destination starts with a Unix-style root directory.
     * Aside from Unix-likes, these are also supported by console platforms.
     * Even Windows (back through XP at least) doesn't seem to mind them.
     */
    snprintf(path, MAX_PATH, DIR_SEPARATOR);
    current = dest + 1;
  }

  else
  {
    /**
     * Destination is relative--start from the current path. Make sure there's
     * a trailing separator.
     */
    if(path_name[strlen(path_name) - 1] != DIR_SEPARATOR_CHAR)
      snprintf(path, MAX_PATH, "%s" DIR_SEPARATOR, path_name);

    else
      snprintf(path, MAX_PATH, "%s", path_name);
  }

  path[MAX_PATH - 1] = '\0';
  current_char = current[0];
  len = strlen(path);

  // Apply directory fragments to the path.
  while(current_char != 0)
  {
    // Increment next to skip the separator so it will be copied over.
    next = strchr(current, DIR_SEPARATOR_CHAR);
    if(!next) next = end;
    else      next++;

    // . does nothing, .. goes back one level
    if(current_char == '.')
    {
      if(current[1] == '.')
      {
        // Skip the rightmost separator (current level) and look for the
        // previous separator. If found, truncate the path to it.
        char *pos = path + len - 1;
        do
        {
          pos--;
        }
        while(pos >= path && *pos != DIR_SEPARATOR_CHAR);

        if(pos >= path)
        {
          pos[1] = 0;
          len = strlen(path);
        }
      }
    }
    else
    {
      snprintf(path + len, MAX_PATH - len, "%.*s", (int)(next - current),
       current);
      path[MAX_PATH - 1] = '\0';
      len = strlen(path);
    }

    current = next;
    current_char = current[0];
  }

  // This needs to be done before the stat for some platforms (e.g. 3DS)
  clean_path_slashes(path, path_temp, MAX_PATH);
  if(stat(path_temp, &stat_info) >= 0)
  {
    snprintf(path_name, MAX_PATH, "%s", path_temp);
    return 0;
  }

  return -1;
}

// Index must be an array of 256 ints
void boyer_moore_index(const void *B, const size_t b_len,
 int index[256], boolean ignore_case)
{
  char *b = (char *)B;
  int i;

  char *s = b;
  char *last = b + b_len - 1;

  for(i = 0; i < 256; i++)
    index[i] = b_len;

  if(!ignore_case)
  {
    for(s = b; s < last; s++)
      index[(int)*s] = last - s;
  }
  else
  {
    for(s = b; s < last; s++)
      index[memtolower((int)*s)] = last - s;

    // Duplicating the lowercase values over the uppercase values helps avoid
    // an extra tolower in the search function.
    memcpy(index + 'A', index + 'a', sizeof(int) * 26);
  }
}

// Search for substring B in haystack A. The index greatly increases the
// search speed, especially for large needles. This is actually a reduced
// Boyer-Moore search, as the original version uses two separate indexes.
void *boyer_moore_search(const void *A, const size_t a_len,
 const void *B, const size_t b_len, const int index[256], boolean ignore_case)
{
  const unsigned char *a = (const unsigned char *)A;
  const unsigned char *b = (const unsigned char *)B;
  size_t i = b_len - 1;
  size_t idx;
  int j;
  if(!ignore_case)
  {
    while(i < a_len)
    {
      j = b_len - 1;

      while(j >= 0 && a[i] == b[j])
        j--, i--;

      if(j == -1)
        return (void *)(a + i + 1);

      idx = index[(int)a[i]];
      i += MAX(b_len - j, idx);
    }
  }
  else
  {
    while(i < a_len)
    {
      j = b_len - 1;

      while(j >= 0 && memtolower((int)a[i]) == memtolower((int)b[j]))
        j--, i--;

      if(j == -1)
        return (void *)(a + i + 1);

      idx = index[(int)a[i]];
      i += MAX(b_len - j, idx);
    }
  }
  return NULL;
}


int mem_getc(const unsigned char **ptr)
{
  int val = (*ptr)[0];
  *ptr += 1;
  return val;
}

int mem_getw(const unsigned char **ptr)
{
  int val = (*ptr)[0] | ((*ptr)[1] << 8);
  *ptr += 2;
  return val;
}

int mem_getd(const unsigned char **ptr)
{
  int val = (*ptr)[0] | ((*ptr)[1] << 8) | ((*ptr)[2] << 16) | ((int)((*ptr)[3]) << 24);
  *ptr += 4;
  return val;
}

void mem_putc(int src, unsigned char **ptr)
{
  (*ptr)[0] = src;
  *ptr += 1;
}

void mem_putw(int src, unsigned char **ptr)
{
  (*ptr)[0] = src & 0xFF;
  (*ptr)[1] = (src >> 8) & 0xFF;
  *ptr += 2;
}

void mem_putd(int src, unsigned char **ptr)
{
  (*ptr)[0] = src & 0xFF;
  (*ptr)[1] = (src >> 8) & 0xFF;
  (*ptr)[2] = (src >> 16) & 0xFF;
  (*ptr)[3] = (src >> 24) & 0xFF;
  *ptr += 4;
}


// like fsafegets, except from memory.
int memsafegets(char *dest, int size, char **src, char *end)
{
  char *pos = dest;
  char *next = *src;
  char *stop = MIN(end, next+size);
  char ch;

  // Return 0 if this is the end of the memory block
  if(next == NULL)
    return 0;

  // Copy the memory until the end, the bound, or a newline
  while(next<stop && (ch = *next)!='\n')
  {
    *pos = ch;
    pos++;
    next++;
  }
  *pos = 0;

  // Place the counter where the next line begins
  if(next < end)
  {
    *src = next + 1;
  }

  // Mark that this is the end
  else
  {
    *src = NULL;
  }

  // Length at least 1 -- get rid of \r and \n
  if(pos > dest)
    if(pos[-1] == '\r' || pos[-1] == '\n')
      pos[-1] = 0;

  // Length at least 2 -- get rid of \r and \n
  if(pos - 1 > dest)
    if(pos[-2] == '\r' || pos[-2] == '\n')
      pos[-2] = 0;

  return 1;
}


#if defined(__WIN32__) && defined(__STRICT_ANSI__)

/* On WIN32 with C99 defining __STRICT_ANSI__ these POSIX.1-2001 functions
 * are not available. The stricmp/strnicmp functions are available, but not
 * to C99 programs.
 *
 * Redefine them here; these are copied from glibc, and should be optimal.
 */

int strcasecmp(const char *s1, const char *s2)
{
  while(*s1 != '\0' && tolower(*s1) == tolower(*s2))
  {
    s1++;
    s2++;
  }

  return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
  if(n == 0)
    return 0;

  while(n-- != 0 && tolower(*s1) == tolower(*s2))
  {
    if(n == 0 || *s1 == '\0' || *s2 == '\0')
      break;
    s1++;
    s2++;
  }

  return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}

#endif // __WIN32__ && __STRICT_ANSI__

#if defined(__WIN32__) || defined(__amigaos__)

// strsep() stolen from glibc (GPL)

char *strsep(char **stringp, const char *delim)
{
  char *begin, *end;

  begin = *stringp;
  if(!begin)
    return NULL;

  if(delim[0] == '\0' || delim[1] == '\0')
  {
    char ch = delim[0];
    if(ch == '\0')
      end = NULL;
    else
    {
      if(*begin == ch)
        end = begin;
      else if(*begin == '\0')
        end = NULL;
      else
        end = strchr(begin + 1, ch);
    }
  }
  else
    end = strpbrk(begin, delim);

  if(end)
  {
    *end++ = '\0';
    *stringp = end;
  }
  else
    *stringp = NULL;

  return begin;
}

#endif // __WIN32__ || __amigaos__

long dir_tell(struct mzx_dir *dir)
{
  return dir->pos;
}

boolean dir_open(struct mzx_dir *dir, const char *path)
{
  dir->d = opendir(path);
  if(!dir->d)
    return false;

  dir->entries = 0;
  while(readdir(dir->d) != NULL)
    dir->entries++;

// pspdev/devkitPSP historically does not have a rewinddir implementation.
// libctru (3DS) has rewinddir but it doesn't work. FIXME reason for the Switch?
#if defined(CONFIG_PSP) || defined(CONFIG_3DS) || defined(CONFIG_SWITCH)
  strncpy(dir->path, path, PATH_BUF_LEN);
  dir->path[PATH_BUF_LEN - 1] = 0;
  closedir(dir->d);
  dir->d = opendir(path);
#else
  rewinddir(dir->d);
#endif

  dir->pos = 0;
  return true;
}

void dir_close(struct mzx_dir *dir)
{
  if(dir->d)
  {
    closedir(dir->d);
    dir->d = NULL;
    dir->entries = 0;
    dir->pos = 0;
  }
}

void dir_seek(struct mzx_dir *dir, long offset)
{
  long i;

  if(!dir->d)
    return;

  dir->pos = CLAMP(offset, 0L, dir->entries);

// pspdev/devkitPSP historically does not have a rewinddir implementation.
// libctru (3DS) has rewinddir but it doesn't work. FIXME reason for the Switch?
#if defined(CONFIG_PSP) || defined(CONFIG_3DS) || defined(CONFIG_SWITCH)
  closedir(dir->d);
  dir->d = opendir(dir->path);
  if(!dir->d)
    return;
#else
  rewinddir(dir->d);
#endif

  for(i = 0; i < dir->pos; i++)
    readdir(dir->d);
}

boolean dir_get_next_entry(struct mzx_dir *dir, char *entry, int *type)
{
  struct dirent *inode;

  if(!dir->d)
    return false;

  dir->pos = MIN(dir->pos + 1, dir->entries);

  inode = readdir(dir->d);
  if(!inode)
  {
    entry[0] = 0;
    return false;
  }

  if(type)
  {
#ifdef DT_UNKNOWN
    /* On platforms that support it, the d_type field can be used to avoid
     * stat calls. This is critical for the file manager on embedded platforms.
     */
    if(inode->d_type == DT_REG)
      *type = DIR_TYPE_FILE;
    else
    if(inode->d_type == DT_DIR)
      *type = DIR_TYPE_DIR;
    else
      *type = DIR_TYPE_UNKNOWN;
#else
    *type = DIR_TYPE_UNKNOWN;
#endif
  }

  snprintf(entry, PATH_BUF_LEN, "%s", inode->d_name);
  return true;
}

#if defined(__amigaos__)

long __stack_chk_guard[8];

void __stack_chk_fail(void)
{
  warn("Stack overflow detected; terminated");
  exit(0);
}

#endif // __amigaos__

#if defined(CONFIG_PSP) || defined(CONFIG_NDS)
FILE *popen(const char *command, const char *type)
{
	return NULL;
}

int pclose(FILE *stream)
{
	return 0;
}
#endif // CONFIG_PSP || CONFIG_NDS
