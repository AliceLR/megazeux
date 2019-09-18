/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __VFS_H
#define __VFS_H

#include "compat.h"

__M_BEGIN_DECLS

#include "memfile.h"

#include <stdio.h>

typedef struct vfile vfile;
struct stat;

vfile *vfopen_unsafe(const char *filename, const char *mode);
vfile *vfile_init_fp(FILE *fp, const char *mode);
vfile *vfile_init_mem(void *buffer, size_t size, const char *mode);
vfile *vfile_init_mem_ext(void **external_buffer, size_t *external_buffer_size,
 const char *mode);
int vfclose(vfile *vf);

struct memfile *vfile_get_memfile(vfile *vf);

int vchdir(const char *path);
char *vgetcwd(char *buf, size_t size);
int vunlink(const char *path);
int vstat(const char *path, struct stat *buf);

int vfgetc(vfile *vf);
int vfgetw(vfile *vf);
int vfgetd(vfile *vf);
int vfputc(int character, vfile *vf);
int vfputw(int character, vfile *vf);
int vfputd(int character, vfile *vf);
int vfread(void *dest, size_t size, size_t count, vfile *vf);
int vfwrite(const void *src, size_t size, size_t count, vfile *vf);
int vfseek(vfile *vf, long int offset, int whence);
long int vftell(vfile *vf);
void vrewind(vfile *vf);

long vftell_and_rewind(vfile *vf);

// FIXME dirent

__M_END_DECLS

#endif /* VFS_H */
