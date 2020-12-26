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

#include "../Unit.hpp"
#include "../../src/io/memfile.h"

#include <limits.h>
#include <stdlib.h>

UNITTEST(mfopen)
{
  unsigned char buffer[16];

  SECTION(mfopen)
  {
    struct memfile mf;

    mfopen(buffer, arraysize(buffer), &mf);
    ASSERTEQ(mf.start, buffer);
    ASSERTEQ(mf.current, buffer);
    ASSERTEQ(mf.end, mf.start + arraysize(buffer));
    ASSERTEQ(mf.alloc, false);
  }

  SECTION(mfopen_alloc)
  {
    struct memfile *mf;

    mf = mfopen_alloc(buffer, arraysize(buffer));
    ASSERT(mf);
    ASSERTEQ(mf->start, buffer);
    ASSERTEQ(mf->current, buffer);
    ASSERTEQ(mf->end, buffer + arraysize(buffer));
    ASSERTEQ(mf->alloc, true);

    int ret = mf_alloc_free(mf);
    ASSERTEQ(ret, 0);
  }

  SECTION(mf_alloc_free_on_static_memfile)
  {
    struct memfile mf;
    mfopen(buffer, arraysize(buffer), &mf);

    int ret = mf_alloc_free(&mf);
    ASSERTEQ(ret, -1);
    ASSERTEQ(mf.start, buffer);
    ASSERTEQ(mf.current, buffer);
    ASSERTEQ(mf.end, buffer + arraysize(buffer));
  }
}

UNITTEST(mfsync)
{
  unsigned char buffers[4][32];
  struct memfile mf;
  int i;

  void *buf;
  size_t len;

  for(i = 0; i < arraysize(buffers); i++)
  {
    mfopen(buffers[i], arraysize(buffers[i]), &mf);
    ASSERTEQ(mf.start, buffers[i]);
    ASSERTEQ(mf.end, buffers[i] + arraysize(buffers[i]));

    mfsync(&buf, &len, &mf);
    ASSERTEQ(buf, (void *)buffers[i]);
    ASSERTEQ((int)len, arraysize(buffers[i]));

    mfsync(&buf, nullptr, &mf);
    ASSERTEQ(buf, (void *)buffers[i]);

    mfsync(nullptr, &len, &mf);
    ASSERTEQ((int)len, arraysize(buffers[i]));

    mfsync(nullptr, nullptr, &mf);
  }
}

struct buffer_data
{
  char *buffer;
  size_t buffer_len;
  boolean has8;
  boolean has16;
  boolean has32;
  boolean has64;
  boolean has128;
};

UNITTEST(mfhasspace)
{
  char buffer8[8];
  char buffera[16];
  char bufferb[32];
  char bufferc[64];
  char bufferd[128];
  char buffere[256];
  struct buffer_data pairs[] =
  {
    { buffer8, arraysize(buffer8), true, false, false, false, false },
    { buffera, arraysize(buffera), true, true,  false, false, false },
    { bufferb, arraysize(bufferb), true, true,  true,  false, false },
    { bufferc, arraysize(bufferc), true, true,  true,  true,  false },
    { bufferd, arraysize(bufferd), true, true,  true,  true,  true  },
    { buffere, arraysize(buffere), true, true,  true,  true,  true  },
  };
  struct memfile mf;
  int i;

  for(i = 0; i < arraysize(pairs); i++)
  {
    mfopen(pairs[i].buffer, pairs[i].buffer_len, &mf);
    ASSERTEQ(pairs[i].has8,   mfhasspace(8, &mf));
    ASSERTEQ(pairs[i].has16,  mfhasspace(16, &mf));
    ASSERTEQ(pairs[i].has32,  mfhasspace(32, &mf));
    ASSERTEQ(pairs[i].has64,  mfhasspace(64, &mf));
    ASSERTEQ(pairs[i].has128, mfhasspace(128, &mf));

    mf.current = (mf.end - mf.start)/2 + mf.start;
    ASSERTEQ(pairs[i].has16,  mfhasspace(8, &mf));
    ASSERTEQ(pairs[i].has32,  mfhasspace(16, &mf));
    ASSERTEQ(pairs[i].has64,  mfhasspace(32, &mf));
    ASSERTEQ(pairs[i].has128, mfhasspace(64, &mf));
  }
}

