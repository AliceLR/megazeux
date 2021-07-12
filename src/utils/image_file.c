/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
 *
 * PNG boilerplate (from png2smzx):
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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

#include "image_file.h"
#include "../util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#endif

// These constraints are determined by the maximum size of a board/vlayer/MZM,
// multiplied by the number of pixels per char in a given dimension.
#define MAXIMUM_WIDTH  (32767u * 8u)
#define MAXIMUM_HEIGHT (32767u * 14u)
#define MAXIMUM_PIXELS ((16u << 20u) * 8u * 14u)

static uint16_t read16be(const uint8_t in[2])
{
  return (in[0] << 8u) | in[1];
}

static uint32_t read32be(const uint8_t in[4])
{
  return (in[0] << 24u) | (in[1] << 16u) | (in[2] << 8u) | in[3];
}

/**
 * Initialize an image file's pixel array and other values.
 */
static boolean image_init(uint32_t width, uint32_t height, struct image_file *dest)
{
  struct rgba_color *data;

  if(!width || !height || width > MAXIMUM_WIDTH || height > MAXIMUM_HEIGHT ||
   (uint64_t)width * (uint64_t)height > MAXIMUM_PIXELS)
    return false;

  data = calloc(width * height, sizeof(struct rgba_color));
  if(!data)
    return false;

  debug("Size: %zu by %zu\n", (size_t)width, (size_t)height);
  dest->width = width;
  dest->height = height;
  dest->data = data;
  return true;
}

/**
 * Free an image struct's allocated data. Does not modify any other fields.
 */
void image_free(struct image_file *dest)
{
  free(dest->data);
  dest->data = NULL;
}


#ifdef CONFIG_PNG

/**
 * PNG loader.
 */

#include "../pngops.h"

static boolean dummy_constraint(png_uint_32 _w, png_uint_32 _h)
{
  uint64_t w = _w;
  uint64_t h = _h;

  if(!w || !h || w > MAXIMUM_WIDTH || h > MAXIMUM_HEIGHT || w * h > MAXIMUM_PIXELS)
    return false;

  return true;
}

static void *dummy_allocator(png_uint_32 w, png_uint_32 h,
 png_uint_32 *stride, void **pixels)
{
  void *p = malloc(w * h * 4);
  *stride = w * 4;
  *pixels = p;
  return p;
}

static boolean load_png(FILE *fp, struct image_file *dest)
{
  png_uint_32 w;
  png_uint_32 h;

  debug("Image type: PNG\n");
  dest->data = png_read_stream(fp, &w, &h, true, dummy_constraint,
   dummy_allocator);

  if(dest->data)
  {
    dest->width = w;
    dest->height = h;
    return true;
  }
  return false;
}

#endif /* !CONFIG_PNG */


/**
 * BMP loader.
 */

// FIXME


/**
 * NetPBM loaders.
 */

#define fget_value(maxval, fp) \
 ((maxval > 255) ? (fgetc(fp) << 8) | fgetc(fp) : fgetc(fp))

static boolean skip_whitespace(FILE *fp)
{
  int value = fgetc(fp);

  if(value == '#')
  {
    // Skip line comment...
    while(value != '\n' && value >= 0)
      value = fgetc(fp);

    return true;
  }

  if(isspace(value))
    return true;

  ungetc(value, fp);
  return false;
}

static int next_number(size_t *output, FILE *fp)
{
  int value;
  *output = 0;

  while(skip_whitespace(fp));

  while(1)
  {
    value = fgetc(fp);
    if(!isdigit(value))
    {
      ungetc(value, fp);
      return value;
    }

    *output = (*output * 10) + (value - '0');

    if(*output > UINT32_MAX)
      return EOF;
  }
}

static boolean next_value(uint32_t *value, uint32_t maxval, FILE *fp)
{
  size_t v;
  if(next_number(&v, fp) == EOF || v > maxval)
    return false;

  *value = v;
  return true;
}

static boolean next_bit_value(uint32_t *value, FILE *fp)
{
  int v;

  // Unlike plaintext PGM and PPM, plaintext PBM doesn't require whitespace to
  // separate components--each component is always a single digit character.
  while(skip_whitespace(fp));

  v = fgetc(fp);
  if(v != '0' && v != '1')
    return false;

  *value = (v - '0');
  return true;
}

