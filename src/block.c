/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

/* Block functions and dialogs */

#include <stdio.h>

#include "helpsys.h"
#include "idput.h"
#include "error.h"
#include "block.h"
#include "window.h"
#include "data.h"
#include "idarray.h"
#include "world_struct.h"

//--------------------------
//
// ( ) Copy block
// ( ) Move block
// ( ) Clear block
// ( ) Flip block
// ( ) Mirror block
// ( ) Paint block
// ( ) Copy to/from overlay
// ( ) Save as ANSi
// ( ) Save as MZM
//
//    _OK_      _Cancel_
//
//--------------------------

int block_cmd(World *mzx_world)
{
  int dialog_result;
  element *elements[3];
  dialog di;
  int block_operation = 0;
  char *radio_button_strings[] =
  {
    "Copy block",
    "Copy block (repeated)",
    "Move block",
    "Clear block",
    "Flip block",
    "Mirror block",
    "Paint block",
    "Copy to/from overlay",
    "Save as MZM"
  };

  set_context(73);
  elements[0] = construct_radio_button(2, 2, radio_button_strings,
   9, 21, &block_operation);
  elements[1] = construct_button(5, 12, "OK", 0);
  elements[2] = construct_button(15, 12, "Cancel", -1);

  construct_dialog(&di, "Choose block command", 26, 3, 29, 15,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  pop_context();

  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return block_operation;
}

int rtoo_obj_type(World *mzx_world)
{
  int dialog_result;
  element *elements[3];
  dialog di;
  int object_type = 0;
  char *radio_button_strings[] =
  {
    "Custom Block",
    "Custom Floor",
    "Text"
  };

  set_context(74);
  elements[0] = construct_radio_button(6, 4, radio_button_strings,
   3, 12, &object_type);
  elements[1] = construct_button(5, 11, "OK", 0);
  elements[2] = construct_button(15, 11, "Cancel", -1);

  construct_dialog(&di, "Object type", 26, 4, 28, 14,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);
  pop_context();

  if(dialog_result)
    return -1;

  return object_type;
}

int choose_char_set(World *mzx_world)
{
  int dialog_result;
  element *elements[3];
  dialog di;
  int charset_type = 0;
  char *radio_button_strings[] =
  {
    "MegaZeux default",
    "ASCII set",
    "SMZX set",
    "Blank set"
  };

  set_context(75);
  elements[0] = construct_radio_button(4, 4, radio_button_strings,
   4, 16, &charset_type);
  elements[1] = construct_button(5, 11, "OK", 0);
  elements[2] = construct_button(15, 11, "Cancel", -1);

  construct_dialog(&di, "Object type", 26, 4, 28, 14,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);
  pop_context();

  if(dialog_result)
    return -1;

  return charset_type;
}

int export_type(World *mzx_world)
{
  int export_choice = 0;
  int dialog_result;
  element *elements[3];
  dialog di;
  char *radio_strings[] =
  {
    "Board file (MZB)", "Character set (CHR)",
    "Palette (PAL)", "Sound effects (SFX)"
  };

  set_context(77);

  elements[0] = construct_radio_button(2, 3, radio_strings,
   4, 19, &export_choice);
  elements[1] = construct_button(5, 8, "OK", 0);
  elements[2] = construct_button(15, 8, "Cancel", -1);

  construct_dialog(&di, "Export as:", 26, 5, 28, 11,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);
  pop_context();

  if(dialog_result)
    return -1;

  return export_choice;
}

int import_type(World *mzx_world)
{
  int import_choice = 0;
  int dialog_result;
  element *elements[3];
  dialog di;
  char *radio_strings[] =
  {
    "Board file (MZB)", "Character set (CHR)",
    "World file (MZX)", "Palette (PAL)",
    "Sound effects (SFX)", "MZM (choose pos.)"
  };

  set_context(77);

  elements[0] = construct_radio_button(2, 3, radio_strings,
   6, 19, &import_choice);
  elements[1] = construct_button(5, 10, "OK", 0);
  elements[2] = construct_button(15, 10, "Cancel", -1);

  construct_dialog(&di, "Import:", 26, 4, 28, 13,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);
  pop_context();

  if(dialog_result)
    return -1;

  return import_choice;
}

int import_mzm_obj_type(World *mzx_world)
{
  int import_choice = 0;
  int dialog_result;
  element *elements[3];
  dialog di;
  char *radio_strings[] = { "Board", "Overlay" };

  set_context(78);

  elements[0] = construct_radio_button(6, 5, radio_strings,
   2, 19, &import_choice);
  elements[1] = construct_button(5, 11, "OK", 0);
  elements[2] = construct_button(15, 11, "Cancel", -1);

  construct_dialog(&di, "Import MZM as-", 26, 4, 28, 13,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);
  pop_context();

  if(dialog_result)
    return -1;

  return import_choice;
}
