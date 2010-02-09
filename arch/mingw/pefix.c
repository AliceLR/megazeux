/**
 * Zeroes the TimeDateStamp fields and recomputes checksums in Microsoft
 * Portable-Executable (PE) files so that two identical programs generate
 * two identical binaries when compiled with the same toolchain.
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static long ftell_and_rewind(FILE *f)
{
  long size;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);

  return size;
}

static int pe_csum(FILE *f, uint32_t *csum)
{
  uint32_t sum = 0;
  long count, len;
  uint16_t w;

  len = ftell_and_rewind(f);

  for(count = len; count > 1; count -= 2)
  {
    if(fread(&w, sizeof(uint16_t), 1, f) != 1)
      return 1;
    sum += w;
  }

  if(count == 1)
  {
    w = 0;
    if(fread(&w, sizeof(uint8_t), 1, f) != 1)
      return 1;
    sum += w;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);

  *csum = (uint32_t)((uint16_t)sum + len);
  return 0;
}

int main(int argc, char *argv[])
{
  long csum_offset;
  uint32_t ul;
  int ret = 1;
  FILE *f;

  if(argc != 2)
  {
    fprintf(stderr, "usage: %s [exe]\n", argv[0]);
    goto err_out;
  }

  f = fopen(argv[1], "r+b");
  if(!f)
  {
    fprintf(stderr, "Failed to open %s for read/write\n", argv[1]);
    goto err_out; 
  }

  // Check 'MZ' magic
  if(fgetc(f) != 'M')
    goto err_magic;
  if(fgetc(f) != 'Z')
    goto err_magic;

  // Next 58 bytes of DOS header aren't interesting
  if(fseek(f, 58, SEEK_CUR))
    goto err_read;

  // Read so called 'lfanew' field; offset to PE header
  if(fread(&ul, sizeof(uint32_t), 1, f) != 1)
    goto err_read;

  // Skip to lfanew offset (move us to PE header)
  if(fseek(f, ul, SEEK_SET))
    goto err_read;

  // Check 'PE\x00\x00' magic
  if(fgetc(f) != 'P')
    goto err_magic;
  if(fgetc(f) != 'E')
    goto err_magic;
  if(fgetc(f) != 0)
    goto err_magic;
  if(fgetc(f) != 0)
    goto err_magic;

  // Skip Machine, NumberOfSections
  if(fseek(f, 2 + 2, SEEK_CUR))
    goto err_read;

  // Re-write TimeDateStamp as zero (new Checksum computed later)
  ul = 0;
  if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
    goto err_write;

  // Skip PointerToSymbolTable, NumberOfSymbols, SizeOfOptionalHeader
  // Characteristics
  if(fseek(f, 4 + 4 + 2 + 2, SEEK_CUR))
    goto err_read;

  // Skip Magic, MajorLinkerVersion, MinorLinkerVersion, SizeOfCode,
  // SizeOfInitializedData, SizeOfUninitializedData, AddressOfEntryPoint,
  // BaseOfCode, BaseOfData, ImageBase, SectionAlignment, FileAlignment,
  // MajorOperatingSystemVersion, MinorOperatingSystemVersion,
  // MajorImageVersion, MinorImageVersion, MajorSubsystemVersion,
  // MinorSubsystemVersion, Reserved1, SizeOfImage, SizeOfHeaders
  if(fseek(f, 2 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4 +
              4 + 4 + 4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4, SEEK_CUR))
    goto err_read;

  // Squash obsolete Checksum with zero
  ul = 0;
  csum_offset = ftell(f);
  if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
    goto err_write;

  // Compute and write back valid checksum
  if(pe_csum(f, &ul))
    goto err_read;
  if(fseek(f, csum_offset, SEEK_SET))
    goto err_read;
  if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
    goto err_write;

  ret = 0;
err_fclose:
  fclose(f);
err_out:
  return ret;

err_magic:
  fprintf(stderr, "File format not understood\n");
  goto err_fclose;

err_read:
  fprintf(stderr, "I/O error reading from file\n");
  goto err_fclose;

err_write:
  fprintf(stderr, "I/O error writing to file\n");
  goto err_fclose;
}
