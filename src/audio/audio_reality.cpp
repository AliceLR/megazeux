/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

// Parts of this code are based off of the RAD v2 example.cpp player by Reality.

#include "audio.h"
#include "audio_reality.h"
#include "audio_struct.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../util.h"
#include "../io/vio.h"

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <new>

// Yes, this is how the Reality player is intended to be included.
#define RAD_DETECT_REPEATS 1
#include "../../contrib/rad/opal.cpp"
#include "../../contrib/rad/player20.cpp"
#include "../../contrib/rad/validate20.cpp"

#define OPL_FREQUENCY 49716

// Every order in a RAD is fixed length. This makes a few operations for this
// player much easier.
#define ORDER_LINES 64

class FastOpal : public Opal
{
public:
  FastOpal() : Opal(OPL_FREQUENCY) {}

  /**
   * This skips Opal's built-in linear resampler. The built-in resampler uses
   * integer division, which is slow, particularly on platforms like the 3DS
   * that lack an integer division instruction. This means that the output will
   * always be 49716Hz and thus needs resampling (which still should be faster).
   */
  void Sample(int16_t *left, int16_t *right)
  {
    Output(*left, *right);
  }
};

struct rad_stream_cls
{
  FastOpal adlib;
  RADPlayer player;
};

struct rad_stream
{
  struct sampled_stream s;
  struct rad_stream_cls *cls;
  uint8_t *data;
  size_t data_length;
  uint32_t sample_update_timer;
  uint32_t sample_update_max;
  uint32_t effective_frequency;
};

static boolean rad_mix_data(struct audio_stream *a_src, int32_t * RESTRICT buffer,
 size_t frames, unsigned int channels)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  uint32_t read_wanted = rad_stream->s.allocated_data_length -
   rad_stream->s.stream_offset;
  int16_t *read_buffer = rad_stream->s.output_data +
   (rad_stream->s.stream_offset / 2);
  boolean rval = false;
  uint32_t i;

  for(i = 0; i < read_wanted; i += 4)
  {
    rad_stream->cls->adlib.Sample(read_buffer, read_buffer + 1);
    read_buffer += 2;

    rad_stream->sample_update_timer++;
    if(rad_stream->sample_update_timer >= rad_stream->sample_update_max)
    {
      // Play the next line of the RAD.
      boolean repeated = rad_stream->cls->player.Update();
      rad_stream->sample_update_timer = 0;

      if(repeated && !a_src->repeat)
        rval = true;
    }
  }

  sampled_mix_data((struct sampled_stream *)a_src, buffer, frames, channels);
  return rval;
}

static void rad_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  //struct rad_stream *rad_stream = (struct rad_stream *)a_src;

  //rad_stream->cls->player.SetMasterVolume(volume * 64 / 255);

  // Use sampled_stream volume since the RAD player master volume doesn't
  // take effect fast enough.
  a_src->volume = volume * 256 / 255;
}

static void rad_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  a_src->repeat = repeat;
}

static void rad_set_order(struct audio_stream *a_src, uint32_t order)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;

  rad_stream->cls->player.SetTunePos(order, 0);
}

static void rad_set_position(struct audio_stream *a_src, uint32_t position)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  uint32_t order = position / ORDER_LINES;
  uint32_t line = position % ORDER_LINES;

  rad_stream->cls->player.SetTunePos(order, line);
}

static void rad_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  struct rad_stream *rad_stream = (struct rad_stream *)s_src;

  if(frequency == 0)
  {
    rad_stream->effective_frequency = 44100;
    frequency = OPL_FREQUENCY;
  }
  else
  {
    rad_stream->effective_frequency = frequency;
    frequency = (uint64_t)frequency * OPL_FREQUENCY / 44100;
  }

  s_src->frequency = frequency;
  sampled_set_buffer(s_src);
}

static uint32_t rad_get_order(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;

  return rad_stream->cls->player.GetTunePos();
}

static uint32_t rad_get_position(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  uint32_t order = rad_stream->cls->player.GetTunePos();
  uint32_t line = rad_stream->cls->player.GetTuneLine();

  return order * ORDER_LINES + line;
}

static uint32_t rad_get_length(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  uint32_t length = rad_stream->cls->player.GetTuneEffectiveLength();

  return length * ORDER_LINES;
}

static uint32_t rad_get_frequency(struct sampled_stream *s_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)s_src;
  return rad_stream->effective_frequency;
}

static void rad_destruct(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  free(rad_stream->data);
  delete rad_stream->cls;
  sampled_destruct(a_src);
}

static void rad_player_callback(void *arg, uint16_t reg, uint8_t data)
{
  struct rad_stream_cls *cls = (struct rad_stream_cls *)arg;
  cls->adlib.Port(reg, data);
}

static struct audio_stream *construct_rad_stream(char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  struct rad_stream *rad_stream = NULL;
  struct rad_stream_cls *cls;
  struct audio_stream_spec a_spec;
  struct sampled_stream_spec s_spec;
  const char *validate;
  size_t length;
  uint8_t *data;
  uint32_t rate;
  vfile *vf;

  vf = vfopen_unsafe(filename, "rb");
  if(vf)
  {
    /**
     * NOTE: some legacy RAD files in the Reality public archive have a single
     * byte truncated off of the end for no apparent reason. These files load
     * and play normally otherwise. Allocate an extra null byte so they load.
     */
    length = vfilelength(vf, false);
    data = (uint8_t *)cmalloc(length + 1);
    data[length] = 0;

    if(!vfread(data, length, 1, vf))
    {
      free(data);
      data = NULL;
    }

    vfclose(vf);

    if(data)
    {
      validate = RADValidate(data, length);

      if(!validate)
      {
        rad_stream = (struct rad_stream *)cmalloc(sizeof(struct rad_stream));
        cls = new rad_stream_cls();

        cls->player.Init(data, rad_player_callback, cls);
        rate = cls->player.GetHertz();

        memset(rad_stream, 0, sizeof(struct rad_stream));
        rad_stream->cls = cls;
        rad_stream->data_length = length;
        rad_stream->data = data;
        rad_stream->sample_update_timer = 0;
        rad_stream->sample_update_max = OPL_FREQUENCY / rate;

        memset(&a_spec, 0, sizeof(struct audio_stream_spec));
        a_spec.mix_data     = rad_mix_data;
        a_spec.set_volume   = rad_set_volume;
        a_spec.set_repeat   = rad_set_repeat;
        a_spec.set_order    = rad_set_order;
        a_spec.set_position = rad_set_position;
        a_spec.get_order    = rad_get_order;
        a_spec.get_position = rad_get_position;
        a_spec.get_length   = rad_get_length;
        a_spec.destruct     = rad_destruct;

        memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
        s_spec.set_frequency = rad_set_frequency;
        s_spec.get_frequency = rad_get_frequency;

        initialize_sampled_stream((struct sampled_stream *)rad_stream, &s_spec,
         frequency, 2, true);

        initialize_audio_stream((struct audio_stream *)rad_stream, &a_spec,
         volume, repeat);
      }
      else
      {
        debug("RAD: %s -- %s\n", filename, validate);
        free(data);
      }
    }
  }

  return (struct audio_stream *)rad_stream;
}

void init_reality(struct config_info *conf)
{
  audio_ext_register("rad", construct_rad_stream);
}
