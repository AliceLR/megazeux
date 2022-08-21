/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define delay delay_dos
#include <bios.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>
#undef delay
#include "../../src/audio/audio.h"
#include "../../src/audio/audio_struct.h"
#include "../../src/util.h"
#include "platform_djgpp.h"

#ifdef CONFIG_AUDIO

struct sb_config
{
  // from environment variable, config, ...
  uint16_t port;
  uint8_t irq, dma8, dma16, type;
  // from DSP
  uint8_t version_major, version_minor;
  // from allocation
  int buffer_selector, buffer_segment;
  uint8_t *buffer;
  uint8_t buffer_block;
  // driver-specific:
  // - to restore on deinit
  uint8_t old_21h;
  _go32_dpmi_seginfo old_irq_handler;
  // - if nearptr enabled
  boolean nearptr_buffer_enabled;
  __dpmi_meminfo nearptr_buffer_mapping;
};

static struct sb_config sb_cfg;

static void audio_sb_fill_block(void)
{
  int block_size_bytes = audio.buffer_samples << 2;
  int offset = (sb_cfg.buffer_block != 0) ? block_size_bytes : 0;

  audio_callback((int16_t *)(sb_cfg.buffer + offset), block_size_bytes);
  if(!sb_cfg.nearptr_buffer_enabled)
    dosmemput(sb_cfg.buffer + offset, block_size_bytes, (sb_cfg.buffer_segment << 4) + offset);
}

static void audio_sb_next_block(void)
{
  audio_sb_fill_block();
  sb_cfg.buffer_block ^= 1;
}

static void audio_sb_interrupt(void)
{
  audio_sb_next_block();
  inportb(sb_cfg.port + 0xF); // ack (sb)
  outportb(0x20, 0x20); // ack (pic)
}

static void audio_sb_parse_env(struct sb_config *conf, char *env)
{
  char *token;
  while(env != NULL)
  {
    token = strsep(&env, " ");
    switch(token[0])
    {
      case 'A':
        conf->port = strtol(token + 1, NULL, 16);
        break;
      case 'D':
        conf->dma8 = strtol(token + 1, NULL, 16);
        break;
      case 'H':
        conf->dma16 = strtol(token + 1, NULL, 16);
        break;
      case 'I':
        conf->irq = strtol(token + 1, NULL, 16);
        break;
      case 'T':
        conf->type = strtol(token + 1, NULL, 16);
        break;
    }
  }
}

static uint8_t audio_sb_dsp_read(void)
{
  while(!(inportb(sb_cfg.port + 0xE) & 0x80))
    ;
  return inportb(sb_cfg.port + 0xA);
}

static void audio_sb_dsp_write(uint8_t val)
{
  while(inportb(sb_cfg.port + 0xC) & 0x80)
    ;
  outportb(sb_cfg.port + 0xC, val);
}

static boolean audio_sb_dsp_detect(void)
{
  uint16_t i;
  outportb(sb_cfg.port + 0x6, 1);
  usleep(3);
  outportb(sb_cfg.port + 0x6, 0);
  for(i = 65535; i > 0; i--)
    if(inportb(sb_cfg.port + 0xE) & 0x80)
      break;
  if(i == 0)
    return false;
  for(i = 65535; i > 0; i--)
    if(inportb(sb_cfg.port + 0xA) == 0xAA)
      break;
  return i > 0;
}

