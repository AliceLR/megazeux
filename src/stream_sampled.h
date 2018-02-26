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

#ifndef __AUDIO_STREAM_SAMPLED_H
#define __AUDIO_STREAM_SAMPLED_H

#include "compat.h"

__M_BEGIN_DECLS

#include "audio.h"

struct sampled_stream
{
  struct audio_stream a;
  Uint32 frequency;
  Sint16 *output_data;
  Uint32 data_window_length;
  Uint32 allocated_data_length;
  Uint32 prologue_length;
  Uint32 epilogue_length;
  Uint32 stream_offset;
  Uint32 channels;
  Uint32 negative_comp;
  Uint32 use_volume;
  Sint64 frequency_delta;
  Sint64 sample_index;
  void (* set_frequency)(struct sampled_stream *s_src, Uint32 frequency);
  Uint32 (* get_frequency)(struct sampled_stream *s_src);
};

/*** these should only be exported for audio plugins */

#if defined(CONFIG_AUDIO_MOD_SYSTEM)

void sampled_set_buffer(struct sampled_stream *s_src);
void sampled_mix_data(struct sampled_stream *s_src, Sint32 *dest_buffer,
 Uint32 len);
void sampled_destruct(struct audio_stream *a_src);

void initialize_sampled_stream(struct sampled_stream *s_src,
 void (* set_frequency)(struct sampled_stream *s_src, Uint32 frequency),
 Uint32 (* get_frequency)(struct sampled_stream *s_src),
 Uint32 frequency, Uint32 channels, Uint32 use_volume);

#endif // CONFIG_AUDIO_MOD_SYSTEM

__M_END_DECLS

#endif /* __AUDIO_STREAM_SAMPLED_H */
