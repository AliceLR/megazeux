/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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

#include "../../src/event.h"
#include "../../src/platform.h"
#include "../../src/util.h"
#include "../../src/io/fsafeopen.h"
#include "../../src/audio/audio.h"

#include <nds.h>
#include <maxmod9.h>
#include "platform.h"
#include "../../src/audio/audio.h"
#include "../../src/audio/sfx.h"

#ifdef CONFIG_AUDIO

// Utility logic

static u8 nds_vol_table[] = {
	0, 8, 16, 26, 36, 48, 61, 75, 91, 108, 127
};

#define NDS_VOLUME(vol) nds_vol_table[CLAMP((vol), 0, 10)]

// FIFO calls

static inline void nds_sound_volume(int volume) {
  fifoSendValue32(FIFO_MZX, CMD_MZX_SOUND_VOLUME | (NDS_VOLUME(volume) << 24));
}

static inline void nds_pcs_sound(int freq, int volume) {
  fifoSendValue32(FIFO_MZX, CMD_MZX_PCS_TONE | (((freq) & 0xFFFF) << 8) | (NDS_VOLUME(volume) << 24));
}

// PC speaker implementation

#define PCS_DURATION_PER_VBL 9

static int pcs_playing;
static int pcs_frequency;
static int pcs_duration;

static inline void nds_pcs_tick(int duration)
{
  int last_playing = pcs_playing;
  int last_frequency = pcs_frequency;
  int ticks = 0;
  int ticked;

  while (ticks < duration)
  {
    if (pcs_playing)
    {
      if (pcs_duration <= 0)
      {
        pcs_playing = 0;
      }
      else
      {
        ticked = MIN(duration - ticks, pcs_duration);
        ticks += ticked;
        pcs_duration -= ticked;
      }
      continue;
    }

    // Update PC speaker.
    sfx_next_note(&pcs_playing, &pcs_frequency, &pcs_duration);

    if (!pcs_playing)
      break;
  }

  if (pcs_playing != last_playing)
  {
    nds_pcs_sound(pcs_playing ? pcs_frequency : 0, NDS_VOLUME(audio_get_pcs_volume()));
  }
  else if (pcs_playing && (pcs_frequency != last_frequency))
  {
    nds_pcs_sound(pcs_frequency, NDS_VOLUME(audio_get_pcs_volume()));
  }
}

// Maxmod glue code

#define MAXMOD_SONGS 1
#define MAXMOD_SAMPLES 0

static mm_ds_system maxmod_conf;
static int maxmod_effective_frequency;

static void nds_maxmod_init(void)
{
  int i;

  maxmod_conf.mod_count = MAXMOD_SONGS;
  maxmod_conf.samp_count = MAXMOD_SAMPLES;
  maxmod_conf.mem_bank = malloc(sizeof(mm_word) * (MAXMOD_SONGS + MAXMOD_SAMPLES));
  for (i = 0; i < MAXMOD_SONGS + MAXMOD_SAMPLES; i++)
    maxmod_conf.mem_bank[i] = 0;
  maxmod_conf.fifo_channel = FIFO_MAXMOD;

  DC_FlushAll();

  mmInit(&maxmod_conf);
  mmSelectMode(MM_MODE_C);
}

void audio_set_module_order(int order)
{
  mmPosition(order);
}

int audio_get_module_order(void)
{
  // TODO
  return 0;
}

void audio_set_module_position(int position)
{
  // FIXME: position is not supported by maxmod
  mmPosition(position);
}

int audio_get_module_position(void)
{
  // TODO
  return 0;
}

void audio_set_module_volume(int volume) // 0..255
{
  int base_volume = NDS_VOLUME(audio_get_music_volume()); // 0..127
  mmSetModuleVolume((base_volume * volume) >> 5); // 0..1012 (should be 0..1023)
}

void audio_set_module_frequency(int frequency)
{
  int mm_freq;

  if (frequency == 0)
    frequency = 44100;
  maxmod_effective_frequency = frequency;
  mm_freq = CLAMP(div32(frequency * 0x400, 44100), 0x200, 0x800);

  mmSetModuleTempo(mm_freq);
  mmSetModulePitch(mm_freq);
}

int audio_get_module_frequency(void)
{
  return maxmod_effective_frequency;
}

