/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* DETECT.H- Declarations and #defines for DETECT.ASM */

#ifndef __DETECT_H
#define __DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

int detect_graphics(void);
int detect_processor(void);

#ifdef __cplusplus
}
#endif

//Graphics codes- EGA is 3 and above
#define EGA		3
#define VGAm	5
#define VGAc	6

//Processor codes- 186 is 1 and above
#define P186	1

#endif