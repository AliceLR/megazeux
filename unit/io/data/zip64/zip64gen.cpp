#if 0
gcc -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined zip64gen.cpp -ozip64gen
exit 0
#endif

#include <stdint.h>
#include <stdio.h>

#include "../../../../src/io/memfile.h"

enum VariantFlags
{
  // File headers
  DataDescriptor    = (1 << 0),
  Local64           = (1 << 1),
  Uncompressed64    = (1 << 2),
  Compressed64      = (1 << 3),
  Offset64          = (1 << 4),
  // EOCD
  Records64         = (1 << 5),
  DirectorySize64   = (1 << 6),
  DirectoryOffset64 = (1 << 7),
  // Masks
  Central64         = Uncompressed64|Compressed64|Offset64,
  EOCD64            = Records64|DirectorySize64|DirectoryOffset64,
};


static constexpr const char contents[] =
  "1234567890 1234567890 1234567890\r\n"
  "1234567890 1234567890 1234567890\r\n"
  "1234567890 1234567890 1234567890";
static constexpr uint32_t content_size = 100;
static constexpr uint32_t content_crc = 0xE3046765;


static void local(int flags, FILE *out)
{
  uint8_t buf[30 + 4 + 20]{};
  struct memfile mf;

  mfopen_wr(buf, sizeof(buf), &mf);

  mfwrite("PK\x03\x04", 4, 1, &mf);
  mfputw((flags & Local64) ? 45 : 20, &mf);
  mfputw((flags & DataDescriptor) ? 0x0008 : 0x0000, &mf);
  mfputw(0, &mf);   // Method
  mfputud(0, &mf);  // Date and time

  // CRC
  if(flags & DataDescriptor)
    mfputud(0, &mf);
  else
    mfputud(content_crc, &mf);

  // Compressed, uncompressed
  if(flags & Local64)
  {
    mfputud(0xffffffff, &mf);
    mfputud(0xffffffff, &mf);
  }
  else
  {
    if(flags & DataDescriptor)
    {
      mfputud(0, &mf);
      mfputud(0, &mf);
    }
    else
    {
      mfputud(content_size, &mf);
      mfputud(content_size, &mf);
    }
  }

  mfputw(4, &mf); // Filename
  mfputw((flags & Local64) ? 20 : 0, &mf); // Extra
  mfwrite("test", 4, 1, &mf);

  if(flags & Local64)
  {
    mfputw(0x0001, &mf);
    mfputw(16, &mf);

    if(flags & DataDescriptor)
    {
      mfputuq(0, &mf);
      mfputuq(0, &mf);
    }
    else
    {
      mfputuq(content_size, &mf);
      mfputuq(content_size, &mf);
    }
  }
  fwrite(buf, mftell(&mf), 1, out);
}

static void descriptor(int flags, FILE *out)
{
  uint8_t buf[20];
  struct memfile mf;

  mfopen_wr(buf, sizeof(buf), &mf);

  mfputud(content_crc, &mf);

  if(flags & Local64)
  {
    mfputuq(content_size, &mf);
    mfputuq(content_size, &mf);
  }
  else
  {
    mfputud(content_size, &mf);
    mfputud(content_size, &mf);
  }

  fwrite(buf, mftell(&mf), 1, out);
}


static void central(int flags, FILE *out)
{
  uint8_t buf[46 + 4 + 32]{};
  struct memfile mf;
  size_t sz = 0;

  mfopen_wr(buf, sizeof(buf), &mf);

  mfwrite("PK\x01\x02", 4, 1, &mf);
  mfputw((flags & Central64) ? 45 : 20, &mf);
  mfputw((flags & Central64) ? 45 : 20, &mf);
  mfputw((flags & DataDescriptor) ? 0x0008 : 0x0000, &mf);
  mfputw(0, &mf);   // Method
  mfputud(0, &mf);  // Date and time
  mfputud(content_crc, &mf);

  // Compressed
  if(flags & Compressed64)
  {
    mfputud(0xffffffff, &mf);
    sz += 8;
  }
  else
    mfputud(content_size, &mf);

  // Uncompressed
  if(flags & Uncompressed64)
  {
    mfputud(0xffffffff, &mf);
    sz += 8;
  }
  else
    mfputud(content_size, &mf);

  if(flags & Offset64)
    sz += 8;

  mfputw(4, &mf); // Filename
  mfputw((flags & Central64) ? sz + 4 : 0, &mf); // Extra
  mfputw(0, &mf); // Comment
  mfputw(0, &mf); // Disk number start
  mfputw(0, &mf); // Internal file attributes
  mfputud(0, &mf); // External file attributes

  // Offset of local header
  if(flags & Offset64)
    mfputud(0xffffffff, &mf);
  else
    mfputud(0, &mf);

  mfwrite("test", 4, 1, &mf);

  if(flags & Central64)
  {
    mfputw(0x0001, &mf);
    mfputw(sz, &mf);

    if(flags & Uncompressed64)
      mfputuq(content_size, &mf);
    if(flags & Compressed64)
      mfputuq(content_size, &mf);
    if(flags & Offset64)
      mfputuq(0, &mf);
  }
  fwrite(buf, mftell(&mf), 1, out);
}


