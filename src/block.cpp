/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "world.h"

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

char bdi_types[3] = { DE_RADIO, DE_BUTTON, DE_BUTTON };
char bdi_xs[3] = { 2, 5, 15 };
char bdi_ys[3] = { 2, 12, 12 };

char *bdi_strs[3] =
{
  "Copy block\n"
  "Copy block (repeated)\n"
  "Move block\n"
  "Clear block\n"
  "Flip block\n"
  "Mirror block\n"
  "Paint block\n"
  "Copy to/from overlay\n"
  "Save as MZM",
  "OK", "Cancel"
};

int bdi_p1s[3] = { 9, 0, -1};
int bdi_p2s[1] = { 21 };
int block_op = 0;
void *bdi_storage[1] = { &block_op };

dialog bdi =
{
  26, 3, 54, 17, "Choose block command", 3, bdi_types, bdi_xs,
  bdi_ys, bdi_strs, bdi_p1s, bdi_p2s, bdi_storage, 0
};

int block_cmd(World *mzx_world)
{
  int dialog_ret;
  set_context(73);
  dialog_ret = run_dialog(mzx_world, &bdi);
  pop_context();
  if(dialog_ret)
    return -1;

  return block_op;
}

char adi_types[3] = { DE_RADIO, DE_BUTTON, DE_BUTTON };
char adi_xs[3] = { 6, 5, 15 };
char adi_ys[3] = { 4, 11, 11 };
char *adi_strs[3] = { "Custom Block\nCustom Floor\nText", "OK", "Cancel" };
int adi_p1s[3] = { 3, 0, -1 };
int adi_p2s[1] = { 12 };
int obj_type = 0;
void *adi_storage[1] = { &obj_type };

dialog adi =
{
  26, 4, 53, 17, "Object type", 3, adi_types, adi_xs, adi_ys, adi_strs,
  adi_p1s, adi_p2s, adi_storage, 0
};

int rtoo_obj_type(World *mzx_world)
{
  set_context(74);

  if(run_dialog(mzx_world, &adi))
  {
    pop_context();
    return -1;
  }

  pop_context();
  return obj_type;
}

char csdi_types[3] = { DE_RADIO, DE_BUTTON, DE_BUTTON };
char csdi_xs[3] = { 4, 5, 15 };
char csdi_ys[3] = { 4, 11, 11 };
char *csdi_strs[3] =
{
  "MegaZeux default\n"
  "ASCII set\n"
  "SMZX set\n"
  "Blank set", "OK", "Cancel"
};

int csdi_p1s[3] = { 4, 0, -1};
int csdi_p2s[1] = { 16 };
int cs_type = 0;
void *csdi_storage[1] = { &cs_type };

dialog csdi =
{
  26, 4, 53, 17, "Character set", 3, csdi_types, csdi_xs, csdi_ys,
  csdi_strs, csdi_p1s, csdi_p2s, csdi_storage, 0
};

int choose_char_set(World *mzx_world)
{
  set_context(75);
  if(run_dialog(mzx_world, &csdi))
  {
    pop_context();
    return -1;
  }
  pop_context();
  return cs_type;
}

char ed_types[3] = { DE_INPUT, DE_BUTTON, DE_BUTTON };
char ed_xs[3] = { 5, 15, 37 };
char ed_ys[3] = { 2, 4, 4 };
char *ed_strs[3] = { NULL, "OK", "Cancel" };
int ed_p1s[3] = { 32, 0, 1 };
int ed_p2s = 192;
void *fe_ptr = NULL;

dialog e_di =
{
  10, 8, 69, 14, NULL, 3, ed_types, ed_xs, ed_ys, ed_strs, ed_p1s,
  &ed_p2s, &fe_ptr, 0
};

char edc_types[5] = { DE_INPUT, DE_NUMBER, DE_NUMBER, DE_BUTTON, DE_BUTTON };
char edc_xs[5] = { 5, 5, 28, 15, 37 };
char edc_ys[5] = { 2, 3, 3, 5, 5 };
char *edc_strs[5] = { "Save as: ", "Offset:  ", "Size: ", "OK", "Cancel" };
int edc_p1s[5] = { 32, 0, 1, 0, 1 };
int edc_p2s[5] = { 192, 255, 256, 0, 0 };
void *edc_storage[5] = { NULL };

