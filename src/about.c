/* MegaZeux
 *
 * Copyright (C) 2023 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "about.h"
#include "platform_attribute.h" /* ATTRIBUTE_PRINTF */
#include "util.h"
#include "window.h"
#include "io/path.h"
#include "io/vio.h"

#include <ctype.h>
#include <zlib.h>

#ifdef CONFIG_SDL
#include <SDL.h>
#endif
#ifdef CONFIG_XMP
#include <xmp.h>
#endif
#ifdef CONFIG_MIKMOD
#include <mikmod.h>
#endif
#ifdef CONFIG_OPENMPT
#include <libopenmpt/libopenmpt.h>
#endif
#ifdef CONFIG_PNG
#include <png.h>
#endif
#ifdef CONFIG_VORBIS
#ifndef CONFIG_TREMOR
#include <vorbis/codec.h>
#endif
#endif

#define ABOUT_X       0
#define ABOUT_Y       1
#define ABOUT_WIDTH   80
#define ABOUT_HEIGHT  23
#define MAX_FILES     32
#define MAX_VERSIONS  32
#define MAX_LICENSE   (1 << 18) /* 256k should be enough for a license... */

ATTRIBUTE_PRINTF(1, 2)
static char *about_line(const char *fmt, ...)
{
  va_list args;
  char buf[ABOUT_WIDTH - 5];
  char *str;
  size_t len;

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  len = strlen(buf) + 1;
  str = (char *)malloc(len);
  if(str)
    memcpy(str, buf, len);

  return str;
}

/**
 * Build the "About MegaZeux" list text, including all version information.
 */
static char **about_text(int *num_lines)
{
  char **lines = (char  **)malloc(sizeof(char *) * MAX_VERSIONS);
  int i = 0;
  if(!lines)
    return NULL;

#ifdef VERSION_DATE
  lines[i++] = about_line("MegaZeux " VERSION VERSION_DATE);
#else
  lines[i++] = about_line("MegaZeux " VERSION);
#endif
  lines[i++] = about_line("Platform: " PLATFORM);
#ifdef VERSION_HEAD
  lines[i++] = about_line("Build head: " VERSION_HEAD);
#endif
  lines[i++] = about_line(" ");
  lines[i++] = about_line("MegaZeux is free software licensed under the GNU GPL version 2 and");
  lines[i++] = about_line("comes with NO WARRANTY. See \"License\" for more information.");
  lines[i++] = about_line(" ");

#ifdef CONFIG_SDL
  {
    SDL_version compiled;
    SDL_version ver;
    SDL_VERSION(&compiled);
#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_GetVersion(&ver);
#else
    ver = *SDL_Linked_Version();
#endif
    if(memcmp(&compiled, &ver, sizeof(SDL_version)))
    {
      lines[i++] = about_line("SDL: %u.%u.%u (compiled: %u.%u.%u)",
       ver.major, ver.minor, ver.patch, compiled.major, compiled.minor, compiled.patch);
    }
    else
      lines[i++] = about_line("SDL: %u.%u.%u", ver.major, ver.minor, ver.patch);
  }
#endif

#ifdef CONFIG_XMP
  if(strcmp(XMP_VERSION, xmp_version))
    lines[i++] = about_line("libxmp: %s (compiled: " XMP_VERSION ")", xmp_version);
  else
    lines[i++] = about_line("libxmp: %s", xmp_version);
#endif

#ifdef CONFIG_MIKMOD
  {
    long compiled = LIBMIKMOD_VERSION;
    long ver = MikMod_GetVersion();

    if(compiled != ver)
    {
      lines[i++] = about_line("MikMod: %ld.%ld.%ld (compiled: %ld.%ld.%ld)",
       ver >> 16, (ver >> 8) & 0xff, ver & 0xff,
       compiled >> 16, (compiled >> 8) & 0xff, compiled & 0xff);
    }
    else
      lines[i++] = about_line("MikMod: %ld.%ld.%ld", ver >> 16, (ver >> 8) & 0xff, ver & 0xff);
  }
#endif

#ifdef CONFIG_MODPLUG
  lines[i++] = about_line("libmodplug");
#endif

#ifdef CONFIG_OPENMPT
  {
    const char *compiled = OPENMPT_API_VERSION_STRING;
    const char *ver = openmpt_get_string("library_version");

    if(strcmp(ver, compiled))
      lines[i++] = about_line("libopenmpt: %s (compiled: %s)", ver, compiled);
    else
      lines[i++] = about_line("libopenmpt: %s", ver);
  }
#endif

  if(strcmp(ZLIB_VERSION, zlibVersion()))
    lines[i++] = about_line("zlib: %s (compiled: " ZLIB_VERSION ")", zlibVersion());
  else
    lines[i++] = about_line("zlib: " ZLIB_VERSION);

#ifdef CONFIG_PNG
  lines[i++] = about_line("libpng: " PNG_LIBPNG_VER_STRING);
#endif

#ifdef CONFIG_VORBIS
#ifndef CONFIG_TREMOR
  lines[i++] = about_line("libvorbis: %s", vorbis_version_string());
#else
  lines[i++] = about_line("libtremor");
#endif
#endif

  lines[i++] = about_line(" ");
  lines[i++] = about_line("CONFDIR: " CONFDIR);
  lines[i++] = about_line("CONFFILE: " CONFFILE);
  lines[i++] = about_line("SHAREDIR: " SHAREDIR);
  lines[i++] = about_line("LICENSEDIR: " LICENSEDIR);

  *num_lines = i;
  return lines;
}

