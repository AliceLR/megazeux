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

//DT_DATA.CPP- Simple global data that names each graphics card and
//				   processor type. Formatted for use with DOS services,
//             while retaining compatibility with traditional string
//             printing functions.

#include "dt_data.h"

char far *gcard_strs[6]={
	"EGA mono\0","EGA\0","VGA mono\0",
	"VGA+\0","MCGA mono\0","MCGA\0" };

/*char far *proc_strs[5]={
	"186\0","286\0","386sx\0","386dx\0",
        "486+\0" };*/
