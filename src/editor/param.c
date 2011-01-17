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

/* PARAM.CPP- Millions of dialog boxes for inputting parameters of
          a specified id. If a parameter is already available,
          it is edited. Special functions are called in other
          files for robots, scrolls, and signs. */

#include <string.h>

#include "../helpsys.h"
#include "../graphics.h"
#include "../intake.h"
#include "../scrdisp.h"
#include "../error.h"
#include "../window.h"
#include "../data.h"
#include "../robot.h"
#include "../rasm.h"
#include "../event.h"

#include "edit.h"
#include "edit_di.h"
#include "param.h"
#include "robo_ed.h"
#include "window.h"

static const char *const potion_fx[16] =
{
  "No Effect   ",
  "Invinco     ",
  "Blast       ",
  "Health x10  ",
  "Health x50  ",
  "Poison      ",
  "Blind       ",
  "Kill Enemies",
  "Lava Walker ",
  "Detonate    ",
  "Banish      ",
  "Summon      ",
  "Avalanche   ",
  "Freeze Time ",
  "Wind        ",
  "Slow Time   "
};

static int pe_chest(struct world *mzx_world, int param)
{
  int type = param & 0x0F;
  int var = param >> 4;

  const char *list[11] =
  {
    "Empty               ",
    "Key                 ",
    "Coins               ",
    "Lives               ",
    "Ammo                ",
    "Gems                ",
    "Health              ",
    "Potion              ",
    "Ring                ",
    "Lo Bombs            ",
    "Hi Bombs            "
  };

  // First, pick chest contents
  type = list_menu(list, 21, "Choose chest contents", type, 11, 27, 0);

  if(type < 0)
    return -1;

  switch(type)
  {
    case 0: // Empty
    {
      var = 0;
      break;
    }

    case 1: // Key
    {
      var = color_selection(var, 0);
      if(var > 0)
        var = var & 0x0F;
      break;
    }

    case 7:
    case 8: // Ring/potion
    {
      var = list_menu(potion_fx, 13, "Choose effect", var, 16, 31, 0);
      break;
    }

    default: // Number
    {
      struct dialog di;
      struct element *elements[3];
      int mult_five = 1;
      int dialog_result;
      const char *question = "Amount (multiple of five): ";

      set_confirm_buttons(elements);
      if(type == 3)
      {
        question = "Number of lives: ";
        mult_five = 0;
      }

      elements[2] = construct_number_box(5, 6, question,
       0, 15, mult_five, &var);

      construct_dialog(&di, "Set Quantity", 10, 5, 60, 18,
       elements, 3, 2);

      dialog_result = run_dialog(mzx_world, &di);

      if(dialog_result)
        var = -1;

      destruct_dialog(&di);
      break;
    }
  }

  if(var < 0)
    return -1;

  return (var << 4) | type;
}

