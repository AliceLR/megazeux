/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <memory>

#include "../Unit.hpp"

#include "../../src/util.h"
#include "../../src/io/path.h"
#include "../../src/io/zip.h"
#include "../../src/io/zip_stream.h"

static const size_t BUFFER_SIZE = (1 << 17);
static const char DATA_DIR[] = "../data";

struct pair
{
  const char *data;
  size_t len;
  uint16_t flags;
};

struct zip_stream_test_data
{
  pair expected;
  pair deflate;
  pair deflate64;
  pair shrink;
  pair reduce1;
  pair reduce2;
  pair reduce3;
  pair reduce4;
  pair implode;
  boolean inputs_are_paths;
  boolean need_free;
  boolean expected_is_base64;
  boolean compressed_is_base64;
};

static zip_stream_test_data data[] =
{
  // WHITE.PAL from Fred the Freak Gaiden was originally shrunk.
  {
    { "????????????????????????????????????????????????", 48, 0 },
    { "sycRAAA=", 5, 0 },
    { "sycRAAA=", 5, 0 },
    { "PwIKHEiwoMGDCAUC", 12, 0 },
    {},
    {},
    {},
    {},
    { "DwASAyQVNic4OWp7TJ1uHwkGARM05faW93/+CAZgAw==", 31, 0 },
    false,
    false,
    false,
    true
  },
  // FRED_01.PAL from Fred the Freak Gaiden was originally shrunk.
  {
    { "AAAAAAAqACoAGz8bBz8HAjQCKhUAKioqFRUVFRU/FT8VFT8/PxUVPxU/MzMJPz8/", 48, 0 },
    { "YwABLSCUtpdmt2dnMmHSEmXQ0tISBQF7IBS1t7cHs4yNOYFMAA==", 37, 0 },
    { "YwABLSCUtpdmt2dnMmHSEmXQ0tISBQF7IBS1t7cHs4yNOYFMAA==", 37, 0 },
    { "AAIGVAGA4IYfGw78OCCAhgAVFQqqgFihYoUfFyv+2Kjx4owZCTb+AA==", 40, 0 },
    {},
    {},
    {},
    {},
    {},
    false,
    false,
    true,
    true
  },
  // FRED_02.PAL from Fred the Freak Gaiden was originally shrunk.
  {
    { "AAAAAAAqACoAACoqKgAAKgAqBwcHDw8PCQkJCgoKFT8VFT8/FSoVKhUAPz8VPz8/", 48, 0 },
    { "HYaxEQBACMLsPG2oWIIl2H+r502KpD6KJemuuwHMzO7SpE3FSm0/", 39, 0 },
    { "HYaxEQBACMLsPG2oWIIl2H+r502KpD6KJemuuwHMzO7SpE3FSm0/", 39, 0 },
    { "AAIGVAGAYEEVBgmqOMDwgcMEEBVIrPCjAsWKKipkBPCjYscf", 36, 0 },
    {},
    {},
    {},
    {},
    {},
    false,
    false,
    true,
    true
  },
  // CN_S.CHR from Dark Nova was originally imploded.
  {
    { "CN_S.CHR",           3584, 0 },
    { "CN_S.CHR.deflate",   1618, 0 },
    { "CN_S.CHR.deflate",   1618, 0 }, // Result is same as above.
    { "CN_S.CHR.shrink",    1906, 0 },
    { "CN_S.CHR.reduce1",   2123, 0 },
    { "CN_S.CHR.reduce2",   2134, 0 },
    { "CN_S.CHR.reduce3",   2147, 0 },
    { "CN_S.CHR.reduce4",   2138, 0 },
    { "CN_S.CHR.implode",   1793, 0x0000 },
    true,
    false,
    false,
    false
  },
  // FREAKSOF.MZX from Freaks Collection was originally imploded.
  {
    { "FREAKSOF.MZX",       16525, 0 },
    { "freaksof.deflate",   6132, 0 },
    { "freaksof.deflate64", 6137, 0 },
    { "freaksof.shrink",    9425, 0 },
    { "freaksof.reduce1",   7600, 0 },
    { "freaksof.reduce2",   7417, 0 },
    { "freaksof.reduce3",   7386, 0 },
    { "freaksof.reduce4",   7367, 0 },
    { "freaksof.implode",   6586, 0x0000 },
    true,
    false,
    false,
    false
  },
  {
    { "dch1.txt",           7045, 0 },
    { "dch1.deflate",       3385, 0 },
    { "dch1.deflate64",     3352, 0 },
    { "dch1.shrink",        3980, 0 },
    { "dch1.reduce1",       4549, 0 },
    { "dch1.reduce2",       4505, 0 },
    { "dch1.reduce3",       4459, 0 },
    { "dch1.reduce4",       4411, 0 },
    { "dch1.implode",       3666, 0x0006 },
    true,
    false,
    false,
    false
  },
  {
    { "CT_LEVEL.MOD",       111885, 0 },
    { "ct_level.deflate",   61105, 0 },
    { "ct_level.deflate64", 61051, 0 },
    { "ct_level.shrink",    80628, 0 },
    { "ct_level.reduce1",   67216, 0 },
    { "ct_level.reduce2",   66109, 0 },
    { "ct_level.reduce3",   66077, 0 },
    { "ct_level.reduce4",   65837, 0 },
    { "ct_level.implode",   66879, 0x0000 },
    true,
    false,
    false,
    false
  },
};

