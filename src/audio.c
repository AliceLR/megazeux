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
#include "audio.h"
#include "sfx.h"
#include "data.h"
#include "configure.h"
#include "fsafeopen.h"
#include "util.h"

// For WAV loader fallback
#ifdef CONFIG_SDL
#include "SDL.h"
#endif

#ifdef CONFIG_TREMOR
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#endif

#if defined(CONFIG_MODPLUG) && defined(CONFIG_MIKMOD)
#error Can only have one module system enabled concurrently!
#endif

#ifdef CONFIG_MODPLUG
#include "audio_modplug.h"
#include "gdm2s3m.h"
#endif

#ifdef CONFIG_MIKMOD
#include "audio_mikmod.h"
#endif

#define FP_SHIFT      13
#define FP_AND        ((1 << FP_SHIFT) - 1)

struct wav_info
{
  Uint32 channels;
  Uint32 freq;
  Uint16 format;
  Uint32 loop_start;
  Uint32 loop_end;
};

struct wav_stream
{
  struct sampled_stream s;
  Uint8 *wav_data;
  Uint32 data_offset;
  Uint32 data_length;
  Uint32 channels;
  Uint32 bytes_per_sample;
  Uint32 natural_frequency;
  Uint16 format;
  Uint32 loop_start;
  Uint32 loop_end;
};

struct vorbis_stream
{
  struct sampled_stream s;
  OggVorbis_File vorbis_file_handle;
  vorbis_info *vorbis_file_info;
  Uint32 loop_start;
  Uint32 loop_end;
};

static const int default_period = 428;

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

static const int freq_conversion = 3579364;

// Macros are used to generate functions to help reduce redundancy and
// maintain some kind of speed. Additional, fixed point is used (again,
// for speed purposes to avoid the hits in converting between fixed and
// floating point). I'm almost completely sure that fixed point is ideal
// for nearest and linear resampling but it might not be so good for
// cubic because it has to use 64bit ints and a lot of shifts
// (should work great on a 64bit machine though). The cubic resampler can
// be further optimized in other ways as well, and might be in due time.
// For now, if cubic doesn't give you good speed stick with linear which
// should be quite fast.

#define FLAT_SETUP_INDEX(channels)
#define NEAREST_SETUP_INDEX(channels)

#define FRACTIONAL_SETUP_INDEX(channels)                                \
  int_index = (Sint32)(s_index >> FP_SHIFT) * channels;                 \
  frac_index = (Sint32)(s_index & FP_AND);                              \

#define LINEAR_SETUP_INDEX(channels)                                    \
  FRACTIONAL_SETUP_INDEX(channels)                                      \

#define CUBIC_SETUP_INDEX(channels)                                     \
  FRACTIONAL_SETUP_INDEX(channels)                                      \

#define FLAT_MIX_SAMPLE(dest, channels, offset)                         \
  dest src_buffer[i2 + offset]                                          \

#define NEAREST_MIX_SAMPLE(dest, channels, offset)                      \
  dest src_buffer[((s_index >> FP_SHIFT) * channels) + offset]          \

#define LINEAR_MIX_SAMPLE(dest, channels, offset)                       \
  right_sample = src_buffer[int_index + offset];                        \
  dest (right_sample + ((frac_index *                                   \
   (src_buffer[int_index + channels + offset] - right_sample)) >>       \
   FP_SHIFT))                                                           \

#define CUBIC_MIX_SAMPLE(dest, channels, offset)                        \
  s0 = src_buffer[int_index - channels + offset] << FP_SHIFT;           \
  s1 = src_buffer[int_index + offset] << FP_SHIFT;                      \
  s2 = src_buffer[int_index + channels + offset] << FP_SHIFT;           \
  s3 = src_buffer[int_index + (channels * 2) + offset] << FP_SHIFT;     \
                                                                        \
  a = (((3 * (s1 - s2)) - s0 + s3) / 2);                                \
  b = ((2 * s2) + s0 - (((5 * s1) + s3) / 2));                          \
  c = ((s2 - s0) / 2);                                                  \
                                                                        \
  dest ((Sint32)(((((((((a * frac_index) >> FP_SHIFT) + b) *            \
   frac_index) >> FP_SHIFT) + c) * frac_index) >> FP_SHIFT) + s1) >>    \
   FP_SHIFT)                                                            \

#define RESAMPLE_LOOP_HEADER                                            \
  for(i = 0; i < write_len; i += 2, s_index += d)                       \
  {                                                                     \

#define FLAT_LOOP_HEADER(channels)                                      \
  for(i = 0, i2 = 0; i < write_len; i += 2, i2 += channels)             \
  {                                                                     \

#define NEAREST_LOOP_HEADER(dummy)                                      \
  RESAMPLE_LOOP_HEADER                                                  \

#define LINEAR_LOOP_HEADER(dummy)                                       \
  RESAMPLE_LOOP_HEADER                                                  \

#define CUBIC_LOOP_HEADER(dummy)                                        \
  RESAMPLE_LOOP_HEADER                                                  \

#define SPLIT_HEADER                                                    \
  Sint32 int_index;                                                     \
  Sint32 frac_index;                                                    \

#define RESAMPLE_HEADER                                                 \
  Sint64 s_index = s_src->sample_index;                                 \
  Sint64 d = s_src->frequency_delta;                                    \

#define FLAT_HEADER                                                     \
  Uint32 i2;                                                            \

#define NEAREST_HEADER                                                  \
  RESAMPLE_HEADER                                                       \

#define LINEAR_HEADER                                                   \
  RESAMPLE_HEADER                                                       \
  SPLIT_HEADER                                                          \
  Sint32 right_sample;                                                  \

#define CUBIC_HEADER                                                    \
  RESAMPLE_HEADER                                                       \
  SPLIT_HEADER                                                          \
  Sint32 s0, s1, s2, s3;                                                \
  Sint64 a, b, c;                                                       \

#define MIXER_FOOTER(channels)                                          \
  }                                                                     \
  s_index -= (s_src->data_window_length / (channels * 2)) << FP_SHIFT;  \
  s_src->sample_index = s_index;                                        \

#define FLAT_FOOTER(dummy)                                              \
  }                                                                     \
  s_src->sample_index = 0;                                              \

