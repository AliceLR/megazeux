/* MegaZeux
 *
 * Copyright (C) 2008 Joel Bouchard Lamontagne <logicow@gmail.com>
 * Copyright (C) 2006-2007 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#include "data.h"
#include "graphics.h"
#include "platform.h"
#include "render.h"
#include "render_layer.h"
#include "renderers.h"
#include "util.h"

#ifdef CONFIG_SDL
#include "render_sdl.h"
#endif

#ifdef CONFIG_EGL
#include <GLES2/gl2.h>
#include "render_egl.h"
#endif

#ifdef CONFIG_GLES
typedef GLenum GLiftype;
#else
typedef GLint GLiftype;
#endif

#include "render_gl.h"

static const struct gl_version gl_required_version = { 2, 0 };

#define CHARSET_COLS 64
#define CHARSET_ROWS (FULL_CHARSET_SIZE / CHARSET_COLS)
#define BG_WIDTH 128
#define BG_HEIGHT 32

#define SCREEN_PIX_W_F (1.0f * SCREEN_PIX_W)
#define SCREEN_PIX_H_F (1.0f * SCREEN_PIX_H)

// Must be powers of 2
#define TEX_DATA_WIDTH 512
#define TEX_DATA_HEIGHT 1024

// Charsets: CHARSET_COLS * CHAR_W (512) x CHARSET_ROWS * CHAR_H (896)
#define TEX_DATA_CHR_X 0
#define TEX_DATA_CHR_Y 0

// Palette: FULL_PAL_SIZE + 1 (273) x 1
#define TEX_DATA_PAL_X 0
#define TEX_DATA_PAL_Y 896

// Indicies: SMZX_PAL_SIZE (256) x 4
#define TEX_DATA_IDX_X 0
#define TEX_DATA_IDX_Y 897

// Layer: 81 x 26 (max)
#define TEX_DATA_LAYER_X 0
#define TEX_DATA_LAYER_Y 901

// NOTE: Layer data packing scheme
// (highest two bits currently unused but included as part of the char)
// w        z        y        x
// 00000000 00000000 00000000 00000000
// CCCCCCCC CCCCCCBB BBBBBBBF FFFFFFFF
#define LAYER_FG_POS 0
#define LAYER_BG_POS 9
#define LAYER_CHAR_POS 18

enum
{
  TEX_SCREEN_ID,
  TEX_DATA_ID,
  NUM_TEXTURES
};

enum
{
  ATTRIB_POSITION,
  ATTRIB_TEXCOORD,
  ATTRIB_COLOR,
};

/**
 * Some GL drivers attempt to run GLSL in software, resulting in extremely poor
 * performance for MegaZeux. When one of the drivers in this blacklist is
 * detected, video output will fall back to a different renderer instead.
 */
struct blacklist_entry
{
  const char *match_string; // Compare with GL_RENDERER output to detect driver.
  const char *reason;       // Reason for blacklisting. Displays to the user.
};

static struct blacklist_entry auto_glsl_blacklist[] =
{
  { "swrast",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "softpipe",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "llvmpipe",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "Software Rasterizer",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "Chromium",
    "  Chromium redirects GL commands to other GL drivers. The destination\n"
    "  often seems to be a software renderer, causing poor performance on\n"
    "  some machines.\n" },
  { "Intel EMGD",
    "  This driver may have extremely questionable \"OpenGL 2.0\" support.\n"
    "  It may output a wall of spurious/nonsensical warnings when compiling\n"
    "  the shaders, and it may claim that these shaders compiled\n"
    "  successfully in cases where they actually did not.\n" },
  { "GL4ES wrapper",
    "  GL4ES is an OpenGL to GLES translation library that may or may not\n"
    "  have problems with this renderer.\n" },
};

