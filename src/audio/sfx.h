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

#ifndef __AUDIO_SFX_H
#define __AUDIO_SFX_H

#include "../compat.h"

__M_BEGIN_DECLS

// Number of unique sound effects and length of sound effects
#define NUM_SFX         50
#define SFX_SIZE        69
#define LEGACY_SFX_SIZE 69

// Requires NUM_SFX/SFX_SIZE, so include after.
#include "../world_struct.h"

// Size of sound queue
#define NOISEMAX        4096

#ifdef CONFIG_EDITOR
CORE_LIBSPEC extern char sfx_strs[NUM_SFX][SFX_SIZE];
#endif // CONFIG_EDITOR

#ifdef CONFIG_AUDIO

// Used by audio_pcs.c
void sfx_next_note(void);

void play_sfx(struct world *mzx_world, int sfx);
void play_string(char *str, int sfx_play);
void sfx_clear_queue(void);
char sfx_is_playing(void);
int sfx_length_left(void);

#else // !CONFIG_AUDIO

static inline void play_sfx(struct world *mzx_world, int sfxn) {}
static inline void play_string(char *str, int sfx_play) {}
static inline void sfx_clear_queue(void) {}
static inline char sfx_is_playing(void) { return 0; }
static inline int sfx_length_left(void) { return 0; }

#endif // CONFIG_AUDIO

__M_END_DECLS

#endif // __AUDIO_SFX_H
