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

enum resource_id
{
  CONFIG_TXT = 0,
  MZX_DEFAULT_CHR,
  MZX_EDIT_CHR,
  SMZX_PAL,
#ifdef CONFIG_EDITOR
  MZX_ASCII_CHR,
  MZX_BLANK_CHR,
  MZX_SMZX_CHR,
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
CORE_LIBSPEC char *mzx_res_get_by_id(enum resource_id id);

CORE_LIBSPEC boolean redirect_stdio(const char *base_path, boolean require_conf);

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

CORE_LIBSPEC int create_path_if_not_exists(const char *filename);

typedef void (*fn_ptr)(void);

struct dso_syms_map
{
  const char *name;
  fn_ptr *sym_ptr;
};

#include <sys/types.h>

// Code to load/save multi-byte ints to/from little endian memory
int mem_getc(const unsigned char **ptr);
int mem_getd(const unsigned char **ptr);
int mem_getw(const unsigned char **ptr);
void mem_putc(int src, unsigned char **ptr);
void mem_putd(int src, unsigned char **ptr);
void mem_putw(int src, unsigned char **ptr);

#if defined(__WIN32__) && defined(__STRICT_ANSI__)
CORE_LIBSPEC int strcasecmp(const char *s1, const char *s2);
CORE_LIBSPEC int strncasecmp(const char *s1, const char *s2, size_t n);
#endif // __WIN32__ && __STRICT_ANSI__

#if defined(__WIN32__) || defined(__amigaos__)
CORE_LIBSPEC char *strsep(char **stringp, const char *delim);
#endif // __WIN32__ || __amigaos__

#ifndef __WIN32__
#if defined(CONFIG_PSP) || defined(CONFIG_GP2X) \
 || defined(CONFIG_NDS) || defined(CONFIG_WII) \
 || defined(CONFIG_3DS) || defined(CONFIG_SWITCH)
#include <string.h>
#else
#include <strings.h>
#endif
#endif // !__WIN32__

#if defined(__WIN32__) && !defined(_MSC_VER)
#define mkdir(file,mode) mkdir(file)
#endif

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

#elif defined(CONFIG_NDS) && !defined(CONFIG_STDIO_REDIRECT) /* ANDROID */

// When the graphics have initialized, print to a debug buffer rather than the screen.
void info(const char *format, ...)  __attribute__((format(printf, 1, 2)));
void warn(const char *format, ...)  __attribute__((format(printf, 1, 2)));
#define trace(...) do { } while(0)

#ifdef DEBUG
void debug(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else
#define debug(...) do { } while(0)
#endif

#else /* ANDROID, CONFIG_NDS */

#define info(...) \
 do { \
   fprintf(stdout, "INFO: " __VA_ARGS__); \
   fflush(stdout); \
 } while(0)

#define warn(...) \
 do { \
   fprintf(stderr, "WARNING: " __VA_ARGS__); \
   fflush(stderr); \
 } while(0)

#ifdef DEBUG
#define debug(...) \
 do { \
   fprintf(stderr, "DEBUG: " __VA_ARGS__); \
   fflush(stderr); \
 } while(0)
#else
#define debug(...) do { } while(0)
#endif
#if defined(DEBUG) && defined(DEBUG_TRACE)
#define trace(...) \
 do { \
    fprintf(stderr, "TRACE: " __VA_ARGS__); \
    fflush(stderr); \
 } while(0)
#else
#define trace(...) do { } while(0)
#endif

#endif /* ANDROID, CONFIG_NDS */

__M_END_DECLS

#endif // __UTIL_H
