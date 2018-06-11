/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

// Common functions for sampled streams.

#include "audio.h"
#include "sampled_stream.h"

#define FP_SHIFT      13
#define FP_AND        ((1 << FP_SHIFT) - 1)

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

void sampled_set_buffer(struct sampled_stream *s_src)
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

void sampled_mix_data(struct sampled_stream *s_src, Sint32 *dest_buffer,
 Uint32 len)
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

void sampled_destruct(struct audio_stream *a_src)
{
  struct sampled_stream *s_stream = (struct sampled_stream *)a_src;
  free(s_stream->output_data);
  destruct_audio_stream(a_src);
}

void initialize_sampled_stream(struct sampled_stream *s_src,
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
