/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

// Code to handle module playing, sample playing, and PC speaker
// sfx emulation.

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sys/stat.h>

#include "audio.h"
#include "audio_pcs.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../configure.h"
#include "../data.h"
#include "../fsafeopen.h"
#include "../platform.h"
#include "../util.h"

#if defined(CONFIG_MODPLUG) + defined(CONFIG_MIKMOD) + \
 defined(CONFIG_XMP) + defined(CONFIG_OPENMPT) > 1

#error Can only have one module system enabled concurrently!
#endif

#include "audio_wav.h"

#if defined(CONFIG_VORBIS)
#include "audio_vorbis.h"
#endif

#ifdef CONFIG_MODPLUG
#include "audio_modplug.h"
#include "gdm2s3m.h"
#endif

#ifdef CONFIG_MIKMOD
#include "audio_mikmod.h"
#endif

#ifdef CONFIG_XMP
#include "audio_xmp.h"
#endif

#ifdef CONFIG_OPENMPT
#include "audio_openmpt.h"
#endif

#ifdef CONFIG_REALITY
#include "audio_reality.h"
#endif

// May be used by audio plugins
struct audio audio;

#define __lock()      platform_mutex_lock(&audio.audio_mutex)
#define __unlock()    platform_mutex_unlock(&audio.audio_mutex)

#ifdef DEBUG

#define LOCK()   lock(__FILE__, __LINE__)
#define UNLOCK() unlock(__FILE__, __LINE__)

static volatile int locked = 0;
static volatile char last_lock[32];

#ifdef CONFIG_SDL
#include "../compat_sdl.h"
#include <SDL_thread.h>
static volatile SDL_threadID last_thread = 0;
#endif

static void lock(const char *file, int line)
{
#ifdef CONFIG_SDL
  // lock may be held here, but it shouldn't be held by the current thread.
  // If this is SDL, we can determine if the current thread is holding it.
  // Otherwise, print nothing because this debug message is annoying and is
  // almost always spurious.
  SDL_threadID cur_thread = SDL_ThreadID();

  if(locked && (last_thread == cur_thread))
  {
    debug("%s:%d (thread %ld): locked at %s (thread %ld) already!\n",
     file, line, cur_thread, last_lock, last_thread);
  }
#endif

  // acquire the mutex
  __lock();

  // store information on this lock
  snprintf((char *)last_lock, 32, "%s:%d", file, line);
  last_lock[31] = '\0';
#ifdef CONFIG_SDL
  last_thread = SDL_ThreadID();
#endif

  locked = 1;
}

static void unlock(const char *file, int line)
{
  // lock should be held here
  if(!locked)
    debug("%s:%d: tried to unlock when not locked!\n", file, line);

  // all ok, unlock this mutex
  locked = 0;
  __unlock();
}

#else // !DEBUG

#define LOCK()     __lock()
#define UNLOCK()   __unlock()

#endif // DEBUG

static const int freq_conversion = 3579364;

int audio_get_real_frequency(int period)
{
  /* Convert MZX's "frequencies" to an actual usable frequency. */
  return freq_conversion / period;
}

static int volume_function(int input, int volume_setting)
{
  /* Adjust volume (0-255) exponentially according to a given setting (0-10).
   * 0 is no volume whatsoever and 10 is maximum volume. */

  float setting_f = volume_setting / 10.0f;
  float setting_exp = (expf(setting_f) - 1) / (M_E - 1);
  int output = (int)((float)input * setting_exp + 0.5f);

  return CLAMP(output, 0, 255);
}

void destruct_audio_stream(struct audio_stream *a_src)
{
  if(a_src == audio.stream_list_base)
    audio.stream_list_base = a_src->next;

  if(a_src == audio.stream_list_end)
    audio.stream_list_end = a_src->previous;

  if(a_src->next)
    a_src->next->previous = a_src->previous;

  if(a_src->previous)
    a_src->previous->next = a_src->next;

  free(a_src);
}

