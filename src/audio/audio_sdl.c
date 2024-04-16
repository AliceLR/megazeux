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
  audio_callback((int16_t *)stream, len);
}

void init_audio_platform(struct config_info *conf)
{
  SDL_AudioSpec desired_spec =
  {
    0,
    AUDIO_S16SYS,
    2,
    0,
    conf->audio_buffer_samples,
    0,
    0,
    sdl_audio_callback,
    NULL
  };

  desired_spec.freq = audio.output_frequency;

#if SDL_VERSION_ATLEAST(2,0,0)
  audio_device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &audio_settings, 0);
#else
  SDL_OpenAudio(&desired_spec, &audio_settings);
#endif

  audio.mix_buffer = cmalloc(audio_settings.size * 2);
  audio.buffer_samples = audio_settings.samples;

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
  SDL_PauseAudioDevice(audio_device);
  SDL_DestroyAudioStream(audio_stream);
  free(audio_buffer);
  audio_stream = NULL;
  audio_buffer = NULL;
#elif SDL_VERSION_ATLEAST(2,0,0)
  SDL_PauseAudioDevice(audio_device, 1);
#else
  SDL_PauseAudio(1);
#endif
  free(audio.mix_buffer);
}
