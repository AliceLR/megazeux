/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2004 madbrain
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "audio.h"
#include "audio_struct.h"
#include "audio_vorbis.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../io/vio.h"

#ifdef CONFIG_TREMOR
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#endif

// ov_read() documentation states the 'bigendianp' argument..
//   "Specifies big or little endian byte packing.
//   0 for little endian, 1 for big endian. Typical value is 0."

// Tremor (as of r19494) doesn't use this, so suppress the warning...
#ifndef CONFIG_TREMOR
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
static const int ENDIAN_PACKING = 1;
#else
static const int ENDIAN_PACKING = 0;
#endif
#endif // !CONFIG_TREMOR

struct vorbis_stream
{
  struct sampled_stream s;
  OggVorbis_File vorbis_file_handle;
  vorbis_info *vorbis_file_info;
  vfile *input_file;
  uint32_t loop_start;
  uint32_t loop_end;
};

static boolean vorbis_mix_data(struct audio_stream *a_src,
 int32_t * RESTRICT buffer, size_t frames, unsigned int channels)
{
  uint32_t read_len = 0;
  struct vorbis_stream *v_stream = (struct vorbis_stream *)a_src;
  uint32_t read_wanted = v_stream->s.allocated_data_length -
   v_stream->s.stream_offset;
  uint32_t pos = 0;
  char *read_buffer = (char *)v_stream->s.output_data +
   v_stream->s.stream_offset;
  int current_section;

  do
  {
    read_wanted -= read_len;

    if(a_src->repeat && v_stream->loop_end)
      pos = (uint32_t)ov_pcm_tell(&v_stream->vorbis_file_handle);

#ifdef CONFIG_TREMOR
    read_len =
     ov_read(&(v_stream->vorbis_file_handle), read_buffer,
     read_wanted, &current_section);
#else
    read_len =
     ov_read(&(v_stream->vorbis_file_handle), read_buffer,
     read_wanted, ENDIAN_PACKING, 2, 1, &current_section);
#endif

    if(a_src->repeat && (pos < v_stream->loop_end) &&
     (pos + read_len / v_stream->s.channels / 2 >= v_stream->loop_end) &&
     (v_stream->loop_start < v_stream->loop_end))
    {
      read_len = (v_stream->loop_end - pos) * v_stream->s.channels * 2;
      ov_pcm_seek(&(v_stream->vorbis_file_handle), v_stream->loop_start);
    }

    // If it hit the end go back to the beginning if repeat is on

    if(read_len == 0)
    {
      if(a_src->repeat)
      {
        ov_raw_seek(&(v_stream->vorbis_file_handle), 0);

        if(read_wanted)
        {
#ifdef CONFIG_TREMOR
          read_len =
           ov_read(&(v_stream->vorbis_file_handle), read_buffer,
           read_wanted, &current_section);
#else
          read_len =
           ov_read(&(v_stream->vorbis_file_handle), read_buffer,
           read_wanted, ENDIAN_PACKING, 2, 1, &current_section);
#endif
        }
      }
      else
      {
        memset(read_buffer, 0, read_wanted);
      }
    }

    read_buffer += read_len;
  } while((read_len < read_wanted) && read_len);

  sampled_mix_data((struct sampled_stream *)v_stream, buffer, frames, channels);

  if(read_len == 0)
    return true;

  return false;
}

static void vorbis_set_volume(struct audio_stream *a_src, unsigned int volume)
{
  a_src->volume = volume * 256 / 255;
}

static void vorbis_set_repeat(struct audio_stream *a_src, boolean repeat)
{
  a_src->repeat = repeat;
}

static void vorbis_set_position(struct audio_stream *a_src, uint32_t position)
{
  ov_pcm_seek(&(((struct vorbis_stream *)a_src)->vorbis_file_handle),
   (ogg_int64_t)position);
}

static void vorbis_set_loop_start(struct audio_stream *a_src, uint32_t position)
{
  ((struct vorbis_stream *)a_src)->loop_start = position;
}

static void vorbis_set_loop_end(struct audio_stream *a_src, uint32_t position)
{
  ((struct vorbis_stream *)a_src)->loop_end = position;
}

