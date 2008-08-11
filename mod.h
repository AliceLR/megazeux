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

/* Declarations for MOD.CPP */

#ifndef __MOD_H
#define __MOD_H

void load_mod(char far *filename);
void end_mod(void);
void play_sample(int freq,char far *file);
void end_sample(void);
void jump_mod(int order);
void volume_mod(int vol);
void mod_exit(void);

//Set to 0 for editor, 1 for debugging games, 2 for regular games
extern unsigned char error_mode;

void mod_init(void);
void spot_sample(int freq,int sample);

//This function will remove samples from the cache one at a time,
//starting with least-played and ending with currently-playing.
//Call it removes ONE sample unless CLEAR_ALL is set. Returns 1
//if nothing was found to deallocate (IE no further fixes possible)
char free_sam_cache(char clear_all);

//Call if music_gvol or sound_gvol changes
void fix_global_volumes(void);

#endif