#define NEAREST_FOOTER(channels)                                        \
  MIXER_FOOTER(channels)                                                \

#define LINEAR_FOOTER(channels)                                         \
  MIXER_FOOTER(channels)                                                \

#define CUBIC_FOOTER(channels)                                          \
  MIXER_FOOTER(channels)                                                \

#define VOL                                                             \
  * volume / 256                                                        \

#define SETUP_MIXER(type, num, mod)                                     \
case num:                                                               \
{                                                                       \
  type##_HEADER                                                         \
  type##_LOOP_HEADER(2)                                                 \
  type##_SETUP_INDEX(2)                                                 \
  type##_MIX_SAMPLE(dest_buffer[i] +=, 2, 0) mod;                       \
  type##_MIX_SAMPLE(dest_buffer[i + 1] +=, 2, 1) mod;                   \
  type##_FOOTER(2)                                                      \
  break;                                                                \
}                                                                       \

#define SETUP_MIXER_MONO(type, num, mod)                                \
case num:                                                               \
{                                                                       \
  Sint32 current_sample;                                                \
  type##_HEADER                                                         \
  type##_LOOP_HEADER(1)                                                 \
  type##_SETUP_INDEX(1)                                                 \
  type##_MIX_SAMPLE(current_sample =, 1, 0) mod;                        \
  dest_buffer[i] += current_sample;                                     \
  dest_buffer[i + 1] += current_sample;                                 \
  type##_FOOTER(1)                                                      \
  break;                                                                \
}                                                                       \

#define SETUP_MIXER_ALL(type, num)                                      \
  SETUP_MIXER(type, (num * 4), )                                        \
  SETUP_MIXER_MONO(type, (num * 4) + 1, )                               \
  SETUP_MIXER(type, (num * 4) + 2, VOL)                                 \
  SETUP_MIXER_MONO(type, (num * 4) + 3, VOL)                            \


// WAV sample types

#define SAMPLE_U8     0
#define SAMPLE_S8     1
#define SAMPLE_S16LSB 2

// ov_read() documentation states the 'bigendianp' argument..
//   "Specifies big or little endian byte packing.
//   0 for little endian, 1 for big endian. Typical value is 0."

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
static const int ENDIAN_PACKING = 1;
#else
static const int ENDIAN_PACKING = 0;
#endif

static void sampled_negative_threshold(struct sampled_stream *s_src)
{
  Sint32 negative_threshold =
   ((s_src->frequency_delta + FP_AND) & ~FP_AND);

  if(s_src->sample_index < -negative_threshold)
  {
    s_src->sample_index += negative_threshold;
    s_src->negative_comp =
     (Uint32)(negative_threshold >> FP_SHIFT) * s_src->channels * 2;
    s_src->stream_offset += s_src->negative_comp;
  }
}

__audio_c_maybe_static void sampled_set_buffer(struct sampled_stream *s_src)
{
  Uint32 prologue_length = 4;
  Uint32 epilogue_length = 12;
  Uint32 bytes_per_sample = 2 * s_src->channels;
  Uint32 frequency = s_src->frequency;
  Uint32 data_window_length;
  Uint32 allocated_data_length;
  Sint64 frequency_delta;

  if(!frequency)
    frequency = audio.output_frequency;

  frequency_delta =
   ((Sint64)frequency << FP_SHIFT) / audio.output_frequency;

  s_src->frequency_delta = frequency_delta;
  s_src->negative_comp = 0;

  data_window_length =
   (Uint32)(ceil((double)audio.buffer_samples *
   frequency / audio.output_frequency) * bytes_per_sample);

  prologue_length +=
   (Uint32)ceil((double)frequency_delta) * bytes_per_sample;

  allocated_data_length = data_window_length + prologue_length +
   epilogue_length;

  if(allocated_data_length < bytes_per_sample)
    allocated_data_length = bytes_per_sample;

  s_src->data_window_length = data_window_length;
  s_src->allocated_data_length = allocated_data_length;
  s_src->prologue_length = prologue_length;
  s_src->epilogue_length = epilogue_length;
  s_src->stream_offset = prologue_length;

  s_src->output_data =
   crealloc(s_src->output_data, allocated_data_length);

  sampled_negative_threshold(s_src);
}

__audio_c_maybe_static void sampled_mix_data(struct sampled_stream *s_src,
 Sint32 *dest_buffer, Uint32 len)
{
  Uint8 *output_data = (Uint8 *)s_src->output_data;
  Sint16 *src_buffer =
   (Sint16 *)(output_data + s_src->prologue_length);
  Uint32 write_len = len / 2;
  Sint32 volume = ((struct audio_stream *)s_src)->volume;
  Uint32 resample_mode = audio.master_resample_mode + 1;
  Uint32 volume_mode = s_src->use_volume;
  Uint32 mono_mode = (s_src->channels == 1);
  Uint32 i;

  if(s_src->frequency == audio.output_frequency)
    resample_mode = 0;

  if(volume == 256)
    volume_mode = 0;

  switch((resample_mode << 2) | (volume_mode << 1) | mono_mode)
  {
    SETUP_MIXER_ALL(FLAT, 0)
    SETUP_MIXER_ALL(NEAREST, 1)
    SETUP_MIXER_ALL(LINEAR, 2)
    SETUP_MIXER_ALL(CUBIC, 3)
  }

  if(s_src->stream_offset == s_src->prologue_length)
    s_src->stream_offset += s_src->epilogue_length;

  if(s_src->negative_comp)
  {
    s_src->stream_offset -= s_src->negative_comp;
    s_src->negative_comp = 0;
  }

  sampled_negative_threshold(s_src);

  memmove(output_data, output_data +
   s_src->allocated_data_length - s_src->stream_offset,
   s_src->stream_offset);
}

