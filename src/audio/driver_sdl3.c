/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <stdlib.h>

#include "../SDLmzx.h"
#include "../util.h"

#include "audio.h"
#include "audio_struct.h"

static SDL_AudioSpec audio_settings;
static SDL_AudioDeviceID audio_device;
static SDL_AudioStream *audio_stream;
static void *audio_buffer;
static unsigned audio_format;

static void sdl_audio_callback(void *userdata, SDL_AudioStream *stream,
 int required_bytes, int requested_bytes)
{
  size_t framesize = SDL_AUDIO_FRAMESIZE(audio_settings);
  size_t frames = MAX(0, required_bytes) / framesize;
  while(frames > 0)
  {
    size_t out = audio_mixer_render_frames(userdata, frames,
     audio_settings.channels, audio_format);

    assert(out < frames);
    SDL_PutAudioStreamData(audio_stream, userdata, out * framesize);
    frames -= out;
  }
}

void init_audio_platform(struct config_info *conf)
{
  void *tmp;
  // TODO: configurable audio channels, format

  audio_device = SDL_AUDIO_DEVICE_DEFAULT_OUTPUT;
  audio_format = SAMPLE_S16;

  memset(&audio_settings, 0, sizeof(audio_settings));
  if(SDL_GetAudioDeviceFormat(audio_device, &audio_settings, NULL) < 0)
  {
    // Can't query, try to continue anyway...
    audio_settings.freq = 48000;
  }
  audio_settings.format = SDL_AUDIO_S16;
  audio_settings.channels = 2;

  if(conf->audio_sample_rate != 0)
  {
    // Reject very low sample rates to avoid PulseAudio hangs.
    audio_settings.freq = MAX(conf->audio_sample_rate, 2048);
  }

  if(!audio_mixer_init(audio_settings.freq, conf->audio_buffer_samples,
   audio_settings.channels))
    return;

  audio_settings.freq = audio.output_frequency;

  tmp = crealloc(audio_buffer,
   audio.buffer_frames * audio.buffer_channels * sizeof(int16_t));
  if(!tmp)
    return;

  audio_buffer = tmp;
  audio_stream = SDL_OpenAudioDeviceStream(audio_device, &audio_settings,
   sdl_audio_callback, tmp);
  if(!audio_stream)
    goto err;

  SDL_ResumeAudioDevice(audio_device);
  return;

err:
  free(tmp);
  audio_buffer = NULL;
  return;
}

void quit_audio_platform(void)
{
  if(audio_stream)
  {
    SDL_DestroyAudioStream(audio_stream);
    audio_stream = NULL;
  }
  free(audio_buffer);
  audio_buffer = NULL;
}
