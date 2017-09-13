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

/* Selection dialogs */
/* Prior to 2.91, this was block.c */

#include "select.h"

#include "../helpsys.h"
#include "../idput.h"
#include "../error.h"
#include "../event.h"
#include "../window.h"
#include "../data.h"
#include "../idarray.h"
#include "../world_struct.h"

#include <stdio.h>

//--------------------------
//
// ( ) Copy block
// ( ) Copy block (repeated)
// ( ) Move block
// ( ) Clear block
// ( ) Flip block
// ( ) Mirror block
// ( ) Paint block
// ( ) Copy to/from overlay
// ( ) Save as MZM
//
//    _OK_      _Cancel_
//
//--------------------------
int block_cmd(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  int block_operation = 0;
  const char *radio_button_strings[] =
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

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_BLOCK_CMD);
  elements[0] = construct_radio_button(2, 2, radio_button_strings,
   9, 21, &block_operation);
  elements[1] = construct_button(5, 12, "OK", 0);
  elements[2] = construct_button(15, 12, "Cancel", -1);

  construct_dialog(&di, "Choose block command", 26, 3, 29, 15,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  pop_context();

  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return block_operation;
}

int layer_to_board_object_type(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  int object_type = 0;
  const char *radio_button_strings[] =
  {
    "Custom Block",
    "Custom Floor",
    "Text"
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_BLOCK_TYPE);
  elements[0] = construct_radio_button(6, 4, radio_button_strings,
   3, 12, &object_type);
  elements[1] = construct_button(5, 11, "OK", 0);
  elements[2] = construct_button(15, 11, "Cancel", -1);

  construct_dialog(&di, "Object type", 26, 4, 28, 14,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return object_type;
}

int choose_char_set(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  int charset_type = 0;
  const char *radio_button_strings[] =
  {
    "MegaZeux default",
    "ASCII set",
    "SMZX set",
    "Blank set"
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_CHOOSE_CHARSET);
  elements[0] = construct_radio_button(4, 4, radio_button_strings,
   4, 16, &charset_type);
  elements[1] = construct_button(5, 11, "OK", 0);
  elements[2] = construct_button(15, 11, "Cancel", -1);

  construct_dialog(&di, "Object type", 26, 4, 28, 14,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return charset_type;
}

int export_type(struct world *mzx_world)
{
  int export_choice = 0;
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  const char *radio_strings[] =
  {
    "Board file (MZB)",
    "Character set (CHR)",
    "Palette (PAL)",
    "Sound effects (SFX)",
    "Downver. world (MZX)",
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_IMPORTEXPORT_TYPE);

  elements[0] = construct_radio_button(2, 3, radio_strings,
   5, 19, &export_choice);
  elements[1] = construct_button(5, 9, "OK", 0);
  elements[2] = construct_button(15, 9, "Cancel", -1);

  construct_dialog(&di, "Export as:", 26, 5, 28, 12,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return export_choice;
}

int import_type(struct world *mzx_world)
{
  int import_choice = 0;
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  const char *radio_strings[] =
  {
    "Board file (MZB)",
    "Character set (CHR)",
    "World file (MZX)",
    "Palette (PAL)",
    "Sound effects (SFX)",
    "MZM (choose pos.)"
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_IMPORTEXPORT_TYPE);

  elements[0] = construct_radio_button(2, 3, radio_strings,
   6, 19, &import_choice);
  elements[1] = construct_button(5, 10, "OK", 0);
  elements[2] = construct_button(15, 10, "Cancel", -1);

  construct_dialog(&di, "Import:", 26, 4, 28, 13,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return -1;

  return import_choice;
}