static Uint32 vorbis_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  Uint32 read_len = 0;
  struct vorbis_stream *v_stream = (struct vorbis_stream *)a_src;
  Uint32 read_wanted = v_stream->s.allocated_data_length -
   v_stream->s.stream_offset;
  Uint32 pos = 0;
  char *read_buffer = (char *)v_stream->s.output_data +
   v_stream->s.stream_offset;
  int current_section;

  do
  {
    read_wanted -= read_len;

    if(a_src->repeat && v_stream->loop_end)
      pos = (Uint32)ov_pcm_tell(&v_stream->vorbis_file_handle);

#ifdef CONFIG_TREMOR
    read_len =
     ov_read(&(v_stream->vorbis_file_handle), read_buffer,
     read_wanted, &current_section);
#else
    read_len =
     ov_read(&(v_stream->vorbis_file_handle), read_buffer,
     read_wanted, ENDIAN_PACKING, 2, 1, &current_section);
#endif

    if(a_src->repeat && (pos < v_stream->loop_end)
     && (pos + read_len / v_stream->s.channels / 2 >= v_stream->loop_end))
    {
      read_len = (v_stream->loop_end - pos) * v_stream->s.channels * 2;
      ov_pcm_seek(&(v_stream->vorbis_file_handle), v_stream->loop_start);
    }

    // If it hit the end go back to the beginning if repeat is on

    if(read_len == 0)
    {
      if(a_src->repeat)
      {
        ov_raw_seek(&(v_stream->vorbis_file_handle), 0);

        if(read_wanted)
        {
#ifdef CONFIG_TREMOR
          read_len =
           ov_read(&(v_stream->vorbis_file_handle), read_buffer,
           read_wanted, &current_section);
#else
          read_len =
           ov_read(&(v_stream->vorbis_file_handle), read_buffer,
           read_wanted, ENDIAN_PACKING, 2, 1, &current_section);
#endif
        }
      }
      else
      {
        memset(read_buffer, 0, read_wanted);
      }
    }

    read_buffer += read_len;
  } while((read_len < read_wanted) && read_len);

  sampled_mix_data((struct sampled_stream *)v_stream, buffer, len);

  if(read_len == 0)
    return 1;

  return 0;
}

static void vorbis_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume * 256 / 255;
}

static void vorbis_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

static void vorbis_set_position(struct audio_stream *a_src, Uint32 position)
{
  ov_pcm_seek(&(((struct vorbis_stream *)a_src)->vorbis_file_handle),
   (ogg_int64_t)position);
}

static void vorbis_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
    frequency = ((struct vorbis_stream *)s_src)->vorbis_file_info->rate;

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 vorbis_get_position(struct audio_stream *a_src)
{
  struct vorbis_stream *v = (struct vorbis_stream *)a_src;
  return (Uint32)ov_pcm_tell(&v->vorbis_file_handle);
}

static Uint32 vorbis_get_frequency(struct sampled_stream *s_src)
{
  return s_src->frequency;
}

static Uint32 wav_read_data(struct wav_stream *w_stream, Uint8 *buffer,
 Uint32 len, Uint32 repeat)
{
  Uint8 *src = (Uint8 *)w_stream->wav_data + w_stream->data_offset;
  Uint32 data_read;
  Uint32 read_len = len;
  Uint32 new_offset;
  Uint32 i;

  switch(w_stream->format)
  {
    case SAMPLE_U8:
    {
      Sint16 *dest = (Sint16 *)buffer;

      read_len /= 2;

      new_offset = w_stream->data_offset + read_len;

      if(w_stream->data_offset + read_len >= w_stream->data_length)
      {
        read_len = w_stream->data_length - w_stream->data_offset;
        if(repeat)
          new_offset = 0;
        else
          new_offset = w_stream->data_length;
      }

      if(repeat && (w_stream->data_offset < w_stream->loop_end)
       && (w_stream->data_offset + read_len >= w_stream->loop_end))
      {
        read_len = w_stream->loop_end - w_stream->data_offset;
        new_offset = w_stream->loop_start;
      }

      data_read = read_len * 2;

      for(i = 0; i < read_len; i++)
      {
        dest[i] = (Sint8)(src[i] - 128) << 8;
      }

      break;
    }

    default:
    {
      Uint8 *dest = (Uint8 *) buffer;

      new_offset = w_stream->data_offset + read_len;

      if(w_stream->data_offset + read_len >= w_stream->data_length)
      {
        read_len = w_stream->data_length - w_stream->data_offset;
        if(repeat)
          new_offset = 0;
        else
          new_offset = w_stream->data_length;
      }

      if(repeat && (w_stream->data_offset < w_stream->loop_end)
       && (w_stream->data_offset + read_len >= w_stream->loop_end))
      {
        read_len = w_stream->loop_end - w_stream->data_offset;
        new_offset = w_stream->loop_start;
      }

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
      // swap bytes on big endian machines
      for(i = 0; i < read_len; i += 2)
      {
        dest[i] = src[i + 1];
        dest[i + 1] = src[i];
      }
#else
      // no swap necessary on little endian machines
      memcpy(dest, src, read_len);
#endif

      data_read = read_len;

      break;
    }
  }

  w_stream->data_offset = new_offset;

  return data_read;
}

