/* MegaZeux
 *
 * Copyright (C) 2006-2007 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007,2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "platform.h"
#include "render.h"
#include "render_layer.h"
#include "renderers.h"
#include "util.h"

#ifdef CONFIG_SDL
#include "render_sdl.h"
#endif

#ifdef CONFIG_EGL
#include <GLES/gl.h>
#include "render_egl.h"
#endif

#include "render_gl.h"

// NOTE: This renderer should work with almost any GL capable video card.
//       However, if you plan to change it, please bear in mind that this
//       has been carefully written to work with both OpenGL 1.x and
//       OpenGL ES 1.x. The latter API lacks many functions present in
//       desktop OpenGL. GL ES is typically used on cellphones.

static const struct gl_version gl_required_version = { 1, 1 };

static struct
{
  void (GL_APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (GL_APIENTRY *glClear)(GLbitfield mask);
  void (GL_APIENTRY *glDeleteTextures)(GLsizei n, GLuint *textures);
  void (GL_APIENTRY *glDisableClientState)(GLenum cap);
  void (GL_APIENTRY *glDrawArrays)(GLenum mode, GLint first, GLsizei count);
  void (GL_APIENTRY *glEnable)(GLenum cap);
  void (GL_APIENTRY *glEnableClientState)(GLenum cap);
  void (GL_APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
  GLenum (GL_APIENTRY *glGetError)(void);
  const GLubyte* (GL_APIENTRY *glGetString)(GLenum name);
  void (GL_APIENTRY *glTexCoordPointer)(GLint size, GLenum type,
   GLsizei stride, const GLvoid *ptr);
  void (GL_APIENTRY *glTexImage2D)(GLenum target, GLint level,
   GLint internalformat,GLsizei width, GLsizei height, GLint border,
   GLenum format, GLenum type, const GLvoid *pixels);
  void (GL_APIENTRY *glTexParameterf)(GLenum target, GLenum pname,
   GLfloat param);
  void (GL_APIENTRY *glTexSubImage2D)(GLenum target, GLint level,
   GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
   GLenum format, GLenum type, const GLvoid *pixels);
  void (GL_APIENTRY *glVertexPointer)(GLint size, GLenum type,
   GLsizei stride, const GLvoid *ptr);
  void (GL_APIENTRY *glViewport)(GLint x, GLint y, GLsizei width,
   GLsizei height);
}
gl1;

static const struct dso_syms_map gl1_syms_map[] =
{
  { "glBindTexture",        (fn_ptr *)&gl1.glBindTexture },
  { "glClear",              (fn_ptr *)&gl1.glClear },
  { "glDeleteTextures",     (fn_ptr *)&gl1.glDeleteTextures },
  { "glDisableClientState", (fn_ptr *)&gl1.glDisableClientState },
  { "glDrawArrays",         (fn_ptr *)&gl1.glDrawArrays },
  { "glEnable",             (fn_ptr *)&gl1.glEnable },
  { "glEnableClientState",  (fn_ptr *)&gl1.glEnableClientState },
  { "glGenTextures",        (fn_ptr *)&gl1.glGenTextures },
  { "glGetError",           (fn_ptr *)&gl1.glGetError },
  { "glGetString",          (fn_ptr *)&gl1.glGetString },
  { "glTexCoordPointer",    (fn_ptr *)&gl1.glTexCoordPointer },
  { "glTexImage2D",         (fn_ptr *)&gl1.glTexImage2D },
  { "glTexParameterf",      (fn_ptr *)&gl1.glTexParameterf },
  { "glTexSubImage2D",      (fn_ptr *)&gl1.glTexSubImage2D },
  { "glVertexPointer",      (fn_ptr *)&gl1.glVertexPointer },
  { "glViewport",           (fn_ptr *)&gl1.glViewport },
  { NULL, NULL }
};

#define gl_check_error() gl_error(__FILE__, __LINE__, gl1.glGetError)

struct gl1_render_data
{
#ifdef CONFIG_EGL
  struct egl_render_data egl;
#endif
#ifdef CONFIG_SDL
  struct sdl_render_data sdl;
#endif
  uint32_t *pixels;
  unsigned int w;
  unsigned int h;
  GLuint texture_number;
};

static boolean gl1_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct gl1_render_data *render_data = cmalloc(sizeof(struct gl1_render_data));

  if(!render_data)
    goto err_out;

  if(!GL_LoadLibrary(GL_LIB_FIXED))
    goto err_free_render_data;

  memset(render_data, 0, sizeof(struct gl1_render_data));
  graphics->render_data = render_data;

  graphics->ratio = conf->video_ratio;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  if(!set_video_mode())
    goto err_free_render_data;

  return true;

err_free_render_data:
  free(render_data);
  graphics->render_data = NULL;
err_out:
  return false;
}

static void gl1_resize_screen(struct graphics_data *graphics,
 int width, int height)
{
  struct gl1_render_data *render_data = graphics->render_data;
  int v_width, v_height;

  get_context_width_height(graphics, &width, &height);
  fix_viewport_ratio(width, height, &v_width, &v_height, graphics->ratio);

  gl1.glViewport((width - v_width) >> 1, (height - v_height) >> 1,
   v_width, v_height);
  gl_check_error();

  gl1.glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl1.glEnableClientState(GL_VERTEX_ARRAY);

  gl1.glEnable(GL_TEXTURE_2D);

  // Free any preexisting textures if they exist
  gl1.glDeleteTextures(1, &render_data->texture_number);
  gl_check_error();

  gl1.glGenTextures(1, &render_data->texture_number);
  gl_check_error();

  gl1.glBindTexture(GL_TEXTURE_2D, render_data->texture_number);
  gl_check_error();

  gl1.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  gl_set_filter_method(graphics->gl_filter_method, gl1.glTexParameterf);
}

static boolean gl1_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  struct gl1_render_data *render_data = graphics->render_data;

  gl_set_attributes(graphics);

  if(!gl_set_video_mode(graphics, width, height, depth, fullscreen, resize,
   gl_required_version))
    return false;

  gl_set_attributes(graphics);

  if(!gl_load_syms(gl1_syms_map))
  {
    gl_cleanup(graphics);
    return false;
  }

  // We need a specific version of OpenGL; desktop GL must be 1.1.
  // All OpenGL ES 1.x implementations are supported, so don't do
  // the check with these configurations.
#ifndef CONFIG_GLES
  {
    static boolean initialized = false;

    if(!initialized)
    {
      const char *version;
      double version_float;

      version = (const char *)gl1.glGetString(GL_VERSION);
      if(!version)
      {
        warn("Could not load GL version string.\n");
        gl_cleanup(graphics);
        return false;
      }

      version_float = atof(version);
      if(version_float < 1.1)
      {
        warn("Need >= OpenGL 1.1, got OpenGL %.1f.\n", version_float);
        gl_cleanup(graphics);
        return false;
      }

      initialized = true;
    }
  }
#endif

  render_data->pixels = cmalloc(sizeof(uint32_t) * SCREEN_PIX_W * SCREEN_PIX_H);
  if(!render_data->pixels)
  {
    gl_cleanup(graphics);
    return false;
  }

  render_data->w = SCREEN_PIX_W;
  render_data->h = SCREEN_PIX_H;

  gl1_resize_screen(graphics, width, height);
  return true;
}

static void gl1_free_video(struct graphics_data *graphics)
{
  struct gl1_render_data *render_data = graphics->render_data;

  if(render_data)
  {
    gl1.glDeleteTextures(1, &render_data->texture_number);
    gl_check_error();

    gl_cleanup(graphics);
    free(render_data);
    graphics->render_data = NULL;
  }
}

static void gl1_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  unsigned int i;
  for(i = 0; i < count; i++)
  {
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
    graphics->flat_intensity_palette[i] = (palette[i].r << 24) |
     (palette[i].g << 16) | (palette[i].b << 8);
#else
    graphics->flat_intensity_palette[i] = (palette[i].b << 16) |
     (palette[i].g << 8) | palette[i].r;
#endif
  }
}

static void gl1_render_graph(struct graphics_data *graphics)
{
  struct gl1_render_data *render_data = graphics->render_data;
  unsigned int mode = graphics->screen_mode;

  if(!mode)
    render_graph32(render_data->pixels, render_data->w * 4, graphics);
  else
    render_graph32s(render_data->pixels, render_data->w * 4, graphics);
}

static void gl1_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct gl1_render_data *render_data = graphics->render_data;

  render_layer(render_data->pixels, 32, render_data->w * 4, graphics, layer);
}

static void gl1_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct gl1_render_data *render_data = graphics->render_data;

  render_cursor(render_data->pixels, render_data->w * 4, 32, x, y,
   graphics->flat_intensity_palette[color], lines, offset);
}

static void gl1_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct gl1_render_data *render_data = graphics->render_data;

  render_mouse(render_data->pixels, render_data->w * 4, 32, x, y, 0xFFFFFFFF,
   0x0, w, h);
}

static void gl1_sync_screen(struct graphics_data *graphics)
{
  struct gl1_render_data *render_data = graphics->render_data;

  const float texture_width = 1.0f * SCREEN_PIX_W / GL_POWER_2_WIDTH;
  const float texture_height = 1.0f * SCREEN_PIX_H / GL_POWER_2_HEIGHT;

  const float tex_coord_array[2 * 4] = {
    0.0f,          0.0f,
    0.0f,          texture_height,
    texture_width, 0.0f,
    texture_width, texture_height
  };

  gl1.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_data->w, render_data->h,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->pixels);
  gl_check_error();

  gl1.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();

  gl1.glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_array);
  gl_check_error();

  gl1.glVertexPointer(2, GL_FLOAT, 0, vertex_array_single);
  gl_check_error();

  gl1.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  gl_swap_buffers(graphics);
}

void render_gl1_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = gl1_init_video;
  renderer->free_video = gl1_free_video;
  renderer->set_video_mode = gl1_set_video_mode;
  renderer->update_colors = gl1_update_colors;
  renderer->resize_screen = gl1_resize_screen;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = gl1_render_graph;
  renderer->render_layer = gl1_render_layer;
  renderer->render_cursor = gl1_render_cursor;
  renderer->render_mouse = gl1_render_mouse;
  renderer->sync_screen = gl1_sync_screen;
}
