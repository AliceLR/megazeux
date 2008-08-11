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

/* Declarations for DATA.ASM (most global data) */

#ifndef __DATA_H
#define __DATA_H

#include "const.h"
#include "robot.h"
#include "counter.h"
#include <stdio.h>

/* This first section is for idput.cpp */
extern unsigned char id_chars[455];
extern unsigned char id_dmg[128];
extern unsigned char def_id_chars[455];
extern unsigned char *player_color;
extern unsigned char *player_char;            // was [4]
extern unsigned char bullet_color[3];         // was [3]
extern unsigned char missile_color;
extern unsigned char *bullet_char;
extern char          refresh_mod_playing;
extern unsigned char music_on;
extern unsigned char sfx_on;
extern unsigned char music_gvol;              // Global volume (settings)
extern unsigned char sound_gvol;              // Global volume (settings)
extern unsigned char music_device;
extern unsigned int  mixing_rate;
extern unsigned char sfx_channels;
extern unsigned char overall_speed;
extern char curr_file[FILENAME_SIZE];
extern char curr_sav[FILENAME_SIZE];
extern char help_file[PATHNAME_SIZE];     // Drive + Path + Filename
extern char megazeux_dir[PATHNAME_SIZE];  // Directory started in
extern char current_dir[PATHNAME_SIZE];   // Current directory
extern unsigned char megazeux_drive;      // Startup drive (0=A...)
extern unsigned char current_drive;       // Current drive (0=A...)

extern unsigned char scroll_color;        // Current scroll color
extern unsigned char cheats_active;       // (additive flag)
extern unsigned char current_help_sec0;   // Use for context-sens.help
extern int was_zapped;

extern char saved_pl_color;

extern char *update_done;
extern int update_done_size;

extern char          *thing_names[128];
extern unsigned int  flags[128];

#endif