static void debase64(char *dest, size_t dest_len, const char *_src, size_t src_len)
{
  static const char lut[256] =
  {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62,0, 0, 0, 63,
    52,53,54,55,56,57,58,59,60,61,0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,0, 0, 0, 0, 0,
    0, 26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,0, 0, 0, 0, 0,
  };
  const uint8_t *src = (const uint8_t *)_src;
  size_t i;
  size_t j;

  assert(dest_len >= (src_len * 3 / 4));
  assert((src_len & 3) == 0);

  for(i = 0, j = 0; i < src_len; i += 4)
  {
    uint32_t x = lut[src[i+3]] | (lut[src[i+2]] << 6) |
     (lut[src[i+1]] << 12) | (lut[src[i]] << 18);

    dest[j++] = (x >> 16) & 0xFF;
    dest[j++] = (x >> 8) & 0xFF;
    dest[j++] = x & 0xFF;
  }
}

static char *load_file(const char *path)
{
  char buffer[MAX_PATH];
  if(path_join(buffer, arraysize(buffer), DATA_DIR, path) < 1)
    return nullptr;

  FILE *fp = fopen_unsafe(buffer, "rb");
  if(!fp)
    return nullptr;

  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  rewind(fp);

  char *data = (char *)cmalloc(len + 1);
  assert(fread(data, len, 1, fp));
  fclose(fp);

  data[len] = '\0';
  return data;
}

static char *load_file(const char *path, char *buffer, size_t *buffer_len)
{
  char path_buffer[MAX_PATH];
  if(path_join(path_buffer, arraysize(path_buffer), DATA_DIR, path) < 1)
    return nullptr;

  FILE *fp = fopen_unsafe(path_buffer, "rb");
  if(!fp)
    return nullptr;

  fseek(fp, 0, SEEK_END);
  size_t file_len = ftell(fp);
  rewind(fp);

  assert(file_len <= *buffer_len);
  *buffer_len = file_len;
  int fread_ret = fread(buffer, file_len, 1, fp);
  assert(fread_ret);
  fclose(fp);
  return buffer;
}

