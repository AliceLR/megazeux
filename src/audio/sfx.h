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
#define SFX_SIZE        69
#define LEGACY_SFX_SIZE 69

enum sfx_id
{
  SFX_GEM               = 0,
  SFX_MAGIC_GEM         = 1,
  SFX_HEALTH            = 2,
  SFX_AMMO              = 3,
  SFX_COIN              = 4,
  SFX_LIFE              = 5,
  SFX_LO_BOMB           = 6,
  SFX_HI_BOMB           = 7,
  SFX_KEY               = 8,
  SFX_FULL_KEYS         = 9,
  SFX_UNLOCK            = 10,
  SFX_CANT_UNLOCK       = 11,
  SFX_INVIS_WALL        = 12,
  SFX_FOREST            = 13,
  SFX_GATE_LOCKED       = 14,
  SFX_OPEN_GATE         = 15,
  SFX_INVINCO_START     = 16,
  SFX_INVINCO_BEAT      = 17,
  SFX_INVINCO_END       = 18,
  SFX_DOOR_LOCKED       = 19,
  SFX_OPEN_DOOR         = 20,
  SFX_HURT              = 21,
  SFX_LAVA              = 22,
  SFX_DEATH             = 23,
  SFX_GAME_OVER         = 24,
  SFX_GATE_CLOSE        = 25,
  SFX_PUSH              = 26,
  SFX_TRANSPORT         = 27,
  SFX_SHOOT             = 28,
  SFX_BREAK             = 29,
  SFX_OUT_OF_AMMO       = 30,
  SFX_RICOCHET          = 31,
  SFX_OUT_OF_BOMBS      = 32,
  SFX_PLACE_LO_BOMB     = 33,
  SFX_PLACE_HI_BOMB     = 34,
  SFX_SWITCH_BOMB_TYPE  = 35,
  SFX_EXPLOSION         = 36,
  SFX_ENTRANCE          = 37,
  SFX_POUCH             = 38,
  SFX_RING_POTION       = 39,
  SFX_EMPTY_CHEST       = 40,
  SFX_CHEST             = 41,
  SFX_OUT_OF_TIME       = 42,
  SFX_FIRE_HURT         = 43,
  SFX_STOLEN_GEM        = 44,
  SFX_ENEMY_HP_DOWN     = 45,
  SFX_DRAGON_FIRE       = 46,
  SFX_SCROLL_SIGN       = 47,
  SFX_GOOP              = 48,
  SFX_UNUSED_49         = 49,
  NUM_SFX               = 50
};

// Requires NUM_SFX/SFX_SIZE, so don't include.
struct world;

// Size of sound queue
#define NOISEMAX        4096

#ifdef CONFIG_EDITOR
CORE_LIBSPEC extern char sfx_strs[NUM_SFX][SFX_SIZE];
#endif // CONFIG_EDITOR

#ifdef CONFIG_AUDIO

// Called by audio_pcs.c under lock.
void sfx_next_note(int *is_playing, int *freq, int *duration);

void play_sfx(struct world *mzx_world, enum sfx_id sfx);
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
