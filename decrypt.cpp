/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char magic_code[16] = "æRëòmMJ·‡²’ˆÞ‘$";

int get_pw_xor_code(char *password, int pro_method)
{
  int work = 85; // Start with 85... (01010101)
  int i;
  // Clear pw after first null

  for(i = strlen(password); i < 16; i++)
	{
    password[i] = 0;
	}

  for(i = 0; i < 15; i++)
  {
    //For each byte, roll once to the left and xor in pw byte if it
    //is an odd character, or add in pw byte if it is an even character.
    work <<= 1;
    if(work > 255)
      work ^= 257; // Wraparound from roll

    if(i & 1)
    {
      work += (signed char)password[i]; // Add (even byte)
      if(work > 255)
        work ^= 257; // Wraparound from add
    }
    else
    {
      work ^= (signed char)password[i]; // XOR (odd byte);
    }
  }
  // To factor in protection method, add it in and roll one last time
  work += pro_method;
  if(work > 255)
    work ^= 257;

  work <<= 1;
  if(work > 255)
    work ^= 257;

  // Can't be 0-
  if(work == 0)
    work = 86; // (01010110)
  // Done!
  return work;
}

void decrypt(char *file_name)
{
  FILE *source;
  FILE *dest;
  int file_length;
  int pro_method;
  int i;
  int len;
  char num_boards;
  char offset_low_byte;
  char xor_val;
  char password[15];
  char *file_buffer;
  char *src_ptr;

  source = fopen(file_name, "rb");
  fseek(source, 0, SEEK_END);
  file_length = ftell(source);
  fseek(source, 0, SEEK_SET);

  file_buffer = (char *)malloc(file_length);
  src_ptr = file_buffer;
  fread(file_buffer, file_length, 1, source);
  fclose(source);

  src_ptr += 25;

  dest = fopen(file_name, "wb");
  pro_method = *src_ptr;
  src_ptr++;

  // Get password
  memcpy(password, src_ptr, 15);
  src_ptr += 18;
  // First, normalize password...
  for(i = 0; i < 15; i++)
  {
    password[i] ^= magic_code[i];
    password[i] -= 0x12 + pro_method;
    password[i] ^= 0x8D;
  }

  // Xor code
  xor_val = get_pw_xor_code(password, pro_method);

  // Copy title
  fwrite(file_buffer, 25, 1, dest);

  fputc(0, dest);
  fputs("M\x02\x11", dest);
  len = file_length - 44;
  for(; len > 0; len--)
  {
    fputc((*src_ptr) ^ xor_val, dest);
    src_ptr++;
  }

  // Must fix all the absolute file positions so that they're 15
  // less now
  src_ptr = file_buffer + 4245;
  fseek(dest, 4230, SEEK_SET);
  offset_low_byte = src_ptr[0] ^ xor_val;
  fputc(offset_low_byte - 15, dest);
  if(offset_low_byte < 15)
  {
    fputc((src_ptr[1] ^ xor_val) - 1, dest);
  }
  else
  {
    fputc(src_ptr[1] ^ xor_val, dest);
  }

  fputc(src_ptr[2] ^ xor_val, dest);
  fputc(src_ptr[3] ^ xor_val, dest);

  src_ptr += 4;

  num_boards = ((*src_ptr) ^ xor_val);
  src_ptr++;

  // If custom SFX is there, run through and skip it
  if(!num_boards)
  {
    int sfx_length = (char)(src_ptr[0] ^ xor_val);
    sfx_length |= ((char)(src_ptr[1] ^ xor_val)) << 8;
    src_ptr += sfx_length + 2;
    num_boards = (*src_ptr) ^ xor_val;
    src_ptr++;
  }

  // Skip titles
  src_ptr += (25 * num_boards);
  // Synchronize source and dest positions
  fseek(dest, src_ptr - file_buffer - 15, SEEK_SET);

  // Offset boards
  for(i = 0; i < num_boards; i++)
  {
    // Skip length
    src_ptr += 4;
    fseek(dest, 4, SEEK_CUR);

    // Get offset
    offset_low_byte = src_ptr[0] ^ xor_val;
    fputc(offset_low_byte - 15, dest);
    if(offset_low_byte < 15)
    {
      fputc((src_ptr[1] ^ xor_val) - 1, dest);
    }
    else
    {
      fputc(src_ptr[1] ^ xor_val, dest);
    }
    fputc(src_ptr[2] ^ xor_val, dest);
    fputc(src_ptr[3] ^ xor_val, dest);
    src_ptr += 4;
  }
  free(file_buffer);
  fclose(dest);
}

