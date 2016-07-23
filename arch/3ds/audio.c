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
#include "../../src/graphics.h"
#include "../../src/audio.h"

#include <3ds.h>
#include <string.h>

#ifdef CONFIG_AUDIO

static void* audio_buffer;
static ndspWaveBuf ndsp_buffer;
static int buffer_size;
static Uint32 last_pos = 0;

void ndsp_callback(void* dud)
{
  Uint32 pos = ndspChnGetSamplePos(0);
  u8* data = (u8*) dud;
  if(pos >= audio.buffer_samples && last_pos < audio.buffer_samples)
  {
    audio_callback(&data[0], buffer_size);
  }
  else if(pos < audio.buffer_samples && last_pos >= audio.buffer_samples)
  {
    audio_callback(&data[buffer_size], buffer_size);
  }
  last_pos = pos;
}

void init_audio_platform(struct config_info *conf)
{
  float mix[12];

  audio.buffer_samples = conf->buffer_size & ~7;
  if(!audio.buffer_samples)
    audio.buffer_samples = 2048;

  buffer_size = sizeof(Sint16) * 2 * audio.buffer_samples;
  audio.mix_buffer = malloc(buffer_size * 2);

  ndspInit();

  memset(mix, 0, sizeof(mix));
  mix[0] = mix[1] = 1.0f;
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  ndspSetOutputCount(1);
  ndspSetMasterVol(1.0f);

  audio_buffer = linearAlloc(buffer_size * 2);
  memset(audio_buffer, 0, buffer_size * 2);
  ndspSetCallback(ndsp_callback, audio_buffer);

  memset(&ndsp_buffer, 0, sizeof(ndspWaveBuf));
  ndspChnReset(0);
  ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
  ndspChnSetRate(0, audio.output_frequency);
  ndspChnSetFormat(0, NDSP_CHANNELS(2) | NDSP_ENCODING(NDSP_ENCODING_PCM16));
  ndspChnSetMix(0, mix);

  ndsp_buffer.data_vaddr = audio_buffer;
  ndsp_buffer.nsamples = audio.buffer_samples * 2;
  ndsp_buffer.looping = true;
  ndsp_buffer.status = NDSP_WBUF_FREE;
  ndspChnWaveBufAdd(0, &ndsp_buffer);
}

void quit_audio_platform(void)
{
  // stub
}

#endif // CONFIG_AUDIO
