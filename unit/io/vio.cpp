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
#include "../../src/network/Scoped.hpp"
#include "../../src/io/path.h"
#include "../../src/io/vio.c"

static constexpr char TEST_READ_FILENAME[]  = "VFILE_TEST_DATA";
static constexpr char TEST_WRITE_FILENAME[] = "VFILE_TEST_WRITE";
static constexpr char TEST_DIR[]            = "VFILE_TEST_DIR";

// Randomly generated binary data. Note: this is null terminated for vfputs.
// The first byte is also \n to help test vungetc/vfsafegets behavior.
static const uint8_t test_data[] =
{
  0x0A,0xFF,0xE9,0x06,0x94,0xEE,0xC2,0xCE,0x87,0x72,0x5C,0xCE,0x5A,0xBD,0x72,0xE8,
  0x3A,0xD3,0x47,0x5F,0x2B,0x3F,0x8E,0x31,0x04,0x04,0x87,0xA7,0xBB,0xD6,0x6C,0x2A,
  0xE3,0x8A,0x0B,0x58,0x73,0xCC,0xB4,0x57,0xC9,0x10,0x55,0x69,0xEF,0xCB,0xE4,0xC1,
  0x1C,0xE2,0xFD,0x96,0x7E,0x62,0xCD,0x75,0x36,0x73,0x2E,0xDB,0xA5,0x87,0x13,0x03,
  0xE6,0xD7,0x27,0xFD,0x9B,0x10,0xB1,0x0D,0xED,0x49,0xBB,0x01,0xCE,0x39,0x73,0x84,
  0x81,0xA7,0xD1,0xB3,0x5D,0x21,0x7A,0xAD,0xCE,0xF1,0xE5,0x20,0x9C,0x4D,0xBC,0x1A,
  0x70,0xCE,0x85,0x1C,0x94,0x24,0x81,0x71,0xFA,0x07,0xD4,0xBD,0x80,0x70,0xE7,0xD9,
  0x72,0x0A,0x0B,0xDE,0x50,0xE3,0x5F,0x80,0xD2,0x68,0xF3,0x9E,0x2A,0x90,0x2D,0x16,
  0x89,0x57,0x6C,0xDE,0xE4,0x6E,0xEC,0x51,0xF8,0x31,0xEA,0xC6,0x8C,0xD9,0x08,0x67,
  0xF6,0xF3,0xF2,0x41,0xE0,0x11,0x42,0x97,0x4C,0xBF,0xA1,0x7C,0xD6,0xB8,0x2F,0xA1,
  0x39,0x5A,0x26,0x6B,0x15,0x58,0xBA,0x48,0xF0,0xAF,0x43,0x44,0x7A,0xDB,0x9D,0xD7,
  0x15,0x4A,0xCF,0x01,0x94,0x11,0xEC,0x99,0x43,0xDE,0x37,0xE3,0x28,0x2D,0x8A,0x60,
  0x89,0xBF,0xF9,0xEA,0xAF,0x48,0xB2,0x07,0xE9,0x69,0x27,0x5E,0xD3,0xDD,0x70,0xD0,
  0xD7,0xF7,0xEA,0x49,0xF5,0x4C,0x25,0x2F,0xC0,0xAD,0xFD,0xFA,0xA9,0x58,0x06,0xFD,
  0x80,0x6E,0x2E,0x83,0x38,0xA8,0x9D,0x1E,0xEB,0x46,0xE0,0x3C,0x1D,0x49,0x47,0xFB,
  0x45,0xE2,0x8B,0x3F,0x8A,0x2A,0xB4,0x01,0xCA,0x13,0x3A,0xEA,0xE0,0x9F,0x6B,0x00,
};

static constexpr int VFSAFEGETS_BUFFER = 64;
static constexpr int MAX_LINES = 10;

static const char *TEST_READ_TEXT_FILENAME[] =
{
  "VFILE_TEST_DATA_TEXT",
  "VFILE_TEST_DATA_TEXT2",
  "VFILE_TEST_DATA_TEXT3",
};

static const char * const vfsafegets_data[] =
{
  // Test 1.
  "kjflkjsdlfksjdfksdj\r\n"
  "abcdef\n"
//"i hope no one is actually using these in 2020 ;-(\r"
  "use this line to fill past the buffer!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
  "\r\n"
  "end of file also counts as an end of line",

  // Test 2.
  "trailing EOL should result in a single line read.\n",

  // Test 3.
  "trailing EOL should result in a single line read.\r\n"
};

