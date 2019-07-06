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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "audio_wav.h"
#include "ext.h"
#include "sampled_stream.h"

#include "../util.h"

// For WAV loader fallback
#ifdef CONFIG_SDL
#include "SDL.h"
#endif

// If the WAV/SAM is larger than this, print a warning to the console.
// (Right now only do this for debug builds because a lot more games than
// anticipated use big WAVs and it could get annoying for end users.)
#define WARN_FILESIZE (1<<22)

struct wav_stream
{
  struct sampled_stream s;
  Uint8 *wav_data;
  Uint32 data_offset;
  Uint32 data_length;
  Uint32 channels;
  Uint32 bytes_per_sample;
  Uint32 natural_frequency;
  Uint16 format;
  Uint32 loop_start;
  Uint32 loop_end;
};

static Uint32 wav_read_data(struct wav_stream *w_stream, Uint8 *buffer,
 Uint32 len, Uint32 repeat)
{
  Uint8 *src = (Uint8 *)w_stream->wav_data + w_stream->data_offset;
  Uint32 data_read;
  Uint32 read_len = len;
  Uint32 new_offset;
  Uint32 i;

  switch(w_stream->format)
  {
    case SAMPLE_U8:
    {
      Sint16 *dest = (Sint16 *)buffer;

      read_len /= 2;

      new_offset = w_stream->data_offset + read_len;

      if(w_stream->data_offset + read_len >= w_stream->data_length)
      {
        read_len = w_stream->data_length - w_stream->data_offset;
        if(repeat)
          new_offset = 0;
        else
          new_offset = w_stream->data_length;
      }

      if(repeat && (w_stream->data_offset < w_stream->loop_end) &&
       (w_stream->data_offset + read_len >= w_stream->loop_end) &&
       (w_stream->loop_start < w_stream->loop_end))
      {
        read_len = w_stream->loop_end - w_stream->data_offset;
        new_offset = w_stream->loop_start;
      }

      data_read = read_len * 2;

      for(i = 0; i < read_len; i++)
      {
        dest[i] = (Sint8)(src[i] - 128) << 8;
      }

      break;
    }

    default:
    {
      Uint8 *dest = (Uint8 *) buffer;

      new_offset = w_stream->data_offset + read_len;

      if(w_stream->data_offset + read_len >= w_stream->data_length)
      {
        read_len = w_stream->data_length - w_stream->data_offset;
        if(repeat)
          new_offset = 0;
        else
          new_offset = w_stream->data_length;
      }

      if(repeat && (w_stream->data_offset < w_stream->loop_end)
       && (w_stream->data_offset + read_len >= w_stream->loop_end))
      {
        read_len = w_stream->loop_end - w_stream->data_offset;
        new_offset = w_stream->loop_start;
      }

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
      // swap bytes on big endian machines
      for(i = 0; i < read_len; i += 2)
      {
        dest[i] = src[i + 1];
        dest[i + 1] = src[i];
      }
#else
      // no swap necessary on little endian machines
      memcpy(dest, src, read_len);
#endif

      data_read = read_len;

      break;
    }
  }

  w_stream->data_offset = new_offset;

  return data_read;
}

static Uint32 wav_mix_data(struct audio_stream *a_src, Sint32 *buffer,
 Uint32 len)
{
  Uint32 read_len = 0;
  struct wav_stream *w_stream = (struct wav_stream *)a_src;
  Uint32 read_wanted = w_stream->s.allocated_data_length -
   w_stream->s.stream_offset;
  Uint8 *read_buffer = (Uint8 *)w_stream->s.output_data +
   w_stream->s.stream_offset;

  read_len = wav_read_data(w_stream, read_buffer, read_wanted, a_src->repeat);

  if(read_len < read_wanted)
  {
    read_buffer += read_len;
    read_wanted -= read_len;

    if(a_src->repeat)
    {
      read_len = wav_read_data(w_stream, read_buffer, read_wanted, true);
    }
    else
    {
      memset(read_buffer, 0, read_wanted);
      read_len = 0;
    }
  }

  sampled_mix_data((struct sampled_stream *)w_stream, buffer, len);

  if(read_len == 0)
    return 1;

  return 0;
}

static void wav_set_volume(struct audio_stream *a_src, Uint32 volume)
{
  a_src->volume = volume * 256 / 255;
}

static void wav_set_repeat(struct audio_stream *a_src, Uint32 repeat)
{
  a_src->repeat = repeat;
}

static void wav_set_position(struct audio_stream *a_src, Uint32 position)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  if(position < (w_stream->data_length / w_stream->bytes_per_sample))
    w_stream->data_offset = position * w_stream->bytes_per_sample;
}