static Uint32 wav_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  Uint32 read_len = 0;
  struct wav_stream *w_stream = (struct wav_stream *)a_src;
  Uint32 read_wanted = w_stream->s.allocated_data_length -
   w_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)w_stream->s.output_data +
   w_stream->s.stream_offset;

  read_len = wav_read_data(w_stream, read_buffer, read_wanted, a_src->repeat);

  if(read_len < read_wanted)
  {
    read_buffer += read_len;
    read_wanted -= read_len;

    if(a_src->repeat)
    {
      read_len = wav_read_data(w_stream, read_buffer, read_wanted, true);
    }
    else
    {
      memset(read_buffer, 0, read_wanted);
      read_len = 0;
    }
  }

  sampled_mix_data((struct sampled_stream *)w_stream, buffer, len);

  if(read_len == 0)
    return 1;

  return 0;
}

static void wav_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume * 256 / 255;
}

static void wav_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

static void wav_set_position(struct audio_stream *a_src, Uint32 position)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  if(position < (w_stream->data_length / w_stream->bytes_per_sample))
    w_stream->data_offset = position * w_stream->bytes_per_sample;
}

static void wav_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
    frequency = ((struct wav_stream *)s_src)->natural_frequency;

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 wav_get_position(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  return w_stream->data_offset / w_stream->bytes_per_sample;
}

static Uint32 wav_get_frequency(struct sampled_stream *s_src)
{
  return s_src->frequency;
}

static Uint32 pcs_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  struct pc_speaker_stream *pcs_stream = (struct pc_speaker_stream *)a_src;
  Uint32 offset = 0, i;
  Uint32 sample_duration = pcs_stream->last_duration;
  Uint32 end_duration;
  Uint32 increment_value, increment_buffer;
  Uint32 sfx_scale = (pcs_stream->volume + 1) * 32;
  Uint32 sfx_scale_half = sfx_scale / 2;
  Sint32 *mix_dest_ptr = buffer;
  Sint16 cur_sample;

  if(sample_duration >= len / 4)
  {
    end_duration = len / 4;
    pcs_stream->last_duration = sample_duration - end_duration;
    sample_duration = end_duration;
  }

  if(pcs_stream->last_playing)
  {
    increment_value =
     (Uint32)((float)pcs_stream->last_frequency /
     (audio.output_frequency) * 4294967296.0);

    increment_buffer = pcs_stream->last_increment_buffer;

    for(i = 0; i < sample_duration; i++)
    {
      cur_sample = (Uint32)((increment_buffer & 0x80000000) >> 31) *
       sfx_scale - sfx_scale_half;
      mix_dest_ptr[0] += cur_sample;
      mix_dest_ptr[1] += cur_sample;
      mix_dest_ptr += 2;
      increment_buffer += increment_value;
    }
    pcs_stream->last_increment_buffer = increment_buffer;
  }
  else
  {
    mix_dest_ptr += (sample_duration * 2);
  }

  offset += sample_duration;

  if(offset < len / 4)
    pcs_stream->last_playing = 0;

  while(offset < len / 4)
  {
    sound_system();
    sample_duration = (Uint32)((float)audio.output_frequency / 500 *
     pcs_stream->note_duration);

    if(offset + sample_duration >= len / 4)
    {
      end_duration = (len / 4) - offset;
      pcs_stream->last_duration = sample_duration - end_duration;
      pcs_stream->last_playing = pcs_stream->playing;
      pcs_stream->last_frequency = pcs_stream->frequency;

      sample_duration = end_duration;
    }

    offset += sample_duration;

    if(pcs_stream->playing)
    {
      increment_value =
       (Uint32)((float)pcs_stream->frequency /
       audio.output_frequency * 4294967296.0);
      increment_buffer = 0;

      for(i = 0; i < sample_duration; i++)
      {
        cur_sample = (Uint32)((increment_buffer & 0x80000000) >> 31) *
         sfx_scale - sfx_scale_half;
        mix_dest_ptr[0] += cur_sample;
        mix_dest_ptr[1] += cur_sample;
        mix_dest_ptr += 2;
        increment_buffer += increment_value;
      }
      pcs_stream->last_increment_buffer = increment_buffer;
    }
    else
    {
      mix_dest_ptr += (sample_duration * 2);
    }
  }

  return 0;
}

static void pcs_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  ((struct pc_speaker_stream *)a_src)->volume = volume;
}

static void destruct_audio_stream(struct audio_stream *a_src)
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

__audio_c_maybe_static void sampled_destruct(struct audio_stream *a_src)
{
  struct sampled_stream *s_stream = (struct sampled_stream *)a_src;
  free(s_stream->output_data);
  destruct_audio_stream(a_src);
}

static void vorbis_destruct(struct audio_stream *a_src)
{
  struct vorbis_stream *v_stream = (struct vorbis_stream *)a_src;
  ov_clear(&(v_stream->vorbis_file_handle));
  sampled_destruct(a_src);
}

static void wav_destruct(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;
  free(w_stream->wav_data);
  sampled_destruct(a_src);
}

static void pcs_destruct(struct audio_stream *a_src)
{
  destruct_audio_stream(a_src);
}

