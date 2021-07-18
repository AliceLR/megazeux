/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../Unit.hpp"

#ifdef CONFIG_PNG
#include "../../src/pngops.c"
#endif

#include "../../src/utils/image_file.c"

#define DATA_BASEDIR "../image_file/"

struct image_file_const
{
  unsigned int width;
  unsigned int height;
  const struct rgba_color *data;
};

/********** BW template. **********/

#define WHITE { 255, 255, 255, 255 }
#define BLACK {   0,   0,   0, 255 }

static const struct rgba_color raw_bw[] =
{
  // R G B A; 5x5
  BLACK, WHITE, WHITE, WHITE, BLACK,
  WHITE, WHITE, WHITE, WHITE, WHITE,
  WHITE, WHITE, BLACK, WHITE, WHITE,
  WHITE, WHITE, WHITE, WHITE, WHITE,
  BLACK, WHITE, WHITE, WHITE, BLACK,
};

static const struct image_file_const base_bw_img
{
  5, 5, raw_bw
};

/********** Greyscale template. **********/

static const struct rgba_color raw_gs[] =
{
  // R G B A; 4x7
  { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
  { 170, 170, 170, 255 }, { 170, 170, 170, 255 }, { 170, 170, 170, 255 }, { 170, 170, 170, 255 },
  {  85,  85,  85, 255 }, {  85,  85,  85, 255 }, {  85,  85,  85, 255 }, {  85,  85,  85, 255 },
  {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
  { 255, 255, 255, 170 }, { 255, 255, 255, 170 }, { 255, 255, 255, 170 }, { 255, 255, 255, 170 },
  { 255, 255, 255,  85 }, { 255, 255, 255,  85 }, { 255, 255, 255,  85 }, { 255, 255, 255,  85 },
  { 255, 255, 255,   0 }, { 255, 255, 255,   0 }, { 255, 255, 255,   0 }, { 255, 255, 255,   0 },
};

static const struct image_file_const base_gs_img =
{
  4, 7, raw_gs
};

/********** RGB(A) template. **********/

static const struct rgba_color raw_rgba[] =
{
  // R G B A; 8x6
  { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 },
  { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 },

  { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 },
  { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 },

  { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 },
  { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 }, { 0, 0, 255, 255 },

  { 255, 255, 0, 170 }, { 255, 255, 0, 170 }, { 255, 255, 0, 170 }, { 255, 255, 0, 170 },
  { 255, 170, 0,  85 }, { 255, 170, 0,  85 }, { 255, 170, 0,  85 }, { 255, 170, 0,  85 },

  { 255, 0, 255, 170 }, { 255, 0, 255, 170 }, { 255, 0, 255, 170 }, { 255, 0, 255, 170 },
  { 170, 0, 255,  85 }, { 170, 0, 255,  85 }, { 170, 0, 255,  85 }, { 170, 0, 255,  85 },

  { 0, 255, 255, 170 }, { 0, 255, 255, 170 }, { 0, 255, 255, 170 }, { 0, 255, 255, 170 },
  { 0, 255, 170,  85 }, { 0, 255, 170,  85 }, { 0, 255, 170,  85 }, { 0, 255, 170,  85 },
};

static const struct image_file_const base_rgba_img
{
  8, 6, raw_rgba
};


static boolean compare_rgb(const rgba_color &base, const rgba_color &in)
{
  return (base.r == in.r) && (base.g == in.g) && (base.b == in.b) && (in.a == 255);
}

static boolean compare_rgba(const rgba_color &base, const rgba_color &in)
{
  return (base.r == in.r) && (base.g == in.g) && (base.b == in.b) && (base.a == in.a);
}

template<boolean (*COMPARE_FN)(const rgba_color &a, const rgba_color &b)>
static boolean compare_image(const struct image_file_const &base,
 const struct image_file &in, char *msgbuf, size_t buf_size)
{
  if(in.width != base.width)
  {
    snprintf(msgbuf, buf_size, "width mismatch");
    return false;
  }

  if(in.height != base.height)
  {
    snprintf(msgbuf, buf_size, "height mismatch");
    return false;
  }

  size_t num = base.width * base.height;
  for(size_t i = 0; i < num; i++)
  {
    if(!COMPARE_FN(base.data[i], in.data[i]))
    {
      snprintf(msgbuf, buf_size, "pixel mismatch @ %zu - %08x %08x", i,
       *(uint32_t *)(base.data + i), *(uint32_t *)(in.data + i));
      return false;
    }
  }

  return true;
}


UNITTEST(PNG)
{
#ifndef CONFIG_PNG
  SKIP();
#else

  struct image_file img{};
  char msg[256];
  boolean ret;

  size_t off = snprintf(msg, sizeof(msg), "png: ");

  ret = load_image_from_file(DATA_BASEDIR "rgba.png", &img, NULL);
  ASSERTX(ret, msg);
  ret = compare_image<compare_rgba>(base_rgba_img, img, msg + off, sizeof(msg) - off);
  ASSERTX(ret, msg);
  image_free(&img);

#endif /* CONFIG_PNG */
}


UNITTEST(BMP)
{
  // FIXME
  UNIMPLEMENTED();
}


UNITTEST(Netpbm)
{
  struct image_file img{};
  char path[512];
  char msg[256];
  boolean ret;

  static const char *bw_inputs[] =
  {
    /* PBM */
    "p1.pbm",
    "p4.pbm",
  };

  static const char *grey_inputs[] =
  {
    /* PGM */
    "p2.pgm",
    "p2_18.pgm",
    "p2_64.pgm",
    "p5.pgm",
    "p5_18.pgm",
    "p5_64.pgm",
    /* PAM GRAYSCALE */
    "p7_gs.pam",
    "p7_gs18.pam",
    "p7_gs64.pam",
  };

  static const char *greya_inputs[] =
  {
    /* PAM GRAYSCALE_ALPHA */
    "p7_gsa.pam",
    "p7_gsa18.pam",
    "p7_gsa64.pam",
  };

  static const char *rgb_inputs[] =
  {
    /* PPM */
    "p3.ppm",
    "p3_18.ppm",
    "p3_64.ppm",
    "p6.ppm",
    "p6_18.ppm",
    "p6_64.ppm",
    /* PAM RGB */
    "p7_rgb.pam",
    "p7_rgb18.pam",
    "p7_rgb64.pam",
  };

  static const char *rgba_inputs[] =
  {
    /* PAM RGB_ALPHA */
    "p7_rgba.pam",
    "p7_rgba18.pam",
    "p7_rgba64.pam",
  };

  SECTION(Bitmap)
  {
    for(const char *filename : bw_inputs)
    {
      size_t off = snprintf(msg, sizeof(msg), "%s: ", filename);
      snprintf(path, sizeof(path), DATA_BASEDIR "%s", filename);

      ret = load_image_from_file(path, &img, NULL);
      ASSERTX(ret, msg);
      ret = compare_image<compare_rgb>(base_bw_img, img, msg + off, sizeof(msg) - off);
      ASSERTX(ret, msg);
      image_free(&img);
    }
  }

  SECTION(Greymap)
  {
    for(const char *filename : grey_inputs)
    {
      size_t off = snprintf(msg, sizeof(msg), "%s: ", filename);
      snprintf(path, sizeof(path), DATA_BASEDIR "%s", filename);

      ret = load_image_from_file(path, &img, NULL);
      ASSERTX(ret, msg);
      ret = compare_image<compare_rgb>(base_gs_img, img, msg + off, sizeof(msg) - off);
      ASSERTX(ret, msg);
      image_free(&img);
    }
  }

  SECTION(Pixmap)
  {
    for(const char *filename : rgb_inputs)
    {
      size_t off = snprintf(msg, sizeof(msg), "%s: ", filename);
      snprintf(path, sizeof(path), DATA_BASEDIR "%s", filename);

      ret = load_image_from_file(path, &img, NULL);
      ASSERTX(ret, msg);
      ret = compare_image<compare_rgb>(base_rgba_img, img, msg + off, sizeof(msg) - off);
      ASSERTX(ret, msg);
      image_free(&img);
    }
  }

  SECTION(Alpha)
  {
    for(const char *filename : greya_inputs)
    {
      size_t off = snprintf(msg, sizeof(msg), "%s: ", filename);
      snprintf(path, sizeof(path), DATA_BASEDIR "%s", filename);

      ret = load_image_from_file(path, &img, NULL);
      ASSERTX(ret, msg);
      ret = compare_image<compare_rgba>(base_gs_img, img, msg + off, sizeof(msg) - off);
      ASSERTX(ret, msg);
      image_free(&img);
    }

    for(const char *filename : rgba_inputs)
    {
      size_t off = snprintf(msg, sizeof(msg), "%s: ", filename);
      snprintf(path, sizeof(path), DATA_BASEDIR "%s", filename);

      ret = load_image_from_file(path, &img, NULL);
      ASSERTX(ret, msg);
      ret = compare_image<compare_rgba>(base_rgba_img, img, msg + off, sizeof(msg) - off);
      ASSERTX(ret, msg);
      image_free(&img);
    }
  }
}


UNITTEST(farbfeld)
{
  struct image_file img{};
  char msg[256];
  boolean ret;

  size_t off = snprintf(msg, sizeof(msg), "farbfeld: ");

  ret = load_image_from_file(DATA_BASEDIR "farbfeld.ff", &img, NULL);
  ASSERTX(ret, msg);
  ret = compare_image<compare_rgba>(base_rgba_img, img, msg + off, sizeof(msg) - off);
  ASSERTX(ret, msg);
  image_free(&img);
}


UNITTEST(raw)
{
  struct image_file img{};
  char msg[256];
  boolean ret;
  size_t off;

  static const struct image_raw_format gs_format = { base_gs_img.width, base_gs_img.height, 1 };
  static const struct image_raw_format gsa_format = { base_gs_img.width, base_gs_img.height, 2 };
  static const struct image_raw_format rgb_format = { base_rgba_img.width, base_rgba_img.height, 3 };
  static const struct image_raw_format rgba_format = { base_rgba_img.width, base_rgba_img.height, 4 };

  SECTION(Greyscale)
  {
    off = snprintf(msg, sizeof(msg), "raw_gs.raw: ");

    ret = load_image_from_file(DATA_BASEDIR "raw_gs.raw", &img, &gs_format);
    ASSERTX(ret, msg);
    ret = compare_image<compare_rgb>(base_gs_img, img, msg + off, sizeof(msg) - off);
    ASSERTX(ret, msg);
    image_free(&img);
  }

  SECTION(GreyscaleAlpha)
  {
    off = snprintf(msg, sizeof(msg), "raw_gsa.raw: ");

    ret = load_image_from_file(DATA_BASEDIR "raw_gsa.raw", &img, &gsa_format);
    ASSERTX(ret, msg);
    ret = compare_image<compare_rgba>(base_gs_img, img, msg + off, sizeof(msg) - off);
    ASSERTX(ret, msg);
    image_free(&img);
  }

  SECTION(RGB)
  {
    off = snprintf(msg, sizeof(msg), "raw_rgb.raw: ");

    ret = load_image_from_file(DATA_BASEDIR "raw_rgb.raw", &img, &rgb_format);
    ASSERTX(ret, msg);
    ret = compare_image<compare_rgb>(base_rgba_img, img, msg + off, sizeof(msg) - off);
    ASSERTX(ret, msg);
    image_free(&img);
  }

  SECTION(RGBA)
  {
    off = snprintf(msg, sizeof(msg), "raw_rgba.raw: ");

    ret = load_image_from_file(DATA_BASEDIR "raw_rgba.raw", &img, &rgba_format);
    ASSERTX(ret, msg);
    ret = compare_image<compare_rgba>(base_rgba_img, img, msg + off, sizeof(msg) - off);
    ASSERTX(ret, msg);
    image_free(&img);
  }
}
