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

#if !defined(CONFIG_WII) && !defined(CONFIG_NDS)

#undef false
#undef true
#undef bool

typedef enum
{
  false = 0,
  true  = 1,
} bool;

#endif // !CONFIG_WII && !CONFIG_NDS

#endif /* __cplusplus */

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

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef ANDROID
#define HAVE_SYS_UIO_H
#define LOG_TAG "MegaZeux"
#include <cutils/log.h>
#undef CONDITION
#endif

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

#if defined(CONFIG_SDL) && !defined(SKIP_SDL)

#include <SDL.h>
#include <SDL_syswm.h>

#if !SDL_VERSION_ATLEAST(2,0,0)

// This block remaps anything that is EXACTLY equivalent to its new SDL 2.0
// counterpart. More complex changes are handled with #ifdefs "in situ".

// Data types

typedef SDLKey SDL_Keycode;
typedef void   SDL_Window;

// Macros / enumerants

#define SDLK_KP_0         SDLK_KP0
#define SDLK_KP_1         SDLK_KP1
#define SDLK_KP_2         SDLK_KP2
#define SDLK_KP_3         SDLK_KP3
#define SDLK_KP_4         SDLK_KP4
#define SDLK_KP_5         SDLK_KP5
#define SDLK_KP_6         SDLK_KP6
#define SDLK_KP_7         SDLK_KP7
#define SDLK_KP_8         SDLK_KP8
#define SDLK_KP_9         SDLK_KP9
#define SDLK_NUMLOCKCLEAR SDLK_NUMLOCK
#define SDLK_SCROLLLOCK   SDLK_SCROLLOCK
#define SDLK_LGUI         SDLK_LSUPER
#define SDLK_RGUI         SDLK_RSUPER
#define SDLK_PAUSE        SDLK_BREAK

#define SDL_WINDOW_FULLSCREEN  SDL_FULLSCREEN
#define SDL_WINDOW_RESIZABLE   SDL_RESIZABLE
#define SDL_WINDOW_OPENGL      SDL_OPENGL

// API functions

#define SDL_SetEventFilter(filter, userdata) SDL_SetEventFilter(filter)

static inline SDL_bool SDL_GetWindowWMInfo(SDL_Window *window,
                                           SDL_SysWMinfo *info)
{
  return SDL_GetWMInfo(info) == 1 ? SDL_TRUE : SDL_FALSE;
}

static inline SDL_Window *SDL_GetWindowFromID(Uint32 id)
{
  return NULL;
}

static inline void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y)
{
  SDL_WarpMouse(x, y);
}

static inline void SDL_SetWindowTitle(SDL_Window *window, const char *title)
{
  SDL_WM_SetCaption(title, "");
}

static inline void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon)
{
   SDL_WM_SetIcon(icon, NULL);
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)
static inline int SDL_GL_SetSwapInterval(int interval)
{
  return SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, interval);
}
#endif

#ifdef CONFIG_X11
static inline XEvent *SDL_SysWMmsg_GetXEvent(SDL_SysWMmsg *msg)
{
  return &msg->event.xevent;
}
#endif /* CONFIG_X11 */

#if defined(CONFIG_ICON) && defined(__WIN32__)
static inline HWND SDL_SysWMinfo_GetWND(SDL_SysWMinfo *info)
{
  return info->window;
}
#endif /* CONFIG_ICON && __WIN32__ */

#else /* SDL_VERSION_ATLEAST(2,0,0) */

#ifdef CONFIG_X11
static inline XEvent *SDL_SysWMmsg_GetXEvent(SDL_SysWMmsg *msg)
{
  return &msg->msg.x11.event;
}
#endif /* CONFIG_X11 */

#if defined(CONFIG_ICON) && defined(__WIN32__)
static inline HWND SDL_SysWMinfo_GetWND(SDL_SysWMinfo *info)
{
  return info->info.win.window;
}
#endif /* CONFIG_ICON && __WIN32__ */

#endif /* SDL_VERSION_ATLEAST(2,0,0) */

#endif /* CONFIG_SDL && !SKIP_SDL */

__M_END_DECLS

#endif // __COMPAT_H