void initialize_audio_stream(struct audio_stream *a_src,
 struct audio_stream_spec *a_spec, Uint32 volume, Uint32 repeat)
{
  // TODO should probably just memcpy into a spec in the audio_stream instead.
  a_src->mix_data = a_spec->mix_data;
  a_src->set_volume = a_spec->set_volume;
  a_src->set_repeat = a_spec->set_repeat;
  a_src->set_order = a_spec->set_order;
  a_src->set_position = a_spec->set_position;
  a_src->set_loop_start = a_spec->set_loop_start;
  a_src->set_loop_end = a_spec->set_loop_end;
  a_src->get_order = a_spec->get_order;
  a_src->get_position = a_spec->get_position;
  a_src->get_length = a_spec->get_length;
  a_src->get_loop_start = a_spec->get_loop_start;
  a_src->get_loop_end = a_spec->get_loop_end;
  a_src->get_sample = a_spec->get_sample;
  a_src->destruct = a_spec->destruct;
  a_src->is_spot_sample = false;

  if(a_src->set_volume)
    a_src->set_volume(a_src, volume);

  if(a_src->set_repeat)
    a_src->set_repeat(a_src, repeat);

  a_src->next = NULL;

  LOCK();

  if(audio.stream_list_base == NULL)
  {
    audio.stream_list_base = a_src;
  }
  else
  {
    audio.stream_list_end->next = a_src;
  }

  a_src->previous = audio.stream_list_end;
  audio.stream_list_end = a_src;

  UNLOCK();
}

static void clip_buffer(Sint16 *dest, Sint32 *src, int len)
{
  Sint32 cur_sample;
  int i;

  for(i = 0; i < len; i++)
  {
    cur_sample = src[i];
    if(cur_sample > 32767)
      cur_sample = 32767;

    if(cur_sample < -32768)
      cur_sample = -32768;

    dest[i] = cur_sample;
  }
}

void audio_callback(Sint16 *stream, int len)
{
  Uint32 destroy_flag;
  struct audio_stream *current_astream;

  LOCK();

  current_astream = audio.stream_list_base;

  if(current_astream)
  {
    memset(audio.mix_buffer, 0, len * 2);

    while(current_astream != NULL)
    {
      struct audio_stream *next_astream = current_astream->next;

      destroy_flag = current_astream->mix_data(current_astream,
       audio.mix_buffer, len);

      if(destroy_flag)
      {
        current_astream->destruct(current_astream);

        // if the destroyed stream was our music, we shouldn't
        // let end_mod try to destroy it again.
        if(current_astream == audio.primary_stream)
          audio.primary_stream = NULL;
      }

      current_astream = next_astream;
    }

    clip_buffer(stream, audio.mix_buffer, len / 2);
  }

  UNLOCK();
}

void init_audio(struct config_info *conf)
{
  platform_mutex_init(&audio.audio_mutex);

  audio.output_frequency = conf->output_frequency;
  audio.master_resample_mode = conf->resample_mode;

  audio.max_simultaneous_samples = -1;
  audio.max_simultaneous_samples_config = conf->max_simultaneous_samples;

  init_wav(conf);

#ifdef CONFIG_VORBIS
  init_vorbis(conf);
#endif

#ifdef CONFIG_MODPLUG
  init_modplug(conf);
#endif

#ifdef CONFIG_MIKMOD
  init_mikmod(conf);
#endif

#ifdef CONFIG_XMP
  init_xmp(conf);
#endif

#ifdef CONFIG_OPENMPT
  init_openmpt(conf);
#endif

#ifdef CONFIG_REALITY
  init_reality(conf);
#endif

  audio_set_music_volume(conf->music_volume);
  audio_set_sound_volume(conf->sam_volume);
  audio_set_music_on(conf->music_on);
  audio_set_pcs_on(conf->pc_speaker_on);

  init_pc_speaker(conf);

  audio_set_pcs_volume(conf->pc_speaker_volume);

  init_audio_platform(conf);
}

