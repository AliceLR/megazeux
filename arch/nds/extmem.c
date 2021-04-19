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
#define IS_EXTRAM(ptr) (((u32)(ptr) & 0xFF000000) != 0x02000000)

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
static boolean nds_has_mspace[MSPACE_COUNT];
static mspace nds_mspace[MSPACE_COUNT];

static void nds_ext_print_info(void)
{
  static boolean has_printed = false;
  if(has_printed) return;
  has_printed = true;

#ifdef ALLOW_VRAM_MSPACE
  if(nds_has_mspace[MSPACE_VRAM])
    info("Using extra VRAM for board storage.\n");
#endif

  if(nds_has_mspace[MSPACE_SLOT_2])
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
    if(nds_has_mspace[i])
    {
      retval = mspace_malloc(nds_mspace[i], bytes);
      if(retval != NULL)
        break;
    }
  }

  return retval;
}

static inline void *nds_ext_realloc(void *mem, size_t bytes)
{
  void *retval;
  int i;

  for(i = 0; i < MSPACE_COUNT; i++)
  {
    if(nds_has_mspace[i])
    {
      retval = mspace_realloc(nds_mspace[i], mem, bytes);
      if(retval != NULL)
        break;
    }
  }

  return retval;
}

static void nds_ext_free(void *mem)
{
  int i;

  // Relies on PROCEED_ON_ERROR=1 and FOOTERS=1 in dlmalloc.
  // This combination means that mspace_free will return on
  // pointers not originating from its mspace, and deallocate
  // otherwise.
  for(i = 0; i < MSPACE_COUNT; i++)
    if(nds_has_mspace[i])
      mspace_free(nds_mspace[i], mem);
}

static void nds_ext_unlock()
{
  if(nds_has_mspace[MSPACE_SLOT_2])
    ram_unlock();
}

static void nds_ext_lock()
{
  if(nds_has_mspace[MSPACE_SLOT_2])
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

boolean nds_ram_init(RAM_TYPE type)
{
  int i;

  // Clear mspace flags.
  memset(nds_has_mspace, 0, sizeof(nds_has_mspace));

  // Allocate VRAM mspace.
#ifdef ALLOW_VRAM_MSPACE
  // Use VRAM banks C-G as mspace.
  // See internals_notes.txt for details.
  vramSetBankC(VRAM_C_LCD);
  vramSetBankD(VRAM_D_LCD);
  vramSetBankE(VRAM_E_LCD);
  vramSetBankF(VRAM_F_LCD);
  vramSetBankG(VRAM_G_LCD);
  nds_mspace[MSPACE_VRAM] = create_mspace_with_base((u16*) 0x06840000, 0x06898000 - 0x06840000, 0);
  nds_has_mspace[MSPACE_VRAM] = true;
#endif

  // Check for RAM expansion.
  if(ram_init(type))
  {
    // Initialize an mspace for this RAM.
    slot2_base = (u16 *)ram_unlock();
    slot2_capacity = ram_size();
    nds_mspace[MSPACE_SLOT_2] = create_mspace_with_base(slot2_base, slot2_capacity, 0);
    nds_has_mspace[MSPACE_SLOT_2] = true;
    nds_ext_lock();
  }

  has_extmem = false;
  for(i = 0; i < MSPACE_COUNT; i++)
  {
    if(nds_has_mspace[i])
    {
      has_extmem = true;
      break;
    }
  }

  return has_extmem;
}