UNITTEST(mfmove)
{
  const int OFFSET = 96;
  unsigned char buffera[128];
  unsigned char bufferb[256];
  unsigned char bufferc[64];
  struct memfile mf;

  static_assert(arraysize(bufferc) < OFFSET,
   "OFFSET should be larger than the size of bufferc.");

  mfopen(buffera, arraysize(buffera), &mf);
  mf.current = mf.start + 96;
  ASSERTEQ(mf.current, buffera + 96);

  mfmove(bufferb, arraysize(bufferb), &mf);
  ASSERTEQ(mf.start, bufferb);
  ASSERTEQ(mf.end, bufferb + arraysize(bufferb));
  ASSERTEQ(mf.current, bufferb + 96);

  mfmove(bufferc, arraysize(bufferc), &mf);
  ASSERTEQ(mf.start, bufferc);
  ASSERTEQ(mf.end, bufferc + arraysize(bufferc));
  ASSERTEQ(mf.current, bufferc + arraysize(bufferc));
  ASSERT(mf.current != bufferc + 96);
}

UNITTEST(mfresize)
{
  const int BUFSIZE = 128;
  const int NEWSIZE = 256;
  void *buf = malloc(BUFSIZE);
  struct memfile mf;
  size_t size;

  ASSERT(buf);
  mfopen(buf, BUFSIZE, &mf);
  mfresize(NEWSIZE, &mf);
  mfsync(&buf, &size, &mf);
  ASSERTEQ(size, NEWSIZE);

  mfresize(BUFSIZE, &mf);
  mfsync(&buf, &size, &mf);
  ASSERTEQ(size, BUFSIZE);

  free(buf);
}

struct seq
{
  long int position;
  int next;
  int retval;
};

UNITTEST(mfseek_mftell)
{
  unsigned char buffer[256];
  struct memfile mf;
  static const int offs_safe[] =
  {
    128,
    256,
    0,
    32,
    79,
    84,
    255,
    1,
    172,
  };
  static const int offs_unsafe[] =
  {
    -1,
    -1000,
    -128731282,
  };
  static const int offs_maybe_safe[] =
  {
    arraysize(buffer) + 1,
    1024,
    9999,
    293819028,
    INT_MAX,
  };
  int ret;
  int i;

  mfopen(buffer, arraysize(buffer), &mf);

  SECTION(mftell)
  {
    for(i = 0; i < arraysize(offs_safe); i++)
    {
      mf.current = mf.start + offs_safe[i];
      ASSERTEQ(mftell(&mf), offs_safe[i]);
    }
  }

  SECTION(seek_set)
  {
    for(i = 0; i < arraysize(offs_safe); i++)
    {
      ret = mfseek(&mf, offs_safe[i], SEEK_SET);
      ASSERTEQ(ret, 0);
      ASSERTEQ(mftell(&mf), offs_safe[i]);
    }

    ASSERT(!mfseek(&mf, 129, SEEK_SET));

    for(i = 0; i < arraysize(offs_unsafe); i++)
    {
      ret = mfseek(&mf, offs_unsafe[i], SEEK_SET);
      ASSERTEQ(ret, -1);
      ASSERTEQ(mftell(&mf), 129);
    }

    for(i = 0; i < arraysize(offs_maybe_safe); i++)
    {
      ret = mfseek(&mf, offs_maybe_safe[i], SEEK_SET);
      ASSERTEQ(ret, -1);
      ASSERTEQ(mftell(&mf), 129);
    }
  }

  SECTION(seek_cur)
  {
    static const seq sequence[] =
    {
      {   0,   32,  0 },
      {  32,  -17,  0 },
      {  15,  200,  0 },
      { 215,  -80,  0 },
      { 135,   62,  0 },
      { 197,  100, -1 },
      { 197, -500, -1 },
      { 197,    0,  0 }
    };

    for(i = 0; i < arraysize(sequence); i++)
    {
      ASSERTEQ(mftell(&mf), sequence[i].position);
      ret = mfseek(&mf, sequence[i].next, SEEK_CUR);
      ASSERTEQ(ret, sequence[i].retval);
    }
  }

  SECTION(seek_end)
  {
    mfseek(&mf, 0, SEEK_END);
    ASSERTEQ(mftell(&mf), arraysize(buffer));
  }

  SECTION(seek_past_end)
  {
    mf.seek_past_end = true;

    for(i = 0; i < arraysize(offs_maybe_safe); i++)
    {
      ret = mfseek(&mf, offs_maybe_safe[i], SEEK_SET);
      ASSERTEQ(ret, 0);
      ASSERTEQ(mftell(&mf), offs_maybe_safe[i]);
      ASSERTEQ(mfhasspace(1, &mf), false);
    }
  }
}

