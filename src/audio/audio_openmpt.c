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
#include "ext.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../util.h"

#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>

struct openmpt_stream
{
  struct sampled_stream s;
  openmpt_module *module_data;
  Uint32 effective_frequency;
  Uint32 *row_tbl;
  int row_tbl_size;
  Uint32 total_rows;
};

static int openmpt_resample_mode;

static Uint32 omp_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  struct sampled_stream *s = (struct sampled_stream *)a_src;

  Uint32 read_wanted = s->allocated_data_length - s->stream_offset;
  Uint8 *read_buffer = (Uint8 *)s->output_data + s->stream_offset;
  Uint32 r_val = 0;
  Uint32 read_len;

  read_len = openmpt_module_read_interleaved_stereo(omp_stream->module_data,
   s->frequency, read_wanted/4, (int16_t*) read_buffer) * 4;

  sampled_mix_data(s, buffer, len);

  if(read_len < read_wanted && !a_src->repeat)
  {
    memset(read_buffer + read_len, 0, read_wanted - read_len);
    r_val = 1;
  }

  return r_val;
}

static void omp_set_volume(struct audio_stream *a_src, Uint32 volume)
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

static void omp_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  a_src->repeat = repeat;

  openmpt_module_set_repeat_count(omp_stream->module_data, repeat ? -1 : 0);
}

static void omp_set_order(struct audio_stream *a_src, Uint32 order)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  openmpt_module_set_position_order_row(omp_stream->module_data, order, 0);
}

static void omp_set_position(struct audio_stream *a_src, Uint32 position)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  int ord;
  int row;

  for(ord = 1; ord < omp_stream->row_tbl_size; ord++)
  {
    if(position < omp_stream->row_tbl[ord])
    {
      row = position - omp_stream->row_tbl_size;

      openmpt_module_set_position_order_row(omp_stream->module_data, ord, row);
      return;
    }
  }

  // fallback - position out of range
  openmpt_module_set_position_order_row(omp_stream->module_data, 0, 0);
}

static void omp_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
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
    frequency = (Uint32)((float)frequency * audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;
  sampled_set_buffer(s_src);
}

static Uint32 omp_get_order(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  return openmpt_module_get_current_order(omp_stream->module_data);
}

static Uint32 omp_get_position(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  int ord = openmpt_module_get_current_order(omp_stream->module_data);

  return omp_stream->row_tbl[ord] +
   openmpt_module_get_current_row(omp_stream->module_data);
}

static Uint32 omp_get_length(struct audio_stream *a_src)
{
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  return omp_stream->row_tbl[omp_stream->row_tbl_size - 1];
}

static Uint32 omp_get_frequency(struct sampled_stream *s_src)
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
  (void)data;

  if(message)
     fprintf(mzxerr, "%s\n", message);
}

static struct audio_stream *construct_openmpt_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  struct audio_stream *ret_val = NULL;
  FILE *input_file;
  int row_pos;
  int ord;
  int i;

  input_file = fopen_unsafe(filename, "rb");

  if(input_file)
  {
    openmpt_module *open_file =
     openmpt_module_create(openmpt_stream_get_file_callbacks(), input_file,
     &omp_log, NULL, NULL);

    if(open_file)
    {
      struct openmpt_stream *omp_stream = cmalloc(sizeof(struct openmpt_stream));
      struct sampled_stream_spec s_spec;
      struct audio_stream_spec a_spec;
      omp_stream->module_data = open_file;

      omp_stream->row_tbl_size = openmpt_module_get_num_orders(open_file) + 1;
      omp_stream->row_tbl = malloc(sizeof(int) * omp_stream->row_tbl_size);

      for(i = 0, row_pos = 0; i < omp_stream->row_tbl_size; i++)
      {
        omp_stream->row_tbl[i] = row_pos;
        ord = openmpt_module_get_order_pattern(open_file, i);
        if (i < omp_stream->row_tbl_size - 1)
          row_pos += openmpt_module_get_pattern_num_rows(open_file, ord);
      }

      openmpt_module_set_render_param(omp_stream->module_data,
       OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
       openmpt_resample_mode);

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
       frequency, 2, 0);

      initialize_audio_stream((struct audio_stream *)omp_stream, &a_spec,
       volume, repeat);

      ret_val = (struct audio_stream *)omp_stream;
    }

    fclose(input_file);
  }

  return ret_val;
}

void init_openmpt(struct config_info *conf)
{
  // As OpenMPT is configured on a per-module basis, the only thing
  // this "init" function does is store a global variable for usage
  // in module initialization.
  openmpt_resample_mode = 1 << conf->module_resample_mode;

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
