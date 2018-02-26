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

#include "platform.h"
#include "data.h"
#include "configure.h"
#include "fsafeopen.h"
#include "util.h"

#include "audio.h"
#include "stream_pcs.h"
#include "stream_registry.h"
#include "stream_sampled.h"

#if defined(CONFIG_MODPLUG) + defined(CONFIG_MIKMOD) + \
 defined(CONFIG_XMP) + defined(CONFIG_OPENMPT) > 1

#error Can only have one module system enabled concurrently!
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

// May be used by audio plugins
struct audio audio;

#define __lock()      platform_mutex_lock(&audio.audio_mutex)
#define __unlock()    platform_mutex_unlock(&audio.audio_mutex)

#ifdef DEBUG

#define LOCK()   lock(__FILE__, __LINE__)
#define UNLOCK() unlock(__FILE__, __LINE__)

static volatile int locked = 0;
static volatile char last_lock[16];

static void lock(const char *file, int line)
{
  // lock should _not_ be held here
  if(locked)
    debug("%s:%d: locked at %s already!\n", file, line, last_lock);

  // acquire the mutex
  __lock();
  locked = 1;

  // store information on this lock
  snprintf((char *)last_lock, 16, "%s:%d", file, line);
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

void construct_audio_stream(struct audio_stream *a_src,
 Uint32 (* mix_data)(struct audio_stream *a_src, Sint32 *buffer, Uint32 len),
 void (* set_volume)(struct audio_stream *a_src, Uint32 volume),
 void (* set_repeat)(struct audio_stream *a_src, Uint32 repeat),
 void (* set_order)(struct audio_stream *a_src, Uint32 order),
 void (* set_position)(struct audio_stream *a_src, Uint32 pos),
 Uint32 (* get_order)(struct audio_stream *a_src),
 Uint32 (* get_position)(struct audio_stream *a_src),
 Uint32 (* get_length)(struct audio_stream *a_src),
 void (* destruct)(struct audio_stream *a_src),
 Uint32 volume, Uint32 repeat)
{
  a_src->mix_data = mix_data;
  a_src->set_volume = set_volume;
  a_src->set_repeat = set_repeat;
  a_src->set_order = set_order;
  a_src->set_position = set_position;
  a_src->get_order = get_order;
  a_src->get_position = get_position;
  a_src->get_length = get_length;
  a_src->destruct = destruct;

  if(set_volume)
    set_volume(a_src, volume);

  if(set_repeat)
    set_repeat(a_src, repeat);

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

  init_vorbis(conf);
  init_wav(conf);

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

  set_music_volume(conf->music_volume);
  set_sound_volume(conf->sam_volume);
  set_music_on(conf->music_on);
  set_sfx_on(conf->pc_speaker_on);

  init_pc_speaker(conf);

  set_sfx_volume(conf->pc_speaker_volume);

  init_audio_platform(conf);
}

void quit_audio(void)
{
  quit_audio_platform();
  audio_free_registry();
  free(audio.pcs_stream);
}

/* If the mod was successfully changed, return 1.  This value is used
*  to determine whether to change real_mod_playing.
*/
int load_module(char *filename, bool safely,
 int volume)
{
  char translated_filename[MAX_PATH];
  struct audio_stream *a_src;

  // FIXME: Weird hack, why not use end_module() directly?
  if(!filename || !filename[0])
  {
    end_module();
    return 1;
  }

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename) != FSAFE_SUCCESS)
    {
      debug("Module filename '%s' failed safety checks\n", filename);
      return 0;
    }

    filename = translated_filename;
  }

  end_module();

  a_src = construct_stream_audio_file(filename, 0,
   volume * audio.music_volume / 8, 1);

  LOCK();

  audio.primary_stream = a_src;

  UNLOCK();
  return 1;
}

void end_module(void)
{
  if(audio.primary_stream)
  {
    LOCK();
    audio.primary_stream->destruct(audio.primary_stream);
    UNLOCK();
  }

  audio.primary_stream = NULL;
}

void set_max_samples(int max_samples)
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

int get_max_samples(void)
{
  return audio.max_simultaneous_samples;
}

static void limit_samples(int max)
{
  int samples_playing = 0;
  int cancel_num = 0;
  struct audio_stream *current_astream = audio.stream_list_base;
  struct audio_stream *next_astream;

  if (max == -1) return; // No limit

  // We want to limit the # of samples playing.

  LOCK();

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
  if (cancel_num > 0) {
    current_astream = audio.stream_list_base;
    while(current_astream)
    {
      next_astream = current_astream->next;

      if((current_astream != audio.primary_stream) &&
      (current_astream != (struct audio_stream *)(audio.pcs_stream)))
      {
        current_astream->destruct(current_astream);
        cancel_num--;
        if (cancel_num <= 0) break;
      }

      current_astream = next_astream;
    }
  }

  UNLOCK();
}

