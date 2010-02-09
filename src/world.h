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

/* When making new versions, change the number below, and
 * change the number in the version mzx_strings to follow. From now on,
 * be sure that the last two characters are the hex equivilent of the
 * MINOR version number.
 */

/* READ THIS -- MAGIC INFO
 *
 * World files:
 *
 *  MZX - Ver 1.x MegaZeux
 *  MZ2 - Ver 2.x MegaZeux
 *  MZA - Ver 2.51S1 Megazeux          (fabricated as 0x0208)
 *  M\x02\x09 - 2.5.1spider2+
 *  M\x02\x3E - MZX 2.62.x
 *  M\x02\x41 - MZX 2.65
 *  M\x02\x44 - MZX 2.68
 *  M\x02\x45 - MZX 2.69
 *  M\x02\x46 - MZX 2.69b
 *  M\x02\x48 - MZX 2.69c
 *  M\x02\x49 - MZX 2.70
 *  M\x02\x50 - MZX 2.80 (non-DOS)
 *  M\x02\x51 - MZX 2.81
 *  M\x02\x52 - MZX 2.82
 *
 * Save files:
 *
 *  MZSV2 - Ver 2.x MegaZeux
 *  MZXSA - Ver 2.51S1 MegaZeux        (fabricated as 0x0208)
 *  MZS\x02\x09 - 2.5.1spider2+
 *  MZS\x02\x3E - MZX 2.62.x
 *  MZS\x02\x41 - MZX 2.65
 *  MZS\x02\x44 - MZX 2.68
 *  MZS\x02\x45 - MZX 2.69
 *  MZS\x02\x46 - MZX 2.69b
 *  MZS\x02\x48 - MZX 2.69c
 *  MZS\x02\x49 - MZX 2.70
 *  MZS\x02\x50 - MZX 2.80 (non-DOS)
 *  MZS\x02\x51 - MZX 2.81
 *  MZS\x02\x52 - MZX 2.82
 *
 * Board files follow a similar pattern to world files. Versions prior to
 * 2.51S1 failed to bump the magic number -- they are "MB2", which is the same
 * magic used back to 2.00. For versions greater than 2.51S1, they match
 * world file magic, but the first byte is always 0xFF (see board.c).
 *
 * As of 2.80+, all letters after the name denote bug fixes/minor additions
 * and do not have different save formats. If a save format change is
 * necessitated, a numerical change must be enacted.
 */

/* The magic stamp for worlds, boards or SAV files created with this version
 * of MegaZeux. If you introduce a counter or any other incompatible change,
 * such as altering semantics or actually changing the binary format, this
 * value MUST be bumped.
 */
#define WORLD_VERSION      0x0253

/* See the downver.c tool for more information. Please, if you bump the
 * WORLD_VERSION, always make sure this is updated with its previous
 * value. Therefore, users in a naive frenzy can always downgrade their
 * work to an older version (if it at all makes sense to do so).
 */
#define WORLD_VERSION_PREV 0x0252

CORE_LIBSPEC FILE *try_load_world(const char *file, bool savegame,
 int *version, char *name);
CORE_LIBSPEC int save_world(World *mzx_world, const char *file, int savegame,
 int faded);
CORE_LIBSPEC bool reload_world(World *mzx_world, const char *file, int *faded);
CORE_LIBSPEC void clear_world(World *mzx_world);
CORE_LIBSPEC void clear_global_data(World *mzx_world);
CORE_LIBSPEC void default_scroll_values(World *mzx_world);

CORE_LIBSPEC void add_ext(char *src, const char *ext);

bool reload_savegame(World *mzx_world, const char *file, int *faded);
bool reload_swap(World *mzx_world, const char *file, int *faded);

// Code to load multi-byte ints from little endian file

int fgetw(FILE *fp);
int fgetd(FILE *fp);
void fputw(int src, FILE *fp);
void fputd(int src, FILE *fp);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC void default_global_data(World *mzx_world);
CORE_LIBSPEC void optimize_null_boards(World *mzx_world);
CORE_LIBSPEC void set_update_done(World *mzx_world);
CORE_LIBSPEC int world_magic(const char magic_string[3]);
CORE_LIBSPEC extern char world_version_string[4];
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __WORLD_H
