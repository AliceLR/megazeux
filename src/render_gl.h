/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
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
*/

#ifndef __RENDER_GL_H
#define __RENDER_GL_H

#include "compat.h"
#include "graphics.h"

#include "SDL.h"
#include "SDL_opengl.h"

#define GL_NON_POWER_2_WIDTH      640
#define GL_NON_POWER_2_HEIGHT     350
#define GL_POWER_2_WIDTH          1024
#define GL_POWER_2_HEIGHT         512

#define CONFIG_GL_FILTER_LINEAR   "linear"
#define CONFIG_GL_FILTER_NEAREST  "nearest"

#ifdef LINK_OPENGL

#include "GL/gl.h"

#define GL_LOAD_SYM(OBJ,FUNC)               \
  OBJ->FUNC = FUNC;                         \

#define GL_CAN_USE true

#else // !LINK_OPENGL

#define GL_LOAD_SYM(OBJ,FUNC)               \
  OBJ->FUNC = SDL_GL_GetProcAddress(#FUNC); \
  if(!OBJ->FUNC)                            \
    return false;                           \

#define GL_CAN_USE (SDL_GL_LoadLibrary(NULL) >= 0)

#endif

#define GL_STRIP_FLAGS(A) ((A & (SDL_FULLSCREEN | SDL_RESIZABLE)) | SDL_OPENGL)

__M_BEGIN_DECLS

int gl_check_video_mode(graphics_data *graphics, int width, int height,
 int depth, int fullscreen, int resize);
void gl_set_filter_method(const char *method,
 void (APIENTRY *glTexParameteri_p)(GLenum target, GLenum pname, GLint param));
void gl_set_attributes(graphics_data *graphics);

__M_END_DECLS

#endif // __RENDER_GL_H