static boolean pbm_header(uint32_t *width, uint32_t *height, FILE *fp)
{
  // NOTE: already read magic.
  size_t w = 0;
  size_t h = 0;

  if(next_number(&w, fp) == EOF ||
   next_number(&h, fp) == EOF)
    return false;

  // Skip exactly one whitespace (for binary files).
  skip_whitespace(fp);

  *width = w;
  *height = h;
  return true;
}

static boolean ppm_header(uint32_t *width, uint32_t *height, uint32_t *maxval, FILE *fp)
{
  // NOTE: already read magic. PGM and PPM use the same header format.
  size_t w = 0;
  size_t h = 0;
  size_t m = 0;

  if(next_number(&w, fp) == EOF ||
   next_number(&h, fp) == EOF ||
   next_number(&m, fp) == EOF)
    return false;

  // Skip exactly one whitespace (for binary files).
  skip_whitespace(fp);

  if(!m || m > 65535)
    return false;

  *width = w;
  *height = h;
  *maxval = m;
  return true;
}

static boolean load_portable_bitmap_plain(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t size;
  uint32_t i;

  debug("Image type: PBM (P1)\n");

  if(!pbm_header(&width, &height, fp))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;

  for(i = 0; i < size; i++)
  {
    uint32_t val;
    if(!next_bit_value(&val, fp))
      break;

    val = val ? 0 : 255;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_bitmap_binary(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t x;
  uint32_t y;

  debug("Image type: PBM (P4)\n");

  if(!pbm_header(&width, &height, fp))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;

  // Each row is padded to a whole number of bytes...
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width;)
    {
      int val = fgetc(fp);
      int mask;
      if(val < 0)
        break;

      for(mask = 0x80; mask && x < width; mask >>= 1, x++)
      {
        int pixval = (val & mask) ? 0 : 255;
        pos->r = pixval;
        pos->g = pixval;
        pos->b = pixval;
        pos->a = 255;
        pos++;
      }
    }
  }
  return true;
}

static boolean load_portable_greymap_plain(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PGM (P2)\n");

  if(!ppm_header(&width, &height, &maxval, fp))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    uint32_t val;
    if(!next_value(&val, maxval, fp))
      break;

    val = (val * maxscale) >> 32u;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_greymap_binary(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PGM (P5)\n");

  if(!ppm_header(&width, &height, &maxval, fp))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int val = fget_value(maxval, fp);
    if(val < 0)
      break;

    val = (val * maxscale) >> 32u;
    pos->r = val;
    pos->g = val;
    pos->b = val;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_pixmap_plain(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PPM (P3)\n");

  if(!ppm_header(&width, &height, &maxval, fp))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    uint32_t r, g, b;
    if(!next_value(&r, maxval, fp))
      break;
    if(!next_value(&g, maxval, fp))
      break;
    if(!next_value(&b, maxval, fp))
      break;

    pos->r = (r * maxscale) >> 32u;
    pos->g = (g * maxscale) >> 32u;
    pos->b = (b * maxscale) >> 32u;
    pos->a = 255;
    pos++;
  }
  return true;
}

static boolean load_portable_pixmap_binary(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  uint32_t width;
  uint32_t height;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PPM (P6)\n");

  if(!ppm_header(&width, &height, &maxval, fp))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int r = fget_value(maxval, fp);
    int g = fget_value(maxval, fp);
    int b = fget_value(maxval, fp);
    if(r < 0 || g < 0 || b < 0)
      break;

    pos->r = (r * maxscale) >> 32u;
    pos->g = (g * maxscale) >> 32u;
    pos->b = (b * maxscale) >> 32u;
    pos->a = 255;
    pos++;
  }
  return true;
}

struct pam_tupl
{
  const char *name;
  int8_t red_pos;
  int8_t green_pos;
  int8_t blue_pos;
  int8_t alpha_pos;
};

