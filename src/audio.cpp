/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// Code to handle module playing, sample playing, and PC speaker
// sfx emulation.

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include "audio.h"
#include "modplug.h"
#include "stdafx.h"
#include "sndfile.h"
#include "SDL.h"
#include "sfx.h"
#include "data.h"
#include "configure.h"
#include "gdm2s3m.h"
#include "fsafeopen.h"
#include "delay.h"

// The native WAV support does work now, however SDL's wav loading
// code doesn't work very well with a majority of the SAMs converted
// by the previous versions (probably because they're not really
// correctly done :x). If you erase those WAVs and let them be made
// anew they should work OK; otherwise the same old modplug code will
// be used instead, with modplug's resampling settings. modplug applies
// some volume filtering as well (some kind of ADSR?) so they won't
// sound the same. The native code produces a more "pure" and sharp
// sound.

struct _ModPlugFile
{
  CSoundFile mSoundFile;
};

audio_struct audio;

const int freq_conversion = 3579364;

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
  int_index = (s_index >> FP_SHIFT) * channels;                         \
  frac_index = s_index & FP_AND;                                        \

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
  dest right_sample + ((frac_index *                                    \
   (src_buffer[int_index + channels + offset] - right_sample)) >>       \
   FP_SHIFT)                                                            \

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

void sampled_negative_threshold(sampled_stream *s_src)
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

void sampled_set_buffer(sampled_stream *s_src)
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
   (Uint32)(ceil((double)audio.audio_settings.samples *
   frequency / audio.output_frequency) * bytes_per_sample);

  prologue_length += (Uint32)(ceil(frequency_delta) * bytes_per_sample);

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
   (Sint16 *)realloc(s_src->output_data, allocated_data_length);

  sampled_negative_threshold(s_src);
}

void sampled_mix_data(sampled_stream *s_src, Sint32 *dest_buffer,
 Uint32 len)
{
  Uint8 *output_data = (Uint8 *)s_src->output_data;
  Sint16 *src_buffer =
   (Sint16 *)(output_data + s_src->prologue_length);
  Uint32 write_len = len / 2;
  Sint32 volume = ((audio_stream *)s_src)->volume;
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

  memcpy(output_data, output_data +
   s_src->allocated_data_length - s_src->stream_offset,
   s_src->stream_offset);
}

Uint32 mp_mix_data(audio_stream *a_src, Sint32 *buffer, Uint32 len)
{
  Uint32 read_len;
  modplug_stream *mp_stream = (modplug_stream *)a_src;
  Uint32 read_wanted = mp_stream->s.allocated_data_length -
   mp_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)mp_stream->s.output_data +
   mp_stream->s.stream_offset;

  read_len =
   ModPlug_Read(mp_stream->module_data, read_buffer, read_wanted);

  if(read_len < read_wanted)
  {
    // Modplug still fails to repeat on some mods, even after already
    // restarting it still ends the thing (even fades it). This won't
    // eliminate the fade but at least it'll stop the mod from
    // ending..

    if(a_src->repeat)
      a_src->set_position(a_src, a_src->get_position(a_src));
    else
      memset(read_buffer + read_len, 0, read_wanted - read_len);

    read_len = 0;
  }

  sampled_mix_data((sampled_stream *)mp_stream, buffer, len);

/*
  if(read_len == 0)
    return 1;
*/

  return 0;
}

void mp_set_volume(audio_stream *a_src, Uint32 volume)
{
  ModPlugFile *mp_file = ((modplug_stream *)a_src)->module_data;

  a_src->volume = volume;
  mp_file->mSoundFile.SetMasterVolume(volume);
}

void mp_set_repeat(audio_stream *a_src, Uint32 repeat)
{
  ModPlugFile *mp_file = ((modplug_stream *)a_src)->module_data;

  a_src->repeat = repeat;

  if(repeat)
    mp_file->mSoundFile.SetRepeatCount(-1);
  else
    mp_file->mSoundFile.SetRepeatCount(0);
}

