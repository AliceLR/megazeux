/**
 *  Copyright (C) 2003-2004  Alistair John Strachan  (alistair@devzero.co.uk)
 *
 *  This is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2, or (at your option) any later
 *  version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __ERROR_H
#define __ERROR_H

#include "types.h"

__G_BEGIN_DECLS

#define error(x) error_handle(__FILE__, __LINE__, x);

/**
 * possible errors
 */

typedef enum {
  FILE_OPEN,
  FILE_INVALID,
  CONVERT_ERROR,
  OUT_OF_DATA
} error_code;

/* function prototypes */
void error_handle (const char *file, int line, error_code error);

__G_END_DECLS

#endif /* __ERROR_H */
