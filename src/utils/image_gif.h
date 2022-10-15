/* MegaZeux
 *
 * Copyright (C) 2022 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __UTILS_IMAGE_GIF_H
#define __UTILS_IMAGE_GIF_H

#include <stdint.h>
#include <stdlib.h>

#define GIF_DEFAULT_DELAY 5

/* Read up to `num` bytes from `handle` into the buffer pointed to by `dest`.
 * Returns the number of bytes actually read from `handle`. */
typedef size_t (*gif_read_function)(void *dest, size_t num, void *handle);
/* Allocate user-managed memory. Used for pixel buffers returned by gif_composite. */
typedef void *(*gif_alloc_function)(size_t sz);
typedef uint8_t gif_bool;

enum gif_bool_values
{
  GIF_FALSE,
  GIF_TRUE
};

enum gif_error
{
  GIF_OK = 0,
  GIF_IO,
  GIF_MEM,
  GIF_INVALID,
  GIF_SIGNATURE
};

/* The loader does not normalize color tables to 24bpp.
 * This is performed in the compositor instead. */
struct gif_color
{
  uint8_t r; /* red component (0 to gif->component_max) */
  uint8_t g; /* green component (0 to gif->component_max) */
  uint8_t b; /* blue component (0 to gif->component_max) */
};

struct gif_rgba /* for gif_composite output */
{
  uint8_t r; /* red component (0 to 255) */
  uint8_t g; /* green component (0 to 255) */
  uint8_t b; /* blue component (0 to 255) */
  uint8_t a; /* alpha component (usually 0 or 255) */
};

struct gif_color_table
{
  gif_bool  is_sorted;          /* If true, entries are sorted from most to least used. */
  uint16_t  num_entries;        /* Number of entries in table (1 to 256). */
  struct gif_color entries[1];  /* All color entries in table (flexible array member). */
};

struct gif_graphics_control
{
  /* Graphics Control Extension */

  /*  0 */ // 0x21
  /*  1 */ // 0xF9
  /*  2 */ // 4                         /* Size of GCE block. */
  /*pack*/ uint8_t  disposal_method;    /* Image disposal method after display (0 to 7). */
  /*pack*/ uint8_t  input_required;     /* Image requires user input to advance (0 or 1). */
  /*  4 */ uint16_t delay_time;         /* Image display duration in ms. */
  /*  6 */ int16_t  transparent_color;  /* Image transparent color (0 to 255, or -1 for none). */
};

struct gif_image
{
  /* Image Descriptor */

  /*  0 */ // 0x2C
  /*  1 */ uint16_t left;               /* Left corner X position on canvas. */
  /*  3 */ uint16_t top;                /* Top corner Y position on canvas. */
  /*  5 */ uint16_t width;              /* Width of image. */
  /*  7 */ uint16_t height;             /* Height of image. */
  /*pack*/ gif_bool is_interlaced;      /* GIF_TRUE if this image was stored interlaced. */

  /* Local Color Table */
  struct gif_color_table *color_table;

  /* Graphic Control Extension */
  struct gif_graphics_control control;

  /* Data (uncompressed, deinterlaced, flexible array member). */
  uint8_t pixels[1];
};

struct gif_comment
{
  uint8_t length;                       /* Length of comment subblock. */
  char comment[1];                      /* Comment (\0 terminator appended). */
};

struct gif_appdata
{
  uint8_t length;                       /* Length of application subblock. */
  uint8_t appdata[1];                   /* Application data subblock. */
};

struct gif_info
{
  /* Header */
  /*  0 */ char     signature[3];       /* "GIF" */
  /*  3 */ char     version[3];         /* "87a" or "89a" */

  /* Logical Screen Descriptor */
  /*  6 */ uint16_t width;              /* Canvas width. */
  /*  8 */ uint16_t height;             /* Canvas height. */
  /*pack*/ uint8_t  component_res;      /* Original image component bit depth? */
  /* 11 */ uint8_t  bg_color;           /* Index of background color in global color table. */
  /* 12 */ uint8_t  pixel_ratio;        /* Pixel aspect ratio. ratio = (val+15)/64 */

  /* Global Color Table */
  struct gif_color_table *global_color_table; /* NULL if not present. */

  /* Graphics Control Extension
   * This contains the most recently loaded Graphics Control Extension data.
   * This is intended to populate the following image with this data as it
   * is meant to preceed its corresponding image. Certain libraries too good
   * to follow the standard only respect/generate a Graphics Control block
   * at the very end of the file, so this can be used to recover that info if
   * needed. */
  struct gif_graphics_control control;
  gif_bool control_after_final_image;

  /* Images */
  unsigned num_images_alloc;
  unsigned num_images;                  /* Number of images in GIF. */
  struct gif_image **images;            /* Array of images, or NULL if none. */

  /* Comments */
  unsigned num_comments_alloc;
  unsigned num_comments;                /* Number of comments. */
  struct gif_comment **comments;        /* Array of comments, or NULL if none. */

  /* Application */
  char application[9];                  /* Application string (\0 appended). */
  uint8_t application_code[3];          /* Application code. */
  unsigned num_appdata_alloc;
  unsigned num_appdata;                 /* Number of application data subblocks. */
  struct gif_appdata **appdata;         /* Application data subblocks, or NULL if none. */

  /* TODO: Plain Text */
};


/**
 * Get the error string for a given error.
 *
 * @param   err   a `gif_error` value.
 * @returns       a statically allocated string corresponding to the error.
 */
const char *gif_error_string(enum gif_error err);

/**
 * Read all GIF data structures from the input handle into RAM.
 *
 * @param   gif             `gif_info` struct to store all GIF data to.
 * @param   handle          stream handle for readfn.
 * @param   readfn          function which returns bytes from `handle` stream.
 * @param   skip_signature  if `GIF_TRUE`, do not read "GIF" and immediately
 *                          attempt to read the version string instead. Use
 *                          this if the file format has already been IDed.
 * @returns                 `GIF_OK` on success, otherwise a `gif_error` value.
 */
enum gif_error gif_read(struct gif_info *gif, void *handle,
 const gif_read_function readfn, gif_bool skip_signature);

/**
 * Free all GIF data structures from a `gif_info` struct.
 *
 * @param   gif             `gif_info` struct to free GIF data from.
 */
void gif_close(struct gif_info *gif);


#ifndef GIF_NO_COMPOSITOR

/**
 * Returns the dimensions of the output composited GIF.
 *
 * @param   w               width of output image is stored here.
 * @param   h               height of output image is stored here.
 * @param   gif             `gif_info` struct representing the GIF to composite.
 */
void gif_composite_size(unsigned *w, unsigned *h, const struct gif_info *gif);

/**
 * Composite a GIF into a linear array of `gif_rgba` pixels. The width and
 * height of the generated image can be obtained using `gif_composite_size`.
 * The pointer written to `pixels` must be freed by the user.
 *
 * @param   pixels          destination for generated composite pixel array.
 * @param   gif             `gif_info` struct representing the GIF to composite.
 * @param   allocfn         allocator function for the `gif_rgba` array.
 * @returns                 `GIF_OK` on success, otherwise a `gif_error` value.
 */
enum gif_error gif_composite(struct gif_rgba **pixels, const struct gif_info *gif,
 const gif_alloc_function allocfn);

#endif /* !GIF_NO_COMPOSITOR */

#endif /* __UTILS_IMAGE_GIF_H */