static void check_data(zip_stream_test_data &data)
{
  if(data.inputs_are_paths)
  {
    data.expected.data = (const char *)load_file(data.expected.data);
    data.deflate.data = (const char *)load_file(data.deflate.data);
    data.deflate64.data = (const char *)load_file(data.deflate64.data);
    data.shrink.data = (const char *)load_file(data.shrink.data);
    data.reduce1.data = (const char *)load_file(data.reduce1.data);
    data.reduce2.data = (const char *)load_file(data.reduce2.data);
    data.reduce3.data = (const char *)load_file(data.reduce3.data);
    data.reduce4.data = (const char *)load_file(data.reduce4.data);
    data.implode.data = (const char *)load_file(data.implode.data);
    data.expected_is_base64 = false;
    data.compressed_is_base64 = false;
    data.need_free = true;
  }
}

static void check_data()
{
  static boolean loaded = false;
  if(!loaded)
  {
    loaded = true;
    for(int i = 0; i < arraysize(data); i++)
      check_data(data[i]);
  }
}

static zip_method_handler *get_stream(zip_compression_method method)
{
  assert(method <= arraysize(zip_method_handlers));
  return zip_method_handlers[method];
}

static enum zip_error compress(zip_method_handler *stream,
 zip_compression_method method, uint16_t flags,
 const char *in, size_t in_len, char *out, size_t out_len, size_t *final_out)
{
  uint8_t buffer[ZIP_STREAM_DATA_ALLOC_SIZE];
  zip_stream_data *sd = (zip_stream_data *)buffer;
  enum zip_error result;

  assert(in);
  assert(out);
  assert(final_out);
  assert(stream->compress_open);
  assert(stream->compress_block || stream->compress_file);
  assert(stream->input);
  assert(stream->output);
  assert(stream->close);

  stream->compress_open(sd, method, flags);
  stream->input(sd, in, in_len);
  stream->output(sd, out, out_len);

  if(stream->compress_block)
    result = stream->compress_block(sd);
  else
    result = stream->compress_file(sd);

  stream->close(sd, nullptr, final_out);
  return result;
}

static enum zip_error decompress(zip_method_handler *stream,
 zip_compression_method method, uint16_t flags,
 const char *in, size_t in_len, char *out, size_t out_len)
{
  uint8_t buffer[ZIP_STREAM_DATA_ALLOC_SIZE];
  zip_stream_data *sd = (zip_stream_data *)buffer;
  enum zip_error result;

  assert(in);
  assert(out);
  assert(stream->decompress_open);
  assert(stream->decompress_block || stream->decompress_file);
  assert(stream->input);
  assert(stream->output);
  assert(stream->close);

  stream->decompress_open(sd, method, flags);
  stream->input(sd, in, in_len);
  stream->output(sd, out, out_len);

  if(stream->decompress_block)
    result = stream->decompress_block(sd);
  else
    result = stream->decompress_file(sd);

  size_t final_len = 0;
  stream->close(sd, nullptr, &final_len);
/*
  char buf[32];
  snprintf(buf, 32, "%d.in", method);
  FILE *fp = fopen_unsafe(buf, "wb");
  fwrite(in, in_len, 1, fp);
  fclose(fp);
  snprintf(buf, 32, "%d.out", method);
  fp = fopen_unsafe(buf, "wb");
  fwrite(out, final_len, 1, fp);
  fclose(fp);
*/
  return result;
}

#define SET_A(a,a_len,p,is_b64) \
  a = buffer_a; \
  a_len = p.len; \
  ASSERTX(p.data, desc); \
  if(is_b64) \
    debase64(buffer_a, BUFFER_SIZE, p.data, strlen(p.data)); \
  else \
    memcpy(buffer_a, p.data, a_len);

#define SET_B(b,b_len,p,is_b64) \
  b = buffer_b; \
  b_len = p.len; \
  flags = p.flags; \
  if(!p.data) \
    continue; \
  if(is_b64) \
    debase64(buffer_b, BUFFER_SIZE, p.data, strlen(p.data)); \
  else \
    memcpy(buffer_b, p.data, b_len);

/**
 * Tests:
 * 1) a valid input should decompress to the exact expected output.
 * 2) a truncated input should generally fail (file) or return ZIP_INPUT_EMPTY (block).
 * 3) corrupted inputs (simulated by copying parts of the expected output over
 *    parts of the input) should just plain fail or produce a different output.
 */
