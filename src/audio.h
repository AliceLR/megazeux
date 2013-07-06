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
#include "board_struct.h"

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
  void (* destruct)(struct audio_stream *a_src);
};

struct sampled_stream
{
  struct audio_stream a;
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
  void (* set_frequency)(struct sampled_stream *s_src, Uint32 frequency);
  Uint32 (* get_frequency)(struct sampled_stream *s_src);
};

struct pc_speaker_stream
{
  struct audio_stream a;
  Uint32 volume;
  Uint32 playing;
  Uint32 frequency;
  Sint32 last_frequency;
  Uint32 note_duration;
  Uint32 last_duration;
  Uint32 last_playing;
  Uint32 sample_cutoff;
  Uint32 last_increment_buffer;
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

  struct audio_stream *primary_stream;
  struct pc_speaker_stream *pcs_stream;
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

CORE_LIBSPEC void init_audio(struct config_info *conf);
CORE_LIBSPEC void quit_audio(void);
CORE_LIBSPEC void end_module(void);
CORE_LIBSPEC int  load_board_module(struct board *src_board);
CORE_LIBSPEC void play_sample(int freq, char *filename, bool safely);

void end_sample(void);
void jump_module(int order);
int get_order(void);
void volume_module(int vol);
void module_exit(void);
void module_init(void);
void spot_sample(int freq, int sample);
void shift_frequency(int freq);
int get_frequency(void);
void set_position(int pos);
int get_position(void);
int free_sam_cache(char clear_all);
void fix_global_volumes(void);
void sound(int frequency, int duration);
void nosound(int duration);
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

void audio_callback(Sint16 *stream, int len);
void init_audio_platform(struct config_info *conf);
void quit_audio_platform(void);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC int load_module(char *filename, bool safely, int volume);
#endif

#ifdef CONFIG_MODPLUG
int check_ext_for_sam_and_convert(const char *filename, char *new_file);
int check_ext_for_gdm_and_convert(const char *filename, char *new_file);
#define __sam_to_wav_maybe_static __audio_c_maybe_static
#else
#define __sam_to_wav_maybe_static static
#endif

/*** these should only be exported for audio plugins */

#if defined(CONFIG_MODPLUG) || defined(CONFIG_MIKMOD)

void sampled_set_buffer(struct sampled_stream *s_src);
void sampled_mix_data(struct sampled_stream *s_src, Sint32 *dest_buffer,
 Uint32 len);
void sampled_destruct(struct audio_stream *a_src);
void initialize_sampled_stream(struct sampled_stream *s_src,
void (* set_frequency)(struct sampled_stream *s_src, Uint32 frequency),
 Uint32 (* get_frequency)(struct sampled_stream *s_src),
 Uint32 frequency, Uint32 channels, Uint32 use_volume);
void construct_audio_stream(struct audio_stream *a_src,
 Uint32 (* mix_data)(struct audio_stream *a_src, Sint32 *buffer, Uint32 len),
 void (* set_volume)(struct audio_stream *a_src, Uint32 volume),
 void (* set_repeat)(struct audio_stream *a_src, Uint32 repeat),
 void (* set_order)(struct audio_stream *a_src, Uint32 order),
 void (* set_position)(struct audio_stream *a_src, Uint32 pos),
 Uint32 (* get_order)(struct audio_stream *a_src),
 Uint32 (* get_position)(struct audio_stream *a_src),
 void (* destruct)(struct audio_stream *a_src),
 Uint32 volume, Uint32 repeat);

#endif // CONFIG_MODPLUG || CONFIG_MIKMOD

/*** end audio plugins exports */

#else // !CONFIG_AUDIO

static inline void init_audio(struct config_info *conf) {}
static inline void quit_audio(void) {}
static inline void set_music_volume(int volume) {}
static inline void set_sound_volume(int volume) {}
static inline void set_music_on(int val) {}
static inline void set_sfx_on(int val) {}
static inline void set_sfx_volume(int volume) {}
static inline void end_sample(void) {}
static inline void end_module(void) {}
static inline void load_module(char *filename, bool safely, int volume) {}
static inline int load_board_module(struct board *src_board) { return 1; }
static inline void volume_module(int vol) {}
static inline void set_position(int pos) {}
static inline void jump_module(int order) {}
static inline void shift_frequency(int freq) {}
static inline void play_sample(int freq, char *filename, bool safely) {}
static inline void spot_sample(int freq, int sample) {}
static inline int get_music_on_state(void) { return 0; }
static inline int get_sfx_on_state(void) { return 0; }
static inline int get_music_volume(void) { return 0; }
static inline int get_sound_volume(void) { return 0; }
static inline int get_sfx_volume(void) { return 0; }
static inline int get_position(void) { return 0; }
static inline int get_order(void) { return 0; }
static inline int get_frequency(void) { return 0; }

#endif // CONFIG_AUDIO

__M_END_DECLS

#endif // __AUDIO_H
