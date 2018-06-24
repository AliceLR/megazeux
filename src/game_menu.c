/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "const.h"
#include "core.h"
#include "counter.h"
#include "event.h"
#include "game.h"
#include "graphics.h"
#include "window.h"
#include "world_struct.h"

#define MENU_COL            0x19
#define MENU_COL_DARK       0x10
#define MENU_COL_CORNER     0x18
#define MENU_COL_TITLE      0x1E
#define MENU_COL_TEXT       0x1F
#define MENU_COL_COUNTER    0x1B

#define GAME_MENU_X         8
#define GAME_MENU_Y         4
#define GAME_MENU_WIDTH     29
#define GAME_MENU_HEIGHT    13
#define GAME_HELP_HEIGHT    1
#define GAME_TEXT_HEIGHT    11
#define GAME_DEBUG_HEIGHT   4

#define GAME_STATUS_X       (GAME_MENU_X + GAME_MENU_WIDTH + 1)
#define GAME_STATUS_Y       GAME_MENU_Y
#define GAME_STATUS_WIDTH   30
#define GAME_STATUS_HEIGHT  18
#define GAME_STATUS_INDENT  16

#define MAIN_MENU_X         28
#define MAIN_MENU_Y         4
#define MAIN_MENU_WIDTH     24
#define MAIN_MENU_HEIGHT    9
#define MAIN_HELP_HEIGHT    1
#define MAIN_NETWORK_HEIGHT 1
#define MAIN_EDITOR_HEIGHT  2

#define KEY_CHAR            '\x0C'

static const char main_menu_title[] = "MegaZeux " VERSION;

static const char main_menu_1[] =
 "Enter- Menu\n"
 "Esc  - Exit MegaZeux\n";

static const char main_menu_2[] =
 "F1/H - Help\n";

static const char main_menu_3[] =
 "F2/S - Settings\n"
 "F3/L - Load world\n"
 "F4/R - Restore game\n"
 "F5/P - Play world";

static const char main_menu_4[] =
 "F7/U - Updater";

static const char main_menu_5[] =
 "F8/N - New World\n"
 "F9/E - Edit World";

static const char main_menu_6[] =
 "F10  - Quickload\n";
// "";  // unused

static const char game_menu_title[] = "Game Menu";

static const char game_menu_1[] =
 "F1     - Help\n";

static const char game_menu_2[] =
 "Enter  - Menu/status\n"
 "Esc    - Exit to title\n"
 "F2     - Settings\n"
 "F3     - Save game\n"
 "F4     - Restore game\n"
 "F5/Ins - Toggle bomb type\n"
 "F9     - Quicksave\n"
 "F10    - Quickload\n"
 "Arrows - Move\n"
 "Space  - Shoot (w/dir)\n"
 "Delete - Bomb";

static const char game_menu_3[] =
 "\n"
 "F6     - Debug Window\n"
 "F11    - Counter Debugger\n"
 "Alt+F11- Robot Debugger";

/**
 * Draw a menu border and title to the screen.
 */

static void draw_menu_box(int x, int y, int width, int height,
 const char *title)
{
  draw_window_box(x, y, width + x - 1, height + y - 1,
   MENU_COL, MENU_COL_DARK, MENU_COL_CORNER, true, true);

  if(title)
  {
    int title_width = strlen(title);
    int title_x = (width - title_width)/2 + x;

    write_string(title, title_x, y, MENU_COL_TITLE, false);
    draw_char(' ', MENU_COL_TITLE, title_x - 1, y);
    draw_char(' ', MENU_COL_TITLE, title_x + title_width, y);
  }
}

/**
 * Write a single counter to the status screen.
 */

static void show_counter(struct world *mzx_world, const char *counter_name,
 int x, int y, boolean skip_if_zero)
{
  int counter_value = get_counter(mzx_world, counter_name, 0);

  if((skip_if_zero) && (!counter_value))
    return;

  write_string(counter_name, x, y, MENU_COL_COUNTER, false);
  write_number(counter_value, MENU_COL_TEXT, x + GAME_STATUS_INDENT, y,
   1, false, 10);
}

/**
 * Draw the game status window. This window displays information to the player
 * such as built-in counters, keys, custom status counters, and active effects.
 */

