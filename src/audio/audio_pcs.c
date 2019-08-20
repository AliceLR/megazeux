/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "audio.h"
#include "audio_pcs.h"
#include "sfx.h"

#include "../configure.h"

#include <string.h>

struct pc_speaker_stream
{
  struct audio_stream a;
  Uint32 volume;
  Uint32 playing;
  Uint32 frequency;
  Sint32 last_frequency;
  Uint32 note_duration;
  Uint32 last_duration;
  Uint32 last_playing;
  Uint32 sample_cutoff;
  Uint32 last_increment_buffer;
};

static Uint32 pcs_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  struct pc_speaker_stream *pcs_stream = (struct pc_speaker_stream *)a_src;
  Uint32 offset = 0, i;
  Uint32 sample_duration = pcs_stream->last_duration;
  Uint32 end_duration;
  Uint32 increment_value, increment_buffer;
  Uint32 sfx_scale = (pcs_stream->volume ? pcs_stream->volume + 1 : 0) * 32;
  Uint32 sfx_scale_half = sfx_scale / 2;
  Sint32 *mix_dest_ptr = buffer;
  Sint16 cur_sample;

  if(sample_duration >= len / 4)
  {
    end_duration = len / 4;
    pcs_stream->last_duration = sample_duration - end_duration;
    sample_duration = end_duration;
  }

  if(pcs_stream->last_playing)
  {
    increment_value =
     (Uint32)((float)pcs_stream->last_frequency /
     (audio.output_frequency) * 4294967296.0);

    increment_buffer = pcs_stream->last_increment_buffer;

    for(i = 0; i < sample_duration; i++)
    {
      cur_sample = (Uint32)((increment_buffer & 0x80000000) >> 31) *
       sfx_scale - sfx_scale_half;
      mix_dest_ptr[0] += cur_sample;
      mix_dest_ptr[1] += cur_sample;
      mix_dest_ptr += 2;
      increment_buffer += increment_value;
    }
    pcs_stream->last_increment_buffer = increment_buffer;
  }
  else
  {
    mix_dest_ptr += (sample_duration * 2);
  }

  offset += sample_duration;

  if(offset < len / 4)
    pcs_stream->last_playing = 0;

  while(offset < len / 4)
  {
    sfx_next_note((int *)&(pcs_stream->playing),
     (int *)&(pcs_stream->frequency), (int *)&(pcs_stream->note_duration));

    // Minimum note duration is 1 to prevent locking up the audio thread.
    if(pcs_stream->note_duration < 1)
      pcs_stream->note_duration = 1;

    sample_duration = (Uint32)((float)audio.output_frequency / 500 *
     pcs_stream->note_duration);

    if(offset + sample_duration >= len / 4)
    {
      end_duration = (len / 4) - offset;
      pcs_stream->last_duration = sample_duration - end_duration;
      pcs_stream->last_playing = pcs_stream->playing;
      pcs_stream->last_frequency = pcs_stream->frequency;

      sample_duration = end_duration;
    }

    offset += sample_duration;

    if(pcs_stream->playing)
    {
      increment_value =
       (Uint32)((float)pcs_stream->frequency /
       audio.output_frequency * 4294967296.0);
      increment_buffer = 0;

      for(i = 0; i < sample_duration; i++)
      {
        cur_sample = (Uint32)((increment_buffer & 0x80000000) >> 31) *
         sfx_scale - sfx_scale_half;
        mix_dest_ptr[0] += cur_sample;
        mix_dest_ptr[1] += cur_sample;
        mix_dest_ptr += 2;
        increment_buffer += increment_value;
      }
      pcs_stream->last_increment_buffer = increment_buffer;
    }
    else
    {
      mix_dest_ptr += (sample_duration * 2);
    }
  }

  return 0;
}

static void pcs_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  ((struct pc_speaker_stream *)a_src)->volume = volume;
}

static void pcs_destruct(struct audio_stream *a_src)
{
  destruct_audio_stream(a_src);
}

static struct audio_stream *construct_pc_speaker_stream(void)
{
  struct pc_speaker_stream *pcs_stream;
  struct audio_stream_spec a_spec;

  pcs_stream = ccalloc(1, sizeof(struct pc_speaker_stream));

  memset(&a_spec, 0, sizeof(struct audio_stream_spec));
  a_spec.mix_data   = pcs_mix_data;
  a_spec.set_volume = pcs_set_volume;
  a_spec.destruct   = pcs_destruct;

  // The volume here will be corrected after initialization...
  initialize_audio_stream((struct audio_stream *)pcs_stream, &a_spec, 255, 0);

  return (struct audio_stream *)pcs_stream;
}

void init_pc_speaker(struct config_info *conf)
{
  audio.pcs_stream = construct_pc_speaker_stream();
}
