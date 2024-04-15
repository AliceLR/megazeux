/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __SDLMZX_H
#define __SDLMZX_H

#include "compat.h"

__M_BEGIN_DECLS

#if defined(CONFIG_SDL)

/* TODO: SDL3 hack so the dso code doesn't need to be overhauled (again). */
#define SDL_FUNCTION_POINTER_IS_VOID_POINTER

#if CONFIG_SDL == 3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#if defined(_WIN32) || defined(CONFIG_X11)
#include <SDL_syswm.h>
#endif
#endif

#include <limits.h>

/* SDL1 backwards compatibility for SDL2 ************************************/

#if !SDL_VERSION_ATLEAST(2,0,0)

// This block remaps anything that is EXACTLY equivalent to its new SDL 2.0
// counterpart. More complex changes are handled with #ifdefs "in situ".

// Data types
typedef SDLKey SDL_Keycode;
typedef int (*SDL_ThreadFunction)(void *);
typedef Uint32 SDL_threadID;
// Use a macro because sdl1.2-compat typedefs SDL_Window...
#define SDL_Window void

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
#define SDLK_PAUSE        SDLK_BREAK

#define SDL_WINDOW_FULLSCREEN  SDL_FULLSCREEN
#define SDL_WINDOW_RESIZABLE   SDL_RESIZABLE
#define SDL_WINDOW_OPENGL      SDL_OPENGL

// API functions

#define SDL_SetEventFilter(filter, userdata) SDL_SetEventFilter(filter)

#ifdef CONFIG_X11
static inline SDL_bool SDL_GetWindowWMInfo(SDL_Window *window,
                                           SDL_SysWMinfo *info)
{
  return SDL_GetWMInfo(info) == 1 ? SDL_TRUE : SDL_FALSE;
}
#endif

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

static inline void SDL_SetWindowGrab(SDL_Window *window, SDL_bool grabbed)
{
  // Not a perfect equivalent; the SDL 1.2 version will grab even if unfocused
  SDL_GrabMode mode = (grabbed == SDL_TRUE) ? SDL_GRAB_ON : SDL_GRAB_OFF;
  SDL_WM_GrabInput(mode);
}

static inline char *SDL_GetCurrentVideoDriver(void)
{
  static char namebuf[16];
  return SDL_VideoDriverName(namebuf, 16);
}

static inline int SDL_JoystickInstanceID(SDL_Joystick *joystick)
{
  return SDL_JoystickIndex(joystick);
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)
static inline int SDL_GL_SetSwapInterval(int interval)
{
#if SDL_VERSION_ATLEAST(1,2,10)
  return SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, interval);
#endif
  return 0;
}
#endif

#ifdef _WIN32
static inline HWND SDL_GetWindowProperty_HWND(SDL_Window *window)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  SDL_GetWindowWMInfo(window, &info);
  return info.window;
}
#endif /* _WIN32 */

#elif !SDL_VERSION_ATLEAST(3,0,0)

#ifdef _WIN32
static inline HWND SDL_GetWindowProperty_HWND(SDL_Window *window)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  SDL_GetWindowWMInfo(window, &info);
  return info.info.win.window;
}
#endif /* _WIN32 */

#else /* SDL_VERSION_ATLEAST(3,0,0) */

#ifdef _WIN32
static inline void *SDL_GetWindowProperty_HWND(SDL_Window *window)
{
  return SDL_GetProperty(SDL_GetWindowProperties(window),
   SDL_PROPERTY_WINDOW_WIN32_HWND_POINTER, NULL);
}
#endif /* _WIN32 */

#endif /* SDL_VERSION_ATLEAST(3,0,0) */


/* SDL 1/2 backwards compatibility for SDL3 *********************************/

/**
 * SDL_audio.h
 */
#if !SDL_VERSION_ATLEAST(3,0,0)
#define SDL_AUDIO_U8    AUDIO_U8
#define SDL_AUDIO_S8    AUDIO_S8
#define SDL_AUDIO_S16   AUDIO_S16SYS
#define SDL_AUDIO_S16LE AUDIO_S16LSB
#define SDL_AUDIO_S16BE AUDIO_S16MSB
#else
/* SDL3 defines SDL_FreeWAV to an error type, but MZX needs to keep it to
 * transparently support older versions. */