static void draw_game_status(struct world *mzx_world)
{
  char *keys = mzx_world->keys;
  int x;
  int y;
  int i;

  draw_menu_box(
   GAME_STATUS_X, GAME_STATUS_Y, GAME_STATUS_WIDTH, GAME_STATUS_HEIGHT,
   NULL
  );

  x = GAME_STATUS_X + 2;
  y = GAME_STATUS_Y + 1;

  show_counter(mzx_world, "Gems",     x, y++, false);
  show_counter(mzx_world, "Ammo",     x, y++, false);
  show_counter(mzx_world, "Health",   x, y++, false);
  show_counter(mzx_world, "Lives",    x, y++, false);
  show_counter(mzx_world, "Lobombs",  x, y++, false);
  show_counter(mzx_world, "Hibombs",  x, y++, false);
  show_counter(mzx_world, "Coins",    x, y++, false);
  show_counter(mzx_world, "Score",    x, y++, false);

  // Bomb selection
  write_string("(cur.)", (x + 9), y - (mzx_world->bomb_type ? 3 : 4),
   MENU_COL, false);

  // Keys
  write_string("Keys", x, y, MENU_COL_COUNTER, false);

  for(i = 0; i < 8; i++)
  {
    if(keys[i] != NO_KEY)
      draw_char(KEY_CHAR, 0x10 + keys[i], (x + i + GAME_STATUS_INDENT), y);

    if(keys[i+8] != NO_KEY)
      draw_char(KEY_CHAR, 0x10 + keys[i+8], (x + i + GAME_STATUS_INDENT), y+1);
  }

  y += 2;

  // Custom status counters
  for(i = 0; i < NUM_STATUS_COUNTERS; i++)
  {
    char *name = mzx_world->status_counters_shown[i];
    if(name[0])
      show_counter(mzx_world, name, x, y, true);

    y++;
  }

  // Active effects
  y = GAME_STATUS_Y + GAME_STATUS_HEIGHT - 1;

  if(mzx_world->firewalker_dur > 0)
    write_string("-W-", x+4, y, 0x1C, false);

  if(mzx_world->freeze_time_dur > 0)
    write_string("-F-", x+8, y, 0x1B, false);

  if(mzx_world->slow_time_dur > 0)
    write_string("-S-", x+12, y, 0x1E, false);

  if(mzx_world->wind_dur > 0)
    write_string("-W-", x+16, y, 0x1F, false);

  if(mzx_world->blind_dur > 0)
    write_string("-B-", x+20, y, 0x19, false);
}

/**
 * Draw the game menu, adjusting its height and contents based on which
 * features are enabled.
 */

static void draw_game_menu(struct world *mzx_world)
{
  int height = GAME_MENU_HEIGHT;
  int x;
  int y;

  if(mzx_world->help_file)
    height += GAME_HELP_HEIGHT;

  if(debug_counters && mzx_world->editing)
    height += GAME_DEBUG_HEIGHT;

  draw_menu_box(
   GAME_MENU_X, GAME_MENU_Y, GAME_MENU_WIDTH, height,
   game_menu_title
  );

  x = GAME_MENU_X + 2;
  y = GAME_MENU_Y + 1;

  if(mzx_world->help_file)
  {
    write_string(game_menu_1, x, y, MENU_COL_TEXT, true);
    y += GAME_HELP_HEIGHT;
  }

  write_string(game_menu_2, x, y, MENU_COL_TEXT, true);
  y += GAME_TEXT_HEIGHT;

  if(debug_counters && mzx_world->editing)
  {
    write_string(game_menu_3, x, y, MENU_COL_TEXT, true);
    y += GAME_DEBUG_HEIGHT;
  }
}

/**
 * Draw the main menu, adjusting its height and contents based on which
 * features are enabled.
 */

static void draw_main_menu(struct world *mzx_world)
{
  int height = MAIN_MENU_HEIGHT;
  int x;
  int y;

  if(mzx_world->help_file)
    height += MAIN_HELP_HEIGHT;

  if(check_for_updates)
    height += MAIN_NETWORK_HEIGHT;

  if(edit_world)
    height += MAIN_EDITOR_HEIGHT;

  draw_menu_box(
   MAIN_MENU_X, MAIN_MENU_Y, MAIN_MENU_WIDTH, height,
   main_menu_title
  );

  x = MAIN_MENU_X + 2;
  y = MAIN_MENU_Y + 1;

  write_string(main_menu_1, x, y, MENU_COL_TEXT, true);
  y += 2;

  if(mzx_world->help_file)
  {
    write_string(main_menu_2, x, y, MENU_COL_TEXT, true);
    y += MAIN_HELP_HEIGHT;
  }

  write_string(main_menu_3, x, y, MENU_COL_TEXT, true);
  y += 4;

  if(check_for_updates)
  {
    write_string(main_menu_4, x, y, MENU_COL_TEXT, true);
    y += MAIN_NETWORK_HEIGHT;
  }

  if(edit_world)
  {
    write_string(main_menu_5, x, y, MENU_COL_TEXT, true);
    y += MAIN_EDITOR_HEIGHT;
  }

  write_string(main_menu_6, x, y, MENU_COL_TEXT, true);
  y += 1;
}

/**
 * Close the menu on an exit event or an return/escape press.
 * NOTE: In DOS versions of MZX, clicking the menus would close the
 * menu and trigger the clicked function. This feature was removed in
 * the port and has not yet been restored.
 */

static boolean menu_key(context *ctx, int *key)
{
  if(get_exit_status() || *key == IKEY_RETURN || *key == IKEY_ESCAPE)
  {
    destroy_context(ctx);
    return true;
  }
  return false;
}

/**
 * Restore the screen when a menu is closed.
 */

static void menu_destroy(context *ctx)
{
  restore_screen();
}

/**
 * Create and run the game menu context for gameplay. This menu also displays
 * a status screen with the player's current stats.
 */

void game_menu(context *parent)
{
  save_screen();
  draw_game_menu(parent->world);
  draw_game_status(parent->world);

  m_show();

  create_context(NULL, parent, CTX_GAME_MENU,
    NULL,
    NULL,
    NULL,
    menu_key,
    NULL,
    NULL,
    menu_destroy
  );
}

/**
 * Create and run the main menu context for the title screen.
 */

void main_menu(context *parent)
{
  save_screen();
  draw_main_menu(parent->world);

  m_show();

  create_context(NULL, parent, CTX_MAIN_MENU,
    NULL,
    NULL,
    NULL,
    menu_key,
    NULL,
    NULL,
    menu_destroy
  );
}
