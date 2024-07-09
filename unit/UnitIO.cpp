/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "UnitIO.hpp"

#include <vector>
#include <zlib.h>

std::vector<uint8_t> unit::io::load_compressed(const char *path)
{
  std::vector<uint8_t> compressed = load(path);
  ASSERT(compressed.size() > 18, "not gzip");
  uint32_t uncompressed_size =
   read32le(compressed.data() + compressed.size() - 4);

  std::vector<uint8_t> out(uncompressed_size);

  z_stream zs{};
  zs.avail_out = uncompressed_size;
  zs.next_out = out.data();
  zs.avail_in = compressed.size();
  zs.next_in = compressed.data();

  int ret = inflateInit2(&zs, MAX_WBITS | 16);
  ASSERTEQ(ret, Z_OK, "failed inflate init");
  ret = inflate(&zs, Z_FINISH);
  ASSERTEQ(ret, Z_STREAM_END, "failed inflate");
  inflateEnd(&zs);

  return out;
}

void unit::io::save_compressed(const std::vector<uint8_t> &src, const char *path)
{
  size_t path_len = strlen(path);
  ASSERTCMP(path + path_len - 3, ".gz", "filename must end in .gz");

  z_stream zs{};
  zs.avail_in = src.size();
  zs.next_in = (Bytef *)src.data();

  int ret = deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED,
   MAX_WBITS | 16, 9, Z_DEFAULT_STRATEGY);
  ASSERTEQ(ret, Z_OK, "failed deflate init");

  size_t compressed_bound = deflateBound(&zs, src.size());

  std::vector<uint8_t> compressed(compressed_bound);

  zs.avail_out = compressed_bound;
  zs.next_out = compressed.data();

  ret = deflate(&zs, Z_FINISH);
  ASSERTEQ(ret, Z_STREAM_END, "failed deflate");

  size_t total_len = compressed_bound - zs.avail_out;
  deflateEnd(&zs);

  FILE *fp = fopen_unsafe(path, "wb");
  ASSERT(fp, "failed fopen");
  size_t out_len = fwrite(compressed.data(), 1, total_len, fp);
  ASSERTEQ(out_len, total_len, "failed fwrite");
  fclose(fp);
}

/* TGA writer: always RLE, outputs with gzip compression if filename is .gz */
template<typename T>
static void write_tga(const std::vector<T> &pixels, unsigned w, unsigned h,
 const uint32_t *flat_palette, unsigned colors, const char *file)
{
  static constexpr int is_indexed = sizeof(T) == 1;
  static constexpr int alphabits = sizeof(T) == 2 ? 1 : 8;

  std::vector<uint8_t> out;
  out.reserve(w * h * sizeof(T));

  out.push_back(0);
  out.push_back(is_indexed ? 1 : 0);
  out.push_back(is_indexed ? 9 : 10);
  unit::io::put16le(out, 0);
  unit::io::put16le(out, is_indexed ? colors : 0);
  out.push_back(is_indexed ? 32 : 0);
  unit::io::put16le(out, 0);
  unit::io::put16le(out, h);
  unit::io::put16le(out, w);
  unit::io::put16le(out, h);
  out.push_back(sizeof(T) * 8);
  out.push_back(0x20 | alphabits);

  if(is_indexed)
    unit::io::put_raw(out, flat_palette, colors * sizeof(uint32_t));

  size_t offset = 0;
  for(size_t y = 0; y < h; y++)
  {
    for(size_t x = 0; x < w; )
    {
      size_t num = 1;
      size_t start = offset;
      T color = pixels[offset++];
      x++;
      while(x < w && num < 128 && pixels[offset] == color)
        offset++, x++, num++;
      if(num > 1)
      {
        out.push_back(0x80 | (num - 1));
        unit::io::put_raw(out, &color, sizeof(T));
        continue;
      }

      while(x < w && num < 128 &&
       !(x + 1 < w && pixels[offset] == pixels[offset + 1]))
        offset++, x++, num++;

      out.push_back(num - 1);
      unit::io::put_raw(out, pixels.data() + start, num * sizeof(T));
    }
  }
  unit::io::put32le(out, 0);
  unit::io::put32le(out, 0);
  unit::io::put_raw(out, "TRUEVISION-XFILE.", 18);

  ssize_t ext_pos = strlen(file) - 3;
  if(ext_pos >= 0 && !strcmp(file + ext_pos, ".gz"))
    unit::io::save_compressed(out, file);
  else
    unit::io::save(out, file);
}

