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
#include "audio_wav.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../util.h"

//#ifdef __WIN32__
//#define BUILDING_STATIC
//#endif

#include <xmp.h>

struct xmp_stream
{
  struct sampled_stream s;
  xmp_context ctx;
  Uint32 row_tbl[XMP_MAX_MOD_LENGTH];
  Uint32 effective_frequency;
  Uint32 total_rows;
  Uint32 repeat;
};

static int get_xmp_resample_mode(void)
{
  struct config_info *conf = get_config();

  switch(conf->module_resample_mode)
  {
    case RESAMPLE_MODE_NONE:
      return XMP_INTERP_NEAREST;

    case RESAMPLE_MODE_LINEAR:
    default:
      return XMP_INTERP_LINEAR;

    case RESAMPLE_MODE_CUBIC:
    case RESAMPLE_MODE_FIR:
      return XMP_INTERP_SPLINE;
  }
}

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
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  a_src->volume = volume;

  // xmp uses SMIX volume for all extra channels, but some files can generate
  // these channels through normal behavior. Since we don't use SMIX, we can
  // safely set both volumes to the same value.
  xmp_set_player(stream->ctx, XMP_PLAYER_VOLUME, volume * 100 / 255);
  xmp_set_player(stream->ctx, XMP_PLAYER_SMIX_VOLUME, volume * 100 / 255);
}

static void audio_xmp_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

static void audio_xmp_set_position(struct audio_stream *a_src, Uint32 position)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  struct xmp_frame_info info;
  int i;

  xmp_get_frame_info(stream->ctx, &info);

  for(i = 1; i < XMP_MAX_MOD_LENGTH; i++)
  {
    if(stream->row_tbl[i] > position)
    {
      int position_row = position - stream->row_tbl[i - 1];
      xmp_set_position(stream->ctx, i - 1);
      xmp_set_row(stream->ctx, position_row);
      return;
    }
  }
  xmp_set_position(stream->ctx, 0);
}

static void audio_xmp_set_order(struct audio_stream *a_src, Uint32 order)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;
  // XMP won't jump to the start of the specified order it's already playing
  // most of the time, so set the row too.
  xmp_set_position(stream->ctx, order);
  xmp_set_row(stream->ctx, 0);
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

// Return a duplicate of a sample from the current playing mod.
static boolean audio_xmp_get_sample(struct audio_stream *a_src, Uint32 which,
 struct wav_info *dest)
{
  struct xmp_stream *stream = (struct xmp_stream *)a_src;

  struct xmp_module_info info;
  xmp_get_module_info(stream->ctx, &info);

  if(info.mod && info.mod->xxs && (info.mod->smp > (int)which))
  {
    struct xmp_sample *sam = &(info.mod->xxs[which]);

    // I don't know if it's even possible for synth samples to get this far,
    // but check for them so they can be ignored anyway.
    if(sam->len && !(sam->flg & XMP_SAMPLE_SYNTH))
    {
      dest->channels = 1;
      dest->freq = audio_get_real_frequency(SAM_DEFAULT_PERIOD);
      dest->data_length = sam->len;

      // If the sample loops, the sample this returns needs to loop too.
      if(sam->flg & XMP_SAMPLE_LOOP)
      {
        dest->loop_start = sam->lps;
        dest->loop_end = sam->lpe;
      }
      else
        dest->loop_start = dest->loop_end = 0;

      // XMP samples are signed, and if 16bit, LSB, so just copy it directly.
      if(sam->flg & XMP_SAMPLE_16BIT)
      {
        dest->format = SAMPLE_S16LSB;
        dest->data_length *= 2;
      }
      else
        dest->format = SAMPLE_S8;

      dest->wav_data = cmalloc(dest->data_length);
      memcpy(dest->wav_data, sam->data, dest->data_length);
      return true;
    }
  }
  return false;
}

