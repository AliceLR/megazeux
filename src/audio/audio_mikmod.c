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

// Provides a Mikmod module stream backend

#include "audio.h"
#include "audio_mikmod.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../util.h"
#include "../io/vfile.h"

#ifdef _WIN32
#define MIKMOD_STATIC
#endif

#include <mikmod.h>

struct mikmod_stream
{
  struct sampled_stream s;
  MODULE *module_data;
  Uint32 effective_frequency;
  Uint32 num_orders;
  Uint32 *order_to_pos_table;
};

struct LMM_MREADER
{
  MREADER mr;
  vfile *vf;
  int eof;
};

static BOOL LMM_Seek(struct MREADER *mr, long to, int dir)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  return vfseek(lmmmr->vf, to, dir);
}

static long LMM_Tell(struct MREADER *mr)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  return vftell(lmmmr->vf);
}

static BOOL LMM_Read(struct MREADER *mr, void *buf, size_t sz)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  return vfread(buf, sz, 1, lmmmr->vf);
}

static int LMM_Get(struct MREADER *mr)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  return vfgetc(lmmmr->vf);
}

static BOOL LMM_Eof(struct MREADER *mr)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER*)mr;
  return vftell(lmmmr->vf) >= lmmmr->eof;
}

static MODULE *mm_load_vfile(vfile *vf, int maxchan)
{
  struct LMM_MREADER lmmmr;

  memset(&lmmmr, 0, sizeof(struct LMM_MREADER));
  lmmmr.mr.Seek = LMM_Seek;
  lmmmr.mr.Tell = LMM_Tell;
  lmmmr.mr.Read = LMM_Read;
  lmmmr.mr.Get = LMM_Get;
  lmmmr.mr.Eof = LMM_Eof;

  lmmmr.vf = vf;
  lmmmr.eof = vfilelength(vf, false);

  return Player_LoadGeneric((MREADER *)&lmmmr, maxchan, 0);
}

/**
 * Use MikMod internal data to provide support for length and position.
 * TODO: Dunno how stable this is :-(
 */
static void mm_init_order_table(struct mikmod_stream *mm_stream, MODULE *mm_data)
{
  Uint32 current = 0;
  Uint32 i;
  Uint32 *tbl;
  Uint32 num_orders;

  num_orders = mm_data->numpos;
  tbl = cmalloc((num_orders + 1) * sizeof(Uint32));

  for(i = 0; i < num_orders; i++)
  {
    Uint32 pattern = mm_data->positions[i];

    tbl[i] = current;

    if(pattern < mm_data->numpat)
      current += mm_data->pattrows[pattern];
  }
  tbl[num_orders] = current;

  mm_stream->num_orders = num_orders;
  mm_stream->order_to_pos_table = tbl;
}

static boolean mm_position_to_order_row(struct mikmod_stream *mm_stream,
 Uint32 position, Uint32 *order, Uint32 *row)
{
  Uint32 *tbl = mm_stream->order_to_pos_table;
  Uint32 i;

  for(i = 0; i < mm_stream->num_orders; i++)
  {
    if(tbl[i] <= position && tbl[i + 1] > position)
    {
      *order = i;
      *row = position - tbl[i];
      return true;
    }
  }
  return false;
}

static Uint32 mm_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  Uint32 read_wanted = mm_stream->s.allocated_data_length -
   mm_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)mm_stream->s.output_data +
   mm_stream->s.stream_offset;

  VC_WriteBytes((SBYTE*)read_buffer, read_wanted);
  sampled_mix_data((struct sampled_stream *)mm_stream, buffer, len);
  return 0;
}

static void mm_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume;
  Player_SetVolume((SWORD)(volume/2));
}

static void mm_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  MODULE *mm_file = ((struct mikmod_stream *)a_src)->module_data;

  a_src->repeat = repeat;

  if(repeat)
    mm_file->wrap = 1;
  else
    mm_file->wrap = 0;
}

static void mm_set_order(struct audio_stream *a_src, Uint32 order)
{
  Player_SetPosition(order);
}

static void mm_set_position(struct audio_stream *a_src, Uint32 position)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  Uint32 order, row;

  if(mm_position_to_order_row(mm_stream, position, &order, &row))
  {
    Player_SetPosition(order);

    // FIXME this doesn't actually work, not sure what else to try.
    mm_stream->module_data->patpos = row;
  }
}

static void mm_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
  {
    ((struct mikmod_stream *)s_src)->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    ((struct mikmod_stream *)s_src)->effective_frequency = frequency;
    frequency = (Uint32)((float)frequency *
     audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 mm_get_order(struct audio_stream *a_src)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  return mm_stream->module_data->sngpos;
}

static Uint32 mm_get_position(struct audio_stream *a_src)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  MODULE *mm_data = mm_stream->module_data;
  Uint32 order = CLAMP(mm_data->sngpos, 0, (Sint32)mm_stream->num_orders);

  return mm_stream->order_to_pos_table[order] + mm_data->patpos;
}

