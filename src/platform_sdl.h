/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#ifndef __PLATFORM_SDL_H
#define __PLATFORM_SDL_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform.h"

#include "SDL_stdinc.h"
#include "SDL_endian.h"

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define PLATFORM_BYTE_ORDER PLATFORM_BIG_ENDIAN
#else
#define PLATFORM_BYTE_ORDER PLATFORM_LIL_ENDIAN
#endif

__M_END_DECLS

#endif // __PLATFORM_SDL_H
