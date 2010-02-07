/* MegaZeux
 *
 * Copyright (C) 2007 Joel Bouchard Lamontagne <logicow@gmail.com>
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

#include "config.h"
#include "graphics.h"
#include "render.h"
#include "render_gl.h"
#include "renderers.h"

static void (*set_colors32[4])(graphics_data *, Uint32 *, Uint8, Uint8) =
{
  set_colors32_mzx,
  set_colors32_smzx,
  set_colors32_smzx,
  set_colors32_smzx3
};

#define SAFE_TEXTURE_MARGIN_X 0.0004
#define SAFE_TEXTURE_MARGIN_Y 0.0002

typedef struct
{
  int syms_loaded;
  void (APIENTRY *glAlphaFunc)(GLenum func, GLclampf ref);
  void (APIENTRY *glBegin)(GLenum mode);
  void (APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
  void (APIENTRY *glClear)(GLbitfield mask);
  void (APIENTRY *glColor3ubv)(const GLubyte *v);
  void (APIENTRY *glColor4f)(GLfloat red, GLfloat green, GLfloat blue,
   GLfloat alpha);
  void (APIENTRY *glColor4ub)(GLubyte red, GLubyte green, GLubyte blue,
   GLubyte alpha);
  void (APIENTRY *glCopyTexImage2D)(GLenum target, GLint level,
   GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height,
   GLint border);
  void (APIENTRY *glDisable)(GLenum cap);
  void (APIENTRY *glEnable)(GLenum cap);
  void (APIENTRY *glEnd)(void);
  void (APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
  const GLubyte* (APIENTRY *glGetString)(GLenum name);
  void (APIENTRY *glTexCoord2f)(GLfloat s, GLfloat t);
  void (APIENTRY *glTexImage2D)(GLenum target, GLint level,
   GLint internalformat, GLsizei width, GLsizei height, GLint border,
   GLenum format, GLenum type, const GLvoid *pixels);
  void (APIENTRY *glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
  void (APIENTRY *glTexParameteri)(GLenum target, GLenum pname, GLint param);
  void (APIENTRY *glTexSubImage2D)(GLenum target, GLint level, GLint xoffset,
   GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
   const GLvoid *pixels);
  void (APIENTRY *glVertex2f)(GLfloat x, GLfloat y);
  void (APIENTRY *glVertex2i)(GLint x, GLint y);
  void (APIENTRY *glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
} gl2_syms;

typedef struct
{
  Uint32 *pixels;
  Uint8 charset_texture[CHAR_H * CHARSET_SIZE * CHAR_W * 2];
  Uint32 background_texture[SCREEN_W * SCREEN_H];
  GLuint texture_number[3];
  GLubyte palette[3 * SMZX_PAL_SIZE];
  Uint8 remap_texture;
  Uint8 remap_char[CHARSET_SIZE * 2];
  Uint8 ignore_linear;
  gl2_syms gl;
} gl2_render_data;

static int gl2_load_syms (gl2_syms *gl)
{
  if (gl->syms_loaded)
    return true;

  // Since 1.1
  GL_LOAD_SYM(gl, glAlphaFunc)
  // Since 1.0
  GL_LOAD_SYM(gl, glBegin)
  // Since 1.1
  GL_LOAD_SYM(gl, glBindTexture)
  // Since 1.0
  GL_LOAD_SYM(gl, glBlendFunc)
  // Since 1.0
  GL_LOAD_SYM(gl, glClear)
  // Since 1.0
  GL_LOAD_SYM(gl, glColor3ubv)
  // Since 1.0
  GL_LOAD_SYM(gl, glColor4f)
  // Since 1.0
  GL_LOAD_SYM(gl, glColor4ub)
  // Since 1.1
  GL_LOAD_SYM(gl, glCopyTexImage2D)
  // Since 1.0 (parameters may require more recent version)
  GL_LOAD_SYM(gl, glDisable)
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
  GL_LOAD_SYM(gl, glTexParameterf)
  // Since 1.0
  GL_LOAD_SYM(gl, glTexParameteri)
  // Since 1.1
  GL_LOAD_SYM(gl, glTexSubImage2D)
  // Since 1.0
  GL_LOAD_SYM(gl, glVertex2f)
  // Since 1.0
  GL_LOAD_SYM(gl, glVertex2i)
  // Since 1.0
  GL_LOAD_SYM(gl, glViewport)

  gl->syms_loaded = true;
  return true;
}

static int gl2_init_video(graphics_data *graphics, config_info *conf)
{
  gl2_render_data *render_data = malloc(sizeof(gl2_render_data));
  gl2_syms *gl = &render_data->gl;

  if (!render_data)
    return false;

  if (!GL_CAN_USE)
  {
    free(render_data);
    return false;
  }

  graphics->render_data = render_data;
  gl->syms_loaded = false;

  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if (conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  // We want to deal internally with 32bit surfaces
  render_data->pixels = malloc(sizeof(Uint32) * GL_POWER_2_WIDTH *
   GL_POWER_2_HEIGHT);

  if (!render_data->pixels)
  {
    free(render_data);
    return false;
  }

  if (!set_video_mode())
  {
    free(render_data);
    return false;
  }

  // NOTE: This must come AFTER set_video_mode()!
  const char *version = (const char *)gl->glGetString(GL_VERSION);

  // we need a specific "version" of OpenGL compatibility
  if (version && atof(version) < 1.1)
  {
    fprintf(stderr, "Your OpenGL implementation is too old (need v1.1).\n");
    free(render_data);
    return false;
  }

  return true;
}

static void gl2_remap_charsets(graphics_data *graphics)
{
  gl2_render_data *render_data = graphics->render_data;
  render_data->remap_texture = true;
}

static void gl2_remap_char(graphics_data *graphics, Uint16 chr)
{
  gl2_render_data *render_data = graphics->render_data;
  render_data->remap_char[chr] = true;
}

static void gl2_remap_charbyte(graphics_data *graphics, Uint16 chr, Uint8 byte)
{
  gl2_remap_char(graphics, chr);
}

// FIXME: Many magic numbers
static void gl2_resize_screen(graphics_data *graphics, int viewport_width,
 int viewport_height)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;

  /* If the window is exactly 640x350, then any filtering is useless.
   * Turning filtering off here might speed things up.
   *
   * Otherwise, linear filtering breaks if the window is smaller than
   * 640x350, so also turn it off here.
   */
  if (viewport_width == 640 && viewport_height == 350)
    render_data->ignore_linear = true;
  else if (viewport_width < 640 || viewport_height < 350)
    render_data->ignore_linear = true;
  else
    render_data->ignore_linear = false;

  gl->glViewport(0, 0, viewport_width, viewport_height);

  gl->glGenTextures(3, render_data->texture_number);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);

  gl_set_filter_method(graphics->gl_filter_method, gl->glTexParameteri);

  gl->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  gl->glEnable(GL_TEXTURE_2D);
  gl->glAlphaFunc(GL_GREATER,0.565f);

  gl->glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  memset(render_data->pixels, 255,
   sizeof(Uint32) * GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);

  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE,
   render_data->pixels);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl->glTexParameteri);

  gl->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 256, 256, 0, GL_ALPHA,
   GL_UNSIGNED_BYTE, render_data->pixels);

  gl2_remap_charsets(graphics);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[2]);

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl->glTexParameteri);

  gl->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 32, 0, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->pixels);
}

