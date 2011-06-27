/* MegaZeux
 *
 * Copyright (C) 2008 Joel Bouchard Lamontagne <logicow@gmail.com>
 * Copyright (C) 2006-2007 Gilead Kutnick <exophase@adelphia.net>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "platform.h"
#include "render.h"
#include "renderers.h"
#include "util.h"
#include "data.h"

#ifdef CONFIG_SDL
#include "render_sdl.h"
#endif

#ifdef CONFIG_EGL
#include <GLES2/gl2.h>
#include "render_egl.h"
typedef GLenum GLiftype;
#else
typedef GLint GLiftype;
#endif

#include "render_gl.h"

enum
{
  ATTRIB_POSITION,
  ATTRIB_TEXCOORD,
  ATTRIB_COLOR,
};

static struct
{
  void (GL_APIENTRY *glAttachShader)(GLuint program, GLuint shader);
  void (GL_APIENTRY *glBindAttribLocation)(GLuint program, GLuint index,
   const char *name);
  void (GL_APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (GL_APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
  void (GL_APIENTRY *glClear)(GLbitfield mask);
  void (GL_APIENTRY *glCompileShader)(GLuint shader);
  void (GL_APIENTRY *glCopyTexImage2D)(GLenum target, GLint level,
   GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height,
   GLint border);
  GLenum (GL_APIENTRY *glCreateProgram)(void);
  GLenum (GL_APIENTRY *glCreateShader)(GLenum type);
  void (GL_APIENTRY *glDisable)(GLenum cap);
  void (GL_APIENTRY *glDisableVertexAttribArray)(GLuint index);
  void (GL_APIENTRY *glDrawArrays)(GLenum mode, GLint first, GLsizei count);
  void (GL_APIENTRY *glEnable)(GLenum cap);
  void (GL_APIENTRY *glEnableVertexAttribArray)(GLuint index);
  void (GL_APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
  GLenum (GL_APIENTRY *glGetError)(void);
  void (GL_APIENTRY *glGetProgramInfoLog)(GLuint program, GLsizei bufsize,
   GLsizei *len, char *infolog);
  void (GL_APIENTRY *glGetShaderInfoLog)(GLuint shader, GLsizei bufsize,
   GLsizei *len, char *infolog);
  const GLubyte* (GL_APIENTRY *glGetString)(GLenum name);
  void (GL_APIENTRY *glLinkProgram)(GLuint program);
  void (GL_APIENTRY *glShaderSource)(GLuint shader, GLsizei count,
   const char **strings, const GLint *length);
  void (GL_APIENTRY *glTexImage2D)(GLenum target, GLint level,
   GLiftype internalformat, GLsizei width, GLsizei height, GLint border,
   GLenum format, GLenum type, const GLvoid *pixels);
  void (GL_APIENTRY *glTexParameterf)(GLenum target, GLenum pname,
   GLfloat param);
  void (GL_APIENTRY *glTexSubImage2D)(GLenum target, GLint level,
   GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
   GLenum format, GLenum type, const void *pixels);
  void (GL_APIENTRY *glUseProgram)(GLuint program);
  void (GL_APIENTRY *glVertexAttribPointer)(GLuint index, GLint size,
   GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
  void (GL_APIENTRY *glViewport)(GLint x, GLint y, GLsizei width,
   GLsizei height);
}
glsl;

static const struct dso_syms_map glsl_syms_map[] =
{
  { "glAttachShader",             (fn_ptr *)&glsl.glAttachShader },
  { "glBindAttribLocation",       (fn_ptr *)&glsl.glBindAttribLocation },
  { "glBindTexture",              (fn_ptr *)&glsl.glBindTexture },
  { "glBlendFunc",                (fn_ptr *)&glsl.glBlendFunc },
  { "glClear",                    (fn_ptr *)&glsl.glClear },
  { "glCompileShader",            (fn_ptr *)&glsl.glCompileShader },
  { "glCopyTexImage2D",           (fn_ptr *)&glsl.glCopyTexImage2D },
  { "glCreateProgram",            (fn_ptr *)&glsl.glCreateProgram },
  { "glCreateShader",             (fn_ptr *)&glsl.glCreateShader },
  { "glDisable",                  (fn_ptr *)&glsl.glDisable },
  { "glDisableVertexAttribArray", (fn_ptr *)&glsl.glDisableVertexAttribArray },
  { "glDrawArrays",               (fn_ptr *)&glsl.glDrawArrays },
  { "glEnable",                   (fn_ptr *)&glsl.glEnable },
  { "glEnableVertexAttribArray",  (fn_ptr *)&glsl.glEnableVertexAttribArray },
  { "glGenTextures",              (fn_ptr *)&glsl.glGenTextures },
  { "glGetError",                 (fn_ptr *)&glsl.glGetError },
  { "glGetProgramInfoLog",        (fn_ptr *)&glsl.glGetProgramInfoLog },
  { "glGetShaderInfoLog",         (fn_ptr *)&glsl.glGetShaderInfoLog },
  { "glGetString",                (fn_ptr *)&glsl.glGetString },
  { "glLinkProgram",              (fn_ptr *)&glsl.glLinkProgram },
  { "glShaderSource",             (fn_ptr *)&glsl.glShaderSource },
  { "glTexImage2D",               (fn_ptr *)&glsl.glTexImage2D },
  { "glTexParameterf",            (fn_ptr *)&glsl.glTexParameterf },
  { "glTexSubImage2D",            (fn_ptr *)&glsl.glTexSubImage2D },
  { "glUseProgram",               (fn_ptr *)&glsl.glUseProgram },
  { "glVertexAttribPointer",      (fn_ptr *)&glsl.glVertexAttribPointer },
  { "glViewport",                 (fn_ptr *)&glsl.glViewport },
  { NULL, NULL}
};

#define gl_check_error() gl_error(__FILE__, __LINE__, glsl.glGetError)

struct glsl_render_data
{
#ifdef CONFIG_EGL
  struct egl_render_data egl;
#endif
  Uint32 *pixels;
  Uint32 charset_texture[CHAR_H * CHARSET_SIZE * CHAR_W * 2];
  Uint32 background_texture[SCREEN_W * SCREEN_H];
  GLuint texture_number[3];
  GLubyte palette[3 * SMZX_PAL_SIZE];
  Uint8 remap_texture;
  Uint8 remap_char[CHARSET_SIZE * 2];
  enum ratio_type ratio;
  GLuint scaler_program;
  GLuint tilemap_program;
  GLuint tilemap_smzx12_program;
  GLuint tilemap_smzx3_program;
  GLuint mouse_program;
  GLuint cursor_program;
};

static const char *source_cache[SHADERS_CURSOR_FRAG - SHADERS_SCALER_VERT + 1];

#define INFO_MAX 1000

static void glsl_verify_compile(struct glsl_render_data *render_data,
 GLenum shader)
{
  char buffer[INFO_MAX];
  int len = 0;

  glsl.glGetShaderInfoLog(shader, INFO_MAX - 1, &len, buffer);
  buffer[len] = 0;

  if(len > 0)
    warn("%s", buffer);
}

static void glsl_verify_link(struct glsl_render_data *render_data,
 GLenum program)
{
  char buffer[INFO_MAX];
  int len = 0;

  glsl.glGetProgramInfoLog(program, INFO_MAX - 1, &len, buffer);
  buffer[len] = 0;

  if(len > 0)
    warn("%s", buffer);
}

static char *glsl_load_string(const char *filename)
{
  char *buffer = NULL;
  unsigned long size;
  FILE *f;

  f = fopen_unsafe(filename, "rb");
  if(!f)
    goto err_out;

  size = ftell_and_rewind(f);
  if(!size)
    goto err_close;

  buffer = cmalloc(size + 1);
  buffer[size] = '\0';

  if(fread(buffer, size, 1, f) != 1)
  {
    free(buffer);
    buffer = NULL;
  }

err_close:
  fclose(f);
err_out:
  return buffer;
}

static GLenum glsl_load_shader(struct graphics_data *graphics,
 enum resource_id res, GLenum type)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int index = res - SHADERS_SCALER_VERT;
  GLenum shader;

  assert(res >= SHADERS_SCALER_VERT && res <= SHADERS_CURSOR_FRAG);

  // If we've already seen this shader, it's loaded, and we don't
  // need to do any more file I/O.

  if(!source_cache[index])
  {
    source_cache[index] = glsl_load_string(mzx_res_get_by_id(res));
    if(!source_cache[index])
    {
      warn("Failed to load shader '%s'\n", mzx_res_get_by_id(res));
      return 0;
    }
  }

  shader = glsl.glCreateShader(type);

#ifdef CONFIG_EGL
  {
    const GLchar *sources[2];
    GLint lengths[2];

    sources[0] = "precision mediump float;";
    sources[1] = source_cache[index];

    lengths[0] = (GLint)strlen(sources[0]);
    lengths[1] = (GLint)strlen(source_cache[index]);

    glsl.glShaderSource(shader, 2, sources, lengths);
  }
#else
  {
    GLint length = (GLint)strlen(source_cache[index]);
    glsl.glShaderSource(shader, 1, &source_cache[index], &length);
  }
#endif

  glsl.glCompileShader(shader);
  glsl_verify_compile(render_data, shader);

  return shader;
}

static GLuint glsl_load_program(struct graphics_data *graphics,
 enum resource_id v_res, enum resource_id f_res)
{
  GLenum vertex, fragment;
  GLuint program = 0;
  char *path;

  path = cmalloc(MAX_PATH);

  vertex = glsl_load_shader(graphics, v_res, GL_VERTEX_SHADER);
  if(!vertex)
    goto err_free_path;

  fragment = glsl_load_shader(graphics, f_res, GL_FRAGMENT_SHADER);
  if(!fragment)
    goto err_free_path;

  program = glsl.glCreateProgram();

  glsl.glAttachShader(program, vertex);
  glsl.glAttachShader(program, fragment);

err_free_path:
  free(path);
  return program;
}

static void glsl_load_shaders(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;

  render_data->scaler_program = glsl_load_program(graphics,
   SHADERS_SCALER_VERT, SHADERS_SCALER_FRAG);
  if(render_data->scaler_program)
  {
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->scaler_program);
    glsl_verify_link(render_data, render_data->scaler_program);
  }

  render_data->tilemap_program = glsl_load_program(graphics,
   SHADERS_TILEMAP_VERT, SHADERS_TILEMAP_FRAG);
  if(render_data->tilemap_program)
  {
    glsl.glBindAttribLocation(render_data->tilemap_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->tilemap_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->tilemap_program);
    glsl_verify_link(render_data, render_data->tilemap_program);
  }

  render_data->tilemap_smzx12_program = glsl_load_program(graphics,
   SHADERS_TILEMAP_VERT, SHADERS_TILEMAP_SMZX12_FRAG);
  if(render_data->tilemap_smzx12_program)
  {
    glsl.glBindAttribLocation(render_data->tilemap_smzx12_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->tilemap_smzx12_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->tilemap_smzx12_program);
    glsl_verify_link(render_data, render_data->tilemap_smzx12_program);
  }

  render_data->tilemap_smzx3_program = glsl_load_program(graphics,
   SHADERS_TILEMAP_VERT, SHADERS_TILEMAP_SMZX3_FRAG);
  if(render_data->tilemap_smzx3_program)
  {
    glsl.glBindAttribLocation(render_data->tilemap_smzx3_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->tilemap_smzx3_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->tilemap_smzx3_program);
    glsl_verify_link(render_data, render_data->tilemap_smzx3_program);
  }

  render_data->mouse_program = glsl_load_program(graphics,
   SHADERS_MOUSE_VERT, SHADERS_MOUSE_FRAG);
  if(render_data->mouse_program)
  {
    glsl.glBindAttribLocation(render_data->mouse_program,
     ATTRIB_POSITION, "Position");
    glsl.glLinkProgram(render_data->mouse_program);
    glsl_verify_link(render_data, render_data->mouse_program);
  }

  render_data->cursor_program  = glsl_load_program(graphics,
   SHADERS_CURSOR_VERT, SHADERS_CURSOR_FRAG);
  if(render_data->cursor_program)
  {
    glsl.glBindAttribLocation(render_data->cursor_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->cursor_program,
     ATTRIB_COLOR, "Color");
    glsl.glLinkProgram(render_data->cursor_program);
    glsl_verify_link(render_data, render_data->cursor_program);
  }
}

static bool glsl_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct glsl_render_data *render_data;

  render_data = cmalloc(sizeof(struct glsl_render_data));
  if(!render_data)
    return false;

  if(!GL_LoadLibrary(GL_LIB_PROGRAMMABLE))
    goto err_free;

  graphics->render_data = render_data;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  render_data->pixels = cmalloc(sizeof(Uint32) * GL_POWER_2_WIDTH *
   GL_POWER_2_HEIGHT);
  if(!render_data->pixels)
    goto err_free;

  render_data->ratio = conf->video_ratio;
  if(!set_video_mode())
    goto err_free;

  return true;

err_free:
  free(render_data);
  return false;
}

static void glsl_free_video(struct graphics_data *graphics)
{
  int i;

  for(i = 0; i < SHADERS_CURSOR_FRAG - SHADERS_SCALER_VERT + 1; i++)
    if(source_cache[i])
      free((void *)source_cache[i]);

  free(graphics->render_data);
  graphics->render_data = NULL;
}

static void glsl_remap_charsets(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  render_data->remap_texture = true;
}

// FIXME: Many magic numbers
static void glsl_resize_screen(struct graphics_data *graphics,
 int width, int height)
{
  struct glsl_render_data *render_data = graphics->render_data;

  glsl.glViewport(0, 0, width, height);
  gl_check_error();

  glsl.glGenTextures(3, render_data->texture_number);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_LINEAR, glsl.glTexParameterf);

  glsl.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  memset(render_data->pixels, 255,
   sizeof(Uint32) * GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);

  glsl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
   render_data->pixels);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, glsl.glTexParameterf);

  glsl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->pixels);
  gl_check_error();

  glsl_remap_charsets(graphics);

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[2]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, glsl.glTexParameterf);

  glsl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 32, 0, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->pixels);
  gl_check_error();

  glsl_load_shaders(graphics);
}

static bool glsl_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  gl_set_attributes(graphics);

  if(!gl_set_video_mode(graphics, width, height, depth, fullscreen, resize))
    return false;

  if(!gl_load_syms(glsl_syms_map))
    return false;

  // We need a specific version of OpenGL; desktop GL must be 2.0.
  // All OpenGL ES 2.0 implementations are supported, so don't do
  // the check with EGL configurations (EGL implies OpenGL ES).
#ifndef CONFIG_EGL
  {
    static bool initialized = false;

    if(!initialized)
    {
      const char *version;
      double version_float;

      version = (const char *)glsl.glGetString(GL_VERSION);
      if(!version)
        return false;

      version_float = atof(version);
      if(version_float < 2.0)
      {
        warn("Need >= OpenGL 2.0, got OpenGL %.1f.\n", version_float);
        return false;
      }

      initialized = true;
    }
  }
#endif

  glsl_resize_screen(graphics, width, height);
  return true;
}

static void glsl_remap_char(struct graphics_data *graphics, Uint16 chr)
{
  struct glsl_render_data *render_data = graphics->render_data;
  render_data->remap_char[chr] = true;
}

static void glsl_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
  glsl_remap_char(graphics, chr);
}

static int *glsl_char_bitmask_to_texture(signed char *c, int *p)
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

static inline void glsl_do_remap_charsets(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  int *p = (int *)render_data->charset_texture;
  unsigned int i, j, k;

  for(i = 0; i < 16; i++, c += -14 + 32 * 14)
    for(j = 0; j < 14; j++, c += -32 * 14 + 1)
      for(k = 0; k < 32; k++, c += 14)
        p = glsl_char_bitmask_to_texture(c, p);

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32 * 8, 16 * 14, GL_RGBA,
    GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static inline void glsl_do_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  struct glsl_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  int *p = (int *)render_data->charset_texture;
  unsigned int i;

  c += chr * 14;

  for(i = 0; i < 14; i++, c++)
    p = glsl_char_bitmask_to_texture(c, p);

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0, chr % 32 * 8, chr / 32 * 14, 8, 14,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static void glsl_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct glsl_render_data *render_data = graphics->render_data;
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

static void glsl_render_graph(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct char_element *src = graphics->text_video;
  Uint32 *colorptr, *dest, i;
  int width, height;

  static const float tex_coord_array_single[2 * 4] = {
    0.0f,          0.0f,
    0.0f,          25.0f,
    25.0f / 80.0f, 0.0f,
    25.0f / 80.0f, 25.0f,
  };

  get_context_width_height(graphics, &width, &height);
  if(width < 640 || height < 350)
  {
    if(width >= 1024)
      width = 1024;
    if(height >= 512)
      height = 512;
  }
  else
  {
    width = 640;
    height = 350;
  }

  glsl.glViewport(0, 0, width, height);
  gl_check_error();

  if(graphics->screen_mode == 0)
    glsl.glUseProgram(render_data->tilemap_program);
  else if(graphics->screen_mode == 1 || graphics->screen_mode == 2)
    glsl.glUseProgram(render_data->tilemap_smzx12_program);
  else
    glsl.glUseProgram(render_data->tilemap_smzx3_program);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  gl_check_error();

  if(render_data->remap_texture)
  {
    glsl_do_remap_charsets(graphics);
    render_data->remap_texture = false;
    memset(render_data->remap_char, false, sizeof(Uint8) * CHARSET_SIZE * 2);
  }
  else
  {
    for(i = 0; i < CHARSET_SIZE * 2; i++)
    {
      if(render_data->remap_char[i])
      {
        glsl_do_remap_char(graphics, i);
        render_data->remap_char[i] = false;
      }
    }
  }

  dest = render_data->background_texture;

  for(i = 0; i < SCREEN_W * SCREEN_H; i++, dest++, src++)
    *dest = (src->char_value<<16) + (src->bg_color<<8) + src->fg_color;

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  gl_check_error();

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 225, SCREEN_W, SCREEN_H, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->background_texture);
  gl_check_error();

  colorptr = graphics->flat_intensity_palette;
  dest = render_data->background_texture;

  for(i = 0; i < SMZX_PAL_SIZE; i++, dest++, colorptr++)
    *dest = *colorptr;

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 224, SMZX_PAL_SIZE, 1, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->background_texture);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array_single);
  gl_check_error();

  glsl.glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0,
   tex_coord_array_single);
  gl_check_error();

  glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);
  glsl.glDisableVertexAttribArray(ATTRIB_TEXCOORD);
}

static void glsl_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct glsl_render_data *render_data = graphics->render_data;
  GLubyte *pal_base = &render_data->palette[color * 3];

  const float vertex_array[2 * 4] = {
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + lines + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + lines + offset)*-2.0f/350.0f+1.0f
  };

  const GLubyte color_array[3 * 4] = {
    pal_base[0], pal_base[1], pal_base[2],
    pal_base[0], pal_base[1], pal_base[2],
    pal_base[0], pal_base[1], pal_base[2],
    pal_base[0], pal_base[1], pal_base[2],
  };

  glsl.glDisable(GL_TEXTURE_2D);

  glsl.glUseProgram(render_data->cursor_program);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_COLOR);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array);
  gl_check_error();

  glsl.glVertexAttribPointer(ATTRIB_COLOR, 3, GL_UNSIGNED_BYTE, GL_FALSE, 0,
   color_array);
  gl_check_error();

  glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);
  glsl.glDisableVertexAttribArray(ATTRIB_COLOR);
}

static void glsl_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct glsl_render_data *render_data = graphics->render_data;

  const float vertex_array[2 * 4] = {
     x*2.0f/640.0f-1.0f,       y*-2.0f/350.0f+1.0f,
     x*2.0f/640.0f-1.0f,      (y + h)*-2.0f/350.0f+1.0f,
    (x + w)*2.0f/640.0f-1.0f,  y*-2.0f/350.0f+1.0f,
    (x + w)*2.0f/640.0f-1.0f, (y + h)*-2.0f/350.0f+1.0f
  };

  glsl.glEnable(GL_BLEND);

  glsl.glUseProgram(render_data->mouse_program);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array);
  gl_check_error();

  glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);

  glsl.glDisable(GL_BLEND);
}

static void glsl_sync_screen(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int v_width, v_height, width, height;

  get_context_width_height(graphics, &width, &height);
  fix_viewport_ratio(width, height, &v_width, &v_height, render_data->ratio);
  glsl.glViewport((width - v_width) >> 1, (height - v_height) >> 1,
   v_width, v_height);

  if(width < 640 || height < 350)
  {
    width = (width < 1024) ? width : 1024;
    height = (height < 512) ? height : 512;
  }
  else
  {
    width = 640;
    height = 350;
  }

  glsl.glUseProgram(render_data->scaler_program);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);
  gl_check_error();

  glsl.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1024, 512, 0);
  gl_check_error();

  glsl.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_TEXCOORD);
 
  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array_single);
  gl_check_error();

  {
    const float tex_coord_array_single[2 * 4] = {
       0.0f,            height / 512.0f,
       0.0f,            0.0f,
       width / 1024.0f, height / 512.0f,
       width / 1024.0f, 0.0f,
    };

    glsl.glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0,
     tex_coord_array_single);
    gl_check_error();
  }

  glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);
  glsl.glDisableVertexAttribArray(ATTRIB_TEXCOORD);

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  gl_check_error();

  gl_swap_buffers(graphics);

  glsl.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();
}

void render_glsl_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = glsl_init_video;
  renderer->free_video = glsl_free_video;
  renderer->check_video_mode = gl_check_video_mode;
  renderer->set_video_mode = glsl_set_video_mode;
  renderer->update_colors = glsl_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_charsets = glsl_remap_charsets;
  renderer->remap_char = glsl_remap_char;
  renderer->remap_charbyte = glsl_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = glsl_render_graph;
  renderer->render_cursor = glsl_render_cursor;
  renderer->render_mouse = glsl_render_mouse;
  renderer->sync_screen = glsl_sync_screen;
}