UNITTEST(read_write)
{
  static const unsigned char data8[] =
  {
    0x00, 0x01, 0xFF, 0xFE, 0x7F, 0x53, 0xA3, 0xD8,
    0xFF, 0x00, 0x12, 0x34, 0x45, 0x67, 0x89, 0xAB,
  };
  static const uint16_t data16[] =
  {
    0x0100, 0xFEFF, 0x537F, 0xD8A3,
    0x00FF, 0x3412, 0x6745, 0xAB89,
  };
  static const int32_t data32s[] =
  {
    (int)(0xFEFF0100), (int)(0xD8A3537F),
    (int)(0x341200FF), (int)(0xAB896745),
  };
  static const uint32_t data32u[] =
  {
    0xFEFF0100, 0xD8A3537F,
    0x341200FF, 0xAB896745,
  };

  const int SIZE = arraysize(data8);
  unsigned char dest[SIZE * 2];
  struct memfile mf;
  int res;
  int res2;
  int i;

  static_assert(arraysize(data16) * 2 == SIZE,
   "input array size doesn't match u16 output array size!");

  static_assert(arraysize(data32s) * 4 == SIZE,
   "input array size doesn't match s32 output array size!");

  static_assert(arraysize(data32u) * 4 == SIZE,
   "input array size doesn't match u32 output array size!");

  SECTION(mfgetc)
  {
    mfopen(data8, SIZE, &mf);

    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    for(i = 0; i < arraysize(data8); i++)
    {
      uint8_t tmp = mfgetc(&mf);
      ASSERTEQ(tmp, data8[i]);
    }
  }

  SECTION(mfgetw)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(i = 0; i < arraysize(data16); i++)
    {
      uint16_t tmp = mfgetw(&mf);
      ASSERTEQ(tmp, data16[i]);
    }
  }

  SECTION(mfgetd)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(i = 0; i < arraysize(data32s); i++)
    {
      int tmp = mfgetd(&mf);
      ASSERTEQ(tmp, data32s[i]);
    }
  }

  SECTION(mfgetud)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(data8, SIZE, &mf);

    for(i = 0; i < arraysize(data32u); i++)
    {
      uint32_t tmp = mfgetud(&mf);
      ASSERTEQ(tmp, data32u[i]);
    }
  }

  SECTION(mfread)
  {
    mfopen(data8, SIZE, &mf);

    res = mfread(dest, SIZE, 1, &mf);
    ASSERTEQ(res, 1);
    res = memcmp(data8, dest, SIZE);
    ASSERTEQ(res, 0);

    mf.current = mf.start;
    res = mfread(dest, 1, SIZE, &mf);
    ASSERTEQ(res, SIZE);
    res = memcmp(data8, dest, SIZE);
    ASSERTEQ(res, 0);

    mf.current = mf.start;
    res = mfread(dest, SIZE/2, 1, &mf);
    res2 = mfread(dest + SIZE/2, 1, SIZE/2, &mf);
    ASSERTEQ(res, 1);
    ASSERTEQ(res2, SIZE/2);
    res = memcmp(data8, dest, SIZE);
    ASSERTEQ(res, 0);

    mf.current = mf.start;
    res = mfread(dest, SIZE, 2, &mf);
    ASSERTEQ(res, 1);

    mf.current = mf.start;
    res = mfread(dest, 3, SIZE*2/3, &mf);
    ASSERTEQ(res, SIZE/3);

    res = mfread(dest, 1, SIZE - SIZE/3*3 + 1, &mf);
    ASSERTEQ(res, SIZE - SIZE/3*3);

    res = mfread(dest, 1, 1, &mf);
    ASSERTEQ(res, 0);
  }

  SECTION(mfputc)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(dest, arraysize(dest), &mf);

    for(i = 0; i < arraysize(data8); i++)
    {
      res = mfputc(data8[i], &mf);
      ASSERTEQ((unsigned)res, data8[i]);
      ASSERTEQ((unsigned)res, dest[i]);
    }
  }

  SECTION(mfputw)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(dest, arraysize(dest), &mf);

    for(i = 0; i < arraysize(data16); i++)
    {
      mfputw(data16[i], &mf);
      ASSERTEQ((uint16_t)dest[i * 2 + 0], data16[i] & 0xFF);
      ASSERTEQ((uint16_t)dest[i * 2 + 1], (data16[i] >> 8) & 0xFF);
    }
  }

  SECTION(mfputd)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(dest, arraysize(dest), &mf);

    for(i = 0; i < arraysize(data32s); i++)
    {
      mfputd(data32s[i], &mf);
      ASSERTEQ((int)dest[i * 4 + 0], data32s[i] & 0xFF);
      ASSERTEQ((int)dest[i * 4 + 1], (data32s[i] >> 8) & 0xFF);
      ASSERTEQ((int)dest[i * 4 + 2], (data32s[i] >> 16) & 0xFF);
      ASSERTEQ((int)dest[i * 4 + 3], (data32s[i] >> 24) & 0xFF);
    }
  }

  SECTION(mfputud)
  {
    // NOTE: this function does not perform bounds checks since when using it
    // it is assumed these have already been performed.
    mfopen(dest, arraysize(dest), &mf);

    for(i = 0; i < arraysize(data32u); i++)
    {
      mfputud(data32u[i], &mf);
      ASSERTEQ((uint32_t)dest[i * 4 + 0], data32u[i] & 0xFF);
      ASSERTEQ((uint32_t)dest[i * 4 + 1], (data32u[i] >> 8) & 0xFF);
      ASSERTEQ((uint32_t)dest[i * 4 + 2], (data32u[i] >> 16) & 0xFF);
      ASSERTEQ((uint32_t)dest[i * 4 + 3], (data32u[i] >> 24) & 0xFF);
    }
  }

  SECTION(mfwrite)
  {
    mfopen(dest, SIZE, &mf);

    res = mfwrite(data8, SIZE, 1, &mf);
    ASSERTEQ(res, 1);
    res = memcmp(data8, dest, SIZE);
    ASSERTEQ(res, 0);

    mf.current = mf.start;
    res = mfwrite(data8, 1, SIZE, &mf);
    ASSERTEQ(res, SIZE);
    res = memcmp(data8, dest, SIZE);
    ASSERTEQ(res, 0);

    mf.current = mf.start;
    res = mfwrite(data8, SIZE/2, 1, &mf);
    res2 = mfwrite(data8 + SIZE/2, 1, SIZE/2, &mf);
    ASSERTEQ(res, 1);
    ASSERTEQ(res2, SIZE/2);
    res = memcmp(data8, dest, SIZE);
    ASSERTEQ(res, 0);

    mf.current = mf.start;
    res = mfwrite(data8, SIZE, 2, &mf);
    ASSERTEQ(res, 1);

    mf.current = mf.start;
    res = mfwrite(data8, 3, SIZE*2/3, &mf);
    ASSERTEQ(res, SIZE/3);

    res = mfwrite(data8, 1, SIZE - SIZE/3*3 + 1, &mf);
    ASSERTEQ(res, SIZE - SIZE/3*3);

    res = mfwrite(data8, 1, 1, &mf);
    ASSERTEQ(res, 0);
  }
}

