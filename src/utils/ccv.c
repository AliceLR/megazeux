/*

Copyright (C) 2017  Dr Lancer-X (drlancer@megazeux.org)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#define CCV_VERSION "v0.074"

// With fix contributed by Lachesis

// Compile with:
// gcc -o ccv ccv.c -Wall -O3

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>

#include "../config.h"

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

// This software requires uthash to compile. Download it from
// http://troydhanson.github.com/uthash/

#include "uthash.h"

#define MAX_PATH 65535

#define USAGE  "Usage:\n" \
"ccv input.bmp\n" \
".. converts input.bmp to input.chr. The conversion will convert the image to a charset or partial charset containing a quantised representation of the image. No char reuse will be done.\n" \
"ccv input.bmp -mzm\n" \
".. converts input.bmp to input.chr and input.mzm. If the same character would have been used multiple times, it will be reused. Hence while the .chr itself may not match the input image, the combination of the .chr and .mzm will.\n" \
"ccv input.bmp -reuse\n" \
".. converts input.bmp to input.chr, reusing chars where possible. This option is implied with -mzm, but can be used for simple charset output with this.\n" \
"ccv input.bmp -mzm -noreuse\n" \
".. converts input.bmp to input.chr and input.mzm without reusing characters.\n" \
"ccv input.bmp -o output\n" \
".. converts input.bmp to output.chr\n" \
"ccv input.bmp -mzm -o output\n" \
".. converts input.bmp to output.chr and output.mzm\n" \
"ccv input1.bmp input2.bmp -mzm\n" \
".. converts input1.bmp and input2.bmp to input1.chr, input2.chr, input1.mzm and input2.mzm\n" \
"ccv input.bmp -mzm -blank 32 \n" \
".. converts input.bmp to input.chr and input.mzm. Char 32 will be blank and blank parts of the image will be assigned char #32 instead of some other char.\n" \
"ccv -c 30 input.bmp -mzm\n" \
".. converts input.bmp to input.chr and input.mzm, with no more than 30 characters used. If necessary, close characters will be reused. -c implies -reuse.\n" \
"ccv -offset 128 input.bmp -mzm\n" \
".. converts input.bmp to input.chr and input.mzm with the char indices in input.mzm being offset by 128 places. Hence loading the charset with load char set \"@128input.chr\" and placing the MZM should result in accurate graphics.\n" \
"ccv -exclude 32 input.bmp -mzm\n" \
".. converts input.bmp to input.chr and input.mzm. If char #32 would otherwise be used in the conversion, it will be replaced with a blank char instead and will not be referenced in the output -mzm.\n" \
"ccv -exclude 32-63 input.bmp -mzm\n" \
".. converts input.bmp to input.chr and input.mzm. If chars #32 to #63 would otherwise be used in the conversion, they will be replaced with blank chars instead and will not be referenced in the output -mzm.\n" \
"ccv -exclude 0,32 input.bmp -mzm\n" \
".. converts input.bmp to input.chr and input.mzm. If chars #0 or #32 would otherwise be used in the conversion, they will be replaced with blank chars instead and will not be referenced in the output -mzm.\n" \
"ccv input.raw -w 256 -h 112\n" \
".. converts input.raw to input.chr. input.raw is assumed to be an 8-bit headerless image of the dimensions given, where zero is black and nonzero is white.\n" \
"ccv input.bmp -threshold 160\n" \
".. converts input.bmp to input.chr turning pixels >= 160 brightness white and other pixels black. If not supplied, the default is 127.\n" \
"ccv input.bmp -dither floyd-steinberg\n" \
"ccv input.bmp -dither stucki\n" \
"ccv input.bmp -dither jarvis-judice-ninke\n" \
"ccv input.bmp -dither burkes\n" \
"ccv input.bmp -dither sierra\n" \
"ccv input.bmp -dither stevenson-arce\n" \
".. converts input.bmp to input.chr using the given error-diffusion kernel. Note that you can abbreviate it (i.e. 'floyd', 'jarvis') as it only has to match part of the name. Note that the -threshold value, if supplied, is still used here.\n" \
"ccv -q input.bmp\n" \
".. suppress output to stdout\n" \

void Usage()
{
  fprintf(stderr, "ccv " CCV_VERSION " - char conversion tool for megazeux\n");
  fprintf(stderr, "by Dr Lancer-X <drlancer@megazeux.org>\n");
  fprintf(stderr, "Usage: ccv [options] input_file [...]\n");
  fprintf(stderr, "Type \"ccv -help\" for extended options information\n");
}

void Help()
{
  fprintf(stderr, USAGE);
}

void Error(const char *string, ...)
{
  va_list vaList;
  va_start(vaList, string);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, string, vaList);
  fprintf(stderr, "\n");
  va_end(vaList);
  exit(1);
}

inline static int lc_strcmp(const char *A, const char *B)
{
  const char *a = A;
  const char *b = B;

  if (A == NULL) return -1;
  if (B == NULL) return 1;
  for (;;) {
    if (tolower(*a) < tolower(*b)) return -1;
    if (tolower(*a) > tolower(*b)) return 1;
    if (*a == '\0') return 0;
    a++;
    b++;
  }
}
inline static int lc_strncmp(const char *A, const char *B, int n)
{
  const char *a = A;
  const char *b = B;
  int i = 0;

  if (A == NULL) return -1;
  if (B == NULL) return 1;
  for (;;) {
    if (i >= n) return 0;
    if (tolower(*a) < tolower(*b)) return -1;
    if (tolower(*a) > tolower(*b)) return 1;
    if (*a == '\0') return 0;
    a++;
    b++;
    i++;
  }
}

inline static void file_write32(int val, FILE *fp)
{
  fputc((val >> 0) & 0xFF, fp);
  fputc((val >> 8) & 0xFF, fp);
  fputc((val >> 16) & 0xFF, fp);
  fputc((val >> 24) & 0xFF, fp);
}

inline static int file_read32(FILE *fp)
{
  unsigned int r = 0;
  r |= fgetc(fp);
  r |= fgetc(fp) << 8;
  r |= fgetc(fp) << 16;
  r |= fgetc(fp) << 24;
  return r;
}

inline static void file_write16(int val, FILE *fp)
{
  fputc((val >> 0) & 0xFF, fp);
  fputc((val >> 8) & 0xFF, fp);
}

inline static int file_read16(FILE *fp)
{
  unsigned int r = 0;
  r |= fgetc(fp);
  r |= fgetc(fp) << 8;
  return r;
}

typedef struct {
  char path[MAX_PATH+1];
  UT_hash_handle hh;
} InputFile;

typedef struct {
  InputFile *files;

  int w;
  int h;

  int mzm;
  int reuse;
  int noreuse;
  int quiet;

  int c;
  int threshold;

  int offset;
  int exclude_on;
  char exclude[256];

  int blank;

  char output[MAX_PATH - 4 + 1];
  char dither[256];
} Config;

Config *DefaultConfig()
{
  Config *cfg = malloc(sizeof(Config));
  cfg->files = NULL;

  cfg->w = 0;
  cfg->h = 0;

  cfg->mzm = 0;
  cfg->reuse = 0;
  cfg->noreuse = 0;
  cfg->quiet = 0;

  cfg->offset = 0;
  cfg->exclude_on = 0;
  for (int i = 0; i < 256; i++) {
    cfg->exclude[i] = 0;
  }
  cfg->blank = -1;

  cfg->c = 0;
  cfg->threshold = 127;

  cfg->output[0] = '\0';
  cfg->dither[0] = '\0';

  return cfg;
}

int Option(const char *option_name, int arguments, char *argument, int args_left)
{
  if (strcmp(argument, option_name) != 0) return 0;
  if (args_left < arguments) {
    Error("Insufficient arguments for %s", option_name);
  }
  return 1;
}

Config *LoadConfig(int argc, char **argv)
{
  Config *cfg = DefaultConfig();

  char arg_used[argc];
  int i;

  memset(arg_used, 0, argc);

  if (argc >= 1 && strcmp(argv[0], "-help") == 0) {
    Help();
    exit(0);
  }

  for (i = 0; i < argc; i++) {
    if (arg_used[i]) continue;
    if (argv[i][0] == '-') { // Argument
      int args_left = argc - i - 1;
      if (Option("-q", 0, argv[i], args_left)) {
        cfg->quiet = 1;
        continue;
      }
      if (Option("-w", 1, argv[i], args_left)) {
        cfg->w = atoi(argv[i+1]);
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-h", 1, argv[i], args_left)) {
        cfg->h = atoi(argv[i+1]);
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-mzm", 0, argv[i], args_left)) {
        cfg->mzm = 1;
        cfg->reuse = 1;
        continue;
      }
      if (Option("-reuse", 0, argv[i], args_left)) {
        cfg->reuse = 1;
        continue;
      }
      if (Option("-noreuse", 0, argv[i], args_left)) {
        cfg->noreuse = 1;
        continue;
      }
      if (Option("-output", 1, argv[i], args_left)) {
        strncpy(cfg->output, argv[i+1], MAX_PATH-4+1);
        cfg->output[MAX_PATH-4] = '\0';
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-c", 1, argv[i], args_left)) {
        cfg->c = atoi(argv[i+1]);
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-dither", 1, argv[i], args_left)) {
        strncpy(cfg->dither, argv[i+1], 256);
        cfg->output[255] = '\0';
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-threshold", 1, argv[i], args_left)) {
        cfg->threshold = atoi(argv[i+1]);
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-offset", 1, argv[i], args_left)) {
        cfg->offset = atoi(argv[i+1]);
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-blank", 1, argv[i], args_left)) {
        cfg->blank = atoi(argv[i+1]);
        arg_used[i+1] = 1;
        continue;
      }
      if (Option("-exclude", 1, argv[i], args_left)) {
        char *s = argv[i+1];
        char tok[256];
        char op;
        int curr = 0;
        int range_st = -1;
        int range_ed = -1;

        cfg->exclude_on = 1;

        while (*s) {
          if (sscanf(s, "%[0-9]", tok) == 0) break;
          s += strlen(tok);

          cfg->exclude[atoi(tok)] = 1;

          if (curr == 1) {
            curr = 0;
            range_ed = atoi(tok);

            for (int ch = range_st; range_st <= range_ed ? ch <= range_ed : ch >= range_ed; range_st <= range_ed ? ch++ : ch--) {
              cfg->exclude[ch] = 1;
            }
          }

          op = s[0];
          if (op) s++;
          if (op == '-') {
            range_st = atoi(tok);
            curr = 1;
          }
        }

        arg_used[i+1] = 1;
        continue;
      }
    } else { // Filename
      InputFile *file = malloc(sizeof(InputFile));
      strncpy(file->path, argv[i], MAX_PATH);
      file->path[MAX_PATH] = 0;
      HASH_ADD_STR(cfg->files, path, file);
    }
  }

  if (cfg->files == NULL) {
    Usage();
    exit(1);
  }

  return cfg;
}

typedef struct {
  int w, h;
  int channels;
  unsigned char *pixels;
} Image;

Image *CreateImage(int w, int h, int channels)
{
  Image *image = malloc(sizeof(Image));
  image->pixels = malloc(w * h * channels);
  image->w = w;
  image->h = h;
  image->channels = channels;
  return image;
}

Image *LoadImage_RAW(Config *cfg, const char *path)
{
  Image *img;
  FILE *fp;
  int i;
  if ((cfg->w == 0) || (cfg->h == 0)) Error("Nonzero -w # and -h # values must be specified when loading raw images.");

  img = CreateImage(cfg->w, cfg->h, 1);

  fp = fopen(path, "rb");
  if (!fp) Error("Failed to open %s", path);
  for (i = 0; i < cfg->w * cfg->h; i++) {
    img->pixels[i] = fgetc(fp) != 0 ? 255 : 0;
  }
  fclose(fp);

  return img;
}

Image *LoadImage_BMP(Config *cfg, const char *path)
{
  FILE *fp = fopen(path, "rb");
  Image *img;
  char sig_a, sig_b;
  int pixelarray, dibheadersize, w, h, bpp, compression, palette_size;
  int channels, output_channels, padding;
  int byte_offset, bit_offset;
  unsigned char byte_val;
  int i, x, y, b;
  struct {
    unsigned char r, g, b;
  } palette[256];

  if (!fp) Error("Failed to open %s", path);

  sig_a = fgetc(fp);
  sig_b = fgetc(fp);

  if ((sig_a != 'B') || (sig_b != 'M')) Error("BMP header corrupt (%s)", path);
  file_read32(fp);
  file_read32(fp);
  pixelarray = file_read32(fp);

  dibheadersize = file_read32(fp);
  w = file_read32(fp);
  h = file_read32(fp);
  file_read16(fp); // planes
  bpp = file_read16(fp);
  compression = file_read32(fp);
  if (compression != 0) Error("Compressed BMPs are not supported");
  file_read32(fp); // image size
  file_read32(fp); // h res
  file_read32(fp); // v res
  palette_size = file_read32(fp); // colours

  img = NULL;

  if (bpp <= 8) { // Paletted image
    if (palette_size == 0) palette_size = 1<<bpp;

    fseek(fp, dibheadersize + 14, SEEK_SET);

    for (i = 0; i < palette_size; i++) {
      palette[i].b = fgetc(fp);
      palette[i].g = fgetc(fp);
      palette[i].r = fgetc(fp);
      fgetc(fp);
    }

    fseek(fp, pixelarray, SEEK_SET);
    img = CreateImage(w, h, 3);

    for (y = h-1; y >= 0; y--) {
      byte_offset = -1;
      bit_offset = 8;
      byte_val = -1;
      for (x = 0; x < w; x++) {

        int v = 0;
        for (b = 0; b < bpp; b++) {
          if (bit_offset == 8) {
            bit_offset = 0;
            byte_offset++;
            byte_val = fgetc(fp);
          }
          v |= byte_val & (128 >> bit_offset) ? 1 << b : 0;
          bit_offset++;
        }

        img->pixels[(y*w+x)*3+0] = palette[v].r;
        img->pixels[(y*w+x)*3+1] = palette[v].g;
        img->pixels[(y*w+x)*3+2] = palette[v].b;
      }
      byte_offset++;
      while (byte_offset % 4) {
        fgetc(fp);
        byte_offset++;
      }
    }

  } else { // Truecolour image
    channels = bpp / 8;
    output_channels = channels <= 3 ? channels : 3;

    fseek(fp, pixelarray, SEEK_SET);

    img = CreateImage(w, h, output_channels);
    padding = (w * channels + 3) / 4 * 4 - (w * channels);
    for (y = 0; y < h; y++) {
      for (i = 0; i < w; i++) {
        img->pixels[(h - y - 1) * w * output_channels + (i * output_channels) + output_channels - 1] = fgetc(fp); // b
        if (channels >= 2) {
          img->pixels[(h - y - 1) * w * output_channels + (i * output_channels) + output_channels - 2] = fgetc(fp); // g
          if (channels >= 3) {
            img->pixels[(h - y - 1) * w * output_channels + (i * output_channels) + output_channels - 3] = fgetc(fp); // r
            if (channels >= 4) {
              fgetc(fp); // a
            }
          }
        }
      }

      if (padding) fseek(fp, padding, SEEK_CUR);
    }
  }

  fclose(fp);

  return img;
}

Image *LoadImage(Config *cfg, const char *path)
{
  const char *ext = strrchr(path, '.');
  if (ext) {
    if (lc_strcmp(ext, ".bmp") == 0) return LoadImage_BMP(cfg, path);
  }
  return LoadImage_RAW(cfg, path);
}

void FreeImage(Image *img)
{
  free(img->pixels);
  free(img);
}

int Brightness(unsigned char *pixels, int channels)
{
  int sum = 0, i;
  if (channels == 1) return pixels[0];
  if (channels == 3) return ((pixels[0] * 30 + pixels[1] * 59 + pixels[2] * 11) + 50) / 100;

  for (i = 0; i < channels; i++) {
    sum += pixels[i];
  }
  return (sum + (channels / 2)) / channels;
}

typedef struct {
  char method[256];
  int diffuse[1024];
  unsigned w, h, div;
} Diffuse;

Diffuse diffmethods[] = {
{"Floyd-Steinberg", {0, -1, 7,
                    3, 5, 1}, 3, 2, 16},

{"Jarvis-Judice-Ninke", {0, 0, -1, 7, 5,
                         3, 5,  7, 5, 3,
                         1, 3,  5, 3, 1}, 5, 3, 48},

{"Stucki", {0, 0, -1, 7, 5,
            2, 4,  8, 4, 2,
            1, 2,  4, 2, 1}, 5, 3, 42},

{"Burkes", {0, 0, -1, 8, 4,
            2, 4, 8, 4, 2}, 5, 2, 32},

{"Sierra", {0, 0, -1, 5, 3,
            2, 4,  5, 4, 2,
            0, 2,  3, 2, 0}, 5, 3, 32},

{"Stevenson-Arce", {0,  0,  0,  -1, 0,  32, 0,
                    12, 0,  26, 0,  30, 0,  16,
                    0,  12, 0,  26, 0,  12, 0,
                    5,  0,  12, 0,  12, 0,  5}, 7, 4, 200}
};

void QuantiseDiffuse(Image *img, Image *qimg, int threshold, const char *method)
{
  int *err = malloc(img->w * img->h * sizeof(int));
  int method_i = -1;
  unsigned int i;
  int mx, my, x, y;
  Diffuse D;
  int v_noerr, v, wr;
  memset(err, 0, img->w * img->h * sizeof(int));

  for (i = 0; i < sizeof(diffmethods) / sizeof(diffmethods[0]); i++) {
    if (lc_strncmp(method, diffmethods[i].method, strlen(method)) == 0) method_i = i;
  }
  if (method_i == -1) {
    Error("-dither method '%s' not found. Try another setting", method);
  }

  D = diffmethods[method_i];
  mx = 0;
  my = 0;
  for (i = 0; i < D.w * D.h; i++) {
    x = i % D.w;
    y = i / D.w;
    if (D.diffuse[i] == -1) {
      mx = x;
      my = y;
      D.diffuse[i] = 0;
    }
  }
  for (y = 0; y < img->h; y++) {
    for (x = 0; x < img->w; x++) {
      v_noerr = Brightness(img->pixels + (y * img->w + x) * img->channels, img->channels);
      v = v_noerr + err[y * img->w + x] / D.div;
      if (v > 255) v = 255;
      if (v < 0) v = 0;
      wr = v >= threshold ? 255 : 0;
      qimg->pixels[y * img->w + x] = v >= threshold ? 1 : 0;

      if ((v_noerr - wr) != 0) {

        for (i = 0; i < D.w * D.h; i++) {
          int ox = (i % D.w) - mx + x;
          int oy = (i / D.w) - my + y;
          if ((D.diffuse[i] > 0) && (ox >= 0) && (oy >= 0) && (ox < img->w) && (oy < img->h)) {
            err[oy * img->w + ox] += D.diffuse[i] * (v - wr);
          }
        }
      }
    }
  }
  free(err);
}

void QuantiseTheshold(Image *img, Image *qimg, int threshold)
{
  int i, v;
  for (i = 0; i < img->w * img->h; i++) {
    v = Brightness(img->pixels + i * img->channels, img->channels);
    qimg->pixels[i] = v >= threshold ? 1 : 0;
  }
}

Image *Quantise(Config *cfg, Image *img)
{
  Image *qimg = CreateImage(img->w, img->h, 1);

  if (cfg->dither[0] == '\0') {
    QuantiseTheshold(img, qimg, cfg->threshold);
  } else {
    QuantiseDiffuse(img, qimg, cfg->threshold, cfg->dither);
  }
  return qimg;
}

typedef struct {
  int chars;
  unsigned char *data;
} Charset;

Charset *CreateCharset(Image *img)
{
  Charset *cset = malloc(sizeof(Charset));
  int cset_size = (img->w / 8) * (img->h / 14);
  int chr, cy, cx, py, px;
  cset->data = malloc(cset_size * 14);
  memset(cset->data, 0, cset_size * 14);

  cset->chars = cset_size;

  chr = 0;
  for (cy = 0; cy < img->h / 14; cy++) {
    for (cx = 0; cx < img->w / 8; cx++) {
      for (py = 0; py < 14; py++) {
        for (px = 0; px < 8; px++) {
          cset->data[chr * 14 + py] |= img->pixels[(cy * 14 + py) * img->w + (cx * 8 + px)] ? 128 >> px : 0;
        }
      }
      chr++;
    }
  }

  return cset;
}

typedef struct {
  int w, h;
  int *data;
} Mzm;

Mzm *CreateMzm(int w, int h)
{
  Mzm *mzm = malloc(sizeof(Mzm));
  int i;
  mzm->w = w;
  mzm->h = h;
  mzm->data = malloc(w * h * sizeof(int));
  for (i = 0; i < w * h; i++) {
    mzm->data[i] = i;
  }
  return mzm;
}

typedef struct {
  unsigned char data[14];
  int i;
  UT_hash_handle hh;
} Char;

void Reuse(Charset *cset, Mzm *mzm)
{
  Char *clist = NULL;
  Char *c;
  int out_chars = 0;
  int i;
  unsigned char *cdata;
  for (i = 0; i < cset->chars; i++) {
    cdata = cset->data + i * 14;

    HASH_FIND(hh, clist, cdata, sizeof(clist->data), c);
    if (!c) {
      c = malloc(sizeof(Char));
      memcpy(c->data, cdata, 14);
      if (mzm) {
        c->i = mzm->data[i] = out_chars;
      }
      memmove(cset->data + out_chars * 14, cset->data + i * 14, 14);
      HASH_ADD(hh, clist, data, sizeof(clist->data), c);
      out_chars++;
    } else {
      if (mzm) {
        mzm->data[i] = c->i;
      }
    }
  }
  cset->chars = out_chars;
  cset->data = realloc(cset->data, cset->chars * 14);
}

char *OutputPath(const char *input, Config *cfg, const char *ext)
{
  char *output_path = NULL;
  char buf[MAX_PATH+1];
  char *buf_e;
  size_t output_len;
  if (cfg->output[0] == '\0') {
    snprintf(buf, MAX_PATH, "%s", input);
    buf[MAX_PATH] = '\0';
    buf_e = strrchr(buf, '.');
    if (buf_e) *buf_e = '\0';

    output_len = strlen(buf) + strlen(ext) + 1;
    output_path = malloc(output_len);
    snprintf(output_path, output_len, "%s%s", buf, ext);
    output_path[output_len - 1] = '\0';
  } else {
    output_len = strlen(cfg->output) + strlen(ext) + 1;
    output_path = malloc(output_len);
    snprintf(output_path, output_len, "%s%s", cfg->output, ext);
    output_path[output_len - 1] = '\0';
  }
  return output_path;
}

void WriteCharset(const char *path, Charset *cset)
{
  FILE *fp = fopen(path, "wb");
  fwrite(cset->data, 14, cset->chars, fp);
  fclose(fp);
}

void WriteMzm(const char *path, Mzm *mzm)
{
  FILE *fp = fopen(path, "wb");
  int i;
  fwrite("MZM2", 1, 4, fp);

  file_write16(mzm->w, fp);
  file_write16(mzm->h, fp);
  file_write32(0, fp);
  fputc(0, fp); // # of robots
  fputc(1, fp); // storage mode
  fputc(0, fp); // robot storage mode
  fputc(0, fp); // reserved

  for (i = 0; i < mzm->w * mzm->h; i++) {
    fputc(mzm->data[i] & 0xFF, fp);
    fputc(15, fp);
  }
  fclose(fp);
}

static inline int CountBits(const unsigned char *A)
{
  int c, i;
  const unsigned int *a;
  unsigned int v;
  unsigned int count;
  unsigned char val;
  if (sizeof(unsigned int) == 4) {
    c = 0;

    a = (const unsigned int*)A;
    for (i = 0; i < 3; i++) {
      v = *a;
      v = v - ((v >> 1) & 0x55555555);
      v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
      c += (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
      a++;
    }
    v = (A[13] << 8 | A[12]);
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    c += (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;

    return c;
  } else {
    count = 0;
    for (int i = 0; i < 14; i++) {
      val = A[i];
      for (; val; count++)
      {
        val &= val - 1; // clear the least significant bit set
      }
    }
    return count;
  }
  return -1;
}

static inline int CharDist(const unsigned char *A, const unsigned char *B)
{
  unsigned char v[14];
  int i;
  for (i = 0; i < 14; i++) {
    v[i] = A[i] ^ B[i];
  }
  return CountBits(v);
}

// Simple quantisation- combine the two closest pairs of chars until the right number of chars are available
void Reduce_Simple(Charset *cset, int chars)
{
  int pair_i, pair_j, pair_dist, data_dist;
  int i, j;
  unsigned char *data_i, *data_j;
  int bits;

  while (cset->chars > chars)
  {
    pair_i = -1;
    pair_j = -1;
    pair_dist = INT_MAX;

    for (i = 0; i < cset->chars; i++) {
      data_i = cset->data + i * 14;
      for (j = i + 1; j < cset->chars; j++) {
        data_j = cset->data + j * 14;

        data_dist = CharDist(data_i, data_j);
        if (data_dist < pair_dist) {
          pair_dist = data_dist;
          pair_i = i;
          pair_j = j;
        }
      }
    }

    data_i = cset->data + pair_i * 14;
    data_j = cset->data + pair_j * 14;
    bits = CountBits(data_i) + CountBits(data_j);
    if (bits >= 112) {
      for (i = 0; i < 14; i++) {
        //data_i[i] = data_i[i] > data_j[i] ? data_i[i] : data_j[i];
        data_i[i] = data_i[i] | data_j[i];
      }
    } else {
      for (i = 0; i < 14; i++) {
        data_i[i] = ~(~data_i[i] | ~data_j[i]);
      }
    }

    memmove(data_j, data_j + 14, (cset->chars - pair_j - 1) * 14);
    cset->chars--;
  }

  cset->data = realloc(cset->data, cset->chars * 14);
}

int ChrImageDist(const unsigned char *cdata, Image *img, int xoff, int yoff)
{
  int tdist = 0;
  int x, y, px, py, chr_lightness, img_lightness;
  for (y = 0; y < 14; y++) {
    for (x = 0; x < 8; x++) {
      px = xoff + x;
      py = yoff + y;
      chr_lightness = cdata[y] & (128 >> x) ? 1 : 0;
      img_lightness = img->pixels[((py * img->w) + px)] ? 1 : 0;
      tdist += abs(chr_lightness - img_lightness);
    }
  }
  return tdist;
}

void RemapMzm(Charset *cset, Mzm *mzm, Image *qimage)
{
  // Remap the MZM to the charset to create the closest approximation of the quantised image.
  int x, y, c;
  for (y = 0; y < mzm->h; y++) {
    for (x = 0; x < mzm->w; x++) {
      int closest_chr = -1;
      int closest_dist = INT_MAX;

      for (c = 0; c < cset->chars; c++) {
        int chr_dist = ChrImageDist(cset->data + c * 14, qimage, x * 8, y * 14);
        if (chr_dist < closest_dist) {
          closest_dist = chr_dist;
          closest_chr = c;
        }
      }
      mzm->data[y * mzm->w + x] = closest_chr;
    }
  }
}

void Reduce(Charset *cset, Mzm *mzm, Image *original, int chars, Config *cfg)
{
  if (cset->chars <= chars) return;

  Reduce_Simple(cset, chars);
  RemapMzm(cset, mzm, original);
}

void OffsetMzm(Mzm *mzm, int offset)
{
  int i;
  if (mzm) {
    for (i = 0; i < mzm->w * mzm->h; i++) {
      mzm->data[i] += offset;
    }
  }
}

void ExcludeChars(Charset *cset, Mzm *mzm, char *exclude, int offset)
{
  int remap[256];
  int remap_offset = 0;
  int ci, i;
  for (i = 0; i < cset->chars && i < 256; i++) {
    remap[i] = i;
    ci = i + remap_offset + offset;
    if ((ci >= 0) && (ci < 256) && (exclude[ci])) {
      cset->data = realloc(cset->data, (cset->chars + 1) * 14);
      remap[i] = cset->chars;
      memmove(cset->data + (remap[i])*14, cset->data + i * 14, 14);
      memset(cset->data + i * 14, 0, 14);

      cset->chars++;
    }
  }

  if (mzm) {
    for (i = 0; i < mzm->w * mzm->h; i++) {
      while ((mzm->data[i] != remap[mzm->data[i]])) {
        mzm->data[i] = remap[mzm->data[i]];
      }
    }
  }
}

void SwapBlank(Charset *cset, Mzm *mzm, int blank)
{
  int current_blank = -1;
  int i, j, isblank;
  unsigned char tmp[14];
  for (i = 0; i < cset->chars; i++) {
    isblank = 1;
    for (j = 0; j < 14; j++) {
      if (cset->data[i * 14 + j] != 0) {
        isblank = 0;
        break;
      }
    }

    if (isblank) current_blank = i;
    break;
  }
  if (current_blank != -1) {
    memcpy(tmp, cset->data + current_blank*14, 14);
    memcpy(cset->data + current_blank*14, cset->data + blank*14, 14);
    memcpy(cset->data + blank*14, tmp, 14);

    if (mzm) {
      for (i = 0; i < mzm->w * mzm->h; i++) {
        if (mzm->data[i] == current_blank) {
          mzm->data[i] = blank;
        } else if (mzm->data[i] == blank) {
          mzm->data[i] = current_blank;
        }
      }
    }
  }
}

int main(int argc, char **argv)
{
  Config *cfg = LoadConfig(argc - 1, argv + 1);
  InputFile *file;
  Image *image, *qimage;
  Charset *cset;
  Mzm *mzm;

#ifdef CONFIG_PLEDGE_UTILS
  // TODO unveil
  if(pledge(PROMISES, ""))
  {
    Error("Failed pledge!");
    return 1;
  }
#endif

  for(file = cfg->files; file != NULL; file = file->hh.next) {
    image = LoadImage(cfg, file->path);

    if (((image->w % 8) != 0) || ((image->h % 14) != 0)) Error("Image dimensions are not divisible by 8x14");
    qimage = Quantise(cfg, image);

    cset = CreateCharset(qimage);

    mzm = NULL;
    if (cfg->mzm) {
      mzm = CreateMzm(qimage->w / 8, qimage->h / 14);
    }

    if (cfg->reuse && !cfg->noreuse) {
      Reuse(cset, mzm);
    }

    if (cfg->c) {
      Reduce(cset, mzm, qimage, cfg->c, cfg);
    }

    if (cfg->blank != -1) {
      SwapBlank(cset, mzm, cfg->blank);
    }

    if (cfg->exclude_on) {
      ExcludeChars(cset, mzm, cfg->exclude, cfg->offset);
    }


    if (cfg->offset) {
      OffsetMzm(mzm, cfg->offset);
    }


    { // Output charset
      char *filename_chr = OutputPath(file->path, cfg, ".chr");

      WriteCharset(filename_chr, cset);

      if (!cfg->quiet) {
        printf("%s -> %s (%d chars)", file->path, filename_chr, cset->chars);
      }
      free(filename_chr);
    }

    if (cfg->mzm) { // Output MZM
      char *filename_mzm = OutputPath(file->path, cfg, ".mzm");
      WriteMzm(filename_mzm, mzm);

      if (!cfg->quiet) {
        printf(", %s", filename_mzm);
      }

      free(filename_mzm);
    }

    if (!cfg->quiet) printf("\n");
  }

  return 0;
}
