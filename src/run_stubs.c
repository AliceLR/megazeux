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

#include "run_stubs.h"

#ifdef CONFIG_EDITOR
void editor_init(void) { }
void init_macros(World *mzx_world) { }
void free_extended_macros(World *mzx_world) { }
void load_editor_config(World *mzx_world, int argc, char *argv[]) { }
#endif

#ifdef CONFIG_UPDATER
bool network_layer_init(config_info *info, char *argv[]) { return true; }
void network_layer_exit(config_info *info) {}
#endif
