/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

/* Declaration for ERROR.CPP */

#ifndef __ERROR_H
#define __ERROR_H

#include "compat.h"

#define ERROR_OPT_FAIL  1
#define ERROR_OPT_RETRY 2
#define ERROR_OPT_EXIT  4
#define ERROR_OPT_OK    8
#define ERROR_OPT_HELP  16

__M_BEGIN_DECLS

//Call for an error OR a warning. Type=0 for a warning, 1 for a recoverable
//error, 2 for a fatal error. Options are (bits set in options and returned
//as action) FAIL=1, RETRY=2, EXIT TO DOS=4, OK=8, HELP=16 (OK is for usually
//only for warnings) Code is a specialized error code for debugging purposes.
//Type of 3 for a critical error.

CORE_LIBSPEC int error(const char *string, unsigned int type,
 unsigned int options, unsigned int code);

__M_END_DECLS

#endif // __ERROR_H