static Uint32 mm_get_length(struct audio_stream *a_src)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  return mm_stream->order_to_pos_table[mm_stream->num_orders];
}

static Uint32 mm_get_frequency(struct sampled_stream *s_src)
{
  return ((struct mikmod_stream *)s_src)->effective_frequency;
}

static void mm_destruct(struct audio_stream *a_src)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  Player_Stop();
  Player_Free(mm_stream->module_data);
  free(mm_stream->order_to_pos_table);
  sampled_destruct(a_src);
}

static struct audio_stream *construct_mikmod_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  vfile *input_file;
  struct audio_stream *ret_val = NULL;

  input_file = vfopen_unsafe(filename, "rb");

  if(input_file)
  {
    MODULE *open_file = mm_load_vfile(input_file, 64);

    if(open_file)
    {
      struct mikmod_stream *mm_stream = cmalloc(sizeof(struct mikmod_stream));
      struct sampled_stream_spec s_spec;
      struct audio_stream_spec a_spec;
      mm_stream->module_data = open_file;
      Player_Start(mm_stream->module_data);

      mm_init_order_table(mm_stream, open_file);

      memset(&a_spec, 0, sizeof(struct audio_stream_spec));
      a_spec.mix_data     = mm_mix_data;
      a_spec.set_volume   = mm_set_volume;
      a_spec.set_repeat   = mm_set_repeat;
      a_spec.set_order    = mm_set_order;
      a_spec.set_position = mm_set_position;
      a_spec.get_order    = mm_get_order;
      a_spec.get_position = mm_get_position;
      a_spec.get_length   = mm_get_length;
      a_spec.destruct     = mm_destruct;

      memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
      s_spec.set_frequency = mm_set_frequency;
      s_spec.get_frequency = mm_get_frequency;

      initialize_sampled_stream((struct sampled_stream *)mm_stream, &s_spec,
       frequency, 2, 0);

      initialize_audio_stream((struct audio_stream *)mm_stream, &a_spec,
       volume, repeat);

      ret_val = (struct audio_stream *)mm_stream;
    }

    vfclose(input_file);
  }

  return ret_val;
}

void init_mikmod(struct config_info *conf)
{
  md_mixfreq = audio.output_frequency;
  md_mode = DMODE_16BITS;
  md_mode |= DMODE_STEREO;
  md_device = 0;
  md_volume = 96;
  md_musicvolume = 128;
  md_sndfxvolume = 128;
  md_pansep = 128;
  md_reverb = 0;
  md_mode |= DMODE_SOFT_MUSIC | DMODE_SURROUND;

  MikMod_RegisterDriver(&drv_nos);

  /* XM and AMF seem to be broken with Mikmod? */

  MikMod_RegisterLoader(&load_gdm);
  //MikMod_RegisterLoader(&load_xm);
  MikMod_RegisterLoader(&load_s3m);
  MikMod_RegisterLoader(&load_mod);
  MikMod_RegisterLoader(&load_m15); // Soundtracker .MOD
  MikMod_RegisterLoader(&load_med);
  MikMod_RegisterLoader(&load_mtm);
  MikMod_RegisterLoader(&load_stm);
  MikMod_RegisterLoader(&load_it);
  MikMod_RegisterLoader(&load_669);
  MikMod_RegisterLoader(&load_ult);
  MikMod_RegisterLoader(&load_dsm);
  MikMod_RegisterLoader(&load_far);
  MikMod_RegisterLoader(&load_okt);
  //MikMod_RegisterLoader(&load_amf);

  // FIXME: Should break a lot more here
  if(MikMod_Init(NULL))
    fprintf(stderr, "MikMod Init failed: %s", MikMod_strerror(MikMod_errno));

  audio_ext_register("669", construct_mikmod_stream);
  //audio_ext_register("amf", construct_mikmod_stream);
  audio_ext_register("dsm", construct_mikmod_stream);
  audio_ext_register("far", construct_mikmod_stream);
  audio_ext_register("gdm", construct_mikmod_stream);
  audio_ext_register("it", construct_mikmod_stream);
  audio_ext_register("med", construct_mikmod_stream);
  audio_ext_register("mod", construct_mikmod_stream);
  audio_ext_register("mtm", construct_mikmod_stream);
  audio_ext_register("okt", construct_mikmod_stream);
  audio_ext_register("s3m", construct_mikmod_stream);
  audio_ext_register("stm", construct_mikmod_stream);
  audio_ext_register("ult", construct_mikmod_stream);
  //audio_ext_register("xm", construct_mikmod_stream);
}
