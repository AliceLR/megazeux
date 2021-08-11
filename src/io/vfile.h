/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Types for vio.h and memfile.h so they don't have to be included in
 * world_struct.h and other high-traffic headers.
 */

#ifndef __IO_VFILE_H
#define __IO_VFILE_H

#include "../compat.h"

__M_BEGIN_DECLS

typedef struct vfile vfile;
typedef struct vdir vdir;
struct memfile;
struct stat;

__M_END_DECLS

#endif /* __IO_VFILE_H */