/**
 * Get the license directory.
 */
static const char *get_license_dir(char *license_dir, size_t len)
{
  if(!path_is_absolute(LICENSEDIR))
  {
    const char *exec_dir = mzx_res_get_by_id(MZX_EXECUTABLE_DIR);
    if(!exec_dir)
      return "";

    if(path_join(license_dir, len, exec_dir, LICENSEDIR) < 0)
      return "";

    return license_dir;
  }
  else
    return LICENSEDIR;
}

/**
 * Scan license_dir for all applicable license files.
 */
static void load_license_list(char *names[MAX_FILES], char *files[MAX_FILES],
 const char *license_dir)
{
  vdir *dir;
  enum vdir_type type;
  char buf[MAX_PATH];
  char *license = NULL;
  char *license_3rd = NULL;
  int num_files = 0;

  dir = vdir_open(license_dir);
  if(dir)
  {
    // Pass 1: find LICENSE (or LICENSE.) and LICENCE.3rd.
    while(num_files < MAX_FILES)
    {
      if(!vdir_read(dir, buf, sizeof(buf), &type))
        break;
      if(type == DIR_TYPE_DIR)
        continue;

      if((!strcasecmp(buf, "LICENSE") || !strcasecmp(buf, "LICENSE.")) && !license)
      {
        license = about_line("%s", buf);
      }
      else

      if(!strcasecmp(buf, "LICENSE.3rd") && !license_3rd)
      {
        license_3rd = about_line("%s", buf);
      }
    }
    // MegaZeux's license should always go first.
    if(license)
    {
      names[num_files] = about_line("License");
      files[num_files++] = license;
    }
    if(license_3rd)
    {
      names[num_files] = about_line("3rd Party");
      files[num_files++] = license_3rd;
    }

    // Pass 2: add all other license files.
    vdir_rewind(dir);
    while(num_files < MAX_FILES)
    {
      if(!vdir_read(dir, buf, sizeof(buf), &type))
        break;
      if(type == DIR_TYPE_DIR)
        continue;

      if(!strncasecmp(buf, "LICENSE.", 8) && strcasecmp(buf, "LICENSE.3rd") && buf[8])
      {
        names[num_files] = about_line("%-.16s", buf + 8);
        files[num_files++] = about_line("%s", buf);
      }
#ifdef CONFIG_DJGPP
      else

      /* Even if the extensions are completely different, having multiple of
       * these licenses might cause SFN numbers greater than 1. */
      if(!strncasecmp(buf, "LICENS~", 7) && isdigit((unsigned char)buf[7]) &&
       buf[8] == '.' && buf[9] != '\0')
      {
        names[num_files] = about_line("%-.16s", buf + 9);
        files[num_files++] = about_line("%s", buf);
      }
#endif
    }

    vdir_close(dir);
  }
}

static void destroy_license_list(char *names[MAX_FILES], char *files[MAX_FILES])
{
  int i;
  for(i = 0; i < MAX_FILES; i++)
  {
    free(names[i]);
    free(files[i]);
  }
}

