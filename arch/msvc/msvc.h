/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Ian Burgmyer <spectere@gmail.com>
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

// all MSVC specific hacks should go here, if possible

#ifndef MSVC_H
#define MSVC_H

#include <io.h>
#include <ctype.h>
#include <direct.h>
#include <process.h>

// unistd.h mode defines (required for access calls)
#define X_OK   0
#define W_OK   2
#define R_OK   4

#ifndef S_ISREG
#define S_ISREG(mode) (mode & _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) (mode & _S_IFDIR)
#endif

#define access      _access
#define chdir       _chdir
#define execv       _execv
#define getcwd      _getcwd
#define rmdir       _rmdir

#define unlink      _unlink

#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#define mkdir(name,mode) _mkdir(name)

#define inline __inline

#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef _W64 int ssize_t;
#endif

#endif // MSVC_H
