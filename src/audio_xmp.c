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

// Provides a libxmp module stream backend

#include "audio.h"
#include "audio_xmp.h"
#include "const.h"
#include "util.h"

//#ifdef __WIN32__
//#define BUILDING_STATIC
//#endif

#include <xmp.h>

struct xmp_stream
{
  struct sampled_stream s;
  xmp_context ctx;
  int row_tbl[XMP_MAX_MOD_LENGTH];
  Uint32 effective_frequency;
  Uint32 total_rows;
  Uint32 repeat;
};

static int xmp_resample_mode;

static Uint32 audio_xmp_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  Uint32 read_wanted = stream->s.allocated_data_length -
   stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)stream->s.output_data +
   stream->s.stream_offset;
  Uint32 r_val = 0;

  int xmp_r_val =
   xmp_play_buffer(stream->ctx, read_buffer, read_wanted, a_src->repeat ? 0 : 1);

  if(xmp_r_val == -XMP_END && !a_src->repeat)
  {
    r_val = 1;
  }

  sampled_mix_data((struct sampled_stream *)stream, buffer, len);

  return r_val;
}

static void audio_xmp_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume;
  xmp_set_player(((struct xmp_stream *)a_src)->ctx, XMP_PLAYER_VOLUME,
   volume * 100 / 255);
}

static void audio_xmp_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

static void audio_xmp_set_position(struct audio_stream *a_src, Uint32 position)
{
  struct xmp_frame_info info;
  int i;
  xmp_get_frame_info(((struct xmp_stream *)a_src)->ctx, &info);
  // FIXME: xmp does not have a way to set the position accurately. Here, we try
  // to narrow it down to the nearest order.
  for (i = 1; i < XMP_MAX_MOD_LENGTH; i++)
    if ((Uint32) ((struct xmp_stream *)a_src)->row_tbl[i] > position)
    {
      xmp_set_position(((struct xmp_stream *)a_src)->ctx, i - 1);
      return;
    }
  xmp_set_position(((struct xmp_stream *)a_src)->ctx, 0);
}

static void audio_xmp_set_order(struct audio_stream *a_src, Uint32 order)
{
  xmp_set_position(((struct xmp_stream *)a_src)->ctx, order);
}

static void audio_xmp_set_frequency(struct sampled_stream *s_src,
 Uint32 frequency)
{
  struct xmp_stream *stream = (struct xmp_stream *)s_src;

  if(frequency == 0)
  {
    stream->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    stream->effective_frequency = frequency;
    frequency = (Uint32)((float)frequency * audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 audio_xmp_get_order(struct audio_stream *a_src)
{
  struct xmp_frame_info info;
  xmp_get_frame_info(((struct xmp_stream *)a_src)->ctx, &info);
  return info.pos;
}

static Uint32 audio_xmp_get_position(struct audio_stream *a_src)
{
  struct xmp_frame_info info;
  xmp_get_frame_info(((struct xmp_stream *)a_src)->ctx, &info);
  return ((struct xmp_stream *)a_src)->row_tbl[info.pos] + info.row;
}

static Uint32 audio_xmp_get_length(struct audio_stream *a_src)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;

  return stream->total_rows;
}

static Uint32 audio_xmp_get_frequency(struct sampled_stream *s_src)
{
  return ((struct xmp_stream *)s_src)->effective_frequency;
}

static void audio_xmp_destruct(struct audio_stream *a_src)
{
  xmp_end_player(((struct xmp_stream *)a_src)->ctx);
  xmp_release_module(((struct xmp_stream *)a_src)->ctx);
  xmp_free_context(((struct xmp_stream *)a_src)->ctx);
  sampled_destruct(a_src);
}

struct audio_stream *construct_xmp_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat)
{
  struct audio_stream *ret_val = NULL;
  struct xmp_module_info info;
  xmp_context ctx;
  unsigned char ord;
  int row_pos;
  int i;

  ctx = xmp_create_context();
  if(ctx)
  {
    xmp_set_player(ctx, XMP_PLAYER_DEFPAN, 50);

    switch(xmp_resample_mode)
    {
      case 0:
      {
        xmp_set_player(ctx, XMP_PLAYER_INTERP, XMP_INTERP_NEAREST);
        break;
      }

      case 1:
      {
        xmp_set_player(ctx, XMP_PLAYER_INTERP, XMP_INTERP_LINEAR);
        break;
      }

      case 2:
      case 3:
      {
        xmp_set_player(ctx, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);
        break;
      }
    }

    if(xmp_load_module(ctx, filename) == 0)
    {
      struct xmp_stream *xmp_stream = cmalloc(sizeof(struct xmp_stream));
      int num_orders;

      xmp_stream->ctx = ctx;
      xmp_start_player(ctx, audio.output_frequency, 0);

      xmp_get_module_info(ctx, &info);
      num_orders = info.mod->len;

      for(i = 0, row_pos = 0; i < num_orders; i++)
      {
        xmp_stream->row_tbl[i] = row_pos;
        ord = info.mod->xxo[i];
        if(ord < info.mod->pat)
          row_pos += info.mod->xxp[ord]->rows;
      }
      xmp_stream->total_rows = row_pos;

      initialize_sampled_stream((struct sampled_stream *)xmp_stream,
       audio_xmp_set_frequency, audio_xmp_get_frequency, frequency, 2, 0);

      ret_val = (struct audio_stream *)xmp_stream;

      construct_audio_stream((struct audio_stream *)xmp_stream,
       audio_xmp_mix_data, audio_xmp_set_volume, audio_xmp_set_repeat,
       audio_xmp_set_order, audio_xmp_set_position, audio_xmp_get_order,
       audio_xmp_get_position, audio_xmp_get_length, audio_xmp_destruct,
       volume, repeat);
    }
  }

  return ret_val;
}

void init_xmp(struct config_info *conf)
{
  xmp_resample_mode = conf->modplug_resample_mode;
}