static int gl2_set_video_mode(graphics_data *graphics, int width, int height,
 int depth, int flags, int fullscreen)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if (!SDL_SetVideoMode(width, height, depth, GL_STRIP_FLAGS(flags)))
    return false;

  if (!gl2_load_syms(gl))
    return false;

  gl2_resize_screen(graphics, width, height);

  return true;
}

static void gl2_update_colors(graphics_data *graphics, SDL_Color *palette,
 Uint32 count)
{
  gl2_render_data *render_data = graphics->render_data;
  Uint32 i;
  for (i = 0; i < count; i++)
  {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
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

static inline void gl2_do_remap_charsets(graphics_data *graphics)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->charset_texture;
  unsigned int i, j, k;

  for (i = 0; i < 16; i++, c += -14 + 32 * 14)
    for (j = 0; j < 14; j++, c += -32 * 14 + 1)
      for (k = 0; k < 32; k++, c += 14)
        p = gl2_char_bitmask_to_texture(c, p);

  gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32 * 8, 16 * 14, GL_ALPHA,
    GL_UNSIGNED_BYTE, render_data->charset_texture);
}

static inline void gl2_do_remap_char(graphics_data *graphics, Uint16 chr)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->charset_texture;
  unsigned int i;

  c += chr * 14;

  for (i = 0; i < 14; i++, c++)
    p = gl2_char_bitmask_to_texture(c, p);
  gl->glTexSubImage2D(GL_TEXTURE_2D, 0, chr % 32 * 8, chr / 32 * 14, 8, 14,
   GL_ALPHA, GL_UNSIGNED_BYTE, render_data->charset_texture);
}