void quit_audio(void)
{
  // Signal the audio thread to stop and wait for it to release the lock.
  quit_audio_platform();

  LOCK();

  audio_ext_free_registry();
  free(audio.pcs_stream);

  UNLOCK();
}

/* If the mod was successfully changed, return 1.  This value is used
*  to determine whether to change real_mod_playing.
*/
int audio_play_module(char *filename, boolean safely, int volume)
{
  char translated_filename[MAX_PATH];
  struct audio_stream *a_src;
  int real_volume;

  if(!filename || !filename[0])
  {
    debug("audio_play_module received empty filename!\n");
    return 0;
  }

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename) != FSAFE_SUCCESS &&
     audio_legacy_translate(filename, translated_filename) != FSAFE_SUCCESS)
    {
      debug("Module filename '%s' failed safety checks\n", filename);
      return 0;
    }

    filename = translated_filename;
  }

  audio_end_module();

  real_volume = volume_function(volume, audio.music_volume);
  a_src = audio_ext_construct_stream(filename, 0, real_volume, 1);

  LOCK();

  audio.primary_stream = a_src;

  UNLOCK();
  return 1;
}

void audio_end_module(void)
{
  if(audio.primary_stream)
  {
    struct audio_stream *current_astream;

    LOCK();

    // Ensure that this didn't change while waiting for the lock.
    if(audio.primary_stream)
    {
      audio.primary_stream->destruct(audio.primary_stream);
      audio.primary_stream = NULL;
    }

    // Also end any sound effects attached to the mod.
    current_astream = audio.stream_list_base;
    while(current_astream)
    {
      struct audio_stream *next_astream = current_astream->next;

      if(current_astream->is_spot_sample)
        current_astream->destruct(current_astream);

      current_astream = next_astream;
    }

    UNLOCK();
  }
}

void audio_set_max_samples(int max_samples)
{
  // -1 is unlimited
  int max_samples_config = audio.max_simultaneous_samples_config;

  if(max_samples_config >= 0)
  {
    if((max_samples_config < max_samples) || (max_samples < 0))
      max_samples = max_samples_config;
  }

  audio.max_simultaneous_samples = max_samples;
}

int audio_get_max_samples(void)
{
  return audio.max_simultaneous_samples;
}

static void limit_samples(int max)
{
  int samples_playing = 0;
  int cancel_num = 0;
  struct audio_stream *current_astream;
  struct audio_stream *next_astream;

  // Don't limit samples if the max samples setting is -1.
  if(max == -1)
    return;

  LOCK();

  current_astream = audio.stream_list_base;
  while(current_astream)
  {
    next_astream = current_astream->next;

    if((current_astream != audio.primary_stream) &&
     (current_astream != (struct audio_stream *)(audio.pcs_stream)))
    {
      samples_playing++;
    }

    current_astream = next_astream;
  }

  cancel_num = samples_playing - max;
  if(cancel_num > 0)
  {
    current_astream = audio.stream_list_base;
    while(current_astream && cancel_num > 0)
    {
      next_astream = current_astream->next;

      if((current_astream != audio.primary_stream) &&
       (current_astream != (struct audio_stream *)(audio.pcs_stream)))
      {
        current_astream->destruct(current_astream);
        cancel_num--;
      }

      current_astream = next_astream;
    }
  }

  UNLOCK();
}

void audio_play_sample(char *filename, boolean safely, int period)
{
  Uint32 vol = volume_function(255, audio.sound_volume);
  char translated_filename[MAX_PATH];

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename) != FSAFE_SUCCESS &&
     audio_legacy_translate(filename, translated_filename) != FSAFE_SUCCESS)
    {
      debug("Sample filename '%s' failed safety checks\n", filename);
      return;
    }

    filename = translated_filename;
  }

  if(period == 0)
  {
    // Use 0 to instruct handler to get default frequency
    audio_ext_construct_stream(filename, 0, vol, 0);
  }
  else
  {
    audio_ext_construct_stream(filename,
     audio_get_real_frequency(period * 2), vol, 0);
  }

  limit_samples(audio.max_simultaneous_samples);
}

