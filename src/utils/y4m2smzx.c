/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2023 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "image_file.h"
#include "smzxconv.h"
#include "y4m.h"

#include "../graphics.h"
#include "../io/memfile.h"

#ifdef _WIN32
#include <fcntl.h>
#endif

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

#define DEFAULT_MZX_SPEED   4
#define DEFAULT_FRAMERATE_N 125
#define DEFAULT_FRAMERATE_D (2 * (DEFAULT_MZX_SPEED - 1))

static void fourcc(const char *fourcc, size_t len, struct memfile *mf)
{
  mfwrite(fourcc, 4, 1, mf);
  mfputud(len, mf);
}

int main(int argc, char **argv)
{
  struct y4m_data y4m;
  struct memfile mf;
  uint8_t buffer[256];
  mzx_tile *tile = NULL;
  mzx_glyph chr[256];
  mzx_color pal[256];

  smzx_converter *conv = NULL;
  struct rgba_color *rgba_buffer = NULL;
  const char *input_file_name = NULL;
  const char *output_file_name = NULL;
  FILE *in = NULL;
  FILE *out = NULL;
  boolean init = false;
  size_t total = 0;
  int ret = 2;

  uint8_t mzm_head[16];
  unsigned mzm_size;
  unsigned width_chars;
  unsigned height_chars;
  unsigned framerate_n = DEFAULT_FRAMERATE_N;
  unsigned framerate_d = DEFAULT_FRAMERATE_D;

  if(argc < 3)
  {
    fprintf(stderr,
"y4m2smzx Image Conversion Utility\n\n"

"Usage: %s <in.y4m|-> <out.dat|-> [options]\n\n"

"  Converts a .y4m input file to "

"  If the input file is '-', it will be read from stdin.\n\n"
"  If the output file is '-', it will be written to stdout.\n\n"

"  The binary output format is an unpadded IFF-like:\n\n"

"    Head:  MZXV [len:u32le] [chunks...] (len may be zero)\n"
"    Chunk: [FOURCC identifier] [len:u32le] [data...]\n\n"

"  Supported header chunk FOURCCs (must preceed frame data):\n\n"

"    fwid     [display_width_in_chars:u32le] (required)\n"
"    fhei     [display_height_in_chars:u32le] (required)\n"
"    bwid     [buffer_width_in_chars:u32le] (default fwid)\n"
"    bhei     [buffer_height_in_chars:u32le] (default fhei)\n"
"    sprx     [sprite_x_pitch:u32le] (default 0)\n"
"    spry     [sprite_y_pitch:u32le] (default 0)\n"
"    sprw     [sprites_per_row:u32le] (default 1)\n"
"    sprh     [sprites_per_column:u32le] (default 1)\n"
"    spru     [sprites_unbound:u32le] (default 0)\n"
"    spro     [sprite_offset_per_sprite:u32le] (default 0)\n"
"    sprt     [sprite_tcol:s32le] (default -1, bottom sprite always -1)\n"
"\n"
"  Supported frame chunk FOURCCs (omitted data persists between frames):\n\n"

"    smzx     [smzxmode:u32le] (default 0)\n"
"    rate     [numerator:u32le] [denominator:u32le] (default 125/6)\n"
"    fpal     [palette:len bytes]\n"
"    fidx     [indices:len bytes]\n"
"    fchr     [charset:len bytes]\n"
"    fmzm     [mzm:len bytes] (frame displays upon parsing)\n"
"\n",
      argv[0]
    );

    return 1;
  }

  if(!strcmp(argv[1], "-"))
  {
    in = stdin;
#ifdef _WIN32
    /* Windows forces stdin to be text mode by default, fix it. */
    _setmode(_fileno(stdin), _O_BINARY);
#endif
  }
  else
    input_file_name = argv[1];

  if(!strcmp(argv[2], "-"))
  {
    out = stdout;
#ifdef _WIN32
    /* Windows forces stdout to be text mode by default, fix it. */
    _setmode(_fileno(stdout), _O_BINARY);
#endif
  }
  else
    output_file_name = argv[2];

#ifdef CONFIG_PLEDGE_UTILS
#ifdef PLEDGE_HAS_UNVEIL
  if((input_file_name && unveil(input_file_name, "r")) ||
   (output_file_name && unveil(output_file_name, "cw")) ||
   unveil(NULL, NULL))
  {
    fprintf(stderr, "ERROR: Failed unveil!\n");
    return 1;
  }
#endif

  if(pledge(PROMISES, ""))
  {
    fprintf(stderr, "ERROR: Failed pledge!\n");
    return 1;
  }
#endif

  if(input_file_name)
  {
    in = fopen_unsafe(input_file_name, "rb");
    if(!in)
    {
      fprintf(stderr, "ERROR: failed to open input file.\n");
      goto err;
    }
    setvbuf(in, NULL, _IOFBF, 1 << 15);
  }
  if(!y4m_init(&y4m, in))
  {
    fprintf(stderr, "ERROR: input file is not y4m\n");
    goto err;
  }
  init = true;

  if(y4m.width % 8)
  {
    fprintf(stderr, "ERROR: image width not divisible by 8\n");
    goto err;
  }
  if(y4m.height % 14)
  {
    fprintf(stderr, "WARNING: image height not divisible by 14; image will be cropped\n");
  }
  width_chars = y4m.width / 8;
  height_chars = y4m.height / 14;

  conv = smzx_convert_init(width_chars, height_chars, 0, -1, 256, 0, 16);
  if(!conv)
  {
    fprintf(stderr, "ERROR: failed to initialize converter.\n");
    goto err;
  }

  tile = (mzx_tile *)malloc(sizeof(mzx_tile) * width_chars * height_chars);
  if(!tile)
  {
    fprintf(stderr, "ERROR: failed to allocate tile buffer.\n");
    goto err;
  }

  rgba_buffer = (struct rgba_color *)malloc(y4m.rgba_buffer_size);
  if(!rgba_buffer)
  {
    fprintf(stderr, "ERROR: failed to allocate frame buffer.\n");
    goto err;
  }

  if(output_file_name)
  {
    out = fopen_unsafe(output_file_name, "wb");
    if(!out)
    {
      fprintf(stderr, "ERROR: failed to open output file.\n");
      goto err;
    }
    setvbuf(in, NULL, _IOFBF, 1 << 15);
  }

  if(y4m.framerate_n && y4m.framerate_d)
  {
    framerate_n = y4m.framerate_n;
    framerate_d = y4m.framerate_d;
  }

  mzm_size = sizeof(mzm_head) + 2 * width_chars * height_chars;
  mfopen_wr(mzm_head, sizeof(mzm_head), &mf);
  mfwrite("MZM2", 4, 1, &mf);
  mfputw(width_chars, &mf);
  mfputw(height_chars, &mf);
  mfputud(0, &mf);
  mfputc(0, &mf);
  mfputc(1, &mf);
  mfputw(0, &mf);

  mfopen_wr(buffer, sizeof(buffer), &mf);
  fourcc("MZXV", 0, &mf);
  fourcc("fwid", 4, &mf);
  mfputud(width_chars, &mf);
  fourcc("fhei", 4, &mf);
  mfputud(height_chars, &mf);
  fourcc("smzx", 4, &mf);
  mfputud(2, &mf);
  fourcc("rate", 8, &mf);
  mfputud(framerate_n, &mf);
  mfputud(framerate_d, &mf);

  total = mftell(&mf) - 8;
  if(!fwrite(buffer, total + 8, 1, out))
    goto err;

  while(y4m_init_frame(&y4m, in))
  {
    if(!y4m_read_frame(&y4m, in))
    {
      fprintf(stderr, "ERROR: failed to process frame\n");
      goto err;
    }
    y4m_convert_frame_rgba(&y4m, (struct y4m_rgba_color *)rgba_buffer);
    smzx_convert(conv, rgba_buffer, tile, chr, pal);

    mfseek(&mf, 0, SEEK_SET);
    fourcc("fchr", CHAR_SIZE * CHARSET_SIZE, &mf);
    fwrite(buffer, 8, 1, out);
    fwrite(chr, CHAR_SIZE, CHARSET_SIZE, out);
    total += 8 + CHAR_SIZE * CHARSET_SIZE;

    mfseek(&mf, 0, SEEK_SET);
    fourcc("fpal", SMZX_PAL_SIZE * 3, &mf);
    fwrite(buffer, 8, 1, out);
    fwrite(pal, SMZX_PAL_SIZE, 3, out);
    total += 8 + 3 * SMZX_PAL_SIZE;

    mfseek(&mf, 0, SEEK_SET);
    fourcc("fmzm", mzm_size, &mf);
    fwrite(buffer, 8, 1, out);
    fwrite(mzm_head, sizeof(mzm_head), 1, out);
    fwrite(tile, 2, width_chars * height_chars, out);
    total += 8 + mzm_size;
  }

  // Insert total output file length (if not stdout).
  if(output_file_name)
  {
    if(fseek(out, 4, SEEK_SET) == 0)
    {
      mfseek(&mf, 0, SEEK_SET);
      mfputud(total, &mf);
      fwrite(buffer, 4, 1, out);
    }
  }
  ret = 0;

err:
  if(init)
    y4m_free(&y4m);

  if(conv)
    smzx_convert_free(conv);

  free(tile);
  free(rgba_buffer);

  if(input_file_name)
    fclose(in);

  if(output_file_name)
    fclose(out);

  return ret;
}

