/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

// Must include after <time.h> (MSVC bug)
#include "util.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "const.h" // for MAX_PATH
#include "error.h"

struct mzx_resource
{
  const char *const base_name;
  char *path;

  /* So mzxrun requires fewer files even in CONFIG_EDITOR=1 build */
  bool optional;
};

/* Using C99 initializers would be nicer here, but MSVC doesn't support
 * them and we try to allow compilation with that compiler.
 *
 * As a result, these must be in exactly the same order as the
 * enum resource_id enumeration defines them.
 */
static struct mzx_resource mzx_res[] = {
#define ASSETS "assets/"
  { CONFFILE,                      NULL, false },
  { ASSETS "default.chr",          NULL, false },
  { ASSETS "edit.chr",             NULL, false },
  { ASSETS "smzx.pal",             NULL, false },
#ifdef CONFIG_EDITOR
  { ASSETS "ascii.chr",            NULL, true },
  { ASSETS "blank.chr",            NULL, true },
  { ASSETS "smzx.chr",             NULL, true },
#endif
#ifdef CONFIG_HELPSYS
  { ASSETS "help.fil",             NULL, true },
#endif
#ifdef CONFIG_RENDER_GL_PROGRAM
#define SHADERS ASSETS "shaders/"
  { SHADERS "scaler.vert",         NULL, false },
  { SHADERS "scaler.frag",         NULL, false },
  { SHADERS "tilemap.vert",        NULL, false },
  { SHADERS "tilemap.frag",        NULL, false },
  { SHADERS "tilemap.smzx12.frag", NULL, false },
  { SHADERS "tilemap.smzx3.frag",  NULL, false },
  { SHADERS "mouse.vert",          NULL, false },
  { SHADERS "mouse.frag",          NULL, false },
  { SHADERS "cursor.vert",         NULL, false },
  { SHADERS "cursor.frag",         NULL, false },
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
    error(msgbuf, 2, 4, 0);
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

int mzx_res_init(const char *argv0, bool editor)
{
  size_t i, bin_path_len = 0;
  struct stat file_info;
  char *bin_path;
  ssize_t g_ret;
  char *p_dir;
  int ret = 0;

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
    size_t p_dir_len, base_name_len = strlen(mzx_res[i].base_name);
    char *full_path;

    if(i == CONFIG_TXT)
      chdir(CONFDIR);
    else
      chdir(SHAREDIR);

    getcwd(p_dir, MAX_PATH);
    p_dir_len = strlen(p_dir);

    // since we can't add the path delimeter we should really fail hard
    if(p_dir_len >= MAX_PATH)
      continue;

    // append the trailing '/'
    p_dir[p_dir_len++] = '/';
    p_dir[p_dir_len] = 0;

    full_path = cmalloc(p_dir_len + base_name_len + 1);
    memcpy(full_path, p_dir, p_dir_len);
    memcpy(full_path + p_dir_len, mzx_res[i].base_name, base_name_len);
    full_path[p_dir_len + base_name_len] = 0;

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
    /* Skip non-essential resources */
    if(!editor && mzx_res[i].optional)
      continue;

    if(!mzx_res[i].path)
    {
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

char *mzx_res_get_by_id(enum resource_id id)
{
  return mzx_res[id].path;
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

unsigned int Random(unsigned long long range)
{
  static unsigned long long seed = 0;
  unsigned long long value;

  // If the seed is 0, initialise it with time and clock
  if(seed == 0)
    seed = time(NULL) + clock();

  seed = seed * 1664525 + 1013904223;

  value = (seed & 0xFFFFFFFF) * range / 0xFFFFFFFF;
  return (unsigned int)value;
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

#if defined(CONFIG_NDS) || defined(CONFIG_WII)

// NDS/Wii versions of these functions backend directly to libfat

bool dir_open(struct mzx_dir *dir, const char *path)
{
  char entry[PATH_BUF_LEN];

  dir->d = diropen(path);
  if(!dir->d)
    return false;

  dir->entries = 0;
  while(dirnext(dir->d, entry, NULL) == 0)
    dir->entries++;

  dirreset(dir->d);
  dir->pos = 0;
  return true;
}

void dir_close(struct mzx_dir *dir)
{
  if(dir->d)
  {
    dirclose(dir->d);
    dir->d = NULL;
    dir->entries = 0;
    dir->pos = 0;
  }
}

void dir_seek(struct mzx_dir *dir, long offset)
{
  char entry[PATH_BUF_LEN];
  long i;

  if(!dir->d)
    return;

  dir->pos = CLAMP(offset, 0L, dir->entries);

  dirreset(dir->d);
  for(i = 0; i < dir->pos; i++)
    dirnext(dir->d, entry, NULL);
}

bool dir_get_next_entry(struct mzx_dir *dir, char *entry)
{
  if(!dir->d)
    return false;
  dir->pos = MIN(dir->pos + 1, dir->entries);
  return dirnext(dir->d, entry, NULL) == 0;
}

#else // !(CONFIG_NDS || CONFIG_WII)


bool dir_open(struct mzx_dir *dir, const char *path)
{
  dir->d = opendir(path);
  if(!dir->d)
    return false;

  dir->entries = 0;
  while(readdir(dir->d) != NULL)
    dir->entries++;

#ifdef CONFIG_PSP
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

#ifdef CONFIG_PSP
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

bool dir_get_next_entry(struct mzx_dir *dir, char *entry)
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

  strncpy(entry, inode->d_name, PATH_BUF_LEN - 1);
  entry[PATH_BUF_LEN - 1] = 0;
  return true;
}

#endif // CONFIG_NDS || CONFIG_WII

#if defined(CONFIG_AUDIO) || defined(CONFIG_EDITOR)

/* It would be nice if this could be static, but we can't put it in either
 * edit.c or audio.c, because one (but NOT the other) might be disabled.
 *
 * In this case, the code wouldn't be compiled (noticed by the PSP port).
 */
const char *const mod_gdm_ext[] =
{
  ".xm", ".s3m", ".mod", ".med", ".mtm", ".stm", ".it", ".669", ".ult",
  ".wav", ".dsm", ".far", ".okt", ".amf", ".ogg", ".gdm", NULL
};

#endif // CONFIG_AUDIO || CONFIG_EDITOR

#if defined(__amigaos__)

long __stack_chk_guard[8];

void __stack_chk_fail(void)
{
  warn("Stack overflow detected; terminated");
  exit(0);
}

#endif // __amigaos__