void unit::io::save_tga(
 const std::vector<uint8_t> &pixels, unsigned w, unsigned h,
 const uint32_t *palette, unsigned colors, const char *path)
{
  write_tga(pixels, w, h, palette, colors, path);
}

void unit::io::save_tga(
 const std::vector<uint16_t> &pixels, unsigned w, unsigned h,
 const uint32_t *unused, unsigned unused2, const char *path)
{
  write_tga(pixels, w, h, nullptr, 0, path);
}

void unit::io::save_tga(
 const std::vector<uint32_t> &pixels, unsigned w, unsigned h,
 const uint32_t *unused, unsigned unused2, const char *path)
{
  write_tga(pixels, w, h, nullptr, 0, path);
}

/* TGA loader: only returns raw image data.
 * Currently only loads the subset of TGAs that are saved by save_tga. */
template<typename T>
static std::vector<T> load_tga(const char *file)
{
  static constexpr int is_indexed = sizeof(T) == 1;
  static constexpr int alphabits = sizeof(T) == 2 ? 1 : 8;

  std::vector<uint8_t> image;
  std::vector<T> pixels;
  size_t offset = 18;

  ssize_t ext_pos = strlen(file) - 3;
  if(ext_pos >= 0 && !strcmp(file + ext_pos, ".gz"))
    image = unit::io::load_compressed(file);
  else
    image = unit::io::load(file);

  ASSERT(image.size() > 44, "minimum size check failed");
  ASSERTMEM(image.data() + image.size() - 18,
   "TRUEVISION-XFILE.", 18, "magic check failed");

  uint16_t w = unit::io::read16le(image.data() + 12);
  uint16_t h = unit::io::read16le(image.data() + 14);

  if(is_indexed)
  {
    uint16_t colors = unit::io::read16le(image.data() + 5);
    ASSERTEQ(image[1], 1, "not indexed");
    ASSERTEQ(image[2], 9, "not indexed RLE");
    ASSERT(colors > 0, "no colors");
    ASSERTEQ(image[7], 32, "not 32bpp indexed");
    offset += colors * 4;
  }
  else
  {
    ASSERTEQ(image[1], 0, "indexed");
    ASSERTEQ(image[2], 10, "not truecolor RLE");
  }
  ASSERT(w != 0, "bad width");
  ASSERT(h != 0, "bad height");
  ASSERTEQ(image[16], sizeof(T) * 8, "depth mismatch");
  ASSERTEQ(image[17] & 0xf0, 0x20, "not top-to-bottom left-to-right");
  ASSERTEQ(image[17] & 0x0f, alphabits, "wrong alphabits");

  size_t total_px = (size_t)w * h;
  pixels.resize(total_px * sizeof(T));

  size_t end = image.size() - 18;
  size_t pos = 0;
  while(offset < end && pos < total_px)
  {
    size_t num = (image[offset] & 0x7f) + 1;
    ASSERT(num < total_px && pos <= total_px - num, "bad RLE");
    if(image[offset++] & 0x80)
    {
      T color;
      memcpy(&color, image.data() + offset, sizeof(T));
      offset += sizeof(T);
      for(size_t i = 0; i < num; i++)
        pixels[pos++] = color;
    }
    else
    {
      memcpy(pixels.data() + pos, image.data() + offset,
       sizeof(T) * num);
      offset += sizeof(T) * num;
      pos += num;
    }
  }
  return pixels;
}

template<>
std::vector<uint8_t> unit::io::load_tga<uint8_t>(const char *path)
{
  return ::load_tga<uint8_t>(path);
}

template<>
std::vector<uint16_t> unit::io::load_tga<uint16_t>(const char *path)
{
  return ::load_tga<uint16_t>(path);
}

template<>
std::vector<uint32_t> unit::io::load_tga<uint32_t>(const char *path)
{
  return ::load_tga<uint32_t>(path);
}
