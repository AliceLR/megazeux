/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __EDITOR_EDIT_EXPORT_H
#define __EDITOR_EDIT_EXPORT_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../core.h"

void export_board_image(context *parent, struct board *src_board,
 const char *file);
void export_vlayer_image(context *parent, struct world *mzx_world,
 const char *file);

__M_END_DECLS

#endif /* __EDITOR_EDIT_EXPORT_H */
