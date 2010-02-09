/* MegaZeux
 *
 * Copyright (C) 2009 Alistair Strachan <alistair@devzero.co.uk>
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

#ifndef __VFS_ZIPIO_H
#define __VFS_ZIPIO_H

#include "compat.h"

#include <stdio.h>

__M_BEGIN_DECLS

typedef enum
{
  ZIPIO_SUCCESS,
  ZIPIO_OPEN_FAILED,
  ZIPIO_CLOSE_FAILED,
  ZIPIO_TELL_FAILED,
  ZIPIO_SEEK_FAILED,
  ZIPIO_READ_FAILED,
  ZIPIO_NO_CENTRAL_DIRECTORY,
  ZIPIO_NO_END_CENTRAL_DIRECTORY,
  ZIPIO_CORRUPT_CENTRAL_DIRECTORY,
  ZIPIO_CORRUPT_LOCAL_FILE_HEADER,
  ZIPIO_CORRUPT_DATETIME,
  ZIPIO_UNSUPPORTED_FEATURE,
  ZIPIO_UNSUPPORTED_MULTIPLE_DISKS,
  ZIPIO_UNSUPPORTED_ZIP64,
  ZIPIO_UNSUPPORTED_COMPRESSION_METHOD,
} zipio_error_t;

typedef struct zip_handle zip_handle_t;

const char *zipio_strerror(zipio_error_t err);
zipio_error_t zipio_open(const char *filename, zip_handle_t **_z);
zipio_error_t zipio_close(zip_handle_t *z);

__M_END_DECLS

#endif // __VFS_ZIPIO_H
