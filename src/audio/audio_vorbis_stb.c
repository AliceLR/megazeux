/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
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

// TODO:
// - stb_vorbis does not parse Vorbis comments, so we cannot properly support looped music

#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_vorbis_stb.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../../contrib/stb_vorbis/stb_vorbis.c"

struct vorbis_stb_stream
{
  struct sampled_stream s;
  stb_vorbis *stream;
  stb_vorbis_info info;
  Uint32 loop_start;
  Uint32 loop_end;
};

static Uint32 vorbis_stb_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  struct vorbis_stb_stream *stream = (struct vorbis_stb_stream *)a_src;
  Uint32 read_wanted = stream->s.allocated_data_length -
   stream->s.stream_offset;
  Uint32 sample_size = stream->s.channels * 2;
  Uint8 *read_buffer = (Uint8 *)stream->s.output_data +
   stream->s.stream_offset;
  Uint32 r_val = 0;

  Uint32 samples_read = stb_vorbis_get_samples_short_interleaved(stream->stream, stream->s.channels,
    (short*)read_buffer, (read_wanted / sample_size));

  if(a_src->repeat && (samples_read < read_wanted || (stream->loop_start < stream->loop_end && (Uint32)stb_vorbis_get_sample_offset(stream->stream) >= stream->loop_end)))
  {
      stb_vorbis_seek(stream->stream, stream->loop_start);
      samples_read += stb_vorbis_get_samples_short_interleaved(stream->stream, stream->info.channels,
        (short*)read_buffer, (read_wanted / sample_size) - samples_read);
  }
  else if(samples_read < read_wanted)
  {
    r_val = 1;
  }

  sampled_mix_data((struct sampled_stream *)stream, buffer, len);

  return r_val;
}

static void vorbis_stb_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume * 256 / 255;
}

static void vorbis_stb_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

static void vorbis_stb_set_position(struct audio_stream *a_src, Uint32 position)
{
  stb_vorbis_seek(((struct vorbis_stb_stream *)a_src)->stream, position);
}

static void vorbis_stb_set_loop_start(struct audio_stream *a_src, Uint32 position)
{
  ((struct vorbis_stb_stream *)a_src)->loop_start = position;
}

static void vorbis_stb_set_loop_end(struct audio_stream *a_src, Uint32 position)
{
  ((struct vorbis_stb_stream *)a_src)->loop_end = position;
}

static void vorbis_stb_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
    frequency = ((struct vorbis_stb_stream *)s_src)->info.sample_rate;

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 vorbis_stb_get_position(struct audio_stream *a_src)
{
  struct vorbis_stb_stream *v = (struct vorbis_stb_stream *)a_src;
  return stb_vorbis_get_sample_offset(v->stream);
}

static Uint32 vorbis_stb_get_length(struct audio_stream *a_src)
{
  struct vorbis_stb_stream *v = (struct vorbis_stb_stream *)a_src;
  return stb_vorbis_stream_length_in_samples(v->stream);
}

static Uint32 vorbis_stb_get_loop_start(struct audio_stream *a_src)
{
  return ((struct vorbis_stb_stream *)a_src)->loop_start;
}

static Uint32 vorbis_stb_get_loop_end(struct audio_stream *a_src)
{
  return ((struct vorbis_stb_stream *)a_src)->loop_end;
}

static Uint32 vorbis_stb_get_frequency(struct sampled_stream *s_src)
{
  return s_src->frequency;
}

static void vorbis_stb_destruct(struct audio_stream *a_src)
{
  struct vorbis_stb_stream *v_stream = (struct vorbis_stb_stream *)a_src;
  stb_vorbis_close(v_stream->stream);
  sampled_destruct(a_src);
}

static struct audio_stream *construct_vorbis_stb_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  struct vorbis_stb_stream *v_stream = cmalloc(sizeof(struct vorbis_stb_stream));
  struct sampled_stream_spec s_spec;
  struct audio_stream_spec a_spec;

  v_stream->stream = stb_vorbis_open_filename(filename, NULL, NULL);
  if(v_stream->stream == NULL)
  {
    free(v_stream);
    return NULL;
  }

  v_stream->info = stb_vorbis_get_info(v_stream->stream);
  if(v_stream->info.channels > 2)
  {
    stb_vorbis_close(v_stream->stream);
    free(v_stream);
    return NULL;
  }

  v_stream->loop_start = 0;
  v_stream->loop_end = 0;

  memset(&a_spec, 0, sizeof(struct audio_stream_spec));
  a_spec.mix_data       = vorbis_stb_mix_data;
  a_spec.set_volume     = vorbis_stb_set_volume;
  a_spec.set_repeat     = vorbis_stb_set_repeat;
  a_spec.set_position   = vorbis_stb_set_position;
  a_spec.set_loop_start = vorbis_stb_set_loop_start;
  a_spec.set_loop_end   = vorbis_stb_set_loop_end;
  a_spec.get_position   = vorbis_stb_get_position;
  a_spec.get_length     = vorbis_stb_get_length;
  a_spec.get_loop_start = vorbis_stb_get_loop_start;
  a_spec.get_loop_end   = vorbis_stb_get_loop_end;
  a_spec.destruct       = vorbis_stb_destruct;

  memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
  s_spec.set_frequency = vorbis_stb_set_frequency;
  s_spec.get_frequency = vorbis_stb_get_frequency;

  initialize_sampled_stream((struct sampled_stream *)v_stream, &s_spec,
    frequency, v_stream->info.channels, 1);

  initialize_audio_stream((struct audio_stream *)v_stream, &a_spec,
    volume, repeat);

  return (struct audio_stream *)v_stream;
}

void init_vorbis_stb(struct config_info *conf)
{
  audio_ext_register("ogg", construct_vorbis_stb_stream);
}
