/* MegaZeux
 *
 * Copyright (C) 2020 Ian Burgmyer <spectere@gmail.com>
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

#include <stdarg.h>
#include <stdio.h>

#include "compat.h"
#include "config.h"
#include "util.h"

FILE *vitaout = NULL;
FILE *vitaerr = NULL;

void vitadebug_close(void)
{
  if(vitaout != NULL) fclose(vitaout);
  if(vitaerr != NULL) fclose(vitaerr);
}

void vitadebug_init(void)
{
  char clean_path[MAX_PATH];
  clean_path_slashes(CONFDIR, clean_path, MAX_PATH);

  // This will drop the files into the MZX root path (ux0:data/MegaZeux/)
  vitaout = fopen_unsafe("/stdout.txt", "w");
  vitaerr = fopen_unsafe("/stderr.txt", "w");
}

#ifdef DEBUG
void info(const char *format, ...)
{
  if(vitaout == NULL) return;

  va_list args;
  va_start(args, format);

  vfprintf(vitaout, format, args);
  fflush(vitaout);

  va_end(args);
}

void warn(const char *format, ...)
{
  if(vitaerr == NULL) return;

  va_list args;
  va_start(args, format);

  vfprintf(vitaerr, format, args);
  fflush(vitaerr);

  va_end(args);
}

void debug(const char *format, ...)
{
  if(vitaerr == NULL) return;

  va_list args;
  va_start(args, format);

  vfprintf(vitaerr, format, args);
  fflush(vitaerr);

  va_end(args);
}
#endif // DEBUG