#undef SDL_FreeWAV
#define SDL_FreeWAV(w)  SDL_free(w)
#endif

/**
 * SDL_event.h
 */
#if !SDL_VERSION_ATLEAST(3,0,0)
#define SDL_EVENT_GAMEPAD_AXIS_MOTION   SDL_CONTROLLERAXISMOTION
#define SDL_EVENT_JOYSTICK_ADDED        SDL_JOYDEVICEADDED
#define SDL_EVENT_JOYSTICK_REMOVED      SDL_JOYDEVICEREMOVED
#define SDL_EVENT_JOYSTICK_AXIS_MOTION  SDL_JOYAXISMOTION
#define SDL_EVENT_JOYSTICK_BUTTON_DOWN  SDL_JOYBUTTONDOWN
#define SDL_EVENT_JOYSTICK_BUTTON_UP    SDL_JOYBUTTONUP
#define SDL_EVENT_JOYSTICK_HAT_MOTION   SDL_JOYHATMOTION
#define SDL_EVENT_KEY_DOWN              SDL_KEYDOWN
#define SDL_EVENT_KEY_UP                SDL_KEYUP
#define SDL_EVENT_MOUSE_BUTTON_DOWN     SDL_MOUSEBUTTONDOWN
#define SDL_EVENT_MOUSE_BUTTON_UP       SDL_MOUSEBUTTONUP
#define SDL_EVENT_MOUSE_MOTION          SDL_MOUSEMOTION
#define SDL_EVENT_MOUSE_WHEEL           SDL_MOUSEWHEEL
#define SDL_EVENT_TEXT_INPUT            SDL_TEXTINPUT
#define SDL_EVENT_QUIT                  SDL_QUIT
#define SDL_EVENT_FIRST                 SDL_FIRSTEVENT
#define SDL_EVENT_LAST                  SDL_LASTEVENT
#endif

/**
 * SDL_gamepad.h (formerly SDL_gamecontroller.h)
 */
#if !SDL_VERSION_ATLEAST(3,0,0) && SDL_VERSION_ATLEAST(2,0,0)
typedef SDL_GameController        SDL_Gamepad;
typedef SDL_GameControllerAxis    SDL_GamepadAxis;
typedef SDL_GameControllerButton  SDL_GamepadButton;
#define SDL_INIT_GAMEPAD                  SDL_INIT_GAMECONTROLLER
#define SDL_GAMEPAD_AXIS_INVALID          SDL_CONTROLLER_AXIS_INVALID
#define SDL_GAMEPAD_AXIS_LEFTX            SDL_CONTROLLER_AXIS_LEFTX
#define SDL_GAMEPAD_AXIS_LEFTY            SDL_CONTROLLER_AXIS_LEFTY
#define SDL_GAMEPAD_AXIS_RIGHTX           SDL_CONTROLLER_AXIS_RIGHTX
#define SDL_GAMEPAD_AXIS_RIGHTY           SDL_CONTROLLER_AXIS_RIGHTY
#define SDL_GAMEPAD_AXIS_LEFT_TRIGGER     SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define SDL_GAMEPAD_AXIS_RIGHT_TRIGGER    SDL_CONTROLLER_AXIS_TRIGGERRIGHT
#define SDL_GAMEPAD_AXIS_MAX              SDL_CONTROLLER_AXIS_MAX
#define SDL_GAMEPAD_BUTTON_INVALID        SDL_CONTROLLER_BUTTON_INVALID
#define SDL_GAMEPAD_BUTTON_SOUTH          SDL_CONTROLLER_BUTTON_A
#define SDL_GAMEPAD_BUTTON_EAST           SDL_CONTROLLER_BUTTON_B
#define SDL_GAMEPAD_BUTTON_WEST           SDL_CONTROLLER_BUTTON_X
#define SDL_GAMEPAD_BUTTON_NORTH          SDL_CONTROLLER_BUTTON_Y
#define SDL_GAMEPAD_BUTTON_BACK           SDL_CONTROLLER_BUTTON_BACK
#define SDL_GAMEPAD_BUTTON_GUIDE          SDL_CONTROLLER_BUTTON_GUIDE
#define SDL_GAMEPAD_BUTTON_START          SDL_CONTROLLER_BUTTON_START
#define SDL_GAMEPAD_BUTTON_LEFT_STICK     SDL_CONTROLLER_BUTTON_LEFTSTICK
#define SDL_GAMEPAD_BUTTON_RIGHT_STICK    SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define SDL_GAMEPAD_BUTTON_LEFT_SHOULDER  SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define SDL_GAMEPAD_BUTTON_DPAD_UP        SDL_CONTROLLER_BUTTON_DPAD_UP
#define SDL_GAMEPAD_BUTTON_DPAD_DOWN      SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define SDL_GAMEPAD_BUTTON_DPAD_LEFT      SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define SDL_GAMEPAD_BUTTON_DPAD_RIGHT     SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define SDL_GAMEPAD_BUTTON_MISC1          SDL_CONTROLLER_BUTTON_MISC1
#define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1  SDL_CONTROLLER_BUTTON_PADDLE1
#define SDL_GAMEPAD_BUTTON_LEFT_PADDLE1   SDL_CONTROLLER_BUTTON_PADDLE2
#define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2  SDL_CONTROLLER_BUTTON_PADDLE3
#define SDL_GAMEPAD_BUTTON_LEFT_PADDLE2   SDL_CONTROLLER_BUTTON_PADDLE4
#define SDL_GAMEPAD_BUTTON_TOUCHPAD       SDL_CONTROLLER_BUTTON_TOUCHPAD
#define SDL_GAMEPAD_BUTTON_MAX            SDL_CONTROLLER_BUTTON_MAX
#define SDL_IsGamepad(device_id)          SDL_IsGameController(device_id)
#define SDL_OpenGamepad(device_id)        SDL_GameControllerOpen(device_id)
#define SDL_CloseGamepad(g)               SDL_GameControllerClose(g)
#define SDL_GetGamepadAxisFromString(s)   SDL_GameControllerGetAxisFromString(s)
#define SDL_GetGamepadButtonFromString(s) SDL_GameControllerGetButtonFromString(s)
#define SDL_AddGamepadMapping(m)          SDL_GameControllerAddMapping(m)
#define SDL_AddGamepadMappingsFromFile(f) SDL_GameControllerAddMappingsFromFile(f)

