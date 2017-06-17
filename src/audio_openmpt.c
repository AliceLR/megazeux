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
#include "const.h"
#include "util.h"

#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>

struct openmpt_stream
{
  struct sampled_stream s;
  Uint32 *row_tbl;
  int row_tbl_size;
  openmpt_module *module_data;
};

static int openmpt_resample_mode;

static Uint32 omp_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  Uint32 read_len;
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;
  Uint32 read_wanted = omp_stream->s.allocated_data_length - omp_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)omp_stream->s.output_data + omp_stream->s.stream_offset;
  Uint32 r_val = 0;

  read_len = openmpt_module_read_interleaved_stereo(omp_stream->module_data,
omp_stream->s.frequency, read_wanted/4, (int16_t*) read_buffer) * 4;
  sampled_mix_data((struct sampled_stream *)omp_stream, buffer, len);

  if(read_len < read_wanted && !a_src->repeat)
  {
    memset(read_buffer + read_len, 0, read_wanted - read_len);
    r_val = 1;
  }

  return r_val;
}

static void omp_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume;
  openmpt_module_set_render_param(((struct openmpt_stream *)a_src)->module_data,
    OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL,
    volume == 0 ? -2147483648 : (volume - 255) * 15);
}

static void omp_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
  openmpt_module_set_repeat_count(((struct openmpt_stream *)a_src)->module_data, repeat ? -1 : 0);
}

static void omp_set_order(struct audio_stream *a_src, Uint32 order)
{
  openmpt_module_set_position_order_row(((struct openmpt_stream *)a_src)->module_data, order, 0);
}

static void omp_set_position(struct audio_stream *a_src, Uint32 position)
{
  int i;
  struct openmpt_stream *omp_stream = (struct openmpt_stream *)a_src;

  for (i = 1; i < omp_stream->row_tbl_size; i++)
  {
    if (position < omp_stream->row_tbl[i])
    {
      openmpt_module_set_position_order_row(omp_stream->module_data, i, position - omp_stream->row_tbl_size);
      return;
    }
  }

  // fallback - position out of range
  openmpt_module_set_position_order_row(omp_stream->module_data, 0, 0);
}

static void omp_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  s_src->frequency = frequency > 0 ? frequency : 48000;
  sampled_set_buffer(s_src);
}

static Uint32 omp_get_order(struct audio_stream *a_src)
{
  return openmpt_module_get_current_order(((struct openmpt_stream *)a_src)->module_data);
}

static Uint32 omp_get_position(struct audio_stream *a_src)
{
  Uint32 order = (Uint32) openmpt_module_get_current_order(((struct openmpt_stream *)a_src)->module_data);
  return ((struct openmpt_stream *)a_src)->row_tbl[order] + (Uint32) openmpt_module_get_current_row(((struct openmpt_stream *)a_src)->module_data);
}

static Uint32 omp_get_frequency(struct sampled_stream *s_src)
{
  return ((struct sampled_stream *)s_src)->frequency;
}

static void omp_destruct(struct audio_stream *a_src)
{
  openmpt_module_destroy(((struct openmpt_stream *)a_src)->module_data);
  free(((struct openmpt_stream *)a_src)->row_tbl);
  sampled_destruct(a_src);
}

static void omp_log(const char *message, void *data)
{
  (void)data;

  if(message)
     fprintf(stderr, "%s\n", message);
}

struct audio_stream *construct_openmpt_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat)
{
  FILE *input_file;
  int i, row_pos;
  struct audio_stream *ret_val = NULL;

  input_file = fopen_unsafe(filename, "rb");

  if(input_file)
  {
    openmpt_module *open_file = openmpt_module_create(openmpt_stream_get_file_callbacks(),
      input_file, &omp_log, NULL, NULL );

    if(open_file)
    {
      struct openmpt_stream *omp_stream = cmalloc(sizeof(struct openmpt_stream));
      omp_stream->module_data = open_file;
      omp_stream->row_tbl_size = openmpt_module_get_num_orders(open_file) + 1;
      omp_stream->row_tbl = malloc(sizeof(int) * omp_stream->row_tbl_size);
      for(i = 0, row_pos = 0; i < omp_stream->row_tbl_size; i++)
      {
        omp_stream->row_tbl[i] = row_pos;
        if (i < omp_stream->row_tbl_size - 1)
          row_pos += openmpt_module_get_pattern_num_rows(open_file, openmpt_module_get_order_pattern(open_file, i));
      }

      openmpt_module_set_render_param(omp_stream->module_data,
        OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
        openmpt_resample_mode);
      openmpt_module_set_repeat_count(omp_stream->module_data, -1);

      initialize_sampled_stream((struct sampled_stream *)omp_stream,
       omp_set_frequency, omp_get_frequency, frequency, 2, 0);

      ret_val = (struct audio_stream *)omp_stream;

      construct_audio_stream((struct audio_stream *)omp_stream,
       omp_mix_data, omp_set_volume, omp_set_repeat, omp_set_order,
       omp_set_position, omp_get_order, omp_get_position, omp_destruct,
       volume, repeat);
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
  openmpt_resample_mode = 1 << conf->modplug_resample_mode;
}