static void audio_xmp_destruct(struct audio_stream *a_src)
{
  xmp_end_player(((struct xmp_stream *)a_src)->ctx);
  xmp_release_module(((struct xmp_stream *)a_src)->ctx);
  xmp_free_context(((struct xmp_stream *)a_src)->ctx);
  sampled_destruct(a_src);
}

static struct audio_stream *construct_xmp_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  struct xmp_module_info info;
  xmp_context ctx;
  unsigned char ord;
  Uint32 row_pos;
  int i;

  // Open a file handle here and pass xmp the handle. This is a workaround so
  // platform-specific MZX path hacks will work without modifying libxmp.
  FILE *fp = fopen_unsafe(filename, "rb");
  size_t file_len;
  if(!fp)
    return NULL;

  file_len = ftell_and_rewind(fp);

  ctx = xmp_create_context();
  if(ctx)
  {
    xmp_set_player(ctx, XMP_PLAYER_DEFPAN, 50);

    // NOTE: this function uses fdopen(fileno(fp)); when the FILE * this opens
    // is closed, it will also close the FILE * here, so don't close it.
    if(!xmp_load_module_from_file(ctx, fp, file_len))
    {
      struct xmp_stream *xmp_stream = ccalloc(1, sizeof(struct xmp_stream));
      struct sampled_stream_spec s_spec;
      struct audio_stream_spec a_spec;
      int num_orders;

      xmp_stream->ctx = ctx;
      xmp_start_player(ctx, audio.output_frequency, 0);
      xmp_set_player(ctx, XMP_PLAYER_INTERP, get_xmp_resample_mode());

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

      // xmp tries to be clever and uses "sequences" to make certain mods loop
      // to orders that aren't the start of the mod. This breaks legacy mods
      // that rely on looping to the start, so zero all of their entry points.
      for(i = 0; i < info.num_sequences; i++)
      {
        info.seq_data[i].entry_point = 0;
      }

      memset(&a_spec, 0, sizeof(struct audio_stream_spec));
      a_spec.mix_data     = audio_xmp_mix_data;
      a_spec.set_volume   = audio_xmp_set_volume;
      a_spec.set_repeat   = audio_xmp_set_repeat;
      a_spec.set_order    = audio_xmp_set_order;
      a_spec.set_position = audio_xmp_set_position;
      a_spec.get_order    = audio_xmp_get_order;
      a_spec.get_position = audio_xmp_get_position;
      a_spec.get_length   = audio_xmp_get_length;
      a_spec.get_sample   = audio_xmp_get_sample;
      a_spec.destruct     = audio_xmp_destruct;

      memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
      s_spec.set_frequency = audio_xmp_set_frequency;
      s_spec.get_frequency = audio_xmp_get_frequency;

      initialize_sampled_stream((struct sampled_stream *)xmp_stream, &s_spec,
       frequency, 2, 0);

      initialize_audio_stream((struct audio_stream *)xmp_stream, &a_spec,
       volume, repeat);

      return (struct audio_stream *)xmp_stream;
    }
    xmp_free_context(ctx);
  }
  else
    fclose(fp);

  return NULL;
}

void init_xmp(struct config_info *conf)
{
  audio_ext_register("669", construct_xmp_stream);
  audio_ext_register("amf", construct_xmp_stream);
  //audio_ext_register("dsm", construct_xmp_stream);
  audio_ext_register("far", construct_xmp_stream);
  audio_ext_register("gdm", construct_xmp_stream);
  audio_ext_register("it", construct_xmp_stream);
  audio_ext_register("med", construct_xmp_stream);
  audio_ext_register("mod", construct_xmp_stream);
  audio_ext_register("mtm", construct_xmp_stream);
  audio_ext_register("okt", construct_xmp_stream);
  audio_ext_register("s3m", construct_xmp_stream);
  audio_ext_register("stm", construct_xmp_stream);
  audio_ext_register("ult", construct_xmp_stream);
  audio_ext_register("xm", construct_xmp_stream);
}
