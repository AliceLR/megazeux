/* MegaZeux
 *
 * Copyright (C) 2007 Joel Bouchard Lamontagne <logicow@gmail.com>
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

#include "platform.h"
#include "render.h"
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

static struct
{
  void (GL_APIENTRY *glAlphaFunc)(GLenum func, GLclampf ref);
  void (GL_APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (GL_APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
  void (GL_APIENTRY *glClear)(GLbitfield mask);
  void (GL_APIENTRY *glColorPointer)(GLint size, GLenum type, GLsizei stride,
   const GLvoid *pointer);
  void (GL_APIENTRY *glCopyTexImage2D)(GLenum target, GLint level,
   GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height,
   GLint border);
  void (GL_APIENTRY *glDisable)(GLenum cap);
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
   GLint internalformat, GLsizei width, GLsizei height, GLint border,
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
gl2;

static const struct dso_syms_map gl2_syms_map[] =
{
  { "glAlphaFunc",          (fn_ptr *)&gl2.glAlphaFunc },
  { "glBindTexture",        (fn_ptr *)&gl2.glBindTexture },
  { "glBlendFunc",          (fn_ptr *)&gl2.glBlendFunc },
  { "glClear",              (fn_ptr *)&gl2.glClear },
  { "glColorPointer",       (fn_ptr *)&gl2.glColorPointer },
  { "glCopyTexImage2D",     (fn_ptr *)&gl2.glCopyTexImage2D },
  { "glDisable",            (fn_ptr *)&gl2.glDisable },
  { "glDisableClientState", (fn_ptr *)&gl2.glDisableClientState },
  { "glDrawArrays",         (fn_ptr *)&gl2.glDrawArrays },
  { "glEnable",             (fn_ptr *)&gl2.glEnable },
  { "glEnableClientState",  (fn_ptr *)&gl2.glEnableClientState },
  { "glGenTextures",        (fn_ptr *)&gl2.glGenTextures },
  { "glGetError",           (fn_ptr *)&gl2.glGetError },
  { "glGetString",          (fn_ptr *)&gl2.glGetString },
  { "glTexCoordPointer",    (fn_ptr *)&gl2.glTexCoordPointer },
  { "glTexImage2D",         (fn_ptr *)&gl2.glTexImage2D },
  { "glTexParameterf",      (fn_ptr *)&gl2.glTexParameterf },
  { "glTexSubImage2D",      (fn_ptr *)&gl2.glTexSubImage2D },
  { "glVertexPointer",      (fn_ptr *)&gl2.glVertexPointer },
  { "glViewport",           (fn_ptr *)&gl2.glViewport },
  { NULL, NULL }
};

#define gl_check_error() gl_error(__FILE__, __LINE__, gl2.glGetError)

struct gl2_render_data
{
#ifdef CONFIG_EGL
  struct egl_render_data egl;
#endif
  Uint32 *pixels;
  Uint8 charset_texture[CHAR_H * CHARSET_SIZE * CHAR_W * 2];
  Uint32 background_texture[SCREEN_W * SCREEN_H];
  GLuint texture_number[3];
  GLubyte palette[3 * SMZX_PAL_SIZE];
  Uint8 remap_texture;
  Uint8 remap_char[CHARSET_SIZE * 2];
  Uint8 ignore_linear;
  enum ratio_type ratio;
  GLubyte color_array[SCREEN_W * SCREEN_H * 4 * 4];
  float tex_coord_array[SCREEN_W * SCREEN_H * 8];
  float vertex_array[SCREEN_W * SCREEN_H * 8];
};

static const GLubyte color_array_white[4 * 4] = {
  255, 255, 255, 255,
  255, 255, 255, 255,
  255, 255, 255, 255,
  255, 255, 255, 255
};

static bool gl2_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct gl2_render_data *render_data = cmalloc(sizeof(struct gl2_render_data));

  if(!render_data)
    goto err_out;

  if(!GL_LoadLibrary(GL_LIB_FIXED))
    goto err_free_render_data;

  graphics->render_data = render_data;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  // We want to deal internally with 32bit surfaces
  render_data->pixels = cmalloc(sizeof(Uint32) * GL_POWER_2_WIDTH *
   GL_POWER_2_HEIGHT);

  if(!render_data->pixels)
    goto err_free_render_data;

  render_data->ratio = conf->video_ratio;
  if(!set_video_mode())
    goto err_free_render_data;

  return true;

err_free_render_data:
  free(render_data);
err_out:
  return false;
}

static void gl2_free_video(struct graphics_data *graphics)
{
  free(graphics->render_data);
  graphics->render_data = NULL;
}

static void gl2_remap_charsets(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;
  render_data->remap_texture = true;
}

static void gl2_remap_char(struct graphics_data *graphics, Uint16 chr)
{
  struct gl2_render_data *render_data = graphics->render_data;
  render_data->remap_char[chr] = true;
}

static void gl2_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
  gl2_remap_char(graphics, chr);
}

static void gl2_resize_screen(struct graphics_data *graphics,
 int width, int height)
{
  struct gl2_render_data *render_data = graphics->render_data;
  int v_width, v_height;

  // FIXME: Hack, remove
  get_context_width_height(graphics, &width, &height);

  // If the window is exactly 640x350, then any filtering is useless.
  // Turning filtering off here might speed things up.
  //
  // Otherwise, linear filtering breaks if the window is smaller than
  // 640x350, so also turn it off here.
  if(width == 640 && height == 350)
    render_data->ignore_linear = true;
  else if(width < 640 || height < 350)
    render_data->ignore_linear = true;
  else
    render_data->ignore_linear = false;

  fix_viewport_ratio(width, height, &v_width, &v_height, render_data->ratio);
  gl2.glViewport((width - v_width) >> 1, (height - v_height) >> 1,
   v_width, v_height);
  gl_check_error();

  gl2.glGenTextures(3, render_data->texture_number);
  gl_check_error();

  gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);
  gl_check_error();

  gl_set_filter_method(graphics->gl_filter_method, gl2.glTexParameterf);

  gl2.glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl2.glEnableClientState(GL_VERTEX_ARRAY);
  gl2.glEnableClientState(GL_COLOR_ARRAY);
  gl2.glEnable(GL_TEXTURE_2D);

  gl2.glAlphaFunc(GL_GREATER, 0.565f);
  gl_check_error();

  gl2.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
  gl_check_error();

  memset(render_data->pixels, 255,
   sizeof(Uint32) * GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);

  gl2.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
   render_data->pixels);
  gl_check_error();

  gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl2.glTexParameterf);

  gl2.glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 256, 256, 0, GL_ALPHA,
   GL_UNSIGNED_BYTE, render_data->pixels);
  gl_check_error();

  gl2_remap_charsets(graphics);

  gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[2]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl2.glTexParameterf);

  gl2.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 32, 0, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->pixels);
  gl_check_error();
}

static bool gl2_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  gl_set_attributes(graphics);

  if(!gl_set_video_mode(graphics, width, height, depth, fullscreen, resize))
    return false;

  if(!gl_load_syms(gl2_syms_map))
    return false;

  // We need a specific version of OpenGL; desktop GL must be 1.1.
  // All OpenGL ES 1.x implementations are supported, so don't do
  // the check with EGL configurations (EGL implies OpenGL ES).
#ifndef CONFIG_EGL
  {
    static bool initialized = false;

    if(!initialized)
    {
      const char *version;
      double version_float;

      version = (const char *)gl2.glGetString(GL_VERSION);
      if(!version)
        return false;

      version_float = atof(version);
      if(version_float < 1.1)
      {
        warn("Need >= OpenGL 1.1, got OpenGL %.1f.\n", version_float);
        return false;
      }

      initialized = true;
    }
  }
#endif

  gl2_resize_screen(graphics, width, height);
  return true;
}

static void gl2_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct gl2_render_data *render_data = graphics->render_data;
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
    render_data->palette[i*3  ] = (GLubyte)palette[i].r;
    render_data->palette[i*3+1] = (GLubyte)palette[i].g;
    render_data->palette[i*3+2] = (GLubyte)palette[i].b;
  }
}

static char *gl2_char_bitmask_to_texture(signed char *c, char *p)
{
  // This tests the 7th bit, if 0, result is 0.
  // If 1, result is 255 (because of sign extended bit shift!).
  // Note the use of char constants to force 8bit calculation.
  *(p++) = *c << 24 >> 31;
  *(p++) = *c << 25 >> 31;
  *(p++) = *c << 26 >> 31;
  *(p++) = *c << 27 >> 31;
  *(p++) = *c << 28 >> 31;
  *(p++) = *c << 29 >> 31;
  *(p++) = *c << 30 >> 31;
  *(p++) = *c << 31 >> 31;
  return p;
}

static inline void gl2_do_remap_charsets(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->charset_texture;
  unsigned int i, j, k;

  for(i = 0; i < 16; i++, c += -14 + 32 * 14)
    for(j = 0; j < 14; j++, c += -32 * 14 + 1)
      for(k = 0; k < 32; k++, c += 14)
        p = gl2_char_bitmask_to_texture(c, p);

  gl2.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32 * 8, 16 * 14, GL_ALPHA,
    GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static inline void gl2_do_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  struct gl2_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->charset_texture;
  unsigned int i;

  c += chr * 14;

  for(i = 0; i < 14; i++, c++)
    p = gl2_char_bitmask_to_texture(c, p);

  gl2.glTexSubImage2D(GL_TEXTURE_2D, 0, chr % 32 * 8, chr / 32 * 14, 8, 14,
   GL_ALPHA, GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static int gl2_linear_filter_method(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;

  if(render_data->ignore_linear)
    return false;
  return strcasecmp(graphics->gl_filter_method, CONFIG_GL_FILTER_LINEAR) == 0;
}

static void gl2_render_graph(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;
  struct char_element *src = graphics->text_video;
  Sint32 i, i2, i3;
  Uint32 *dest;

  if(!graphics->screen_mode)
  {
    float *tex_coord_array = render_data->tex_coord_array;
    float *vertex_array    = render_data->vertex_array;
    GLubyte *color_array   = render_data->color_array;

    static const float tex_coord_array_single[2 * 4] = {
      0.0f,           0.0f,
      0.0f,           25.0f / 32.0f,
      80.0f / 128.0f, 0.0f,
      80.0f / 128.0f, 25.0f / 32.0f
    };

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
    gl_check_error();

    if(render_data->remap_texture)
    {
      gl2_do_remap_charsets(graphics);
      render_data->remap_texture = false;
      memset(render_data->remap_char, false, sizeof(Uint8) * CHARSET_SIZE * 2);
    }
    else
    {
      for(i = 0; i < CHARSET_SIZE * 2; i++)
      {
        if(render_data->remap_char[i])
        {
          gl2_do_remap_char(graphics, i);
          render_data->remap_char[i] = false;
        }
      }
    }

    if(gl2_linear_filter_method(graphics))
    {
      int width, height;

      /* Put the initial rendering's viewport in the center of the screen
       * so that when it is linearly stretched in sync_screen() it is
       * masked by the stretched image.
       */
      get_context_width_height(graphics, &width, &height);
      gl2.glViewport((width - 640) >> 1, (height - 350) >> 1, 640, 350);
      gl_check_error();
    }

    dest = render_data->background_texture;

    for(i = 0; i < SCREEN_W * SCREEN_H; i++, dest++, src++)
     *dest = graphics->flat_intensity_palette[src->bg_color];

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[2]);
    gl_check_error();

    gl2.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H, GL_RGBA,
     GL_UNSIGNED_BYTE, render_data->background_texture);
     gl_check_error();
   
    gl2.glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_array_single);
    gl_check_error();

    gl2.glVertexPointer(2, GL_FLOAT, 0, vertex_array_single);
    gl_check_error();

    gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array_white);
    gl_check_error();

    gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_check_error();

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
    gl_check_error();

    gl2.glEnable(GL_ALPHA_TEST);

    src = graphics->text_video;

    gl2.glTexCoordPointer(2, GL_FLOAT, 0, render_data->tex_coord_array);
    gl_check_error();

    gl2.glVertexPointer(2, GL_FLOAT, 0, render_data->vertex_array);
    gl_check_error();

    gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, render_data->color_array);
    gl_check_error();

    i = 0;
    while(true)
    {
      GLubyte *pal_base;

      for(i2 = 0; i2 < SCREEN_W; i2++)
      {
        GLubyte *pal_base = &render_data->palette[src->fg_color * 3];

        tex_coord_array[0] = ((src->char_value % 32) + 0) / 32.0f;
        tex_coord_array[1] = ((src->char_value / 32) + 1) * 14.0f / 256.0f;

        tex_coord_array[2] = ((src->char_value % 32) + 0) / 32.0f;
        tex_coord_array[3] = ((src->char_value / 32) + 0) * 14.0f / 256.0f;

        tex_coord_array[4] = ((src->char_value % 32) + 1) / 32.0f;
        tex_coord_array[5] = ((src->char_value / 32) + 1) * 14.0f / 256.0f;

        tex_coord_array[6] = ((src->char_value % 32) + 1) / 32.0f;
        tex_coord_array[7] = ((src->char_value / 32) + 0) * 14.0f / 256.0f;

        vertex_array[0] = (((i2     + 0) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[1] = (((25 - i - 1) * 2.0f) / 25.0f) - 1.0f;

        vertex_array[2] = (((i2     + 0) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[3] = (((25 - i + 0) * 2.0f) / 25.0f) - 1.0f;

        vertex_array[4] = (((i2     + 1) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[5] = (((25 - i - 1) * 2.0f) / 25.0f) - 1.0f;

        vertex_array[6] = (((i2     + 1) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[7] = (((25 - i + 0) * 2.0f) / 25.0f) - 1.0f;

        for(i3 = 0; i3 < 4; i3++)
        {
          memcpy(&color_array[i3 * 4], pal_base, 3);
          color_array[i3 * 4 + 3] = 255;
        }

        tex_coord_array += 8;
        vertex_array += 8;
        color_array += 4 * 4;
        src++;
      }

      if(++i == SCREEN_H)
        break;
      src += SCREEN_W;

      // Draw this row backwards, so we can keep the strip contiguous

      for(i2 = SCREEN_W - 1; i2 >= 0; i2--)
      {
        src--;

        pal_base = &render_data->palette[src->fg_color * 3];

        tex_coord_array[0] = ((src->char_value % 32) + 1) / 32.0f;
        tex_coord_array[1] = ((src->char_value / 32) + 1) * 14.0f / 256.0f;

        tex_coord_array[2] = ((src->char_value % 32) + 1) / 32.0f;
        tex_coord_array[3] = ((src->char_value / 32) + 0) * 14.0f / 256.0f;

        tex_coord_array[4] = ((src->char_value % 32) + 0) / 32.0f;
        tex_coord_array[5] = ((src->char_value / 32) + 1) * 14.0f / 256.0f;

        tex_coord_array[6] = ((src->char_value % 32) + 0) / 32.0f;
        tex_coord_array[7] = ((src->char_value / 32) + 0) * 14.0f / 256.0f;

        vertex_array[0] = (((i2     + 1) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[1] = (((25 - i - 1) * 2.0f) / 25.0f) - 1.0f;

        vertex_array[2] = (((i2     + 1) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[3] = (((25 - i + 0) * 2.0f) / 25.0f) - 1.0f;

        vertex_array[4] = (((i2     + 0) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[5] = (((25 - i - 1) * 2.0f) / 25.0f) - 1.0f;

        vertex_array[6] = (((i2     + 0) * 2.0f) / 80.0f) - 1.0f;
        vertex_array[7] = (((25 - i + 0) * 2.0f) / 25.0f) - 1.0f;

        for(i3 = 0; i3 < 4; i3++)
        {
          memcpy(&color_array[i3 * 4], pal_base, 3);
          color_array[i3 * 4 + 3] = 255;
        }

        tex_coord_array += 8;
        vertex_array += 8;
        color_array += 4 * 4;
      }

      if(++i == SCREEN_H)
        break;
      src += SCREEN_W;
    }

    gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, SCREEN_W * SCREEN_H * 4);
    gl_check_error();

    gl2.glDisable(GL_ALPHA_TEST);
  }
  else
  {
    static const float tex_coord_array_single[2 * 4] = {
      0.0f,             0.0f,
      0.0f,             350.0f / 512.0f,
      640.0f / 1024.0f, 0.0f,
      640.0f / 1024.0f, 350.0f / 512.0f,
    };

    render_graph32s(render_data->pixels, 640 * 4, graphics,
     set_colors32[graphics->screen_mode]);

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);
    gl_check_error();

    gl2.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 350, GL_RGBA,
      GL_UNSIGNED_BYTE, render_data->pixels);
    gl_check_error();

    gl2.glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_array_single);
    gl_check_error();

    gl2.glVertexPointer(2, GL_FLOAT, 0, vertex_array_single);
    gl_check_error();

    gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array_white);
    gl_check_error();

    gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_check_error();

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
    gl_check_error();
  }
}

static void gl2_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct gl2_render_data *render_data = graphics->render_data;
  GLubyte *pal_base = &render_data->palette[color * 3];

  const float vertex_array[2 * 4] = {
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + lines + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + lines + offset)*-2.0f/350.0f+1.0f
  };

  const GLubyte color_array[4 * 4] = {
    pal_base[0], pal_base[1], pal_base[2], 255,
    pal_base[0], pal_base[1], pal_base[2], 255,
    pal_base[0], pal_base[1], pal_base[2], 255,
    pal_base[0], pal_base[1], pal_base[2], 255
  };

  gl2.glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl2.glDisable(GL_TEXTURE_2D);

  gl2.glVertexPointer(2, GL_FLOAT, 0, vertex_array);
  gl_check_error();

  gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array);
  gl_check_error();

  gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  gl2.glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl2.glEnable(GL_TEXTURE_2D);
}

static void gl2_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  const float vertex_array[2 * 4] = {
     x*2.0f/640.0f-1.0f,       y*-2.0f/350.0f+1.0f,
     x*2.0f/640.0f-1.0f,      (y + h)*-2.0f/350.0f+1.0f,
    (x + w)*2.0f/640.0f-1.0f,  y*-2.0f/350.0f+1.0f,
    (x + w)*2.0f/640.0f-1.0f, (y + h)*-2.0f/350.0f+1.0f
  };

  gl2.glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl2.glDisable(GL_TEXTURE_2D);
  gl2.glEnable(GL_BLEND);

  gl2.glVertexPointer(2, GL_FLOAT, 0, vertex_array);
  gl_check_error();

  gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array_white);
  gl_check_error();

  gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  gl2.glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl2.glEnable(GL_TEXTURE_2D);
  gl2.glDisable(GL_BLEND);
}

static void gl2_sync_screen(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;

  if(gl2_linear_filter_method(graphics) && !graphics->screen_mode)
  {
    int v_width, v_height;
    int width, height;

    static const float tex_coord_array_single[2 * 4] = {
       0.0f,             350.0f / 512.0f,
       0.0f,             0.0f,
       640.0f / 1024.0f, 350.0f / 512.0f,
       640.0f / 1024.0f, 0.0f,
    };

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);
    gl_check_error();

    get_context_width_height(graphics, &width, &height);
    gl2.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
     (width - 640) >> 1, (height - 350) >> 1, 1024, 512, 0);
    gl_check_error();

    fix_viewport_ratio(width, height, &v_width, &v_height, render_data->ratio);
    gl2.glViewport((width - v_width) >> 1, (height - v_height) >> 1,
     v_width, v_height);
    gl_check_error();

    gl2.glClear(GL_COLOR_BUFFER_BIT);
    gl_check_error();

    gl2.glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_array_single);
    gl_check_error();

    gl2.glVertexPointer(2, GL_FLOAT, 0, vertex_array_single);
    gl_check_error();

    gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array_white);
    gl_check_error();

    gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_check_error();

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
    gl_check_error();
  }

  gl_swap_buffers(graphics);

  gl2.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();
}

void render_gl2_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = gl2_init_video;
  renderer->free_video = gl2_free_video;
  renderer->check_video_mode = gl_check_video_mode;
  renderer->set_video_mode = gl2_set_video_mode;
  renderer->update_colors = gl2_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_charsets = gl2_remap_charsets;
  renderer->remap_char = gl2_remap_char;
  renderer->remap_charbyte = gl2_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = gl2_render_graph;
  renderer->render_cursor = gl2_render_cursor;
  renderer->render_mouse = gl2_render_mouse;
  renderer->sync_screen = gl2_sync_screen;
}
