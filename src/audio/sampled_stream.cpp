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

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "sampled_stream.h"

#define FP_SHIFT      13
#define FP_AND        ((1 << FP_SHIFT) - 1)

/**
 * This uses C++ templates to generate optimized mixer variants depending on
 * the number of channels, whether or not volume is required, and whether or
 * not resampling is required. This helps avoid redundant code.
 *
 * Likewise, fixed point is used to improve performance. The FP_SHIFT value is
 * the maximum amount an int16_t can be shifted into an int32_t while still
 * leaving enough space for the required math in the linear and cubic mixers.
 * The cubic mixer currently works with values shifted by 2x FP_SHIFT when
 * computing its polynomial.
 *
 * NOTE: FP_SHIFT values of 16 or 32 theoretically might help performance but
 *       some experimentation suggests it's not enough to be worth the effort.
 * TODO: FIR/Sinc-Lanczos resampler!!!!!111one
 */

enum mixer_resample
{
  FLAT        = 0,
  NEAREST     = 1,
  LINEAR      = 2,
  CUBIC       = 3
};

enum mixer_channels
{
  MONO        = 1,
  STEREO      = 2
};

enum mixer_volume
{
  FIXED       = 0,
  DYNAMIC     = 1
};

static void update_sample_index(struct sampled_stream *s_src, int64_t index)
{
  s_src->sample_index = index -
   ((s_src->data_window_length / (s_src->channels * 2)) << FP_SHIFT);
}

template<mixer_volume VOLUME>
static int32_t volume_function(int32_t sample, int volume)
{
  /**
   * NOTE: previous versions of MZX did a /256 here. Just do the bitshift--the
   * rounding difference is more or less irrelevant and it performs better than
   * GCC's optimization for signed constant division.
   */
  if(VOLUME)
    return (sample * volume) >> 8;

  return sample;
}

template<mixer_channels CHANNELS, mixer_volume VOLUME>
static void flat_mix_loop(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t write_len, const int16_t *src, int volume)
{
  for(size_t i = 0; i < write_len; i += 2)
  {
    if(CHANNELS >= STEREO)
    {
      *(dest++) += volume_function<VOLUME>(*(src++), volume);
      *(dest++) += volume_function<VOLUME>(*(src++), volume);
    }
    else
    {
      int32_t smpl = volume_function<VOLUME>(*(src++), volume);
      *(dest++) += smpl;
      *(dest++) += smpl;
    }
  }
  s_src->sample_index = 0;
}

/**
 * C++98 requires that these have external linkage to be used as template
 * parameters. This is pointless but fortunately doesn't hurt C++11 builds.
 */
typedef int32_t (*mix_function)(const int16_t *src_offset, ssize_t frac_index);

template<mixer_channels CHANNELS>
int32_t nearest_mix(const int16_t *src_offset, ssize_t frac_index)
{
  return src_offset[0];
}

template<mixer_channels CHANNELS>
int32_t linear_mix(const int16_t *src_offset, ssize_t frac_index)
{
  int32_t left = src_offset[0];
  int32_t right = src_offset[CHANNELS];
  return left + ((right - left) * frac_index >> FP_SHIFT);
}

template<mixer_channels CHANNELS>
int32_t cubic_mix(const int16_t *src_offset, ssize_t frac_index)
{
  /**
   * NOTE: copied mostly verbatim from the old mixer code, with cleanup.
   * This uses ssize_t instead of int32_t since it seems to be faster for
   * 64-bit machines in this particular resampler.
   *
   * This uses a Hermite spline. This is somewhat faster to compute and
   * generally considered better quality than a Lagrange cubic.
   */
  ssize_t s0 = (ssize_t)src_offset[-CHANNELS]    << FP_SHIFT;
  ssize_t s1 = (ssize_t)src_offset[0]            << FP_SHIFT;
  ssize_t s2 = (ssize_t)src_offset[CHANNELS]     << FP_SHIFT;
  ssize_t s3 = (ssize_t)src_offset[CHANNELS * 2] << FP_SHIFT;

  int64_t a = (((3 * (s1 - s2)) - s0 + s3) / 2);
  int64_t b = ((2 * s2) + s0 - (((5 * s1) + s3) / 2));
  int64_t c = ((s2 - s0) / 2);

  a = ((a * frac_index) >> FP_SHIFT) + b;
  a = ((a * frac_index) >> FP_SHIFT) + c;
  a = ((a * frac_index) >> FP_SHIFT) + s1;
  return (a >> FP_SHIFT);
}

