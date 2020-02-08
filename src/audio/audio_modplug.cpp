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

#include "modplug.h"
#include "stdafx.h"
#include "sndfile.h"

#include "audio.h"
#include "audio_modplug.h"
#include "ext.h"
#include "gdm2s3m.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../fsafeopen.h"
#include "../util.h"

struct _ModPlugFile
{
  CSoundFile mSoundFile;
};

struct modplug_stream
{
  struct sampled_stream s;
  ModPlugFile *module_data;
  Uint32 effective_frequency;
};

static Uint32 mp_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  Uint32 read_len;
  struct modplug_stream *mp_stream = (struct modplug_stream *)a_src;
  Uint32 read_wanted = mp_stream->s.allocated_data_length -
   mp_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)mp_stream->s.output_data +
   mp_stream->s.stream_offset;
  Uint32 r_val = 0;

  read_len =
   ModPlug_Read(mp_stream->module_data, read_buffer, read_wanted);

  if(!a_src->volume)
  {
    // Modplug doesn't support a volume of zero; the lowest volume usable
    // via SetMasterVolume is 1. If the volume is 0, zero the buffer we
    // just read. We read the buffer to ensure the mod still progresses
    // in the background, maintaining the old semantics.
    memset(read_buffer, 0, read_len);
  }

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
      // FIXME: I think this memset should always be done?
      memset(read_buffer + read_len, 0, read_wanted - read_len);
      r_val = 1;
    }

    read_len = 0;
  }

  sampled_mix_data((struct sampled_stream *)mp_stream, buffer, len);

  return r_val;
}

static void mp_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  ModPlugFile *mp_file = ((struct modplug_stream *)a_src)->module_data;

  a_src->volume = volume;
  mp_file->mSoundFile.SetMasterVolume(volume);
}

static void mp_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  ModPlugFile *mp_file = ((struct modplug_stream *)a_src)->module_data;

  a_src->repeat = repeat;

  if(repeat)
    mp_file->mSoundFile.SetRepeatCount(-1);
  else
    mp_file->mSoundFile.SetRepeatCount(0);
}

static void mp_set_order(struct audio_stream *a_src, Uint32 order)
{
  ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentOrder(order);
}

static void mp_set_position(struct audio_stream *a_src, Uint32 position)
{
  ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentPos(position);
}

static void mp_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
  {
    ((struct modplug_stream *)s_src)->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    ((struct modplug_stream *)s_src)->effective_frequency = frequency;
    frequency = (Uint32)((float)frequency *
     audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 mp_get_order(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentOrder();
}

static Uint32 mp_get_position(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentPos();
}

static Uint32 mp_get_length(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetMaxPosition();
}

static Uint32 mp_get_frequency(struct sampled_stream *s_src)
{
  return ((struct modplug_stream *)s_src)->effective_frequency;
}

static void mp_destruct(struct audio_stream *a_src)
{
  struct modplug_stream *mp_stream = (struct modplug_stream *)a_src;
  ModPlug_Unload(mp_stream->module_data);
  sampled_destruct(a_src);
}

static struct audio_stream *construct_modplug_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  ssize_t ext_pos = (ssize_t)strlen(filename) - 4;
  struct audio_stream *ret_val = NULL;
  char *input_buffer;
  FILE *input_file;

  input_file = fopen_unsafe(filename, "rb");

  if(input_file)
  {
    Uint32 file_size = ftell_and_rewind(input_file);
    ModPlugFile *open_file;

    input_buffer = (char *)cmalloc(file_size);
    fread(input_buffer, file_size, 1, input_file);
    open_file = ModPlug_Load(input_buffer, file_size);

    if(open_file)
    {
      struct modplug_stream *mp_stream =
       (struct modplug_stream *)cmalloc(sizeof(struct modplug_stream));
      struct sampled_stream_spec s_spec;
      struct audio_stream_spec a_spec;

      mp_stream->module_data = open_file;

      if((ext_pos > 0) &&
       !strcasecmp(filename + ext_pos, ".wav"))
      {
        open_file->mSoundFile.Ins[1].nC4Speed = frequency;
        open_file->mSoundFile.Ins[2].nC4Speed = frequency;
        frequency = 0;
      }

      memset(&a_spec, 0, sizeof(struct audio_stream_spec));
      a_spec.mix_data     = mp_mix_data;
      a_spec.set_volume   = mp_set_volume;
      a_spec.set_repeat   = mp_set_repeat;
      a_spec.set_order    = mp_set_order;
      a_spec.set_position = mp_set_position;
      a_spec.get_order    = mp_get_order;
      a_spec.get_position = mp_get_position;
      a_spec.get_length   = mp_get_length;
      a_spec.destruct     = mp_destruct;

      memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
      s_spec.set_frequency = mp_set_frequency;
      s_spec.get_frequency = mp_get_frequency;

      initialize_sampled_stream((struct sampled_stream *)mp_stream, &s_spec,
       frequency, 2, 0);

      initialize_audio_stream((struct audio_stream *)mp_stream, &a_spec,
       volume, repeat);

      ret_val = (struct audio_stream *)mp_stream;
    }

    free(input_buffer);
    fclose(input_file);
  }

  return ret_val;
}

static struct audio_stream *modplug_convert_gdm(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  /* Wrapper for construct_modplug_stream to convert .GDMs to .S3Ms. */
  char translated_filename_dest[MAX_PATH];
  char new_file[MAX_PATH];
  int have_s3m = 0;

  /* We know this file has a .gdm extension. */
  ssize_t ext_pos = (ssize_t)strlen(filename) - 4;

  /* Get the name of its .s3m counterpart */
  memcpy(new_file, filename, ext_pos);
  memcpy(new_file + ext_pos, ".s3m", 4);

  /* If the destination S3M already exists, check its size. If it's
   * non-zero in size, we can load it.
   */
  if(!fsafetranslate(new_file, translated_filename_dest))
  {
    FILE *f = fopen_unsafe(translated_filename_dest, "r");
    long file_len = ftell_and_rewind(f);

    fclose(f);
    if(file_len > 0)
      have_s3m = 1;
  }

  /* In the case we need to convert the GDM, we need to find
   * the real source path for it. Translate accordingly.
   */
  if(!have_s3m && !fsafetranslate(filename, translated_filename_dest))
  {
    if(!convert_gdm_s3m(translated_filename_dest, new_file))
      have_s3m = 1;
  }

  /* If we have an S3M, we can now load it. Otherwise, abort.*/
  if(have_s3m)
    return construct_modplug_stream(new_file, frequency, volume, repeat);

  return NULL;
}

void init_modplug(struct config_info *conf)
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

  audio_ext_register("669", construct_modplug_stream);
  audio_ext_register("amf", construct_modplug_stream);
  audio_ext_register("dsm", construct_modplug_stream);
  audio_ext_register("far", construct_modplug_stream);
  audio_ext_register("gdm", modplug_convert_gdm);
  audio_ext_register("it", construct_modplug_stream);
  audio_ext_register("med", construct_modplug_stream);
  audio_ext_register("mod", construct_modplug_stream);
  audio_ext_register("mtm", construct_modplug_stream);
  audio_ext_register("okt", construct_modplug_stream);
  audio_ext_register("s3m", construct_modplug_stream);
  audio_ext_register("stm", construct_modplug_stream);
  audio_ext_register("ult", construct_modplug_stream);
  audio_ext_register("xm", construct_modplug_stream);
}
