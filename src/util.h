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
  MZX_HELP_FIL,
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

void delay(int ms);
int get_ticks(void);

void get_path(const char *file_name, char *dest, unsigned int buf_len);

/* Some platforms like NDS don't have a rename(2), so we need
 * to implement it.
 */
#ifndef rename
#define rename rename
int rename(const char *oldpath, const char *newpath);
#endif

__M_END_DECLS

#endif // __UTIL_H