void mp_set_order(audio_stream *a_src, Uint32 order)
{
  ((modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentOrder(order);
}

void mp_set_position(audio_stream *a_src, Uint32 position)
{
  ((modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentPos(position);
}

void mp_set_frequency(sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
  {
    ((modplug_stream *)s_src)->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    ((modplug_stream *)s_src)->effective_frequency = frequency;
    frequency = (Uint32)((double)frequency *
     audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

Uint32 mp_get_order(audio_stream *a_src)
{
  return ((modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentOrder();
}

Uint32 mp_get_position(audio_stream *a_src)
{
  return ((modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentPos();
}

Uint32 mp_get_frequency(sampled_stream *s_src)
{
  return ((modplug_stream *)s_src)->effective_frequency;
}

Uint32 vorbis_mix_data(audio_stream *a_src, Sint32 *buffer, Uint32 len)
{
  Uint32 read_len = 0;
  vorbis_stream *v_stream = (vorbis_stream *)a_src;
  Uint32 read_wanted = v_stream->s.allocated_data_length -
   v_stream->s.stream_offset;
  char *read_buffer = (char *)v_stream->s.output_data +
   v_stream->s.stream_offset;
  int current_section;

  do
  {
    read_wanted -= read_len;
    read_len =
     ov_read(&(v_stream->vorbis_file_handle), read_buffer,
     read_wanted, 0, 2, 1, &current_section);

    // If it hit the end go back to the beginning if repeat is on

    if(read_len == 0)
    {
      if(a_src->repeat)
      {
        ov_raw_seek(&(v_stream->vorbis_file_handle), 0);

        if(read_wanted)
        {
          read_len =
           ov_read(&(v_stream->vorbis_file_handle), read_buffer,
           read_wanted, 0, 2, 1, &current_section);
        }
      }
      else
      {
        memset(read_buffer, 0, read_wanted);
      }
    }

    read_buffer += read_len;
  } while((read_len < read_wanted) && read_len);

  sampled_mix_data((sampled_stream *)v_stream, buffer, len);

  if(read_len == 0)
    return 1;

  return 0;
}

void vorbis_set_volume(audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume * 256 / 255;
}

void vorbis_set_repeat(audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

void vorbis_set_position(audio_stream *a_src, Uint32 position)
{
  ov_pcm_seek(&(((vorbis_stream *)a_src)->vorbis_file_handle),
   (ogg_int64_t)position);
}

void vorbis_set_frequency(sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
    frequency = ((vorbis_stream *)s_src)->vorbis_file_info->rate;

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

Uint32 vorbis_get_position(audio_stream *a_src)
{
  return ov_pcm_tell(&(((vorbis_stream *)a_src)->vorbis_file_handle));
}

Uint32 vorbis_get_frequency(sampled_stream *s_src)
{
  return s_src->frequency;
}

Uint32 wav_read_data(wav_stream *w_stream, Uint8 *buffer, Uint32 len)
{
  Uint32 data_read;
  Uint32 read_len = len;
  Uint32 i;

  switch(w_stream->format)
  {
    case AUDIO_U8:
    {
      Uint8 *src =
       (Uint8 *)w_stream->wav_data + w_stream->data_offset;
      Sint16 *dest = (Sint16 *)buffer;

      read_len /= 2;

      if(w_stream->data_offset + read_len > w_stream->data_length)
        read_len = w_stream->data_length - w_stream->data_offset;

      data_read = read_len * 2;

      for(i = 0; i < read_len; i++)
      {
        dest[i] = (Sint8)(src[i] - 128) << 8;
      }

      break;
    }

    default:
    {
      if(w_stream->data_offset + read_len > w_stream->data_length)
        read_len = w_stream->data_length - w_stream->data_offset;

      memcpy(buffer, w_stream->wav_data + w_stream->data_offset,
       read_len);

      data_read = read_len;

      break;
    }
  }

  w_stream->data_offset += read_len;

  return data_read;
}

Uint32 wav_mix_data(audio_stream *a_src, Sint32 *buffer, Uint32 len)
{
  Uint32 read_len = 0;
  wav_stream *w_stream = (wav_stream *)a_src;
  Uint32 read_wanted = w_stream->s.allocated_data_length -
   w_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)w_stream->s.output_data +
   w_stream->s.stream_offset;

  read_len = wav_read_data(w_stream, read_buffer, read_wanted);

  if(read_len < read_wanted)
  {
    read_buffer += read_len;
    read_wanted -= read_len;

    if(a_src->repeat)
    {
      w_stream->data_offset = 0;
      wav_read_data(w_stream, read_buffer, read_wanted);
    }
    else
    {
      memset(read_buffer, 0, read_wanted);
      read_len = 0;
    }
  }

  sampled_mix_data((sampled_stream *)w_stream, buffer, len);

  if(read_len == 0)
    return 1;

  return 0;
}

void wav_set_volume(audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume * 256 / 255;
}

void wav_set_repeat(audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

void wav_set_position(audio_stream *a_src, Uint32 position)
{
  wav_stream *w_stream = (wav_stream *)a_src;

  if(position < (w_stream->data_length / w_stream->bytes_per_sample))
    w_stream->data_offset = position / w_stream->bytes_per_sample;
}

void wav_set_frequency(sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
    frequency = ((wav_stream *)s_src)->natural_frequency;

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

Uint32 wav_get_position(audio_stream *a_src)
{
  wav_stream *w_stream = (wav_stream *)a_src;

  return w_stream->data_offset / w_stream->bytes_per_sample;
}

Uint32 wav_get_frequency(sampled_stream *s_src)
{
  return s_src->frequency;
}

Uint32 pcs_mix_data(audio_stream *a_src, Sint32 *buffer, Uint32 len)
{
  pc_speaker_stream *pcs_stream = (pc_speaker_stream *)a_src;
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
     (Uint32)((double)pcs_stream->last_frequency /
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
    sample_duration = (Uint32)((double)audio.output_frequency / 500 *
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
       (Uint32)((double)pcs_stream->frequency /
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

void pcs_set_volume(audio_stream *a_src, Uint32 volume)
{
  ((pc_speaker_stream *)a_src)->volume = volume;
}

void destruct_audio_stream(audio_stream *a_src)
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

void sampled_destruct(audio_stream *a_src)
{
  sampled_stream *s_stream = (sampled_stream *)a_src;
  free(s_stream->output_data);
  destruct_audio_stream(a_src);
}

void mp_destruct(audio_stream *a_src)
{
  modplug_stream *mp_stream = (modplug_stream *)a_src;
  ModPlug_Unload(mp_stream->module_data);
  sampled_destruct(a_src);
}

void vorbis_destruct(audio_stream *a_src)
{
  vorbis_stream *v_stream = (vorbis_stream *)a_src;
  ov_clear(&(v_stream->vorbis_file_handle));
  sampled_destruct(a_src);
}

void wav_destruct(audio_stream *a_src)
{
  wav_stream *w_stream = (wav_stream *)a_src;
  SDL_FreeWAV(w_stream->wav_data);
  sampled_destruct(a_src);
}

void pcs_destruct(audio_stream *a_src)
{
  destruct_audio_stream(a_src);
}

void construct_audio_stream(audio_stream *a_src,
 Uint32 (* mix_data)(audio_stream *a_src, Sint32 *buffer,
  Uint32 len),
 void (* set_volume)(audio_stream *a_src, Uint32 volume),
 void (* set_repeat)(audio_stream *a_src, Uint32 repeat),
 void (* set_order)(audio_stream *a_src, Uint32 order),
 void (* set_position)(audio_stream *a_src, Uint32 pos),
 Uint32 (* get_order)(audio_stream *a_src),
 Uint32 (* get_position)(audio_stream *a_src),
 void (* destruct)(audio_stream *a_src),
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
}

void initialize_sampled_stream(sampled_stream *s_src,
 void (* set_frequency)(sampled_stream *s_src, Uint32 frequency),
 Uint32 (* get_frequency)(sampled_stream *s_src),
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

audio_stream *construct_modplug_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat)
{
  FILE *input_file;
  char *input_buffer;
  Uint32 file_size;
  Sint32 ext_pos = strlen(filename) - 4;
  char new_file[MAX_PATH];
  audio_stream *ret_val = NULL;

  if((ext_pos > 0) && !strcasecmp(filename + ext_pos, ".gdm"))
  {
    char translated_filename_src[MAX_PATH];
    char translated_filename_dest[MAX_PATH];

    // GDM -> S3M
    strcpy(new_file, filename);
    memcpy(new_file + ext_pos, ".s3m", 4);

    if(fsafetranslate(new_file, translated_filename_dest) < 0)
    {
      // If it doesn't exist, create it by converting.
      fsafetranslate(filename, translated_filename_src);
      convert_gdm_s3m(translated_filename_src, new_file);
    }

    filename = new_file;
  }

  if((ext_pos > 0) && !strcasecmp(filename + ext_pos, ".sam"))
  {
    char translated_filename_src[MAX_PATH];
    char translated_filename_dest[MAX_PATH];

    // SAM -> WAV
    strcpy(new_file, filename);
    memcpy(new_file + ext_pos, ".wav", 4);

    if(fsafetranslate(new_file, translated_filename_dest) < 0)
    {
      // If it doesn't exist, create it by converting.
      fsafetranslate(filename, translated_filename_src);
      convert_sam_to_wav(translated_filename_src, new_file);
    }

    filename = new_file;
  }

  input_file = fsafeopen(filename, "rb");

  if(input_file)
  {
    ModPlugFile *open_file;

    file_size = filelength(input_file);

    input_buffer = (char *)malloc(file_size);
    fread(input_buffer, file_size, 1, input_file);
    open_file = ModPlug_Load(input_buffer, file_size);

    if(open_file)
    {
      modplug_stream *mp_stream =
       (modplug_stream *)malloc(sizeof(modplug_stream));

      mp_stream->module_data = open_file;

      if((ext_pos > 0) &&
       !strcasecmp(filename + ext_pos, ".wav"))
      {
        open_file->mSoundFile.Ins[1].nC4Speed = frequency;
        open_file->mSoundFile.Ins[2].nC4Speed = frequency;
        frequency = 0;
      }

      initialize_sampled_stream((sampled_stream *)mp_stream,
       mp_set_frequency, mp_get_frequency, frequency, 2, 0);

      ret_val = (audio_stream *)mp_stream;

      construct_audio_stream((audio_stream *)mp_stream, mp_mix_data,
       mp_set_volume, mp_set_repeat, mp_set_order, mp_set_position,
       mp_get_order, mp_get_position, mp_destruct, volume, repeat);
    }

    free(input_buffer);
    fclose(input_file);
  }

  return ret_val;
}

audio_stream *construct_vorbis_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat)
{
  FILE *input_file = fsafeopen(filename, "rb");
  audio_stream *ret_val = NULL;

  if(input_file)
  {
    OggVorbis_File open_file;

    if(!ov_open(input_file, &open_file, NULL, 0))
    {
      vorbis_info *vorbis_file_info = ov_info(&open_file, -1);

      // Surround OGGs not supported yet..
      if(vorbis_file_info->channels <= 2)
      {
        vorbis_stream *v_stream =
         (vorbis_stream *)malloc(sizeof(vorbis_stream));

        v_stream->vorbis_file_handle = open_file;
        v_stream->vorbis_file_info = vorbis_file_info;

        initialize_sampled_stream((sampled_stream *)v_stream,
         vorbis_set_frequency, vorbis_get_frequency, frequency,
         v_stream->vorbis_file_info->channels, 1);

        ret_val = (audio_stream *)v_stream;

        construct_audio_stream((audio_stream *)v_stream,
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

audio_stream *construct_wav_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat)
{
  Uint32 data_length;
  Uint8 *wav_data;
  SDL_AudioSpec wav_info;
  char safe_filename[512];
  char new_file[MAX_PATH];
  Sint32 ext_pos = strlen(filename) - 4;

  audio_stream *ret_val = NULL;

  if((ext_pos > 0) && !strcasecmp(filename + ext_pos, ".sam"))
  {
    char translated_filename_src[MAX_PATH];
    char translated_filename_dest[MAX_PATH];

    // SAM -> WAV
    strcpy(new_file, filename);
    memcpy(new_file + ext_pos, ".wav", 4);

    if(fsafetranslate(new_file, translated_filename_dest) < 0)
    {
      // If it doesn't exist, create it by converting.
      fsafetranslate(filename, translated_filename_src);
      convert_sam_to_wav(translated_filename_src, new_file);
    }

    filename = new_file;
  }

  fsafetranslate(filename, safe_filename);

  if(SDL_LoadWAV(safe_filename, &wav_info, &wav_data, &data_length))
  {
    // Surround WAVs not supported yet..
    if(wav_info.channels <= 2)
    {
      wav_stream *w_stream =
       (wav_stream *)malloc(sizeof(wav_stream));

      w_stream->wav_data = wav_data;
      w_stream->data_length = data_length;
      w_stream->channels = wav_info.channels;
      w_stream->data_offset = 0;
      w_stream->format = wav_info.format;
      w_stream->natural_frequency = wav_info.freq;
      w_stream->bytes_per_sample = wav_info.channels;

      if((wav_info.format != AUDIO_U8) &&
       (wav_info.format != AUDIO_S8))
      {
        w_stream->bytes_per_sample *= 2;
      }

      initialize_sampled_stream((sampled_stream *)w_stream,
       wav_set_frequency, wav_get_frequency, frequency,
       wav_info.channels, 1);

      ret_val = (audio_stream *)w_stream;

      construct_audio_stream((audio_stream *)w_stream,
       wav_mix_data, wav_set_volume, wav_set_repeat,
       NULL, wav_set_position, NULL, wav_get_position, wav_destruct,
       volume, repeat);
    }
  }

  return ret_val;
}

audio_stream *construct_pc_speaker_stream()
{
  pc_speaker_stream *pcs_stream =
   (pc_speaker_stream *)malloc(sizeof(pc_speaker_stream));

  memset(pcs_stream, 0, sizeof(pc_speaker_stream));

  construct_audio_stream((audio_stream *)pcs_stream, pcs_mix_data,
   pcs_set_volume, NULL, NULL, NULL, NULL, NULL, pcs_destruct,
   audio.sfx_volume * 255 / 8, 0);

  return (audio_stream *)pcs_stream;
}

audio_stream *construct_stream_audio_file(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  Sint32 ext_pos = strlen(filename) - 4;
  audio_stream *a_return = NULL;

  if(audio.music_on)
  {
    if(ext_pos > 0)
    {
      if(!strcmp(filename + ext_pos, ".ogg"))
      {
        a_return = construct_vorbis_stream(filename, frequency,
         volume, repeat);
      }

      if(!strcmp(filename + ext_pos, ".wav") ||
       !strcmp(filename + ext_pos, ".sam"))
      {
        a_return = construct_wav_stream(filename, frequency,
         volume, repeat);
      }
    }

    // modplug is used as a universal fallback. This means that
    // waves not loaded by SDL will be loaded by modplug instead.

    if(a_return == NULL)
    {
      a_return = construct_modplug_stream(filename, frequency,
       volume, repeat);
    }
  }

  return a_return;
}

void clip_buffer(Sint16 *dest, Sint32 *src, int len)
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

void audio_callback(void *userdata, Uint8 *stream, int len)
{
  Uint32 destroy_flag;
  audio_stream *current_astream;

  SDL_LockMutex(audio.audio_mutex);

  current_astream = audio.stream_list_base;

  if(current_astream)
  {
    memset(audio.mix_buffer, 0, len * 2);

    while(current_astream != NULL)
    {
      destroy_flag = current_astream->mix_data(current_astream,
       audio.mix_buffer, len);

      if(destroy_flag)
      {
        audio_stream *next_astream = current_astream->next;
        current_astream->destruct(current_astream);
        current_astream = next_astream;
      }
      else
      {
        current_astream = current_astream->next;
      }
    }

    clip_buffer((Sint16 *)stream, audio.mix_buffer, len / 2);
  }

  SDL_UnlockMutex(audio.audio_mutex);
}

void init_modplug(config_info *conf)
{
  if(conf->oversampling_on)
    audio.mod_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;

  audio.mod_settings.mFrequency = audio.output_frequency;
  audio.mod_settings.mChannels = 2;
  audio.mod_settings.mBits = 16;

  switch(conf->modplug_resample_mode)
  {
    case 0:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_NEAREST;
      break;
    }

    case 1:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
      break;
    }

    case 2:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
      break;
    }

    case 3:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
      break;
    }
  }

  audio.mod_settings.mLoopCount = -1;

  ModPlug_SetSettings(&audio.mod_settings);
}

void init_pc_speaker(config_info *conf)
{
  audio.pcs_stream = (pc_speaker_stream *)construct_pc_speaker_stream();
}

void init_audio(config_info *conf)
{
  SDL_AudioSpec desired_spec =
  {
    0,
    AUDIO_S16SYS,
    2,
    0,
    conf->buffer_size,
    0,
    0,
    audio_callback,
    NULL
  };

  audio.output_frequency = conf->output_frequency;
  audio.master_resample_mode = conf->resample_mode;
  desired_spec.freq = audio.output_frequency;

  init_modplug(conf);
  init_pc_speaker(conf);

  SDL_OpenAudio(&desired_spec, &audio.audio_settings);
  audio.mix_buffer = (Sint32 *)malloc(audio.audio_settings.size * 2);
  audio.audio_mutex = SDL_CreateMutex();

  SDL_PauseAudio(0);
}

void load_mod(char *filename)
{
  end_mod();

  if(filename && filename[0])
  {
    SDL_LockMutex(audio.audio_mutex);

    audio.primary_stream = construct_stream_audio_file(filename, 0,
     audio.music_volume * 255 / 8, 1);

    SDL_UnlockMutex(audio.audio_mutex);
  }
}

void end_mod(void)
{
  if(audio.primary_stream)
  {
    SDL_LockMutex(audio.audio_mutex);

    audio.primary_stream->destruct(audio.primary_stream);

    SDL_UnlockMutex(audio.audio_mutex);
  }

  audio.primary_stream = NULL;
}

void play_sample(int freq, char *filename)
{
  audio_stream *a_src;
  Uint32 vol = 255 * audio.sound_volume / 8;

  SDL_LockMutex(audio.audio_mutex);

  if(freq == 0)
  {
    a_src = construct_stream_audio_file(filename, 0, vol, 0);
  }
  else
  {
    a_src = construct_stream_audio_file(filename,
     (freq_conversion / freq) / 2, vol, 0);
  }

  SDL_UnlockMutex(audio.audio_mutex);
}

void end_sample()
{
  // Destroy all samples - something is a sample if it's not a
  // primary or PC speaker stream. This is a bit of a dirty way
  // to do it though (might want to keep multiple lists instead)

  audio_stream *current_astream = audio.stream_list_base;
  audio_stream *next_astream;

  SDL_LockMutex(audio.audio_mutex);     

  while(current_astream)
  {
    next_astream = current_astream->next;

    if((current_astream != audio.primary_stream) &&
     (current_astream != (audio_stream *)(audio.pcs_stream)))
    {
      current_astream->destruct(current_astream);
    }

    current_astream = next_astream;
  }


  SDL_UnlockMutex(audio.audio_mutex);     
}

void jump_mod(int order)
{
  // This is really just for modules, I can't imagine what an "order"
  // might be for non-sequenced formats. It's also mostly here for
  // legacy reasons; set_position rather supercedes it.

  if(audio.primary_stream && audio.primary_stream->set_order)
  {
    SDL_LockMutex(audio.audio_mutex);     
    audio.primary_stream->set_order(audio.primary_stream, order);   
    SDL_UnlockMutex(audio.audio_mutex);     
  }
}

int get_order()
{
  if(audio.primary_stream && audio.primary_stream->get_order)
  {
    int order;

    SDL_LockMutex(audio.audio_mutex);     
    order = audio.primary_stream->get_order(audio.primary_stream);  
    SDL_UnlockMutex(audio.audio_mutex);     

    return order;
  }
  else
  {
    return 0;
  }
}

void volume_mod(int vol)
{
  if(audio.primary_stream)
  {
    SDL_LockMutex(audio.audio_mutex);     

    audio.primary_stream->set_volume(audio.primary_stream,
     vol * audio.music_volume / 8);

    SDL_UnlockMutex(audio.audio_mutex);     
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
    SDL_LockMutex(audio.audio_mutex);     

    ((sampled_stream *)audio.primary_stream)->
     set_frequency((sampled_stream *)audio.primary_stream,
     freq);

    SDL_UnlockMutex(audio.audio_mutex);     
  }
}

int get_frequency()
{
  if(audio.primary_stream)
  {
    int freq;

    SDL_LockMutex(audio.audio_mutex);     
    freq = ((sampled_stream *)audio.primary_stream)->
     get_frequency((sampled_stream *)audio.primary_stream);
    SDL_UnlockMutex(audio.audio_mutex);     

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
    SDL_LockMutex(audio.audio_mutex);     
    audio.primary_stream->set_position(audio.primary_stream, pos);
    SDL_UnlockMutex(audio.audio_mutex);     
  }
}

int get_position()
{
  if(audio.primary_stream && audio.primary_stream->get_position)
  {
    int pos;

    SDL_LockMutex(audio.audio_mutex);     
    pos = audio.primary_stream->get_position(audio.primary_stream);
    SDL_UnlockMutex(audio.audio_mutex);

    return pos;
  }
  else
  {
    return 0;
  }
}

void sound(int frequency, int duration)
{
  SDL_LockMutex(audio.audio_mutex);     
  audio.pcs_stream->playing = 1;
  audio.pcs_stream->frequency = frequency;
  audio.pcs_stream->note_duration = duration;
  SDL_UnlockMutex(audio.audio_mutex);     
}

void nosound(int duration)
{
  SDL_LockMutex(audio.audio_mutex);     
  audio.pcs_stream->playing = 0;
  audio.pcs_stream->note_duration = duration; 
  SDL_UnlockMutex(audio.audio_mutex);     
}

int filelength(FILE *fp)
{
  int length;

  fseek(fp, 0, SEEK_END);
  length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  return length;
}

const int default_period = 428;

// Props to madbrain for his WAV writing code which the following
// is loosely based off of.

void write_little_endian32(Uint8 *dest, Uint32 value)
{
  dest[0] = value;
  dest[1] = value >> 8;
  dest[2] = value >> 16;
  dest[3] = value >> 24;
}

void write_little_endian16(Uint8 *dest, Uint32 value)
{
  dest[0] = value;
  dest[1] = value >> 8;
}

void write_chars(Uint8 *dest, char *str)
{
  memcpy(dest, str, strlen(str));
}

void convert_sam_to_wav(char *source_name, char *dest_name)
{
  FILE *source, *dest;
  Uint32 frequency;
  Uint32 source_length, dest_length;

  Uint8 *data;
  Uint32 i;

  source = fopen(source_name, "rb");

  if(source == NULL)
    return;

  dest = fopen(dest_name, "wb");

  source_length = filelength(source);

  frequency = freq_conversion / default_period;
  dest_length = source_length + 44;
  data = (Uint8 *)malloc(dest_length);

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
  {
    data[i] += 128;
  }

  fwrite(data, dest_length, 1, dest);

  free(data);
  fclose(source);
  fclose(dest);
}

void set_music_on(int val)
{
  SDL_LockMutex(audio.audio_mutex);     
  audio.music_on = val;
  SDL_UnlockMutex(audio.audio_mutex);     
}

void set_sfx_on(int val)
{
  SDL_LockMutex(audio.audio_mutex); 
  audio.sfx_on = val;
  SDL_UnlockMutex(audio.audio_mutex); 
}

// These don't have to be locked because only the same thread can
// modify them.

int get_music_on_state()
{
  return audio.music_on;
}

int get_sfx_on_state()
{
  return audio.sfx_on;
}

int get_music_volume()
{
  return audio.music_volume;
}

int get_sound_volume()
{
  return audio.sound_volume;
}

int get_sfx_volume()
{
  return audio.sfx_volume;
}

void set_music_volume(int volume)
{
  SDL_LockMutex(audio.audio_mutex);   
  audio.music_volume = volume;
  SDL_UnlockMutex(audio.audio_mutex);   
}

void set_sound_volume(int volume)
{
  audio_stream *current_astream = audio.stream_list_base;

  SDL_LockMutex(audio.audio_mutex);

  audio.sound_volume = volume;

  while(current_astream)
  {
    if((current_astream != audio.primary_stream) &&
     (current_astream != (audio_stream *)(audio.pcs_stream)))
    {
      current_astream->set_volume(current_astream,
       audio.sound_volume * 255 / 8);
    }

    current_astream = current_astream->next;
  }

  SDL_UnlockMutex(audio.audio_mutex);
}

void set_sfx_volume(int volume)
{
  SDL_LockMutex(audio.audio_mutex);
  audio.sfx_volume = volume;
  audio.pcs_stream->a.set_volume((audio_stream *)audio.pcs_stream,
   volume * 255 / 8);
  SDL_UnlockMutex(audio.audio_mutex); 
}