__audio_c_maybe_static void construct_audio_stream(
 struct audio_stream *a_src, Uint32 (* mix_data)(
  struct audio_stream *a_src, Sint32 *buffer, Uint32 len),
 void (* set_volume)(struct audio_stream *a_src, Uint32 volume),
 void (* set_repeat)(struct audio_stream *a_src, Uint32 repeat),
 void (* set_order)(struct audio_stream *a_src, Uint32 order),
 void (* set_position)(struct audio_stream *a_src, Uint32 pos),
 Uint32 (* get_order)(struct audio_stream *a_src),
 Uint32 (* get_position)(struct audio_stream *a_src),
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

__audio_c_maybe_static void initialize_sampled_stream(
 struct sampled_stream *s_src,
 void (* set_frequency)(struct sampled_stream *s_src, Uint32 frequency),
 Uint32 (* get_frequency)(struct sampled_stream *s_src),
 Uint32 frequency, Uint32 channels, Uint32 use_volume)
{
  s_src->set_frequency = set_frequency;
  s_src->get_frequency = get_frequency;
  s_src->channels = channels;
  s_src->output_data = NULL;
  s_src->use_volume = use_volume;
  s_src->sample_index = 0;
  set_frequency(s_src, frequency);

  memset(s_src->output_data, 0, s_src->prologue_length);
}

static struct audio_stream *construct_vorbis_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  FILE *input_file = fopen_unsafe(filename, "rb");
  struct audio_stream *ret_val = NULL;
  vorbis_comment *comment;
  int loopstart = -1;
  int looplength = -1;
  int i;

  if(input_file)
  {
    OggVorbis_File open_file;

    if(!ov_open(input_file, &open_file, NULL, 0))
    {
      vorbis_info *vorbis_file_info = ov_info(&open_file, -1);

      // Surround OGGs not supported yet..
      if(vorbis_file_info->channels <= 2)
      {
        struct vorbis_stream *v_stream =
         cmalloc(sizeof(struct vorbis_stream));

        v_stream->vorbis_file_handle = open_file;
        v_stream->vorbis_file_info = vorbis_file_info;

        v_stream->loop_start = 0;
        v_stream->loop_end = 0;

        comment = ov_comment(&open_file, -1);
        if(comment)
        {
          for(i = 0; i < comment->comments; i++)
          {
            if(!strncasecmp("loopstart=", comment->user_comments[i], 10))
              loopstart = atoi(comment->user_comments[i] + 10);
            else

            if(!strncasecmp("looplength=", comment->user_comments[i], 11))
              looplength = atoi(comment->user_comments[i] + 11);
          }

          if((loopstart >= 0) && (looplength > 0))
          {
            v_stream->loop_start = loopstart;
            v_stream->loop_end = loopstart + looplength;
          }
        }

        initialize_sampled_stream((struct sampled_stream *)v_stream,
         vorbis_set_frequency, vorbis_get_frequency, frequency,
         v_stream->vorbis_file_info->channels, 1);

        ret_val = (struct audio_stream *)v_stream;

        construct_audio_stream((struct audio_stream *)v_stream,
         vorbis_mix_data, vorbis_set_volume, vorbis_set_repeat,
         NULL, vorbis_set_position, NULL, vorbis_get_position,
         vorbis_destruct, volume, repeat);
      }
      else
      {
        ov_clear(&open_file);
      }
    }
    else
    {
      fclose(input_file);
    }
  }

  return ret_val;
}

static int read_little_endian32(char *buf)
{
  unsigned char *b = (unsigned char *)buf;
  int i, s = 0;
  for(i = 3; i >= 0; i--)
    s = (s << 8) | b[i];
  return s;
}

static int read_little_endian16(char *buf)
{
  unsigned char *b = (unsigned char *)buf;
  return (b[1] << 8) | b[0];
}

static void *get_riff_chunk(FILE *fp, int filesize, char *id, int *size)
{
  int maxsize = filesize - ftell(fp) - 8;
  int c;
  char size_buf[4];
  void *buf;

  if(maxsize < 0)
    return NULL;

  if(id)
    fread(id, 1, 4, fp);
  else
    fseek(fp, 4, SEEK_CUR);
  fread(size_buf, 1, 4, fp);

  *size = read_little_endian32(size_buf);
  if(*size > maxsize) *size = maxsize;

  buf = cmalloc(*size);

  if(buf)
    fread(buf, 1, *size, fp);

  // Realign if odd size unless padding byte isn't 0
  if(*size & 1)
  {
    c = fgetc(fp);
    if ((c != 0) && (c != EOF))
      fseek(fp, -1, SEEK_CUR);
  }

  return buf;
}

static int get_next_riff_chunk_id(FILE *fp, int filesize, char *id)
{
  if(filesize - ftell(fp) < 8)
    return 0;

  fread(id, 1, 4, fp);
  fseek(fp, -4, SEEK_CUR);
  return 1;
}

static void skip_riff_chunk(FILE *fp, int filesize)
{
  int s, c;
  int maxsize = filesize - ftell(fp) - 8;
  char size_buf[4];

  if(maxsize >= 0)
  {
    fseek(fp, 4, SEEK_CUR);
    fread(size_buf, 1, 4, fp);
    s = read_little_endian32(size_buf);
    if(s > maxsize)
      s = maxsize;
    fseek(fp, s, SEEK_CUR);

    // Realign if odd size unless padding byte isn't 0
    if(s & 1)
    {
      c = fgetc(fp);
      if ((c != 0) && (c != EOF))
        fseek(fp, -1, SEEK_CUR);
    }
  }
}

static void* get_riff_chunk_by_id(FILE *fp, int filesize,
 const char *id, int *size)
{
  int i;
  char id_buf[4];

  fseek(fp, 12, SEEK_SET);

  while((i = get_next_riff_chunk_id(fp, filesize, id_buf)))
  {
    if(memcmp(id_buf, id, 4)) skip_riff_chunk(fp, filesize);
    else break;
  }

  if(!i)
    return NULL;

  return get_riff_chunk(fp, filesize, NULL, size);
}

// More lenient than SDL's WAV loader, but only supports
// uncompressed PCM files (for now.)