template<mixer_channels CHANNELS, mixer_volume VOLUME, mix_function MIX>
static void resample_mix_loop(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t write_len, const int16_t *src, int volume)
{
  int64_t sample_index = s_src->sample_index;
  int64_t delta = s_src->frequency_delta;

  for(size_t i = 0; i < write_len; i += 2, sample_index += delta)
  {
    ssize_t int_index = (sample_index >> FP_SHIFT) * CHANNELS;
    ssize_t frac_index = sample_index & FP_AND;

    if(CHANNELS >= STEREO)
    {
      int32_t mix_a = MIX(src + int_index + 0, frac_index);
      int32_t mix_b = MIX(src + int_index + 1, frac_index);
      *(dest++) += volume_function<VOLUME>(mix_a, volume);
      *(dest++) += volume_function<VOLUME>(mix_b, volume);
    }
    else
    {
      int32_t mix = MIX(src + int_index, frac_index);
      int32_t smpl = volume_function<VOLUME>(mix, volume);
      *(dest++) += smpl;
      *(dest++) += smpl;
    }
  }
  update_sample_index(s_src, sample_index);
}

template<mixer_channels CHANNELS, mixer_volume VOLUME>
static void mixer_function(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t write_len, const int16_t *src, int volume,
 int resample_mode)
{
  switch((mixer_resample)resample_mode)
  {
    case FLAT:
      flat_mix_loop<CHANNELS, VOLUME>(s_src, dest, write_len, src, volume);
      break;

    case NEAREST:
      resample_mix_loop<CHANNELS, VOLUME, nearest_mix<CHANNELS> >(s_src,
       dest, write_len, src, volume);
      break;

    case LINEAR:
      resample_mix_loop<CHANNELS, VOLUME, linear_mix<CHANNELS> >(s_src,
       dest, write_len, src, volume);
      break;

    case CUBIC:
      resample_mix_loop<CHANNELS, VOLUME, cubic_mix<CHANNELS> >(s_src,
       dest, write_len, src, volume);
      break;
  }
}

template<mixer_channels CHANNELS>
static void mixer_function(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t write_len, const int16_t *src, int volume,
 int resample_mode, mixer_volume volume_mode)
{
  switch(volume_mode)
  {
    case FIXED:
      mixer_function<CHANNELS, FIXED>(s_src, dest, write_len, src, volume, resample_mode);
      break;

    case DYNAMIC:
      mixer_function<CHANNELS, DYNAMIC>(s_src, dest, write_len, src, volume, resample_mode);
      break;
  }
}

static void mixer_function(struct sampled_stream *s_src,
 int32_t * RESTRICT dest, size_t write_len, const int16_t *src, int volume,
 int resample_mode, mixer_channels channel_mode, mixer_volume volume_mode)
{
  switch(channel_mode)
  {
    case MONO:
      mixer_function<MONO>(s_src, dest, write_len, src, volume, resample_mode, volume_mode);
      break;

    case STEREO:
      mixer_function<STEREO>(s_src, dest, write_len, src, volume, resample_mode, volume_mode);
      break;
  }
}

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
   (int16_t *)crealloc(s_src->output_data, allocated_data_length);

  sampled_negative_threshold(s_src);
}

void sampled_mix_data(struct sampled_stream *s_src, Sint32 *dest_buffer,
 Uint32 len)
{
  uint8_t *output_data = (uint8_t *)s_src->output_data;
  int16_t *src_buffer = (int16_t *)(output_data + s_src->prologue_length);
  size_t write_len = len / 2;
  int volume = ((struct audio_stream *)s_src)->volume;
  int resample_mode = audio.master_resample_mode + 1;
  enum mixer_volume   use_volume   = DYNAMIC;
  enum mixer_channels use_channels = STEREO;

  if(s_src->frequency == audio.output_frequency)
    resample_mode = FLAT;

  if(!s_src->use_volume || volume == 256)
    use_volume = FIXED;

  if(s_src->channels < 2)
    use_channels = MONO;

  mixer_function(s_src, dest_buffer, write_len, src_buffer, volume,
   resample_mode, use_channels, use_volume);

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
 struct sampled_stream_spec *s_spec, Uint32 frequency, Uint32 channels,
 Uint32 use_volume)
{
  s_src->set_frequency = s_spec->set_frequency;
  s_src->get_frequency = s_spec->get_frequency;
  s_src->channels = channels;
  s_src->output_data = NULL;
  s_src->use_volume = use_volume;
  s_src->sample_index = 0;

  s_src->set_frequency(s_src, frequency);

  memset(s_src->output_data, 0, s_src->prologue_length);
}
