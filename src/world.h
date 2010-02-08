/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __WORLD_H
#define __WORLD_H

#include "compat.h"

__M_BEGIN_DECLS

#include "world_struct.h"

#include <stdio.h>

#include "board.h"
#include "const.h"
#include "robot.h"
#include "counter.h"
#include "sprite.h"
#include "sfx.h"
#include "configure.h"

#define WORLD_VERSION 0x251

int save_world(World *mzx_world, char *file, int savegame, int faded);
int append_world(World *mzx_world, char *file);
int reload_world(World *mzx_world, char *file, int *faded);
int reload_savegame(World *mzx_world, char *file, int *faded);
int reload_swap(World *mzx_world, char *file, int *faded);
void clear_world(World *mzx_world);
void clear_global_data(World *mzx_world);
void default_scroll_values(World *mzx_world);
void create_blank_world(World *mzx_world);

// Code to load multi-byte ints from little endian file

int fgetw(FILE *fp);
int fgetd(FILE *fp);
void fputw(int src, FILE *fp);
void fputd(int src, FILE *fp);
void add_ext(char *src, char *ext);
void get_path(char *file_name, char *dest);

#ifdef CONFIG_EDITOR
void optimize_null_boards(World *mzx_world);
void set_update_done_current(World *mzx_world);
extern char world_version_string[4];
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __WORLD_H
