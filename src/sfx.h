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

/* Prototypes for SFX.CPP */

#ifndef __SFX_H
#define __SFX_H

#include "compat.h"

__M_BEGIN_DECLS

// Number of unique sound effects
#define NUM_SFX         50

#include "world.h"

// Size of sound queue
#define NOISEMAX        4096

// Special freqs
#define F_REST          1

void play_sfx(World *mzx_world, int sfx);
void clear_sfx_queue(void);
void sound_system(void);
void play_note(int note, int octave, int delay);
void submit_sound(int freq, int delay);
char sfx_init(void);
void sfx_exit(void);
char is_playing(void);
void play_str(char *str, int sfx_play);

typedef struct
{
  int duration;//This is the struc of the sound queue
  int freq;
} noise;

extern char *sfx_strs[NUM_SFX];
extern char *custom_sfx; //Ref. in chunks of 69
extern int custom_sfx_on; //1 to turn on custom sfx

__M_END_DECLS

#endif // __SFX_H
