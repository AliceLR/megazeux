/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __EDITOR_CONFIGURE_H
#define __EDITOR_CONFIGURE_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "macro.h"

struct editor_config_info
{
  // Board editor options
  int editor_space_toggles;
  int bedit_hhelp;

  // Robot editor options
  bool editor_enter_splits;
  char color_codes[32];
  int color_coding_on;
  int default_invalid_status;
  int redit_hhelp;

  // Backup options
  int backup_count;
  int backup_interval;
  char backup_name[256];
  char backup_ext[256];

  // Macro options
  char default_macros[5][64];
  int num_extended_macros;
  int num_macros_allocated;
  struct ext_macro **extended_macros;
};

typedef void (* editor_config_function)(struct editor_config_info *conf,
 char *name, char *value, char *extended_data);

void set_editor_config_from_file(struct editor_config_info *conf,
 const char *conf_file_name);
void default_editor_config(struct editor_config_info *conf);
void set_editor_config_from_command_line(struct editor_config_info *conf,
 int argc, char *argv[]);

__M_END_DECLS

#endif // __EDITOR_CONFIGURE_H