static inline void SDL_SetGamepadEventsEnabled(SDL_bool enabled)
{
  SDL_GameControllerEventState(enabled ? SDL_ENABLE : SDL_DISABLE);
}
#endif

/**
 * SDL_joystick.h
 */
#if !SDL_VERSION_ATLEAST(3,0,0)
#define SDL_OpenJoystick(device_id)       SDL_JoystickOpen(device_id)
#define SDL_CloseJoystick(j)              SDL_JoystickClose(j)
#define SDL_GetJoystickGUID(j)            SDL_JoystickGetGUID(j)
#define SDL_GetJoystickGUIDString(g,b,l)  SDL_JoystickGetGUIDString(g,b,l)
#define SDL_GetJoystickInstanceID(j)      SDL_JoystickInstanceID(j)

static inline void SDL_SetJoystickEventsEnabled(SDL_bool enabled)
{
  SDL_JoystickEventState(enabled ? SDL_ENABLE : SDL_DISABLE);
}
#endif

/**
 * SDL_keyboard.h and SDL_keycode.h
 */
#if !SDL_VERSION_ATLEAST(3,0,0)
#define SDL_KMOD_CTRL         KMOD_CTRL
#define SDL_KMOD_ALT          KMOD_ALT
#define SDL_KMOD_NUM          KMOD_NUM
#define SDL_KMOD_CAPS         KMOD_CAPS
#define SDL_TextInputActive() SDL_IsTextInputActive()
#endif

/**
 *  SDL_pixels.h
 */
#if !SDL_VERSION_ATLEAST(3,0,0)
#define SDL_PIXELFORMAT_XRGB8888                    SDL_PIXELFORMAT_RGB888
#define SDL_PIXELFORMAT_XBGR8888                    SDL_PIXELFORMAT_BGR888
#define SDL_CreatePalette(s)                        SDL_AllocPalette(s)
#define SDL_DestroyPalette(p)                       SDL_FreePalette(p)
#define SDL_CreatePixelFormat(f)                    SDL_AllocFormat(f)
#define SDL_DestroyPixelFormat(pf)                  SDL_FreeFormat(pf)
#define SDL_GetMasksForPixelFormatEnum(f,b,R,G,B,A) SDL_PixelFormatEnumToMasks(f,b,R,G,B,A)
#endif

