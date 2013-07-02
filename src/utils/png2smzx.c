/* Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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
#include <errno.h>

#define SKIP_SDL
#include "pngops.h"
#include "smzxconv.h"

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

// FIXME: Fix this better
int error(const char *string, unsigned int type, unsigned int options,
		  unsigned int code) {
	return 0;
}

static bool dummy_constraint(png_uint_32 w, png_uint_32 h) {
	return true;
}

static void *dummy_allocator(png_uint_32 w, png_uint_32 h,
			     png_uint_32 *stride, void **pixels) {
	void *p = malloc(w * h * 4);
	*stride = w * 4;
	*pixels = p;
	return p;
}

static rgba_color *read_png(const char *file, png_uint_32 *w, png_uint_32 *h) {
	return (rgba_color *)png_read_file(file, w, h, dummy_constraint,
					   dummy_allocator);
}

int main (int argc, char **argv) {
	FILE *fd;
	unsigned char mzmhead[16] = {
		'M', 'Z', 'M', '2', /* Magic */
		0, 0, /* Width */
		0, 0, /* Height */
		0, 0, 0, 0, 0, 1, 0, 0 /* Stuff */
	};
        char input_file_name[MAX_PATH] = { '\0' };
        char output_mzm_name[MAX_PATH] = { '\0' };
        char output_chr_name[MAX_PATH] = { '\0' };
        char output_pal_name[MAX_PATH] = { '\0' };
        char output_base_name[MAX_PATH] = { '\0' };
        char ext[5] = { '\0' };

        int skip_char = -1;

	png_uint_32 w, h, i, t;
	rgba_color *img;
	mzx_tile *tile;
	mzx_glyph chr[256];
	mzx_color pal[256];
	smzx_converter *c;

	if (argc < 2 || argv[1][0] == '-') {
		fprintf(stderr, "png2smzx Image Conversion Utility\n\n"

                        "Usage: %s <in.png> [<out> | <out.mzm> "
			"[<out.chr] [<out.pal>]] [options]\n\n"

			"Options:\n"

			"--skip-char=[value 0-255]	Skip this "
			"char in the conversion process.\n"
                        "\n"
		, argv[0]);
		return 1;
	}

        strncpy(input_file_name, argv[1], MAX_PATH);
        input_file_name[MAX_PATH - 1] = '\0';

        // Read the input files
        for(i = 2; i < (unsigned int) argc; i++)
        {
          strncpy(ext, (argv[i] + strlen(argv[i]) - 4), 5);

          if(!strcasecmp(ext, ".mzm"))
            strncpy(output_mzm_name, argv[i], MAX_PATH);
          if(!strcasecmp(ext, ".chr"))
            strncpy(output_chr_name, argv[i], MAX_PATH);
          if(!strcasecmp(ext, ".pal"))
            strncpy(output_pal_name, argv[i], MAX_PATH);

          if(!strncasecmp(argv[i], "--skip-char", 11) &&
           (argv[i][11] == '=') && argv[i][12])
          {
            skip_char = strtol(argv[i] + 12, NULL, 10) % 256;
            fprintf(stderr, "Skipping char %i.", skip_char);
          }
        }
        output_mzm_name[MAX_PATH - 1] = '\0';
        output_chr_name[MAX_PATH - 1] = '\0';
        output_pal_name[MAX_PATH - 1] = '\0';

        // Fill in the missing filenames
        if(output_mzm_name[0])
        {
          strncpy(output_base_name, output_mzm_name,
           strlen(output_mzm_name) - 4);
          output_base_name[strlen(output_mzm_name) - 4] = '\0';
        }
        else
        {
          if(argc >= 3 && argv[2] && argv[2][0] != '-')
            strncpy(output_base_name, argv[2], MAX_PATH - 5);

          else
            strncpy(output_base_name, input_file_name, MAX_PATH - 5);

          output_base_name[MAX_PATH - 6] = '\0';

          strcpy(output_mzm_name, output_base_name);
          strcpy(output_mzm_name + strlen(output_mzm_name), ".mzm");

          output_pal_name[0] = '\0';
          output_chr_name[0] = '\0';
        }

        if(!output_chr_name[0])
        {
          strcpy(output_chr_name, output_base_name);
          strcpy(output_chr_name + strlen(output_chr_name), ".chr");
        }

        if(!output_pal_name[0])
        {
          strcpy(output_pal_name, output_base_name);
          strcpy(output_pal_name + strlen(output_pal_name), ".pal");
        }


        // Do stuff
	img = (rgba_color *)read_png(input_file_name, &w, &h);
	if (!img) {
		fprintf(stderr, "Error reading image.\n");
		return 1;
	}
	if ((w % 8) || (h % 14)) {
		fprintf(stderr, "Image not divisible by 8x14; cropping...\n");
		if (w % 8) {
			t = w / 8 * 8;
			for (i = 1; i < h; i++)
				memmove(img + i * t, img + i * w,
					sizeof(rgba_color) * t);
		}
	}
	w /= 8;
	h /= 14;
	c = smzx_convert_init(w, h, 0, skip_char, 256, 0, 16);
	if (!c) {
		fprintf(stderr, "Error initializing converter.\n");
		free(img);
		return 1;
	}
	tile = malloc(sizeof(mzx_tile) * w * h);
	if (!tile) {
		fprintf(stderr, "Error allocating tile buffer.\n");
		smzx_convert_free(c);
		free(img);
		return 1;
	}
	smzx_convert(c, img, tile, chr, pal);
	smzx_convert_free(c);
	free(img);
	mzmhead[4] = w & 0xFF;
	mzmhead[5] = w >> 8;
	mzmhead[6] = h & 0xFF;
	mzmhead[7] = h >> 8;
	fd = fopen_unsafe(output_mzm_name, "wb");
	if (!fd) {
		fprintf(stderr, "Error opening MZM file.\n");
		free(tile);
		return 1;
	}
	if (fwrite(mzmhead, sizeof(mzmhead), 1, fd) != 1) {
		fprintf(stderr, "Error writing MZM header.\n");
		fclose(fd);
		free(tile);
		return 1;
	}
	if (fwrite(tile, sizeof(mzx_tile) * w * h, 1, fd) != 1) {
		fprintf(stderr, "Error writing MZM data.\n");
		fclose(fd);
		free(tile);
		return 1;
	}
	free(tile);
	fclose(fd);
	fd = fopen_unsafe(output_chr_name, "wb");
	if (!fd) {
		fprintf(stderr, "Error opening CHR file.\n");
		return 1;
	}
	if (fwrite(chr, sizeof(chr), 1, fd) != 1) {
		fprintf(stderr, "Error writing CHR data.\n");
		fclose(fd);
		return 1;
	}
	fclose(fd);
	fd = fopen_unsafe(output_pal_name, "wb");
	if (!fd) {
		fprintf(stderr, "Error opening PAL file.\n");
		return 1;
	}
	if (fwrite(pal, sizeof(pal), 1, fd) != 1) {
		fprintf(stderr, "Error writing PAL data.\n");
		fclose(fd);
		return 1;
	}
	fclose(fd);
	return 0;
}

