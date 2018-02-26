/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

// Definitions for audio.cpp

#ifndef __AUDIO_H
#define __AUDIO_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform.h"
#include "configure.h"

#ifdef CONFIG_AUDIO

#ifdef CONFIG_MODPLUG
#include "modplug.h"
#endif

struct audio_stream
{
  struct audio_stream *next;
  struct audio_stream *previous;
  Uint32 volume;
  Uint32 repeat;
  Uint32 (* mix_data)(struct audio_stream *a_src, Sint32 *buffer,
   Uint32 len);
  void (* set_volume)(struct audio_stream *a_src, Uint32 volume);
  void (* set_repeat)(struct audio_stream *a_src, Uint32 repeat);
  void (* set_order)(struct audio_stream *a_src, Uint32 order);
  void (* set_position)(struct audio_stream *a_src, Uint32 pos);
  Uint32 (* get_order)(struct audio_stream *a_src);
  Uint32 (* get_position)(struct audio_stream *a_src);
  Uint32 (* get_length)(struct audio_stream *a_src);
  void (* destruct)(struct audio_stream *a_src);
};

struct audio
{
#ifdef CONFIG_MODPLUG
  // for config.txt settings only
  ModPlug_Settings mod_settings;
#endif

  Sint32 *mix_buffer;

  Uint32 buffer_samples;

  Uint32 output_frequency;
  Uint32 master_resample_mode;
  Sint32 max_simultaneous_samples;
  Sint32 max_simultaneous_samples_config;

  struct audio_stream *primary_stream;
  struct audio_stream *pcs_stream;
  struct audio_stream *stream_list_base;
  struct audio_stream *stream_list_end;

  platform_mutex audio_mutex;

  Uint32 music_on;
  Uint32 sfx_on;
  Uint32 music_volume;
  Uint32 sound_volume;
  Uint32 sfx_volume;
};

extern struct audio audio;

CORE_LIBSPEC void set_max_samples(int max_samples);
CORE_LIBSPEC int get_max_samples(void);

CORE_LIBSPEC void init_audio(struct config_info *conf);
CORE_LIBSPEC void quit_audio(void);
CORE_LIBSPEC int load_module(char *filename, bool safely, int volume);
CORE_LIBSPEC void end_module(void);
CORE_LIBSPEC void play_sample(int period, char *filename, bool safely);
CORE_LIBSPEC void volume_module(int vol);

void end_sample(void);
void jump_module(int order);
int get_order(void);
void module_exit(void);
void module_init(void);
void shift_frequency(int freq);
int get_frequency(void);
void set_position(int pos);
int get_position(void);
int audio_get_length(void);
int free_sam_cache(char clear_all);
void fix_global_volumes(void);
void set_music_on(int val);
void set_sfx_on(int val);
int get_music_on_state(void);
int get_sfx_on_state(void);
int get_music_volume(void);
int get_sound_volume(void);
int get_sfx_volume(void);
void set_music_volume(int volume);
void set_sound_volume(int volume);
void set_sfx_volume(int volume);

// Internal functions
void init_wav(struct config_info *conf);
void init_vorbis(struct config_info *conf);
int audio_get_real_frequency(int period);
void destruct_audio_stream(struct audio_stream *a_src);
void construct_audio_stream(struct audio_stream *a_src,
 Uint32 (* mix_data)(struct audio_stream *a_src, Sint32 *buffer, Uint32 len),
 void (* set_volume)(struct audio_stream *a_src, Uint32 volume),
 void (* set_repeat)(struct audio_stream *a_src, Uint32 repeat),
 void (* set_order)(struct audio_stream *a_src, Uint32 order),
 void (* set_position)(struct audio_stream *a_src, Uint32 pos),
 Uint32 (* get_order)(struct audio_stream *a_src),
 Uint32 (* get_position)(struct audio_stream *a_src),
 Uint32 (* get_length)(struct audio_stream *a_src),
 void (* destruct)(struct audio_stream *a_src),
 Uint32 volume, Uint32 repeat);

// Platform-related functions.
void audio_callback(Sint16 *stream, int len);
void init_audio_platform(struct config_info *conf);
void quit_audio_platform(void);

#else // !CONFIG_AUDIO

static inline void init_audio(struct config_info *conf) {}
static inline void quit_audio(void) {}
static inline void set_music_volume(int volume) {}
static inline void set_sound_volume(int volume) {}
static inline void set_music_on(int val) {}
static inline void set_sfx_on(int val) {}
static inline void set_sfx_volume(int volume) {}
static inline void set_max_samples(int max_samples) {}
static inline int get_max_samples(void) { return 0; }
static inline void end_sample(void) {}
static inline void end_module(void) {}
static inline int load_module(char *filename, bool safely, int volume) { return 1; }
static inline void volume_module(int vol) {}
static inline void set_position(int pos) {}
static inline void jump_module(int order) {}
static inline void shift_frequency(int freq) {}
static inline void play_sample(int period, char *filename, bool safely) {}
static inline int get_music_on_state(void) { return 0; }
static inline int get_sfx_on_state(void) { return 0; }
static inline int get_music_volume(void) { return 0; }
static inline int get_sound_volume(void) { return 0; }
static inline int get_sfx_volume(void) { return 0; }
static inline int get_position(void) { return 0; }
static inline int get_order(void) { return 0; }
static inline int get_frequency(void) { return 0; }
static inline int audio_get_length(void) { return 0; }

#endif // CONFIG_AUDIO

__M_END_DECLS

#endif // __AUDIO_H