struct memsafegets_data
{
  const char * const input;
  const char * const output[8];
};

UNITTEST(mfsafegets)
{
  // NOTE: classic Mac line ends not currently supported, don't test...
  static const memsafegets_data data[] =
  {
    {
      "a string\nanother string\n",
      {
        "a string",
        "another string",
        nullptr
      }
    },
    {
      "I'am wining this is so cool\r\nThere is no one versing you though\r\n",
      {
        "I'am wining this is so cool",
        "There is no one versing you though",
        nullptr
      }
    },
    {
      ":loop\nset a 123\nwait 1\ngoto loop\n",
      {
        ":loop",
        "set a 123",
        "wait 1",
        "goto loop",
        nullptr
      }
    },
    {
      ":loop\r\nset a 123\r\nwait 1\r\ngoto loop\r\n",
      {
        ":loop",
        "set a 123",
        "wait 1",
        "goto loop",
        nullptr
      }
    },
    {
      "a\nb\nc\nd\ne\nf\ng",
      { "a", "b", "c", "d", "e", "f", "g", nullptr }
    },
    {
      "a\r\nb\r\nc\r\nd\r\ne\r\nf\r\ng",
      { "a", "b", "c", "d", "e", "f", "g", nullptr }
    },
  };
  const int SHORT_BUF_LEN = 32;
  static const memsafegets_data short_data[] =
  {
    {
      "this should be truncated by the tiny buffer and split into multiple lines"
      "\nand still work after\n",
      {
        "this should be truncated by the",
        " tiny buffer and split into mul",
        "tiple lines",
        "and still work after",
        nullptr
      }
    },
    {
      "wtf are you\r\nusing this very small and pathetic\r\nbuffer for anyway"
      " it's 2020 already\r\n",
      {
        "wtf are you",
        "using this very small and pathe",
        "tic",
        "buffer for anyway it's 2020 alr",
        "eady",
        nullptr
      }
    }
  };
  struct memfile mf;
  char buffer[512];
  int i;
  int j;

  SECTION(NormalData)
  {
    for(i = 0; i < arraysize(data); i++)
    {
      mfopen(data[i].input, strlen(data[i].input), &mf);

      for(j = 0; j < arraysize(data[i].output); j++)
      {
        char *result = mfsafegets(buffer, arraysize(buffer), &mf);
        if(result && data[i].output[j])
        {
          ASSERTCMP(result, data[i].output[j]);
        }
        else
          ASSERTEQ(result, data[i].output[j]);
      }
    }
  }

  SECTION(SmallData)
  {
    for(i = 0; i < arraysize(short_data); i++)
    {
      mfopen(short_data[i].input, strlen(short_data[i].input), &mf);

      for(j = 0; j < arraysize(short_data[i].output); j++)
      {
        char *result = mfsafegets(buffer, SHORT_BUF_LEN, &mf);
        if(result && short_data[i].output[j])
        {
          ASSERTCMP(result, short_data[i].output[j]);
        }
        else
          ASSERTEQ(result, short_data[i].output[j]);
      }
    }
  }
}
