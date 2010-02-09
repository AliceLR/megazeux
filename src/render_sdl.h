/* MegaZeux
 *
 * Copyright (C) 2007-2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __RENDER_SDL_H
#define __RENDER_SDL_H

#include "compat.h"

__M_BEGIN_DECLS

#include "graphics.h"

#include "SDL.h"

int sdl_flags(int depth, bool fullscreen, bool resize);

#ifdef CONFIG_RENDER_GL

#include "render_gl.h"

#include "SDL_opengl.h"

#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

#define GL_STRIP_FLAGS(A) ((A & (SDL_FULLSCREEN | SDL_RESIZABLE)) | SDL_OPENGL)

/* GL_LOAD_SYM() should be used as follows:
 *
 * static int gl_load_syms (gl_syms *gl)
 * {
 *   if(gl->syms_loaded)
 *     return true;
 *
 *   GL_LOAD_SYM(gl, glBegin)
 *   GL_LOAD_SYM(gl, glBindTexture)
 *   GL_LOAD_SYM(gl, glEnd)
 *
 *   gl->syms_loaded = true;
 *   return true;
 * }
 *
 * GL_LOAD_SYM_EXT() can be used instead to always
 * dynamically resolve GL API functions, and must be used
 * for uncommon ARB extensions (see render_glsl.c)
 */

#define GL_LOAD_SYM_EXT(OBJ,FUNC)           \
  OBJ->FUNC = SDL_GL_GetProcAddress(#FUNC); \
  if(!OBJ->FUNC)                            \
    return false;                           \

#ifdef LINK_OPENGL

#include "GL/gl.h"

#define GL_LOAD_SYM(OBJ,FUNC)               \
  OBJ->FUNC = FUNC;                         \

#define GL_CAN_USE true

#else // !LINK_OPENGL

#define GL_LOAD_SYM GL_LOAD_SYM_EXT

#define GL_CAN_USE (SDL_GL_LoadLibrary(NULL) >= 0)

#endif // !LINK_OPENGL

bool gl_set_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize);
bool gl_check_video_mode(struct graphics_data *graphics, int width, int height,
 int depth, bool fullscreen, bool resize);
void gl_set_filter_method(const char *method,
 void (GLAPIENTRY *glTexParameteri_p)(GLenum target, GLenum pname, GLint param));
bool gl_swap_buffers(struct graphics_data *graphics);

#endif // CONFIG_RENDER_GL

__M_END_DECLS

#endif // __RENDER_SDL_H
