/* MegaZeux
 *
 * Copyright (C) 2007 Joel Bouchard Lamontagne <logicow@gmail.com>
 * Copyright (C) 2007,2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

// We need to keep two versions of each char: the regular char, and an
// inverted version for foreground transparency with layer rendering.
#define CHAR_2W (CHAR_W * 2)

// The char texture in this renderer will be 1024x1024
#define CHARSET_COLS 64
#define CHARSET_ROWS (FULL_CHARSET_SIZE / CHARSET_COLS)
#define TEX_BG_WIDTH 128
#define TEX_BG_HEIGHT 32

enum
{
  TEX_SCREEN_ID,
  TEX_DATA_ID,
  TEX_BG_ID,
  NUM_TEXTURES
};

static inline int next_power_of_two(int v)
{
  int i;
  for(i = 1; i <= 65536; i *= 2)
    if(i >= v)
      return i;

  debug("Couldn't find power of two for %d\n", v);
  return v;
}

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
  void (GL_APIENTRY *glDeleteTextures)(GLsizei n, GLuint *textures);
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
  { "glDeleteTextures",     (fn_ptr *)&gl2.glDeleteTextures },
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
#ifdef CONFIG_SDL
  struct sdl_render_data sdl;
#endif
  uint32_t *pixels;
  uint8_t charset_texture[CHAR_2W * CHAR_H * FULL_CHARSET_SIZE];
  uint32_t background_texture[TEX_BG_WIDTH * TEX_BG_HEIGHT];
  GLuint textures[NUM_TEXTURES];
  GLubyte palette[3 * FULL_PAL_SIZE];
  boolean remap_texture;
  boolean remap_char[FULL_CHARSET_SIZE];
  boolean ignore_linear;
  GLubyte color_array[TEX_BG_WIDTH * TEX_BG_HEIGHT * 4 * 4];
  float tex_coord_array[TEX_BG_WIDTH * TEX_BG_HEIGHT * 8];
  float vertex_array[TEX_BG_WIDTH * TEX_BG_HEIGHT * 8];
  float charset_texture_width;
  float charset_texture_height;
  boolean viewport_shrunk;
};

static const GLubyte color_array_white[4 * 4] =
{
  255, 255, 255, 255,
  255, 255, 255, 255,
  255, 255, 255, 255,
  255, 255, 255, 255
};

static boolean gl2_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct gl2_render_data *render_data = cmalloc(sizeof(struct gl2_render_data));

  if(!render_data)
    goto err_out;

  if(!GL_LoadLibrary(GL_LIB_FIXED))
    goto err_free_render_data;

  memset(render_data, 0, sizeof(struct gl2_render_data));
  graphics->render_data = render_data;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  // We want to deal internally with 32bit surfaces
  render_data->pixels = cmalloc(sizeof(uint32_t) * GL_POWER_2_WIDTH *
   GL_POWER_2_HEIGHT);

  if(!render_data->pixels)
    goto err_free_render_data;

  if(!set_video_mode())
    goto err_free_pixels;

  return true;

err_free_pixels:
  free(render_data->pixels);
err_free_render_data:
  free(render_data);
  graphics->render_data = NULL;
err_out:
  return false;
}

static void gl2_free_video(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;
  gl2.glDeleteTextures(NUM_TEXTURES, render_data->textures);
  gl_check_error();

  gl_cleanup(graphics);

  free(render_data->pixels);
  free(render_data);
  graphics->render_data = NULL;
}

static void gl2_remap_char_range(struct graphics_data *graphics, uint16_t first,
 uint16_t count)
{
  struct gl2_render_data *render_data = graphics->render_data;

  if(first + count > FULL_CHARSET_SIZE)
    count = FULL_CHARSET_SIZE - first;

  // FIXME arbitrary
  if(count <= 256)
    memset(render_data->remap_char + first, 1, count);

  else
    render_data->remap_texture = true;
}

static void gl2_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  struct gl2_render_data *render_data = graphics->render_data;
  render_data->remap_char[chr] = true;
}

static void gl2_remap_charbyte(struct graphics_data *graphics,
 uint16_t chr, uint8_t byte)
{
  gl2_remap_char(graphics, chr);
}

static void gl2_resize_screen(struct graphics_data *graphics,
 int width, int height)
{
  struct gl2_render_data *render_data = graphics->render_data;
  int v_width, v_height;
  int charset_width, charset_height;

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

  fix_viewport_ratio(width, height, &v_width, &v_height, graphics->ratio);
  gl2.glViewport((width - v_width) >> 1, (height - v_height) >> 1,
   v_width, v_height);
  gl_check_error();
  render_data->viewport_shrunk = false;

  // Free any preexisting textures if they exist
  gl2.glDeleteTextures(NUM_TEXTURES, render_data->textures);
  gl_check_error();

  gl2.glGenTextures(NUM_TEXTURES, render_data->textures);
  gl_check_error();

  gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID]);
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
   sizeof(uint32_t) * GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);

  gl2.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
   render_data->pixels);
  gl_check_error();

  gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl2.glTexParameterf);
  charset_width = next_power_of_two(CHARSET_COLS * CHAR_2W);
  charset_height = next_power_of_two((CHARSET_ROWS + 1) * CHAR_H);
  render_data->charset_texture_width = charset_width;
  render_data->charset_texture_height = charset_height;
  gl2.glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, charset_width, charset_height,
   0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL);
  gl_check_error();

  gl2_remap_char_range(graphics, 0, FULL_CHARSET_SIZE);

  gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_BG_ID]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl2.glTexParameterf);

  gl2.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_BG_WIDTH, TEX_BG_HEIGHT,
   0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  gl_check_error();
}

static boolean gl2_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  gl_set_attributes(graphics);

  if(!gl_set_video_mode(graphics, width, height, depth, fullscreen, resize,
   gl_required_version))
    return false;

  gl_set_attributes(graphics);

  if(!gl_load_syms(gl2_syms_map))
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

      version = (const char *)gl2.glGetString(GL_VERSION);
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

  gl2_resize_screen(graphics, width, height);
  return true;
}

static void gl2_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct gl2_render_data *render_data = graphics->render_data;
  unsigned int i;
  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] = gl_pack_u32((0xFF << 24) |
     (palette[i].b << 16) | (palette[i].g << 8) | palette[i].r);
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
  signed char inv;

  for(i = 0; i < CHARSET_ROWS; i++, c += -CHAR_H + CHARSET_COLS * CHAR_H)
  {
    for(j = 0; j < CHAR_H; j++, c += -CHARSET_COLS * CHAR_H + 1)
    {
      for(k = 0; k < CHARSET_COLS; k++, c += CHAR_H)
      {
        inv = ~(*c);
        p = gl2_char_bitmask_to_texture(c, p);
        p = gl2_char_bitmask_to_texture(&inv, p);
      }
    }
  }

  gl2.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
   CHARSET_COLS * CHAR_2W,
   CHARSET_ROWS * CHAR_H,
   GL_ALPHA, GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static inline void gl2_do_remap_char(struct graphics_data *graphics,
 uint16_t chr)
{
  struct gl2_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  char *p = (char *)render_data->charset_texture;
  unsigned int i;
  signed char inv;

  if(chr < FULL_CHARSET_SIZE)
  {
    c += chr * 14;

    for(i = 0; i < 14; i++, c++)
    {
      inv = ~(*c);
      p = gl2_char_bitmask_to_texture(c, p);
      p = gl2_char_bitmask_to_texture(&inv, p);
    }
  }
  else
  {
    memset(render_data->charset_texture, 0, CHAR_2W * CHAR_H);
  }

  gl2.glTexSubImage2D(GL_TEXTURE_2D, 0,
   chr % CHARSET_COLS * CHAR_2W,
   chr / CHARSET_COLS * CHAR_H,
   CHAR_2W,
   CHAR_H,
   GL_ALPHA, GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static void gl2_check_remap_chars(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;
  int i;

  gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  if(render_data->remap_texture)
  {
    gl2_do_remap_charsets(graphics);

    // Also remap the invisible char while we're at it.
    gl2_do_remap_char(graphics, FULL_CHARSET_SIZE);

    render_data->remap_texture = false;
    memset(render_data->remap_char, false, sizeof(uint8_t) * FULL_CHARSET_SIZE);
  }
  else
  {
    for(i = 0; i < FULL_CHARSET_SIZE; i++)
    {
      if(render_data->remap_char[i])
      {
        gl2_do_remap_char(graphics, i);
        render_data->remap_char[i] = false;
      }
    }
  }
}

static int gl2_linear_filter_method(struct graphics_data *graphics)
{
  struct gl2_render_data *render_data = graphics->render_data;

  if(render_data->ignore_linear)
    return false;
  return graphics->gl_filter_method == CONFIG_GL_FILTER_LINEAR;
}

static inline int translate_layer_color(struct graphics_data *graphics,
 uint8_t color)
{
  if(color >= 16)
    return (color & 0xF) + graphics->protected_pal_position;

  return color;
}

static inline uint16_t translate_layer_char(uint16_t chr, uint16_t offset)
{
  if(chr == INVISIBLE_CHAR)
    return FULL_CHARSET_SIZE;

  if(chr > 0xFF)
    return (chr & 0xFF) + PROTECTED_CHARSET_POSITION;

  return (chr + offset) % PROTECTED_CHARSET_POSITION;
}

static void gl2_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct gl2_render_data *render_data = graphics->render_data;
  struct char_element *src = layer->data;
  int i, i2, i3, layer_w = layer->w, layer_h = layer->h;
  uint32_t *dest;
  uint16_t char_value;
  int fg_color, bg_color;

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
    render_data->viewport_shrunk = true;
  }

  if(!layer->mode)
  {
    float *tex_coord_array = render_data->tex_coord_array;
    float *vertex_array    = render_data->vertex_array;
    GLubyte *color_array   = render_data->color_array;
    int charset_tex_w      = render_data->charset_texture_width;
    int charset_tex_h      = render_data->charset_texture_height;
    int charset_x;
    int charset_y;
    float x1 = layer->x;
    float y1 = layer->y;
    float x2 = layer->x + layer->w * CHAR_W;
    float y2 = layer->y + layer->h * CHAR_H;

    float tex_coord_array_single[2 * 4] =
    {
      0.0f,                           0.0f,
      0.0f,                           1.0f * layer_h / TEX_BG_HEIGHT,
      1.0f * layer_w / TEX_BG_WIDTH,  0.0f,
      1.0f * layer_w / TEX_BG_WIDTH,  1.0f * layer_h / TEX_BG_HEIGHT
    };

    float vertex_array_single[2 * 4] =
    {
      x1 / SCREEN_PIX_W * 2.0f - 1.0f,
      (y1 / SCREEN_PIX_H * 2.0f - 1.0f) * -1.0f,

      x1 / SCREEN_PIX_W * 2.0f - 1.0f,
      (y2 / SCREEN_PIX_H * 2.0f - 1.0f) * -1.0f,

      x2 / SCREEN_PIX_W * 2.0f - 1.0f,
      (y1 / SCREEN_PIX_H * 2.0f - 1.0f) * -1.0f,

      x2 / SCREEN_PIX_W * 2.0f - 1.0f,
      (y2 / SCREEN_PIX_H * 2.0f - 1.0f) * -1.0f,
    };

    // Charsets
    gl2_check_remap_chars(graphics);

    // Background
    dest = render_data->background_texture;

    for(i = 0; i < layer_w * layer_h; i++, dest++, src++)
    {
      fg_color = translate_layer_color(graphics, src->fg_color);
      bg_color = translate_layer_color(graphics, src->bg_color);

      if(src->char_value != INVISIBLE_CHAR &&
       bg_color != layer->transparent_col && fg_color != layer->transparent_col)
        *dest = graphics->flat_intensity_palette[bg_color];
      else
        *dest = 0x00000000;
    }

    gl2.glEnable(GL_ALPHA_TEST);

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_BG_ID]);
    gl_check_error();

    gl2.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, layer_w, layer_h, GL_RGBA,
     GL_UNSIGNED_BYTE, render_data->background_texture);
    gl_check_error();

    // Draw background
    gl2.glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_array_single);
    gl_check_error();

    gl2.glVertexPointer(2, GL_FLOAT, 0, vertex_array_single);
    gl_check_error();

    gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_array_white);
    gl_check_error();

    gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_check_error();

    // Draw screen
    gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
    gl_check_error();

    src = layer->data;

    gl2.glTexCoordPointer(2, GL_FLOAT, 0, render_data->tex_coord_array);
    gl_check_error();

    gl2.glVertexPointer(2, GL_FLOAT, 0, render_data->vertex_array);
    gl_check_error();

    gl2.glColorPointer(4, GL_UNSIGNED_BYTE, 0, render_data->color_array);
    gl_check_error();

    i = 0;
    while(true)
    {
      GLubyte *pal_base = NULL;

      for(i2 = 0; i2 < layer_w; i2++)
      {
        bg_color = translate_layer_color(graphics, src->bg_color);
        fg_color = translate_layer_color(graphics, src->fg_color);
        char_value = translate_layer_char(src->char_value, layer->offset);

        // Multiply X by 2 as there are normal and inverted copies of each char
        charset_x = char_value % CHARSET_COLS * 2;
        charset_y = char_value / CHARSET_COLS;

        tex_coord_array[0] = (charset_x + 0.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[1] = (charset_y + 1.0f) * CHAR_H / charset_tex_h;

        tex_coord_array[2] = (charset_x + 0.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[3] = (charset_y + 0.0f) * CHAR_H / charset_tex_h;

        tex_coord_array[4] = (charset_x + 1.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[5] = (charset_y + 1.0f) * CHAR_H / charset_tex_h;

        tex_coord_array[6] = (charset_x + 1.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[7] = (charset_y + 0.0f) * CHAR_H / charset_tex_h;

        x1 = (i2 + 0.0f) * CHAR_W + layer->x;
        x2 = (i2 + 1.0f) * CHAR_W + layer->x;
        y1 = (i  + 0.0f) * CHAR_H + layer->y;
        y2 = (i  + 1.0f) * CHAR_H + layer->y;

        vertex_array[0] =  x1 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[1] = (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        vertex_array[2] =  x1 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[3] = (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        vertex_array[4] =  x2 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[5] = (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        vertex_array[6] =  x2 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[7] = (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        if(fg_color != layer->transparent_col)
        {
          pal_base = &render_data->palette[fg_color * 3];
        }
        else

        if(bg_color != layer->transparent_col)
        {
          pal_base = &render_data->palette[bg_color * 3];

          // Offset to the inverted copy of the current char
          tex_coord_array[0] += (CHAR_W * 1.0f) / charset_tex_w;
          tex_coord_array[2] += (CHAR_W * 1.0f) / charset_tex_w;
          tex_coord_array[4] += (CHAR_W * 1.0f) / charset_tex_w;
          tex_coord_array[6] += (CHAR_W * 1.0f) / charset_tex_w;
        }
        else
        {
          pal_base = NULL;
        }

        if(pal_base)
        {
          for(i3 = 0; i3 < 4; i3++)
          {
            memcpy(&color_array[i3 * 4], pal_base, 3);
            color_array[i3 * 4 + 3] = 255;
          }
        }
        else
        {
          for(i3 = 0; i3 < 4; i3++)
          {
            color_array[i3 * 4 + 3] = 0;
          }
        }

        tex_coord_array += 8;
        vertex_array += 8;
        color_array += 4 * 4;

        src++;
      }

      if(++i == layer_h)
        break;
      src += layer_w;

      // Draw this row backwards, so we can keep the strip contiguous

      for(i2 = layer_w - 1; i2 >= 0; i2--)
      {
        src--;
        bg_color = translate_layer_color(graphics, src->bg_color);
        fg_color = translate_layer_color(graphics, src->fg_color);
        char_value = translate_layer_char(src->char_value, layer->offset);

        // Multiply X by 2 as there are normal and inverted copies of each char
        charset_x = char_value % CHARSET_COLS * 2;
        charset_y = char_value / CHARSET_COLS;

        tex_coord_array[0] = (charset_x + 1.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[1] = (charset_y + 1.0f) * CHAR_H / charset_tex_h;

        tex_coord_array[2] = (charset_x + 1.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[3] = (charset_y + 0.0f) * CHAR_H / charset_tex_h;

        tex_coord_array[4] = (charset_x + 0.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[5] = (charset_y + 1.0f) * CHAR_H / charset_tex_h;

        tex_coord_array[6] = (charset_x + 0.0f) * CHAR_W / charset_tex_w;
        tex_coord_array[7] = (charset_y + 0.0f) * CHAR_H / charset_tex_h;

        x1 = (i2 + 0.0f) * CHAR_W + layer->x;
        x2 = (i2 + 1.0f) * CHAR_W + layer->x;
        y1 = (i  + 0.0f) * CHAR_H + layer->y;
        y2 = (i  + 1.0f) * CHAR_H + layer->y;

        vertex_array[0] =  x2 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[1] = (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        vertex_array[2] =  x2 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[3] = (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        vertex_array[4] =  x1 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[5] = (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        vertex_array[6] =  x1 * 2.0f / SCREEN_PIX_W - 1.0f;
        vertex_array[7] = (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f;

        if(fg_color != layer->transparent_col)
        {
          pal_base = &render_data->palette[fg_color * 3];
        }
        else

        if(bg_color != layer->transparent_col)
        {
          pal_base = &render_data->palette[bg_color * 3];

          // Offset to the inverted copy of the current char
          tex_coord_array[0] += (CHAR_W * 1.0f) / charset_tex_w;
          tex_coord_array[2] += (CHAR_W * 1.0f) / charset_tex_w;
          tex_coord_array[4] += (CHAR_W * 1.0f) / charset_tex_w;
          tex_coord_array[6] += (CHAR_W * 1.0f) / charset_tex_w;
        }
        else
        {
          pal_base = NULL;
        }

        if(pal_base)
        {
          for(i3 = 0; i3 < 4; i3++)
          {
            memcpy(&color_array[i3 * 4], pal_base, 3);
            color_array[i3 * 4 + 3] = 255;
          }
        }
        else
        {
          for(i3 = 0; i3 < 4; i3++)
          {
            color_array[i3 * 4 + 3] = 0;
          }
        }

        tex_coord_array += 8;
        vertex_array += 8;
        color_array += 4 * 4;
      }

      if(++i == layer_h)
        break;
      src += layer_w;
    }

    gl2.glDrawArrays(GL_TRIANGLE_STRIP, 0, layer_w * layer_h * 4);
    gl_check_error();

    gl2.glDisable(GL_ALPHA_TEST);
  }
  else
  {
    // SMZX mode- use software layer renderer
    static const float tex_coord_array_single[2 * 4] =
    {
      0.0f,             0.0f,
      0.0f,             350.0f / 512.0f,
      640.0f / 1024.0f, 0.0f,
      640.0f / 1024.0f, 350.0f / 512.0f,
    };

    render_layer(render_data->pixels, 32, 640 * 4, graphics, layer);

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID]);
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

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
    gl_check_error();
  }
}

static void gl2_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct gl2_render_data *render_data = graphics->render_data;
  GLubyte *pal_base = &render_data->palette[color * 3];

  const float vertex_array[2 * 4] =
  {
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + lines + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + lines + offset)*-2.0f/350.0f+1.0f
  };

  const GLubyte color_array[4 * 4] =
  {
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
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  const float vertex_array[2 * 4] =
  {
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

  if(render_data->viewport_shrunk)
  {
    int v_width, v_height;
    int width, height;

    static const float tex_coord_array_single[2 * 4] =
    {
       0.0f,             350.0f / 512.0f,
       0.0f,             0.0f,
       640.0f / 1024.0f, 350.0f / 512.0f,
       640.0f / 1024.0f, 0.0f,
    };

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID]);
    gl_check_error();

    get_context_width_height(graphics, &width, &height);
    gl2.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
     (width - 640) >> 1, (height - 350) >> 1, 1024, 512, 0);
    gl_check_error();

    fix_viewport_ratio(width, height, &v_width, &v_height, graphics->ratio);
    gl2.glViewport((width - v_width) >> 1, (height - v_height) >> 1,
     v_width, v_height);
    gl_check_error();
    render_data->viewport_shrunk = false;

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

    gl2.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
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
  renderer->set_video_mode = gl2_set_video_mode;
  renderer->update_colors = gl2_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_char_range = gl2_remap_char_range;
  renderer->remap_char = gl2_remap_char;
  renderer->remap_charbyte = gl2_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_layer = gl2_render_layer;
  renderer->render_cursor = gl2_render_cursor;
  renderer->render_mouse = gl2_render_mouse;
  renderer->sync_screen = gl2_sync_screen;
}
