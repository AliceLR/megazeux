/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Simon Parzer <simon.parzer@gmail.com>
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

// Provides a libopenmpt module stream backend

#include "audio.h"
#include "audio_openmpt.h"
#include "audio_struct.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../util.h"
#include "../io/vio.h"

#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>

struct openmpt_stream
{
  struct sampled_stream s;
  openmpt_module *module_data;
  uint32_t effective_frequency;
  uint32_t *row_tbl;
  uint32_t row_tbl_size;
  uint32_t total_rows;
};

// OpenMPT's resampling option takes interpolation taps instead of an enum.
static int omp_get_resample_mode(void)
{
  struct config_info *conf = get_config();

  switch(conf->module_resample_mode)
  {
    case RESAMPLE_MODE_NONE:
      return 1;

    case RESAMPLE_MODE_LINEAR:
    default:
      return 2;

    case RESAMPLE_MODE_CUBIC:
      return 4;

    case RESAMPLE_MODE_FIR:
      return 8;
  }
}

static boolean omp_mix_data(struct audio_stream *a_src, int32_t *buffer,
 size_t frames, unsigned int channels)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  struct sampled_stream *s = (struct sampled_stream *)a_src;

  uint32_t read_wanted = s->allocated_data_length - s->stream_offset;
  uint8_t *read_buffer = (uint8_t *)s->output_data + s->stream_offset;
  boolean r_val = false;
  uint32_t read_len;

  read_len = openmpt_module_read_interleaved_stereo(omp_stream->module_data,
   s->frequency, read_wanted / 4, (int16_t *)read_buffer) * 4;

  if(read_len < read_wanted && !a_src->repeat)
  {
    memset(read_buffer + read_len, 0, read_wanted - read_len);
    r_val = true;
  }

  sampled_mix_data(s, buffer, frames, channels);

  return r_val;
}

static void omp_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  a_src->volume = volume;

  if(volume == 0)
    volume = INT_MIN;

  else
    volume = (volume - 255) * 15;

  openmpt_module_set_render_param(omp_stream->module_data,
    OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL, volume);
}

static void omp_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  a_src->repeat = repeat;

  openmpt_module_set_repeat_count(omp_stream->module_data, repeat ? -1 : 0);
}

static void omp_set_order(struct audio_stream *a_src, uint32_t order)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  openmpt_module_set_position_order_row(omp_stream->module_data, order, 0);
}

static void omp_set_position(struct audio_stream *a_src, uint32_t position)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  uint32_t ord;
  uint32_t row;

  for(ord = 1; ord < omp_stream->row_tbl_size; ord++)
  {
    if(position < omp_stream->row_tbl[ord])
    {
      row = position - omp_stream->row_tbl[ord - 1];

      openmpt_module_set_position_order_row(omp_stream->module_data, ord, row);
      return;
    }
  }

  // fallback - position out of range
  openmpt_module_set_position_order_row(omp_stream->module_data, 0, 0);
}