// TODO: text editor element would be preferable here.
// please kill this entire function with fire in a future release
static char **load_license(const char *filename, int *_lines)
{
  vfile *fp;
  char **arr = (char **)malloc(32 * sizeof(char *));
  char **tmp;
  boolean tab;
  size_t lines_alloc = 32;
  size_t lines = 0;
  size_t total = 0;
  size_t len;
  size_t sz;
  size_t i;
  size_t j;

  if(!arr)
    return NULL;

  fp = vfopen_unsafe(filename, "r");
  if(fp)
  {
    char buf[256];

    while(vfsafegets(buf, sizeof(buf), fp))
    {
      if(lines >= lines_alloc)
      {
        lines_alloc *= 2;
        tmp = (char **)realloc(arr, lines_alloc * sizeof(char *));
        if(!tmp)
          break;
        arr = tmp;
      }
      tab = false;
      len = strlen(buf);
      sz = len + 1;
      for(i = 0; i < len; i++)
      {
        if(buf[i] == '\t')
        {
          sz += 7;
          tab = true;
        }
      }
      total += sz;
      if(total > MAX_LICENSE)
        break;

      arr[lines] = (char *)malloc(sz);
      if(!arr[lines])
        break;

      if(tab)
      {
        for(i = 0, j = 0; i < len && j < sz - 1; i++)
        {
          if(buf[i] == '\t')
          {
            memset(arr[lines] + j, ' ', 8);
            j += 8;
          }
          else
            arr[lines][j++] = buf[i];
        }
        arr[lines][j] = '\0';
      }
      else
        memcpy(arr[lines], buf, sz);

      lines++;
    }
    vfclose(fp);
  }
  *_lines = lines;
  return arr;
}

static void destroy_lines(char **list, int lines)
{
  int i;
  if(list)
  {
    for(i = 0; i < lines; i++)
      free(list[i]);
    free(list);
  }
}

/**
 * Display the "About MegaZeux" dialog box.
 */
void about_megazeux(context *parent)
{
  struct dialog src;
  struct element *elements[MAX_FILES + 3];
  char **current = NULL;
  char *names[MAX_FILES];
  char *files[MAX_FILES];
  const char *license_dir;
  char tmp[MAX_PATH];
  char path[MAX_PATH];
  int current_lines = 0;
  int num_elements;
  int list_pos = 0;
  int list_scroll = 0;
  int show = 1;
  int prev = -1;
  int x = 2;
  int y = 1;
  int i;

  license_dir = get_license_dir(tmp, sizeof(tmp));

  memset(names, 0, sizeof(names));
  memset(files, 0, sizeof(files));
  load_license_list(names, files, license_dir);

  while(show > 0)
  {
    if(show != prev)
    {
      list_pos = 0;
      list_scroll = 0;
      prev = show;
    }

    if(show == 1)
    {
      current = about_text(&current_lines);
    }
    else
    {
      if(path_join(path, sizeof(path), license_dir, files[show - 2]) < 0)
        path[0] = '\0';

      current = load_license(path, &current_lines);
    }

    if(!current)
      current_lines = 0;

    elements[0] = construct_button(74, 1, "OK", 0);
    elements[1] = construct_list_box(1, 3, (const char **)current, current_lines, 19,
     ABOUT_WIDTH - 2, show, &list_pos, &list_scroll, false);
    elements[2] = construct_button(2, 1, "About", 1);

    num_elements = 3;
    x = 11;
    y = 1;

    for(i = 0; i < MAX_FILES; i++)
    {
      if(files[i])
      {
        int sz = strlen(names[i]) + 4;
        if(x + sz > ABOUT_WIDTH - 6)
        {
          x = 2;
          y++;
          if(y >= 3)
            break;
        }
        elements[num_elements++] = construct_button(x, y, names[i], i + 2);
        x += sz;
      }
    }

    construct_dialog(&src, "About MegaZeux",
     ABOUT_X, ABOUT_Y, ABOUT_WIDTH, ABOUT_HEIGHT, elements, num_elements, 1);

    show = run_dialog(parent->world, &src);
    destruct_dialog(&src);

    destroy_lines(current, current_lines);
  }
  destroy_license_list(names, files);
}