void init_audio_platform(struct config_info *conf)
{
  _go32_dpmi_seginfo new_irq_handler;
  int buffer_size_bytes;
  // Try to find a Sound Blaster
  char *sb_env = getenv("BLASTER");
  if(sb_env != NULL)
    audio_sb_parse_env(&sb_cfg, sb_env);
  if(sb_cfg.port != 0)
    if(!audio_sb_dsp_detect())
      sb_cfg.port = 0;
  if(sb_cfg.port != 0)
  {
    audio_sb_dsp_write(0xE1); // version
    sb_cfg.version_major = audio_sb_dsp_read();
    sb_cfg.version_minor = audio_sb_dsp_read();

    if(sb_cfg.version_major < 4)
      sb_cfg.port = 0;
    else

    if(sb_cfg.irq >= 8) // TODO: support IRQ8-15?
      sb_cfg.port = 0;
    else

    if(sb_cfg.dma16 < 5 || sb_cfg.dma16 >= 8)
      sb_cfg.port = 0;
  }

  if(sb_cfg.port != 0)
  {
    // configure sample rate
    audio.output_frequency = CLAMP(audio.output_frequency, 5000, 44100);

    if(sb_cfg.version_major >= 4) // SB16
    {
      audio_sb_dsp_write(0x41); // set sampling rate
      audio_sb_dsp_write(audio.output_frequency >> 8);
      audio_sb_dsp_write(audio.output_frequency & 0xFF);
    }
    /* else // pre-SB16
    {
      uint8_t time_constant = 256 - (500000 / audio.output_frequency);
      audio.output_frequency = 500000 / (256 - time_constant);
      audio_sb_dsp_write(0x40); // set time constant
      audio_sb_dsp_write(time_constant);
    } */

    // buffer_samples * 2 (stereo) * 2 (16-bit) * 2 (doubled)
    // each sample is 4 bytes
    if(audio.buffer_samples > 4096)
      audio.buffer_samples = 4096;
    else

    if(audio.buffer_samples <= 0)
      audio.buffer_samples = 2048;

    buffer_size_bytes = audio.buffer_samples << 3;
    sb_cfg.nearptr_buffer_enabled = djgpp_push_enable_nearptr();

    audio.mix_buffer = malloc(audio.buffer_samples << 3);

    // allocate memory, without crossing 64K boundary, and clean it
    sb_cfg.buffer_segment = djgpp_malloc_boundary(buffer_size_bytes, 65536, &sb_cfg.buffer_selector);
    if(sb_cfg.nearptr_buffer_enabled)
    {
      sb_cfg.nearptr_buffer_mapping.address = sb_cfg.buffer_segment << 4;
      sb_cfg.nearptr_buffer_mapping.size = buffer_size_bytes;
      if(__dpmi_physical_address_mapping(&sb_cfg.nearptr_buffer_mapping) != 0)
      {
        sb_cfg.nearptr_buffer_enabled = false;
        djgpp_pop_enable_nearptr();
      }
      else
      {
        sb_cfg.buffer = (uint8_t *)(sb_cfg.nearptr_buffer_mapping.address + __djgpp_conventional_base);
        memset(sb_cfg.buffer, 0, buffer_size_bytes);
      }
    }

    if(!sb_cfg.nearptr_buffer_enabled)
    {
      sb_cfg.buffer = malloc(buffer_size_bytes);
      memset(sb_cfg.buffer, 0, buffer_size_bytes);
      dosmemput(sb_cfg.buffer, buffer_size_bytes, sb_cfg.buffer_segment << 4);
    }

    // lock C irq handler
    // (TODO: rewrite handler in ASM?)
    _go32_dpmi_lock_code(audio_sb_interrupt, 1024);
    _go32_dpmi_lock_data(&sb_cfg, sizeof(struct sb_config));
    sb_cfg.buffer_block = 0;

    // configure irq
    _go32_dpmi_get_protected_mode_interrupt_vector(8 + sb_cfg.irq, &sb_cfg.old_irq_handler);
    new_irq_handler.pm_offset = (int) audio_sb_interrupt;
    new_irq_handler.pm_selector = _go32_my_cs();
    _go32_dpmi_chain_protected_mode_interrupt_vector(8 + sb_cfg.irq, &new_irq_handler);

    sb_cfg.old_21h = inportb(0x21);
    outportb(0x21, sb_cfg.old_21h & (~(1 << sb_cfg.irq)));

    djgpp_enable_dma(sb_cfg.dma16, 0x58, sb_cfg.buffer_segment << 4, buffer_size_bytes);

    // configure dsp
    if(sb_cfg.version_major >= 4)
    {
      audio_sb_dsp_write(0xD1); // turn on speaker

      audio_sb_dsp_write(0xB6); // configure transfer (16-bit, auto-init)
      audio_sb_dsp_write(0x30); // stereo, signed
      audio_sb_dsp_write(((audio.buffer_samples << 1) - 1) & 0xFF);
      audio_sb_dsp_write(((audio.buffer_samples << 1) - 1) >> 8);
    }
  }
}

void quit_audio_platform(void)
{
  // Deinitialize audio
  if(sb_cfg.port != 0)
  {
    audio_sb_dsp_write(0xD3); // turn off speaker

    // undo interrupt change
    outportb(0x21, sb_cfg.old_21h);

    // undo vector change
    _go32_dpmi_set_protected_mode_interrupt_vector(8 + sb_cfg.irq, &sb_cfg.old_irq_handler);

    djgpp_disable_dma(sb_cfg.dma16);

    if(sb_cfg.nearptr_buffer_enabled)
    {
      __dpmi_free_physical_address_mapping(&(sb_cfg.nearptr_buffer_mapping));
      djgpp_pop_enable_nearptr();
    }
    else
    {
      free(sb_cfg.buffer);
    }
    __dpmi_free_dos_memory(sb_cfg.buffer_selector);
  }
}

#endif