static void eocd64(int flags, size_t offset, size_t size, FILE *out)
{
  uint8_t buf[56];
  struct memfile mf;

  mfopen_wr(buf, sizeof(buf), &mf);

  mfwrite("PK\x06\x06", 4, 1, &mf);
  mfputuq(44, &mf);
  mfputw(45, &mf);
  mfputw(45, &mf);
  mfputud(0, &mf);
  mfputud(0, &mf);

  // Despite the spec implying the EOCD64 fields are only loaded
  // if a maximum value is found in the corresponding normal field,
  // PKZIP 6.0, Info-ZIP, and 7-Zip unconditionally replace all
  // fields with those from the EOCD64, so write them all.

  mfputuq(1, &mf); // Records in volume
  mfputuq(1, &mf); // Total records

  mfputuq(size, &mf); // Size of central directory
  mfputuq(offset, &mf); // Offset of central directory in current volume

  fwrite(buf, mftell(&mf), 1, out);
}

static void locator64(int flags, size_t offset, FILE *out)
{
  uint8_t buf[20];
  struct memfile mf;

  mfopen_wr(buf, sizeof(buf), &mf);

  mfwrite("PK\x06\x07", 4, 1, &mf);
  mfputud(0, &mf);
  mfputuq(offset, &mf);
  mfputud(1, &mf);

  fwrite(buf, mftell(&mf), 1, out);
}

static void eocd(int flags, size_t offset, size_t size, FILE *out)
{
  uint8_t buf[22];
  struct memfile mf;

  mfopen_wr(buf, sizeof(buf), &mf);

  mfwrite("PK\x05\x06", 4, 1, &mf);
  mfputw(0, &mf);
  mfputw(0, &mf);

  if(flags & Records64)
  {
    mfputw(0xffff, &mf);
    mfputw(0xffff, &mf);
  }
  else
  {
    mfputw(1, &mf);
    mfputw(1, &mf);
  }

  if(flags & DirectorySize64)
    mfputud(0xfffffffful, &mf);
  else
    mfputud(size, &mf);

  if(flags & DirectoryOffset64)
    mfputud(0xfffffffful, &mf);
  else
    mfputud(offset, &mf);

  mfputw(0, &mf);

  fwrite(buf, mftell(&mf), 1, out);
}


static void gen(const char *dest, int flags)
{
  FILE *out = fopen_unsafe(dest, "wb");

  local(flags, out);
  fwrite(contents, content_size, 1, out);
  if(flags & DataDescriptor)
    descriptor(flags, out);

  long directory_offset = ftell(out);
  central(flags, out);
  long eocd64_offset = ftell(out);
  long directory_size = eocd64_offset - directory_offset;

  if(flags & EOCD64)
  {
    eocd64(flags, directory_offset, directory_size, out);
    locator64(flags, eocd64_offset, out);
  }
  eocd(flags, directory_offset, directory_size, out);
}

int main()
{
  gen("local64.zip",    Local64);
  gen("localcd64.zip",  Local64|Central64);
  gen("localdd64.zip",  Local64|DataDescriptor);
  gen("localcddd64.zip",Local64|DataDescriptor|Central64);
  gen("cd64.zip",       Central64);
  gen("cduncomp64.zip", Uncompressed64);
  gen("cdcomp64.zip",   Compressed64);
  gen("cdoffset64.zip", Offset64);

  gen("eocd64.zip",     EOCD64);
  gen("eocd64rc.zip",   Records64);
  gen("eocd64sz.zip",   DirectorySize64);
  gen("eocd64of.zip",   DirectoryOffset64);

  gen("all64.zip",      EOCD64|Local64|Central64);
  gen("alldd64.zip",    EOCD64|Local64|DataDescriptor|Central64);
  return 0;
}
