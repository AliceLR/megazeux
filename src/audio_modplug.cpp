/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

// Provides a ModPlug module stream backend

#include "audio.h"
#include "audio_modplug.h"
#include "fsafeopen.h"
#include "const.h"
#include "util.h"

#include "modplug.h"
#include "stdafx.h"
#include "sndfile.h"

struct _ModPlugFile
{
  CSoundFile mSoundFile;
};

typedef struct
{
  sampled_stream s;
  ModPlugFile *module_data;
  Uint32 effective_frequency;
} modplug_stream;

static Uint32 mp_mix_data(audio_stream *a_src, Sint32 *buffer, Uint32 len)
{
  Uint32 read_len;
  modplug_stream *mp_stream = (modplug_stream *)a_src;
  Uint32 read_wanted = mp_stream->s.allocated_data_length -
   mp_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)mp_stream->s.output_data +
   mp_stream->s.stream_offset;
  Uint32 r_val = 0;

  read_len =
   ModPlug_Read(mp_stream->module_data, read_buffer, read_wanted);

  if(read_len < read_wanted)
  {
    // Modplug still fails to repeat on some mods, even after already
    // restarting it still ends the thing (even fades it). This won't
    // eliminate the fade but at least it'll stop the mod from
    // ending..

    if(a_src->repeat)
    {
      a_src->set_position(a_src, a_src->get_position(a_src));
    }
    else
    {
      memset(read_buffer + read_len, 0, read_wanted - read_len);
      r_val = 1;
    }

    read_len = 0;
  }

  sampled_mix_data((sampled_stream *)mp_stream, buffer, len);

  return r_val;
}

static void mp_set_volume(audio_stream *a_src, Uint32 volume)
{
  ModPlugFile *mp_file = ((modplug_stream *)a_src)->module_data;

  a_src->volume = volume;
  mp_file->mSoundFile.SetMasterVolume(volume);
}

static void mp_set_repeat(audio_stream *a_src, Uint32 repeat)
{
  ModPlugFile *mp_file = ((modplug_stream *)a_src)->module_data;

  a_src->repeat = repeat;

  if(repeat)
    mp_file->mSoundFile.SetRepeatCount(-1);
  else
    mp_file->mSoundFile.SetRepeatCount(0);
}

static void mp_set_order(audio_stream *a_src, Uint32 order)
{
  ((modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentOrder(order);
}

static void mp_set_position(audio_stream *a_src, Uint32 position)
{
  ((modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentPos(position);
}

static void mp_set_frequency(sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
  {
    ((modplug_stream *)s_src)->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    ((modplug_stream *)s_src)->effective_frequency = frequency;
    frequency = (Uint32)((float)frequency *
     audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 mp_get_order(audio_stream *a_src)
{
  return ((modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentOrder();
}

static Uint32 mp_get_position(audio_stream *a_src)
{
  return ((modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentPos();
}

static Uint32 mp_get_frequency(sampled_stream *s_src)
{
  return ((modplug_stream *)s_src)->effective_frequency;
}

static void mp_destruct(audio_stream *a_src)
{
  modplug_stream *mp_stream = (modplug_stream *)a_src;
  ModPlug_Unload(mp_stream->module_data);
  sampled_destruct(a_src);
}

audio_stream *construct_modplug_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat)
{
  ssize_t ext_pos = strlen(filename) - 4;
  audio_stream *ret_val = NULL;
  char new_file[MAX_PATH];
  char *input_buffer;
  FILE *input_file;

  if(!check_ext_for_gdm_and_convert(filename, new_file))
  {
    if(!check_ext_for_sam_and_convert(filename, new_file))
    {
      strncpy(new_file, filename, MAX_PATH - 1);
      new_file[MAX_PATH - 1] = '\0';
    }
  }

  input_file = fsafeopen(new_file, "rb");

  if(input_file)
  {
    Uint32 file_size = ftell_and_rewind(input_file);
    ModPlugFile *open_file;

    input_buffer = (char *)malloc(file_size);
    fread(input_buffer, file_size, 1, input_file);
    open_file = ModPlug_Load(input_buffer, file_size);

    if(open_file)
    {
      modplug_stream *mp_stream =
       (modplug_stream *)malloc(sizeof(modplug_stream));

      mp_stream->module_data = open_file;

      if((ext_pos > 0) &&
       !strcasecmp(filename + ext_pos, ".wav"))
      {
        open_file->mSoundFile.Ins[1].nC4Speed = frequency;
        open_file->mSoundFile.Ins[2].nC4Speed = frequency;
        frequency = 0;
      }

      initialize_sampled_stream((sampled_stream *)mp_stream,
       mp_set_frequency, mp_get_frequency, frequency, 2, 0);

      ret_val = (audio_stream *)mp_stream;

      construct_audio_stream((audio_stream *)mp_stream, mp_mix_data,
       mp_set_volume, mp_set_repeat, mp_set_order, mp_set_position,
       mp_get_order, mp_get_position, mp_destruct, volume, repeat);
    }

    free(input_buffer);
    fclose(input_file);
  }

  return ret_val;
}

void init_modplug(config_info *conf)
{
  if(conf->oversampling_on)
    audio.mod_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;

  audio.mod_settings.mFrequency = audio.output_frequency;
  audio.mod_settings.mChannels = 2;
  audio.mod_settings.mBits = 16;

  switch(conf->modplug_resample_mode)
  {
    case 0:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_NEAREST;
      break;
    }

    case 1:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
      break;
    }

    case 2:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
      break;
    }

    case 3:
    {
      audio.mod_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
      break;
    }
  }

  audio.mod_settings.mLoopCount = -1;

  ModPlug_SetSettings(&audio.mod_settings);
}
