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

/*
UNITTEST(ZipRead)
{
  UNIMPLEMENTED();

  SECTION(open_close)
  {
    //
  }

  SECTION(read_file)
  {
    //
  }

  SECTION(read_stream)
  {
    //
  }

  SECTION(read_mem_stream)
  {
    //
  }
}

UNITTEST(ZipWrite)
{
  UNIMPLEMENTED();

  SECTION(write_file)
  {
    //
  }

  SECTION(write_stream)
  {
    //
  }

  SECTION(write_mem_stream)
  {
    //
  }
}
*/