static const char * const vfsafegets_output[][MAX_LINES] =
{
  {
    "kjflkjsdlfksjdfksdj",
    "abcdef",
//  "i hope no one is actually using these in 2020 ;-(",
    "use this line to fill past the buffer!!!!!!!!!!!!!!!!!!!!!!!!!!",
    "!!!!!!!!!",
    "",
    "end of file also counts as an end of line",
    nullptr
  },
  {
    "trailing EOL should result in a single line read.",
    nullptr
  },
  {
    "trailing EOL should result in a single line read.",
    nullptr
  }
};

#define back_up(bytes_from_end, vf) \
{ \
  static_assert(bytes_from_end < 0, "invalid bytes from end value :("); \
  vfseek(vf, arraysize(test_data) + bytes_from_end - 1, SEEK_SET); \
  int c = vfgetc(vf); \
  ASSERTEQX(c, test_data[arraysize(test_data) + bytes_from_end - 1], "not eof"); \
}

static void test_vfgetc(vfile *vf)
{
  char buf[64];
  for(int i = 0; i < arraysize(test_data); i++)
  {
    snprintf(buf, arraysize(buf), "vfgetc offset=%d", i);
    int c = vfgetc(vf);
    ASSERTEQX(c, test_data[i], buf);
  }
  int c = vfgetc(vf);
  ASSERTEQX(c, EOF, "eof");
}

static void test_vfgetw(vfile *vf)
{
  char buf[64];
  int expected;
  int c;

  for(int i = 0; i < arraysize(test_data); i += 2)
  {
    snprintf(buf, arraysize(buf), "vfgetw offset=%d", i);

    c = vfgetw(vf);
    expected = test_data[i] | (test_data[i + 1] << 8);
    ASSERTEQX(c, expected, buf);
  }
  // Both bytes should cause EOF.
  c = vfgetw(vf);
  ASSERTEQX(c, EOF, "eof (byte 0)");

  back_up(-1, vf);
  c = vfgetw(vf);
  ASSERTEQX(c, EOF, "eof (byte 1)");
}

static void test_vfgetd(vfile *vf)
{
  char buf[64];
  int expected;
  int c;

  for(int i = 0; i < arraysize(test_data); i += 4)
  {
    snprintf(buf, arraysize(buf), "vfgetd offset=%d", i);

    c = vfgetd(vf);
    expected = static_cast<int>(test_data[i] | (test_data[i + 1] << 8) |
     (test_data[i + 2] << 16) | (test_data[i + 3] << 24));
    ASSERTEQX(c, expected, buf);
  }
  // All four bytes should cause EOF.
  c = vfgetd(vf);
  ASSERTEQX(c, EOF, "eof (byte 0)");

  back_up(-1, vf);
  c = vfgetd(vf);
  ASSERTEQX(c, EOF, "eof (byte 1)");

  back_up(-2, vf);
  c = vfgetd(vf);
  ASSERTEQX(c, EOF, "eof (byte 2)");

  back_up(-3, vf);
  c = vfgetd(vf);
  ASSERTEQX(c, EOF, "eof (byte 3)");
}

static void test_vfread(vfile *vf)
{
  static constexpr int len = arraysize(test_data);
  char buffer[len * 2];
  int ret;

  // One read >length should be refused.
  ret = vfread(buffer, len * 2, 1, vf);
  ASSERTEQX(ret, 0, "vfread too much data");
  vrewind(vf);

  // Two reads =length should read exactly length once.
  ret = vfread(buffer, len, 2, vf);
  ASSERTEQX(ret, 1, "vfread 2x len");
  vrewind(vf);

  // More reads...
  ret = vfread(buffer, len / 2, 3, vf);
  ASSERTEQX(ret, 2, "vfread 1.5x len");
  vrewind(vf);

  ret = vfread(buffer, len / 4, 5, vf);
  ASSERTEQX(ret, 4, "vfread 1.25x len");
  vrewind(vf);

  ret = vfread(buffer, 3, len, vf);
  ASSERTEQX(ret, len / 3, "vfread len x 3");
  // NOTE: seems to be implementation-defined whether this ends up
  // at 255 or 256, so don't bother testing that.
}

static void test_write_check(vfile *vf)
{
  uint8_t tmp[arraysize(test_data)];
  int r = vfread(tmp, arraysize(test_data), 1, vf);
  ASSERTEQX(r, 1, "test_write_check vfread");
  ASSERTXMEM(test_data, tmp, arraysize(test_data), "test_write_check memcmp");
  r = vfgetc(vf);
  ASSERTEQX(r, EOF, "test_write_check eof");
}

