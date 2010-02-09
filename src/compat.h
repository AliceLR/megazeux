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

// Provide some compatibility macros for combined C/C++ binary

#ifndef __COMPAT_H
#define __COMPAT_H

#include "config.h"

#ifdef CONFIG_NDS
#include <nds.h>
#endif

#ifdef __cplusplus

#define __M_BEGIN_DECLS extern "C" {
#define __M_END_DECLS   }

#else

#define __M_BEGIN_DECLS
#define __M_END_DECLS

// gctypes.h tries to define a bool enum with true and false
// gctypes.h also tries to define a BOOL type which interferes
// with stdafx.h from libmodplug.
#ifndef CONFIG_WII
#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif
#else // !CONFIG_WII
#define BOOL _BOOL
#include <gctypes.h>
#undef BOOL
#endif // CONFIG_WII

// Could use C99's _Bool type instead?
typedef int bool;

#endif /* __cplusplus */

#ifdef CONFIG_EDITOR
#define __editor_maybe_static
#else
#define __editor_maybe_static static
#endif

#ifdef CONFIG_AUDIO
#if defined(CONFIG_MODPLUG) || defined(CONFIG_MIKMOD)
#define __audio_c_maybe_static
#else // !CONFIG_MODPLUG && !CONFIG_MIKMOD
#define __audio_c_maybe_static static
#endif // CONFIG_MODPLUG || CONFIG_MIKMOD
#else // !CONFIG_AUDIO
#define __audio_c_maybe_static static
#endif // CONFIG_AUDIO

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 1
#define __global __attribute__((externally_visible))
#else
#define __global
#endif

#ifdef _MSC_VER
#include "msvc.h"
#endif

/* The AmigaOS toolchain litters the namespace with strange MSVC-like
 * type declarations such as TEXT (const char *). What this does is
 * rename any of MegaZeux's uses of TEXT from data.h to _TEXT at
 * compile time after including the AmigaOS header.
 */
#ifdef __amigaos__
#define TEXT _TEXT
#endif

#endif // __COMPAT_H