#define DECOMPRESS_BOILERPLATE(field,method) \
  do \
  { \
    for(int _i = 0; _i < arraysize(data); _i++) \
    { \
      zip_stream_test_data &d = data[_i]; \
      snprintf(desc, arraysize(desc), "%d valid (%zu)", _i, d.expected.len); \
      SET_A(a, a_len, d.expected, d.expected_is_base64); \
      SET_B(b, b_len, d.field, d.compressed_is_base64); \
      result = decompress(stream, method, flags, b, b_len, \
       buffer, BUFFER_SIZE); \
      ASSERTEQX(result, ZIP_SUCCESS, desc); \
      cmp = memcmp(a, buffer, a_len); \
      ASSERTEQX(cmp, 0, desc); \
      for(int _j = 7; _j >= 0; _j--) \
      { \
        snprintf(desc, arraysize(desc), "%d truncated %d/8 (%zu)", _i, _j, d.expected.len); \
        memset(buffer, 0, BUFFER_SIZE); \
        result = decompress(stream, method, flags, b, b_len * _j / 8, \
         buffer, BUFFER_SIZE); \
        ASSERTX(result != ZIP_OUTPUT_FULL, desc); \
        ASSERTX(result != ZIP_EOF, desc); \
        if(result == ZIP_SUCCESS) \
        { \
          cmp = !memcmp(a, buffer, a_len); \
          ASSERTEQX(cmp, 0, desc); \
        } \
      } \
      for(int _j = 0; _j < 8; _j++) \
      { \
        snprintf(desc, arraysize(desc), "%d corrupt %d (%zu)", _i, _j, d.expected.len); \
        memset(buffer, 0, BUFFER_SIZE); \
        SET_B(b, b_len, d.field, d.compressed_is_base64); \
        size_t size = MAX(b_len / 8, 1); \
        if(size > a_len) \
          continue; \
        size_t a_pos = MIN(a_len - size, _j * size); \
        size_t b_pos = MIN(b_len - size, _j * size); \
        if(!memcmp(buffer_b + b_pos, a + a_pos, size)) \
          continue;\
        memcpy(buffer_b + b_pos, a + a_pos, size); \
        result = decompress(stream, method, flags, b, b_len, \
         buffer, BUFFER_SIZE); \
        ASSERTX(result != ZIP_EOF, desc); \
        if(result == ZIP_SUCCESS) \
        { \
          cmp = !memcmp(a, buffer, a_len); \
          ASSERTEQX(cmp, 0, desc); \
        } \
      } \
    } \
  } while(0)


UNITTEST(Decompress)
{
  std::unique_ptr<char> _buffer(new char[BUFFER_SIZE]);
  std::unique_ptr<char> _buffer_a(new char[BUFFER_SIZE]);
  std::unique_ptr<char> _buffer_b(new char[BUFFER_SIZE]);
  char *buffer = _buffer.get();
  char *buffer_a = _buffer_a.get();
  char *buffer_b = _buffer_b.get();
  char desc[64];
  const char *a;
  const char *b;
  size_t a_len;
  size_t b_len;
  uint16_t flags;
  enum zip_error result;
  int cmp;

  ASSERT(buffer && buffer_a && buffer_b);
  check_data();

  SECTION(Inflate)
  {
    zip_method_handler *stream = get_stream(ZIP_M_DEFLATE);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(deflate, ZIP_M_DEFLATE);
    }
    else
      FAIL("Failed to get inflate stream!");
  }

  SECTION(Inflate64)
  {
    zip_method_handler *stream = get_stream(ZIP_M_DEFLATE64);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(deflate64, ZIP_M_DEFLATE64);
    }
    else
      SKIP();
  }

  SECTION(Unshrink)
  {
    zip_method_handler *stream = get_stream(ZIP_M_SHRUNK);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(shrink, ZIP_M_SHRUNK);
    }
    else
      SKIP();
  }

  SECTION(Expand1)
  {
    zip_method_handler *stream = get_stream(ZIP_M_REDUCED_1);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(reduce1, ZIP_M_REDUCED_1);
    }
    else
      SKIP();
  }

  SECTION(Expand2)
  {
    zip_method_handler *stream = get_stream(ZIP_M_REDUCED_2);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(reduce2, ZIP_M_REDUCED_2);
    }
    else
      SKIP();
  }

  SECTION(Expand3)
  {
    zip_method_handler *stream = get_stream(ZIP_M_REDUCED_3);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(reduce3, ZIP_M_REDUCED_3);
    }
    else
      SKIP();
  }

  SECTION(Expand4)
  {
    zip_method_handler *stream = get_stream(ZIP_M_REDUCED_4);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(reduce4, ZIP_M_REDUCED_4);
    }
    else
      SKIP();
  }

  SECTION(Explode)
  {
    zip_method_handler *stream = get_stream(ZIP_M_IMPLODED);
    if(stream)
    {
      DECOMPRESS_BOILERPLATE(implode, ZIP_M_IMPLODED);
    }
    else
      SKIP();
  }
}