static boolean pam_set_tupltype(struct pam_tupl *dest, const char *tuplstr)
{
  static const struct pam_tupl tupltypes[] =
  {
    { "BLACKANDWHITE",        0, 0, 0, -1 },
    { "GRAYSCALE",            0, 0, 0, -1 },
    { "RGB",                  0, 1, 2, -1 },
    { "BLACKANDWHITE_ALPHA",  0, 0, 0,  1 },
    { "GRAYSCALE_ALPHA",      0, 0, 0,  1 },
    { "RGB_ALPHA",            0, 1, 2,  3 },
  };

  size_t i;
  for(i = 0; i < ARRAY_SIZE(tupltypes); i++)
  {
    if(!strcasecmp(tupltypes[i].name, tuplstr))
    {
      *dest = tupltypes[i];
      return true;
    }
  }
  warn("unsupported TUPLTYPE '%s'!\n", tuplstr);
  return false;
}

static boolean pam_header(uint32_t *width, uint32_t *height, uint32_t *maxval,
 uint32_t *depth, struct pam_tupl *tupltype, FILE *fp)
{
  char linebuf[256];
  char tuplstr[256];
  char *key;
  char *value;
  boolean has_w = false;
  boolean has_h = false;
  boolean has_m = false;
  boolean has_d = false;
  boolean has_e = false;
  boolean has_tu = false;

  while(fgets(linebuf, 256, fp))
  {
    size_t len = strlen(linebuf);
    while(len > 0 && (linebuf[len - 1] == '\r' || linebuf[len - 1] == '\n'))
      len--, linebuf[len] = '\0';

    value = linebuf;
    while(isspace(*value))
      value++;

    if(!*value || *value == '#')
      continue;

    key = value;
    while(*value && !isspace(*value))
      value++;

    if(*value)
      *(value++) = '\0';

    if(!strcasecmp(key, "ENDHDR"))
    {
      has_e = true;
      break;
    }
    else

    if(!strcasecmp(key, "WIDTH"))
    {
      if(!*value || has_w)
        return false;

      *width = strtoul(value, NULL, 10);
      has_w = true;
    }
    else

    if(!strcasecmp(key, "HEIGHT"))
    {
      if(!*value || has_h)
        return false;

      *height = strtoul(value, NULL, 10);
      has_h = true;
    }
    else

    if(!strcasecmp(key, "DEPTH"))
    {
      if(!*value || has_d)
        return false;

      *depth = strtoul(value, NULL, 10);
      has_d = true;
    }
    else

    if(!strcasecmp(key, "MAXVAL"))
    {
      if(!*value || has_m)
        return false;

      *maxval = strtoul(value, NULL, 10);
      has_m = true;
    }
    else

    if(!strcasecmp(key, "TUPLTYPE"))
    {
      // NOTE: the spec supports combining multiple of these, but doesn't use it.
      if(!*value || has_tu)
        return false;

      snprintf(tuplstr, sizeof(tuplstr), "%s", value);
      has_tu = true;
    }
  }

  if(!has_w || !has_h || !has_d || !has_m || !has_e)
    return false;

  if(*depth < 1 || *depth > 4)
  {
    warn("invalid depth '%u'!\n", (unsigned int)*depth);
    return false;
  }

  if(*maxval < 1 || *maxval > 65535)
  {
    warn("invalid maxval '%u'!\n", (unsigned int)*maxval);
    return false;
  }

  if(has_tu)
    has_tu = pam_set_tupltype(tupltype, tuplstr);

  if(!has_tu)
  {
    // Guess TUPLTYPE from depth.
    const char *guess = "";
    switch(*depth)
    {
      case 1: guess = "GRAYSCALE"; break;
      case 2: guess = "GRAYSCALE_ALPHA"; break;
      case 3: guess = "RGB"; break;
      case 4: guess = "RGB_ALPHA"; break;
    }
    if(!pam_set_tupltype(tupltype, guess))
      return false;
  }

  return true;
}

