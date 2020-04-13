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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "const.h" // for MAX_PATH
#include "error.h"
#include "io/path.h"

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
  char *full_path;
  char *bin_path;
  ssize_t g_ret;
  char *p_dir;
  int ret = 0;

  bin_path = cmalloc(MAX_PATH);
  p_dir = cmalloc(MAX_PATH);

  g_ret = path_get_directory(bin_path, MAX_PATH, argv0);
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
    size_t full_path_len;
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

    full_path_len = p_dir_len + base_name_len;
    full_path = cmalloc(full_path_len + 1);

    memcpy(full_path, p_dir, p_dir_len);
    memcpy(full_path + p_dir_len, mzx_res[i].base_name, base_name_len);
    full_path[full_path_len] = 0;

    path_clean_slashes(full_path, full_path_len + 1);

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
  char dest_path[MAX_PATH];
  size_t dest_len;
  FILE *fp_wr;
  uint64_t t;

  if(!base_path)
    return false;

  dest_len = path_clean_slashes_copy(dest_path, MAX_PATH - 10, base_path);

  if(require_conf)
  {
    // If the config file is required, attempt to stat it.
    struct stat stat_info;

    path_append(dest_path, MAX_PATH, "config.txt");
    if(stat(dest_path, &stat_info))
      return false;
    dest_path[dest_len] = '\0';
  }

  // Test directory for write access.
  path_append(dest_path, MAX_PATH, "stdout.txt");
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
    dest_path[dest_len] = '\0';
    path_append(dest_path, MAX_PATH, "stderr.txt");
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

  return ((unsigned int)d << 24) | (c << 16) | (b << 8) | a;
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

int create_path_if_not_exists(const char *filename)
{
  struct stat stat_info;
  char parent_directory[MAX_PATH];

  if(path_get_directory(parent_directory, MAX_PATH, filename) <= 0)
    return 1;

  if(!stat(parent_directory, &stat_info))
    return 2;

  create_path_if_not_exists(parent_directory);

  if(mkdir(parent_directory, 0755))
    return 3;

  return 0;
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
