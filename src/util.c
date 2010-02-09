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

struct mzx_resource
{
  const char *base_name;
  char *path;
};

/* Using C99 initializers would be nicer here, but MSVC doesn't support
 * them and we try to allow compilation with that compiler.
 *
 * As a result, these must be in exactly the same order as the
 * enum resource_id enumeration defines them.
 */
static struct mzx_resource mzx_res[] = {
  { CONFFILE,               NULL },
  { "mzx_ascii.chr",        NULL },
  { "mzx_blank.chr",        NULL },
  { "mzx_default.chr",      NULL },
#ifdef CONFIG_HELPSYS
  { "mzx_help.fil",         NULL },
#endif
  { "mzx_smzx.chr",         NULL },
  { "mzx_edit.chr",         NULL },
  { "smzx.pal",             NULL },
#ifdef CONFIG_RENDER_GL_PROGRAM
  { "shaders/scaler.vert",         NULL },
  { "shaders/scaler.frag",         NULL },
  { "shaders/tilemap.vert",        NULL },
  { "shaders/tilemap.frag",        NULL },
  { "shaders/tilemap.smzx12.frag", NULL },
  { "shaders/tilemap.smzx3.frag",  NULL },
  { "shaders/mouse.vert",          NULL },
  { "shaders/mouse.frag",          NULL },
  { "shaders/cursor.vert",         NULL },
  { "shaders/cursor.frag",         NULL },
#endif
};

int mzx_res_init(const char *argv0)
{
  int i, bin_path_len = 0;
  struct stat file_info;
  char *bin_path;
  char *p_dir;
  int ret = 0;
  int g_ret;

  bin_path = malloc(MAX_PATH);
  p_dir = malloc(MAX_PATH);

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
    const int base_name_len = strlen(mzx_res[i].base_name);
    char *full_path;
    int p_dir_len;

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

    full_path = malloc(p_dir_len + base_name_len + 1);
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
        mzx_res[i].path = malloc(bin_path_len + base_name_len + 1);
        memcpy(mzx_res[i].path, bin_path, bin_path_len);
        memcpy(mzx_res[i].path + bin_path_len,
         mzx_res[i].base_name, base_name_len);
        mzx_res[i].path[bin_path_len + base_name_len] = 0;
      }
    }
  }

  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
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

// Random function, returns an integer between 0 and range

int Random(int range)
{
  static unsigned long long seed = 0;
  unsigned long long value;

  // If the seed is 0, initialise it with time and clock
  if(seed == 0)
    seed = time(NULL) + clock();

  seed = seed * 1664525 + 1013904223;

  value = (seed & 0xFFFFFFFF) * range / 0xFFFFFFFF;
  return (int)value;
}

__utils_maybe_static int __get_path(const char *file_name, char *dest,
 unsigned int buf_len)
{
  int c = strlen(file_name) - 1;

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

int get_path(const char *file_name, char *dest, unsigned int buf_len)
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

#ifdef NEED_RENAME

int rename(const char *oldpath, const char *newpath)
{
  int ret = link(oldpath, newpath);
  if(!ret)
    return unlink(oldpath);
  return ret;
}

#endif // NEED_RENAME

#if defined(CONFIG_NDS) || defined(CONFIG_WII)

// NDS/Wii versions of these functions backend directly to libfat

dir_t *dir_open(const char *path)
{
  return diropen(path);
}

void dir_close(dir_t *dir)
{
  if(dir)
    dirclose(dir);
}

int dir_get_next_entry(dir_t *dir, char *entry)
{
  if(!dir)
    return -1;
  return dirnext(dir, entry, NULL);
}

#else // !(CONFIG_NDS || CONFIG_WII)

dir_t *dir_open(const char *path)
{
  return opendir(path);
}

void dir_close(dir_t *dir)
{
  if(dir)
    closedir(dir);
}

int dir_get_next_entry(dir_t *dir, char *entry)
{
  struct dirent *inode;

  if(!dir)
    return -1;

  inode = readdir(dir);
  if(!inode)
  {
    entry[0] = 0;
    return -1;
  }

  strncpy(entry, inode->d_name, MAX_PATH - 1);
  entry[MAX_PATH - 1] = 0;
  return 0;
}

#endif // CONFIG_NDS || CONFIG_WII

#if defined(CONFIG_AUDIO) || defined(CONFIG_EDITOR)

/* It would be nice if this could be static, but we can't put it in either
 * edit.c or audio.c, because one (but NOT the other) might be disabled.
 *
 * In this case, the code wouldn't be compiled (noticed by the PSP port).
 */
const char *mod_gdm_ext[] =
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