/**
 * Compress each of the raw test inputs, then decompress it and check to
 * make sure it's the same.
 */
UNITTEST(Compress)
{
  std::unique_ptr<char> _buffer_a(new char[BUFFER_SIZE]);
  std::unique_ptr<char> _buffer_cmp(new char[BUFFER_SIZE]);
  std::unique_ptr<char> _buffer_dcmp(new char[BUFFER_SIZE]);
  char *buffer_a = _buffer_a.get();
  char *buffer_cmp = _buffer_cmp.get();
  char *buffer_dcmp = _buffer_dcmp.get();
  char desc[64];
  const char *a;
  size_t a_len;
  size_t cmp_len;
  enum zip_error result;
  int i;

  zip_method_handler *stream = get_stream(ZIP_M_DEFLATE);
  if(!stream)
    FAIL("Failed to get deflate stream!");

  ASSERT(buffer_a && buffer_cmp && buffer_dcmp);

  check_data();

  for(i = 0; i < arraysize(data); i++)
  {
    zip_stream_test_data &d = data[i];
    snprintf(desc, arraysize(desc), "%d (%zu)", i, d.expected.len);

    SET_A(a, a_len, d.expected, d.expected_is_base64);

    result = compress(stream, ZIP_M_DEFLATE, 0, a, a_len,
     buffer_cmp, BUFFER_SIZE, &cmp_len);
    ASSERTEQX(result, ZIP_SUCCESS, desc);

    result = decompress(stream, ZIP_M_DEFLATE, 0, buffer_cmp, cmp_len,
     buffer_dcmp, BUFFER_SIZE);
    ASSERTEQX(result, ZIP_SUCCESS, desc);

    int cmp = memcmp(a, buffer_dcmp, a_len);
    ASSERTEQX(cmp, 0, desc);
  }
}

enum contents_type
{
  CONTENTS_RAW,
  CONTENTS_BASE64,
  CONTENTS_FILE,
};

struct zip_test_file_data
{
  const char *filename;
  const char *contents;
  uint16_t flags;
  uint16_t method;
  uint32_t crc32;
  uint32_t uncompressed_size;
  uint32_t compressed_size;
  uint32_t offset;
  enum contents_type contents_type;
};

struct zip_test_data
{
  const char *data;
  size_t data_length;
  enum zip_error open_result;
  uint32_t num_files;
  zip_test_file_data files[16];
};