static int auto_glsl_blacklist_len = ARRAY_SIZE(auto_glsl_blacklist);

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
  void (GL_APIENTRY *glDeleteTextures)(GLsizei n, GLuint *textures);
  GLenum (GL_APIENTRY *glGetError)(void);
  void (GL_APIENTRY *glGetProgramInfoLog)(GLuint program, GLsizei bufsize,
   GLsizei *len, char *infolog);
  void (GL_APIENTRY *glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
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
  void (GL_APIENTRY *glDeleteShader)(GLuint shader);
  void (GL_APIENTRY *glDetachShader)(GLuint program, GLuint shader);
  void (GL_APIENTRY *glDeleteProgram)(GLuint program);
  void (GL_APIENTRY *glGetAttachedShaders)(GLuint program, GLsizei maxCount,
   GLsizei *count, GLuint *shaders);
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
  { "glDeleteProgram",            (fn_ptr *)&glsl.glDeleteProgram },
  { "glDeleteShader",             (fn_ptr *)&glsl.glDeleteShader },
  { "glDeleteTextures",           (fn_ptr *)&glsl.glDeleteTextures },
  { "glDetachShader",             (fn_ptr *)&glsl.glDetachShader },
  { "glDisable",                  (fn_ptr *)&glsl.glDisable },
  { "glDisableVertexAttribArray", (fn_ptr *)&glsl.glDisableVertexAttribArray },
  { "glDrawArrays",               (fn_ptr *)&glsl.glDrawArrays },
  { "glEnable",                   (fn_ptr *)&glsl.glEnable },
  { "glEnableVertexAttribArray",  (fn_ptr *)&glsl.glEnableVertexAttribArray },
  { "glGenTextures",              (fn_ptr *)&glsl.glGenTextures },
  { "glGetAttachedShaders",       (fn_ptr *)&glsl.glGetAttachedShaders },
  { "glGetError",                 (fn_ptr *)&glsl.glGetError },
  { "glGetProgramInfoLog",        (fn_ptr *)&glsl.glGetProgramInfoLog },
  { "glGetShaderInfoLog",         (fn_ptr *)&glsl.glGetShaderInfoLog },
  { "glGetShaderiv",              (fn_ptr *)&glsl.glGetShaderiv },
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
#ifdef CONFIG_SDL
  struct sdl_render_data sdl;
#endif
  Uint32 *pixels;
  Uint32 charset_texture[CHAR_H * FULL_CHARSET_SIZE * CHAR_W];
  Uint32 background_texture[BG_WIDTH * BG_HEIGHT];
  GLuint textures[NUM_TEXTURES];
  GLubyte palette[3 * FULL_PAL_SIZE];
  Uint8 remap_texture;
  Uint8 remap_char[FULL_CHARSET_SIZE];
  GLuint scaler_program;
  GLuint tilemap_program;
  GLuint tilemap_smzx_program;
  GLuint mouse_program;
  GLuint cursor_program;
  struct config_info *conf;
};

static char *source_cache[GLSL_SHADER_RES_COUNT];

#define INFO_MAX 1000