void audio_spot_sample(int period, int which)
{
  // Play a sample from the current playing mod.
  // Currently only works with libxmp (and maybe only ever will).

  Uint32 vol = volume_function(255, audio.sound_volume);
  struct wav_info wav;
  boolean ret = false;

  memset(&wav, 0, sizeof(struct wav_info));

  LOCK();

  if(audio.primary_stream && audio.primary_stream->get_sample)
    ret = audio.primary_stream->get_sample(audio.primary_stream, which, &wav);

  UNLOCK();

  if(ret)
  {
    struct audio_stream *a_src = construct_wav_stream_direct(&wav,
     audio_get_real_frequency(period * 2), vol, !!(wav.loop_end));
    a_src->is_spot_sample = true;

    limit_samples(audio.max_simultaneous_samples);
  }
}

void audio_end_sample(void)
{
  // Destroy all samples - something is a sample if it's not a
  // primary or PC speaker stream. This is a bit of a dirty way
  // to do it though (might want to keep multiple lists instead)

  struct audio_stream *current_astream;
  struct audio_stream *next_astream;

  LOCK();

  current_astream = audio.stream_list_base;
  while(current_astream)
  {
    next_astream = current_astream->next;

    if((current_astream != audio.primary_stream) &&
     (current_astream != (struct audio_stream *)(audio.pcs_stream)))
    {
      current_astream->destruct(current_astream);
    }

    current_astream = next_astream;
  }

  UNLOCK();
}

void audio_set_module_order(int order)
{
  // This is intended for modules only, and should not be supported for any
  // other formats.

  LOCK();

  if(audio.primary_stream && audio.primary_stream->set_order)
    audio.primary_stream->set_order(audio.primary_stream, order);

  UNLOCK();
}

int audio_get_module_order(void)
{
  int order = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->get_order)
    order = audio.primary_stream->get_order(audio.primary_stream);

  UNLOCK();

  return order;
}

void audio_set_module_volume(int volume)
{
  int real_volume = volume_function(volume, audio.music_volume);

  LOCK();

  if(audio.primary_stream)
    audio.primary_stream->set_volume(audio.primary_stream, real_volume);

  UNLOCK();
}

void audio_set_module_frequency(int freq)
{
  // Primary had better be a sampled stream (in reality I can't imagine
  // ever letting it be anything but, but if it comes up a type
  // enumeration could weed this out)

  // Note that shifting the frequency dynamically messes up the phase
  // counters somewhat producing an audible pop. I've tried to reduce
  // this without too much success... This might be less noticeable
  // when interpolation isn't used (but the tradeoff is hardly worth it)

  if(freq >= 16)
  {
    LOCK();

    if(audio.primary_stream)
    {
      struct sampled_stream *s = (struct sampled_stream *)audio.primary_stream;
      s->set_frequency(s, freq);
    }

    UNLOCK();
  }
}

int audio_get_module_frequency(void)
{
  int freq = 0;

  LOCK();

  if(audio.primary_stream)
  {
    struct sampled_stream *s = (struct sampled_stream *)audio.primary_stream;
    freq = s->get_frequency(s);
  }

  UNLOCK();

  return freq;
}

void audio_set_module_position(int pos)
{
  // Position isn't a universal thing and instead depends on the
  // medium and what it supports.

  LOCK();

  if(audio.primary_stream && audio.primary_stream->set_position)
    audio.primary_stream->set_position(audio.primary_stream, pos);

  UNLOCK();
}

int audio_get_module_position(void)
{
  int pos = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->get_position)
    pos = audio.primary_stream->get_position(audio.primary_stream);

  UNLOCK();

  return pos;
}

int audio_get_module_length(void)
{
  int length = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->get_length)
    length = audio.primary_stream->get_length(audio.primary_stream);

  UNLOCK();

  return length;
}

