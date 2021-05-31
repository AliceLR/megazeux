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

#include <modplug.h>
#include <stdafx.h>
#include <sndfile.h>
#include <gdm2s3m.h>

#include "audio.h"
#include "audio_modplug.h"
#include "audio_struct.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../const.h"
#include "../configure.h"
#include "../util.h"
#include "../io/fsafeopen.h"
#include "../io/vio.h"

struct _ModPlugFile
{
  CSoundFile mSoundFile;
};

struct modplug_stream
{
  struct sampled_stream s;
  ModPlugFile *module_data;
  size_t effective_frequency;
};

static void init_modplug_settings(void)
{
  struct config_info *conf = get_config();
  ModPlug_Settings mod_settings;

  memset(&mod_settings, 0, sizeof(ModPlug_Settings));

  if(conf->oversampling_on)
    mod_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;

  mod_settings.mFrequency = audio.output_frequency;
  mod_settings.mChannels = 2;
  mod_settings.mBits = 16;

  switch(conf->module_resample_mode)
  {
    case RESAMPLE_MODE_NONE:
    {
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_NEAREST;
      break;
    }

    case RESAMPLE_MODE_LINEAR:
    {
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
      break;
    }

    case RESAMPLE_MODE_CUBIC:
    {
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
      break;
    }

    case RESAMPLE_MODE_FIR:
    {
      mod_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
      break;
    }
  }

  mod_settings.mLoopCount = -1;

  ModPlug_SetSettings(&mod_settings);
}

static boolean mp_mix_data(struct audio_stream *a_src, int32_t * RESTRICT buffer,
 size_t len)
{
  size_t read_len;
  struct modplug_stream *mp_stream = (struct modplug_stream *)a_src;
  size_t read_wanted = mp_stream->s.allocated_data_length -
   mp_stream->s.stream_offset;
  uint8_t *read_buffer = (uint8_t *)mp_stream->s.output_data +
   mp_stream->s.stream_offset;
  boolean r_val = false;

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
      r_val = true;
    }

    read_len = 0;
  }

  sampled_mix_data((struct sampled_stream *)mp_stream, buffer, len);

  return r_val;
}

static void mp_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  ModPlugFile *mp_file = ((struct modplug_stream *)a_src)->module_data;

  a_src->volume = volume;
  mp_file->mSoundFile.SetMasterVolume(volume);
}

static void mp_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  ModPlugFile *mp_file = ((struct modplug_stream *)a_src)->module_data;

  a_src->repeat = repeat;

  if(repeat)
    mp_file->mSoundFile.SetRepeatCount(-1);
  else
    mp_file->mSoundFile.SetRepeatCount(0);
}

static void mp_set_order(struct audio_stream *a_src, uint32_t order)
{
  ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentOrder(order);
}

static void mp_set_position(struct audio_stream *a_src, uint32_t position)
{
  ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.SetCurrentPos(position);
}

static void mp_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  if(frequency == 0)
  {
    ((struct modplug_stream *)s_src)->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    ((struct modplug_stream *)s_src)->effective_frequency = frequency;
    frequency = (uint32_t)((float)frequency *
     audio.output_frequency / 44100);
  }

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static uint32_t mp_get_order(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentOrder();
}

static uint32_t mp_get_position(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetCurrentPos();
}

static uint32_t mp_get_length(struct audio_stream *a_src)
{
  return ((struct modplug_stream *)a_src)->module_data->
   mSoundFile.GetMaxPosition();
}

static uint32_t mp_get_frequency(struct sampled_stream *s_src)
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
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  ssize_t ext_pos = (ssize_t)strlen(filename) - 4;
  struct audio_stream *ret_val = NULL;
  char *input_buffer;
  vfile *input_file;

  input_file = vfopen_unsafe(filename, "rb");

  if(input_file)
  {
    size_t file_size = vfilelength(input_file, false);
    ModPlugFile *open_file;

    init_modplug_settings();

    input_buffer = (char *)cmalloc(file_size);
    vfread(input_buffer, file_size, 1, input_file);
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
       frequency, 2, false);

      initialize_audio_stream((struct audio_stream *)mp_stream, &a_spec,
       volume, repeat);

      ret_val = (struct audio_stream *)mp_stream;
    }

    free(input_buffer);
    vfclose(input_file);
  }

  return ret_val;
}

static struct audio_stream *modplug_convert_gdm(char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  /* Wrapper for construct_modplug_stream to convert .GDMs to .S3Ms. */
  char translated_filename_dest[MAX_PATH];
  char new_file[MAX_PATH];
  int have_s3m = 0;

  /* We know this file has a .gdm extension. */
  int ext_pos = (int)strlen(filename) - 4;

  // FIXME this is garbage and doesn't work. Add a Modplug GDM loader instead.
  // fsafetranslate here is being misused to find a case-insensitive copy of
  // the requested filename, which instantly breaks when the file is requested
  // through the editor listening mod. Furthermore, this pollutes the current
  // directory and was never a good idea in the first place.
  return NULL;

  /* Get the name of its .s3m counterpart */
  snprintf(new_file, MAX_PATH, "%.*s.s3m", ext_pos, filename);
  new_file[MAX_PATH - 1] = '\0';

  /* If the destination S3M already exists, check its size. If it's
   * non-zero in size, we can load it.
   */
  if(!fsafetranslate(new_file, translated_filename_dest, MAX_PATH))
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
  if(!have_s3m && !fsafetranslate(filename, translated_filename_dest, MAX_PATH))
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
  audio_ext_register("669", construct_modplug_stream);
  audio_ext_register("amf", construct_modplug_stream);
  audio_ext_register("dsm", construct_modplug_stream);
  audio_ext_register("far", construct_modplug_stream);
  audio_ext_register("gdm", modplug_convert_gdm);
  audio_ext_register("it", construct_modplug_stream);
  audio_ext_register("med", construct_modplug_stream);
  audio_ext_register("mod", construct_modplug_stream);
  audio_ext_register("mtm", construct_modplug_stream);
  audio_ext_register("nst", construct_modplug_stream);
  audio_ext_register("oct", construct_modplug_stream);
  audio_ext_register("okt", construct_modplug_stream);
  audio_ext_register("s3m", construct_modplug_stream);
  audio_ext_register("stm", construct_modplug_stream);
  audio_ext_register("ult", construct_modplug_stream);
  audio_ext_register("wow", construct_modplug_stream);
  audio_ext_register("xm", construct_modplug_stream);
}
