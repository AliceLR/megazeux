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
#include "const.h"
#include "util.h"

// TODO: deSDL MikMod plugin
#include "SDL.h"
#include <mikmod.h>

struct mikmod_stream
{
  struct sampled_stream s;
  MODULE *module_data;
  Uint32 effective_frequency;
};

struct LMM_MREADER
{
  MREADER mr;
  int offset;
  int eof;
  SDL_RWops *rw;
};

static BOOL LMM_Seek(struct MREADER *mr, long to, int dir)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  if(dir == SEEK_SET)
    to += lmmmr->offset;
  return SDL_RWseek(lmmmr->rw, to, dir) < lmmmr->offset;
}

static long LMM_Tell(struct MREADER *mr)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  return SDL_RWtell(lmmmr->rw) - lmmmr->offset;
}

static BOOL LMM_Read(struct MREADER *mr, void *buf, size_t sz)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  return SDL_RWread(lmmmr->rw, buf, sz, 1);
}

static int LMM_Get(struct MREADER *mr)
{
  unsigned char c;
  int i = EOF;
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER *)mr;
  if(SDL_RWread(lmmmr->rw,&c,1,1))
    i = c;
  return i;
}

static BOOL LMM_Eof(struct MREADER *mr)
{
  struct LMM_MREADER *lmmmr = (struct LMM_MREADER*)mr;
  return LMM_Tell(mr) >= lmmmr->eof;
}

static MODULE *MikMod_LoadSongRW(SDL_RWops *rw, int maxchan)
{
  struct LMM_MREADER lmmmr={
    { LMM_Seek, LMM_Tell, LMM_Read, LMM_Get, LMM_Eof },
    0, 0, rw
  };

  lmmmr.offset = SDL_RWtell(rw);
  SDL_RWseek(rw, 0, SEEK_END);
  lmmmr.eof = SDL_RWtell(rw);
  SDL_RWseek(rw, lmmmr.offset, SEEK_SET);

  return Player_LoadGeneric((MREADER *)&lmmmr, maxchan, 0);
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

// FIXME: position is unsupported by mikmod
static void mm_set_position(struct audio_stream *a_src, Uint32 position)
{
  mm_set_order(a_src, position);
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
  return ((struct mikmod_stream *)a_src)->module_data->sngpos;
}

// FIXME: position is unsupported by mikmod
static Uint32 mm_get_position(struct audio_stream *a_src)
{
  return mm_get_order(a_src);
}

static Uint32 mm_get_frequency(struct sampled_stream *s_src)
{
  return ((struct mikmod_stream *)s_src)->effective_frequency;
}

static void mm_destruct(struct audio_stream *a_src)
{
  Player_Stop();
  Player_Free(((struct mikmod_stream *)a_src)->module_data);
  sampled_destruct(a_src);
}

struct audio_stream *construct_mikmod_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat)
{
  FILE *input_file;
  char *input_buffer;
  Uint32 file_size;
  struct audio_stream *ret_val = NULL;

  input_file = fopen_unsafe(filename, "rb");

  if(input_file)
  {
    MODULE *open_file;

    file_size = ftell_and_rewind(input_file);

    input_buffer = cmalloc(file_size);
    fread(input_buffer, file_size, 1, input_file);
    open_file = MikMod_LoadSongRW(SDL_RWFromMem(input_buffer, file_size), 64);

    if(open_file)
    {
      struct mikmod_stream *mm_stream = cmalloc(sizeof(struct mikmod_stream));
      mm_stream->module_data = open_file;
      Player_Start(mm_stream->module_data);

      initialize_sampled_stream((struct sampled_stream *)mm_stream,
       mm_set_frequency, mm_get_frequency, frequency, 2, 0);

      ret_val = (struct audio_stream *)mm_stream;

      construct_audio_stream((struct audio_stream *)mm_stream,
       mm_mix_data, mm_set_volume, mm_set_repeat, mm_set_order,
       mm_set_position, mm_get_order, mm_get_position, mm_destruct,
       volume, repeat);
    }

    fclose(input_file);
    free(input_buffer);
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
}
