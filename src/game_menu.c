/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

static const char *main_menu_title = " MegaZeux " VERSION " ";
#define MAIN_MENU_TITLE_X ((80 - strlen(main_menu_title)) / 2)

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

static const char game_menu_1[] =
 "F1    - Help\n";

static const char game_menu_2[] =
 "Enter - Menu/status\n"
 "Esc   - Exit to title\n"
 "F2    - Settings\n"
 "F3    - Save game\n"
 "F4    - Restore game\n"
 "F5/Ins- Toggle bomb type";

static const char game_menu_3[] =
 "F6    - Debug Menu";

static const char game_menu_4[] =
 "F9    - Quicksave\n"
 "F10   - Quickload\n"
 "Arrows- Move\n"
 "Space - Shoot (w/dir)\n"
 "Delete- Bomb";

static void show_counter(struct world *mzx_world, const char *str,
 int x, int y, int skip_if_zero)
{
  int counter_value = get_counter(mzx_world, str, 0);
  if((skip_if_zero) && (!counter_value))
    return;
  write_string(str, x, y, 27, 0); // Name

  write_number(counter_value, 31, x + 16, y, 1, 0, 10); // Value
}

// Show status screen
static void show_status(struct world *mzx_world)
{
  char *keys = mzx_world->keys;
  int i;

  draw_window_box(37, 4, 67, 21, 25, 16, 24, 1, 1);
  show_counter(mzx_world, "Gems", 39, 5, 0);
  show_counter(mzx_world, "Ammo", 39, 6, 0);
  show_counter(mzx_world, "Health", 39, 7, 0);
  show_counter(mzx_world, "Lives", 39, 8, 0);
  show_counter(mzx_world, "Lobombs", 39, 9, 0);
  show_counter(mzx_world, "Hibombs", 39, 10, 0);
  show_counter(mzx_world, "Coins", 39, 11, 0);
  show_counter(mzx_world, "Score", 39, 12, 0);
  write_string("Keys", 39, 13, 27, 0);

  for(i = 0; i < 8; i++) // Show keys
  {
    if(keys[i] != NO_KEY)
      draw_char('\x0c', 16 + keys[i], 55 + i, 13);
  }

  for(; i < 16; i++) // Show keys, 2nd row
  {
    if(keys[i] != NO_KEY)
      draw_char('\x0c', 16 + keys[i], 47 + i, 14);
  }

  // Show custom status counters
  for(i = 0; i < NUM_STATUS_COUNTERS; i++)
  {
    char *name = mzx_world->status_counters_shown[i];
    if(name[0])
      show_counter(mzx_world, name, 39, i + 15, 1);
  }

  // Show hi/lo bomb selection

  write_string("(cur.)", 48, 9 + mzx_world->bomb_type, 25, 0);
  // Show effects
  if(mzx_world->firewalker_dur > 0)
    write_string("-W-", 43, 21, 28, 0);

  if(mzx_world->freeze_time_dur > 0)
    write_string("-F-", 47, 21, 27, 0);

  if(mzx_world->slow_time_dur > 0)
    write_string("-S-", 51, 21, 30, 0);

  if(mzx_world->wind_dur > 0)
    write_string("-W-", 55, 21, 31, 0);

  if(mzx_world->blind_dur > 0)
    write_string("-B-", 59, 21, 25, 0);
}

void game_menu(struct world *mzx_world)
{
  int key, status;
  save_screen();

  draw_window_box(8, 4, 35, 18, 25, 16, 24, 1, 1);
  write_string(" Game Menu ", 17, 4, 30, 0);
  if(mzx_world->help_file)
    write_string(game_menu_1, 10, 5, 31, 1);
  write_string(game_menu_2, 10, 6, 31, 1);
  if(debug_counters && mzx_world->editing)
    write_string(game_menu_3, 10, 12, 31, 1);
  write_string(game_menu_4, 10, 13, 31, 1);

  show_status(mzx_world); // Status screen too

  force_release_all_keys();
  update_screen();
  m_show();

  do
  {
    update_event_status_delay();
    update_screen();

    key = get_key(keycode_internal_wrt_numlock);
    status = get_key_status(keycode_internal_wrt_numlock, key);

    if(get_exit_status())
      break;

  } while(
    (key != IKEY_RETURN && key != IKEY_ESCAPE) || (status != 1)
  );

  force_release_all_keys();
  restore_screen();
}

void main_menu(struct world *mzx_world)
{
  int key, status;
  save_screen();
  draw_window_box(28, 4, 51, 16, 25, 16, 24, 1, 1);
  write_string(main_menu_title, MAIN_MENU_TITLE_X, 4, 30, 0);
  write_string(main_menu_1, 30, 5, 31, 1);
  if(mzx_world->help_file)
    write_string(main_menu_2, 30, 7, 31, 1);
  write_string(main_menu_3, 30, 8, 31, 1);
  if(check_for_updates)
    write_string(main_menu_4, 30, 12, 31, 1);
  if(edit_world)
    write_string(main_menu_5, 30, 13, 31, 1);
  write_string(main_menu_6, 30, 15, 31, 1);

  force_release_all_keys();
  update_screen();
  m_show();

  do
  {
    update_event_status_delay();
    update_screen();
    key = get_key(keycode_internal_wrt_numlock);
    status = get_key_status(keycode_internal_wrt_numlock, key);

    if(get_exit_status())
      break;

  } while((key != IKEY_RETURN && key != IKEY_ESCAPE) || status!=1);

  force_release_all_keys();
  restore_screen();
  update_screen();
}
