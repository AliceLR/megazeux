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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// Definitions for audio.cpp

#ifndef AUDIO_H
#define AUDIO_H

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include "SDL.h"
#include "modplug.h"
#include "configure.h"

#define FP_SHIFT 13
#define FP_AND ((1 << FP_SHIFT) - 1)

#define FP_MULT(a, b) ((a * b) << FP_SHIFT)

typedef struct _audio_stream audio_stream;

struct _audio_stream
{
  struct _audio_stream *next;
  struct _audio_stream *previous;
  Uint32 volume;
  Uint32 repeat;
  Uint32 (* mix_data)(audio_stream *a_src, Sint32 *buffer,
   Uint32 len);
  void (* set_volume)(audio_stream *a_src, Uint32 volume);
  void (* set_repeat)(audio_stream *a_src, Uint32 repeat);
  void (* set_order)(audio_stream *a_src, Uint32 order);
  void (* set_position)(audio_stream *a_src, Uint32 pos);
  Uint32 (* get_order)(audio_stream *a_src);
  Uint32 (* get_position)(audio_stream *a_src);
  void (* destruct)(audio_stream *a_src);
};

typedef struct _sampled_stream sampled_stream;

struct _sampled_stream
{
  audio_stream a;
  Uint32 frequency;
  Sint16 *output_data;
  Uint32 data_window_length;
  Uint32 allocated_data_length;
  Uint32 prologue_length;
  Uint32 epilogue_length;
  Uint32 stream_offset;
  Uint32 channels;
  Uint32 negative_comp;
  Uint32 use_volume;
  Sint64 frequency_delta;
  Sint64 sample_index;
  void (* set_frequency)(sampled_stream *s_src, Uint32 frequency);
  Uint32 (* get_frequency)(sampled_stream *s_src);
};

typedef struct
{
  sampled_stream s;
  ModPlugFile *module_data;
  Uint32 effective_frequency;
} modplug_stream;

typedef struct
{
  sampled_stream s;
  Uint8 *wav_data;
  Uint32 data_offset;
  Uint32 data_length;
  Uint32 channels;
  Uint32 bytes_per_sample;
  Uint32 natural_frequency;
  Uint16 format;
} wav_stream;

typedef struct
{
  sampled_stream s;
  OggVorbis_File vorbis_file_handle;
  vorbis_info *vorbis_file_info;
} vorbis_stream;

typedef struct
{
  audio_stream a;
  Uint32 volume;
  Uint32 playing;
  Uint32 frequency;
  Sint32 last_frequency;
  Uint32 note_duration;
  Uint32 last_duration;
  Uint32 last_playing;
  Uint32 sample_cutoff;
  Uint32 last_increment_buffer;
} pc_speaker_stream;

typedef struct
{
  SDL_AudioSpec audio_settings;
  ModPlug_Settings mod_settings;
  Sint32 *mix_buffer;

  Uint32 output_frequency;
  Uint32 master_resample_mode;

  audio_stream *primary_stream;
  pc_speaker_stream *pcs_stream;
  audio_stream *stream_list_base;
  audio_stream *stream_list_end;

  SDL_mutex *audio_mutex;

  Uint32 music_on;
  Uint32 sfx_on;
  Uint32 music_volume;
  Uint32 sound_volume;
  Uint32 sfx_volume;
} audio_struct;

void init_audio(config_info *conf);
void load_mod(char *filename);
void end_mod(void);
void play_sample(int freq, char *filename);
void end_sample(void);
void jump_mod(int order);
int get_order();
void volume_mod(int vol);
void mod_exit(void);
void mod_init(void);
void spot_sample(int freq, int sample);
void shift_frequency(int freq);
int get_frequency();
void set_position(int pos);
int get_position();
int free_sam_cache(char clear_all);
void fix_global_volumes(void);
void sound(int frequency, int duration);
void nosound(int duration);
int filelength(FILE *fp);
void convert_sam_to_wav(char *source_name, char *dest_name);
void set_music_on(int val);
void set_sfx_on(int val);
int get_music_on_state();
int get_sfx_on_state();
int get_music_volume();
int get_sound_volume();
int get_sfx_volume();
void set_music_volume(int volume);
void set_sound_volume(int volume);
void set_sfx_volume(int volume);

extern int error_mode;

#endif

