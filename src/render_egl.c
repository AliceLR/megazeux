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

#include <GLES/gl.h>
#include "render_egl.h"

#include "util.h"

#include <assert.h>
#include <dlfcn.h>

static void *glso;
static enum gl_lib_type gl_type;

#ifdef ANDROID

#include "sfwrapper.h"

fn_ptr GL_GetProcAddress(const char *proc)
{
  return (fn_ptr)dlsym(glso, proc);
}

#endif /* ANDROID */

bool GL_LoadLibrary(enum gl_lib_type type)
{
  const char *filename;

  gl_type = type;

  if(type == GL_LIB_FIXED)
    filename = "libGLESv1_CM.so";
  else
    filename = "libGLESv2.so";

  glso = dlopen(filename, RTLD_NOW);
  return glso != NULL;
}

// EGL is not directly comparable to SDL as it does not handle "windows"
// in the traditional sense. It is expected that the platform provides a
// native "window" type that handles things like resolution and resizing.

static EGLint config16[] =
{
  EGL_RED_SIZE,        5,
  EGL_GREEN_SIZE,      6,
  EGL_BLUE_SIZE,       5,
  EGL_ALPHA_SIZE,      0,
  EGL_DEPTH_SIZE,      0,
  EGL_RENDERABLE_TYPE, 0, /* Placeholder */
  EGL_NONE,
};

static EGLint config32[] =
{
  EGL_RED_SIZE,        8,
  EGL_GREEN_SIZE,      8,
  EGL_BLUE_SIZE,       8,
  EGL_ALPHA_SIZE,      8,
  EGL_DEPTH_SIZE,      0,
  EGL_RENDERABLE_TYPE, 0, /* Placeholder */
  EGL_NONE,
};

static void update_config_attribs(EGLint *config)
{
  int i;

  for(i = 0; config[i] != EGL_NONE; i++)
  {
    if(config[i] != EGL_RENDERABLE_TYPE)
      continue;

    if(gl_type == GL_LIB_FIXED)
      config[i + 1] = EGL_OPENGL_ES_BIT;
    else
      config[i + 1] = EGL_OPENGL_ES2_BIT;

    break;
  }
}

static const EGLint *get_current_config(int depth)
{
  if(depth == 32)
  {
    debug("Selected RGBA 8888 EGLConfig\n");
    update_config_attribs(config32);
    return config32;
  }

  debug("Selected RGB 565 EGLConfig\n");
  update_config_attribs(config16);
  return config16;
}

static const EGLint gles_v1_attribs[] =
{
  EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE
};

static const EGLint gles_v2_attribs[] =
{
  EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
};

bool gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  struct egl_render_data *egl_render_data = graphics->render_data;
  const EGLint *attribs = gles_v1_attribs;
  EGLNativeWindowType window = 0;
  EGLint w, h;

  assert(egl_render_data != NULL);

#ifdef ANDROID
  window = SurfaceFlingerGetNativeWindow();
#endif

#ifdef CONFIG_X11
  window = XCreateSimpleWindow(egl_render_data->native_display,
                               RootWindow(egl_render_data->native_display, 0),
                               0, 0, 640, 350, 0,
                               BlackPixel(egl_render_data->native_display, 0),
                               BlackPixel(egl_render_data->native_display, 0));
  XMapWindow(egl_render_data->native_display, window);
  XFlush(egl_render_data->native_display);
#endif

  egl_render_data->surface = eglCreateWindowSurface(egl_render_data->display,
                                                    egl_render_data->config,
                                                    window, NULL);
  if(egl_render_data->surface == EGL_NO_SURFACE)
  {
    warn("eglCreateWindowSurface failed\n");
    goto err_cleanup;
  }

  if(!eglBindAPI(EGL_OPENGL_ES_API))
  {
    warn("eglBindAPI failed\n");
    goto err_cleanup;
  }

  if(gl_type == GL_LIB_PROGRAMMABLE)
    attribs = gles_v2_attribs;

  egl_render_data->context = eglCreateContext(egl_render_data->display,
                                              egl_render_data->config,
                                              EGL_NO_CONTEXT, attribs);
  if(egl_render_data->context == EGL_NO_CONTEXT)
  {
    warn("eglCreateContext failed\n");
    goto err_cleanup;
  }

  if(!eglQuerySurface(egl_render_data->display, egl_render_data->surface,
   EGL_WIDTH, &w))
  {
    warn("eglQuerySurface (width) failed\n");
    goto err_cleanup;
  }

  if(!eglQuerySurface(egl_render_data->display, egl_render_data->surface,
   EGL_HEIGHT, &h))
  {
    warn("eglQuerySurface (height) failed\n");
    goto err_cleanup;
  }

  if(fullscreen)
  {
    graphics->resolution_width = w;
    graphics->resolution_height = h;
  }
  else
  {
    graphics->window_width = w;
    graphics->window_height = h;
  }

#ifdef ANDROID
  SurfaceFlingerSetSwapRectangle(0, 0, w, h);
#endif

  if(!eglMakeCurrent(egl_render_data->display, egl_render_data->surface,
                     egl_render_data->surface, egl_render_data->context))
  {
    warn("eglMakeCurrent failed\n");
    goto err_cleanup;
  }

  // Can now issue GL ES commands
  return true;

err_cleanup:
  gl_cleanup(graphics);
  return false;
}

bool gl_check_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize)
{
  struct egl_render_data *egl_render_data = graphics->render_data;
  EGLint num_configs;

  assert(egl_render_data != NULL);

#ifdef ANDROID
  if(!SurfaceFlingerInitialize(width, height, depth, fullscreen))
  {
    warn("SurfaceFlingerInitialize failed\n");
    return false;
  }
#endif

#ifdef CONFIG_X11
  egl_render_data->native_display = XOpenDisplay(NULL);
#endif

  egl_render_data->display = eglGetDisplay(egl_render_data->native_display);
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

  // EGL is not concerned with double buffering -- per-platform hacks should
  // go here if double buffering isn't the default or can be toggled.

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

  if(egl_render_data->display != EGL_NO_DISPLAY)
  {
    eglMakeCurrent(egl_render_data->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
     EGL_NO_CONTEXT);

    if(egl_render_data->context != EGL_NO_CONTEXT)
    {
      eglDestroyContext(egl_render_data->display, egl_render_data->context);
      egl_render_data->context = EGL_NO_CONTEXT;
    }

    if(egl_render_data->surface != EGL_NO_SURFACE)
    {
      eglDestroySurface(egl_render_data->display, egl_render_data->surface);
      egl_render_data->surface = EGL_NO_SURFACE;
    }

    eglTerminate(egl_render_data->display);
    egl_render_data->display = EGL_NO_DISPLAY;
  }

#ifdef ANDROID
  SurfaceFlingerDeinitialize();
#endif

#ifdef CONFIG_X11
  XCloseDisplay(egl_render_data->native_display);
  egl_render_data->native_display = 0;
#endif
}