static void omp_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)s_src;

  if(frequency == 0)
  {
    omp_stream->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    omp_stream->effective_frequency = frequency;
    frequency = (uint32_t)((float)frequency * audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;
  sampled_set_buffer(s_src);
}

static uint32_t omp_get_order(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  return openmpt_module_get_current_order(omp_stream->module_data);
}

static uint32_t omp_get_position(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  int ord = openmpt_module_get_current_order(omp_stream->module_data);

  return omp_stream->row_tbl[ord] +
   openmpt_module_get_current_row(omp_stream->module_data);
}

static uint32_t omp_get_length(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  return omp_stream->row_tbl[omp_stream->row_tbl_size - 1];
}

static uint32_t omp_get_frequency(struct sampled_stream *s_src)
{
  return ((struct sampled_stream *)s_src)->frequency;
}

static void omp_destruct(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  openmpt_module_destroy(omp_stream->module_data);
  free(omp_stream->row_tbl);
  sampled_destruct(a_src);
}

static void omp_log(const char *message, void *data)
{
  if(message)
     fprintf(mzxerr, "%s\n", message);
}

static size_t omp_read_fn(void *stream, void *dest, size_t bytes)
{
  vfile *vf = (vfile *)stream;
  return vfread(dest, 1, bytes, vf);
}

static int omp_seek_fn(void *stream, int64_t offset, int whence)
{
  vfile *vf = (vfile *)stream;
  return vfseek(vf, offset, whence);
}

static int64_t omp_tell_fn(void *stream)
{
  vfile *vf = (vfile *)stream;
  return vftell(vf);
}

static const struct openmpt_stream_callbacks omp_callbacks =
{
  omp_read_fn,
  omp_seek_fn,
  omp_tell_fn
};

static struct audio_stream *construct_openmpt_stream(char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  vfile *input_file;
  uint32_t row_pos;
  uint32_t ord;
  uint32_t i;

  input_file = vfopen_unsafe(filename, "rb");

  if(input_file)
  {
    openmpt_module *open_file = openmpt_module_create2(omp_callbacks, input_file,
     &omp_log, NULL, NULL, NULL, NULL, NULL, NULL);

    vfclose(input_file);

    if(open_file)
    {
      struct openmpt_stream *omp_stream = cmalloc(sizeof(struct openmpt_stream));
      struct sampled_stream_spec s_spec;
      struct audio_stream_spec a_spec;
      omp_stream->module_data = open_file;

      omp_stream->row_tbl_size = openmpt_module_get_num_orders(open_file) + 1;
      omp_stream->row_tbl = malloc(sizeof(uint32_t) * omp_stream->row_tbl_size);

      for(i = 0, row_pos = 0; i < omp_stream->row_tbl_size; i++)
      {
        omp_stream->row_tbl[i] = row_pos;
        ord = openmpt_module_get_order_pattern(open_file, i);
        if(i < omp_stream->row_tbl_size - 1)
          row_pos += openmpt_module_get_pattern_num_rows(open_file, ord);
      }

      openmpt_module_set_render_param(omp_stream->module_data,
       OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
       omp_get_resample_mode());

      openmpt_module_set_repeat_count(omp_stream->module_data, -1);

      memset(&a_spec, 0, sizeof(struct audio_stream_spec));
      a_spec.mix_data     = omp_mix_data;
      a_spec.set_volume   = omp_set_volume;
      a_spec.set_repeat   = omp_set_repeat;
      a_spec.set_order    = omp_set_order;
      a_spec.set_position = omp_set_position;
      a_spec.get_order    = omp_get_order;
      a_spec.get_position = omp_get_position;
      a_spec.get_length   = omp_get_length;
      a_spec.destruct     = omp_destruct;

      memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
      s_spec.set_frequency = omp_set_frequency;
      s_spec.get_frequency = omp_get_frequency;

      initialize_sampled_stream((struct sampled_stream *)omp_stream, &s_spec,
       frequency, 2, false);

      initialize_audio_stream((struct audio_stream *)omp_stream, &a_spec,
       volume, repeat);

      return (struct audio_stream *)omp_stream;
    }
  }
  return NULL;
}

void init_openmpt(struct config_info *conf)
{
  audio_ext_register("669", construct_openmpt_stream);
  audio_ext_register("amf", construct_openmpt_stream);
  audio_ext_register("dsm", construct_openmpt_stream);
  audio_ext_register("far", construct_openmpt_stream);
  audio_ext_register("gdm", construct_openmpt_stream);
  audio_ext_register("it", construct_openmpt_stream);
  audio_ext_register("med", construct_openmpt_stream);
  audio_ext_register("mod", construct_openmpt_stream);
  audio_ext_register("mtm", construct_openmpt_stream);
  audio_ext_register("nst", construct_openmpt_stream);
  audio_ext_register("oct", construct_openmpt_stream);
  audio_ext_register("okt", construct_openmpt_stream);
  audio_ext_register("s3m", construct_openmpt_stream);
  audio_ext_register("stm", construct_openmpt_stream);
  audio_ext_register("ult", construct_openmpt_stream);
  audio_ext_register("wow", construct_openmpt_stream);
  audio_ext_register("xm", construct_openmpt_stream);
}
