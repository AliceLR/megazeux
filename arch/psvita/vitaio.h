/* MegaZeux
 *
 * Copyright (C) 2018-2020 Ian Burgmyer <spectere@gmail.com>
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

#ifndef __VITAIO_H
#define __VITAIO_H

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ROOT_PATH "ux0:/data/MegaZeux"

#ifdef __cplusplus
extern "C" {
#endif

int vitaio_chdir(const char *path);
FILE* vitaio_fopen(const char *pathname, const char *mode);
char* vitaio_getcwd(char *buf, size_t size);
int vitaio_mkdir(const char *path, mode_t mode);
DIR* vitaio_opendir(const char *name);
int vitaio_rmdir(const char *path);
int vitaio_stat(const char *path, struct stat *buf);

#ifdef __cplusplus
}
#endif

#ifndef __VITAIO_C
/* These functions cannot be defined if we're hitting this include file from
 * the implementation code. If we do, it won't be able to call the standard
 * POSIX functions when necessary. */
#define chdir(path) vitaio_chdir(path)
#define fopen(pathname, mode) vitaio_fopen(pathname, mode)
#define getcwd(buf, size) vitaio_getcwd(buf, size)
#define mkdir(path, mode) vitaio_mkdir(path, mode)
#define opendir(name) vitaio_opendir(name)
#define rmdir(path) vitaio_rmdir(path)
#define stat(path, buf) vitaio_stat(path, buf)

#endif /* __VITAIO_C */

#endif /* __VITAIO_H */