static GLint glsl_verify_compile(struct glsl_render_data *render_data,
 GLenum shader)
{
  GLint compile_status;
  char buffer[INFO_MAX];
  int len = 0;

  glsl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  glsl.glGetShaderInfoLog(shader, INFO_MAX - 1, &len, buffer);
  buffer[len] = 0;

  if(len > 0)
    warn("%s", buffer);

  if(compile_status == GL_FALSE)
    warn("Shader compilation failed!\n");

  return compile_status;
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

static GLuint glsl_load_shader(struct graphics_data *graphics,
 enum resource_id res, GLenum type)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int index = res - GLSL_SHADER_RES_FIRST;
  int loaded_config = 0;
  boolean is_user_scaler = false;
  GLint compile_status;
  GLenum shader;

  assert(res >= GLSL_SHADER_RES_FIRST && res <= GLSL_SHADER_RES_LAST);

  if(res == GLSL_SHADER_SCALER_VERT || res == GLSL_SHADER_SCALER_FRAG)
    if(graphics->gl_scaling_shader[0])
      is_user_scaler = true;

  // If we've already seen this shader, it's loaded, and we don't
  // need to do any more file I/O.

  if(!source_cache[index])
  {
    if(is_user_scaler)
    {
      // Try to load these from config before loading the default.

      char *path = cmalloc(MAX_PATH);

      strcpy(path, mzx_res_get_by_id(GLSL_SHADER_SCALER_DIRECTORY));

      switch(res)
      {
        case GLSL_SHADER_SCALER_VERT:
          sprintf(path + strlen(path), "%s%s.vert",
           DIR_SEPARATOR, graphics->gl_scaling_shader);
          break;

        case GLSL_SHADER_SCALER_FRAG:
          sprintf(path + strlen(path), "%s%s.frag",
           DIR_SEPARATOR, graphics->gl_scaling_shader);
          break;

        default:
          break;
      }

      source_cache[index] = glsl_load_string(path);

      if(source_cache[index])
      {
        loaded_config = 1;
      }
      else
      {
        debug("Failed to find shader '%s'; falling back to default\n", path);
        loaded_config = 0;
      }

      free(path);
    }

    if(!loaded_config)
    {
      is_user_scaler = false;

      if(res == GLSL_SHADER_SCALER_FRAG)
        graphics->gl_scaling_shader[0] = 0;

      source_cache[index] = glsl_load_string(mzx_res_get_by_id(res));
      if(!source_cache[index])
      {
        warn("Failed to find required shader '%s'\n", mzx_res_get_by_id(res));
        return 0;
      }
    }
  }

  shader = glsl.glCreateShader(type);

#ifdef CONFIG_GLES
  {
    /**
     * OpenGL ES really doesn't like '#version 110' being specified. This
     * is unfortunate, because some drivers (e.g. Intel HD) will emit warnings
     * if it isn't explicit. Comment these directives out if found.
     */

    char *pos = source_cache[index];

    while((pos = strstr(pos, "#version 110")))
    {
      debug("Found '#version 110' at %d in %s; commenting out\n",
       (int)(pos - source_cache[index]),
       mzx_res_get_by_id(res));
      pos[0] = '/';
      pos[1] = '/';
    }
  }
#endif // CONFIG_GLES

#ifdef CONFIG_GLES
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
    const char **source_ptr = (const char **)&source_cache[index];

    glsl.glShaderSource(shader, 1, source_ptr, &length);
  }
#endif

  glsl.glCompileShader(shader);
  compile_status = glsl_verify_compile(render_data, shader);

  if(compile_status == GL_FALSE)
  {
    if(is_user_scaler)
    {
      // Attempt the default shader
      graphics->gl_scaling_shader[0] = 0;
      free(source_cache[index]);
      source_cache[index] = NULL;

      return glsl_load_shader(graphics, res, type);
    }
    else
    {
      warn("Required shader '%s' failed to compile.\n", mzx_res_get_by_id(res));
      return 0;
    }
  }

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

static void glsl_delete_shaders(GLuint program)
{
  GLuint shaders[16];
  GLsizei shader_count, i;
  glsl.glGetAttachedShaders(program, 16, &shader_count, shaders);
  gl_check_error();
  for(i = 0; i < shader_count; i++)
  {
    glsl.glDetachShader(program, shaders[i]);
    gl_check_error();
    glsl.glDeleteShader(shaders[i]);
    gl_check_error();
  }
}

static void glsl_load_shaders(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;

  render_data->scaler_program = glsl_load_program(graphics,
   GLSL_SHADER_SCALER_VERT, GLSL_SHADER_SCALER_FRAG);
  if(render_data->scaler_program)
  {
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->scaler_program);
    glsl_verify_link(render_data, render_data->scaler_program);
    glsl_delete_shaders(render_data->scaler_program);
  }

  render_data->tilemap_program = glsl_load_program(graphics,
   GLSL_SHADER_TILEMAP_VERT, GLSL_SHADER_TILEMAP_FRAG);
  if(render_data->tilemap_program)
  {
    glsl.glBindAttribLocation(render_data->tilemap_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->tilemap_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->tilemap_program);
    glsl_verify_link(render_data, render_data->tilemap_program);
    glsl_delete_shaders(render_data->tilemap_program);
  }

  render_data->tilemap_smzx_program = glsl_load_program(graphics,
   GLSL_SHADER_TILEMAP_VERT, GLSL_SHADER_TILEMAP_SMZX_FRAG);
  if(render_data->tilemap_smzx_program)
  {
    glsl.glBindAttribLocation(render_data->tilemap_smzx_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->tilemap_smzx_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->tilemap_smzx_program);
    glsl_verify_link(render_data, render_data->tilemap_smzx_program);
    glsl_delete_shaders(render_data->tilemap_smzx_program);
  }

  render_data->mouse_program = glsl_load_program(graphics,
   GLSL_SHADER_MOUSE_VERT, GLSL_SHADER_MOUSE_FRAG);
  if(render_data->mouse_program)
  {
    glsl.glBindAttribLocation(render_data->mouse_program,
     ATTRIB_POSITION, "Position");
    glsl.glLinkProgram(render_data->mouse_program);
    glsl_verify_link(render_data, render_data->mouse_program);
    glsl_delete_shaders(render_data->mouse_program);
  }

  render_data->cursor_program  = glsl_load_program(graphics,
   GLSL_SHADER_CURSOR_VERT, GLSL_SHADER_CURSOR_FRAG);
  if(render_data->cursor_program)
  {
    glsl.glBindAttribLocation(render_data->cursor_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->cursor_program,
     ATTRIB_COLOR, "Color");
    glsl.glLinkProgram(render_data->cursor_program);
    glsl_verify_link(render_data, render_data->cursor_program);
    glsl_delete_shaders(render_data->cursor_program);
  }
}

