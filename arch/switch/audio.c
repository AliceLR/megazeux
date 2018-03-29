/* MegaZeux
 *
 * Copyright (C) 2016, 2018 Adrian Siekierka <asiekierka@gmail.com>
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

#include <switch.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include "platform.h"

#ifdef CONFIG_AUDIO

static u8* audio_buffer;
static AudioOutBuffer audout_buffer[2];
static int buffer_size;
static bool threadRunning = true;
static Thread audioThread;

static void audio_thread_func(void* dud)
{
  AudioOutBuffer* released;
  u32 released_count;
  bool soundFillBlock;

  audio_callback((Sint16*) audout_buffer[0].buffer, buffer_size);
  audoutAppendAudioOutBuffer(&audout_buffer[0]);
  audio_callback((Sint16*) audout_buffer[1].buffer, buffer_size);
  audoutAppendAudioOutBuffer(&audout_buffer[1]);

  while (threadRunning)
  {
    audoutWaitPlayFinish(&released, &released_count, U64_MAX);
    soundFillBlock = released == (&audout_buffer[1]);

    audio_callback((Sint16*) audout_buffer[soundFillBlock ^ 1].buffer, buffer_size);
    audoutAppendAudioOutBuffer(&audout_buffer[soundFillBlock ^ 1]);
  }
}


void init_audio_platform(struct config_info *conf)
{
  audio.buffer_samples = (conf->buffer_size + (0x800 - 1)) & ~(0x800 - 1);
  if(!audio.buffer_samples)
    audio.buffer_samples = 2048;

  buffer_size = sizeof(Sint16) * 2 * audio.buffer_samples;
  audio.mix_buffer = malloc(buffer_size * 2);

  audoutInitialize();

  audio_buffer = memalign(0x1000, buffer_size * 2);
  memset(audio_buffer, 0, buffer_size * 2);

  for (int i = 0; i < 2; i++)
  {
    memset(&audout_buffer[i], 0, sizeof(AudioOutBuffer));
    audout_buffer[i].next = &audout_buffer[i ^ 1];
    audout_buffer[i].buffer = &audio_buffer[i * buffer_size];
    audout_buffer[i].buffer_size = buffer_size;
    audout_buffer[i].data_size = buffer_size / 2;
    audout_buffer[i].data_offset = 0;
  }

  threadRunning = true;
  threadCreate(&audioThread, audio_thread_func, NULL, 8192, 0x2C, 3);
  threadStart(&audioThread);

  audoutStartAudioOut();
}

void quit_audio_platform(void)
{
  threadRunning = false;
  threadWaitForExit(&audioThread);

  free(audio_buffer);
  audoutStopAudioOut();
  audoutExit();
}

#endif // CONFIG_AUDIO