void audio_set_module_loop_start(int pos)
{
  LOCK();

  if(audio.primary_stream && audio.primary_stream->set_loop_start)
    audio.primary_stream->set_loop_start(audio.primary_stream, pos);

  UNLOCK();
}

int audio_get_module_loop_start(void)
{
  int loop_start = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->get_loop_start)
    loop_start = audio.primary_stream->get_loop_start(audio.primary_stream);

  UNLOCK();

  return loop_start;
}

void audio_set_module_loop_end(int pos)
{
  LOCK();

  if(audio.primary_stream && audio.primary_stream->set_loop_end)
    audio.primary_stream->set_loop_end(audio.primary_stream, pos);

  UNLOCK();
}

int audio_get_module_loop_end(void)
{
  int loop_end = 0;

  LOCK();

  if(audio.primary_stream && audio.primary_stream->get_loop_end)
    loop_end = audio.primary_stream->get_loop_end(audio.primary_stream);

  UNLOCK();

  return loop_end;
}

void audio_set_music_on(int val)
{
  LOCK();
  audio.music_on = val;
  UNLOCK();
}

void audio_set_pcs_on(int val)
{
  LOCK();
  audio.pcs_on = val;
  UNLOCK();
}

// These don't have to be locked because only one thread can
// modify them.

int audio_get_music_on(void)
{
  return audio.music_on;
}

int audio_get_pcs_on(void)
{
  return audio.pcs_on;
}

int audio_get_music_volume(void)
{
  return audio.music_volume;
}

int audio_get_sound_volume(void)
{
  return audio.sound_volume;
}

int audio_get_pcs_volume(void)
{
  return audio.pcs_volume;
}

void audio_set_music_volume(int volume)
{
  LOCK();
  audio.music_volume = volume;
  UNLOCK();
}

void audio_set_sound_volume(int volume)
{
  struct audio_stream *current_astream;
  int real_volume;

  LOCK();

  audio.sound_volume = volume;
  real_volume = volume_function(255, audio.sound_volume);

  current_astream = audio.stream_list_base;
  while(current_astream)
  {
    if((current_astream != audio.primary_stream) &&
     (current_astream != audio.pcs_stream))
    {
      current_astream->set_volume(current_astream, real_volume);
    }

    current_astream = current_astream->next;
  }

  UNLOCK();
}

void audio_set_pcs_volume(int volume)
{
  int real_volume;
  if(!audio.pcs_stream)
    return;

  LOCK();

  audio.pcs_volume = volume;
  real_volume = volume_function(255, audio.pcs_volume);

  audio.pcs_stream->set_volume(audio.pcs_stream, real_volume);

  UNLOCK();
}

/**
 * Wrapper for fsafetranslate. Prior to 2.83, the audio code would apply
 * special translations to certain filenames BEFORE the fsafetranslate step
 * that is now in audio_play_module and audio_play_sample; due to this change,
 * certain quirks of the old engine relied on by some games stopped working:
 *
 * + Requests to play files with the .SAM extension would first try to play a
 *   .WAV with the same name even if the .SAM didn't exist at all.
 * + Requests to play files with the .GDM extension would first try to play an
 *   .S3M with the same name even if the .GDM didn't exist at all.
 *
 * This function provides a compatibility layer for this old behavior; use
 * after the initial fsafetranslate fails.
 */
int audio_legacy_translate(const char *path, char newpath[MAX_PATH])
{
  char temp[MAX_PATH];
  ssize_t ext_pos = strlen(path) - 4;

  if(ext_pos >= 0)
  {
    if(!strcasecmp(path + ext_pos, ".SAM"))
    {
      strcpy(temp, path);
      strcpy(temp + ext_pos, ".WAV");
      return fsafetranslate(temp, newpath);
    }
    else

    if(!strcasecmp(path + ext_pos, ".GDM"))
    {
      strcpy(temp, path);
      strcpy(temp + ext_pos, ".S3M");
      return fsafetranslate(temp, newpath);
    }
  }
  return -FSAFE_MATCH_FAILED;
}
