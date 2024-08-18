/* MegaZeux
 *
 * Copyright (C) 2020, 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef UNIT_IO_HPP
#define UNIT_IO_HPP

#include "Unit.hpp"

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <zlib.h>

namespace unit
{
  class io final
  {
    FILE *file_handle = nullptr;
    size_t file_length;

    io(const char *path)
    {
      file_handle = fopen_unsafe(path, "rb");
      ASSERT(file_handle, "couldn't load file: %s", path);

      if(fseek(file_handle, 0, SEEK_END) < 0)
        FAIL("failed to seek to end: %s", path);

#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
      int64_t len = ftello(file_handle);
#else
      long len = ftell(file_handle);
#endif
      ASSERT(len >= 0, "failed ftell: %s", path);
      file_length = len;

      rewind(file_handle);
    }

    ~io()
    {
      fclose(file_handle);
    }

  public:
    static uint16_t read16le(const uint8_t *src)
    {
      return src[0] | (src[1] << 8u);
    }
    static uint16_t read16le(const std::vector<uint8_t> &src, size_t off)
    {
      return read16le(src.data() + off);
    }
    static void put16le(std::vector<uint8_t> &dest, uint16_t value)
    {
      dest.push_back(value & 0xff);
      dest.push_back(value >> 8);
    }

    static uint32_t read32le(const uint8_t *src)
    {
      return src[0] | (src[1] << 8u) | (src[2] << 16u) | (src[3] << 24u);
    }
    static uint32_t read32le(const std::vector<uint8_t> &src, size_t off)
    {
      return read32le(src.data() + off);
    }
    static void put32le(std::vector<uint8_t> &dest, uint32_t value)
    {
      dest.push_back(value & 0xff);
      dest.push_back(value >> 8);
      dest.push_back(value >> 16);
      dest.push_back(value >> 24);
    }

    static void put_raw(std::vector<uint8_t> &dest, const void *src, size_t bytes)
    {
      const uint8_t *pos = reinterpret_cast<const uint8_t *>(src);
      dest.insert(dest.end(), pos, pos + bytes);
    }

    static std::vector<uint8_t> load(const char *path)
    {
      io in(path);

      std::vector<uint8_t> out(in.file_length);

      if(fread(out.data(), 1, in.file_length, in.file_handle) < in.file_length)
        FAIL("failed to read file data: %s", path);

      return out;
    }

    template<typename T>
    static T *load_buffer(T *buffer, size_t *buffer_len, const char *path)
    {
      io in(path);
      ASSERT(in.file_length <= *buffer_len, "buffer too small: %s", path);

      if(fread(buffer, 1, in.file_length, in.file_handle) < in.file_length)
        FAIL("failed to read file data: %s", path);

      *buffer_len = in.file_length;
      return buffer;
    }

    template<typename T>
    static T *load_buffer(T *buffer, size_t buffer_len, const char *path)
    {
      return load_buffer(buffer, &buffer_len, path);
    }

    static void save(const std::vector<uint8_t> &src, const char *path)
    {
      FILE *out = fopen_unsafe(path, "wb");
      ASSERT(out, "failed fopen");
      size_t sz = fwrite(src.data(), 1, src.size(), out);
      fclose(out);
      ASSERTEQ(sz, src.size(), "failed fwrite");
    }

    static std::vector<uint8_t> load_compressed(const char *path);
    static void save_compressed(const std::vector<uint8_t> &src, const char *path);

    static void save_tga(
     const std::vector<uint8_t> &pixels, unsigned w, unsigned h,
     const uint32_t *palette, unsigned colors, const char *path);
    static void save_tga(
     const std::vector<uint16_t> &pixels, unsigned w, unsigned h,
     const uint32_t *unused, unsigned unused2, const char *path);
    static void save_tga(
     const std::vector<uint32_t> &pixels, unsigned w, unsigned h,
     const uint32_t *unused, unsigned unused2, const char *path);

    template<typename T>
    static std::vector<T> load_tga(const char *path);
  };
  template<> std::vector<uint8_t> io::load_tga<uint8_t>(const char *);
  template<> std::vector<uint16_t> io::load_tga<uint16_t>(const char *);
  template<> std::vector<uint32_t> io::load_tga<uint32_t>(const char *);
}

#endif /* UNIT_IO_HPP */
