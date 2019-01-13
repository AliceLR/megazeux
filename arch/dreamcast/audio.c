/* MegaZeux
 *
 * Copyright (C) 2016, 2018 Adrian Siekierka <kontakt@asie.pl>
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
#include "../../src/util.h"
#include "../../src/platform.h"
#include "../../src/graphics.h"
#include "../../src/audio/audio.h"

#include <kos.h>
#include <string.h>

#ifdef CONFIG_AUDIO

static Sint16* sound_buffer;
static snd_stream_hnd_t stream;
static int buffer_size;

static void* dc_audio_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv)
{
  audio_callback(sound_buffer, smp_req);
  *smp_recv = smp_req;
  return sound_buffer;
}

static void *dc_audio_thread(void *dud)
{
  snd_stream_init();
  stream = snd_stream_alloc(dc_audio_callback, buffer_size);

  snd_stream_start(stream, audio.output_frequency, 1);

  while (1)
  {
    snd_stream_poll(stream);
    thd_sleep(10);
  }
  return NULL;
}

void init_audio_platform(struct config_info *conf)
{
  audio.buffer_samples = conf->audio_buffer_samples / 2;
  buffer_size = 4 * audio.buffer_samples;
  audio.mix_buffer = cmalloc(buffer_size * 2);
  sound_buffer = cmalloc(buffer_size);

  thd_create(0, dc_audio_thread, NULL);
}

void quit_audio_platform(void)
{
  snd_stream_destroy(stream);
  snd_stream_shutdown();
  free(sound_buffer);
  free(audio.mix_buffer);
}

#endif // CONFIG_AUDIO
