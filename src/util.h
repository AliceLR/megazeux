/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

// Useful, generic utility functions

#ifndef __UTIL_H
#define __UTIL_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdint.h>
#include <stdio.h>
#if !defined(_MSC_VER) && !defined(__amigaos__)
#include <unistd.h>
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define CLAMP(x, low, high) \
  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define SGN(x) ((x > 0) - (x < 0))

#ifdef CONFIG_STDIO_REDIRECT
CORE_LIBSPEC extern FILE *mzxout_h;
CORE_LIBSPEC extern FILE *mzxerr_h;
#define mzxout (mzxout_h ? mzxout_h : stdout)
#define mzxerr (mzxerr_h ? mzxerr_h : stderr)
#else
#define mzxout stdout
#define mzxerr stderr
#endif

enum resource_id
{
  MZX_EXECUTABLE_DIR = 0,
  CONFIG_TXT,
  MZX_DEFAULT_CHR,
  MZX_EDIT_CHR,
  SMZX_PAL,
#ifdef CONFIG_EDITOR
  MZX_ASCII_CHR,
  MZX_BLANK_CHR,
  MZX_SMZX_CHR,
  MZX_SMZX2_CHR,
#endif
#ifdef CONFIG_HELPSYS
  MZX_HELP_FIL,
#endif
#ifdef CONFIG_RENDER_GL_PROGRAM
  GLSL_SHADER_SCALER_DIRECTORY,
  GLSL_SHADER_SCALER_VERT,
  GLSL_SHADER_SCALER_FRAG,
  GLSL_SHADER_TILEMAP_VERT,
  GLSL_SHADER_TILEMAP_FRAG,
  GLSL_SHADER_TILEMAP_SMZX_FRAG,
  GLSL_SHADER_MOUSE_VERT,
  GLSL_SHADER_MOUSE_FRAG,
  GLSL_SHADER_CURSOR_VERT,
  GLSL_SHADER_CURSOR_FRAG,
#endif
#ifdef CONFIG_GAMECONTROLLERDB
  GAMECONTROLLERDB_TXT,
#endif
  END_RESOURCE_ID_T // must be last
};

#ifdef CONFIG_RENDER_GL_PROGRAM
#define GLSL_SHADER_RES_FIRST GLSL_SHADER_SCALER_VERT
#define GLSL_SHADER_RES_LAST  GLSL_SHADER_CURSOR_FRAG
#define GLSL_SHADER_RES_COUNT (GLSL_SHADER_RES_LAST - GLSL_SHADER_RES_FIRST + 1)
#endif

CORE_LIBSPEC int mzx_res_init(const char *argv0, boolean editor);
CORE_LIBSPEC void mzx_res_free(void);
CORE_LIBSPEC const char *mzx_res_get_by_id(enum resource_id id);

#ifdef CONFIG_STDIO_REDIRECT
CORE_LIBSPEC boolean redirect_stdio_init(const char *base_path, boolean require_conf);
CORE_LIBSPEC void redirect_stdio_exit(void);
#endif

// Code to load multi-byte ints from little endian file
int fgetw(FILE *fp);
CORE_LIBSPEC int fgetd(FILE *fp);
void fputw(int src, FILE *fp);
void fputd(int src, FILE *fp);

CORE_LIBSPEC long ftell_and_rewind(FILE *f);

CORE_LIBSPEC void rng_seed_init(void);
uint64_t rng_get_seed(void);
void rng_set_seed(uint64_t seed);
unsigned int Random(uint64_t range);

typedef void (*fn_ptr)(void);

struct dso_syms_map
{
  const char *name;
  fn_ptr *sym_ptr;
};

#include <sys/types.h>

#if defined(__WIN32__) && defined(__STRICT_ANSI__)
CORE_LIBSPEC int strcasecmp(const char *s1, const char *s2);
CORE_LIBSPEC int strncasecmp(const char *s1, const char *s2, size_t n);
#endif // __WIN32__ && __STRICT_ANSI__

#if defined(__WIN32__) || defined(__amigaos__)
CORE_LIBSPEC char *strsep(char **stringp, const char *delim);
#endif // __WIN32__ || __amigaos__

#ifndef __WIN32__
// POSIX strcasecmp and strncasecmp are in strings.h,
// which may or may not be included by string.h
#include <string.h>
#include <strings.h>
#endif // !__WIN32__

#if defined(__amigaos__)
CORE_LIBSPEC extern long __stack_chk_guard[8];
CORE_LIBSPEC void __stack_chk_fail(void);
#endif

#if defined(ANDROID)

#include <android/log.h>

#define info(...)  __android_log_print(ANDROID_LOG_INFO, "MegaZeux", __VA_ARGS__)
#define warn(...)  __android_log_print(ANDROID_LOG_WARN, "MegaZeux", __VA_ARGS__)

#ifdef DEBUG
#define debug(...)  __android_log_print(ANDROID_LOG_DEBUG, "MegaZeux", __VA_ARGS__)
#else
#define debug(...) do { } while(0)
#endif

#if defined(DEBUG) && defined(DEBUG_TRACE)
#define trace(...)  __android_log_print(ANDROID_LOG_VERBOSE, "MegaZeux", __VA_ARGS__)
#else
#define trace(...) do { } while(0)
#endif

#else /* ANDROID */

#define info(...) \
 do { \
   fprintf(mzxout, "INFO: " __VA_ARGS__); \
   fflush(mzxout); \
 } while(0)

#define warn(...) \
 do { \
   fprintf(mzxerr, "WARNING: " __VA_ARGS__); \
   fflush(mzxerr); \
 } while(0)

#ifdef DEBUG
#define debug(...) \
 do { \
   fprintf(mzxerr, "DEBUG: " __VA_ARGS__); \
   fflush(mzxerr); \
 } while(0)
#else
#define debug(...) do { } while(0)
#endif
#if defined(DEBUG) && defined(DEBUG_TRACE)
#define trace(...) \
 do { \
    fprintf(mzxerr, "TRACE: " __VA_ARGS__); \
    fflush(mzxerr); \
 } while(0)
#else
#define trace(...) do { } while(0)
#endif

#endif /* ANDROID */

__M_END_DECLS

#endif // __UTIL_H
