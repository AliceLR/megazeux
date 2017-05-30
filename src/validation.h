/* MegaZeux
 *
 * Copyright (C) 2012 Alice Rowan <petrifiedrowan@gmail.com>
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

/* VALIDATION.H- Declarations for VALIDATION.C */

#ifndef __VALIDATION_H
#define __VALIDATION_H

#include "compat.h"

__M_BEGIN_DECLS

#include "const.h"

#include <stdlib.h>

enum val_result
{
  VAL_SUCCESS,
  VAL_VERSION,    // Version issue
  VAL_INVALID,    // Failed validation
  VAL_TRUNCATED,  // Passed validation until it hit EOF
  VAL_MISSING,    // file or ptr location in file does not exist
  VAL_ABORTED,    // Load aborted by user
  VAL_NEED_UNLOCK // Unlock world file.
};

CORE_LIBSPEC FILE * val_fopen(const char *filename);

CORE_LIBSPEC enum val_result validate_legacy_world_file(const char *filename,
 int savegame, int *end_of_global_offset, int decrypt_attempted);

__M_END_DECLS

#endif // __VALIDATION_H
