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

#ifdef __cplusplus

#define __M_BEGIN_DECLS extern "C" {
#define __M_END_DECLS   }

#else

#define __M_BEGIN_DECLS
#define __M_END_DECLS

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#endif /* __cplusplus */

#include "config.h"

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

#ifdef _MSC_VER
#include "msvc.h"
#endif

/* On the NDS, extra (slower) pluggable memory may be available.
 * In those cases, large allocations (such as board data) can be
 * moved there to free up space in internal memory.
 *
 * On non-DS platforms these are all mapped to stdlib functions,
 * except for lock/unlock, which become no-ops.
 */
#ifdef CONFIG_NDS

#include "nds_malloc.h"

#define ext_malloc  nds_ext_malloc
#define ext_realloc nds_ext_realloc
#define ext_free    nds_ext_free
#define ext_memcpy  nds_ext_memcpy
#define ext_lock    nds_ext_lock
#define ext_unlock  nds_ext_unlock

#else // !CONFIG_NDS

#define ext_malloc malloc
#define ext_realloc realloc
#define ext_free free
#define ext_memcpy memcpy
#define ext_lock(x)
#define ext_unlock(x)

#endif // CONFIG_NDS

#endif // __COMPAT_H
