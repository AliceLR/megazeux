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
#include "audio_struct.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../util.h"
#include "../io/vio.h"

#ifdef _WIN32
#define MIKMOD_STATIC
#endif

#include <mikmod.h>

struct mikmod_stream
{
  struct sampled_stream s;
  MODULE *module_data;
  uint32_t effective_frequency;
  uint32_t num_orders;
  uint32_t *order_to_pos_table;
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
 * Handle the resample mode for MikMod.
 * This is a global rather than a per-module setting.
 */
static void mm_set_resample_mode(void)
{
  struct config_info *conf = get_config();
  switch(conf->module_resample_mode)
  {
    case RESAMPLE_MODE_NONE:
      md_mode = md_mode & ~(DMODE_INTERP);
      break;

    case RESAMPLE_MODE_LINEAR:
    case RESAMPLE_MODE_CUBIC:
    case RESAMPLE_MODE_FIR:
    default:
      md_mode |= DMODE_INTERP;
      break;
  }
}

/**
 * Use MikMod internal data to provide support for length and position.
 * TODO: Dunno how stable this is :-(
 */
static void mm_init_order_table(struct mikmod_stream *mm_stream, MODULE *mm_data)
{
  uint32_t current = 0;
  uint32_t i;
  uint32_t *tbl;
  uint32_t num_orders;

  num_orders = mm_data->numpos;
  tbl = cmalloc((num_orders + 1) * sizeof(uint32_t));

  for(i = 0; i < num_orders; i++)
  {
    uint32_t pattern = mm_data->positions[i];

    tbl[i] = current;

    if(pattern < mm_data->numpat)
      current += mm_data->pattrows[pattern];
  }
  tbl[num_orders] = current;

  mm_stream->num_orders = num_orders;
  mm_stream->order_to_pos_table = tbl;
}

static boolean mm_position_to_order_row(struct mikmod_stream *mm_stream,
 uint32_t position, uint32_t *order, uint32_t *row)
{
  uint32_t *tbl = mm_stream->order_to_pos_table;
  uint32_t i;

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

static boolean mm_mix_data(struct audio_stream *a_src, int32_t * RESTRICT buffer,
 size_t frames, unsigned int channels)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  uint32_t read_wanted = mm_stream->s.allocated_data_length -
   mm_stream->s.stream_offset;
  uint8_t *read_buffer = (uint8_t *)mm_stream->s.output_data +
   mm_stream->s.stream_offset;

  VC_WriteBytes((SBYTE *)read_buffer, read_wanted);
  sampled_mix_data((struct sampled_stream *)mm_stream, buffer, frames, channels);
  return false;
}

static void mm_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  a_src->volume = volume;
  Player_SetVolume((SWORD)(volume/2));
}

static void mm_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  MODULE *mm_file = ((struct mikmod_stream *)a_src)->module_data;

  a_src->repeat = repeat;

  if(repeat)
    mm_file->wrap = 1;
  else
    mm_file->wrap = 0;
}

static void mm_set_order(struct audio_stream *a_src, uint32_t order)
{
  Player_SetPosition(order);
}

static void mm_set_position(struct audio_stream *a_src, uint32_t position)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  uint32_t order, row;

  if(mm_position_to_order_row(mm_stream, position, &order, &row))
  {
    Player_SetPosition(order);

    /**
     * TODO: DIRTY HACK!
     *
     * Setting the row directly doesn't affect anything because SetPosition
     * sets data->posjmp, which will make MikMod reset the row during the next
     * tick to data->patbrk % data->numrow. So instead of setting the row
     * directly, set patbrk after using Player_SetPosition to force MikMod to
     * jump to this row when it processes the next tick.
     */
    mm_stream->module_data->patbrk = row;
  }
}

static void mm_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  if(frequency == 0)
  {
    ((struct mikmod_stream *)s_src)->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    ((struct mikmod_stream *)s_src)->effective_frequency = frequency;
    frequency = (uint32_t)((float)frequency *
     audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static uint32_t mm_get_order(struct audio_stream *a_src)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  return mm_stream->module_data->sngpos;
}

static uint32_t mm_get_position(struct audio_stream *a_src)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  MODULE *mm_data = mm_stream->module_data;
  uint32_t order = CLAMP(mm_data->sngpos, 0, (int)mm_stream->num_orders);

  return mm_stream->order_to_pos_table[order] + mm_data->patpos;
}