static void wav_set_loop_start(struct audio_stream *a_src, Uint32 position)
{
  ((struct wav_stream *)a_src)->loop_start = position;
}

static void wav_set_loop_end(struct audio_stream *a_src, Uint32 position)
{
  ((struct wav_stream *)a_src)->loop_end = position;
}

static void wav_set_frequency(struct sampled_stream *s_src, Uint32 frequency)
{
  if(frequency == 0)
    frequency = ((struct wav_stream *)s_src)->natural_frequency;

  s_src->frequency = frequency;

  sampled_set_buffer(s_src);
}

static Uint32 wav_get_position(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  return w_stream->data_offset / w_stream->bytes_per_sample;
}

static Uint32 wav_get_length(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;

  return w_stream->data_length / w_stream->bytes_per_sample;
}

static Uint32 wav_get_loop_start(struct audio_stream *a_src)
{
  return ((struct wav_stream *)a_src)->loop_start;
}

static Uint32 wav_get_loop_end(struct audio_stream *a_src)
{
  return ((struct wav_stream *)a_src)->loop_end;
}

static Uint32 wav_get_frequency(struct sampled_stream *s_src)
{
  return s_src->frequency;
}

static void wav_destruct(struct audio_stream *a_src)
{
  struct wav_stream *w_stream = (struct wav_stream *)a_src;
  free(w_stream->wav_data);
  sampled_destruct(a_src);
}

static int read_little_endian32(char *buf)
{
  unsigned char *b = (unsigned char *)buf;
  int i, s = 0;
  for(i = 3; i >= 0; i--)
    s = (s << 8) | b[i];
  return s;
}

static int read_little_endian16(char *buf)
{
  unsigned char *b = (unsigned char *)buf;
  return (b[1] << 8) | b[0];
}

static void *get_riff_chunk(FILE *fp, int filesize, char *id, int *size)
{
  int maxsize = filesize - ftell(fp) - 8;
  int c;
  char size_buf[4];
  void *buf;

  if(maxsize < 0)
    return NULL;

  if(id)
  {
    if(fread(id, 1, 4, fp) < 4)
      return NULL;
  }
  else
  {
    fseek(fp, 4, SEEK_CUR);
  }

  if(fread(size_buf, 1, 4, fp) < 4)
    return NULL;

  *size = read_little_endian32(size_buf);
  if(*size > maxsize) *size = maxsize;

  buf = cmalloc(*size);

  if((int)fread(buf, 1, *size, fp) < *size)
  {
    free(buf);
    return NULL;
  }

  // Realign if odd size unless padding byte isn't 0
  if(*size & 1)
  {
    c = fgetc(fp);
    if((c != 0) && (c != EOF))
      fseek(fp, -1, SEEK_CUR);
  }

  return buf;
}

static int get_next_riff_chunk_id(FILE *fp, int filesize, char *id)
{
  if(filesize - ftell(fp) < 8)
    return 0;

  if(fread(id, 1, 4, fp) < 4)
    return 0;

  fseek(fp, -4, SEEK_CUR);
  return 1;
}

static void skip_riff_chunk(FILE *fp, int filesize)
{
  int s, c;
  int maxsize = filesize - ftell(fp) - 8;
  char size_buf[4];

  if(maxsize >= 0)
  {
    fseek(fp, 4, SEEK_CUR);
    if(fread(size_buf, 1, 4, fp) < 4)
      return;

    s = read_little_endian32(size_buf);
    if(s > maxsize)
      s = maxsize;
    fseek(fp, s, SEEK_CUR);

    // Realign if odd size unless padding byte isn't 0
    if(s & 1)
    {
      c = fgetc(fp);
      if((c != 0) && (c != EOF))
        fseek(fp, -1, SEEK_CUR);
    }
  }
}

static void* get_riff_chunk_by_id(FILE *fp, int filesize,
 const char *id, int *size)
{
  int i;
  char id_buf[4];

  fseek(fp, 12, SEEK_SET);

  while((i = get_next_riff_chunk_id(fp, filesize, id_buf)))
  {
    if(memcmp(id_buf, id, 4)) skip_riff_chunk(fp, filesize);
    else break;
  }

  if(!i)
    return NULL;

  return get_riff_chunk(fp, filesize, NULL, size);
}

// Simple SAM loader.