static const zip_test_data raw_zip_data[] =
{
  {
    "PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    22, ZIP_SUCCESS, 0, {}
  },
  {
    "prefixed data here!"
    "PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    41, ZIP_SUCCESS, 0, {}
  },
  {
    "abcde.zip", 0, ZIP_SUCCESS, 3,
    {
      { "abcde/", "",
        0x0000, ZIP_M_NONE, 0x00000000, 0, 0, 0, CONTENTS_RAW },
      { "abcde/a.txt", "abcde",
        0x0000, ZIP_M_NONE, 0x8587D865, 5, 5, 36, CONTENTS_RAW },
      { "abcde/b.txt",
        "1234567890 1234567890 1234567890\r\n"
        "1234567890 1234567890 1234567890\r\n"
        "1234567890 1234567890 1234567890",
        0x0000, ZIP_M_DEFLATE, 0xE3046765, 100, 32, 82, CONTENTS_RAW },
    }
  },
  {
    "dch1.zip", 0, ZIP_SUCCESS, 1,
    {
      "dch1.txt", "dch1.txt",
      0x0000, ZIP_M_DEFLATE, 0xA3898FBE, 7045, 3385, 0, CONTENTS_FILE },
  },
  {
    "ct_level.zip", 0, ZIP_SUCCESS, 1,
    {
      "CT_LEVEL.MOD", "CT_LEVEL.MOD",
      0x0000, ZIP_M_DEFLATE, 0x2AF73EBE, 111885, 61105, 0, CONTENTS_FILE },
  },
};

// The library should refuse to open these...
static const zip_test_data raw_zip_invalid_data[] =
{
  { "", 0, ZIP_NO_EOCD, 0, {} },
  { "                  PK\x05\x06", 22, ZIP_NO_EOCD, 0, {} },
  {
    "PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    21, ZIP_NO_EOCD, 0, {}
  },
  {
    "PK\x05\x06\x00\x00\x00\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    22, ZIP_NO_CENTRAL_DIRECTORY, 0, {}
  },
  {
    "PK\x05\x06\x01\x00\x00\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    22, ZIP_UNSUPPORTED_MULTIPLE_DISKS, 0, {}
  },
};

static zip_archive *zip_test_open(const zip_test_data &d)
{
  if(!d.data_length)
  {
    char buffer[MAX_PATH];
    if(path_join(buffer, arraysize(buffer), DATA_DIR, d.data) < 0)
      return nullptr;

    return zip_open_file_read(buffer);
  }
  else
    return zip_open_mem_read((const void *)d.data, d.data_length);
}

static zip_archive *zip_test_open(const zip_test_data &d, char *buffer, size_t len)
{
  if(!d.data_length)
  {
    if(!load_file(d.data, buffer, &len))
      return nullptr;

    return zip_open_mem_read(buffer, len);
  }
  else
    return zip_open_mem_read((const void *)d.data, d.data_length);
}

static const char *zip_get_contents(const zip_test_file_data &df, char *buf,
 size_t buf_size)
{
  size_t len = df.contents ? strlen(df.contents) : 0;
  if(len)
  {
    switch(df.contents_type)
    {
      case CONTENTS_RAW:
        return df.contents;

      case CONTENTS_BASE64:
        debase64(buf, buf_size, df.contents, len);
        return buf;

      case CONTENTS_FILE:
        assert(df.uncompressed_size <= buf_size);
        if(load_file(df.contents, buf, &buf_size))
        {
          assert(df.uncompressed_size == buf_size);
          return buf;
        }
        break;
    }
  }
  return "";
}

// Check that the zip exists and that its central directory headers match the
// expected data.
#define ZIP_CHECK(d, zp) \
  do \
  { \
    ASSERTX(zp, desc); \
    ASSERTEQX(zp->num_files, d.num_files, desc); \
    if(d.num_files) \
      ASSERTX(zp->files, desc); \
    for(size_t _j = 0; _j < d.num_files; _j++) \
    { \
      const zip_test_file_data &df = d.files[_j]; \
      zip_file_header *fh = zp->files[_j]; \
      snprintf(desc2, arraysize(desc2), "%d file %zu", i, _j); \
      ASSERTX(fh, desc2); \
      ASSERTEQX(fh->flags, df.flags, desc2); \
      ASSERTEQX(fh->method, df.method, desc2); \
      ASSERTEQX(fh->crc32, df.crc32, desc2); \
      ASSERTEQX(fh->compressed_size, df.compressed_size, desc2); \
      ASSERTEQX(fh->uncompressed_size, df.uncompressed_size, desc2); \
      ASSERTXCMP(fh->file_name, df.filename, desc2); \
      ASSERTX(df.uncompressed_size <= BUFFER_SIZE, desc); \
    } \
  } while(0)

