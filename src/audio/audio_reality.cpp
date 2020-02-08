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
#include "ext.h"
#include "sampled_stream.h"

#include "../util.h"

#include <cstdio>
#include <cstdlib>
#include <new>

// Yes, this is how the Reality player is intended to be included.
#define RAD_DETECT_REPEATS 1
#include "../../contrib/rad/opal.cpp"
#include "../../contrib/rad/player20.cpp"
#include "../../contrib/rad/validate20.cpp"

// Every order in a RAD is fixed length. This makes a few operations for this
// player much easier.
#define ORDER_LINES 64

// HACK: MSYS2 libstdc++ wants to link lpthread when regular new is
// used, but there may not be a good way to force it to link static.
// Using placement new on a regular alloc means nothing should throw
// and thus nothing should try to link lpthread anymore.
template<typename T>//typename ...Args>
T *nothrow_new(int i)//(Args... args)
{
  T *t = new(ccalloc(1,sizeof(T))) T(i); //T(args...);
  return t;
}

template<typename T>
void nothrow_delete(T *t)
{
  t->~T();
  free(t);
}

struct rad_stream_cls
{
  Opal adlib;
  RADPlayer player;
  rad_stream_cls(Uint32 frequency) : adlib(frequency) {}
};

struct rad_stream
{
  struct sampled_stream s;
  struct rad_stream_cls *cls;
  Uint8 *data;
  Uint32 data_length;
  Uint32 sample_update_timer;
  Uint32 sample_update_max;
  Uint32 effective_frequency;
};

static Uint32 rad_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  Uint32 read_wanted = rad_stream->s.allocated_data_length -
   rad_stream->s.stream_offset;
  Sint16 *read_buffer = rad_stream->s.output_data +
   (rad_stream->s.stream_offset / 2);
  Uint32 rval = 0;
  Uint32 i;

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
        rval = 1;
    }
  }

  sampled_mix_data((struct sampled_stream *)a_src, buffer, len);
  return rval;
}

static void rad_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  //struct rad_stream *rad_stream = (struct rad_stream *)a_src;

  //rad_stream->cls->player.SetMasterVolume(volume * 64 / 255);

  // Use sampled_stream volume since the RAD player master volume doesn't
  // take effect fast enough.
  a_src->volume = volume * 256 / 255;
}

static void rad_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

static void rad_set_order(struct audio_stream *a_src, Uint32 order)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;

  rad_stream->cls->player.SetTunePos(order, 0);
}

static void rad_set_position(struct audio_stream *a_src, Uint32 position)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  Uint32 order = position / ORDER_LINES;
  Uint32 line = position % ORDER_LINES;

  rad_stream->cls->player.SetTunePos(order, line);
}

static void rad_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  struct rad_stream *rad_stream = (struct rad_stream *)s_src;

  if(frequency == 0)
  {
    rad_stream->effective_frequency = 44100;
    frequency = audio.output_frequency;
  }
  else
  {
    rad_stream->effective_frequency = frequency;
    frequency = frequency * audio.output_frequency / 44100;
  }

  s_src->frequency = frequency;
  sampled_set_buffer(s_src);
}

static Uint32 rad_get_order(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;

  return rad_stream->cls->player.GetTunePos();
}

static Uint32 rad_get_position(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  Uint32 order = rad_stream->cls->player.GetTunePos();
  Uint32 line = rad_stream->cls->player.GetTuneLine();

  return order * ORDER_LINES + line;
}

static Uint32 rad_get_length(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  Uint32 length = rad_stream->cls->player.GetTuneEffectiveLength();

  return length * ORDER_LINES;
}

static Uint32 rad_get_frequency(struct sampled_stream *s_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)s_src;
  return rad_stream->effective_frequency;
}

static void rad_destruct(struct audio_stream *a_src)
{
  struct rad_stream *rad_stream = (struct rad_stream *)a_src;
  free(rad_stream->data);
  nothrow_delete(rad_stream->cls);
  sampled_destruct(a_src);
}

static void rad_player_callback(void *arg, Uint16 reg, Uint8 data)
{
  struct rad_stream_cls *cls = (struct rad_stream_cls *)arg;
  cls->adlib.Port(reg, data);
}

static struct audio_stream *construct_rad_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  struct rad_stream *rad_stream = NULL;
  struct rad_stream_cls *cls;
  struct audio_stream_spec a_spec;
  struct sampled_stream_spec s_spec;
  const char *validate;
  Uint32 length;
  Uint8 *data;
  Uint32 rate;
  FILE *fp;

  fp = fopen_unsafe(filename, "rb");
  if(fp)
  {
    /**
     * NOTE: some legacy RAD files in the Reality public archive have a single
     * byte truncated off of the end for no apparent reason. These files load
     * and play normally otherwise. Allocate an extra null byte so they load.
     */
    length = ftell_and_rewind(fp);
    data = (Uint8 *)cmalloc(length + 1);
    data[length] = 0;

    if(!fread(data, length, 1, fp))
    {
      free(data);
      data = NULL;
    }

    fclose(fp);

    if(data)
    {
      validate = RADValidate(data, length);

      if(!validate)
      {
        rad_stream = (struct rad_stream *)cmalloc(sizeof(struct rad_stream));
        cls = nothrow_new<rad_stream_cls>(audio.output_frequency);

        cls->player.Init(data, rad_player_callback, cls);
        rate = cls->player.GetHertz();

        memset(rad_stream, 0, sizeof(struct rad_stream));
        rad_stream->cls = cls;
        rad_stream->data_length = length;
        rad_stream->data = data;
        rad_stream->sample_update_timer = 0;
        rad_stream->sample_update_max = audio.output_frequency / rate;

        memset(&a_spec, 0, sizeof(struct audio_stream_spec));
        a_spec.mix_data = rad_mix_data;
        a_spec.set_volume = rad_set_volume;
        a_spec.set_repeat = rad_set_repeat;
        a_spec.set_order = rad_set_order;
        a_spec.set_position = rad_set_position;
        a_spec.get_order = rad_get_order;
        a_spec.get_position = rad_get_position;
        a_spec.get_length = rad_get_length;
        a_spec.destruct = rad_destruct;

        memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
        s_spec.set_frequency = rad_set_frequency;
        s_spec.get_frequency = rad_get_frequency;

        initialize_sampled_stream((struct sampled_stream *)rad_stream, &s_spec,
         frequency, 2, 1);

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
