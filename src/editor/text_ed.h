/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __EDITOR_TEXTEDIT_H
#define __EDITOR_TEXTEDIT_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../core.h"

enum text_edit_type
{
  TE_BOX,
  TE_DIALOG
};

void text_editor(context *parent, int x, int y, int w, int h,
 enum text_edit_type type, const char *title, const char *src, size_t src_len,
 void *p, void (*flush_cb)(struct world *, void *, char *, size_t));

void hex_editor(context *parent, int x, int y, int w, int h,
 const char *title, const void *src, size_t src_len, void *p,
 void (*flush_cb)(struct world *, void *, char *, size_t));

__M_END_DECLS

#endif /* __EDITOR_TEXTEDIT_H */