/**
 * SDL_rect.h
 */
#if !SDL_VERSION_ATLEAST(2,0,10)
typedef struct { float x; float y; float w; float h; } SDL_FRect;
#endif

#if SDL_VERSION_ATLEAST(3,0,0)
typedef SDL_FRect SDL_RenderRect;
static inline SDL_RenderRect sdl_render_rect(int x, int y,
 int w, int h, int full_w, int full_h)
{
  SDL_FRect tmp =
  {
    (float)x / full_w,
    (float)y / full_h,
    (float)w / full_w,
    (float)h / full_h
  };
  return tmp;
}
#elif SDL_VERSION_ATLEAST(2,0,0)
typedef SDL_Rect SDL_RenderRect;
static inline SDL_RenderRect sdl_render_rect(int x, int y,
 int w, int h, int full_w, int full_h)
{
  SDL_Rect tmp = { x, y, w, h };
  return tmp;
}
#endif

/**
 * SDL_render.h
 */
#if !SDL_VERSION_ATLEAST(3,0,0) && SDL_VERSION_ATLEAST(2,0,0)
typedef int SDL_RendererLogicalPresentation;
#define SDL_LOGICAL_PRESENTATION_DISABLED 0
#define SDL_SCALEMODE_BEST                SDL_ScaleModeBest
#define SDL_SCALEMODE_LINEAR              SDL_ScaleModeLinear
#define SDL_SCALEMODE_NEAREST             SDL_ScaleModeNearest
#define SDL_SetRenderClipRect(r, rect)    SDL_RenderSetClipRect(r, rect)
#define SDL_SetRenderLogicalSize(r, w, h) SDL_RenderSetLogicalSize(r, w, h)

static inline int SDL_SetRenderLogicalPresentation(SDL_Renderer *render,
 int w, int h, SDL_RendererLogicalPresentation p, SDL_ScaleMode s)
{
  return SDL_SetRenderLogicalSize(render, w, h);
}
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
static inline int SDL_RenderTexture_mzx(SDL_Renderer *renderer, SDL_Texture *texture,
 const SDL_RenderRect *src_rect, const SDL_RenderRect *dest_rect)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  return SDL_RenderTexture(renderer, texture, src_rect, dest_rect);
#else
  return SDL_RenderCopy(renderer, texture, src_rect, dest_rect);
#endif
}
#endif

/**
 * SDL_surface.h
 */
#if !SDL_VERSION_ATLEAST(3,0,0) && SDL_VERSION_ATLEAST(2,0,0)
#define SDL_DestroySurface(s) SDL_FreeSurface(s)

static inline SDL_Surface *SDL_CreateSurface(int width, int height, Uint32 format)
{
  Uint32 rmask, gmask, bmask, amask;
  int bpp;

  SDL_PixelFormatEnumToMasks(format, &bpp, &rmask, &gmask, &bmask, &amask);
  return SDL_CreateRGBSurface(0, width, height, bpp, rmask, gmask, bmask, amask);
}
#endif

/**
 * SDL_thread.h symbols were mostly the same for SDL 1 and SDL 2.
 */
#if !SDL_VERSION_ATLEAST(3,0,0)
typedef SDL_cond  SDL_Condition;
typedef SDL_mutex SDL_Mutex;
typedef SDL_sem   SDL_Semaphore;
#define SDL_CreateCondition()           SDL_CreateCond()
#define SDL_DestroyCondition(c)         SDL_DestroyCond(c)
#define SDL_WaitCondition(c,m)          SDL_CondWait(c,m)
#define SDL_WaitConditionTimeout(c,m,t) SDL_CondWaitTimeout(c,m,t)
#define SDL_SignalCondition(c)          SDL_CondSignal(c)
#define SDL_BroadcastCondition(c)       SDL_CondBroadcast(c)
#define SDL_WaitSemaphore(s)            SDL_SemWait(s)
#define SDL_PostSemaphore(s)            SDL_SemPost(s)
#define SDL_GetCurrentThreadID()        SDL_ThreadID()
#endif

#endif /* CONFIG_SDL */

__M_END_DECLS

#endif /* __SDLMZX_H */