static int load_wav_file(const char *file, struct wav_info *spec,
 Uint8 **audio_buf, Uint32 *audio_len)
{
  int data_size, filesize, riffsize, channels, srate, sbytes, fmt_size;
  int smpl_size, numloops;
  Uint32 loop_start, loop_end;
  char *fmt_chunk, *smpl_chunk, tmp_buf[4];
  int ret = 0;
  FILE *fp;
#ifdef CONFIG_SDL
  SDL_AudioSpec sdlspec;
#endif

  fp = fopen_unsafe(file, "rb");
  if(!fp)
    goto exit_out;

  // If it doesn't start with "RIFF", it's not a WAV file.
  fread(tmp_buf, 1, 4, fp);
  if(memcmp(tmp_buf, "RIFF", 4))
    goto exit_close;

  // Read reported file size (if the file turns out to be larger, this will be
  // used instead of the real file size.)
  fread(tmp_buf, 1, 4, fp);
  riffsize = read_little_endian32(tmp_buf) + 8;

  // If the RIFF type isn't "WAVE", it's not a WAV file.
  fread(tmp_buf, 1, 4, fp);
  if(memcmp(tmp_buf, "WAVE", 4))
    goto exit_close;

  // With the RIFF header read, we'll now check the file size.
  filesize = ftell_and_rewind(fp);
  if(filesize > riffsize)
    filesize = riffsize;

  fmt_chunk = get_riff_chunk_by_id(fp, filesize, "fmt ", &fmt_size);

  // If there's no "fmt " chunk, or it's less than 16 bytes, it's not a valid
  // WAV file.
  if(!fmt_chunk || (fmt_size < 16))
    goto exit_close;

  // Default to no loop
  spec->loop_start = 0;
  spec->loop_end = 0;

  // If the WAV file isn't uncompressed PCM (format 1), let SDL handle it.
  if(read_little_endian16(fmt_chunk) != 1)
  {
    free(fmt_chunk);
#ifdef CONFIG_SDL
    if(SDL_LoadWAV(file, &sdlspec, audio_buf, audio_len))
    {
      void *copy_buf = cmalloc(*audio_len);
      memcpy(copy_buf, *audio_buf, *audio_len);
      SDL_FreeWAV(*audio_buf);
      *audio_buf = copy_buf;
      spec->channels = sdlspec.channels;
      spec->freq = sdlspec.freq;
      switch(sdlspec.format)
      {
        case AUDIO_U8:
          spec->format = SAMPLE_U8;
          break;
        case AUDIO_S8:
          spec->format = SAMPLE_S8;
          break;
        case AUDIO_S16LSB:
          spec->format = SAMPLE_S16LSB;
          break;
        default:
         free(copy_buf);
         goto exit_close;
      }

      goto exit_close_success;
    }
#endif // CONFIG_SDL
    goto exit_close;
  }

  // Get the data we need from the "fmt " chunk.
  channels = read_little_endian16(fmt_chunk + 2);
  srate = read_little_endian32(fmt_chunk + 4);

  // Average bytes per second go here (4 bytes)
  // Block align goes here (2 bytes)
  // Round up when dividing by 8
  sbytes = (read_little_endian16(fmt_chunk + 14) + 7) / 8;
  free(fmt_chunk);

  // If I remember correctly, we can't do anything past stereo, and 0 channels
  // isn't valid.  We can't do anything past 16-bit either, and 0-bit isn't
  // valid. 0Hz sample rate is invalid too.
  if(!channels || (channels > 2) || !sbytes || (sbytes > 2) || !srate)
    goto exit_close;

  // Everything seems to check out, so let's load the "data" chunk.
  *audio_buf = get_riff_chunk_by_id(fp, filesize, "data", &data_size);
  *audio_len = data_size;

  // No "data" chunk?! FAIL!
  if(!*audio_buf)
    goto exit_close;

  // Empty "data" chunk?! ALSO FAIL!
  if(!data_size)
  {
    free(*audio_buf);
    goto exit_close;
  }

  spec->freq = srate;
  if(sbytes == 1)
    spec->format = SAMPLE_U8;
  else
    spec->format = SAMPLE_S16LSB;
  spec->channels = channels;

  // Check for "smpl" chunk for looping info
  fseek(fp, 8, SEEK_SET);
  smpl_chunk = get_riff_chunk_by_id(fp, filesize, "smpl", &smpl_size);

  // If there's no "smpl" chunk or it's less than 60 bytes, there's no valid
  // loop data
  if(!smpl_chunk || (smpl_size < 60))
    goto exit_close_success;

  numloops = read_little_endian32(smpl_chunk + 28);
  // First loop is at 36
  loop_start = read_little_endian32(smpl_chunk + 44) * channels * sbytes;
  loop_end = read_little_endian32(smpl_chunk + 48) * channels * sbytes;
  free(smpl_chunk);

  // If the number of loops is less than 1, the loop data's invalid
  if (numloops < 1)
    goto exit_close_success;

  // Boundary check loop points
  if ((loop_start >= *audio_len) || (loop_end > *audio_len)
   || (loop_start >= loop_end))
    goto exit_close_success;

  spec->loop_start = loop_start;
  spec->loop_end = loop_end;

exit_close_success:
  ret = 1;
exit_close:
  fclose(fp);
exit_out:
  return ret;
}

// Props to madbrain for his WAV writing code which the following
// is loosely based off of.

static void write_little_endian32(Uint8 *dest, Uint32 value)
{
  dest[0] = value;
  dest[1] = value >> 8;
  dest[2] = value >> 16;
  dest[3] = value >> 24;
}

static void write_little_endian16(Uint8 *dest, Uint32 value)
{
  dest[0] = value;
  dest[1] = value >> 8;
}

static void write_chars(Uint8 *dest, const char *str)
{
  memcpy(dest, str, strlen(str));
}