static int load_sam_file(const char *file, struct wav_info *spec)
{
  size_t source_length;
  void *buf;
  int ret = 0;
  FILE *fp;

  fp = fopen_unsafe(file, "rb");
  if(!fp)
    goto exit_out;
  source_length = ftell_and_rewind(fp);
  if(source_length > WARN_FILESIZE)
  {
    debug("Size of SAM file '%s' is %zu; OGG should be used instead.\n",
     file, source_length);
  }

  // Default to no loop
  spec->channels = 1;
  spec->freq = audio_get_real_frequency(SAM_DEFAULT_PERIOD);
  spec->format = SAMPLE_S8;
  spec->loop_start = 0;
  spec->loop_end = 0;

  buf = cmalloc(source_length);
  if(fread(buf, 1, source_length, fp) < source_length)
  {
    free(buf);
    goto exit_close;
  }
  spec->data_length = source_length;
  spec->wav_data = buf;
  goto exit_close_success;

exit_close_success:
  ret = 1;
exit_close:
  fclose(fp);
exit_out:
  return ret;
}

// More lenient than SDL's WAV loader, but only supports
// uncompressed PCM files (for now.)

static int load_wav_file(const char *file, struct wav_info *spec)
{
  int data_size, filesize, riffsize, channels, srate, sbytes, fmt_size;
  int smpl_size, numloops;
  Uint32 loop_start, loop_end;
  char *fmt_chunk, *smpl_chunk, tmp_buf[4];
  ssize_t sam_ext_pos = (ssize_t)strlen(file) - 4;
  size_t file_size;
  int ret = 0;
  FILE *fp;
#ifdef CONFIG_SDL
  SDL_AudioSpec sdlspec;
#endif

  // First, check if this isn't actually a SAM file. If so,
  // route to load_sam_file instead.
  if((sam_ext_pos > 0) && !strcasecmp(file + sam_ext_pos, ".sam"))
    return load_sam_file(file, spec);

  fp = fopen_unsafe(file, "rb");
  if(!fp)
    goto exit_out;

  file_size = ftell_and_rewind(fp);
  if(file_size > WARN_FILESIZE)
  {
    debug("This WAV is too big sempai OwO;;;\n");
    debug("Size of WAV file '%s' is %zu; OGG should be used instead.\n",
     file, file_size);
  }

  // If it doesn't start with "RIFF", it's not a WAV file.
  if(fread(tmp_buf, 1, 4, fp) < 4)
    goto exit_close;

  if(memcmp(tmp_buf, "RIFF", 4))
    goto exit_close;

  // Read reported file size (if the file turns out to be larger, this will be
  // used instead of the real file size.)
  if(fread(tmp_buf, 1, 4, fp) < 4)
    goto exit_close;

  riffsize = read_little_endian32(tmp_buf) + 8;

  // If the RIFF type isn't "WAVE", it's not a WAV file.
  if(fread(tmp_buf, 1, 4, fp) < 4)
    goto exit_close;

  if(memcmp(tmp_buf, "WAVE", 4))
    goto exit_close;

  // With the RIFF header read, we'll now check the file size.
  filesize = ftell_and_rewind(fp);
  if(filesize > riffsize)
    filesize = riffsize;

  fmt_chunk = get_riff_chunk_by_id(fp, filesize, "fmt ", &fmt_size);

  // If there's no "fmt " chunk, or it's less than 16 bytes, it's not a valid
  // WAV file.
  if(!fmt_chunk || (fmt_size < 16))
    goto exit_close;

  // Default to no loop
  spec->loop_start = 0;
  spec->loop_end = 0;

  // If the WAV file isn't uncompressed PCM (format 1), let SDL handle it.
  if(read_little_endian16(fmt_chunk) != 1)
  {
    free(fmt_chunk);
#ifdef CONFIG_SDL
    if(SDL_LoadWAV(file, &sdlspec, &(spec->wav_data), &(spec->data_length)))
    {
      void *copy_buf = cmalloc(spec->data_length);
      memcpy(copy_buf, spec->wav_data, spec->data_length);
      SDL_FreeWAV(spec->wav_data);
      spec->wav_data = copy_buf;
      spec->channels = sdlspec.channels;
      spec->freq = sdlspec.freq;
      switch(sdlspec.format)
      {
        case AUDIO_U8:
          spec->format = SAMPLE_U8;
          break;
        case AUDIO_S8:
          spec->format = SAMPLE_S8;
          break;
        case AUDIO_S16LSB:
          spec->format = SAMPLE_S16LSB;
          break;
        default:
         free(copy_buf);
         goto exit_close;
      }

      goto exit_close_success;
    }
#endif // CONFIG_SDL
    goto exit_close;
  }

  // Get the data we need from the "fmt " chunk.
  channels = read_little_endian16(fmt_chunk + 2);
  srate = read_little_endian32(fmt_chunk + 4);

  // Average bytes per second go here (4 bytes)
  // Block align goes here (2 bytes)
  // Round up when dividing by 8
  sbytes = (read_little_endian16(fmt_chunk + 14) + 7) / 8;
  free(fmt_chunk);

  // If I remember correctly, we can't do anything past stereo, and 0 channels
  // isn't valid.  We can't do anything past 16-bit either, and 0-bit isn't
  // valid. 0Hz sample rate is invalid too.
  if(!channels || (channels > 2) || !sbytes || (sbytes > 2) || !srate)
    goto exit_close;

  // Everything seems to check out, so let's load the "data" chunk.
  spec->wav_data = get_riff_chunk_by_id(fp, filesize, "data", &data_size);
  spec->data_length = data_size;

  // No "data" chunk?! FAIL!
  if(!spec->wav_data)
    goto exit_close;

  // Empty "data" chunk?! ALSO FAIL!
  if(!data_size)
  {
    free(spec->wav_data);
    goto exit_close;
  }

  spec->freq = srate;
  if(sbytes == 1)
    spec->format = SAMPLE_U8;
  else
    spec->format = SAMPLE_S16LSB;
  spec->channels = channels;

  // Check for "smpl" chunk for looping info
  fseek(fp, 8, SEEK_SET);
  smpl_chunk = get_riff_chunk_by_id(fp, filesize, "smpl", &smpl_size);

  // If there's no "smpl" chunk or it's less than 60 bytes, there's no valid
  // loop data
  if(!smpl_chunk || (smpl_size < 60))
    goto exit_close_success;

  numloops = read_little_endian32(smpl_chunk + 28);
  // First loop is at 36
  loop_start = read_little_endian32(smpl_chunk + 44) * channels * sbytes;
  loop_end = read_little_endian32(smpl_chunk + 48) * channels * sbytes;
  free(smpl_chunk);

  // If the number of loops is less than 1, the loop data's invalid
  if(numloops < 1)
    goto exit_close_success;

  // Boundary check loop points
  if((loop_start >= spec->data_length) || (loop_end > spec->data_length)
   || (loop_start >= loop_end))
    goto exit_close_success;

  spec->loop_start = loop_start;
  spec->loop_end = loop_end;

exit_close_success:
  ret = 1;
exit_close:
  fclose(fp);
exit_out:
  return ret;
}

