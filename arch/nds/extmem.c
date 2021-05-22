/* MegaZeux
 *
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
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

/* This file is only used on NDS, and is not even compiled on
 * other systems. It handles transferral of main-memory data structures
 * into slower external memory to reduce memory pressure.
 */

#include "../../src/error.h"
#include "../../src/extmem.h"
#include "../../src/robot.h"
#include "../../src/util.h"

#include "dlmalloc.h"
#include "ram.h"
#include "extmem.h"

#define ALLOW_VRAM_MSPACE

struct extram_mspace_def
{
  u32 start;
  u32 end;
};

enum extram_mspace
{
  MSPACE_VRAM = 0,
  MSPACE_TWL_WRAM,
  MSPACE_SLOT_2,
  MSPACE_COUNT
};

static u16 *slot2_base;
static size_t slot2_capacity;
static boolean has_extmem;
static mspace nds_mspace[MSPACE_COUNT];
static struct extram_mspace_def nds_mspace_def[MSPACE_COUNT];

static void nds_ext_print_info(void)
{
  static boolean has_printed = false;
  if(has_printed) return;
  has_printed = true;

#ifdef ALLOW_VRAM_MSPACE
  if(nds_mspace_def[MSPACE_VRAM].start != 0)
    info("Using extra VRAM for board storage.\n");
#endif

  if(nds_mspace_def[MSPACE_SLOT_2].start != 0)
    info("Using '%s' RAM expansion with capacity of %d (base %p).\n",
     ram_type_string(), slot2_capacity, (void *)slot2_base);
  else
    info("No RAM expansion detected.\n");
}

static void *nds_ext_malloc(size_t bytes)
{
  void *retval = NULL;
  int i;

  for(i = 0; i < MSPACE_COUNT; i++)
  {
    if(nds_mspace_def[i].start != 0)
    {
      retval = mspace_malloc(nds_mspace[i], bytes);
      if(retval != NULL)
        break;
    }
  }

  return retval;
}

// TODO: Support reallocation from one mspace to another?
static inline void *nds_ext_realloc(void *mem, size_t bytes)
{
  int i;

  for(i = 0; i < MSPACE_COUNT; i++)
    if(((u32)mem) >= nds_mspace_def[i].start && ((u32)mem) < nds_mspace_def[i].end)
      return mspace_realloc(nds_mspace[i], mem, bytes);

  return NULL;
}

static void nds_ext_free(void *mem)
{
  int i;

  for(i = 0; i < MSPACE_COUNT; i++)
    if(((u32)mem) >= nds_mspace_def[i].start && ((u32)mem) < nds_mspace_def[i].end)
      mspace_free(nds_mspace[i], mem);
}

static void nds_ext_unlock()
{
  if(nds_mspace_def[MSPACE_SLOT_2].start != 0)
    ram_unlock();
}

static void nds_ext_lock()
{
  if(nds_mspace_def[MSPACE_SLOT_2].start != 0)
    ram_lock();
}

/* Attempt to allocate a memory mapped buffer in extra RAM. */
uint32_t *platform_extram_alloc(size_t len)
{
  return nds_ext_malloc(len);
}

/* Attempt to reallocate a memory mapped buffer in extra RAM. */
uint32_t *platform_extram_resize(void *buffer, size_t len)
{
  return nds_ext_realloc(buffer, len);
}

/* Free a memory mapped buffer in extra RAM. */
void platform_extram_free(void *buffer)
{
  nds_ext_free(buffer);
}

void platform_extram_lock(void)
{
  nds_ext_print_info();
  nds_ext_unlock();
}

void platform_extram_unlock(void)
{
  nds_ext_lock();
}

static void nds_create_mspace_with_base(enum extram_mspace id, void *base, size_t capacity)
{
  nds_mspace_def[id].start = (u32)base;
  nds_mspace_def[id].end = (u32)base + capacity;
  nds_mspace[id] = create_mspace_with_base(base, capacity, 0);
}

boolean nds_ram_init(RAM_TYPE type)
{
  int i;

  // Clear mspace flags.
  memset(nds_mspace_def, 0, sizeof(nds_mspace_def));

  // Allocate VRAM mspace.
#ifdef ALLOW_VRAM_MSPACE
  // Use VRAM banks C-G as mspace.
  // See internals_notes.txt for details.
  vramSetBankC(VRAM_C_LCD);
  vramSetBankD(VRAM_D_LCD);
  vramSetBankE(VRAM_E_LCD);
  vramSetBankF(VRAM_F_LCD);
  vramSetBankG(VRAM_G_LCD);
  nds_create_mspace_with_base(MSPACE_VRAM, (u16 *)0x06840000, 0x06898000 - 0x06840000);
#endif

  // Check for RAM expansion.
  if(ram_init(type))
  {
    // Initialize an mspace for this RAM.
    slot2_base = (u16 *)ram_unlock();
    slot2_capacity = ram_size();
    nds_create_mspace_with_base(MSPACE_SLOT_2, slot2_base, slot2_capacity);
    nds_ext_lock();
  }

  has_extmem = false;
  for(i = 0; i < MSPACE_COUNT; i++)
  {
    if(nds_mspace_def[i].start > 0)
    {
      has_extmem = true;
      break;
    }
  }

  return has_extmem;
}
