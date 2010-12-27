/* MegaZeux
 *
 * Copyright (C) 2004 Alistair Strachan <alistair@devzero.co.uk>
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

#ifndef __FSAFEOPEN_H
#define __FSAFEOPEN_H

#include "compat.h"

#include <stdio.h>

__M_BEGIN_DECLS

enum
{
  FSAFE_SUCCESS = 0,
  FSAFE_MATCHED_DIRECTORY,
  FSAFE_MATCH_FAILED,
  FSAFE_BRUTE_FORCE_FAILED,
  FSAFE_INVALID_ARGUMENT,
  FSAFE_ABSOLUTE_PATH_ERROR,
  FSAFE_WINDOWS_DRIVE_LETTER_ERROR,
  FSAFE_PARENT_DIRECTORY_ERROR
};

CORE_LIBSPEC char *fsafegets(char *s, int size, FILE *stream);

int fsafetranslate(const char *path, char *newpath);
FILE *fsafeopen(const char *path, const char *mode);

__M_END_DECLS

#endif // __FSAFEOPEN_H
