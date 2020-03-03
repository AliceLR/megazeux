/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk
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

#ifndef __ENDIAN_H
#define __ENDIAN_H

/* If SDL is available, include the header and use SDL's configured
 * endianness. If it's not available, use a list of architectures (actually
 * borrowed from SDL 1.2.13) to figure out our likely endianness.
 */

#define PLATFORM_LIL_ENDIAN 0x1234
#define PLATFORM_BIG_ENDIAN 0x4321

#if defined(CONFIG_SDL) && !defined(SKIP_SDL)

#include <SDL_endian.h>

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define PLATFORM_BYTE_ORDER PLATFORM_BIG_ENDIAN
#else
#define PLATFORM_BYTE_ORDER PLATFORM_LIL_ENDIAN
#endif

#else // !CONFIG_SDL

#if defined(__BIG_ENDIAN__)
#define PLATFORM_BYTE_ORDER PLATFORM_BIG_ENDIAN
#elif defined(__LITTLE_ENDIAN__)
#define PLATFORM_BYTE_ORDER PLATFORM_LIL_ENDIAN
#elif defined(__hppa__) || \
    defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
    (defined(__MIPS__) && defined(__MISPEB__)) || \
    defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
    defined(__sparc__)
#define PLATFORM_BYTE_ORDER PLATFORM_BIG_ENDIAN
#else
#define PLATFORM_BYTE_ORDER PLATFORM_LIL_ENDIAN
#endif

#endif // CONFIG_SDL

/* ModPlug and XMP both use this name to find out about endianness. It's not
 * too bad to pollute our namespace with it, so just do so here.
 */
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
#define WORDS_BIGENDIAN
#endif

/**
 * Also try to get the platform bit width.
 * Emscripten natively supports 64-bit math when compiling to Wasm.
 */
#if defined(_WIN64) || defined(__EMSCRIPTEN__) || \
  (defined(__GNUC__) && \
    (defined(__x86_64__) || defined(__powerpc64__) || defined(__PPC64__) || \
     defined(__aarch64__)))
#define ARCHITECTURE_BITS 64
#else
#define ARCHITECTURE_BITS 32
#endif

#endif // __ENDIAN_H
