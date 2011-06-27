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

// Useful, generic utility functions

#ifndef __UTIL_H
#define __UTIL_H

#include <stdio.h>
#include "compat.h"

__M_BEGIN_DECLS

#if !defined(_MSC_VER) && !defined(__amigaos__)
#include <unistd.h>
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define CLAMP(x, low, high) \
  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

enum resource_id
{
  CONFIG_TXT = 0,
  MZX_DEFAULT_CHR,
  MZX_EDIT_CHR,
  SMZX_PAL,
#ifdef CONFIG_EDITOR
  MZX_ASCII_CHR,
  MZX_BLANK_CHR,
  MZX_SMZX_CHR,
#endif
#ifdef CONFIG_HELPSYS
  MZX_HELP_FIL,
#endif
#ifdef CONFIG_RENDER_GL_PROGRAM
  SHADERS_SCALER_VERT,
  SHADERS_SCALER_FRAG,
  SHADERS_TILEMAP_VERT,
  SHADERS_TILEMAP_FRAG,
  SHADERS_TILEMAP_SMZX12_FRAG,
  SHADERS_TILEMAP_SMZX3_FRAG,
  SHADERS_MOUSE_VERT,
  SHADERS_MOUSE_FRAG,
  SHADERS_CURSOR_VERT,
  SHADERS_CURSOR_FRAG,
#endif
  END_RESOURCE_ID_T // must be last
};

CORE_LIBSPEC int mzx_res_init(const char *argv0, bool editor);
CORE_LIBSPEC void mzx_res_free(void);
CORE_LIBSPEC char *mzx_res_get_by_id(enum resource_id id);

CORE_LIBSPEC long ftell_and_rewind(FILE *f);
unsigned int Random(unsigned long long range);

CORE_LIBSPEC ssize_t get_path(const char *file_name, char *dest, unsigned int buf_len);
#ifdef CONFIG_UTILS
ssize_t __get_path(const char *file_name, char *dest, unsigned int buf_len);
#endif

typedef void (*fn_ptr)(void);

struct dso_syms_map
{
  const char *name;
  fn_ptr *sym_ptr;
};

#if defined(CONFIG_NDS) || defined(CONFIG_WII)

#include <sys/dir.h>

#define PATH_BUF_LEN FILENAME_MAX
typedef DIR_ITER dir_t;

#else // !(CONFIG_NDS || CONFIG_WII)

#include <sys/types.h>
#include <dirent.h>

#define PATH_BUF_LEN MAX_PATH
typedef DIR dir_t;

#endif // CONFIG_NDS || CONFIG_WII

struct mzx_dir {
#ifdef CONFIG_PSP
  char path[PATH_BUF_LEN];
#endif
  dir_t *d;
  long entries;
  long pos;
};

bool dir_open(struct mzx_dir *dir, const char *path);
void dir_close(struct mzx_dir *dir);
void dir_seek(struct mzx_dir *dir, long offset);
long dir_tell(struct mzx_dir *dir);
bool dir_get_next_entry(struct mzx_dir *dir, char *entry);

#if defined(__WIN32__) && defined(__STRICT_ANSI__)
CORE_LIBSPEC int strcasecmp(const char *s1, const char *s2);
CORE_LIBSPEC int strncasecmp(const char *s1, const char *s2, size_t n);
#endif // __WIN32__ && __STRICT_ANSI__

#if defined(__WIN32__) || defined(__amigaos__)
CORE_LIBSPEC char *strsep(char **stringp, const char *delim);
#endif // __WIN32__ || __amigaos__

#ifndef __WIN32__
#if defined(CONFIG_PSP) || defined(CONFIG_GP2X) \
 || defined(CONFIG_NDS) || defined(CONFIG_WII)
#include <string.h>
#else
#include <strings.h>
#endif
#endif // !__WIN32__

#if defined(CONFIG_NDS) || defined(CONFIG_WII)
// FIXME: rmdir() needs implementing on NDS/Wii
#define rmdir(x)
#endif

#if defined(__WIN32__) && !defined(_MSC_VER)
#define mkdir(file,mode) mkdir(file)
#endif

#if defined(CONFIG_AUDIO) || defined(CONFIG_EDITOR)
CORE_LIBSPEC extern const char *const mod_gdm_ext[];
#endif

#if defined(__amigaos__)
CORE_LIBSPEC extern long __stack_chk_guard[8];
CORE_LIBSPEC void __stack_chk_fail(void);
#endif

#if defined(ANDROID)

#define info(...)  LOGI(__VA_ARGS__)
#define warn(...)  LOGW(__VA_ARGS__)

#ifdef DEBUG
#define debug(...) LOGD(__VA_ARGS__)
#else
#define debug(...) do { } while(0)
#endif

#elif defined(CONFIG_NDS) /* ANDROID */

// When the graphics have initialized, print to a debug buffer rather than the screen.
void info(const char *format, ...)  __attribute__((format(printf, 1, 2)));
void warn(const char *format, ...)  __attribute__((format(printf, 1, 2)));

#ifdef DEBUG
void debug(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else
#define debug(...) do { } while(0)
#endif

#else /* ANDROID, CONFIG_NDS */

#define info(...) \
 do { \
   fprintf(stdout, "INFO: " __VA_ARGS__); \
   fflush(stdout); \
 } while(0)

#define warn(...) \
 do { \
   fprintf(stderr, "WARNING: " __VA_ARGS__); \
   fflush(stderr); \
 } while(0)

#ifdef DEBUG
#define debug(...) \
 do { \
   fprintf(stderr, "DEBUG: " __VA_ARGS__); \
   fflush(stderr); \
 } while(0)
#else
#define debug(...) do { } while(0)
#endif

#endif /* ANDROID, CONFIG_NDS */

__M_END_DECLS

#endif // __UTIL_H
