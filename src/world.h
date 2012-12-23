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
 * change the number in the version strings to follow. From now on,
 * be sure that the last two characters are the hex equivalent of the
 * MINOR version number.
 */

/* READ THIS -- MAGIC INFO
 *
 * World files:
 *
 *  MZX - Ver 1.x MegaZeux             (mzx_world->version == 0x0100)
 *  MZ2 - Ver 2.x MegaZeux             (mzx_world->version == 0x0205)
 *  MZA - Ver 2.51S1 Megazeux          (mzx_world->version == 0x0208)
 *  M\x02\x09 - 2.5.1spider2+
 *  M\x02\x32 - MZX 2.62
 *  M\x02\x3E - MZX 2.62b
 *  M\x02\x41 - MZX 2.65
 *  M\x02\x44 - MZX 2.68
 *  M\x02\x45 - MZX 2.69
 *  M\x02\x46 - MZX 2.69b
 *  M\x02\x48 - MZX 2.69c
 *  M\x02\x49 - MZX 2.70
 *  M\x02\x50 - MZX 2.80 (non-DOS)
 *  M\x02\x51 - MZX 2.81
 *  M\x02\x52 - MZX 2.82
 *  M\x02\x53 - MZX 2.83
 *  M\x02\x54 - MZX 2.84
 *
 * Save files:
 *
 *  MZSV2 - Ver 2.x MegaZeux           (mzx_world->version == 0x0205)
 *  MZXSA - Ver 2.51S1 MegaZeux        (mzx_world->version == 0x0208)
 *  MZS\x02\x09 - 2.5.1spider2+
 *  MZS\x02\x32 - MZX 2.62
 *  MZS\x02\x3E - MZX 2.62b
 *  MZS\x02\x41 - MZX 2.65
 *  MZS\x02\x44 - MZX 2.68
 *  MZS\x02\x45 - MZX 2.69
 *  MZS\x02\x46 - MZX 2.69b
 *  MZS\x02\x48 - MZX 2.69c
 *  MZS\x02\x49 - MZX 2.70
 *  MZS\x02\x50 - MZX 2.80 (non-DOS)
 *  MZS\x02\x51 - MZX 2.81
 *  MZS\x02\x52 - MZX 2.82
 *  MZS\x02\x53 - MZX 2.83
 *  MZS\x02\x54 - MZX 2.84
 *
 * Board files follow a similar pattern to world files. Versions prior to
 * 2.51S1 are "MB2". For versions greater than 2.51S1, they match the
 * world file magic. For all boards, the first byte is always 0xFF.
 *
 * As of 2.80+, all letters after the name denote bug fixes/minor additions
 * and may have different save formats. If a save format change is
 * necessitated, a numerical change must be enacted.
 */

/* The magic stamp for worlds, boards or SAV files created with this version
 * of MegaZeux. If you introduce a counter or any other incompatible change,
 * such as altering semantics or actually changing the binary format, this
 * value MUST be bumped.
 */
#define WORLD_VERSION      0x0254

/* See the downver.c tool for more information. Please, if you bump the
 * WORLD_VERSION, always make sure this is updated with its previous
 * value. Therefore, users can always downgrade their work to an
 * older version (if it at all makes sense to do so).
 */
#define WORLD_VERSION_PREV 0x0253

// FIXME: hack
#ifdef CONFIG_DEBYTECODE
#undef  WORLD_VERSION
#define WORLD_VERSION      0x025A
#undef  WORLD_VERSION_PREV
#endif

// These three are needed by validation.c until I figure out a better way
CORE_LIBSPEC int world_magic(const char magic_string[3]);
CORE_LIBSPEC int save_magic(const char magic_string[5]);

CORE_LIBSPEC int save_world(struct world *mzx_world, const char *file,
 int savegame);
CORE_LIBSPEC bool reload_world(struct world *mzx_world, const char *file,
 int *faded);
CORE_LIBSPEC void clear_world(struct world *mzx_world);
CORE_LIBSPEC void clear_global_data(struct world *mzx_world);
CORE_LIBSPEC void default_scroll_values(struct world *mzx_world);

CORE_LIBSPEC void add_ext(char *src, const char *ext);

bool reload_savegame(struct world *mzx_world, const char *file, int *faded);
bool reload_swap(struct world *mzx_world, const char *file, int *faded);

// Code to load multi-byte ints from little endian file

int fgetw(FILE *fp);
int fgetd(FILE *fp);
void fputw(int src, FILE *fp);
void fputd(int src, FILE *fp);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC FILE *try_load_world(const char *file, bool savegame,
 int *version, char *name);
CORE_LIBSPEC void default_global_data(struct world *mzx_world);
CORE_LIBSPEC void set_update_done(struct world *mzx_world);

CORE_LIBSPEC void refactor_board_list(struct world *mzx_world,
 struct board **new_board_list, int new_list_size,
 int *board_id_translation_list);
CORE_LIBSPEC void optimize_null_boards(struct world *mzx_world);
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __WORLD_H
