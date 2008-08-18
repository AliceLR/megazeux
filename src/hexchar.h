/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

/* Prototype for HEXCHAR.CPP */

#ifndef __HEXCHAR_H
#define __HEXCHAR_H

#include "compat.h"

__M_BEGIN_DECLS

void write_hex_byte(char byte, char color, int x, int y);
void write_number(int number, char color, int x, int y,
 int minlen, int rightalign, int base); // 0, 0, 10

__M_END_DECLS

#endif // __HEXCHAR_H