static void convert_sam_to_wav(const char *source_name, const char *dest_name)
{
  Uint32 source_length, dest_length;
  FILE *source, *dest;
  Uint32 frequency;
  Uint8 *data;
  Uint32 i;

  source = fopen_unsafe(source_name, "rb");
  if(!source)
    return;

  dest = fopen_unsafe(dest_name, "wb");
  if(!dest)
    goto err_close_source;

  source_length = ftell_and_rewind(source);

  frequency = freq_conversion / default_period;
  dest_length = source_length + 44;
  data = cmalloc(dest_length);

  write_chars(data, "RIFF");
  write_little_endian32(data + 4, dest_length);
  write_chars(data + 8, "WAVEfmt ");
  write_little_endian32(data + 16, 16);
  // PCM
  write_little_endian16(data + 20, 1);
  // Mono
  write_little_endian16(data + 22, 2);
  // Frequency (bytes per second, x2)
  write_little_endian32(data + 24, frequency);
  write_little_endian32(data + 28, frequency);
  // Bytes per sample
  write_little_endian16(data + 32, 1);
  // Bit depth
  write_little_endian16(data + 34, 8);
  write_chars(data + 36, "data");
  // Source length
  write_little_endian32(data + 40, source_length);

  // Read in the actual sample data from the SAM
  fread(data + 44, source_length, 1, source);

  // Convert from signed to unsigned
  for(i = 44; i < dest_length; i++)
    data[i] += 128;

  fwrite(data, dest_length, 1, dest);

  free(data);
  fclose(dest);
err_close_source:
  fclose(source);
}

static void convert_sam_to_wav_translate(const char *src, const char *dest)
{
  char translated_filename_src[MAX_PATH];
  fsafetranslate(src, translated_filename_src);
  convert_sam_to_wav(translated_filename_src, dest);
}

__sam_to_wav_maybe_static int check_ext_for_sam_and_convert(
 const char *filename, char *new_file)
{
  char translated_filename_dest[MAX_PATH];
  ssize_t ext_pos = (ssize_t)strlen(filename) - 4;

  // this could end up being the same, or it might be modified
  // (if a SAM conversion is possible).
  strcpy(new_file, filename);

  if((ext_pos > 0) && !strcasecmp(filename + ext_pos, ".sam"))
  {
    // SAM -> WAV
    memcpy(new_file + ext_pos, ".wav", 4);

    /* If the destination WAV already exists, check its size.
     * If it doesn't exist, or the size is zero, recreate the WAV.
     */
    if(!fsafetranslate(new_file, translated_filename_dest))
    {
      FILE *f = fopen_unsafe(translated_filename_dest, "r");
      if(ftell_and_rewind(f) == 0)
        convert_sam_to_wav_translate(filename, new_file);
      fclose(f);
    }
    else
      convert_sam_to_wav_translate(filename, new_file);

    return 1;
  }

  return 0;
}

#ifdef CONFIG_MODPLUG

int check_ext_for_gdm_and_convert(const char *filename, char *new_file)
{
  char translated_filename_dest[MAX_PATH];
  ssize_t ext_pos = (ssize_t)strlen(filename) - 4;

  // this could end up being the same, or it might be modified
  // (if a GDM conversion is possible).
  strcpy(new_file, filename);

  if((ext_pos > 0) && !strcasecmp(filename + ext_pos, ".gdm"))
  {
    // GDM -> S3M
    memcpy(new_file + ext_pos, ".s3m", 4);

    /* If the destination S3M already exists, check its size. If it's
     * non-zero in size, abort early.
     */
    if(!fsafetranslate(new_file, translated_filename_dest))
    {
      FILE *f = fopen_unsafe(translated_filename_dest, "r");
      long file_len = ftell_and_rewind(f);

      fclose(f);
      if(file_len > 0)
        return 1;
    }

    /* In the case we need to convert the GDM, we need to find
     * the real source path for it. Translate accordingly.
     */
    if(!fsafetranslate(filename, translated_filename_dest))
      convert_gdm_s3m(translated_filename_dest, new_file);
    return 1;
  }

  return 0;
}

#endif // CONFIG_MODPLUG

static struct audio_stream *construct_wav_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  struct wav_info w_info = {0,0,0,0,0};
  struct audio_stream *ret_val = NULL;
  char new_file[MAX_PATH];
  Uint32 data_length = 0;
  Uint8 *wav_data = NULL;

  check_ext_for_sam_and_convert(filename, new_file);

  if(load_wav_file(new_file, &w_info, &wav_data, &data_length))
  {
    // Surround WAVs not supported yet..
    if(w_info.channels <= 2)
    {
      struct wav_stream *w_stream = cmalloc(sizeof(struct wav_stream));

      w_stream->wav_data = wav_data;
      w_stream->data_length = data_length;
      w_stream->channels = w_info.channels;
      w_stream->data_offset = 0;
      w_stream->format = w_info.format;
      w_stream->natural_frequency = w_info.freq;
      w_stream->bytes_per_sample = w_info.channels;
      w_stream->loop_start = w_info.loop_start;
      w_stream->loop_end = w_info.loop_end;

      if((w_info.format != SAMPLE_U8) && (w_info.format != SAMPLE_S8))
        w_stream->bytes_per_sample *= 2;

      initialize_sampled_stream((struct sampled_stream *)w_stream,
       wav_set_frequency, wav_get_frequency, frequency,
       w_info.channels, 1);

      ret_val = (struct audio_stream *)w_stream;

      construct_audio_stream((struct audio_stream *)w_stream,
       wav_mix_data, wav_set_volume, wav_set_repeat,
       NULL, wav_set_position, NULL, wav_get_position, wav_destruct,
       volume, repeat);
    }
  }

  return ret_val;
}

static struct audio_stream *construct_pc_speaker_stream(void)
{
  struct pc_speaker_stream *pcs_stream;

  if(!audio.sfx_on)
    return NULL;

  pcs_stream = ccalloc(1, sizeof(struct pc_speaker_stream));

  construct_audio_stream((struct audio_stream *)pcs_stream, pcs_mix_data,
   pcs_set_volume, NULL, NULL, NULL, NULL, NULL, pcs_destruct,
   audio.sfx_volume * 255 / 8, 0);

  return (struct audio_stream *)pcs_stream;
}

