/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#define BOOL _BOOL
#include <gctypes.h>
#include <ogc/lwp.h>
#include <ogc/cache.h>
#include <ogc/audio.h>
#undef BOOL

#define STACKSIZE 8192

static lwpq_t audio_queue;
static lwp_t audio_thread;
static u8 audio_stack[STACKSIZE];
static Sint16 *audio_buffer[2];
static volatile int current = 0;
static volatile int audio_stop = 0;
static int buffer_size;

static void *wii_audio_thread(void *dud)
{
  while(!audio_stop)
  {
    LWP_ThreadSleep(audio_queue);
    audio_callback(audio_buffer[current ^ 1], buffer_size);
    DCFlushRange(audio_buffer[current ^ 1], buffer_size);
    current ^= 1;
  }

  return 0;
}

static void dma_callback(void)
{
  AUDIO_StopDMA();
  AUDIO_InitDMA((u32)audio_buffer[current], buffer_size);
  AUDIO_StartDMA();
  LWP_ThreadSignal(audio_queue);
}

void init_audio_platform(config_info *conf)
{
  int i, audfreq;

  // Only output frequencies of 32000 and 48000 are valid
  switch(audio.output_frequency)
  {
    case 32000: audfreq = AI_SAMPLERATE_32KHZ; break;
    case 48000: audfreq = AI_SAMPLERATE_48KHZ; break;
    default: audio.mix_buffer = NULL; return;
  }

  // buffer size must be multiple of 32 bytes, so samples must be multiple of 8
  audio.buffer_samples = conf->buffer_size & ~7;
  if(!audio.buffer_samples)
    audio.buffer_samples = 2048;

  buffer_size = sizeof(Sint16) * 2 * audio.buffer_samples;
  audio.mix_buffer = malloc(buffer_size * 2);

  for(i = 0; i < 2; i++)
  {
    audio_buffer[i] = memalign(32, buffer_size);
    memset(audio_buffer[i], 0, buffer_size);
    DCFlushRange(audio_buffer[i], buffer_size);
  }

  AUDIO_Init(NULL);
  AUDIO_SetDSPSampleRate(audfreq);
  LWP_InitQueue(&audio_queue);
  if(LWP_CreateThread(&audio_thread, wii_audio_thread, NULL, audio_stack,
   STACKSIZE, 80) >= 0)
  {
    AUDIO_RegisterDMACallback(dma_callback);
    AUDIO_InitDMA((u32)audio_buffer[0], buffer_size);
    AUDIO_StartDMA();
  }
}

void quit_audio_platform(void)
{
  void *dud;

  if(!audio.mix_buffer)
    return;

  audio_stop = 1;
  LWP_JoinThread(audio_thread, &dud);
  free(audio.mix_buffer);
  AUDIO_StopDMA();
  LWP_CloseQueue(audio_queue);
  // Don't free hardware audio buffers
  // Memory allocated with memalign() can't neccessarily be free()'d
}
