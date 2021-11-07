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

#include "../audio/sfx.h"

#include "../core.h"
#include "../data.h"
#include "../error.h"
#include "../event.h"
#include "../window.h"
#include "../world.h"
#include "../io/vio.h"

#include "sfx_edit.h"
#include "window.h"

// 8 char names per sfx

static const char *const sfx_names[NUM_BUILTIN_SFX] =
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

static const char *const sfx_ext[] = { ".SFX", NULL };

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

void sfx_edit(struct world *mzx_world)
{
  // First, ask whether sfx should be INTERNAL or CUSTOM. If change is made
  // from CUSTOM to INTERNAL, verify deletion of CUSTOM sfx. If changed to
  // CUSTOM, copy over INTERNAL to CUSTOM. If changed to or left at CUSTOM,
  // go to sfx editing.
  struct dialog a_di, b_di;
  int i;
  int old_sfx_mode = mzx_world->custom_sfx_on;
  int page = 0;
  int dialog_result;
  int num_elements;
  const char *radio_strings[] =
  {
    "Default internal SFX", "Custom SFX"
  };
  struct element *a_elements[3] =
  {
    construct_radio_button(2, 2, radio_strings, 2, 20,
     &mzx_world->custom_sfx_on),
    construct_button(5, 5, "OK", 0),
    construct_button(15, 5, "Cancel", -1)
  };

  struct element *b_elements[21];

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_SFX_EDITOR);

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
      for(i = 0; i < NUM_BUILTIN_SFX; i++, offset += SFX_SIZE)
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
           sfx_names[(page * 17) + i], 68,
           mzx_world->custom_sfx + ((i + (page * 17)) * SFX_SIZE));
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

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  // Done!
  pop_context();
}

/**
 * Import world SFX strings.
 */
void import_sfx(context *parent, boolean *modified)
{
  struct world *mzx_world = parent->world;
  char import_name[MAX_PATH];
  import_name[0] = '\0';

  if(!choose_file(mzx_world, sfx_ext, import_name,
   "Choose SFX file to import", ALLOW_ALL_DIRS))
  {
    char tmp[LEGACY_SFX_SIZE * NUM_BUILTIN_SFX];
    vfile *sfx_file;
    size_t n;

    sfx_file = vfopen_unsafe(import_name, "rb");

    n = vfread(tmp, LEGACY_SFX_SIZE, NUM_BUILTIN_SFX, sfx_file);
    vfclose(sfx_file);

    if(n == NUM_BUILTIN_SFX && load_sfx_array(mzx_world, tmp))
    {
      mzx_world->custom_sfx_on = 1;
      *modified = true;
    }
    else
      error_message(E_SFX_IMPORT, 0, NULL);
  }
}

/**
 * Export world SFX strings.
 */
void export_sfx(context *parent)
{
  struct world *mzx_world = parent->world;
  char export_name[MAX_PATH];
  export_name[0] = '\0';

  if(!new_file(mzx_world, sfx_ext, ".sfx", export_name,
   "Export SFX file", ALLOW_ALL_DIRS))
  {
    char tmp[LEGACY_SFX_SIZE * NUM_BUILTIN_SFX];
    vfile *sfx_file;

    sfx_file = vfopen_unsafe(export_name, "wb");
    if(sfx_file)
    {
      // TODO: replace this hack
      const char *sfx_array = (const char *)sfx_strs;
      if(mzx_world->custom_sfx_on)
      {
        sfx_array = tmp;
        save_sfx_array(mzx_world, tmp);
      }
      if(vfwrite(sfx_array, LEGACY_SFX_SIZE, NUM_BUILTIN_SFX, sfx_file) < NUM_BUILTIN_SFX)
        error_message(E_IO_WRITE, 0, NULL);

      vfclose(sfx_file);
    }
    else
      error_message(E_SFX_EXPORT, 0, NULL);
  }
}