static int gl2_linear_filter_method(graphics_data *graphics)
{
  gl2_render_data *render_data = graphics->render_data;

  if (render_data->ignore_linear)
    return false;
  return (strcasecmp(graphics->gl_filter_method, CONFIG_GL_FILTER_LINEAR) == 0);
}

static void gl2_render_graph(graphics_data *graphics)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;
  Uint32 i;
  float fi, fi2;
  Uint32 *dest;
  char_element *src = graphics->text_video;

  if (!graphics->screen_mode)
  {
    gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
    if (render_data->remap_texture)
    {
      gl2_do_remap_charsets(graphics);
      render_data->remap_texture = false;
      memset(render_data->remap_char, false, sizeof(Uint8) * CHARSET_SIZE * 2);
    }
    else
    {
      for (i = 0; i < CHARSET_SIZE * 2; i++)
        if (render_data->remap_char[i])
        {
          gl2_do_remap_char(graphics, i);
          render_data->remap_char[i] = false;
        }
    }

    if (gl2_linear_filter_method(graphics))
      gl->glViewport(0, 0, 640, 350);

    dest = render_data->background_texture;

    for(i = 0; i < SCREEN_W * SCREEN_H; i++, dest++, src++)
     *dest = graphics->flat_intensity_palette[src->bg_color];

    gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[2]);

    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H, GL_RGBA,
     GL_UNSIGNED_BYTE, render_data->background_texture);

    gl->glColor4f(1.0, 1.0, 1.0, 1.0);

    gl->glBegin(GL_QUADS);
      gl->glTexCoord2f(0, 25.0/32.0);
      gl->glVertex2i(-1, -1);
      gl->glTexCoord2f(0, 0);
      gl->glVertex2i(-1, 1);
      gl->glTexCoord2f(80.0/128.0, 0);
      gl->glVertex2i(1, 1);
      gl->glTexCoord2f(80.0/128.0, 25.0/32.0);
      gl->glVertex2i(1, -1);
    gl->glEnd();

    gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);

    gl->glEnable(GL_ALPHA_TEST);

    gl->glBegin(GL_QUADS);
      src = graphics->text_video;
      for(fi = 1; fi > -1; fi = fi - 2.0/25.0)
      {
        for(fi2 = -1; fi2 < 0.98; fi2 = fi2 + 2.0/80.0)
        {
          gl->glColor3ubv(&render_data->palette[src->fg_color * 3]);

          gl->glTexCoord2f(
            ((src->char_value % 32) + 1) * 0.03125   - SAFE_TEXTURE_MARGIN_X,
            ((src->char_value / 32) + 1) * 0.0546875 - SAFE_TEXTURE_MARGIN_Y
          );
          gl->glVertex2f(fi2 + 2.0/80.0, fi - 2.0/25.0);

          gl->glTexCoord2f(
            ((src->char_value % 32) + 1) * 0.03125   - SAFE_TEXTURE_MARGIN_X,
             (src->char_value / 32)      * 0.0546875 + SAFE_TEXTURE_MARGIN_Y
          );
          gl->glVertex2f(fi2 + 2.0/80.0, fi);

          gl->glTexCoord2f(
             (src->char_value % 32)      * 0.03125   + SAFE_TEXTURE_MARGIN_X,
             (src->char_value / 32)      * 0.0546875 + SAFE_TEXTURE_MARGIN_Y
          );
          gl->glVertex2f(fi2, fi);

          gl->glTexCoord2f(
             (src->char_value % 32)      * 0.03125   + SAFE_TEXTURE_MARGIN_X,
            ((src->char_value / 32) + 1) * 0.0546875 - SAFE_TEXTURE_MARGIN_Y
          );
          gl->glVertex2f(fi2, fi - 2.0/25.0);
          src++;
        }
      }
    gl->glEnd();

    gl->glDisable(GL_ALPHA_TEST);
  }
  else
  {
    render_graph32s(render_data->pixels, 640 * 4, graphics,
     set_colors32[graphics->screen_mode]);

    gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);

    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 350, GL_RGBA,
      GL_UNSIGNED_BYTE, render_data->pixels);

    gl->glColor4f(1.0, 1.0, 1.0, 1.0);

    gl->glBegin(GL_QUADS);
      gl->glTexCoord2f(0, 0.68359375);
      gl->glVertex2i(-1, -1);
      gl->glTexCoord2f(0, 0);
      gl->glVertex2i(-1, 1);
      gl->glTexCoord2f(0.625, 0);
      gl->glVertex2i(1, 1);
      gl->glTexCoord2f(0.625, 0.68359375);
      gl->glVertex2i(1, -1);
    gl->glEnd();

    gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  }
}

