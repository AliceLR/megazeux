/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

/* Declarations */

#ifndef __AUDIO_XMP_H
#define __AUDIO_XMP_H

#include "compat.h"

__M_BEGIN_DECLS

struct audio_stream *construct_xmp_stream(char *filename, Uint32 frequency,
 Uint32 volume, Uint32 repeat);
void init_xmp(struct config_info *conf);

__M_END_DECLS

#endif  // __AUDIO_XMP_H