static uint32_t mm_get_length(struct audio_stream *a_src)
{
  struct mikmod_stream *mm_stream = (struct mikmod_stream *)a_src;
  return mm_stream->order_to_pos_table[mm_stream->num_orders];
}

static uint32_t mm_get_frequency(struct sampled_stream *s_src)
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
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  vfile *input_file;

  /**
   * FIXME since MikMod uses a global player state, attempting to play
   * multiple modules at the same time predictably causes crashes. Use
   * this hack to ignore any modules played as samples for now. :(
   */
  if(!repeat)
    return NULL;

  input_file = vfopen_unsafe(filename, "rb");

  if(input_file)
  {
    MODULE *open_file = mm_load_vfile(input_file, 64);
    vfclose(input_file);

    if(open_file)
    {
      struct mikmod_stream *mm_stream = cmalloc(sizeof(struct mikmod_stream));
      struct sampled_stream_spec s_spec;
      struct audio_stream_spec a_spec;
      mm_stream->module_data = open_file;
      Player_Start(mm_stream->module_data);

      mm_init_order_table(mm_stream, open_file);
      mm_set_resample_mode();

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
       frequency, 2, false);

      initialize_audio_stream((struct audio_stream *)mm_stream, &a_spec,
       volume, repeat);

      return (struct audio_stream *)mm_stream;
    }
    else
      debug("MikMod failed to open '%s': %s\n", filename,
       MikMod_strerror(MikMod_errno));
  }
  return NULL;
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

  /**
   * These are arranged in the same order as MikMod_RegisterAllLoaders
   * registers them--alphabetical, with the exception of SoundTracker 15-sample
   * .MODs last (presumably since they do not have a magic string).
   */
  MikMod_RegisterLoader(&load_669);
  MikMod_RegisterLoader(&load_amf); // DSMI .AMF
  MikMod_RegisterLoader(&load_asy); // ASYLUM .AMF
  MikMod_RegisterLoader(&load_dsm);
  MikMod_RegisterLoader(&load_far);
  MikMod_RegisterLoader(&load_gdm);
  MikMod_RegisterLoader(&load_it);
  MikMod_RegisterLoader(&load_mod);
  MikMod_RegisterLoader(&load_med);
  MikMod_RegisterLoader(&load_mtm);
  MikMod_RegisterLoader(&load_okt);
  MikMod_RegisterLoader(&load_stm);
  MikMod_RegisterLoader(&load_s3m);
  MikMod_RegisterLoader(&load_ult);
  MikMod_RegisterLoader(&load_xm);
  MikMod_RegisterLoader(&load_m15); // Soundtracker .MOD

  // FIXME: Should break a lot more here
  if(MikMod_Init(NULL))
    warn("MikMod Init failed: %s\n", MikMod_strerror(MikMod_errno));

  audio_ext_register("669", construct_mikmod_stream);
  audio_ext_register("amf", construct_mikmod_stream);
  audio_ext_register("dsm", construct_mikmod_stream);
  audio_ext_register("far", construct_mikmod_stream);
  audio_ext_register("gdm", construct_mikmod_stream);
  audio_ext_register("it", construct_mikmod_stream);
  audio_ext_register("med", construct_mikmod_stream);
  audio_ext_register("mod", construct_mikmod_stream);
  audio_ext_register("mtm", construct_mikmod_stream);
  audio_ext_register("nst", construct_mikmod_stream);
  audio_ext_register("oct", construct_mikmod_stream);
  audio_ext_register("okt", construct_mikmod_stream);
  audio_ext_register("s3m", construct_mikmod_stream);
  audio_ext_register("stm", construct_mikmod_stream);
  audio_ext_register("ult", construct_mikmod_stream);
  audio_ext_register("wow", construct_mikmod_stream);
  audio_ext_register("xm", construct_mikmod_stream);
}
