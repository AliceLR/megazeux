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
#include "../../src/audio/audio.h"
#include "../../src/audio/audio_struct.h"

#include <3ds.h>
#include <string.h>
#include "platform.h"

#ifdef CONFIG_AUDIO

static u8 *audio_buffer;
static ndspWaveBuf ndsp_buffer[2];
static int buffer_size;
static bool soundFillBlock = false;

static void ndsp_callback(void *dud)
{
  if(ndsp_buffer[soundFillBlock].status == NDSP_WBUF_DONE)
  {
    audio_callback(ndsp_buffer[soundFillBlock].data_pcm16, buffer_size);

    DSP_FlushDataCache(ndsp_buffer[soundFillBlock].data_pcm8,
     audio.buffer_samples * 4);
    ndspChnWaveBufAdd(0, &ndsp_buffer[soundFillBlock]);
    soundFillBlock = !soundFillBlock;
  }
}

void init_audio_platform(struct config_info *conf)
{
  float mix[12];

  audio.buffer_samples = conf->audio_buffer_samples & ~7;
  if(!audio.buffer_samples)
    audio.buffer_samples = 2048;

  buffer_size = sizeof(int16_t) * 2 * audio.buffer_samples;
  audio.mix_buffer = cmalloc(buffer_size * 2);

  ndspInit();

  memset(mix, 0, sizeof(mix));
  mix[0] = mix[1] = 1.0f;
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  ndspSetOutputCount(1);
  ndspSetMasterVol(1.0f);

  audio_buffer = clinearAlloc(buffer_size * 2, 0x80);
  memset(audio_buffer, 0, buffer_size * 2);
  ndspSetCallback(ndsp_callback, audio_buffer);

  memset(&ndsp_buffer[0], 0, sizeof(ndspWaveBuf));
  memset(&ndsp_buffer[1], 0, sizeof(ndspWaveBuf));
  ndspChnReset(0);
  ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
  ndspChnSetRate(0, audio.output_frequency);
  ndspChnSetFormat(0, NDSP_CHANNELS(2) | NDSP_ENCODING(NDSP_ENCODING_PCM16));
  ndspChnSetMix(0, mix);

  ndsp_buffer[0].data_vaddr = &audio_buffer[0];
  ndsp_buffer[0].nsamples = audio.buffer_samples;
  ndsp_buffer[1].data_vaddr = &audio_buffer[buffer_size];
  ndsp_buffer[1].nsamples = audio.buffer_samples;

  ndspChnWaveBufAdd(0, &ndsp_buffer[0]);
  ndspChnWaveBufAdd(0, &ndsp_buffer[1]);
}

void quit_audio_platform(void)
{
  // stub
  linearFree(audio_buffer);
  ndspExit();
}

#endif // CONFIG_AUDIO
