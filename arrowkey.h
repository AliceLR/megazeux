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

/* ARROWKEY.H- Declarations for ARROWKEY.ASM */

#ifndef __ARROWKEY_H
#define __ARROWKEY_H

extern char far kb_installed;/* Whether the keyboard routines are installed */
extern char far up_pressed;
extern char far dn_pressed;
extern char far lf_pressed;
extern char far rt_pressed;
extern char far sp_pressed;
extern char far del_pressed;
extern char far key_code;    /* Latest key code */
extern char far keyb_mode;   /* Set to 1 for different key discard code */
extern char far curr_table;  /* The current keyboard table (0/1) */
extern char far state_table[128];

#ifdef __cplusplus
extern "C" {
#endif

void far install_i09(void);  /* Run to install keyboard code */
void far uninstall_i09(void);/* Run to uninstall keyboard code */

/* Run with param of 0 to switch to game mode keyboard, or param of 1 to
	switch to somewhat normal mode keyboard */
void far switch_keyb_table(char table);

#ifdef __cplusplus
}
#endif

#endif