static boolean glsl_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct glsl_render_data *render_data;

  render_data = cmalloc(sizeof(struct glsl_render_data));
  if(!render_data)
    return false;

  if(!GL_LoadLibrary(GL_LIB_PROGRAMMABLE))
    goto err_free;

  memset(render_data, 0, sizeof(struct glsl_render_data));
  graphics->render_data = render_data;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->gl_scaling_shader = conf->gl_scaling_shader;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  // Buffer for screen texture
  render_data->pixels = cmalloc(sizeof(Uint32) * GL_POWER_2_WIDTH *
   GL_POWER_2_HEIGHT);
  render_data->conf = conf;
  if(!render_data->pixels)
    goto err_free;

  if(!set_video_mode())
    goto err_free_pixels;

  return true;

err_free_pixels:
  free(render_data->pixels);
err_free:
  free(render_data);
  graphics->render_data = NULL;
  return false;
}

// FIXME free more
static void glsl_free_video(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int i;

  for(i = 0; i < GLSL_SHADER_RES_COUNT; i++)
  {
    if(source_cache[i])
    {
      free((void *)source_cache[i]);
      source_cache[i] = NULL;
    }
  }

  if(render_data)
  {
    glsl.glDeleteTextures(NUM_TEXTURES, render_data->textures);
    gl_check_error();

    glsl.glUseProgram(0);
    gl_check_error();

    gl_cleanup(graphics);
    free(render_data->pixels);
    free(render_data);
    graphics->render_data = NULL;
  }
}

static void glsl_remap_char_range(struct graphics_data *graphics, Uint16 first,
 Uint16 count)
{
  struct glsl_render_data *render_data = graphics->render_data;

  if(first + count > FULL_CHARSET_SIZE)
    count = FULL_CHARSET_SIZE - first;

  // FIXME arbitrary
  if(count <= 256)
    memset(render_data->remap_char + first, 1, count);

  else
    render_data->remap_texture = true;
}

static void glsl_resize_screen(struct graphics_data *graphics,
 int width, int height)
{
  struct glsl_render_data *render_data = graphics->render_data;

  glsl.glViewport(0, 0, width, height);
  gl_check_error();

  // Free any preexisting textures if they exist
  glsl.glDeleteTextures(NUM_TEXTURES, render_data->textures);
  gl_check_error();

  glsl.glGenTextures(NUM_TEXTURES, render_data->textures);
  gl_check_error();

  // Screen texture
  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_LINEAR, glsl.glTexParameterf);

  glsl.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  memset(render_data->pixels, 255,
   sizeof(Uint32) * GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);

  glsl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
   render_data->pixels);
  gl_check_error();

  // Data texture
  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, glsl.glTexParameterf);

  glsl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DATA_WIDTH, TEX_DATA_HEIGHT,
   0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  gl_check_error();

  glsl_remap_char_range(graphics, 0, FULL_CHARSET_SIZE);

  glsl_load_shaders(graphics);
}