static struct audio_stream *construct_stream_audio_file(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  const char *const *exts = mod_gdm_ext;
  struct audio_stream *a_return = NULL;
  size_t ext_pos, len = strlen(filename);

  if(!audio.music_on)
    return NULL;

  // check string contains a valid extension (no ext isn't valid)
  if(len >= 4 && filename[len - 4] == '.')
    ext_pos = len - 4;
  else if(len >= 3 && filename[len - 3] == '.')
    ext_pos = len - 3;
  else
    return NULL;

  // ogg music is special cased
  if(!strcasecmp(filename + ext_pos, ".ogg"))
  {
    a_return = construct_vorbis_stream(filename, frequency,
      volume, repeat);
  }

  // samples can be played as MODs
  if(!strcasecmp(filename + ext_pos, ".wav") ||
    !strcasecmp(filename + ext_pos, ".sam"))
  {
    a_return = construct_wav_stream(filename, frequency,
      volume, repeat);
  }

  // no need to check module formats; found a loader for SAM/WAV/OGG
  if(a_return)
    return a_return;

  // enforce support only for editor selectable module extensions
  while(*exts)
  {
    if(!strcasecmp(filename + ext_pos, *exts))
      break;
    exts++;
  }

  // wasn't a recognized extension
  if(!*exts)
    return NULL;

#ifdef CONFIG_MODPLUG
  // The native WAV support does work now, however SDL's wav loading
  // code doesn't work very well with a majority of the SAMs converted
  // by the previous versions (probably because they're not really
  // correctly done :x). If you erase those WAVs and let them be made
  // anew they should work OK; otherwise the same old modplug code will
  // be used instead, with modplug's resampling settings. modplug applies
  // some volume filtering as well (some kind of ADSR?) so they won't
  // sound the same. The native code produces a more "pure" and sharp
  // sound.
  a_return = construct_modplug_stream(filename, frequency, volume, repeat);
#endif

#ifdef CONFIG_MIKMOD
  // Mikmod will handle all editor formats except WAV. Therefore loading
  // bad WAV files as MODs may be more likely to fail with MikMod.
  a_return = construct_mikmod_stream(filename, frequency, volume, repeat);
#endif

  return a_return;
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

static void init_pc_speaker(struct config_info *conf)
{
  audio.pcs_stream = (struct pc_speaker_stream *)construct_pc_speaker_stream();
}

void init_audio(struct config_info *conf)
{
  platform_mutex_init(&audio.audio_mutex);

  audio.output_frequency = conf->output_frequency;
  audio.master_resample_mode = conf->resample_mode;

#ifdef CONFIG_MODPLUG
  init_modplug(conf);
#endif

#ifdef CONFIG_MIKMOD
  init_mikmod(conf);
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
  free(audio.pcs_stream);
}

__editor_maybe_static void load_module(char *filename, bool safely,
 int volume)
{
  char translated_filename[MAX_PATH];
  struct audio_stream *a_src;

  // FIXME: Weird hack, why not use end_module() directly?
  if(!filename || !filename[0])
  {
    end_module();
    return;
  }

  // Should never happen, but let's find out
  if(!strcmp(filename, "*"))
  {
    warn("Passed '*' as a file to load! Report this!\n");
    return;
  }

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename) != FSAFE_SUCCESS)
    {
      warn("Module filename '%s' failed safety checks\n", filename);
      return;
    }

    filename = translated_filename;
  }

  end_module();

  a_src = construct_stream_audio_file(filename, 0,
   volume * audio.music_volume / 8, 1);

  LOCK();

  audio.primary_stream = a_src;

  UNLOCK();
}

void load_board_module(struct board *src_board)
{
  load_module(src_board->mod_playing, true, src_board->volume);
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

void play_sample(int freq, char *filename, bool safely)
{
  Uint32 vol = 255 * audio.sound_volume / 8;
  char translated_filename[MAX_PATH];

  if(safely)
  {
    if(fsafetranslate(filename, translated_filename) != FSAFE_SUCCESS)
    {
      warn("Sample filename '%s' failed safety checks\n", filename);
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

// FIXME - Implement? The best route right now would be to load a module
// then mangle it so it only plays the given sample at the given frequency.
// I'm concerned that hacking modplug files could produce strange results,
// including memory leaks. Also, playing the sample in a pattern means that
// it will be cut off eventually, which is especially detrimental if the
// sample is set to loop (as was used in a game once). Implementing this
// completely outside of modplug would be a lot of work since samples can
// be stored in module formats in many different ways, although
// realistically only the mod and s3m file formats need to be supported
// (I don't know how samples are stored in these but there probably aren't
// many possibilities). The safest mechanism of implementation may be to
// load the mod using modplug, copy out the sample data from modplug's
// internal samples (which are easily accessible), destroy the mod, and
// convert the sample into a wav sample internally and play it with or
// without looping depending on the sample settings, and at the specified
// volume and pitch. Unfortunately any other settings (if there are any
// supported by mod/s3m) would probably be lost.
// Overall I think it's not worth the effort, since only a couple of MZX
// games used this.

void spot_sample(int freq, int sample)
{

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

// BIG FAT NOTE:
// This function is only used in sfx() under lock, DO NOT ADD LOCKING!
void sound(int frequency, int duration)
{
  if(audio.pcs_stream)
  {
    audio.pcs_stream->playing = 1;
    audio.pcs_stream->frequency = frequency;
    audio.pcs_stream->note_duration = duration;
  }
}

// BIG FAT NOTE:
// This function is only used in sfx() under lock, DO NOT ADD LOCKING!
void nosound(int duration)
{
  if(audio.pcs_stream)
  {
    audio.pcs_stream->playing = 0;
    audio.pcs_stream->note_duration = duration;
  }
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
     (current_astream != (struct audio_stream *)(audio.pcs_stream)))
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
  audio.pcs_stream->a.set_volume((struct audio_stream *)audio.pcs_stream,
   volume * 255 / 8);

  UNLOCK();
}
