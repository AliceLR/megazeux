/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "render_egl.h"

#include "util.h"

#include <assert.h>

/* EGL is not directly comparable to SDL as it does not handle "windows"
 * in the traditional sense. It is expected that the platform provides a
 * native "window" type that handles things like resolution and resizing.
 */

static const EGLint config16[] =
{
  EGL_RED_SIZE,   5,
  EGL_GREEN_SIZE, 6,
  EGL_BLUE_SIZE,  5,
  EGL_ALPHA_SIZE, 0,
  EGL_NONE,       EGL_NONE,
};

static const EGLint config32[] =
{
  EGL_RED_SIZE,   8,
  EGL_GREEN_SIZE, 8,
  EGL_BLUE_SIZE,  8,
  EGL_ALPHA_SIZE, 8,
  EGL_NONE,       EGL_NONE,
};

static const EGLint *get_current_config(int depth)
{
  if(depth == 32)
    return config32;
  return config16;
}

bool gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  struct egl_render_data *egl_render_data = graphics->render_data;
  EGLNativeWindowType window;

  assert(egl_render_data != NULL);

#ifdef ANDROID
  /* get window surface */
#endif

  egl_render_data->surface = eglCreateWindowSurface(egl_render_data->display,
                                                    egl_render_data->config,
                                                    window, NULL);
  if(egl_render_data->surface == EGL_NO_SURFACE)
  {
    warn("eglCreateWindowSurface failed\n");
    return false;
  }

  egl_render_data->context = eglCreateContext(egl_render_data->display,
                                              egl_render_data->config,
                                              EGL_NO_CONTEXT, NULL);
  if(egl_render_data->context == EGL_NO_CONTEXT)
  {
    warn("eglCreateContext failed\n");
    return false;
  }

  if(eglMakeCurrent(egl_render_data->display, egl_render_data->surface,
                    egl_render_data->surface, egl_render_data->context))
  {
    warn("eglMakeCurrent failed\n");
    return false;
  }

  /* Can now issue GLES commands */

  return true;
}

bool gl_check_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  struct egl_render_data *egl_render_data = graphics->render_data;
  EGLint num_configs;

  assert(egl_render_data != NULL);

  egl_render_data->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if(egl_render_data->display == EGL_NO_DISPLAY)
  {
    warn("eglGetDisplay failed\n");
    return false;
  }

  if(!eglInitialize(egl_render_data->display, NULL, NULL))
  {
    warn("eglInitialize failed\n");
    return false;
  }

  if(!eglChooseConfig(egl_render_data->display,
                      get_current_config(depth),
                      &egl_render_data->config, 1, &num_configs))
  {
    warn("eglChooseConfig failed\n");
    return false;
  }

  return true;
}

void gl_set_attributes(struct graphics_data *graphics)
{
  struct egl_render_data *egl_render_data = graphics->render_data;

  assert(egl_render_data != NULL);

  /* EGL is not concerned with double buffering -- per-platform hacks should
   * go here if double buffering isn't the default or can be toggled.
   */

  if(graphics->gl_vsync == 0)
    eglSwapInterval(egl_render_data->display, 0);
  else if(graphics->gl_vsync >= 1)
    eglSwapInterval(egl_render_data->display, 1);
}

bool gl_swap_buffers(struct graphics_data *graphics)
{
  struct egl_render_data *egl_render_data = graphics->render_data;

  assert(egl_render_data != NULL);

  return eglSwapBuffers(egl_render_data->display, egl_render_data->surface);
}

void gl_cleanup(struct graphics_data *graphics)
{
  struct egl_render_data *egl_render_data = graphics->render_data;

  assert(egl_render_data != NULL);

  eglMakeCurrent(egl_render_data->display, EGL_NO_SURFACE,
                 EGL_NO_SURFACE, EGL_NO_CONTEXT);

  eglDestroyContext(egl_render_data->display, egl_render_data->context);
  eglDestroySurface(egl_render_data->display, egl_render_data->surface);
  eglTerminate(egl_render_data->display);
}