static boolean glsl_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  gl_set_attributes(graphics);

  if(!gl_set_video_mode(graphics, width, height, depth, fullscreen, resize,
   gl_required_version))
    return false;

  gl_set_attributes(graphics);

  if(!gl_load_syms(glsl_syms_map))
  {
    gl_cleanup(graphics);
    return false;
  }

  // We need a specific version of OpenGL; desktop GL must be 2.0.
  // All OpenGL ES 2.0 implementations are supported, so don't do
  // the check with these configurations.
#ifndef CONFIG_GLES
  {
    static boolean initialized = false;

    if(!initialized)
    {
      const char *version;
      double version_float;

      version = (const char *)glsl.glGetString(GL_VERSION);
      if(!version)
      {
        warn("Could not load GL version string.\n");
        gl_cleanup(graphics);
        return false;
      }

      version_float = atof(version);
      if(version_float < 2.0)
      {
        warn("Need >= OpenGL 2.0, got OpenGL %.1f.\n", version_float);
        gl_cleanup(graphics);
        return false;
      }

      initialized = true;
    }
  }
#endif

  glsl_resize_screen(graphics, width, height);
  return true;
}

static boolean glsl_auto_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  struct glsl_render_data *render_data = graphics->render_data;
  const char *renderer;
  int i;

  if(glsl_set_video_mode(graphics, width, height, depth, fullscreen, resize))
  {
    // Driver blacklist. If the renderer is on the blacklist we want to fall
    // back on software as these renderers have disastrous GLSL performance.

    renderer = (const char *)glsl.glGetString(GL_RENDERER);

    // Print the full renderer string for reference.
    info("GL driver: %s\n\n", renderer);

    for(i = 0; i < auto_glsl_blacklist_len; i++)
    {
      if(strstr(renderer, auto_glsl_blacklist[i].match_string))
      {
        warn(
          "Detected blacklisted driver: \"%s\". Disabling glsl. Reason:\n\n"
          "%s\n\n"
          "Run again with \"video_output=glsl\" to force-enable glsl.\n\n",
          auto_glsl_blacklist[i].match_string,
          auto_glsl_blacklist[i].reason
        );
        gl_cleanup(graphics);
        return false;
      }
    }

    // Switch from auto_glsl to glsl now that we know it works.
    graphics->renderer.set_video_mode = glsl_set_video_mode;
    strcpy(render_data->conf->video_output, "glsl");

    return true;
  }
  return false;
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

  for(i = 0; i < CHARSET_ROWS; i++, c += -CHAR_H + CHARSET_COLS * CHAR_H)
    for(j = 0; j < CHAR_H; j++, c += -CHARSET_COLS * CHAR_H + 1)
      for(k = 0; k < CHARSET_COLS; k++, c += CHAR_H)
        p = glsl_char_bitmask_to_texture(c, p);

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_CHR_X,
   TEX_DATA_CHR_Y,
   CHARSET_COLS * CHAR_W,
   CHARSET_ROWS * CHAR_H,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->charset_texture);
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

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_CHR_X + (chr % CHARSET_COLS) * CHAR_W,
   TEX_DATA_CHR_Y + (chr / CHARSET_COLS) * CHAR_H,
   CHAR_W,
   CHAR_H,
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
    graphics->flat_intensity_palette[i] = (0xFF << 24) | (palette[i].b << 16) |
     (palette[i].g << 8) | palette[i].r;
    render_data->palette[i*3  ] = (GLubyte)palette[i].r;
    render_data->palette[i*3+1] = (GLubyte)palette[i].g;
    render_data->palette[i*3+2] = (GLubyte)palette[i].b;
  }
}

