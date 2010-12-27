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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "pngops.h"
#include "smzxconv.h"

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
	png_uint_32 w, h, i, t;
	rgba_color *img;
	mzx_tile *tile;
	mzx_glyph chr[256];
	mzx_color pal[256];
	smzx_converter *c;
	if (argc != 5) {
		fprintf(stderr, "Usage: %s <in.png> <out.mzm> <out.chr> "
			"<out.pal>\n", argv[0]);
		return 1;
	}
	img = (rgba_color *)read_png(argv[1], &w, &h);
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
	c = smzx_convert_init(w, h, 0, -1, 256, 0, 16);
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
	fd = fopen_unsafe(argv[2], "wb");
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
	fd = fopen_unsafe(argv[3], "wb");
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
	fd = fopen_unsafe(argv[4], "wb");
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

