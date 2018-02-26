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
//#ifdef CONFIG_AUDIO
//AUDIO_LIBSPEC extern char sfx_strs[NUM_SFX][SFX_SIZE];
//#else
// FIXME this should probably be EDITOR_LIBSPEC
CORE_LIBSPEC extern char sfx_strs[NUM_SFX][SFX_SIZE];
//#endif // !CONFIG_AUDIO
#endif // CONFIG_EDITOR

#ifdef CONFIG_AUDIO

void sfx_next_note(void);

/*AUDIO_LIBSPEC*/ void play_sfx(struct world *mzx_world, int sfx);
/*AUDIO_LIBSPEC*/ void clear_sfx_queue(void);
/*AUDIO_LIBSPEC*/ char is_playing(void);
/*AUDIO_LIBSPEC*/ void play_str(char *str, int sfx_play);
/*AUDIO_LIBSPEC*/ int sfx_length_left(void);

#else // !CONFIG_AUDIO

static inline void clear_sfx_queue(void) {}
static inline void play_sfx(struct world *mzx_world, int sfxn) {}
static inline void play_str(char *str, int sfx_play) {}
static inline char is_playing(void) { return 0; }
static inline int sfx_length_left(void) { return 0; }

#endif // CONFIG_AUDIO

__M_END_DECLS

#endif // __AUDIO_SFX_H
