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

#ifdef __cplusplus

#define __M_BEGIN_DECLS extern "C" {
#define __M_END_DECLS   }

#else

#define __M_BEGIN_DECLS
#define __M_END_DECLS

#if !defined(CONFIG_WII) && !defined(CONFIG_NDS) && !defined(CONFIG_3DS)

#undef false
#undef true
#undef bool

enum
{
  false = 0,
  true  = 1,
};

#endif // !CONFIG_WII && !CONFIG_NDS

#endif /* __cplusplus */

typedef unsigned char boolean;

#ifdef CONFIG_3DS
#include <3ds.h>
#endif

#ifdef CONFIG_NDS
#include <nds.h>
#endif

#ifdef CONFIG_WII
#define BOOL _BOOL
#include <gctypes.h>
#undef BOOL
#endif

#ifdef CONFIG_EDITOR
#define __editor_maybe_static
#else
#define __editor_maybe_static static
#endif

#ifdef CONFIG_UPDATER
#define __updater_maybe_static
#else
#define __updater_maybe_static static
#endif

#ifdef CONFIG_UTILS
#define __utils_maybe_static
#else
#define __utils_maybe_static static
#endif

#ifdef _MSC_VER
#include "msvc.h"
#endif

#ifdef __APPLE__
// Patch in some potentially missing defines for older platforms that
// may be required by headers from our dependencies.
#include <TargetConditionals.h>
#include <Availability.h>

#ifndef TARGET_OS_TV
#define TARGET_OS_TV 0
#endif

#ifndef __has_feature
#define __has_feature(f) 0
#endif
#endif

#ifdef __WIN32__
// Usually defined in Windows headers but somehow those always seem to add 30%
// or more build time and we don't include them globally for any other purpose.
#define MAX_PATH 260
#endif

#ifdef __OpenBSD__
#include <sys/param.h>
// These macros conflict with internally-defined MZX macros
#undef MIN
#undef MAX

// unveil added in OpenBSD 6.3
#ifdef OpenBSD
#if OpenBSD >= 201805
#define PLEDGE_HAS_UNVEIL
#endif

#endif /* OpenBSD */
#endif /* __OpenBSD__ */

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

#if defined(CONFIG_MODULAR) && defined(__WIN32__)
#define LIBSPEC __declspec(dllimport)
#elif defined(CONFIG_MODULAR)
#define LIBSPEC __attribute__((visibility("default")))
#else
#define LIBSPEC
#endif

#ifndef CORE_LIBSPEC
#define CORE_LIBSPEC LIBSPEC
#endif

#ifndef EDITOR_LIBSPEC
#define EDITOR_LIBSPEC LIBSPEC
#endif

#ifndef AUDIO_LIBSPEC
#define AUDIO_LIBSPEC LIBSPEC
#endif

#ifndef UTILS_LIBSPEC
#define UTILS_LIBSPEC CORE_LIBSPEC
#endif

#ifdef CONFIG_UPDATER
#define UPDATER_LIBSPEC CORE_LIBSPEC
#else
#define UPDATER_LIBSPEC
#endif

__M_BEGIN_DECLS

#ifdef __GNUC__

// On GNU compilers we can mark fopen() as "deprecated" to remind callers
// to properly audit calls to it.
//
// Any case where a file to open is specified by a game, the path must first
// be translated, so that any case-insensitive matches will be used and
// safety checks will be applied. Callers should use either
// fsafetranslate()+fopen_unsafe(), or call fsafeopen() directly.
//
// Any case where a file comes from the editor, or from MZX-proper, it
// probably should not be translated. In this case fopen_unsafe() can be
// used directly.

#include <stdio.h>

static inline FILE *fopen_unsafe(const char *path, const char *mode)
 { return fopen(path, mode); }
static inline FILE *check_fopen(const char *path, const char *mode)
 __attribute__((deprecated));
static inline FILE *check_fopen(const char *path, const char *mode)
 { return fopen_unsafe(path, mode); }

#define fopen(path, mode) check_fopen(path, mode)

// These functions have the warn_unused_result attribute but in most cases
// right now we'd just print a warning. So, wrap them and print a warning.

#include <unistd.h>

static inline char *check_getcwd(char *buf, size_t size)
{
  char *ret = getcwd(buf, size);
  if(!ret)
  {
    fprintf(stderr, "WARNING: Failed getcwd from %s\n", buf);
    fflush(stderr);
  }
  return ret;
}

static inline int check_chdir(const char *path)
{
  int ret = chdir(path);
  if(ret)
  {
    fprintf(stderr, "WARNING: failed chdir to %s\n", path);
    fflush(stderr);
  }
  return ret;
}

#define getcwd(buf, size) check_getcwd(buf, size)
#define chdir(path) check_chdir(path)

#else // !__GNUC__

#define fopen_unsafe(file, mode) fopen(file, mode)

#endif // __GNUC__

#ifdef CONFIG_CHECK_ALLOC

#include <stddef.h>

#define ccalloc(nmemb, size) check_calloc(nmemb, size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_calloc(size_t nmemb, size_t size, const char *file,
 int line);

#define cmalloc(size) check_malloc(size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_malloc(size_t size, const char *file,
 int line);

#define crealloc(ptr, size) check_realloc(ptr, size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_realloc(void *ptr, size_t size, const char *file,
 int line);

#else /* CONFIG_CHECK_ALLOC */

#include <stdlib.h>

static inline void *ccalloc(size_t nmemb, size_t size)
{
  return calloc(nmemb, size);
}

static inline void *cmalloc(size_t size)
{
  return malloc(size);
}

static inline void *crealloc(void *ptr, size_t size)
{
  return realloc(ptr, size);
}

#endif /* CONFIG_CHECK_ALLOC */

__M_END_DECLS

#endif // __COMPAT_H
