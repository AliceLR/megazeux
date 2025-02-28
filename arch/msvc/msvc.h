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
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IXUSR
#define S_IXUSR _S_IEXEC
#endif
#ifndef S_IRGRP
#define S_IRGRP (S_IRUSR >> 3)
#endif
#ifndef S_IWGRP
#define S_IWGRP (S_IWUSR >> 3)
#endif
#ifndef S_IXGRP
#define S_IXGRP (S_IXUSR >> 3)
#endif
#ifndef S_IROTH
#define S_IROTH (S_IRGRP >> 3)
#endif
#ifndef S_IWOTH
#define S_IWOTH (S_IWGRP >> 3)
#endif
#ifndef S_IXOTH
#define S_IXOTH (S_IXGRP >> 3)
#endif
#ifndef S_IRWXU
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif
#ifndef S_IRWXG
#define S_IRWXG (S_IRWXU >> 3)
#endif
#ifndef S_IRWXO
#define S_IRWXO (S_IRWXG >> 3)
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