static void vorbis_set_frequency(struct sampled_stream *s_src, uint32_t frequency)
{
  if(frequency == 0)
    frequency = ((struct vorbis_stream *)s_src)->vorbis_file_info->rate;

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static uint32_t vorbis_get_position(struct audio_stream *a_src)
{
  struct vorbis_stream *v = (struct vorbis_stream *)a_src;
  return ov_pcm_tell(&v->vorbis_file_handle);
}

static uint32_t vorbis_get_length(struct audio_stream *a_src)
{
  struct vorbis_stream *v = (struct vorbis_stream *)a_src;
  return ov_pcm_total(&v->vorbis_file_handle, -1);
}

static uint32_t vorbis_get_loop_start(struct audio_stream *a_src)
{
  return ((struct vorbis_stream *)a_src)->loop_start;
}

static uint32_t vorbis_get_loop_end(struct audio_stream *a_src)
{
  return ((struct vorbis_stream *)a_src)->loop_end;
}

static uint32_t vorbis_get_frequency(struct sampled_stream *s_src)
{
  return s_src->frequency;
}

static void vorbis_destruct(struct audio_stream *a_src)
{
  struct vorbis_stream *v_stream = (struct vorbis_stream *)a_src;
  ov_clear(&(v_stream->vorbis_file_handle));
  vfclose(v_stream->input_file);
  sampled_destruct(a_src);
}

static size_t vorbis_read_fn(void *ptr, size_t size, size_t count, void *stream)
{
  vfile *vf = (vfile *)stream;
  return vfread(ptr, size, count, vf);
}

static int vorbis_seek_fn(void *stream, ogg_int64_t offset, int whence)
{
  vfile *vf = (vfile *)stream;
  return vfseek(vf, offset, whence);
}

static long vorbis_tell_fn(void *stream)
{
  vfile *vf = (vfile *)stream;
  return vftell(vf);
}

static const ov_callbacks vorbis_callbacks =
{
  vorbis_read_fn,
  vorbis_seek_fn,
  NULL,
  vorbis_tell_fn,
};

static struct audio_stream *construct_vorbis_stream(char *filename,
 uint32_t frequency, unsigned int volume, boolean repeat)
{
  vfile *input_file;
  vorbis_comment *comment;
  int loopstart = -1;
  int looplength = -1;
  int loopend = -1;
  int i;

  // Using a large file buffer at minimum is pretty much mandatory for OGGs to
  // work on the 3DS and Wii U. Since 32k isn't much to ask for most platforms
  // that can support OGGs in the first place, just unconditionally use it.
  // TODO: VFS full file caching (useful for 3DS/Wii U, mandatory for DJGPP).
  input_file = vfopen_unsafe_ext(filename, "rb", V_LARGE_BUFFER);

  if(input_file)
  {
    OggVorbis_File open_file;

    if(!ov_open_callbacks(input_file, &open_file, NULL, 0, vorbis_callbacks))
    {
      vorbis_info *vorbis_file_info = ov_info(&open_file, -1);

      // Surround OGGs not supported yet..
      if(vorbis_file_info->channels <= 2)
      {
        struct vorbis_stream *v_stream = cmalloc(sizeof(struct vorbis_stream));
        struct sampled_stream_spec s_spec;
        struct audio_stream_spec a_spec;

        v_stream->vorbis_file_handle = open_file;
        v_stream->vorbis_file_info = vorbis_file_info;
        v_stream->input_file = input_file;

        v_stream->loop_start = 0;
        v_stream->loop_end = 0;

        comment = ov_comment(&open_file, -1);
        if(comment)
        {
          for(i = 0; i < comment->comments; i++)
          {
            if(!strncasecmp("loopstart=", comment->user_comments[i], 10))
              loopstart = atoi(comment->user_comments[i] + 10);
            else

            if(!strncasecmp("loopend=", comment->user_comments[i], 8))
              loopend = atoi(comment->user_comments[i] + 8);
            else

            if(!strncasecmp("looplength=", comment->user_comments[i], 11))
              looplength = atoi(comment->user_comments[i] + 11);
          }

          if(loopstart >= 0 && (looplength > 0 || loopend > loopstart))
          {
            v_stream->loop_start = loopstart;

            // looplength takes priority since it's older and more "standard"
            if(looplength > 0)
              v_stream->loop_end = loopstart + looplength;
            else
              v_stream->loop_end = loopend;
          }
        }

        memset(&a_spec, 0, sizeof(struct audio_stream_spec));
        a_spec.mix_data       = vorbis_mix_data;
        a_spec.set_volume     = vorbis_set_volume;
        a_spec.set_repeat     = vorbis_set_repeat;
        a_spec.set_position   = vorbis_set_position;
        a_spec.set_loop_start = vorbis_set_loop_start;
        a_spec.set_loop_end   = vorbis_set_loop_end;
        a_spec.get_position   = vorbis_get_position;
        a_spec.get_length     = vorbis_get_length;
        a_spec.get_loop_start = vorbis_get_loop_start;
        a_spec.get_loop_end   = vorbis_get_loop_end;
        a_spec.destruct       = vorbis_destruct;

        memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
        s_spec.set_frequency = vorbis_set_frequency;
        s_spec.get_frequency = vorbis_get_frequency;

        initialize_sampled_stream((struct sampled_stream *)v_stream, &s_spec,
         frequency, v_stream->vorbis_file_info->channels, true);

        initialize_audio_stream((struct audio_stream *)v_stream, &a_spec,
         volume, repeat);

        return (struct audio_stream *)v_stream;
      }
      ov_clear(&open_file);
    }
    vfclose(input_file);
  }
  return NULL;
}

void init_vorbis(struct config_info *conf)
{
  audio_ext_register("ogg", construct_vorbis_stream);
}