static boolean load_portable_arbitrary_map(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  struct pam_tupl tupltype;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t maxval;
  uint64_t maxscale;
  uint32_t size;
  uint32_t i;

  debug("Image type: PAM (P7)\n");

  if(!pam_header(&width, &height, &maxval, &depth, &tupltype, fp))
    return false;

  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = dest->width * dest->height;
  maxscale = ((uint64_t)255 << 32u) / maxval;

  for(i = 0; i < size; i++)
  {
    int v[4];
    for(uint32_t i = 0; i < depth; i++)
    {
      v[i] = fget_value(maxval, fp);
      if(v[i] < 0)
        return true;

      v[i] = (v[i] * maxscale) >> 32u;
    }

    pos->r = v[tupltype.red_pos];
    pos->g = v[tupltype.green_pos];
    pos->b = v[tupltype.blue_pos];
    pos->a = tupltype.alpha_pos >= 0 ? v[tupltype.alpha_pos] : 255;
    pos++;
  }
  return true;
}


/**
 * Farbfeld loader.
 */

static uint8_t convert_16b_to_8b(uint16_t component)
{
  return (uint32_t)component * 255u / 65535u;
}

static boolean load_farbfeld(FILE *fp, struct image_file *dest)
{
  struct rgba_color *pos;
  uint8_t buf[8];
  uint32_t width;
  uint32_t height;
  uint32_t size;
  uint32_t i;

  debug("Image type: farbfeld\n");

  if(fread(buf, 1, 8, fp) < 8)
    return false;

  width  = read32be(buf + 0);
  height = read32be(buf + 4);
  if(!image_init(width, height, dest))
    return false;

  pos = dest->data;
  size = width * height;

  for(i = 0; i < size; i++)
  {
    if(fread(buf, 1, 8, fp) < 8)
      break;

    pos->r = convert_16b_to_8b(read16be(buf + 0));
    pos->g = convert_16b_to_8b(read16be(buf + 2));
    pos->b = convert_16b_to_8b(read16be(buf + 4));
    pos->a = convert_16b_to_8b(read16be(buf + 6));
    pos++;
  }
  return true;
}


/**
 * Common file handling.
 */

#define MAGIC(a,b) (((a) << 8) | (b))

/**
 * Load an image from a stream.
 * Assume this stream can NOT be seeked.
 */
boolean load_image_from_stream(FILE *fp, struct image_file *dest)
{
  uint8_t magic[8];

  memset(dest, 0, sizeof(struct image_file));

  if(fread(magic, 1, 2, fp) < 2)
    return false;

  switch(MAGIC(magic[0], magic[1]))
  {
    /* BMP */
    case MAGIC('B','M'): return false;

    /* NetPBM */
    case MAGIC('P','1'): return load_portable_bitmap_plain(fp, dest);
    case MAGIC('P','2'): return load_portable_greymap_plain(fp, dest);
    case MAGIC('P','3'): return load_portable_pixmap_plain(fp, dest);
    case MAGIC('P','4'): return load_portable_bitmap_binary(fp, dest);
    case MAGIC('P','5'): return load_portable_greymap_binary(fp, dest);
    case MAGIC('P','6'): return load_portable_pixmap_binary(fp, dest);
    case MAGIC('P','7'): return load_portable_arbitrary_map(fp, dest);
  }

  if(fread(magic + 2, 1, 6, fp) < 6)
    return false;

  /* farbfeld */
  if(!memcmp(magic, "farbfeld", 8))
    return load_farbfeld(fp, dest);

#ifdef CONFIG_PNG

  /* PNG (via libpng). */
  if(png_sig_cmp(magic, 0, 8) == 0)
    return load_png(fp, dest);

#else

  if(!memcmp(magic, "\x89PNG\r\n\x1A\n", 8))
    warn("MegaZeux utils were compiled without PNG support--is this a PNG?\n");

#endif

  debug("unknown format\n");
  return false;
}

/**
 * Load an image from a file.
 */
boolean load_image_from_file(const char *filename, struct image_file *dest)
{
  FILE *fp;
  boolean ret;

  memset(dest, 0, sizeof(struct image_file));

  if(!strcmp(filename, "-"))
  {
#ifdef _WIN32
    /* Windows forces stdin to be text mode by default, fix it. */
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    return load_image_from_stream(stdin, dest);
  }

  fp = fopen_unsafe(filename, "rb");
  if(!fp)
    return false;

  setvbuf(fp, NULL, _IOFBF, 32768);

  ret = load_image_from_stream(fp, dest);
  fclose(fp);

  return ret;
}
