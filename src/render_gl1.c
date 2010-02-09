/* MegaZeux
 *
 * Copyright (C) 2006-2007 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "platform.h"
#include "graphics.h"
#include "render.h"
#include "render_sdl.h"
#include "render_gl.h"
#include "renderers.h"
#include "util.h"

typedef struct
{
  int syms_loaded;
  void (APIENTRY *glBegin)(GLenum mode);
  void (APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (APIENTRY *glEnable)(GLenum cap);
  void (APIENTRY *glEnd)(void);
  void (APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
  const GLubyte* (APIENTRY *glGetString)(GLenum name);
  void (APIENTRY *glTexCoord2f)(GLfloat s, GLfloat t);
  void (APIENTRY *glTexImage2D)(GLenum target, GLint level,
   GLint internalformat,GLsizei width, GLsizei height, GLint border,
   GLenum format, GLenum type, const GLvoid *pixels);
  void (APIENTRY *glTexParameteri)(GLenum target, GLenum pname, GLint param);
  void (APIENTRY *glVertex3f)(GLfloat x, GLfloat y, GLfloat z);
  void (APIENTRY *glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
} gl1_syms;

typedef struct
{
  Uint32 *pixels;
  Uint32 w;
  Uint32 h;
  gl1_syms gl;
} gl1_render_data;

static int gl1_load_syms (gl1_syms *gl)
{
  if(gl->syms_loaded)
    return true;

  // Since 1.0
  GL_LOAD_SYM(gl, glBegin)
  // Since 1.1
  GL_LOAD_SYM(gl, glBindTexture)
  // Since 1.0 (parameters may require more recent version)
  GL_LOAD_SYM(gl, glEnable)
  // Since 1.0
  GL_LOAD_SYM(gl, glEnd)
  // Since 1.1
  GL_LOAD_SYM(gl, glGenTextures)
  // Since 1.0
  GL_LOAD_SYM(gl, glGetString)
  // Since 1.0
  GL_LOAD_SYM(gl, glTexCoord2f)
  // Since 1.0 (parameters may require more recent version)
  GL_LOAD_SYM(gl, glTexImage2D)
  // Since 1.0
  GL_LOAD_SYM(gl, glTexParameteri)
  // Since 1.0
  GL_LOAD_SYM(gl, glVertex3f)
  // Since 1.0
  GL_LOAD_SYM(gl, glViewport)

  gl->syms_loaded = true;
  return true;
}

static bool gl1_init_video(graphics_data *graphics, config_info *conf)
{
  gl1_render_data *render_data = malloc(sizeof(gl1_render_data));
  gl1_syms *gl = &render_data->gl;
  int internal_width, internal_height;
  const char *version, *extensions;

  if(!render_data)
    return false;

  if(!GL_CAN_USE)
  {
    free(render_data);
    return false;
  }

  graphics->render_data = render_data;
  gl->syms_loaded = false;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  if(!set_video_mode())
  {
    free(render_data);
    return false;
  }

  // NOTE: These must come AFTER set_video_mode()!
  version = (const char *)gl->glGetString(GL_VERSION);
  extensions = (const char *)gl->glGetString(GL_EXTENSIONS);

  // we need a specific "version" of OpenGL compatibility
  if(version && atof(version) < 1.1)
  {
    warn("Your OpenGL implementation is too old (need v1.1).\n");
    free(render_data);
    return false;
  }

  // we also might be able to utilise an extension
  if(extensions && strstr(extensions, "GL_ARB_texture_non_power_of_two"))
  {
    internal_width = GL_NON_POWER_2_WIDTH;
    internal_height = GL_NON_POWER_2_HEIGHT;
  }
  else
  {
    internal_width = GL_POWER_2_WIDTH;
    internal_height = GL_POWER_2_HEIGHT;
  }

  render_data->pixels = malloc(sizeof(Uint32) * internal_width *
   internal_height);
  render_data->w = internal_width;
  render_data->h = internal_height;

  return render_data->pixels != NULL;
}

static bool gl1_set_video_mode(graphics_data *graphics, int width, int height,
 int depth, int fullscreen, int resize)
{
  gl1_render_data *render_data = graphics->render_data;
  gl1_syms *gl = &render_data->gl;

  GLuint texture_number;

  gl_set_attributes(graphics);

  if(!SDL_SetVideoMode(width, height, depth,
   GL_STRIP_FLAGS(sdl_flags(depth, fullscreen, resize))))
    return false;

  if(!gl1_load_syms(gl))
    return false;

  gl->glViewport(0, 0, width, height);

  gl->glEnable(GL_TEXTURE_2D);

  gl->glGenTextures(1, &texture_number);
  gl->glBindTexture(GL_TEXTURE_2D, texture_number);

  gl_set_filter_method(graphics->gl_filter_method, gl->glTexParameteri);

  return true;
}

static void gl1_update_colors(graphics_data *graphics, rgb_color *palette,
 Uint32 count)
{
  Uint32 i;
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

static void gl1_render_graph(graphics_data *graphics)
{
  gl1_render_data *render_data = graphics->render_data;
  Uint32 mode = graphics->screen_mode;

  if(!mode)
  {
    render_graph32(render_data->pixels, render_data->w * 4, graphics,
     set_colors32[mode]);
  }
  else
  {
    render_graph32s(render_data->pixels, render_data->w * 4, graphics,
     set_colors32[mode]);
  }
}

static void gl1_render_cursor(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 color, Uint8 lines, Uint8 offset)
{
  gl1_render_data *render_data = graphics->render_data;

  render_cursor(render_data->pixels, render_data->w * 4, 32, x, y,
   graphics->flat_intensity_palette[color], lines, offset);
}

static void gl1_render_mouse(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 w, Uint8 h)
{
  gl1_render_data *render_data = graphics->render_data;

  render_mouse(render_data->pixels, render_data->w * 4, 32, x, y, 0xFFFFFFFF,
   w, h);
}

static void gl1_sync_screen(graphics_data *graphics)
{
  gl1_render_data *render_data = graphics->render_data;
  gl1_syms *gl = &render_data->gl;
  float texture_width, texture_height;

  texture_width = (float)640 / render_data->w;
  texture_height = (float)350 / render_data->h;

  gl->glTexImage2D(GL_TEXTURE_2D, 0, 3, render_data->w, render_data->h, 0,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->pixels);

  gl->glBegin(GL_QUADS);
    gl->glTexCoord2f(0.0, texture_height);
    gl->glVertex3f(-1.0, -1.0, 0.0);

    gl->glTexCoord2f(texture_width, texture_height);
    gl->glVertex3f(1.0, -1.0, 0.0);

    gl->glTexCoord2f(texture_width, 0.0);
    gl->glVertex3f(1.0, 1.0, 0.0);

    gl->glTexCoord2f(0.0, 0.0);
    gl->glVertex3f(-1.0, 1.0, 0.0);
  gl->glEnd();

  SDL_GL_SwapBuffers();
}

void render_gl1_register(graphics_data *graphics)
{
  memset(graphics, 0, sizeof(graphics_data));
  graphics->init_video = gl1_init_video;
  graphics->check_video_mode = gl_check_video_mode;
  graphics->set_video_mode = gl1_set_video_mode;
  graphics->update_colors = gl1_update_colors;
  graphics->resize_screen = resize_screen_standard;
  graphics->get_screen_coords = get_screen_coords_scaled;
  graphics->set_screen_coords = set_screen_coords_scaled;
  graphics->render_graph = gl1_render_graph;
  graphics->render_cursor = gl1_render_cursor;
  graphics->render_mouse = gl1_render_mouse;
  graphics->sync_screen = gl1_sync_screen;
}
