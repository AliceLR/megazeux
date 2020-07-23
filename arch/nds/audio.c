/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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

#include "../../src/event.h"
#include "../../src/platform.h"
#include "../../src/util.h"
#include "../../src/audio/audio.h"

#include <nds.h>
#include "platform.h"
#include "../../src/audio/audio.h"
#include "../../src/audio/sfx.h"

#ifdef CONFIG_AUDIO

#define PCS_DURATION_PER_VBL 9

static u8 nds_vol_table[] = {
	0, 8, 16, 26, 36, 48, 61, 75, 91, 108, 127
};

#define NDS_VOLUME(vol) nds_vol_table[CLAMP((vol), 0, 10)]

static inline void nds_sound_volume(int volume) {
  fifoSendValue32(FIFO_MZX, CMD_MZX_SOUND_VOLUME | (NDS_VOLUME(volume) << 24));
}

static inline void nds_pcs_sound(int freq, int volume) {
  fifoSendValue32(FIFO_MZX, CMD_MZX_PCS_TONE | (((freq) & 0xFFFF) << 8) | (NDS_VOLUME(volume) << 24));
}

static int pcs_playing;
static int pcs_frequency;
static int pcs_duration;

void nds_audio_vblank(void)
{
  int last_playing = pcs_playing;
  int last_frequency = pcs_frequency;
  int ticks = 0;
  int ticked;

  while (ticks < PCS_DURATION_PER_VBL)
  {
    if (pcs_playing)
    {
      if (pcs_duration <= 0)
      {
        pcs_playing = 0;
      }
      else
      {
        ticked = MIN(PCS_DURATION_PER_VBL - ticks, pcs_duration);
        ticks += ticked;
        pcs_duration -= ticked;
      }
      continue;
    }

    // Update PC speaker.
    sfx_next_note(&pcs_playing, &pcs_frequency, &pcs_duration);

    if (!pcs_playing)
      break;
  }

  if (pcs_playing != last_playing)
  {
    nds_pcs_sound(pcs_playing ? pcs_frequency : 0, NDS_VOLUME(audio_get_pcs_volume()));
  }
  else if (pcs_playing && (pcs_frequency != last_frequency))
  {
    nds_pcs_sound(pcs_frequency, NDS_VOLUME(audio_get_pcs_volume()));
  }
}

void init_audio_platform(struct config_info *conf)
{
  pcs_playing = 0;
  pcs_frequency = 0;
  pcs_duration = 0;
  nds_sound_volume(10);
}

void quit_audio_platform(void)
{
  nds_sound_volume(0);
  nds_pcs_sound(0, 0);
}

#endif // CONFIG_AUDIO
