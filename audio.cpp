/* $Id$
 * MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick
 * Copyright (C) 2004 madbrain
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// Code to handle module playing, sample playing, and PC speaker
// sfx emulation.

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "audio.h"
#include "modplug.h"
#include "libmodplug/stdafx.h"
#include "libmodplug/sndfile.h"
#include "SDL.h"
#include "sfx.h"
#include "data.h"
#include "configure.h"
#include "gdm2s3m.h"
#include "fsafeopen.h"
#include "delay.h"

struct _ModPlugFile
{
  CSoundFile mSoundFile;
};

audio_struct audio;

int error_mode;

// An update should happen every 2ms, so this many samples
const double pc_speaker_interval = 44100 / 500;
const int freq_conversion = 3579364;

void audio_callback(void *userdata, Uint8 *stream, int len)
{
  int i, i2, offset = 0;
  Sint16 *mix_src_ptr;
  Sint32 *mix_dest_ptr;
  Sint32 *buffer_src_ptr;
  Sint16 *buffer_dest_ptr;
  Sint32 cur_sample;
  int sample_duration, end_duration;
  int increment_value, increment_buffer;

  if(audio.mod_playing && music_on)
  {
    int read_len =
     ModPlug_Read(audio.current_mod, audio.mod_buffer, len);

    if(read_len < len)
    {
      // Reset position
      audio.current_mod->mSoundFile.SetCurrentPos(0);
      // Read anew remaining bytes
      ModPlug_Read(audio.current_mod, audio.mod_buffer + read_len,
       len - read_len);
    }
  }
  else
  {
    memset(audio.mod_buffer, 0, len);
  }

  mix_dest_ptr = audio.mix_buffer;
  mix_src_ptr = audio.mod_buffer;

  for(i = 0; i < len / 2; i++)
  {
    *mix_dest_ptr = *mix_src_ptr;
    mix_dest_ptr++;
    mix_src_ptr++;
  }

  // Mix PC speaker effect. First get whatever ran over from last time.

  mix_dest_ptr = audio.mix_buffer;

  sample_duration = audio.pc_speaker_last_duration;

  if(sample_duration >= len / 4)
  {
    end_duration = len / 4;
    audio.pc_speaker_last_duration = sample_duration - end_duration;
    sample_duration = end_duration;
  }

  if(audio.pc_speaker_last_playing)
  {
    increment_value =
     (int)((double)audio.pc_speaker_last_frequency / 44100.0 * 4294967296.0);
    increment_buffer = audio.pc_speaker_last_increment_buffer;

    for(i = 0; i < sample_duration; i++)
    {
      cur_sample = (Uint32)((increment_buffer & 0x80000000) >> 18) - 4096;
      *mix_dest_ptr += cur_sample;
      *(mix_dest_ptr + 1) += cur_sample;
      increment_buffer += increment_value;
      mix_dest_ptr += 2;
    }
    audio.pc_speaker_last_increment_buffer = increment_buffer;
  }
  else
  {
    mix_dest_ptr += (sample_duration * 2);
  }

  offset += sample_duration;

  if(offset < len / 4)
    audio.pc_speaker_last_playing = 0;

  while(offset < len / 4)
  {
    sound_system();
    sample_duration = (int)(pc_speaker_interval * audio.pc_speaker_note_duration);

    if(offset + sample_duration >= len / 4)
    {
      end_duration = (len / 4) - offset;
      audio.pc_speaker_last_duration = sample_duration - end_duration;
      audio.pc_speaker_last_playing = audio.pc_speaker_playing;
      audio.pc_speaker_last_frequency = audio.pc_speaker_frequency;

      sample_duration = end_duration;
    }

    offset += sample_duration;

    if(audio.pc_speaker_playing)
    {
      increment_value =
       (int)((double)audio.pc_speaker_frequency / 44100.0 * 4294967296.0);
      increment_buffer = 0;

      for(i = 0; i < sample_duration; i++)
      {
        cur_sample = (Uint32)((increment_buffer & 0x80000000) >> 18) - 4096;
        *mix_dest_ptr += cur_sample;
        *(mix_dest_ptr + 1) += cur_sample;
        increment_buffer += increment_value;
        mix_dest_ptr += 2;
      }
      audio.pc_speaker_last_increment_buffer = increment_buffer;
    }
    else
    {
      mix_dest_ptr += (sample_duration * 2);
    }
  }

  // Mix samples, if any are playing

  if(audio.num_samples_playing && music_on)
  {
    for(i = 0; i < 16; i++)
    {
      if(audio.samples_playing[i])
      {
        int read_len =
         ModPlug_Read(audio.samples_playing[i], audio.sample_buffers[i], len);

        mix_src_ptr = audio.sample_buffers[i];
        mix_dest_ptr = audio.mix_buffer;

        for(i2 = 0; i2 < read_len / 2; i2++)
        {
          *mix_dest_ptr += *mix_src_ptr;
          mix_src_ptr++;
          mix_dest_ptr++;
        }

        // If a full read wasn't done the sample's play is up, so destroy it
        if(read_len < len)
          end_individual_sample(i);
      }
    }
  }

  buffer_src_ptr = audio.mix_buffer;
  buffer_dest_ptr = (Sint16 *)stream;

  for(i = 0; i < len / 2; i++)
  {
    cur_sample = *buffer_src_ptr;
    if(cur_sample > 32767)
      cur_sample = 32767;

    if(cur_sample < -32768)
      cur_sample = -32768;

    *buffer_dest_ptr = cur_sample;
    buffer_src_ptr++;
    buffer_dest_ptr++;
  }
}

void init_audio(config_info *conf)
{
  int buffer_size = conf->buffer_size;

  SDL_AudioSpec desired_spec =
  {
    44100,
    AUDIO_S16SYS,
    2,
    0,
    buffer_size,
    0,
    0,
    audio_callback,
    NULL
  };

  if(conf->oversampling_on)
  {
    audio.mod_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
  }

  audio.mod_settings.mFrequency = 44100;
  audio.mod_settings.mChannels = 2;
  audio.mod_settings.mBits = 16;

  switch(conf->resampling_mode)
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

  SDL_OpenAudio(&desired_spec, &audio.audio_settings);
  audio.mod_buffer = (Sint16 *)malloc(audio.audio_settings.size);
  audio.mix_buffer = (Sint32 *)malloc(audio.audio_settings.size * 2);
  SDL_PauseAudio(0);
}

void load_mod(char *filename)
{
  FILE *input_file;
  char *input_buffer;
  int file_size;
  int extension_pos = strlen(filename) - 4;
  char new_file[MAX_PATH];

  if(extension_pos && !strcasecmp(filename + extension_pos, ".gdm"))
  {
    char translated_filename_src[MAX_PATH];
    char translated_filename_dest[MAX_PATH];

    // GDM -> S3M
    strcpy(new_file, filename);
    memcpy(new_file + extension_pos, ".s3m", 4);

    fsafetranslate(filename, translated_filename_src);

    if(fsafetranslate(new_file, translated_filename_dest) < 0)
    {
      // If it doesn't exist, create it by converting.
      convert_gdm_s3m(translated_filename_src, new_file);
    }

    filename = new_file;
  }

  if(audio.mod_playing)
    end_mod();

  input_file = fsafeopen(filename, "rb");

  if(input_file)
  {
    file_size = filelength(input_file);

    input_buffer = (char *)malloc(file_size);
    fread(input_buffer, file_size, 1, input_file);
    audio.current_mod = ModPlug_Load(input_buffer, file_size);

    if(audio.current_mod)
    {
      audio.mod_playing = 1;
    }

    free(input_buffer);
  }
}

void end_mod(void)
{
  if(audio.mod_playing)
  {
    ModPlug_Unload(audio.current_mod);
    audio.mod_playing = 0;
  }
}

void play_sample(int freq, char *filename)
{
  FILE *input_file;
  char *input_buffer;
  int file_size;
  int extension_pos = strlen(filename) - 4;
  char new_file[256];

  // FIXME - destroy least recently used?
  if(audio.num_samples_playing >= MAX_SAMS)
  {
    unsigned int smallest = audio.sam_timestamps[0];
    int smallest_i = 0;
    int i;
    for(i = 1; i < MAX_SAMS; i++)
    {
      if(audio.sam_timestamps[i] < smallest)
      {
        smallest = audio.sam_timestamps[i];
        smallest_i = i;
      }
    }

    end_individual_sample(smallest_i);
  }

  if(extension_pos && !strcasecmp(filename + extension_pos, ".sam"))
  {
    char translated_filename_src[MAX_PATH];
    char translated_filename_dest[MAX_PATH];

    // GDM -> S3M
    strcpy(new_file, filename);
    memcpy(new_file + extension_pos, ".wav", 4);

    fsafetranslate(filename, translated_filename_src);

    if(fsafetranslate(new_file, translated_filename_dest) < 0)
    {
      // If it doesn't exist, create it by converting.
      convert_sam_to_wav(translated_filename_src, new_file);
    }

    filename = new_file;
  }

  input_file = fsafeopen(filename, "rb");

  if(input_file)
  {
    int i;
    ModPlugFile *sample_loaded;

    file_size = filelength(input_file);

    input_buffer = (char *)malloc(file_size);
    fread(input_buffer, file_size, 1, input_file);
    sample_loaded = ModPlug_Load(input_buffer, file_size);

    if(sample_loaded)
    {
      // A little hack to modify the pitch
      sample_loaded->mSoundFile.Ins[1].nC4Speed = (freq_conversion / freq) / 2;
      sample_loaded->mSoundFile.Ins[2].nC4Speed = (freq_conversion / freq) / 2;
      sample_loaded->mSoundFile.Ins[1].nVolume = (256 * sound_gvol) / 8;
      sample_loaded->mSoundFile.Ins[2].nVolume = (256 * sound_gvol) / 8;

      // Find a free position to put it
      for(i = 0; i < 16; i++)
      {
        if(audio.samples_playing[i] == NULL)
          break;
      }

      audio.samples_playing[i] = sample_loaded;
      audio.sample_buffers[i] =
       (Sint16 *)malloc(audio.audio_settings.size);
      audio.num_samples_playing++;
      audio.sam_timestamps[i] = get_ticks();
    }

    free(input_buffer);
    fclose(input_file);
  }
}

void end_individual_sample(int sam_num)
{
  ModPlug_Unload(audio.samples_playing[sam_num]);
  free(audio.sample_buffers[sam_num]);
  audio.num_samples_playing--;
  audio.samples_playing[sam_num] = NULL;
}

void end_sample(void)
{
  int i;
  for(i = 0; i < 16; i++)
  {
    if(audio.samples_playing[i])
      end_individual_sample(i);
  }
}

void jump_mod(int order)
{
  if(audio.mod_playing)
    audio.current_mod->mSoundFile.SetCurrentOrder(order);
}

int get_order()
{
  if(audio.mod_playing)
    return audio.current_mod->mSoundFile.GetCurrentOrder();
  else
    return 0;
}


// FIXME - This may have to be adjusted slightly. It SEEMS that
// playing the module at a volume of 1-128 is suitable.

void volume_mod(int vol)
{
  if(audio.mod_playing)
    audio.current_mod->mSoundFile.SetMasterVolume((vol * music_gvol / 16) + 1);
}

// FIXME - Implement? The best route right now would be to load a module
// then mangle it so it only plays the given sample at the given frequency.

void spot_sample(int freq, int sample)
{

}

void sound(int frequency, int duration)
{
  audio.pc_speaker_playing = 1;
  audio.pc_speaker_frequency = frequency;
  audio.pc_speaker_note_duration = duration;
}

void nosound(int duration)
{
  audio.pc_speaker_playing = 0;
  audio.pc_speaker_note_duration = duration;
}


int filelength(FILE *fp)
{
  int length;

  fseek(fp, 0, SEEK_END);
  length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  return length;
}

const int default_period = 428;

// Based off of original source by madbrain

void convert_sam_to_wav(char *source_name, char *dest_name)
{
  FILE *source, *dest;
  int frequency;
  int source_length, dest_length;

  char *data;
  int i;

  source = fopen(source_name, "rb");

  if(source == NULL)
    return;

  dest = fopen(dest_name, "wb");

  source_length = filelength(source);

  frequency = freq_conversion / default_period;
  data = (char *)malloc(source_length + 44);
  dest_length = source_length - 8;

  data[0] = 'R';
  data[1] = 'I';
  data[2] = 'F';
  data[3] = 'F';
  data[4] = dest_length & 0xFF;
  data[5] = (dest_length >> 8) & 0xFF;
  data[6] = (dest_length >> 16) & 0xFF;
  data[7] = (dest_length >> 24) & 0xFF;
  data[8] = 'W';
  data[9] = 'A';
  data[10] = 'V';
  data[11] = 'E';

  data[12] = 'f';
  data[13] = 'm';
  data[14] = 't';
  data[15] = ' ';
  data[16] = 16;
  data[17] = 0;
  data[18] = 0;
  data[19] = 0;
  data[20] = 1;     // pcm
  data[21] = 0;
  data[22] = 2;     // mono
  data[23] = 0;

  // frequency, bytes/second
  data[24] = frequency & 0xFF;
  data[25] = (frequency >> 8) & 0xFF;
  data[26] = (frequency >> 16) & 0xFF;
  data[27] = (frequency >> 24) & 0xFF;
  data[28] = frequency & 0xFF;
  data[29] = (frequency >> 8) & 0xFF;
  data[30] = (frequency >> 16) & 0xFF;
  data[31] = (frequency >> 24) & 0xFF;

  data[32] = 1;     // total bytes/sample
  data[33] = 0;
  data[34] = 8;     // bit depth
  data[35] = 0;
  data[36] = 'd';
  data[37] = 'a';
  data[38] = 't';
  data[39] = 'a';

  // Sample length
  data[40] = source_length & 0xFF;
  data[41] = (source_length >> 8) & 0xFF;
  data[42] = (source_length >> 16) & 0xFF;
  data[43] = (source_length >> 24) & 0xFF;

  fread(data + 44, source_length, 1, source);

  for(i = 44; i <dest_length; i++)
    data[i] += 128;

  fwrite(data, dest_length, 1, dest);

  free(data);
  fclose(source);
  fclose(dest);
}