static void glsl_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct char_element *src = layer->data;
  Uint32 *colorptr, *dest, i, j;
  int width, height;
  Uint32 char_value, fg_color, bg_color;

  int x1 = layer->x;
  int x2 = layer->x + layer->w * CHAR_W;
  int y1 = layer->y;
  int y2 = layer->y + layer->h * CHAR_H;

  float v_left =   1.0f * x1 / SCREEN_PIX_W_F * 2.0f - 1.0f;
  float v_right =  1.0f * x2 / SCREEN_PIX_W_F * 2.0f - 1.0f;
  float v_top =    1.0f * y1 / SCREEN_PIX_H_F * 2.0f - 1.0f;
  float v_bottom = 1.0f * y2 / SCREEN_PIX_H_F * 2.0f - 1.0f;

  float vertex_array_single[2 * 4] =
  {
    v_left, -v_top,
    v_left, -v_bottom,
    v_right, -v_top,
    v_right, -v_bottom,
  };

  float tex_coord_array_single[2 * 4] =
  {
    0.0f,     0.0f,
    0.0f,     layer->h,
    layer->w, 0.0f,
    layer->w, layer->h,
  };

  // Clamp draw area to size of screen texture.
  get_context_width_height(graphics, &width, &height);
  if(width < SCREEN_PIX_W || height < SCREEN_PIX_H)
  {
    if(width >= GL_POWER_2_WIDTH)
      width = GL_POWER_2_WIDTH;

    if(height >= GL_POWER_2_HEIGHT)
      height = GL_POWER_2_HEIGHT;
  }
  else
  {
    width = SCREEN_PIX_W;
    height = SCREEN_PIX_H;
  }

  glsl.glViewport(0, 0, width, height);
  gl_check_error();

  if(layer->mode == 0)
    glsl.glUseProgram(render_data->tilemap_program);
  else
    glsl.glUseProgram(render_data->tilemap_smzx_program);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  if(render_data->remap_texture)
  {
    glsl_do_remap_charsets(graphics);
    render_data->remap_texture = false;
    memset(render_data->remap_char, false, sizeof(Uint8) * FULL_CHARSET_SIZE);
  }
  else
  {
    for(i = 0; i < FULL_CHARSET_SIZE; i++)
    {
      if(render_data->remap_char[i])
      {
        glsl_do_remap_char(graphics, i);
        render_data->remap_char[i] = false;
      }
    }
  }

  // Layer data
  dest = render_data->background_texture;

  for(i = 0; i < layer->w * layer->h; i++, dest++, src++)
  {
    char_value = src->char_value;
    bg_color = src->bg_color;
    fg_color = src->fg_color;

    if(char_value != INVISIBLE_CHAR)
    {
      if(char_value < PROTECTED_CHARSET_POSITION)
        char_value = (char_value + layer->offset) % PROTECTED_CHARSET_POSITION;

      if(bg_color >= 16)
        bg_color = (bg_color & 0xF) + graphics->protected_pal_position;

      if(fg_color >= 16)
        fg_color = (fg_color & 0xF) + graphics->protected_pal_position;
    }
    else
    {
      bg_color = FULL_PAL_SIZE;
      fg_color = FULL_PAL_SIZE;
    }

    *dest =
     (char_value << LAYER_CHAR_POS) |
     (bg_color << LAYER_BG_POS) |
     (fg_color << LAYER_FG_POS);
  }

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_LAYER_X, TEX_DATA_LAYER_Y, layer->w, layer->h,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->background_texture);
  gl_check_error();

  colorptr = graphics->flat_intensity_palette;
  dest = render_data->background_texture;

  for(i = 0; i < graphics->protected_pal_position + 16; i++, dest++, colorptr++)
    *dest = *colorptr;

  // Palette
  if(layer->transparent_col != -1)
    render_data->background_texture[layer->transparent_col] = 0x00000000;
  render_data->background_texture[FULL_PAL_SIZE] = 0x00000000;

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_PAL_X, TEX_DATA_PAL_Y, FULL_PAL_SIZE + 1, 1,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->background_texture);
  gl_check_error();

  // Indices
  dest = render_data->background_texture;
  for(i = 0; i < 4; i++)
    for(j = 0; j < SMZX_PAL_SIZE; j++, dest++)
      *dest = graphics->smzx_indices[j * 4 + i];

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_IDX_X, TEX_DATA_IDX_Y, SMZX_PAL_SIZE, 4,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->background_texture);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  glsl.glEnable(GL_BLEND);
  glsl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
  glsl.glDisable(GL_BLEND);
}