#if 0
#include <yuv4mpeg.h>

#define MZX_SPEED 4

static const y4m_ratio_t dest_fps = { 125, 2 * (MZX_SPEED - 1) };

int main(int argc, char **argv)
{
  int sf, df, stop = 0;
  int fd, frame_count, csub_type, csub_x, csub_y, x, y, yc, ic, i, r, g, b;
  smzx_converter *c;
  struct rgba_color img[640 * 350];
  mzx_tile tile[80 * 25];
  mzx_glyph chr[256];
  mzx_color pal[256];
  int yuvlen[3];
  uint8_t *yuv[3];
  y4m_ratio_t src_fps;
  y4m_ratio_t frame_ratio;
  y4m_stream_info_t istream;
  y4m_frame_info_t iframe;

  fd = fileno(stdin);
  y4m_accept_extensions(1);
  y4m_init_stream_info(&istream);
  y4m_init_frame_info(&iframe);

  if(y4m_read_stream_header(fd, &istream) != Y4M_OK)
  {
    fprintf(stderr, "Error reading stream header.\n");
    return 1;
  }
  if(y4m_si_get_plane_count(&istream) != 3)
  {
    fprintf(stderr, "Only 3-plane formats supported.\n");
    return 1;
  }
  if((y4m_si_get_width(&istream) != 640) || (y4m_si_get_height(&istream) != 350))
  {
    fprintf(stderr, "Video not 640x350.\n");
    return 1;
  }

  src_fps = y4m_si_get_framerate(&istream);
  frame_ratio.n = src_fps.d * dest_fps.n;
  frame_ratio.d = src_fps.n * dest_fps.d;
  y4m_ratio_reduce(&frame_ratio);
  frame_count = frame_ratio.d;
  csub_type = y4m_si_get_chroma(&istream);
  csub_x = y4m_chroma_ss_x_ratio(csub_type).d;
  csub_y = y4m_chroma_ss_y_ratio(csub_type).d;
  yuvlen[0] = y4m_si_get_plane_length(&istream, 0);
  yuvlen[1] = y4m_si_get_plane_length(&istream, 1);
  yuvlen[2] = y4m_si_get_plane_length(&istream, 2);

  yuv[0] = malloc(yuvlen[0]);
  yuv[1] = malloc(yuvlen[1]);
  yuv[2] = malloc(yuvlen[2]);
  if(!yuv[0] || !yuv[1] || !yuv[2])
  {
    fprintf(stderr, "Error allocating YUV buffer.\n");
    free(yuv[0]);
    free(yuv[1]);
    free(yuv[2]);
    y4m_fini_frame_info(&iframe);
    y4m_fini_stream_info(&istream);
    return 1;
  }

  c = smzx_convert_init(80, 25, 0, -1, 256, 0, 16);
  if(!c)
  {
    fprintf(stderr, "Error initializing converter.\n");
    free(yuv[0]);
    free(yuv[1]);
    free(yuv[2]);
    y4m_fini_frame_info(&iframe);
    y4m_fini_stream_info(&istream);
    return 1;
  }

  sf = df = 0;
  while(!stop && (y4m_read_frame(fd, &istream, &iframe, yuv) == Y4M_OK))
  {
    frame_count += frame_ratio.n;
    if(frame_count > frame_ratio.d)
    {
      for(i = y = 0; y < 350; y++)
      {
        yc = y / csub_y * 640 / csub_x;
        for(x = 0; x < 640; x++, i++)
        {
          ic = yc + x / csub_x;
          r = yuv[0][i]
            + 359 * (yuv[2][ic] - 128) / 256;
          g = yuv[0][i]
            - 88 * (yuv[1][ic] - 128) / 256
            - 183 * (yuv[2][ic] - 128) / 256;
          b = yuv[0][i]
            + 453 * (yuv[1][ic] - 128) / 256;
          r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
          g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
          b = (b < 0) ? 0 : ((b > 255) ? 255 : b);
          img[i].r = r;
          img[i].g = g;
          img[i].b = b;
          img[i].a = 255;
        }
      }
      smzx_convert(c, img, tile, chr, pal);
      fprintf(stderr, "Converted src: %d dest: %d\n", sf, df);
    }

    while(frame_count > frame_ratio.d)
    {
      if(!fwrite(pal, sizeof(pal), 1, stdout))
      {
        fprintf(stderr, "Error writing stream.\n");
        stop = 1;
        break;
      }
      if(!fwrite(chr, sizeof(chr), 1, stdout))
      {
        fprintf(stderr, "Error writing stream.\n");
        stop = 1;
        break;
      }
      if(!fwrite(tile, sizeof(tile), 1, stdout))
      {
        fprintf(stderr, "Error writing stream.\n");
        stop = 1;
        break;
      }
      frame_count -= frame_ratio.d;
      df++;
    }
    sf++;
  }

  free(yuv[0]);
  free(yuv[1]);
  free(yuv[2]);
  y4m_fini_frame_info(&iframe);
  y4m_fini_stream_info(&istream);
  return 0;
}
#endif
