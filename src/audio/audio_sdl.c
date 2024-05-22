/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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
#include "audio_struct.h"

#include "../SDLmzx.h"
#include "../util.h"

#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static SDL_AudioSpec audio_settings;
#if SDL_VERSION_ATLEAST(2,0,0)
static SDL_AudioDeviceID audio_device;
#endif

#if SDL_VERSION_ATLEAST(3,0,0)

static SDL_AudioStream *audio_stream;
static void *audio_buffer;

static void sdl_audio_callback(void *userdata, SDL_AudioStream *stream,
 int required_bytes, int requested_bytes)
{
  // TODO: convert to frames/channels/format.
  requested_bytes = (requested_bytes >> 2) << 2;
  audio_callback((int16_t *)userdata, requested_bytes);
}

// FIXME merge this into the older version.
void init_audio_platform(struct config_info *conf)
{
  void *tmp;
  // TODO: query native spec from audio stream: SDL_GetAudioDeviceFormat
  // TODO: configurable audio channels, default frequency setting
  SDL_AudioSpec desired_spec =
  {
    SDL_AUDIO_S16,
    2,
    audio.output_frequency,
  };

  // FIXME
  tmp = crealloc(audio_buffer, conf->audio_buffer_samples * 4);
  if(!tmp)
    return;

  audio_device = 0; // FIXME
  audio_settings = desired_spec;
  audio_buffer = tmp;
  audio_stream =
   SDL_OpenAudioDeviceStream(0, &desired_spec, sdl_audio_callback, tmp);

  // FIXME this should be done by init_audio.
  audio.mix_buffer = (int32_t *)cmalloc(conf->audio_buffer_samples * 8);
  audio.buffer_samples = conf->audio_buffer_samples;
  SDL_ResumeAudioDevice(audio_device);
}

#else /* !SDL_VERSION_ATLEAST(3,0,0) */

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
  // TODO: non-stereo and 8-bit?
  size_t frames = len / (2 * sizeof(int16_t));
  audio_mixer_render_frames(stream, frames, 2, SAMPLE_S16);
}

void init_audio_platform(struct config_info *conf)
{
  SDL_AudioSpec desired_spec =
  {
    conf->audio_sample_rate,
    AUDIO_S16SYS,
    2,
    0,
    conf->audio_buffer_samples,
    0,
    0,
    sdl_audio_callback,
    NULL
  };

  // Reject very low sample rates to avoid PulseAudio hangs.
  if(conf->audio_sample_rate > 0 && conf->audio_sample_rate < 2048)
    desired_spec.freq = 2048;

#if SDL_VERSION_ATLEAST(2,0,0)
  audio_device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &audio_settings, 0);
  if(!audio_device)
  {
    warn("failed to init SDL audio device: %s\n", SDL_GetError());
    return;
  }
#else
  SDL_OpenAudio(&desired_spec, &audio_settings);
#endif

  if(!audio_mixer_init(audio_settings.freq, audio_settings.samples,
   audio_settings.channels))
    return;
  // If the mixer doesn't like SDL's selected frequency, exit to avoid bugs...
  if(audio.output_frequency != (size_t)audio_settings.freq)
    return;

  // now set the audio going
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_PauseAudioDevice(audio_device, 0);
#else
  SDL_PauseAudio(0);
#endif

#ifdef __EMSCRIPTEN__
  // Safari doesn't unlock the audio context unless a sound is played
  // immediately after an input event has been fired. This takes an audio
  // context that we set up earlier in JavaScript and sets up SDL2 to use that
  // instead of the one that it creates (since SDL's context will never be
  // unlocked).
  //
  // This swap should only happen if it's required by the web browser.
  EM_ASM({ window.replaceSdlAudioContext(Module["SDL2"]); });
#endif
}

#endif /* !SDL_VERSION_ATLEAST(3,0,0) */

void quit_audio_platform(void)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  if(audio_stream)
  {
    SDL_DestroyAudioStream(audio_stream);
    audio_stream = NULL;
  }
  free(audio_buffer);
  audio_buffer = NULL;
#elif SDL_VERSION_ATLEAST(2,0,0)
  if(audio_device)
  {
    SDL_PauseAudioDevice(audio_device, 1);
    SDL_CloseAudioDevice(audio_device);
    audio_device = 0;
  }
#else
  SDL_PauseAudio(1);
#endif
}
