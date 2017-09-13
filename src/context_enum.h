/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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


/* CONTEXT_ENUM.H- Enumerations of the help system context */

#ifndef __CONTEXT_ENUM_H
#define __CONTEXT_ENUM_H

enum help_context {
  CTX_MAIN = 72,
  CTX_BLOCK_CMD = 73,
  CTX_BLOCK_TYPE = 74,
  CTX_CHOOSE_CHARSET = 75,
  CTX_IMPORTEXPORT_TYPE = 77,
  CTX_CHAR_EDIT = 79,
  CTX_STATUS_COUNTERS = 82,
  CTX_BOARD_EXITS = 83,
  CTX_BOARD_SIZES = 84,
  CTX_BOARD_INFO = 85,
  CTX_CHANGE_CHAR_IDS = 89,
  CTX_CHANGE_DAMAGE = 90,
  CTX_GLOBAL_SETTINGS = 86,
  CTX_GLOBAL_SETTINGS_2 = 88,
  CTX_ROBO_ED = 87,
  CTX_PALETTE_EDITOR = 93,
  CTX_SENSOR_EDITOR = 94,
  CTX_SUPER_MEGAZEUX = 95,
  CTX_SFX_EDITOR = 97,
  CTX_DIALOG_BOX = 98,
  CTX_F2_MENU = 92,
  CTX_PLAY_GAME = 91,
  CTX_UPDATER = 99,
  CTX_COUNTER_DEBUG = 100,
  CTX_ROBOT_DEBUG = 101,
  CTX_BREAKPOINT_EDITOR = 102,
};

#endif /* __CONTEXT_ENUM */