static void test_vfputc(vfile *vf)
{
  char buf[64];
  long pos = vftell(vf);

  for(int i = 0; i < arraysize(test_data); i++)
  {
    snprintf(buf, arraysize(buf), "vfputc offset=%d", i);
    vfputc(test_data[i], vf);
  }
  int r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQX(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfputw(vfile *vf)
{
  char buf[64];
  long pos = vftell(vf);

  for(int i = 0; i < arraysize(test_data); i += 2)
  {
    int d = test_data[i] | (test_data[i + 1] << 8);
    snprintf(buf, arraysize(buf), "vfputw offset=%d", i);
    vfputw(d, vf);
  }
  int r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQX(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfputd(vfile *vf)
{
  char buf[64];
  long pos = vftell(vf);

  for(int i = 0; i < arraysize(test_data); i += 4)
  {
    int d = test_data[i] | (test_data[i + 1] << 8) |
     (test_data[i + 2] << 16) | (test_data[i + 3] << 24);
    snprintf(buf, arraysize(buf), "vfputw offset=%d", i);
    vfputd(d, vf);
  }
  int r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQX(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfwrite(vfile *vf)
{
  long pos = vftell(vf);

  int r = vfwrite(test_data, 1, arraysize(test_data), vf);
  ASSERTEQX(r, arraysize(test_data), "vfwrite");

  r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQX(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfputs(vfile *vf)
{
  long pos = vftell(vf);

  int r = vfputs(reinterpret_cast<const char *>(test_data), vf);
  ASSERTEQX(r, 0, "vfputs");

  r = vfputc(0, vf);
  ASSERTEQX(r, 0, "vfputc");

  r = vfseek(vf, pos, SEEK_SET);
  ASSERTEQX(r, 0, "vfseek");
  test_write_check(vf);
}

static void test_vfseek_vftell_vrewind_read(vfile *vf)
{
  static constexpr int set_valid[] = { 0, 128, 256, 7, 19, 157, 79, 9999 };
  static constexpr int cur_valid[] = { 0, 128, 64, 32, 16, -16, -32, -64, -128 }; // Cumulative.
  static constexpr int set_cur_invalid[] = { -15, -120312, -1 };
  static constexpr int end_valid[] = { 0, -1, -5, -20, -256, 10, 9999 };
  static constexpr int end_invalid[] = { -257, -2000, -127848, -2391231 };
  static constexpr int len = arraysize(test_data);

  char buf[64];
  long expected;
  long pos;
  int ret;

  // SEEK_SET (valid).
  for(int i = 0; i < arraysize(set_valid); i++)
  {
    snprintf(buf, arraysize(buf), "SEEK_SET valid %d", i);
    ret = vfseek(vf, set_valid[i], SEEK_SET);
    pos = vftell(vf);
    ASSERTEQX(ret, 0, buf);
    ASSERTEQX(pos, set_valid[i], buf);
  }
  vrewind(vf);
  pos = vftell(vf);
  ASSERTEQX(pos, 0, "vrewind after SEEK_SET");

  // SEEK_CUR (valid).
  expected = 0;
  for(int i = 0; i < arraysize(cur_valid); i++)
  {
    snprintf(buf, arraysize(buf), "SEEK_CUR valid %d", i);
    expected += cur_valid[i];
    ret = vfseek(vf, cur_valid[i], SEEK_CUR);
    pos = vftell(vf);
    ASSERTEQX(ret, 0, buf);
    ASSERTEQX(pos, expected, buf);
  }
  vrewind(vf);
  pos = vftell(vf);
  ASSERTEQX(pos, 0, "vrewind after SEEK_CUR");

  // SEEK_SET and SEEK_CUR (invalid).
  for(int i = 0; i < arraysize(set_cur_invalid); i++)
  {
    snprintf(buf, arraysize(buf), "SEEK_SET invalid %d", i);
    ret = vfseek(vf, set_cur_invalid[i], SEEK_SET);
    ASSERTX(ret != 0, buf);
    vrewind(vf);
    pos = vftell(vf);
    ASSERTEQX(pos, 0, buf);

    snprintf(buf, arraysize(buf), "SEEK_CUR invalid %d", i);
    ret = vfseek(vf, set_cur_invalid[i], SEEK_CUR);
    ASSERTX(ret != 0, buf);
    vrewind(vf);
    pos = vftell(vf);
    ASSERTEQX(pos, 0, buf);
  }

  // SEEK_END (valid).
  for(int i = 0; i < arraysize(end_valid); i++)
  {
    snprintf(buf, arraysize(buf), "SEEK_END valid %d", i);
    ret = vfseek(vf, end_valid[i], SEEK_END);
    pos = vftell(vf);
    ASSERTEQX(ret, 0, buf);
    ASSERTEQX(pos, len + end_valid[i], buf);
  }
  vrewind(vf);
  pos = vftell(vf);
  ASSERTEQX(pos, 0, "vrewind after SEEK_END");

  // SEEK_END (invalid).
  for(int i = 0; i < arraysize(end_invalid); i++)
  {
    snprintf(buf, arraysize(buf), "SEEK_END invalid %d", i);
    ret = vfseek(vf, end_invalid[i], SEEK_END);
    ASSERTX(ret != 0, buf);
    vrewind(vf);
    pos = vftell(vf);
    ASSERTEQX(pos, 0, buf);
  }
}

static void test_filelength(long expected_len, vfile *vf)
{
  long pos = vftell(vf);
  long len = vfilelength(vf, false);
  long newpos = vftell(vf);

  ASSERTEQX(len, expected_len, "vfilelength");
  ASSERTEQX(pos, newpos, "vfilelength with no rewind");

  if(len >> 1)
  {
    vfseek(vf, len >> 1, SEEK_SET);
    len = vfilelength(vf, true);
    newpos = vftell(vf);

    ASSERTEQX(len, expected_len, "vfilelength 2");
    ASSERTEQX(newpos, 0, "vfilelength with rewind");
  }
}

static void test_vungetc(vfile *vf)
{
  long pos;
  int ret;
  int value;
  int first_dword;
  char next_64[64];
  char last_5[5];
  char tmp[64];
  char *retstr;

  first_dword = vfgetd(vf);
  vfread(next_64, 64, 1, vf);
  vfseek(vf, arraysize(test_data) - 5, SEEK_SET);
  vfread(last_5, 5, 1, vf);
  vrewind(vf);

  // vungetc should fail if EOF or some other junk is provided.
  // Note: MSVCRT stdio &255s non-EOF values (?).
  ret = vungetc(EOF, vf);
  ASSERTEQ(ret, EOF);
  ret = vungetc(-128141, vf);
  ASSERTEQ(ret, EOF);
  ret = vungetc(256, vf);
  ASSERTEQ(ret, EOF);

  // vfgetc should read the buffered char.
  ret = vungetc(0xAB, vf);
  ASSERTEQ(ret, 0xAB);
  value = vfgetc(vf);
  ASSERTEQ(value, 0xAB);

  // vfgetw should read the buffered char + one char from the data.
  ret = vungetc(0xCD, vf);
  ASSERTEQ(ret, 0xCD);
  value = vfgetw(vf);
  ASSERTEQ(value, 0xCD | ((first_dword & 0xFF) << 8));

  // vfgetd should read the buffered char + three chars from the data.
  ret = vungetc(0xEF, vf);
  ASSERTEQ(ret, 0xEF);
  value = vfgetd(vf);
  ASSERTEQ(value, 0xEF | (first_dword & ~0xFF));

  // vfread should read the buffered char + n-1 chars from the data.
  ret = vungetc(0x12, vf);
  ASSERTEQ(ret, 0x12);
  value = vfread(tmp, 64, 1, vf);
  ASSERTEQ(value, 1);
  ASSERTEQ(tmp[0], 0x12);
  ASSERTMEM(tmp + 1, next_64, 63);

  // vfsafegets should read the buffered char + n-1 chars from the data.
  vfseek(vf, arraysize(test_data) - 5, SEEK_SET);
  ret = vungetc(0xFF, vf);
  ASSERTEQ(ret, 0xFF);
  retstr = vfsafegets(tmp, arraysize(tmp), vf);
  ASSERT(retstr);
  ASSERTEQ(tmp[0], 0xFF);
  ASSERTMEM(tmp + 1, last_5, 5);

  // If the buffer char is \r and the next char in the stream is \n, consume both.
  vfseek(vf, 0, SEEK_SET);
  ret = vungetc(0x0D, vf);
  ASSERTEQ(ret, 0x0D);
  retstr = vfsafegets(tmp, arraysize(tmp), vf);
  ASSERT(retstr);
  ASSERTEQ((int)tmp[0], 0);
  pos = vftell(vf);
  ASSERTEQ(pos, 1);

  // Reading a buffer char from the end of the file should not return NULL.
  vfseek(vf, 0, SEEK_END);
  ret = vungetc('a', vf);
  ASSERTEQ(ret, 'a');
  retstr = vfsafegets(tmp, arraysize(tmp), vf);
  ASSERT(retstr);
  ASSERTCMP(retstr, "a");
  pos = vftell(vf);
  ASSERTEQ(pos, arraysize(test_data));

  // vseek should discard the buffered char.
  ret = vungetc(0x34, vf);
  ASSERTEQ(ret, 0x34);
  vfseek(vf, 128, SEEK_SET);
  value = vfgetc(vf);
  ASSERTEQ(value, test_data[128]);

  // vrewind should discard the buffered char.
  ret = vungetc(0x56, vf);
  ASSERTEQ(ret, 0x56);
  vrewind(vf);
  value = vfgetd(vf);
  ASSERTEQ(value, first_dword);

  // vfilelength with rewind should discard the buffered char.
  ret = vungetc(0x78, vf);
  ASSERTEQ(ret, 0x78);
  long len = vfilelength(vf, true);
  ASSERTEQ(len, arraysize(test_data));
  value = vfgetd(vf);
  ASSERTEQ(value, first_dword);

  // ftell should subtract the buffered char count from the cursor for binary
  // streams. Calling ftell when (cursor - buffered char count)<0 is undefined.
  ret = vfseek(vf, 128, SEEK_SET);
  ASSERTEQ(ret, 0);
  ret = vungetc(0x9A, vf);
  ASSERTEQ(ret, 0x9A);
  pos = vftell(vf);
  ASSERTEQ(pos, 127);
}

static void test_vfsafegets(vfile *vf, int i, const char * const (&output)[MAX_LINES])
{
  char buffer[VFSAFEGETS_BUFFER];
  char msg[64];
  char *cursor;
  int line = 0;

  while((cursor = vfsafegets(buffer, VFSAFEGETS_BUFFER, vf)))
  {
    snprintf(msg, arraysize(msg), "test=%d, line=%d", i, line);
    ASSERTX(line < MAX_LINES - 1, msg);
    ASSERTX(output[line], msg);
    ASSERTXCMP(buffer, output[line], msg);
    line++;
  }
  ASSERT(!output[line]);
}

#define READ_TESTS(vf) do { \
  SECTION(read_vfgetc)      test_vfgetc(vf); \
  SECTION(read_vfgetw)      test_vfgetw(vf); \
  SECTION(read_vfgetd)      test_vfgetd(vf); \
  SECTION(read_vfread)      test_vfread(vf); \
  SECTION(read_vfseektell)  test_vfseek_vftell_vrewind_read(vf); \
  SECTION(read_filelength)  test_filelength(arraysize(test_data), vf); \
  SECTION(read_vungetc)     test_vungetc(vf); \
} while(0)

#define WRITE_TESTS(vf, is_append) do { \
  SECTION(write_vfputc)     test_vfputc(vf); \
  SECTION(write_vfputw)     test_vfputw(vf); \
  SECTION(write_vfputd)     test_vfputd(vf); \
  SECTION(write_vfwrite)    test_vfwrite(vf); \
  SECTION(write_vfputs)     test_vfputs(vf); \
  /*if(!is_append) { SECTION(write_vfseektell)  test_vfseek_vftell_rewind_write(vf); } */ \
} while(0)

UNITTEST(Init)
{
  FILE *fp = fopen_unsafe(TEST_READ_FILENAME, "wb");
  ASSERTX(fp, "fopen_unsafe");

  int r = fwrite(test_data, sizeof(test_data), 1, fp);
  ASSERTEQX(r, 1, "fwrite");

  r = fclose(fp);
  ASSERTEQX(r, 0, "fclose");

  samesize(TEST_READ_TEXT_FILENAME, vfsafegets_data);
  samesize(TEST_READ_TEXT_FILENAME, vfsafegets_output);

  for(int i = 0; i < arraysize(TEST_READ_TEXT_FILENAME); i++)
  {
    fp = fopen_unsafe(TEST_READ_TEXT_FILENAME[i], "wb");
    ASSERTX(fp, "fopen_unsafe");

    r = fwrite(vfsafegets_data[i], strlen(vfsafegets_data[i]), 1, fp);
    ASSERTEQX(r, 1, "fwrite");

    r = fclose(fp);
    ASSERTEQX(r, 0, "fclose");
  }
}

UNITTEST(FileRead)
{
  ScopedFile<vfile, vfclose> vf_in =
   vfopen_unsafe_ext(TEST_READ_FILENAME, "rb", V_SMALL_BUFFER);
  ASSERT(vf_in);
  READ_TESTS(vf_in);
}

UNITTEST(FileWrite)
{
  ScopedFile<vfile, vfclose> vf_out = vfopen_unsafe(TEST_WRITE_FILENAME, "w+b");
  ASSERT(vf_out);
  WRITE_TESTS(vf_out, false);
}

UNITTEST(FileAppend)
{
  ScopedFile<vfile, vfclose> vf_out = vfopen_unsafe(TEST_WRITE_FILENAME, "a+b");
  ASSERT(vf_out);
  // Align the read cursor with the write cursor.
  vfseek(vf_out, 0, SEEK_END);
  WRITE_TESTS(vf_out, true);
}

UNITTEST(MemoryRead)
{
  char buffer[arraysize(test_data)];
  memcpy(buffer, test_data, arraysize(test_data));
  ScopedFile<vfile, vfclose> vf_in = vfile_init_mem(buffer, arraysize(buffer), "rb");
  ASSERT(vf_in);
  READ_TESTS(vf_in);
}

UNITTEST(MemoryWrite)
{
  static constexpr int len = arraysize(test_data);
  char buffer[len];
  int ret;

  ScopedFile<vfile, vfclose> vf_out = vfile_init_mem(buffer, len, "w+b");
  ASSERT(vf_out);
  WRITE_TESTS(vf_out, false);

  SECTION(NoWritePastEnd)
  {
    /* Make sure writes that would exceed a fixed size buffer are refused. */
    vfseek(vf_out, len, SEEK_SET);
    ret = vfputc(0xAB, vf_out);
    ASSERTEQ(ret, EOF);

    vfseek(vf_out, len - 1, SEEK_SET);
    ret = vfputw(0xCD, vf_out);
    ASSERTEQ(ret, EOF);

    vfseek(vf_out, len - 3, SEEK_SET);
    ret = vfputd(0xEF, vf_out);
    ASSERTEQ(ret, EOF);

    static constexpr char tmp[] = "abcdefghij";
    vfseek(vf_out, len - 7, SEEK_SET);
    ret = vfputs(tmp, vf_out);
    ASSERTEQ(ret, EOF);

    vfseek(vf_out, 128, SEEK_SET);
    ret = vfwrite(test_data, len, 1, vf_out);
    ASSERTEQ(ret, 0);
  }
}

/* Make sure buffer expansion works correctly. */
UNITTEST(MemoryWriteExt)
{
  void *buffer = nullptr;
  size_t size = 0;

  ScopedFile<vfile, vfclose> vf_out = vfile_init_mem_ext(&buffer, &size, "w+b");
  ASSERT(vf_out);
  WRITE_TESTS(vf_out, false);
  free(buffer);
}

/* That goes for append mode too... */
UNITTEST(MemoryAppendExt)
{
  static constexpr size_t BUF_SIZE = 256;
  void *buffer = malloc(BUF_SIZE);
  size_t size = BUF_SIZE;
  ASSERT(buffer);

  {
    ScopedFile<vfile, vfclose> vf_out = vfile_init_mem_ext(&buffer, &size, "a+b");
    ASSERT(vf_out);
    // Align the read cursor with the write cursor.
    vfseek(vf_out, 0, SEEK_END);
    WRITE_TESTS(vf_out, true);
  }
  free(buffer);
  if(this->expected_section)
    ASSERTEQ(size, arraysize(test_data) + BUF_SIZE);
}

UNITTEST(vfsafegets)
{
  SECTION(FileBinary)
  {
    for(int i = 0; i < arraysize(TEST_READ_TEXT_FILENAME); i++)
    {
      ScopedFile<vfile, vfclose> vf =
       vfopen_unsafe_ext(TEST_READ_TEXT_FILENAME[i], "rb", V_SMALL_BUFFER);
      ASSERT(vf);
      test_vfsafegets(vf, i, vfsafegets_output[i]);
    }
  }

  SECTION(FileText)
  {
    // In practice, nothing in MZX uses text streams (intentionally) because
    // even most MZX text files can contain binary chars. See:
    // https://www.digitalmzx.com/forums/index.php?app=tracker&showissue=592
    for(int i = 0; i < arraysize(TEST_READ_TEXT_FILENAME); i++)
    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(TEST_READ_TEXT_FILENAME[i], "r");
      ASSERT(vf);
      test_vfsafegets(vf, i, vfsafegets_output[i]);
    }
  }

  SECTION(Memory)
  {
    // Memory vfiles don't bother implementing text mode so just assume binary.
    for(int i = 0; i < arraysize(vfsafegets_output); i++)
    {
      const char *input = vfsafegets_data[i];
      size_t len = strlen(input);
      std::unique_ptr<char[]> tmp(new char[len + 1]);

      memcpy(tmp.get(), input, len + 1);
      ScopedFile<vfile, vfclose> vf = vfile_init_mem(tmp.get(), len, "rb");
      ASSERT(vf);
      test_vfsafegets(vf, i, vfsafegets_output[i]);
    }
  }
}

UNITTEST(vtempfile)
{
  SECTION(File)
  {
    ScopedFile<vfile, vfclose> vf = vtempfile(0);
    ASSERT(vf);
    test_vfwrite(vf);
  }

  SECTION(Memory)
  {
    ScopedFile<vfile, vfclose> vf = vtempfile(arraysize(test_data));
    ASSERT(vf);
    test_vfwrite(vf);
  }

  SECTION(MemorySmallInit)
  {
    ScopedFile<vfile, vfclose> vf = vtempfile(1);
    ASSERT(vf);
    test_vfputc(vf);
  }
}

UNITTEST(Filesystem)
{
  static constexpr char TEST_RENAME_FILENAME[] = "VFILE_TEST_dfbdfbshd";
  static constexpr char TEST_RENAME_DIR[] = "VFILE_TEST_DIR_ndfjsdbnfjdfd";
  static char execdir[1024];
  struct stat stat_info;
  int ret;

  if(!this->expected_section)
  {
    char *t = getcwd(execdir, arraysize(execdir));
    ASSERTEQ(t, execdir);
  }
  else
  {
    ret = vchdir(execdir);
    ASSERTEQ(ret, 0);
  }

  SECTION(vchdir)
  {
    ret = vchdir("..");
    ASSERTEQ(ret, 0);
    ret = vchdir("data");
    ASSERTEQ(ret, 0);

    ScopedFile<vfile, vfclose> vf = vfopen_unsafe("CT_LEVEL.MOD", "rb");
    ASSERT(vf);
    long len = vfilelength(vf, false);
    ASSERTEQ(len, 111885);
  }

  SECTION(vgetcwd)
  {
    char buffer[1024];
    char buffer2[1024];
    char *t = getcwd(buffer, arraysize(buffer));
    ASSERTEQ(t, buffer);
    ret = vchdir("..");
    ASSERTEQ(ret, 0);
    t = getcwd(buffer2, arraysize(buffer2));
    ASSERTEQ(t, buffer2);
    size_t len = strlen(buffer2);
    ASSERT(len < arraysize(buffer));
    ASSERTMEM(buffer, buffer2, len);
  }

  SECTION(vmkdir)
  {
    ret = vmkdir(TEST_DIR, 0777);
    ASSERTEQ(ret, 0);
    ret = stat(TEST_DIR, &stat_info);
    ASSERTEQ(ret, 0);
    ASSERT(S_ISDIR(stat_info.st_mode));
  }

  SECTION(vunlink)
  {
    ret = stat(TEST_WRITE_FILENAME, &stat_info);
    ASSERTEQ(ret, 0);
    ASSERT(S_ISREG(stat_info.st_mode));
    ret = vunlink(TEST_WRITE_FILENAME);
    ASSERTEQ(ret, 0);
    ret = stat(TEST_WRITE_FILENAME, &stat_info);
    ASSERT(ret != 0);
  }

  SECTION(vrmdir)
  {
    ret = stat(TEST_DIR, &stat_info);
    ASSERTEQ(ret, 0);
    ASSERT(S_ISDIR(stat_info.st_mode));
    ret = vrmdir(TEST_DIR);
    ASSERTEQ(ret, 0);
    ret = stat(TEST_DIR, &stat_info);
    ASSERT(ret != 0);
  }

  SECTION(vaccess)
  {
#ifndef _WIN32
    static constexpr int access_flags = R_OK|W_OK|X_OK;
#else
    static constexpr int access_flags = R_OK|W_OK;
#endif
    ret = access(".", access_flags);
    ASSERTEQ(ret, 0);
    ret = vaccess(".", R_OK|W_OK|X_OK);
    ASSERTEQ(ret, 0);

    ret = vchdir("..");
    ASSERTEQ(ret, 0);
    ret = vchdir("data");
    ASSERTEQ(ret, 0);
    ret = access("CT_LEVEL.MOD", R_OK|W_OK);
    ASSERTEQ(ret, 0);
    ret = vaccess("CT_LEVEL.MOD", R_OK|W_OK);
    ASSERTEQ(ret, 0);
  }

  SECTION(vstat)
  {
    struct stat stat_info2;
    ret = stat(".", &stat_info);
    ASSERTEQ(ret, 0);
    ret = vstat(".", &stat_info2);
    ASSERTEQ(ret, 0);
    ASSERT(S_ISDIR(stat_info.st_mode));
    ASSERT(S_ISDIR(stat_info2.st_mode));

    ret = vchdir("..");
    ASSERTEQ(ret, 0);
    ret = vchdir("data");
    ASSERTEQ(ret, 0);
    ret = stat("CT_LEVEL.MOD", &stat_info);
    ASSERTEQ(ret, 0);
    ret = vstat("CT_LEVEL.MOD", &stat_info2);
    ASSERTEQ(ret, 0);
    ASSERT(S_ISREG(stat_info.st_mode));
    ASSERT(S_ISREG(stat_info2.st_mode));
  }

  SECTION(vrename)
  {
    char buffer[1024];

    // Clean up from any previous tests...
    snprintf(buffer, arraysize(buffer), "%s" DIR_SEPARATOR "%s", TEST_RENAME_DIR, TEST_RENAME_FILENAME);
    vunlink(buffer);
    snprintf(buffer, arraysize(buffer), "%s" DIR_SEPARATOR "%s", TEST_DIR, TEST_RENAME_FILENAME);
    vunlink(buffer);
    vrmdir(TEST_RENAME_DIR);
    vrmdir(TEST_DIR);

    ret = vmkdir(TEST_DIR, 0777);
    ASSERTEQ(ret, 0);

    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(TEST_WRITE_FILENAME, "wb");
      ASSERT(vf);
    }

    ret = vstat(TEST_WRITE_FILENAME, &stat_info);
    ASSERTEQ(ret, 0);

    // Rename file.
    ret = vrename(TEST_WRITE_FILENAME, buffer);
    ASSERTEQ(ret, 0);

    ret = vstat(buffer, &stat_info);
    ASSERTEQ(ret, 0);

    // Rename dir.
    ret = vrename(TEST_DIR, TEST_RENAME_DIR);
    ASSERTEQ(ret, 0);

    ret = vstat(TEST_RENAME_DIR, &stat_info);
    ASSERTEQ(ret, 0);

    // Renamed filename still in dir?
    ret = vchdir(TEST_RENAME_DIR);
    ASSERTEQ(ret, 0);

    ret = vstat(TEST_RENAME_FILENAME, &stat_info);
    ASSERTEQ(ret, 0);
  }

  SECTION(UTF8)
  {
    // NOTE: using the combining encodings of öè here to appease Mac OS X,
    // which converts some single codepoint encodings to combining encodings.
    static constexpr char UTF8_DIR[] = u8"\u00e6Ro\u0308e\u0300mMJ\u00b7\u2021\u00b2\u2019\u02c6\u00de\u2018$";
    static constexpr char UTF8_FILE[] = u8"\u00A5\u2014\U0001F970";
    static constexpr char UTF8_FILE2[] = u8"\U0001F970\u2014\u00A5";
    static constexpr int utf8_dir_len = arraysize(UTF8_DIR) - 1;

    ret = vstat(UTF8_DIR, &stat_info);
    if(!ret)
    {
      ret = vchdir(UTF8_DIR);
      ASSERTEQ(ret, 0);
      ret = vunlink(UTF8_FILE);
      ret = vunlink(UTF8_FILE2);
      ret = vchdir("..");
      ASSERTEQ(ret, 0);
      ret = vrmdir(UTF8_DIR);
      ASSERTEQ(ret, 0);
    }

    ret = vmkdir(UTF8_DIR, 0777);
    ASSERTEQ(ret, 0);

    ret = vstat(UTF8_DIR, &stat_info);
    ASSERTEQ(ret, 0);
    ASSERT(S_ISDIR(stat_info.st_mode));

    ret = vchdir(UTF8_DIR);
    ASSERTEQ(ret, 0);
    char buffer[1024];

    char *t = vgetcwd(buffer, arraysize(buffer));
    ASSERTEQ(t, buffer);
    size_t len = strlen(buffer);
    ASSERTCMP(buffer + len - utf8_dir_len, UTF8_DIR);

    {
      ScopedFile<vfile, vfclose> vf = vfopen_unsafe(UTF8_FILE, "wb");
      ASSERT(vf);
      ret = vfwrite(test_data, arraysize(test_data), 1, vf);
      ASSERTEQ(ret, 1);
    }

    ret = vstat(UTF8_FILE, &stat_info);
    ASSERTEQ(ret, 0);
    ASSERTEQ(stat_info.st_size, arraysize(test_data));
    ASSERT(S_ISREG(stat_info.st_mode));

    ret = vaccess(UTF8_FILE, R_OK|W_OK);
    ASSERTEQ(ret, 0);

    ret = vrename(UTF8_FILE, UTF8_FILE2);
    ASSERTEQ(ret, 0);

    ret = vstat(UTF8_FILE2, &stat_info);
    ASSERTEQ(ret, 0);
    ASSERTEQ(stat_info.st_size, arraysize(test_data));
    ASSERT(S_ISREG(stat_info.st_mode));

    ret = vunlink(UTF8_FILE2);
    ASSERTEQ(ret, 0);

    ret = vchdir("..");
    ASSERTEQ(ret, 0);

    ret = vrmdir(UTF8_DIR);
    ASSERTEQ(ret, 0);
  }
}
