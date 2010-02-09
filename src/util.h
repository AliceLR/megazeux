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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define CLAMP(x, low, high) \
  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

typedef enum
{
  CONFIG_TXT = 0,
  MZX_ASCII_CHR,
  MZX_BLANK_CHR,
  MZX_DEFAULT_CHR,
#ifdef CONFIG_HELPSYS
  MZX_HELP_FIL,
#endif
  MZX_SMZX_CHR,
  MZX_EDIT_CHR,
  SMZX_PAL,
  END_RESOURCE_ID_T, // must be last
}
mzx_resource_id_t;

int mzx_res_init(const char *argv0);
void mzx_res_free(void);
char *mzx_res_get_by_id(mzx_resource_id_t id);

long ftell_and_rewind(FILE *f);
int Random(int range);

int get_path(const char *file_name, char *dest, unsigned int buf_len);

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

dir_t *dir_open(const char *path);
void dir_close(dir_t *dir);
int dir_get_next_entry(dir_t *dir, char *entry);

#if defined(__WIN32__) && defined(__STRICT_ANSI__)
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
#else
#if defined(CONFIG_PSP) || defined(CONFIG_GP2X) || defined(CONFIG_NDS) \
 || defined(CONFIG_WII)
#include <string.h>
#else
#include <strings.h>
#endif
#endif // __WIN32__ && __STRICT_ANSI__

/* Some platforms like NDS don't have a rename(2), so we need
 * to implement it.
 */
#if !defined(rename) && !defined(__WIN32__)
#define NEED_RENAME
#define rename rename
int rename(const char *oldpath, const char *newpath);
#endif

#if defined(CONFIG_NDS) || defined(CONFIG_WII)
// FIXME: rmdir() needs implementing on NDS/Wii
#define rmdir(x)
#endif

#if defined(CONFIG_AUDIO) || defined(CONFIG_EDITOR)
extern const char *mod_gdm_ext[];
#endif

__M_END_DECLS

#endif // __UTIL_H