struct audio_stream *construct_wav_stream_direct(struct wav_info *w_info,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  struct wav_stream *w_stream = cmalloc(sizeof(struct wav_stream));
  struct sampled_stream_spec s_spec;
  struct audio_stream_spec a_spec;

  w_stream->wav_data = w_info->wav_data;
  w_stream->data_length = w_info->data_length;
  w_stream->channels = w_info->channels;
  w_stream->data_offset = 0;
  w_stream->format = w_info->format;
  w_stream->natural_frequency = w_info->freq;
  w_stream->bytes_per_sample = w_info->channels;
  w_stream->loop_start = w_info->loop_start;
  w_stream->loop_end = w_info->loop_end;

  if((w_info->format != SAMPLE_U8) && (w_info->format != SAMPLE_S8))
    w_stream->bytes_per_sample *= 2;

  memset(&a_spec, 0, sizeof(struct audio_stream_spec));
  a_spec.mix_data       = wav_mix_data;
  a_spec.set_volume     = wav_set_volume;
  a_spec.set_repeat     = wav_set_repeat;
  a_spec.set_position   = wav_set_position;
  a_spec.set_loop_start = wav_set_loop_start;
  a_spec.set_loop_end   = wav_set_loop_end;
  a_spec.get_position   = wav_get_position;
  a_spec.get_length     = wav_get_length;
  a_spec.get_loop_start = wav_get_loop_start;
  a_spec.get_loop_end   = wav_get_loop_end;
  a_spec.destruct       = wav_destruct;

  memset(&s_spec, 0, sizeof(struct sampled_stream_spec));
  s_spec.set_frequency = wav_set_frequency;
  s_spec.get_frequency = wav_get_frequency;

  initialize_sampled_stream((struct sampled_stream *)w_stream, &s_spec,
    frequency, w_info->channels, 1);

  initialize_audio_stream((struct audio_stream *)w_stream, &a_spec,
    volume, repeat);

  return (struct audio_stream *)w_stream;
}

static struct audio_stream *construct_wav_stream(char *filename,
 Uint32 frequency, Uint32 volume, Uint32 repeat)
{
  struct wav_info w_info;
  memset(&w_info, 0, sizeof(struct wav_info));

  if(load_wav_file(filename, &w_info))
  {
    // Surround WAVs not supported yet..
    if(w_info.channels <= 2)
      return construct_wav_stream_direct(&w_info, frequency, volume, repeat);

    free(w_info.wav_data);
  }
  return NULL;
}

void init_wav(struct config_info *conf)
{
  audio_ext_register("sam", construct_wav_stream);
  audio_ext_register("wav", construct_wav_stream);
}