static void glsl_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  struct glsl_render_data *render_data = graphics->render_data;
  GLubyte *pal_base = &render_data->palette[color * 3];

  int x1 = x * 8;
  int x2 = x * 8 + 8;
  int y1 = y * 14 + offset;
  int y2 = y * 14 + offset + lines;

  const float vertex_array[2 * 4] =
  {
    x1 * 2.0f / SCREEN_PIX_W - 1.0f, (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x1 * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
  };

  const float color_array[4 * 4] =
  {
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
  };

  glsl.glDisable(GL_TEXTURE_2D);

  glsl.glUseProgram(render_data->cursor_program);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_COLOR);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array);
  gl_check_error();

  glsl.glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 0,
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

  int x2 = x + w;
  int y2 = y + h;

  const float vertex_array[2 * 4] =
  {
    x  * 2.0f / SCREEN_PIX_W - 1.0f, (y  * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x  * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y  * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
  };

  glsl.glEnable(GL_BLEND);
  glsl.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

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
  fix_viewport_ratio(width, height, &v_width, &v_height, graphics->ratio);
  glsl.glViewport((width - v_width) >> 1, (height - v_height) >> 1,
   v_width, v_height);

  // Clamp draw area to size of screen texture.
  if(width < SCREEN_PIX_W || height < SCREEN_PIX_H)
  {
    if(width >= GL_POWER_2_WIDTH)
      width = GL_POWER_2_WIDTH;

    if(height >= GL_POWER_2_HEIGHT)
      height = GL_POWER_2_HEIGHT;
  }
  else
  {
    width = SCREEN_PIX_W;
    height = SCREEN_PIX_H;
  }

  glsl.glUseProgram(render_data->scaler_program);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID]);
  gl_check_error();

  glsl.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0,
   GL_POWER_2_WIDTH, GL_POWER_2_HEIGHT, 0);
  gl_check_error();

  glsl.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array_single);
  gl_check_error();

  {
    const float tex_coord_array_single[2 * 4] =
    {
      0.0f,                              height / (1.0f * GL_POWER_2_HEIGHT),
      0.0f,                              0.0f,
      width / (1.0f * GL_POWER_2_WIDTH), height / (1.0f * GL_POWER_2_HEIGHT),
      width / (1.0f * GL_POWER_2_WIDTH), 0.0f,
    };

    glsl.glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0,
     tex_coord_array_single);
    gl_check_error();

    glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_check_error();
  }

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);
  glsl.glDisableVertexAttribArray(ATTRIB_TEXCOORD);

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  gl_swap_buffers(graphics);

#ifndef __EMSCRIPTEN__
  glsl.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();
#endif
}

static void glsl_switch_shader(struct graphics_data *graphics,
 const char *name)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int index = GLSL_SHADER_SCALER_VERT - GLSL_SHADER_RES_FIRST;

  if(source_cache[index]) free(source_cache[index]);
  source_cache[index] = NULL;

  index = GLSL_SHADER_SCALER_FRAG - GLSL_SHADER_RES_FIRST;
  if(source_cache[index]) free(source_cache[index]);
  source_cache[index] = NULL;

  strncpy(graphics->gl_scaling_shader, name, 32);
  graphics->gl_scaling_shader[31] = 0;

  glsl.glDeleteProgram(render_data->scaler_program);
  gl_check_error();

  render_data->scaler_program = glsl_load_program(graphics,
   GLSL_SHADER_SCALER_VERT, GLSL_SHADER_SCALER_FRAG);
  if(render_data->scaler_program)
  {
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->scaler_program);
    glsl_verify_link(render_data, render_data->scaler_program);
    glsl_delete_shaders(render_data->scaler_program);
  }
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
  renderer->remap_char_range = glsl_remap_char_range;
  renderer->remap_char = glsl_remap_char;
  renderer->remap_charbyte = glsl_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_layer = glsl_render_layer;
  renderer->render_cursor = glsl_render_cursor;
  renderer->render_mouse = glsl_render_mouse;
  renderer->sync_screen = glsl_sync_screen;
  renderer->switch_shader = glsl_switch_shader;
}

void render_auto_glsl_register(struct renderer *renderer)
{
  render_glsl_register(renderer);
  renderer->set_video_mode = glsl_auto_set_video_mode;
}
