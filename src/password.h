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

/* Declarations for PASSWORD.CPP */

#ifndef __PASSWORD_H
#define __PASSWORD_H

#include <stdio.h>

//Write password info to an open file
void write_password(FILE *fp);
//Read password info from an open file
void read_password(FILE *fp);
//Get xor code for the current password
unsigned char get_pw_xor_code(void);
//Returns 0 if got the password right, or if there is no protection or
//password.
char check_pw(void);
//Edit password settings.
void password_dialog(void);

#endif
