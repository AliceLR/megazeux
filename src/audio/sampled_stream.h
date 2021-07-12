/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __AUDIO_SAMPLED_STREAM_H
#define __AUDIO_SAMPLED_STREAM_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "audio.h"

struct sampled_stream
{
  struct audio_stream a;
  size_t frequency;
  int16_t *output_data;
  size_t data_window_length;
  size_t allocated_data_length;
  size_t prologue_length;
  size_t epilogue_length;
  size_t stream_offset;
  size_t negative_comp;
  size_t channels;
  boolean use_volume;
  int64_t frequency_delta;
  int64_t sample_index;
  void      (* set_frequency)(struct sampled_stream *s_src, uint32_t frequency);
  uint32_t  (* get_frequency)(struct sampled_stream *s_src);
};

struct sampled_stream_spec
{
  void      (* set_frequency)(struct sampled_stream *s_src, uint32_t frequency);
  uint32_t  (* get_frequency)(struct sampled_stream *s_src);
};

void sampled_set_buffer(struct sampled_stream *s_src);
void sampled_mix_data(struct sampled_stream *s_src,
 int32_t * RESTRICT dest_buffer, size_t dest_frames, unsigned int dest_channels);
void sampled_destruct(struct audio_stream *a_src);

void initialize_sampled_stream(struct sampled_stream *s_src,
 struct sampled_stream_spec *s_spec, uint32_t frequency, uint32_t channels,
 boolean use_volume);

__M_END_DECLS

#endif /* __AUDIO_SAMPLED_STREAM_H */