static int pe_health(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Amount: ",
   0, 255, 0, &param);

  construct_dialog(&di, "Set Health/Ammo", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_ring(struct world *mzx_world, int param)
{
  int p;
  if(param >= 16)
    param = 0;
  p = list_menu(potion_fx, 13, "Choose effect", param, 16, 31, 0);
  if(p < 0)
    return -1;
  else
    return p;
}

static int pe_bomb(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Low strength bomb", "High strength bomb"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   2, 18, &param);

  construct_dialog(&di, "Set Bomb", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_lit_bomb(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  int type = param >> 7;
  int stage = 7 - (param & 0x3F);
  const char *radio_strings[] =
  {
    "Low strength bomb", "High strength bomb"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   2, 18, &type);
  elements[3] = construct_number_box(15, 9, "Fuse length: ",
   1, 7, 0, &stage);

  construct_dialog(&di, "Set Lit Bomb", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (type << 7) | (7 - stage);
}

static int pe_explosion(struct world *mzx_world, int param)
{
  int size = param >> 4;
  int stage = (param & 0x0F) + 1;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Explosion stage: ",
   1, 4, 0, &stage);
  elements[3] = construct_number_box(15, 8, "Explosion size: ",
   0, 15, 0, &size);

  construct_dialog(&di, "Set Explosion", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (size << 4) | (stage - 1);
}

static int pe_door(struct world *mzx_world, int param)
{
  int alignment = param & 1;
  int opens = (param >> 1) & 0x03;
  int check_results[1] = { (param >> 3) & 0x01 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "Horizontal", "Vertical"
  };
  const char *radio_strings_2[] =
  {
    "Opens N/W", "Opens N/E", "Opens S/W", "Opens S/E"
  };
  const char *check_strings[] = { "Locked door" };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 3, radio_strings_1,
   2, 18, &alignment);
  elements[3] = construct_radio_button(15, 6, radio_strings_2,
   4, 9, &opens);
  elements[4] = construct_check_box(15, 11, check_strings,
   1, 11, check_results);

  construct_dialog(&di, "Set Door", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return alignment | (opens << 1) | (check_results[0] << 3);
}

static int pe_gate(struct world *mzx_world, int param)
{
  int check_results[1] = { param };
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *check_strings[] = { "Locked gate" };

  set_confirm_buttons(elements);
  elements[2] = construct_check_box(15, 7, check_strings,
   1, 11, check_results);

  construct_dialog(&di, "Set Gate", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return check_results[0];
}

static int pe_transport(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West", "All"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   5, 5, &param);

  construct_dialog(&di, "Set Transport", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_pouch(struct world *mzx_world, int param)
{
  int coins = param >> 4;
  int gems = param & 0x0F;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(6, 6,
   "Number of gems (mult. of 5):  ", 0, 15, 1, &gems);
  elements[3] = construct_number_box(6, 8,
   "Number of coins (mult. of 5): ", 0, 15, 1, &coins);

  construct_dialog(&di, "Set Pouch", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (coins << 4) | gems;
}

static int pe_pusher(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 6, radio_strings,
   4, 5, &param);

  construct_dialog(&di, "Set Pusher/Missle/Spike", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_lazer_gun(struct world *mzx_world, int param)
{
  int dir = param & 0x03;
  int start = ((param >> 2) & 0x07) + 1;
  int end = (param >> 5) + 1;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 4, radio_strings,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 9, "Start time: ",
   1, 8, 0, &start);
  elements[4] = construct_number_box(15, 11, "Length on:  ",
   1, 7, 0, &end);

  construct_dialog(&di, "Set 'Lazer' Gun", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | (((start - 1) << 2) + ((end - 1) << 5));
}

static int pe_bullet(struct world *mzx_world, int param)
{
  int dir = param & 0x03;
  int type = param >> 2;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "North", "South", "East", "West"
  };
  const char *radio_strings_2[] =
  {
    "Player", "Neutral", "Enemy"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 5, radio_strings_1,
   4, 5, &dir);
  elements[3] = construct_radio_button(15, 10, radio_strings_2,
   3, 7, &type);

  construct_dialog(&di, "Set Bullet", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | (type << 2);
}

static int pe_ricochet_panel(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Orientation \\", "Orientation /"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 7, radio_strings,
   2, 13, &param);

  construct_dialog(&di, "Set Ricochet Panel", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return param;
}

static int pe_mine(struct world *mzx_world, int param)
{
  struct dialog di;
  struct element *elements[3];
  int dialog_result;

  param = (param >> 4) + 1;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 7, "Blast radius:  ",
   1, 8, 0, &param);

  construct_dialog(&di, "Set Mine", 10, 5, 60, 18,
   elements, 3, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (param - 1) << 4;
}

static int pe_snake(struct world *mzx_world, int param)
{
  int dir = param & 0x3;
  int intel = (param >> 4) + 1;
  int check_results[1] = { ((param >> 2) & 1) ^ 1 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };
  const char *check_strings[] = { "Fast movement " };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 4, radio_strings,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 9, "Intelligence:  ",
   1, 8, 0, &intel);
  elements[4] = construct_check_box(15, 11, check_strings,
   1, 13, check_results);

  construct_dialog(&di, "Set Snake", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | ((check_results[0] ^ 1) << 2) | ((intel - 1) << 4);
}

static int pe_eye(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int radius = ((param >> 3) & 0x07) + 1;
  int check_results[1] = { (param >> 6) ^ 1 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "Fast movement " };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Intelligence: ",
   1, 8, 0, &intel);
  elements[3] = construct_number_box(15, 8, "Blast radius: ",
   1, 8, 0, &radius);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 13, check_results);

  construct_dialog(&di, "Set Eye", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((radius - 1) << 3) | ((check_results[0] ^ 1) << 6);
}

static int pe_thief(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int speed = ((param >> 3) & 0x03) + 1;
  int gems = (param >> 7) + 1;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6,  "Intelligence:   ",
   1, 8, 0, &intel);
  elements[3] = construct_number_box(15, 8,  "Movement speed: ",
   1, 4, 0, &speed);
  elements[4] = construct_number_box(15, 10, "Gems stolen:    ",
   1, 2, 0, &gems);

  construct_dialog(&di, "Set Thief", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((speed - 1) << 3) | ((gems - 1) << 7);
}

static int pe_slime_blob(struct world *mzx_world, int param)
{
  int speed = (param & 0x07) + 1;
  int check_results[2] = { (param >> 6) & 0x01, (param >> 7) & 0x01 };
  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  const char *check_strings[] =
  {
    "Hurts player", "Invincible"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Spread speed: ",
   1, 8, 0, &speed);
  elements[3] = construct_check_box(15, 8, check_strings,
   2, 12, check_results);

  construct_dialog(&di, "Set Slime Blob", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (speed - 1) | (check_results[0] << 6) |
   (check_results[1] << 7);
}

static int pe_runner(struct world *mzx_world, int param)
{
  int dir = param & 0x03;
  int hp = (param >> 6) + 1;
  int speed = ((param >> 2) & 3) + 1;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "North", "South", "East", "West"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 4, radio_strings,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 9, "Hit points: ",
   1, 4, 0, &hp);
  elements[4] = construct_number_box(15, 11, "Movement speed: ",
   1, 4, 0, &speed);

  construct_dialog(&di, "Set Runner", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return dir | ((speed - 1) << 2) | ((hp - 1) << 6);
}

static int pe_ghost(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int speed = ((param >> 4) & 0x03) + 1;
  int check_results[] = { (param >> 3) & 0x01 };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "Invincible" };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Intelligence:  ",
   1, 8, 0, &intel);
  elements[3] = construct_number_box(15, 8, "Movement speed: ",
   1, 4, 0, &speed);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 10, check_results);

  construct_dialog(&di, "Set Ghost", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((speed - 1) << 4) | (check_results[0] << 3);
}

static int pe_dragon(struct world *mzx_world, int param)
{
  int fire_rate = (param & 0x03) + 1;
  int hp = ((param >> 5) & 0x07) + 1;
  int check_results[1] = { ((param >> 2) & 0x01) };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "Moves" };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Firing rate: ",
   1, 4, 0, &fire_rate);
  elements[3] = construct_number_box(15, 8, "Hit points:  ",
   1, 8, 0, &hp);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 5, check_results);

  construct_dialog(&di, "Set Dragon", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (fire_rate - 1) | ((hp - 1) << 5) | (check_results[0] << 2);
}

static int pe_fish(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int check_results[4] =
  {
    (param >> 6) & 0x01, (param >> 5) & 0x01,
    (param >> 7) & 0x01, ((param >> 3) & 0x01) ^ 1
  };

  struct dialog di;
  struct element *elements[4];
  int dialog_result;
  const char *check_strings[] =
  {
    "Hurts player", "Affected by current",
    "2 hit points", "Fast movement"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 5, "Intelligence: ",
   1, 8, 0, &intel);
  elements[3] = construct_check_box(15, 7, check_strings,
   4, 19, check_results);

  construct_dialog(&di, "Set Fish", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | (check_results[0] << 6) |
   (check_results[1] << 5) | (check_results[2] << 7) |
   ((check_results[3] ^1) << 3);
}

static int pe_shark(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int fire_rate = ((param >> 5) & 0x03) + 1;
  int fires = ((param >> 3) & 0x03);
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Fires bullets", "Fires seekers",
    "Fires fire", "Fires nothing"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 4, "Intelligence: ",
   1, 8, 0, &intel);
  elements[3] = construct_number_box(15, 6, "Firing rate:  ",
   1, 8, 0, &fire_rate);
  elements[4] = construct_radio_button(15, 8, radio_strings,
   4, 13, &fires);

  construct_dialog(&di, "Set Shark/Spitting Tiger", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((fire_rate - 1) << 5) | (fires << 3);
}

static int pe_spider(struct world *mzx_world, int param)
{
  int intel = (param & 0x07) + 1;
  int web = ((param >> 3) & 0x03);
  int check_results[2] =
  {
    ((param >> 5) & 0x01) ^ 1, ((param >> 7) & 0x01)
  };

  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *radio_strings[] =
  {
    "Thin web only", "Thick web only", "Any web", "Anywhere"
  };
  const char *check_strings[] =
  {
    "Fast movement", "2 hit points"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 3, "Intelligence: ",
   1, 8, 0, &intel);
  elements[3] = construct_radio_button(15, 5, radio_strings,
   4, 14, &web);
  elements[4] = construct_check_box(15, 10, check_strings,
   2, 13, check_results);

  construct_dialog(&di, "Set Spider", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | (web << 3) |
   ((check_results[0] ^ 1) << 5) | (check_results[1] << 7);
}

static int pe_goblin(struct world *mzx_world, int param)
{
  int intel = ((param >> 6) & 0x03) + 1;
  int rest_len = (param & 0x03) + 1;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 7, "Intelligence: ",
   1, 4, 0, &intel);
  elements[3] = construct_number_box(15, 9, "Rest length:  ",
   1, 4, 0, &rest_len);

  construct_dialog(&di, "Set Goblin", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (rest_len - 1) | ((intel - 1) << 6);
}

static int pe_bullet_gun(struct world *mzx_world, int param)
{
  int dir = (param >> 3) & 0x03;
  int intel = ((param >> 5) & 0x03) + 1;
  int fire_rate = (param & 0x07) + 1;
  int type = (param >> 7);
  struct dialog di;
  struct element *elements[6];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "North", "South", "East", "West"
  };
  const char *radio_strings_2[] =
  {
    "Fires bullets", "Fires fire"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 2, radio_strings_1,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 7, "Intelligence: ",
   1, 4, 0, &intel);
  elements[4] = construct_number_box(15, 9, "Firing rate:  ",
   1, 8, 0, &fire_rate);
  elements[5] = construct_radio_button(15, 11, radio_strings_2,
   2, 13, &type);

  construct_dialog(&di, "Set Bullet Gun/Spinning Gun", 10, 5, 60, 18,
   elements, 6, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (fire_rate - 1) | ((dir << 3) + ((intel - 1) << 5) + (type << 7));
}

static int pe_bear(struct world *mzx_world, int param)
{
  int sens = (param & 0x07) + 1;
  int speed = ((param >> 3) & 0x03) + 1;
  int check_results[1] = { (param >> 7) };
  struct dialog di;
  struct element *elements[5];
  int dialog_result;
  const char *check_strings[] = { "2 hit points" };

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 6, "Sensitivity:   ",
   1, 8, 0, &sens);
  elements[3] = construct_number_box(15, 8, "Movement speed: ",
   1, 4, 0, &speed);
  elements[4] = construct_check_box(15, 10, check_strings,
   1, 12, check_results);

  construct_dialog(&di, "Set Bear", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (sens - 1) | ((speed - 1) << 3) | (check_results[0] << 7);
}

static int pe_bear_cub(struct world *mzx_world, int param)
{
  int intel = (param & 0x03) + 1;
  int switch_rate = ((param >> 2) & 0x03) + 1;
  struct dialog di;
  struct element *elements[4];
  int dialog_result;

  set_confirm_buttons(elements);
  elements[2] = construct_number_box(15, 7, "Intelligence: ",
   1, 4, 0, &intel);
  elements[3] = construct_number_box(15, 9, "Switch rate:  ",
   1, 4, 0, &switch_rate);

  construct_dialog(&di, "Set Bear Cub", 10, 5, 60, 18,
   elements, 4, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (intel - 1) | ((switch_rate - 1) << 2);
}

static int pe_missile_gun(struct world *mzx_world, int param)
{
  int dir = (param >> 3) & 0x03;
  int intel = ((param >> 5) & 0x03) + 1;
  int fire_rate = (param & 0x07) + 1;
  int type = (param >> 7);
  struct dialog di;
  struct element *elements[6];
  int dialog_result;
  const char *radio_strings_1[] =
  {
    "North", "South", "East", "West"
  };
  const char *radio_strings_2[] =
  {
    "Fires once", "Fires multiple"
  };

  set_confirm_buttons(elements);
  elements[2] = construct_radio_button(15, 2, radio_strings_1,
   4, 5, &dir);
  elements[3] = construct_number_box(15, 7, "Intelligence:  ",
   1, 4, 0, &intel);
  elements[4] = construct_number_box(15, 9, "Firing rate:   ",
   1, 8, 0, &fire_rate);
  elements[5] = construct_radio_button(15, 11, radio_strings_2,
   2, 14, &type);

  construct_dialog(&di, "Set Missile Gun", 10, 5, 60, 18,
   elements, 6, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(dialog_result)
    return -1;

  return (fire_rate - 1) | (dir << 3) |
   ((intel - 1) << 5) | (type << 7);
}

int edit_sensor(struct world *mzx_world, struct sensor *cur_sensor)
{
  char sensor_name[15];
  char sensor_robot[15];
  int sensor_char = cur_sensor->sensor_char;
  struct dialog di;
  struct element *elements[5];
  int dialog_result;

  set_context(94);
  strcpy(sensor_name, cur_sensor->sensor_name);
  strcpy(sensor_robot, cur_sensor->robot_to_mesg);

  set_confirm_buttons(elements);
  elements[2] = construct_input_box(15, 6, "Sensor's name:    ",
   14, 0, sensor_name);
  elements[3] = construct_input_box(15, 8, "Robot to message: ",
   14, 0, sensor_robot);
  elements[4] = construct_char_box(15, 10, "Sensor character: ",
   1, &sensor_char);

  construct_dialog(&di, "Set Sensor", 10, 5, 60, 18,
   elements, 5, 2);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  pop_context();

  if(dialog_result)
    return -1;

  strcpy(cur_sensor->sensor_name, sensor_name);
  strcpy(cur_sensor->robot_to_mesg, sensor_robot);
  cur_sensor->sensor_char = sensor_char;
  return 0;
}

int edit_scroll(struct world *mzx_world, struct scroll *cur_scroll)
{
  scroll_edit(mzx_world, cur_scroll, 2);
  return 0;
}

int edit_robot(struct world *mzx_world, struct robot *cur_robot)
{
  int new_char;
  // First get name...
  m_hide();
  save_screen();
  draw_window_box(16, 12, 50, 14, DI_DEBUG_BOX, DI_DEBUG_BOX_DARK,
   DI_DEBUG_BOX_CORNER, 1, 1);
  write_string("Name for robot:", 18, 13, DI_DEBUG_LABEL, 0);
  m_show();

  if(intake(mzx_world, cur_robot->robot_name, 14, 34, 13,
   15, 1, 0, NULL, 0, NULL) != IKEY_ESCAPE)
  {
    restore_screen();
    save_screen();
    // ...and character.
    new_char = char_selection(cur_robot->robot_char);
    if(new_char < 0)
    {
      // ESC means keep char but don't edit robot
      if(new_char == -256)
        new_char = 0;
      cur_robot->robot_char = -new_char;
    }
    else
    {
      cur_robot->robot_char = new_char;
      // Now edit the program.
      set_context(87);
      robot_editor(mzx_world, cur_robot);
    }
  }
  else
  {
    pop_context();
    restore_screen();
    return -1;
  }

  pop_context();
  restore_screen();
  return 0;
}

// Returns parameter or -1 for ESC. Must pass a legit parameter, or -1 to
// use default or load up a new robot/scroll/sign.
int edit_param(struct world *mzx_world, int id, int param)
{
  if(def_params[id] == -2)
  {
    // No editing allowed
    // If has a param, return it, otherwise return 0
    if(param > -1)
      return param;

    return 0;
  }

  if(def_params[id] == -3)
  {
    int new_param = param;

    if(new_param < 0)
      new_param = 177;

    // Character
    new_param = char_selection(new_param);

    if(new_param < 0)
      new_param = param;

    return new_param;
  }

  if(def_params[id] == -4)
  {
    int new_param = param;
    // Board
    if(new_param < 0)
      new_param = 0;

    new_param = choose_board(mzx_world, new_param,
     "Choose destination board", 0);
    if(new_param < 0)
      new_param = param;

    return new_param;
  }

  if(param < 0)
    param = def_params[id];

  // Switch according to thing
  switch(id)
  {
    case 27: // Chest
    {
      param = pe_chest(mzx_world, param);
      break;
    }

    case 30: // Health
    case 35: // Ammo
    {
      param = pe_health(mzx_world, param);
      break;
    }

    case 31: // Ring
    case 32: // Potion
    {
      param = pe_ring(mzx_world, param);
      break;
    }

    case 36: // Bomb
    {
      param = pe_bomb(mzx_world, param);
      break;
    }

    case 37: // Lit bomb
    {
      param = pe_lit_bomb(mzx_world, param);
      break;
    }

    case 38: // Explosion
    {
      param = pe_explosion(mzx_world, param);
      break;
    }

    case 41: // Door
    {
      param = pe_door(mzx_world, param);
      break;
    }

    case 47: // Gate
    {
      param = pe_gate(mzx_world, param);
      break;
    }

    case 49: // Transport
    {
      param = pe_transport(mzx_world, param);
      break;
    }

    case 55: // Pouch
    {
      param = pe_pouch(mzx_world, param);
      break;
    }

    case 56: // Pusher
    case 62: // Missile
    case 75: // Spike
    {
      param = pe_pusher(mzx_world, param);
      break;
    }

    case 78: // Shooting fire
    {
      param = pe_pusher(mzx_world, param >> 1) << 1;
      break;
    }

    case 60: // Lazer gun
    {
      param = pe_lazer_gun(mzx_world, param);
      break;
    }

    case 61: // Bullet
    {
      param = pe_bullet(mzx_world, param);
      break;
    }

    case 72: // Ricochet panel
    {
      param = pe_ricochet_panel(mzx_world, param);
      break;
    }

    case 74: // Mine
    {
      param = pe_mine(mzx_world, param);
      break;
    }

    case 80: // Snake
    {
      param = pe_snake(mzx_world, param);
      break;
    }

    case 81: // Eye
    {
      param = pe_eye(mzx_world, param);
      break;
    }

    case 82: // Thief
    {
      param = pe_thief(mzx_world, param);
      break;
    }

    case 83: // Slime blob
    {
      param = pe_slime_blob(mzx_world, param);
      break;
    }

    case 84: // Runner
    {
      param = pe_runner(mzx_world, param);
      break;
    }

    case 85: // Ghost
    {
      param = pe_ghost(mzx_world, param);
      break;
    }

    case 86: // Dragon
    {
      param = pe_dragon(mzx_world, param);
      break;
    }

    case 87: // Fish
    {
      param = pe_fish(mzx_world, param);
      break;
    }

    case 88: // Shark
    case 91: // Spitting tiger
    {
      param = pe_shark(mzx_world, param);
      break;
    }

    case 89: // Spider
    {
      param = pe_spider(mzx_world, param);
      break;
    }

    case 90: // Goblin
    {
      param = pe_goblin(mzx_world, param);
      break;
    }

    case 92: // Bullet gun
    case 93: // Spinning gun
    {
      param = pe_bullet_gun(mzx_world, param);
      break;
    }

    case 94: // Bear
    {
      param = pe_bear(mzx_world, param);
      break;
    }

    case 95: // Bear cub
    {
      param = pe_bear_cub(mzx_world, param);
      break;
    }

    case 97: // Missile gun
    {
      param = pe_missile_gun(mzx_world, param);
      break;
    }
  }
  // Return param
  return param;
}