static void gl2_render_cursor(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 color, Uint8 lines, Uint8 offset)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;

  gl->glDisable(GL_TEXTURE_2D);

  gl->glBegin(GL_QUADS);
    gl->glColor3ubv(&render_data->palette[color * 3]);
    gl->glVertex2f((x * 8)*2.0/640.0-1.0,
                  (y * 14 + lines + offset)*-2.0/350.0+1.0);
    gl->glVertex2f((x * 8)*2.0/640.0-1.0,
                  (y * 14 + offset)*-2.0/350.0+1.0);
    gl->glVertex2f((x * 8 + 8)*2.0/640.0-1.0,
                  (y * 14 + offset)*-2.0/350.0+1.0);
    gl->glVertex2f((x * 8 + 8)*2.0/640.0-1.0,
                  (y * 14 + lines + offset)*-2.0/350.0+1.0);
  gl->glEnd();

  gl->glEnable(GL_TEXTURE_2D);
}

static void gl2_render_mouse(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 w, Uint8 h)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;

  gl->glDisable(GL_TEXTURE_2D);
  gl->glEnable(GL_BLEND);

  gl->glBegin(GL_QUADS);
    gl->glColor4ub(255, 255, 255, 255);
    gl->glVertex2f( x*2.0/640.0-1.0,         (y + h)*-2.0/350.0+1.0);
    gl->glVertex2f( x*2.0/640.0-1.0,          y*-2.0/350.0+1.0);
    gl->glVertex2f((x + w)*2.0/640.0-1.0,     y*-2.0/350.0+1.0);
    gl->glVertex2f((x + w)*2.0/640.0-1.0,    (y + h)*-2.0/350.0+1.0);
  gl->glEnd();

  gl->glEnable(GL_TEXTURE_2D);
  gl->glDisable(GL_BLEND);
}

static void gl2_sync_screen(graphics_data *graphics)
{
  gl2_render_data *render_data = graphics->render_data;
  gl2_syms *gl = &render_data->gl;

  if (gl2_linear_filter_method(graphics) && !graphics->screen_mode)
  {
    gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);
    gl->glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1024, 512, 0);

    if(!graphics->fullscreen)
      gl->glViewport(0, 0, graphics->window_width, graphics->window_height);
    else
      gl->glViewport(0, 0, graphics->resolution_width,
       graphics->resolution_height);

    gl->glColor4f(1.0, 1.0, 1.0, 1.0);

    gl->glBegin(GL_QUADS);
      gl->glTexCoord2f(0, 0.68359375);
      gl->glVertex2i(-1, 1);
      gl->glTexCoord2f(0, 0);
      gl->glVertex2i(-1, -1);
      gl->glTexCoord2f(0.625, 0);
      gl->glVertex2i(1, -1);
      gl->glTexCoord2f(0.625, 0.68359375);
      gl->glVertex2i(1, 1);
    gl->glEnd();

    gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  }

  SDL_GL_SwapBuffers();

  gl->glClear(GL_COLOR_BUFFER_BIT);
}

void render_gl2_register(graphics_data *graphics)
{
  graphics->init_video = gl2_init_video;
  graphics->check_video_mode = gl_check_video_mode;
  graphics->set_video_mode = gl2_set_video_mode;
  graphics->update_colors = gl2_update_colors;
  graphics->resize_screen = resize_screen_standard;
  graphics->remap_charsets = gl2_remap_charsets;
  graphics->remap_char = gl2_remap_char;
  graphics->remap_charbyte = gl2_remap_charbyte;
  graphics->get_screen_coords = get_screen_coords_scaled;
  graphics->set_screen_coords = set_screen_coords_scaled;
  graphics->render_graph = gl2_render_graph;
  graphics->render_cursor = gl2_render_cursor;
  graphics->render_mouse = gl2_render_mouse;
  graphics->sync_screen = gl2_sync_screen;
}