void play_sample(int freq, char *filename, bool safely)
{
  Uint32 vol = 255 * audio.sound_volume / 8;
  char translated_filename[MAX_PATH];

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename) != FSAFE_SUCCESS)
    {
      debug("Sample filename '%s' failed safety checks\n", filename);
      return;
    }

    filename = translated_filename;
  }

  if(freq == 0)
  {
    construct_stream_audio_file(filename, 0, vol, 0);
  }
  else
  {
    construct_stream_audio_file(filename,
     (freq_conversion / freq) / 2, vol, 0);
  }

  limit_samples(audio.max_simultaneous_samples);
}

void end_sample(void)
{
  // Destroy all samples - something is a sample if it's not a
  // primary or PC speaker stream. This is a bit of a dirty way
  // to do it though (might want to keep multiple lists instead)

  struct audio_stream *current_astream = audio.stream_list_base;
  struct audio_stream *next_astream;

  LOCK();

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

void jump_module(int order)
{
  // This is really just for modules, I can't imagine what an "order"
  // might be for non-sequenced formats. It's also mostly here for
  // legacy reasons; set_position rather supercedes it.

  if(audio.primary_stream && audio.primary_stream->set_order)
  {
    LOCK();
    audio.primary_stream->set_order(audio.primary_stream, order);
    UNLOCK();
  }
}

int get_order(void)
{
  if(audio.primary_stream && audio.primary_stream->get_order)
  {
    int order;

    LOCK();
    order = audio.primary_stream->get_order(audio.primary_stream);
    UNLOCK();

    return order;
  }
  else
  {
    return 0;
  }
}

void volume_module(int vol)
{
  if(audio.primary_stream)
  {
    LOCK();
    audio.primary_stream->set_volume(audio.primary_stream,
     vol * audio.music_volume / 8);
    UNLOCK();
  }
}

void shift_frequency(int freq)
{
  // Primary had better be a sampled stream (in reality I can't imagine
  // ever letting it be anything but, but if it comes up a type
  // enumeration could weed this out)

  // Note that shifting the frequency dynamically messes up the phase
  // counters somewhat producing an audible pop. I've tried to reduce
  // this without too much success... This might be less noticeable
  // when interpolation isn't used (but the tradeoff is hardly worth it)

  if(audio.primary_stream && freq >= 16)
  {
    LOCK();
    ((struct sampled_stream *)audio.primary_stream)->
     set_frequency((struct sampled_stream *)audio.primary_stream,
     freq);
    UNLOCK();
  }
}

int get_frequency(void)
{
  if(audio.primary_stream)
  {
    int freq;

    LOCK();
    freq = ((struct sampled_stream *)audio.primary_stream)->
     get_frequency((struct sampled_stream *)audio.primary_stream);
    UNLOCK();

    return freq;
  }
  else
  {
    return 0;
  }
}

void set_position(int pos)
{
  // Position isn't a universal thing and instead depends on the
  // medium and what it supports.

  if(audio.primary_stream && audio.primary_stream->set_position)
  {
    LOCK();
    audio.primary_stream->set_position(audio.primary_stream, pos);
    UNLOCK();
  }
}

int get_position(void)
{
  if(audio.primary_stream && audio.primary_stream->get_position)
  {
    int pos;

    LOCK();
    pos = audio.primary_stream->get_position(audio.primary_stream);
    UNLOCK();

    return pos;
  }

  return 0;
}

int audio_get_length(void)
{
  if(audio.primary_stream && audio.primary_stream->get_length)
  {
    int length;

    LOCK();
    length = audio.primary_stream->get_length(audio.primary_stream);
    UNLOCK();

    return length;
  }

  return 0;
}

void set_music_on(int val)
{
  LOCK();
  audio.music_on = val;
  UNLOCK();
}

void set_sfx_on(int val)
{
  LOCK();
  audio.sfx_on = val;
  UNLOCK();
}

// These don't have to be locked because only the same thread can
// modify them.

int get_music_on_state(void)
{
  return audio.music_on;
}

int get_sfx_on_state(void)
{
  return audio.sfx_on;
}

int get_music_volume(void)
{
  return audio.music_volume;
}

int get_sound_volume(void)
{
  return audio.sound_volume;
}

int get_sfx_volume(void)
{
  return audio.sfx_volume;
}

void set_music_volume(int volume)
{
  LOCK();
  audio.music_volume = volume;
  UNLOCK();
}

void set_sound_volume(int volume)
{
  struct audio_stream *current_astream = audio.stream_list_base;

  LOCK();

  audio.sound_volume = volume;

  while(current_astream)
  {
    if((current_astream != audio.primary_stream) &&
     (current_astream != audio.pcs_stream))
    {
      current_astream->set_volume(current_astream,
       audio.sound_volume * 255 / 8);
    }

    current_astream = current_astream->next;
  }

  UNLOCK();
}

void set_sfx_volume(int volume)
{
  if(!audio.pcs_stream)
    return;

  LOCK();

  audio.sfx_volume = volume;
  audio.pcs_stream->set_volume(audio.pcs_stream, volume * 255 / 8);

  UNLOCK();
}