int audio_get_module_length(void)
{
  // TODO
  return 0;
}

void audio_set_module_loop_start(int loop_start)
{
  // TODO
}

int audio_get_module_loop_start(void)
{
  // TODO
  return 0;
}

void audio_set_module_loop_end(int loop_end)
{
  // TODO
}

int audio_get_module_loop_end(void)
{
  // TODO
  return 0;
}

int audio_play_module(char *filename, boolean safely, int volume)
{
  char mas_filename[MAX_PATH];
  char translated_filename[MAX_PATH];
  u32 mas_size;
  u8 *mas_buffer;
  char *mas_ext_pos;
  FILE *mas_file;

  // we can only play pre-converted .MAS files
  strcpy(mas_filename, filename);
  mas_ext_pos = strrchr(mas_filename, '.');
  if (mas_ext_pos == NULL)
  {
    return 0;
  }
  strcpy(mas_ext_pos, ".mas");

  if(safely)
  {
    if(fsafetranslate(mas_filename, translated_filename, MAX_PATH) != FSAFE_SUCCESS)
    {
      return 0;
    }

    strcpy(mas_filename, translated_filename);
  }

  // load the maxmod soundbank file
  mas_file = fopen_unsafe(mas_filename, "rb");
  if (mas_file == NULL)
  {
    return 0;
  }

  // get soundbank filesize
  fread(&mas_size, 4, 1, mas_file);

  // read buffer
  if (mas_size <= 0)
  {
    fclose(mas_file);
    return 0;
  }

  mas_buffer = malloc(mas_size + 8);
  if (mas_buffer == NULL)
  {
    fclose(mas_file);
    return 0;
  }

  if (!fread(mas_buffer + 4, mas_size + 4, 1, mas_file))
  {
    fclose(mas_file);
    return 0;
  }

  fclose(mas_file);
  audio_end_module();

  maxmod_conf.mem_bank[0] = (mm_word) mas_buffer;
  DC_FlushAll();

  // play module
  mmLoad(0);
  mmStart(0, MM_PLAY_LOOP);

  audio_set_module_volume(volume);
  audio_set_module_frequency(0);

  return 1;
}

void audio_end_module(void)
{
  if (!mmActive())
    return;

  mmStop();

  while (mmActive())
  {
    DC_FlushAll();
    swiWaitForVBlank();
  }

  if (maxmod_conf.mem_bank[0] != 0)
  {
    mmUnload(0);

    free((void*) maxmod_conf.mem_bank[0]);
    maxmod_conf.mem_bank[0] = 0;
  }
}

void audio_spot_sample(int period, int which)
{
  // not implemented
}

// Sample playback code

void audio_play_sample(char *filename, boolean safely, int period)
{
  // TODO
}

void audio_end_sample(void)
{

}

// Audio glue code

#define NDS_SAMPLES_MAX 2

int nds_max_samples;

int audio_get_max_samples(void)
{
  return nds_max_samples;
}

void audio_set_max_samples(int max_samples)
{
  if (max_samples < 0 || max_samples > NDS_SAMPLES_MAX)
    max_samples = NDS_SAMPLES_MAX;
  nds_max_samples = max_samples;

  // TODO: vary locked channels depending on nds_max_samples value
  mmLockChannels(0x07 << 8);
}

void nds_audio_vblank(void)
{
  nds_pcs_tick(PCS_DURATION_PER_VBL);
}

void init_audio(struct config_info *conf)
{
  audio_set_music_volume(conf->music_volume);
  audio_set_sound_volume(conf->sam_volume);
  audio_set_music_on(conf->music_on);
  audio_set_pcs_on(conf->pc_speaker_on);
  audio_set_pcs_volume(conf->pc_speaker_volume);

  init_audio_platform(conf);

  audio_set_max_samples(-1);
}

void quit_audio(void)
{
  quit_audio_platform();
}

void init_audio_platform(struct config_info *conf)
{
  // PC speaker init
  pcs_playing = 0;
  pcs_frequency = 0;
  pcs_duration = 0;

  // master volume init
  nds_sound_volume(10);

  // maxmod init
  nds_maxmod_init();
}

void quit_audio_platform(void)
{
  audio_end_module();

  nds_sound_volume(0);
  nds_pcs_sound(0, 0);
}

#endif // CONFIG_AUDIO
