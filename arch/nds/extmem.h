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

#include "ram.h"

#ifndef NDS_EXTMEM_H
#define NDS_EXTMEM_H

// NDS-specific interface
// Kevin Vance
bool nds_ram_init(RAM_TYPE type);
void *nds_ext_malloc(size_t bytes);
void *nds_ext_realloc(void *mem, size_t bytes);
void nds_ext_free(void *mem);
void nds_ext_unlock(void);
void nds_ext_lock(void);
void nds_ext_copy(void *dest, void *src, size_t size);

// These functions take a pointer to a pointer to the memory region to swap,
// e.g. (&foo) for (char *foo).
bool nds_ext_in(void *normal_buffer, size_t size);
bool nds_ext_out(void *extra_buffer, size_t size);

#endif /* NDS_EXTMEM_H */
