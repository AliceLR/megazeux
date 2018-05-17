/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __MANIFEST_H
#define __MANIFEST_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../platform.h"
#include "sha256.h"
#include "host.h"

#define MANIFEST_TXT "manifest.txt"

struct manifest_entry
{
  struct manifest_entry *next;
  Uint32 sha256[8];
  unsigned long size;
  char *name;
};

UPDATER_LIBSPEC bool manifest_compute_sha256(struct SHA256_ctx *ctx, FILE *f,
 unsigned long len);
UPDATER_LIBSPEC bool manifest_entry_check_validity(struct manifest_entry *e,
 FILE *f);
UPDATER_LIBSPEC struct manifest_entry *manifest_list_create(FILE *f);

UPDATER_LIBSPEC void manifest_entry_free(struct manifest_entry *e);
UPDATER_LIBSPEC void manifest_list_free(struct manifest_entry **head);
UPDATER_LIBSPEC enum host_status manifest_get_updates(struct host *h,
 const char *basedir, struct manifest_entry **removed,
 struct manifest_entry **replaced, struct manifest_entry **added);
UPDATER_LIBSPEC bool manifest_entry_download_replace(struct host *h,
 const char *basedir, struct manifest_entry *e,
 void (*delete_hook)(const char *file));

__M_END_DECLS

#endif // __MANIFEST_H
