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

#ifndef __VITADEBUG_H
#define __VITADEBUG_H

#include <stdio.h>

void vitadebug_close(void);
void vitadebug_init(void);

#ifdef DEBUG
void info(const char *format, ...);
void warn(const char *format, ...);
void debug(const char *format, ...);
#else // !DEBUG
// The Vita doesn't do anything with stdout/stderr, so we just disable it here.
#define info(...) do { } while(0)
#define warn(...) do { } while(0)
#define debug(...) do { } while(0)
#endif // DEBUG

#endif // __VITADEBUG_H