dialog ec_di =
{
  10, 8, 69, 15, "Export", 5, edc_types, edc_xs, edc_ys, edc_strs, edc_p1s,
  edc_p2s, edc_storage, 0
};

int save_file_dialog(World *mzx_world, char *title, char *prompt, char *dest)
{
  fe_ptr = (void *)dest;
  ed_strs[0] = prompt;
  e_di.title = title;
  return run_dialog(mzx_world, &e_di);
}

int save_char_dialog(World *mzx_world, char *dest, int *char_offset,
 int *char_size)
{
  edc_storage[0] = dest;
  edc_storage[1] = char_offset;
  edc_storage[2] = char_size;
  return run_dialog(mzx_world, &ec_di);
}

// The ansi prefix code
char ansi_pre[3] = "[";
// EGA colors to ANSi colors
char col2ansi[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
// Mini func to output meta codes to transform color from one to another.
// Curr color is the current color and dest color is the color we want.

char exdi_types[3] = { DE_RADIO, DE_BUTTON, DE_BUTTON };
char exdi_xs[3] = { 2, 5, 15 };
char exdi_ys[3] = { 3, 8, 8 };

char *exdi_strs[3] =
{
  "Board file (MZB)\n"
  "Character set (CHR)\n"
  "Palette (PAL)\n"
  "Sound effects (SFX)",
  "OK", "Cancel"
};

int exdi_p1s[3] = { 4, 0, -1 };
int exdi_p2s[1] = { 19 };
void *exdi_storage[1] = { NULL };

dialog exdi =
{
  26, 5, 53, 15, "Export as:", 3, exdi_types, exdi_xs, exdi_ys,
  exdi_strs, exdi_p1s, exdi_p2s, exdi_storage, 0
};

int export_type(World *mzx_world)
{
  int storage = 0;
  set_context(77);
  exdi_storage[0] = &storage;

  if(run_dialog(mzx_world, &exdi))
  {
    pop_context();
    return -1;
  }
  pop_context();
  return storage;
}

char imdi_types[3] = { DE_RADIO, DE_BUTTON, DE_BUTTON };
char imdi_xs[3] = { 2, 5, 15 };
char imdi_ys[3] = { 3, 10, 10 };

char *imdi_strs[3] =
{
  "Board file (MZB)\n"
  "Character set (CHR)\n"
  "World file (MZX)\n"
  "Palette (PAL)\n"
  "Sound effects (SFX)\n"
  "MZM (choose pos.)",
  "OK", "Cancel"
};

int imdi_p1s[3] = { 6, 0, -1 };
int imdi_p2s[1] = { 19 };
void *imdi_storage[1] = { NULL };

dialog imdi =
{
  26, 4, 53, 16, "Import:", 3, imdi_types, imdi_xs, imdi_ys,
  imdi_strs, imdi_p1s, imdi_p2s,  imdi_storage, 0
};

int import_type(World *mzx_world)
{
  int storage = 0;
  set_context(78);
  imdi_storage[0] = &storage;
  if(run_dialog(mzx_world, &imdi))
  {
    pop_context();
    return -1;
  }
  pop_context();
  return storage;
}

char imzdi_types[3] = { DE_RADIO, DE_BUTTON, DE_BUTTON };
char imzdi_xs[3] = { 6, 5, 15 };
char imzdi_ys[3] = { 5, 11, 11 };
char *imzdi_strs[3] = { "Board\nOverlay", "OK", "Cancel" };
int imzdi_p1s[3] = { 2, 0, -1 };
int imzdi_p2s[1] = { 12 };
int imz_type = 0;
void *imzdi_storage[1] = { &imz_type };

dialog imzdi =
{
  26, 4, 53, 17, "Import MZM as-", 3, imzdi_types, imzdi_xs,
  imzdi_ys, imzdi_strs, imzdi_p1s, imzdi_p2s, imzdi_storage, 0
};

int import_mzm_obj_type(World *mzx_world)
{
  set_context(78);
  if(run_dialog(mzx_world, &imzdi))
  {
    pop_context();
    return -1;
  }

  pop_context();
  return imz_type;
}