#define ZIP_GET_CONTENTS(df) zip_get_contents(df, db64_ptr.get(), BUFFER_SIZE)

UNITTEST(ZipRead)
{
  std::unique_ptr<char> buffer_ptr(new char[BUFFER_SIZE]);
  std::unique_ptr<char> db64_ptr(new char[BUFFER_SIZE]);
  std::unique_ptr<char> file_ptr(new char[BUFFER_SIZE]);
  char *buffer = buffer_ptr.get();
  char *file_buffer = file_ptr.get();
  struct zip_archive *zp;
  enum zip_error result;
  char desc[64];
  char desc2[64];
  char small_buffer[32];
  boolean has_files = false;
  int cmp;

  ASSERT(buffer);
  ASSERT(file_buffer);

  SECTION(OpenClose)
  {
    for(int i = 0; i < arraysize(raw_zip_data); i++)
    {
      const zip_test_data &d = raw_zip_data[i];
      snprintf(desc, arraysize(desc), "%d", i);

      zp = zip_test_open(d);
      ZIP_CHECK(d, zp);

      result = zip_close(zp, nullptr);
      ASSERTEQX(result, ZIP_SUCCESS, desc);
    }
  }

  SECTION(OpenInvalid)
  {
    for(int i = 0; i < arraysize(raw_zip_invalid_data); i++)
    {
      const zip_test_data &d = raw_zip_invalid_data[i];
      snprintf(desc, arraysize(desc), "%d", i);

      zp = zip_test_open(d);
      ASSERTEQX(zp, nullptr, desc);

      // TODO: can't test the actual error returned from zip_read_directory anymore :(
    }
  }

  SECTION(ReadFile)
  {
    for(int i = 0; i < arraysize(raw_zip_data); i++)
    {
      const zip_test_data &d = raw_zip_data[i];
      if(d.num_files)
      {
        snprintf(desc, arraysize(desc), "%d", i);
        has_files = true;
        zp = zip_test_open(d);
        ZIP_CHECK(d, zp);

        for(size_t j = 0; j < d.num_files; j++)
        {
          snprintf(desc2, arraysize(desc2), "%d file %zu", i, j);
          const zip_test_file_data &df = d.files[j];
          size_t real_length = 0;

          result = zip_read_file(zp, buffer, BUFFER_SIZE, &real_length);
          ASSERTEQX(result, ZIP_SUCCESS, desc2);
          ASSERTEQX(real_length, df.uncompressed_size, desc2);

          if(real_length)
          {
            const char *contents = ZIP_GET_CONTENTS(df);
            cmp = memcmp(buffer, contents, real_length);
            ASSERTEQX(cmp, 0, desc2);
          }
        }
        result = zip_close(zp, nullptr);
        ASSERTEQX(result, ZIP_SUCCESS, desc);
      }
    }
    if(!has_files)
      FAIL("Add test zips with files to read!");
  }

  SECTION(ReadStream)
  {
    for(int i = 0; i < arraysize(raw_zip_data); i++)
    {
      const zip_test_data &d = raw_zip_data[i];
      if(d.num_files)
      {
        snprintf(desc, arraysize(desc), "%d", i);
        has_files = true;
        zp = zip_test_open(d);
        ZIP_CHECK(d, zp);

        for(size_t j = 0; j < d.num_files; j++)
        {
          snprintf(desc2, arraysize(desc2), "%d file %zu", i, j);
          const zip_test_file_data &df = d.files[j];
          size_t real_length = 0;

          result = zip_read_open_file_stream(zp, &real_length);
          ASSERTEQX(result, ZIP_SUCCESS, desc2);

          const char *contents  = ZIP_GET_CONTENTS(df);
          for(size_t k = 0; k < real_length; k += arraysize(small_buffer))
          {
            size_t n = MIN((size_t)arraysize(small_buffer), real_length - k);
            result = zread(small_buffer, n, zp);
            if(result == ZIP_UNSUPPORTED_DECOMPRESSION_STREAM)
            {
              // TODO remove this workaround for broken stream close CRC-32
              // computation (fixed in https://github.com/AliceLR/megazeux/pull/244)
              zread(buffer, real_length, zp);
              break;
            }
            ASSERTEQX(result, ZIP_SUCCESS, desc2);
            cmp = memcmp(small_buffer, contents + k, n);
            ASSERTEQX(cmp, 0, desc2);
          }
          result = zip_read_close_stream(zp);
          ASSERTEQX(result, ZIP_SUCCESS, desc2);
        }
        result = zip_close(zp, nullptr);
        ASSERTEQX(result, ZIP_SUCCESS, desc);
      }
    }
    if(!has_files)
      FAIL("Add test zips with files to read!");
  }

  SECTION(ReadMemStream)
  {
    for(int i = 0; i < arraysize(raw_zip_data); i++)
    {
      const zip_test_data &d = raw_zip_data[i];
      if(d.num_files)
      {
        snprintf(desc, arraysize(desc), "%d", i);
        has_files = true;
        zp = zip_test_open(d, file_buffer, BUFFER_SIZE);
        ZIP_CHECK(d, zp);
        ASSERTEQX(zp->is_memory, true, desc);

        for(size_t j = 0; j < d.num_files; j++)
        {
          snprintf(desc2, arraysize(desc2), "%d file %zu", i, j);
          const zip_test_file_data &df = d.files[j];
          struct memfile mf;
          size_t real_length = 0;
          unsigned int method;

          result = zip_get_next_method(zp, &method);
          ASSERTEQX(result, ZIP_SUCCESS, desc2);

          result = zip_get_next_uncompressed_size(zp, &real_length);
          ASSERTEQX(result, ZIP_SUCCESS, desc2);

          result = zip_read_open_mem_stream(zp, &mf);
          if(method == ZIP_M_NONE)
          {
            const char *contents  = ZIP_GET_CONTENTS(df);
            ASSERTEQX(result, ZIP_SUCCESS, desc2);

            for(size_t k = 0; k < real_length; k += arraysize(small_buffer))
            {
              size_t n = MIN((size_t)arraysize(small_buffer), real_length - k);
              cmp = mfread(small_buffer, n, 1, &mf);
              ASSERTEQX(cmp, 1, desc2);
              cmp = memcmp(small_buffer, contents + k, n);
              ASSERTEQX(cmp, 0, desc2);
            }
            result = zip_read_close_stream(zp);
            ASSERTEQX(result, ZIP_SUCCESS, desc2);
          }
          else
          {
            ASSERTEQX(result, ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM, desc2);
            result = zip_skip_file(zp);
            ASSERTEQX(result, ZIP_SUCCESS, desc2);
          }
        }
        result = zip_close(zp, nullptr);
        ASSERTEQX(result, ZIP_SUCCESS, desc);
      }
    }
    if(!has_files)
      FAIL("Add test zips with files to read!");
  }
}

/*
// NOTE: file and memory tests are separate here to test expandable archives.
// Intentionally trying to write ZIPs past a fixed buffer should be tested too.
UNITTEST(ZipWrite)
{
  UNIMPLEMENTED();

  SECTION(File_WriteFile)
  {
    //
  }

  SECTION(MemoryExt_WriteFile)
  {
    //
  }

  SECTION(File_WriteStream)
  {
    //
  }

  SECTION(MemoryExt_WriteStream)
  {
    //
  }

  SECTION(MemoryExt_WriteMemStream)
  {
    //
  }

  SECTION(Memory_EOF)
  {
    //
  }
}
*/
