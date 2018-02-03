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
bool is_editor(void) { return false; }
void editor_init(void) { }
void init_macros(struct world *mzx_world) { }
void free_editor_config(struct world *mzx_world) { }
void load_editor_config(struct world *mzx_world, int *argc, char *argv[]) { }
#endif

#ifdef CONFIG_UPDATER
bool updater_init(int argc, char *argv[]) { return true; }
bool is_updater(void) { return false; }
#endif
