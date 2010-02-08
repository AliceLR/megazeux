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

#include "util.h"

#include <sys/stat.h>
#include <time.h>
#include "SDL.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

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
 * mzx_resource_id_t enumeration defines them.
 */
static struct mzx_resource mzx_res[] = {
  { CONFFILE,          NULL },
  { "mzx_ascii.chr",   NULL },
  { "mzx_blank.chr",   NULL },
  { "mzx_default.chr", NULL },
  { "mzx_help.fil",    NULL },
  { "mzx_smzx.chr",    NULL },
  { "mzx_edit.chr",    NULL },
  { "smzx.pal",        NULL },
};

int mzx_res_init(const char *argv0)
{
  char bin_path[MAX_PATH];
  struct stat file_info;
  int i, bin_path_len;
  int ret = 0;

  get_path(argv0, bin_path, MAX_PATH);

  // move and convert to absolute path style
  chdir(bin_path);
  getcwd(bin_path, MAX_PATH);
  bin_path_len = strlen(bin_path);

  // append the trailing '/'
  bin_path[bin_path_len] = '/';
  bin_path_len++;

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
    char p_dir[MAX_PATH];
    char *full_path;
    int p_dir_len;

    if(i == CONFIG_TXT)
      chdir(CONFDIR);
    else
      chdir(SHAREDIR);

    getcwd(p_dir, MAX_PATH);
    p_dir_len = strlen(p_dir);

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
    chdir(bin_path);

    // We located the file related to bin_path
    if(!stat(mzx_res[i].base_name, &file_info))
    {
      mzx_res[i].path = malloc(bin_path_len + base_name_len + 1);
      memcpy(mzx_res[i].path, bin_path, bin_path_len);
      memcpy(mzx_res[i].path + bin_path_len,
       mzx_res[i].base_name, base_name_len);
      mzx_res[i].path[bin_path_len + base_name_len] = 0;
    }
  }

  chdir(bin_path);

  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
    if(!mzx_res[i].path)
    {
      fprintf(stderr, "Failed to locate critical resource '%s'.\n",
       mzx_res[i].base_name);
      ret = 1;
    }
  }

  return ret;
}

void mzx_res_free(void)
{
  int i;
  for(i = 0; i < END_RESOURCE_ID_T - 1; i++)
    if(mzx_res[i].path)
      free(mzx_res[i].path);
}

char *mzx_res_get_by_id(mzx_resource_id_t id)
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

// Currently, just does a cheap delay..

void delay(int ms)
{
  SDL_Delay(ms);
}

int get_ticks(void)
{
  return SDL_GetTicks();
}

void get_path(const char *file_name, char *dest, unsigned int buf_len)
{
  int c = strlen(file_name) - 1;

  // no path, or it's too long to store
  if (c == -1 || c > (int)buf_len)
  {
    if(buf_len > 0)
      dest[0] = 0;
    return;
  }

  while((file_name[c] != '/') && (file_name[c] != '\\') && c)
    c--;

  if(c > 0)
    memcpy(dest, file_name, c);
  dest[c] = 0;
}
