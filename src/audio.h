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

#include "SDL.h"
#include "modplug.h"
#include "configure.h"

#define MAX_SAMS 16

typedef struct
{
  SDL_AudioSpec audio_settings;
  ModPlugFile *current_mod;
  ModPlug_Settings mod_settings;
  int mod_playing;
  int num_samples_playing;
  ModPlugFile *samples_playing[MAX_SAMS];
  Sint32 *mix_buffer;
  Sint16 *mod_buffer;
  Sint16 *sample_buffers[MAX_SAMS];
  Uint32 sam_timestamps[MAX_SAMS];
  Uint32 repeat_timeout;
  Uint32 repeat_timestamp;
  Uint32 pc_speaker_on;
  Uint32 pc_speaker_playing;
  Uint32 pc_speaker_frequency;
  Sint32 pc_speaker_last_frequency;
  Uint32 pc_speaker_note_duration;
  Uint32 pc_speaker_last_duration;
  Uint32 pc_speaker_last_playing;
  Uint32 pc_speaker_sample_cutoff;
  Uint32 pc_speaker_last_increment_buffer;

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
void end_individual_sample(int sam_num);
void end_sample(void);
void jump_mod(int order);
int get_order();
void volume_mod(int vol);
void mod_exit(void);
void mod_init(void);
void spot_sample(int freq, int sample);
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
