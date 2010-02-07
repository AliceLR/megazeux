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

// Code to edit (via edit dialog) and test custom sound effects

#include <string.h>

#include "helpsys.h"
#include "sfx_edit.h"
#include "sfx.h"
#include "window.h"
#include "data.h"

// 8 char names per sfx

static const char *sfx_names[NUM_SFX] =
{
  "Gem      ",
  "MagicGem ",
  "Health   ",
  "Ammo     ",
  "Coin     ",
  "Life     ",
  "Lo Bomb  ",
  "Hi Bomb  ",
  "Key      ",
  "FullKeys ",
  "Unlock   ",
  "Need Key ",
  "InvsWall ",
  "Forest   ",
  "GateLock ",
  "OpenGate ",
  "Invinco1 ",
  "Invinco2 ",
  "Invinco3 ",
  "DoorLock ",
  "OpenDoor ",
  "Ouch     ",
  "Lava     ",
  "Death    ",
  "GameOver ",
  "GateClse ",
  "Push     ",
  "Trnsport ",
  "Shoot    ",
  "Break    ",
  "NeedAmmo ",
  "Ricochet ",
  "NeedBomb ",
  "Place Lo ",
  "Place Hi ",
  "BombType ",
  "Explsion ",
  "Entrance ",
  "Pouch    ",
  "Potion   ",
  "EmtyChst ",
  "Chest    ",
  "Time Out ",
  "FireOuch ",
  "StolenGm ",
  "EnemyHit ",
  "DrgnFire ",
  "Scroll   ",
  "Goop     ",
  "Unused   "
};

//--------------------------
//
// ( ) Default internal SFX
// ( ) Custom SFX
//
//    _OK_      _Cancel_
//
//--------------------------


//(full screen)
//
//|--
//|
//|Explsion_________//_____|
//|MagicGem_________//_____|
//|etc.    _________//_____|
//|
//|
//|
//| Next Prev Done
//|
//|--

// 17 on pg 1/2, 16 on pg 3

void sfx_edit(World *mzx_world)
{
  // First, ask whether sfx should be INTERNAL or CUSTOM. If change is made
  // from CUSTOM to INTERNAL, verify deletion of CUSTOM sfx. If changed to
  // CUSTOM, copy over INTERNAL to CUSTOM. If changed to or left at CUSTOM,
  // go to sfx editing.
  dialog a_di, b_di;
  int i;
  int old_sfx_mode = mzx_world->custom_sfx_on;
  int page = 0;
  int dialog_result;
  int num_elements;
  const char *radio_strings[] =
  {
    "Default internal SFX", "Custom SFX"
  };
  element *a_elements[3] =
  {
    construct_radio_button(2, 2, radio_strings, 2, 20,
     &mzx_world->custom_sfx_on),
    construct_button(5, 5, "OK", 0),
    construct_button(15, 5, "Cancel", -1)
  };

  element *b_elements[21];

  set_context(97);

  construct_dialog(&a_di, "Choose SFX mode", 26, 7, 28, 8,
   a_elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &a_di);

  if(!dialog_result)
  {
    // Custom to internal
    if((old_sfx_mode) && (!mzx_world->custom_sfx_on))
    {
      if(confirm(mzx_world, "Delete current custom SFX?"))
      {
        mzx_world->custom_sfx_on = 1;
      }
    }

    // Internal to custom
    if((!old_sfx_mode) && (mzx_world->custom_sfx_on))
    {
      char *offset = mzx_world->custom_sfx;
      for(i = 0; i < NUM_SFX; i++, offset += 69)
      {
        strcpy(offset, sfx_strs[i]);
      }
    }

    // Internal
    if(mzx_world->custom_sfx_on)
    {
      // Edit custom
      do
      {
        // Setup page
        num_elements = 17;

        if(page >= 2)
          num_elements = 16;

        for(i = 0; i < num_elements; i++)
        {
          b_elements[i] = construct_input_box(1, i + 2,
           sfx_names[(page * 17) + i], 68, 224,
           mzx_world->custom_sfx + ((i + (page * 17)) * 69));
        }

        b_elements[i] = construct_label(23, 20,
         "Press Alt+T to test a sound effect");
        b_elements[i + 1] = construct_button(16, 22, "Next", 1);
        b_elements[i + 2] = construct_button(36, 22, "Previous", 2);
        b_elements[i + 3] = construct_button(60, 22, "Done", 0);

        num_elements += 4;

        construct_dialog_ext(&b_di, "Edit custom SFX", 0, 0, 80, 25,
         b_elements, num_elements, 1, 0, 0, NULL);
        // Run dialog
        dialog_result = run_dialog(mzx_world, &b_di);
        destruct_dialog(&b_di);

        // Next/prev page?
        switch(dialog_result)
        {
          case 1:
          {
            page++;
            if(page > 2)
              page = 0;
            break;
          }

          case 2:
          {
            page--;
            if(page < 0)
              page = 2;
            break;
          }
        }
      } while(dialog_result > 0);
    }
  }

  destruct_dialog(&a_di);

  // Done!
  pop_context();
}
