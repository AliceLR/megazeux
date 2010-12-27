/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

#include "../platform.h"
#include "../helpsys.h"
#include "../scrdisp.h"
#include "../sfx.h"
#include "../counter.h"
#include "../game.h"
#include "../intake.h"
#include "../error.h"
#include "../idarray.h"
#include "../data.h"
#include "../const.h"
#include "../graphics.h"
#include "../window.h"
#include "../idput.h"
#include "../mzm.h"
#include "../event.h"
#include "../audio.h"
#include "../world.h"
#include "../util.h"
#include "debug.h"

#include "block.h"
#include "board.h"
#include "char_ed.h"
#include "edit.h"
#include "edit_di.h"
#include "fill.h"
#include "graphics.h"
#include "pal_ed.h"
#include "param.h"
#include "robot.h"
#include "sfx_edit.h"
#include "window.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#define EDIT_SCREEN_EXTENDED 24
#define EDIT_SCREEN_NORMAL   19

void load_editor_config(struct world *mzx_world, int *argc, char *argv[])
{
  default_editor_config(&mzx_world->editor_conf);
  set_editor_config_from_file(&mzx_world->editor_conf,
   mzx_res_get_by_id(CONFIG_TXT));
  set_editor_config_from_command_line(&mzx_world->editor_conf, argc, argv);
}

/* Edit menu- (w/box ends on sides) Current menu name is highlighted. The
  bottom section zooms to show a list of options for the current menu,
  although all keys are available at all times. PGUP/PGDN changes menu.
  The menus are shown in listed order. [draw] can be [text]. (in which case
  there is the words "Type to place text" after the ->) [draw] can also be
  [place] or [block]. The options DRAW, __TEXT, DEBUG, BLOCK, MOD, and DEF.
  COLORS are highlighted white (instead of green) when active. Debug mode
  pops up a small window in the lower left corner, which rushes to the right
  side and back when the cursor reaches 3/4 of the way towards it,
  horizontally. The menu itself is 15 lines wide and 7 lines high. The
  board display, w/o the debug menu, is 19 lines high and 80 lines wide.
  Note: SAVE is highlighted white if the world has changed since last load
  or save.

  The commands with ! instead of : have not been implemented yet.

  The menu line turns to "Overlay editing-" and the lower portion shows
  overlay commands on overlay mode.
*/

static void synchronize_board_values(struct world *mzx_world,
 struct board **src_board, int *board_width, int *board_height,
 char **level_id, char **level_param, char **level_color,
 char **overlay, char **overlay_color)
{
  *src_board = mzx_world->current_board;
  *board_width = (*src_board)->board_width;
  *board_height = (*src_board)->board_height;
  *level_id = (*src_board)->level_id;
  *level_param = (*src_board)->level_param;
  *level_color = (*src_board)->level_color;
  *overlay = (*src_board)->overlay;
  *overlay_color = (*src_board)->overlay_color;
  clear_screen_no_update();
}

static void fix_scroll(int *cursor_board_x, int *cursor_board_y,
 int *scroll_x, int *scroll_y, int cursor_x, int cursor_y,
 int board_width, int board_height, int edit_screen_height)
{
  if(*scroll_x + 80 > board_width)
  {
    *scroll_x = (board_width - 80);
    if(*scroll_x < 0)
      *scroll_x = 0;
  }

  if(*scroll_y + edit_screen_height > board_height)
  {
    *scroll_y = (board_height - edit_screen_height);
    if(*scroll_y < 0)
      *scroll_y = 0;
  }

  if(*cursor_board_x >= board_width)
    *cursor_board_x = (board_width - 1);

  if(*cursor_board_y >= board_height)
    *cursor_board_y = (board_height - 1);
}

static void fix_board(struct world *mzx_world, int new_board)
{
  mzx_world->current_board_id = new_board;
  mzx_world->current_board = mzx_world->board_list[new_board];
}

static void fix_mod(struct world *mzx_world, struct board *src_board)
{
  load_board_module(src_board);
  strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
}

#define NUM_MENUS 6

static const char menu_names[NUM_MENUS][9] =
{
  " WORLD ", " BOARD ", " THING ", " CURSOR ", " SHOW ", " MISC "
};

static const char menu_positions[] =
  "11111112222222333333344444444555555666666";

static const char draw_names[7][10] =
{
  " Current:", " Drawing:", "    Text:", "   Block:", "   Block:", " Import:"
};

static const char *const menu_lines[NUM_MENUS][2]=
{
  {
    " L:Load\tS:Save  G:Global Info    Alt+R:Restart  Alt+T:Test",
    " Alt+S:Status Info  Alt+C:Char Edit  Alt+E:Palette  Alt+F:Sound Effects"
  },
  {
    " Alt+Z:Clear   X:Exits\t  Alt+P:Size/Pos  I:Info  A:Add  D:Delete  V:View",
    " Alt+I:Import  Alt+X:Export  B:Select board  Alt+O:Edit Overlay"
  },
  {
    " F3:Terrain  F4:Item\tF5:Creature  F6:Puzzle  F7:Transport  F8:Element",
    " F9:Misc\tF10:Objects  P:Parameter  C:Color"
  },
  {
    " \x12\x1d:Move  Space:Place  Enter:Modify+Grab  Alt+M:Modify  Ins:Grab  Del:Delete",
    " F:Fill   Tab:Draw\tF2:Text\t\t  Alt+B:Block   Alt+\x12\x1d:Move 10"
  },
  {
    " Shift+F1:Show InvisWalls  Shift+F2:Show robots  Shift+F3:Show Fakes",
    " Shift+F4:Show Spaces"
  },
  {
    " F1:Help    Home\\End:Corner  Alt+A:Select Char Set  Alt+D:Default Colors",
    " ESC:Exit   Alt+L:Test SAM   Alt+Y:Debug Mode\t  Alt+N:Music    *:Mod *"
  }
};

static const char *const overlay_menu_lines[4] =
{
  " OVERLAY EDITING- (Alt+O to end)",
  " \x12\x1d:Move  Space:Place  Ins:Grab  Enter:Character  Del:Delete\t   F:Fill",
  " C:Color  Alt+B:Block  Tab:Draw  Alt+\x12\x1d:Move 10   Alt+S:Show level  F2:Text",
  "Character"
};

static const char tmenu_num_choices[8] = { 17, 14, 18, 8, 6, 11, 12, 10 };

static const char *const tmenu_titles[8] =
{
  "Terrains", "Items", "Creatures", "Puzzle Pieces",
  "Transport", "Elements", "Miscellaneous", "Objects"
};

// Each 'item' is 20 char long, including '\0'.
static const char *const thing_menus[8][20] =
{
  // Terrain (F3)
  {
    "Space           ~1 ",
    "Normal          ~E\xB2",
    "Solid           ~D\xDB",
    "Tree            ~A\x06",
    "Line            ~B\xCD",
    "Custom Block    ~F?",
    "Breakaway       ~C\xB1",
    "Custom Break    ~F?",
    "Fake            ~9\xB2",
    "Carpet          ~4\xB1",
    "Floor           ~6\xB0",
    "Tiles           ~0\xFE",
    "Custom Floor    ~F?",
    "Web             ~7\xC5",
    "Thick Web       ~7\xCE",
    "Forest          ~2\xB2",
    "Invis. Wall     ~1 "
  },

  // Item (F4)
  {
    "Gem             ~A\x04",
    "Magic Gem       ~E\x04",
    "Health          ~C\x03",
    "Ring            ~E\x09",
    "Potion          ~B\x96",
    "Energizer       ~D\x07",
    "Ammo            ~3\xA4",
    "Bomb            ~8\x0B",
    "Key             ~F\x0C",
    "Lock            ~F\x0A",
    "Coin            ~E\x07",
    "Life            ~B\x9B",
    "Pouch           ~7\x9F",
    "Chest           ~6\xA0"
  },

  // Creature (F5)
  {
    "Snake           ~2\xE7",
    "Eye             ~F\xE8",
    "Thief           ~C\x05",
    "Slime Blob      ~A*",
    "Runner          ~4\x02",
    "Ghost           ~7\xE6",
    "Dragon          ~4\x15",
    "Fish            ~E\xE0",
    "Shark           ~7\x7F",
    "Spider          ~7\x95",
    "Goblin          ~D\x05",
    "Spitting Tiger  ~B\xE3",
    "Bear            ~6\xAC",
    "Bear Cub        ~6\xAD",
    "Lazer Gun       ~4\xCE",
    "Bullet Gun      ~F\x1B",
    "Spinning Gun    ~F\x1B",
    "Missile Gun     ~8\x11"
  },

  // Puzzle (F6)
  {
    "Boulder         ~7\xE9",
    "Crate           ~6\xFE",
    "Custom Push     ~F?",
    "Box             ~E\xFE",
    "Custom Box      ~F?",
    "Pusher          ~D\x1F",
    "Slider NS       ~D\x12",
    "Slider EW       ~D\x1D"
  },

  // Tranport (F7)
  {
    "Stairs          ~A\xA2",
    "Cave            ~6\xA1",
    "Transport       ~E<",
    "Whirlpool       ~B\x97",
    "CWRotate        ~9/",
    "CCWRotate       ~9\\"
  },

  // Element (F8)
  {
    "Still Water     ~9\xB0",
    "N Water         ~9\x18",
    "S Water         ~9\x19",
    "E Water         ~9\x1A",
    "W Water         ~9\x1B",
    "Ice             ~B\xFD",
    "Lava            ~C\xB2",
    "Fire            ~E\xB1",
    "Goop            ~8\xB0",
    "Lit Bomb        ~8\xAB",
    "Explosion       ~E\xB1"
  },

  // Misc (F9)
  {
    "Door            ~6\xC4",
    "Gate            ~8\x16",
    "Ricochet Panel  ~9/",
    "Ricochet        ~A*",
    "Mine            ~C\x8F",
    "Spike           ~8\x1E",
    "Custom Hurt     ~F?",
    "Text            ~F?",
    "N Moving Wall   ~F?",
    "S Moving Wall   ~F?",
    "E Moving Wall   ~F?",
    "W Moving Wall   ~F?"
  },

  // Objects (F10)
  {
    "Robot           ~F?",
    "Pushable Robot  ~F?",
    "Player          ~B\x02",
    "Scroll          ~F\xE4",
    "Sign            ~6\xE2",
    "Sensor          ~F?",
    "Bullet          ~F\xF9",
    "Missile         ~8\x10",
    "Seeker          ~A/",
    "Shooting Fire   ~E\x0F"
  }
};

static const enum thing tmenu_thing_ids[8][18] =
{
  // Terrain (F3)
  {
    SPACE, NORMAL, SOLID, TREE, LINE, CUSTOM_BLOCK, BREAKAWAY,
    CUSTOM_BREAK, FAKE, CARPET, FLOOR, TILES, CUSTOM_FLOOR, WEB,
    THICK_WEB, FOREST, INVIS_WALL
  },
  // Item (F4)
  {
    GEM, MAGIC_GEM, HEALTH, RING, POTION, ENERGIZER, AMMO, BOMB, KEY,
    LOCK, COIN, LIFE, POUCH, CHEST
  },

  // Creature (F5)
  {
    SNAKE, EYE, THIEF, SLIMEBLOB, RUNNER, GHOST, DRAGON, FISH, SHARK,
    SPIDER, GOBLIN, SPITTING_TIGER, BEAR, BEAR_CUB, LAZER_GUN,
    BULLET_GUN, SPINNING_GUN, MISSILE_GUN
  },

  // Puzzle (F6)
  {
    BOULDER, CRATE, CUSTOM_PUSH, BOX, CUSTOM_BOX, PUSHER, SLIDER_NS,
    SLIDER_EW
  },

  // Tranport (F7)
  {
    STAIRS, CAVE, TRANSPORT, WHIRLPOOL_1, CW_ROTATE, CCW_ROTATE
  },

  // Element (F8)
  {
    STILL_WATER, N_WATER, S_WATER, E_WATER, W_WATER, ICE, LAVA, FIRE,
    GOOP, LIT_BOMB, EXPLOSION
  },

  // Misc (F9)
  {
    DOOR, GATE, RICOCHET_PANEL, RICOCHET, MINE, SPIKE, CUSTOM_HURT,
    __TEXT, N_MOVING_WALL, S_MOVING_WALL, E_MOVING_WALL, W_MOVING_WALL
  },

  // Objects (F10)
  {
    ROBOT, ROBOT_PUSHABLE, PLAYER, SCROLL, SIGN, SENSOR, BULLET,
    MISSILE, SEEKER, SHOOTING_FIRE
  }
};

// Default colors
static const char def_colors[128] =
{
  7 , 0 , 0 , 10, 0 , 0 , 0 , 0 , 7 , 6 , 0 , 0 , 0 , 0 , 0 , 0 ,   // 0x00-0x0F
  0 , 0 , 7 , 7 , 25, 25, 25, 25, 25, 59, 76, 6 , 0 , 0 , 12, 14,   // 0x10-0x1F
  11, 15, 24, 3 , 8 , 8 , 239,0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 8 ,   // 0x20-0x2F
  8 , 0 , 14, 0 , 0 , 0 , 0 , 7 , 0 , 0 , 0 , 0 , 4 , 15, 8 , 12,   // 0x30-0x3F
  0 , 2 , 11, 31, 31, 31, 31, 0 , 9 , 10, 12, 8 , 0 , 0 , 14, 10,   // 0x40-0x4F
  2 , 15, 12, 10, 4 , 7 , 4 , 14, 7 , 7 , 2 , 11, 15, 15, 6 , 6 ,   // 0x50-0x5F
  0 , 8 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,   // 0x60-0x6F
  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 6 , 15, 27    // 0x70-0x7F
};

static const char *const mod_ext[] =
{
  ".xm", ".s3m", ".mod", ".med", ".mtm", ".stm", ".it", ".669", ".ult",
  ".wav", ".dsm", ".far", ".okt", ".amf", ".ogg", NULL
};

static const char drawmode_help[5][32] =
{
  "Type to place text",
  "Press ENTER on other corner",
  "Press ENTER to place block",
  "Press ENTER to place MZM"
};

#define num2hex(x) ((x) > 9 ? 87 + (x) : 48 + (x))

static void write_hex_byte(char byte, char color, int x, int y)
{
  int t1, t2;
  t1 = (byte & 240) >> 4;
  t1 = num2hex(t1);
  t2 = byte & 15;
  t2 = num2hex(t2);
  draw_char(t1, color, x, y);
  draw_char(t2, color, x + 1, y);
}

// It's important that after changing the param the thing is placed (using
// place_current_at_xy) so that scrolls/sensors/robots get copied.

static int change_param(struct world *mzx_world, enum thing id, int param,
 struct robot *copy_robot, struct scroll *copy_scroll,
 struct sensor *copy_sensor)
{
  if(id == SENSOR)
  {
    return edit_sensor(mzx_world, copy_sensor);
  }
  else

  if(is_robot(id))
  {
    return edit_robot(mzx_world, copy_robot);
  }
  else

  if(is_signscroll(id))
  {
    return edit_scroll(mzx_world, copy_scroll);
  }
  else
  {
    return edit_param(mzx_world, id, param);
  }
}

int place_current_at_xy(struct world *mzx_world, enum thing id, int color,
 int param, int x, int y, struct robot *copy_robot,
 struct scroll *copy_scroll, struct sensor *copy_sensor, int overlay_edit)
{
  struct board *src_board = mzx_world->current_board;
  int offset = x + (y * src_board->board_width);
  char *level_id = src_board->level_id;
  enum thing old_id = (enum thing)level_id[offset];

  if(!overlay_edit)
  {
    if(old_id != PLAYER)
    {
      if(id == PLAYER)
      {
        id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
        mzx_world->player_x = x;
        mzx_world->player_y = y;
      }
      else

      if(is_robot(id))
      {
        if(is_robot(old_id))
        {
          int old_param = src_board->level_param[offset];
          replace_robot_source(src_board, copy_robot, old_param);
          src_board->level_color[offset] = color;
          src_board->level_id[offset] = id;
          return old_param;
        }

        param = duplicate_robot_source(src_board, copy_robot, x, y);
        if(param != -1)
        {
          (src_board->robot_list[param])->xpos = x;
          (src_board->robot_list[param])->ypos = y;
          (src_board->robot_list[param])->used = 1;
        }
      }
      else

      if(is_signscroll(id))
      {
        if(is_signscroll(old_id))
        {
          int old_param = src_board->level_param[offset];
          replace_scroll(src_board, copy_scroll, old_param);
          src_board->level_color[offset] = color;
          src_board->level_id[offset] = id;
          return old_param;
        }

        param = duplicate_scroll(src_board, copy_scroll);
        if(param != -1)
          (src_board->scroll_list[param])->used = 1;
      }
      else

      if(id == SENSOR)
      {
        if(old_id == SENSOR)
        {
          int old_param = src_board->level_param[offset];
          replace_sensor(src_board, copy_sensor, old_param);
          src_board->level_color[offset] = color;
          return old_param;
        }

        param = duplicate_sensor(src_board, copy_sensor);
        if(param != -1)
          (src_board->sensor_list[param])->used = 1;
      }

      if(param != -1)
      {
        place_at_xy(mzx_world, id, color, param, x, y);
      }
    }
  }
  else
  {
    int offset = x + (y * src_board->board_width);
    src_board->overlay[offset] = param;
    src_board->overlay_color[offset] = color;
  }

  return param;
}

static void grab_at_xy(struct world *mzx_world, enum thing *new_id,
 int *new_color, int *new_param, struct robot *copy_robot,
 struct scroll *copy_scroll, struct sensor *copy_sensor,
 int x, int y, int overlay_edit)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int offset = x + (y * board_width);
  enum thing old_id = *new_id;
  int old_param = *new_param;

  if(!overlay_edit)
  {
    enum thing grab_id = (enum thing)src_board->level_id[offset];
    int grab_param = src_board->level_param[offset];

    // Hack.
    if(grab_id != PLAYER)
      *new_color = src_board->level_color[offset];
    else
      *new_color = get_id_color(src_board, offset);

    // First, clear the existing copies, unless the new id AND param
    // matches, because in that case it's the same thing

    if(is_robot(old_id) &&
     ((old_id != grab_id) || (old_param != grab_param)))
    {
      clear_robot_contents(copy_robot);
    }

    if(is_signscroll(old_id) &&
     ((old_id != grab_id) || (old_param != grab_param)))
    {
      clear_scroll_contents(copy_scroll);
    }

    if(is_robot(grab_id))
    {
      struct robot *src_robot = src_board->robot_list[grab_param];
      duplicate_robot_direct_source(src_robot, copy_robot, 0, 0);
    }
    else

    if(is_signscroll(grab_id))
    {
      struct scroll *src_scroll = src_board->scroll_list[grab_param];
      duplicate_scroll_direct(src_scroll, copy_scroll);
    }
    else

    if(grab_id == SENSOR)
    {
      struct sensor *src_sensor = src_board->sensor_list[grab_param];
      duplicate_sensor_direct(src_sensor, copy_sensor);
    }

    *new_id = grab_id;
    *new_param = grab_param;
  }
  else
  {
    *new_param = src_board->overlay[offset];
    *new_color = src_board->overlay_color[offset];
  }
}

static void thing_menu(struct world *mzx_world, int menu_number,
 enum thing *new_id, int *new_color, int *new_param,
 struct robot *copy_robot, struct scroll *copy_scroll,
 struct sensor *copy_sensor, int x, int y)
{
  struct board *src_board = mzx_world->current_board;
  int color, param, chosen, old_id = *new_id;
  enum thing id;

  id = (enum thing)src_board->level_id[x + (y * src_board->board_width)];
  if(id == PLAYER)
  {
    error("Cannot overwrite the player- move it first", 0, 8, 0x0000);
    return;
  }

  cursor_off();
  chosen =
   list_menu(thing_menus[menu_number], 20, tmenu_titles[menu_number], 0,
   tmenu_num_choices[menu_number], 27, 0);

  if(chosen >= 0)
  {
    id = tmenu_thing_ids[menu_number][chosen];

    if(def_colors[id])
    {
      color = def_colors[id];
    }
    else
    {
      color = *new_color;
    }

    // Perhaps put a new blank scroll, robot, or sensor in one of the copies
    if(id == SENSOR)
    {
      create_blank_sensor_direct(copy_sensor);
    }
    else

    if(is_robot(id))
    {
      if(is_robot(old_id))
        clear_robot_contents(copy_robot);

      create_blank_robot_direct(copy_robot, x, y);
    }

    if(is_signscroll(id))
    {
      if(is_signscroll(old_id))
        clear_scroll_contents(copy_scroll);

      create_blank_scroll_direct(copy_scroll);
    }

    param =
     change_param(mzx_world, id, -1, copy_robot, copy_scroll, copy_sensor);

    if(param >= 0)
    {
      param = place_current_at_xy(mzx_world, id, color, param,
       x, y, copy_robot, copy_scroll, copy_sensor, 0);

      *new_id = id;
      *new_param = param;
      *new_color = color;
    }
  }
}

static void draw_edit_window(struct board *src_board, int array_x, int array_y,
 int window_height)
{
  int viewport_width = 80, viewport_height = window_height;
  int x, y;
  int a_x, a_y;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  if(board_width < viewport_width)
    viewport_width = board_width;
  if(board_height < viewport_height)
    viewport_height = board_height;

  for(y = 0, a_y = array_y; y < viewport_height; y++, a_y++)
  {
    for(x = 0, a_x = array_x; x < viewport_width; x++, a_x++)
    {
      id_put(src_board, x, y, a_x, a_y, a_x, a_y);
    }
  }
}

static void flash_thing(struct world *mzx_world, int start, int end,
 int flash_one, int flash_two, int scroll_x, int scroll_y,
 int edit_screen_height)
{
  struct board *src_board = mzx_world->current_board;
  int backup[32];
  int i, i2;

  cursor_off();

  for(i = 0, i2 = start; i2 < end + 1; i++, i2++)
  {
    backup[i] = id_chars[i2];
  }

  src_board->overlay_mode |= 0x80;

  do
  {
    for(i = start; i < end + 1; i++)
    {
      id_chars[i] = flash_one;
    }

    draw_edit_window(src_board, scroll_x, scroll_y, edit_screen_height);

    update_screen();
    delay(UPDATE_DELAY * 2);

    for(i = start; i < end + 1; i++)
    {
      id_chars[i] = flash_two;
    }

    draw_edit_window(src_board, scroll_x, scroll_y, edit_screen_height);

    update_screen();
    delay(UPDATE_DELAY * 2);

    update_event_status();
  } while(!get_key(keycode_internal));

  update_event_status_delay();

  src_board->overlay_mode &= 0x7F;

  for(i = 0, i2 = start; i2 < end + 1; i++, i2++)
  {
    id_chars[i2] = backup[i];
  }
}

static void clear_layer_block(int src_x, int src_y, int width,
 int height, char *dest_char, char *dest_color, int dest_width)
{
  int dest_offset = src_x + (src_y * dest_width);
  int dest_skip = dest_width - width;
  int i, i2;

  for(i = 0; i < height; i++, dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, dest_offset++)
    {
      // Now perform the copy to the buffer sections
      dest_char[dest_offset] = 32;
      dest_color[dest_offset] = 7;
    }
  }
}

static void clear_board_block(struct board *src_board, int x, int y,
 int width, int height)
{
  int board_width = src_board->board_width;
  int dest_offset = x + (y * board_width);
  int dest_skip = board_width - width;
  int src_offset = 0;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  enum thing dest_id;
  int dest_param;
  int i, i2;

  for(i = 0; i < height; i++, dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      dest_id = (enum thing)level_id[dest_offset];
      if(dest_id != PLAYER)
      {
        dest_param = level_param[dest_offset];

        if(dest_id == SENSOR)
        {
          clear_sensor_id(src_board, dest_param);
        }
        else

        if(is_signscroll(dest_id))
        {
          clear_scroll_id(src_board, dest_param);
        }
        else

        if(is_robot(dest_id))
        {
          clear_robot_id(src_board, dest_param);
        }

        level_id[dest_offset] = (char)SPACE;
        level_param[dest_offset] = 0;
        level_color[dest_offset] = 7;
      }

      level_under_id[dest_offset] = (char)SPACE;
      level_under_param[dest_offset] = 0;
      level_under_color[dest_offset] = 7;
    }
  }
}

static void draw_menu_status(int overlay_edit, int line, int draw_mode,
 int current_color, int current_id, char sensor_char, char robot_char,
 char *level_id, char *level_param, int current_param, struct board *src_board)
{
  int display_next_pos;

  write_string(draw_names[draw_mode], 42, line, EC_MODE_STR, 0);
  display_next_pos = (int)strlen(draw_names[draw_mode]) + 42;

  if(draw_mode > 1)
  {
    write_string(drawmode_help[draw_mode - 2],
     (Uint32)display_next_pos, line, EC_MODE_HELP, 0);
  }
  else
  {
    int display_char, display_color = current_color;

    if(!overlay_edit)
    {
      if(current_id == SENSOR)
      {
        display_char = sensor_char;
      }
      else

      if(is_robot(current_id))
      {
        display_char = robot_char;
      }
      else
      {
        int temp_char = level_id[0];
        int temp_param = level_param[0];
        level_id[0] = current_id;
        level_param[0] = current_param;

        display_char = get_id_char(src_board, 0);

        level_id[0] = temp_char;
        level_param[0] = temp_param;
      }
    }
    else
    {
      display_char = current_param;
    }

    draw_char(' ', 7, display_next_pos, line);
    draw_char_ext(display_char, display_color,
     display_next_pos + 1, line, 0, 0);
    draw_char(' ', 7, display_next_pos + 2, line);

    display_next_pos += 4;
    draw_char('(', EC_CURR_THING, display_next_pos, line);
    draw_color_box(display_color, 0, display_next_pos + 1, line, 80);
    display_next_pos += 5;

    if(overlay_edit)
    {
      write_string("Character", display_next_pos, line, EC_CURR_THING, 0);
      display_next_pos += 9;
      write_hex_byte(current_param, EC_CURR_PARAM,
       display_next_pos + 1, line);
      display_next_pos += 3;
    }
    else
    {
      write_string(thing_names[current_id], display_next_pos, line,
       EC_CURR_THING, 0);
      display_next_pos += (int)strlen(thing_names[current_id]);
      draw_char('p', EC_CURR_PARAM, display_next_pos + 1, line);
      write_hex_byte(current_param, EC_CURR_PARAM,
      display_next_pos + 2, line);
      display_next_pos += 4;
    }

    draw_char(')', EC_CURR_THING, display_next_pos, line);
  }
}

static void draw_menu_normal(int overlay_edit, int draw_mode, int current_menu,
 int current_color, int current_id, char sensor_char, char robot_char,
 char *level_id, char *level_param, int current_param, struct board *src_board)
{
  draw_window_box(0, 19, 79, 24, EC_MAIN_BOX, EC_MAIN_BOX_DARK,
   EC_MAIN_BOX_CORNER, 0, 1);
  draw_window_box(0, 21, 79, 24, EC_MAIN_BOX, EC_MAIN_BOX_DARK,
   EC_MAIN_BOX_CORNER, 0, 1);

  if(!overlay_edit)
  {
    int i, write_color, x;
    x = 1; // X position

    for(i = 0; i < NUM_MENUS; i++)
    {
      if(i == current_menu)
        write_color = EC_CURR_MENU_NAME;
      else
        write_color = EC_MENU_NAME; // Pick the color

      // Write it
      write_string(menu_names[i], x, 20, write_color, 0);
      // Add to x
      x += (int)strlen(menu_names[i]);
    }

    write_string(menu_lines[current_menu][0], 1, 22, EC_OPTION, 1);
    write_string(menu_lines[current_menu][1], 1, 23, EC_OPTION, 1);
  }
  else
  {
    write_string(overlay_menu_lines[0], 1, 20, EC_MENU_NAME, 1);
    write_string(overlay_menu_lines[1], 1, 22, EC_OPTION, 1);
    write_string(overlay_menu_lines[2], 1, 23, EC_OPTION, 1);
  }

  draw_menu_status(overlay_edit, EDIT_SCREEN_NORMAL + 1, draw_mode,
   current_color, current_id, sensor_char, robot_char, level_id,
   level_param, current_param, src_board);

  draw_char(196, EC_MAIN_BOX_CORNER, 78, 21);
  draw_char(217, EC_MAIN_BOX_DARK, 79, 21);
}

static void draw_menu_minimal(int overlay_edit, int draw_mode,
 int current_color, int current_id, char sensor_char, char robot_char,
 char *level_id, char *level_param, int current_param,
 struct board *src_board, int cursor_board_x, int cursor_board_y)
{
  int i;

  for(i = 0; i < 80; i++)
    draw_char_ext(' ', EC_MAIN_BOX, i, EDIT_SCREEN_EXTENDED, 256, 16);

  write_string("Alt+H: Toggle Help", 3, EDIT_SCREEN_EXTENDED, EC_OPTION, 1);

  write_string("X/Y:      /     ", 3+21, EDIT_SCREEN_EXTENDED,
   EC_MODE_STR, 1);

  write_number(cursor_board_x, 31, 3+21+5, EDIT_SCREEN_EXTENDED, 5, 0, 10);
  write_number(cursor_board_y, 31, 3+21+11, EDIT_SCREEN_EXTENDED, 5, 0, 10);

  draw_menu_status(overlay_edit, EDIT_SCREEN_EXTENDED, draw_mode,
   current_color, current_id, sensor_char, robot_char, level_id,
   level_param, current_param, src_board);
}

static void __edit_world(struct world *mzx_world)
{
  struct board *src_board;
  struct robot copy_robot;
  struct scroll copy_scroll;
  struct sensor copy_sensor;

  int i;
  int cursor_board_x = 0, cursor_board_y = 0;
  int cursor_x = 0, cursor_y = 0;
  int scroll_x = 0, scroll_y = 0;
  enum thing current_id = SPACE;
  int current_color = 7;
  int current_param = 0;
  int board_width, board_height;
  int key;
  int draw_mode = 0;
  int overlay_edit = 0;
  int current_menu = 0;
  int show_level = 1;
  int block_x = -1, block_y = -1;
  int block_dest_x = -1, block_dest_y = -1;
  int block_command = -1;
  struct board *block_board = NULL;
  int text_place;
  int text_start_x = -1;
  int modified = 0;
  int debug_x = 60;
  int saved_overlay_mode;
  int edit_screen_height;
  int copy_repeat_width = -1;
  int copy_repeat_height = -1;
  int backup_count = mzx_world->editor_conf.backup_count;
  unsigned int backup_interval = mzx_world->editor_conf.backup_interval;
  char *backup_name = mzx_world->editor_conf.backup_name;
  char *backup_ext = mzx_world->editor_conf.backup_ext;
  int backup_timestamp = get_ticks();
  int backup_num = 0;
  char *level_id;
  char *level_param;
  char *level_color;
  char *overlay;
  char *overlay_color;
  char current_world[128];
  char mzm_name_buffer[128];
  char current_listening_dir[MAX_PATH];
  char current_listening_mod[512];
  int listening_flag = 0;
  int saved_scroll_x[16] = { 0 };
  int saved_scroll_y[16] = { 0 };
  int saved_cursor_x[16] = { 0 };
  int saved_cursor_y[16] = { 0 };
  int saved_board[16] = { 0 };
  int saved_debug_x[16] =
   { 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60 };

  const char *mzb_ext[] = { ".MZB", NULL };
  const char *sfx_ext[] = { ".SFX", NULL };
  const char *chr_ext[] = { ".CHR", NULL };
  const char *pal_ext[] = { ".PAL", NULL };
  const char *mzm_ext[] = { ".MZM", NULL };

  getcwd(current_listening_dir, MAX_PATH);

  chdir(config_dir);
  set_config_from_file(&(mzx_world->conf), "editor.cnf");
  chdir(current_listening_dir);

  current_world[0] = 0;

  copy_robot.used = 0;
  copy_sensor.used = 0;
  copy_scroll.used = 0;

  mzx_world->version = WORLD_VERSION;

  set_palette_intensity(100);

  if(mzx_world->active)
  {
    clear_world(mzx_world);
    clear_global_data(mzx_world);
  }
  mzx_world->active = 1;

  create_blank_world(mzx_world);
  m_show();

  end_module();

  synchronize_board_values(mzx_world, &src_board, &board_width, &board_height,
   &level_id, &level_param, &level_color, &overlay, &overlay_color);

  update_screen();

  default_palette();
  save_editor_palette();
  insta_fadein();

  if(mzx_world->editor_conf.bedit_hhelp)
    edit_screen_height = EDIT_SCREEN_EXTENDED;
  else
    edit_screen_height = EDIT_SCREEN_NORMAL;

  do
  {
    find_player(mzx_world);

    if((backup_count) &&
     ((get_ticks() - backup_timestamp) > (backup_interval * 1000)))
    {
      char backup_name_formatted[512];
      sprintf(backup_name_formatted,
       "%s%d%s", backup_name, backup_num + 1, backup_ext);

      save_world(mzx_world, backup_name_formatted, 0);
      backup_num = (backup_num + 1) % backup_count;
      backup_timestamp = get_ticks();
    }

    cursor_x = cursor_board_x - scroll_x;
    cursor_y = cursor_board_y - scroll_y;

    if(current_param == -1)
    {
      current_id = SPACE;
      current_param = 0;
      current_color = 7;
    }

    cursor_solid();
    move_cursor(cursor_x, cursor_y);

    saved_overlay_mode = src_board->overlay_mode;

    clear_screen_no_update();

    if(!overlay_edit)
    {
      src_board->overlay_mode = 0;
      draw_edit_window(src_board, scroll_x, scroll_y,
       edit_screen_height);
    }
    else
    {
      src_board->overlay_mode = 1;
      if(!show_level)
      {
        src_board->overlay_mode |= 0x40;
        draw_edit_window(src_board, scroll_x, scroll_y,
         edit_screen_height);
        src_board->overlay_mode ^= 0x40;
      }
      else
      {
        draw_edit_window(src_board, scroll_x, scroll_y,
         edit_screen_height);
      }
    }
    src_board->overlay_mode = saved_overlay_mode;

    if(edit_screen_height == EDIT_SCREEN_NORMAL)
    {
      draw_menu_normal(overlay_edit, draw_mode, current_menu, current_color,
       current_id, copy_sensor.sensor_char, copy_robot.robot_char,
       level_id, level_param, current_param, src_board);
    }
    else
    {
      draw_menu_minimal(overlay_edit, draw_mode, current_color, current_id,
       copy_sensor.sensor_char, copy_robot.robot_char, level_id, level_param,
       current_param, src_board, cursor_board_x, cursor_board_y);
    }

    // Highlight block for draw mode 3
    if(draw_mode == 3)
    {
      int block_screen_x = block_x - scroll_x;
      int block_screen_y = block_y - scroll_y;
      int start_x, start_y;
      int block_width, block_height;

      if(block_screen_x < 0)
        block_screen_x = 0;

      if(block_screen_y < 0)
        block_screen_y = 0;

      if(block_screen_x >= 80)
        block_screen_x = 79;

      if(block_screen_y >= edit_screen_height)
        block_screen_y = edit_screen_height - 1;

      if(block_screen_x < cursor_x)
      {
        start_x = block_screen_x;
        block_width = cursor_x - block_screen_x + 1;
      }
      else
      {
        start_x = cursor_x;
        block_width = block_screen_x - cursor_x + 1;
      }

      if(block_screen_y < cursor_y)
      {
        start_y = block_screen_y;
        block_height = cursor_y - block_screen_y + 1;
      }
      else
      {
        start_y = cursor_y;
        block_height = block_screen_y - cursor_y + 1;
      }

      for(i = 0; i < block_height; i++)
      {
        color_line(block_width, start_x, start_y + i, 0x9F);
      }
    }

    if(debug_mode)
    {
      draw_debug_box(mzx_world, debug_x, edit_screen_height - 6,
       cursor_board_x, cursor_board_y);
    }

    text_place = 0;

    update_screen();

    update_event_status_delay();
    key = get_key(keycode_internal);

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_y == 20) && (edit_screen_height < 20))
      {
        if((mouse_x >= 1) && (mouse_x <= 41))
        {
          current_menu = menu_positions[mouse_x - 1] - '1';
        }
        else

        if((mouse_x >= 56) && (mouse_x <= 58))
        {
          int new_color;
          cursor_off();
          new_color = color_selection(current_color, 0);
          if(new_color >= 0)
            current_color = new_color;
        }
      }
      else

      if(mouse_y < edit_screen_height)
      {
        cursor_board_x = mouse_x + scroll_x;
        cursor_board_y = mouse_y + scroll_y;

        if((mouse_x >= debug_x) && (mouse_x < debug_x + 20))
        {
          if(debug_x == 60)
            debug_x = 0;
          else
            debug_x = 60;
        }

        if((cursor_board_x >= board_width) ||
         (cursor_board_y >= board_height))
        {
          if(cursor_board_x >= board_width)
            cursor_board_x = board_width - 1;

          if(cursor_board_y >= board_height)
            cursor_board_y = board_height - 1;
        }
        else

        if(get_mouse_status() & MOUSE_BUTTON(MOUSE_BUTTON_RIGHT))
        {
          grab_at_xy(mzx_world, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y, overlay_edit);
        }
        else
        {
          current_param = place_current_at_xy(mzx_world, current_id,
           current_color, current_param, cursor_board_x, cursor_board_y,
           &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
        }
      }
    }

    if((key >= IKEY_0) && (key <= IKEY_9))
    {
      int s_num = key - IKEY_0;

      if(get_ctrl_status(keycode_internal))
      {
        saved_cursor_x[s_num] = cursor_board_x;
        saved_cursor_y[s_num] = cursor_board_y;
        saved_scroll_x[s_num] = scroll_x;
        saved_scroll_y[s_num] = scroll_y;
        saved_debug_x[s_num] = debug_x;
        saved_board[s_num] = mzx_world->current_board_id;
      }
      else

      if(get_alt_status(keycode_internal))
      {
        cursor_board_x = saved_cursor_x[s_num];
        cursor_board_y = saved_cursor_y[s_num];
        scroll_x = saved_scroll_x[s_num];
        scroll_y = saved_scroll_y[s_num];
        debug_x = saved_debug_x[s_num];

        if(mzx_world->current_board_id != saved_board[s_num])
        {
          fix_board(mzx_world, saved_board[s_num]);
          synchronize_board_values(mzx_world, &src_board, &board_width,
           &board_height, &level_id, &level_param, &level_color, &overlay,
           &overlay_color);
          fix_mod(mzx_world, src_board);
          fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
           cursor_x, cursor_y, board_width, board_height, edit_screen_height);
        }
      }
    }

    switch(key)
    {
      case IKEY_UP:
      {
        int i, move_amount = 1;

        if(get_alt_status(keycode_internal))
          move_amount = 10;

        if(get_ctrl_status(keycode_internal) && (copy_repeat_height >= 0))
          move_amount = copy_repeat_height;

        for(i = 0; i < move_amount; i++)
        {
          if(!cursor_board_y)
            break;

          cursor_board_y--;

          if(draw_mode == 1)
          {
            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
            modified = 1;
          }

          if(((cursor_board_y - scroll_y) < 3) && scroll_y)
            scroll_y--;
        }
        break;
      }

      case IKEY_DOWN:
      {
        int i, move_amount = 1;

        if(get_alt_status(keycode_internal))
          move_amount = 10;

        if(get_ctrl_status(keycode_internal) && (copy_repeat_height >= 0))
          move_amount = copy_repeat_height;

        for(i = 0; i < move_amount; i++)
        {
          if(cursor_board_y == (board_height - 1))
            break;

          cursor_board_y++;

          if(draw_mode == 1)
          {
            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
            modified = 1;
          }

          // Scroll board position if there's room to
          if(((cursor_board_y - scroll_y) > (edit_screen_height - 5)) &&
           (scroll_y < (board_height - edit_screen_height)))
            scroll_y++;
        }
        break;
      }

      case IKEY_LEFT:
      {
        int i, move_amount = 1;

        if(get_alt_status(keycode_internal))
          move_amount = 10;

        if(get_ctrl_status(keycode_internal) && (copy_repeat_width >= 0))
          move_amount = copy_repeat_width;

        for(i = 0; i < move_amount; i++)
        {
          if(!cursor_board_x)
            break;

          cursor_board_x--;

          if(draw_mode == 1)
          {
            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
            modified = 1;
          }

          if(((cursor_board_x - scroll_x) < 5) && scroll_x)
            scroll_x--;
        }

        if((cursor_board_x - scroll_x) < (debug_x + 25))
        {
          debug_x = 60;
          clear_screen_no_update();
        }

        break;
      }

      case IKEY_RIGHT:
      {
        int i, move_amount = 1;

        if(get_alt_status(keycode_internal))
          move_amount = 10;

        if(get_ctrl_status(keycode_internal) && (copy_repeat_width >= 0))
          move_amount = copy_repeat_width;

        for(i = 0; i < move_amount; i++)
        {
          if(cursor_board_x == (board_width - 1))
            break;

          cursor_board_x++;

          if(draw_mode == 1)
          {
            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
            modified = 1;
          }

          if(((cursor_board_x - scroll_x) > 74) &&
           (scroll_x < (board_width - 80)))
            scroll_x++;
        }

        if((cursor_board_x - scroll_x) > (debug_x - 5))
        {
          debug_x = 0;
          clear_screen_no_update();
        }

        break;
      }

      case IKEY_SPACE:
      {
        if(draw_mode == 2)
        {
          place_current_at_xy(mzx_world, __TEXT, current_color,
           ' ', cursor_board_x, cursor_board_y, &copy_robot,
           &copy_scroll, &copy_sensor, overlay_edit);

          if(cursor_board_x < (board_width - 1))
            cursor_board_x++;

          if(((cursor_board_x - scroll_x) > 74) &&
           (scroll_x < (board_width - 80)))
            scroll_x++;
        }
        else
        {
          /* There are two editor placement modes. This behavior can be
           * toggled with the "editor_space_toggles" config.txt option.
           *
           * The legacy mode (editor_space_toggles = 1) allows users to
           * toggle between their param buffer and space, under any
           * situation.
           *
           * The new mode (editor_space_toggles = 0) disallows accidental
           * rewriting of robots (you must press Delete instead) and otherwise
           * implicitly writes the param. This makes placing new blocks with
           * the same ID (but different colour, say) faster.
           */
          int offset = cursor_board_x + (cursor_board_y * board_width);

          /* If it's not an overlay edit, and we're placing an identical
           * block, and the user has selected legacy placement mode, toggle
           * the current param type with SPACE.
           */
          if((!overlay_edit) && (current_id == level_id[offset]) &&
             (current_color == level_color[offset]) &&
             mzx_world->editor_conf.editor_space_toggles)
          {
            place_current_at_xy(mzx_world, SPACE, 7, 0, cursor_board_x,
             cursor_board_y, &copy_robot, &copy_scroll, &copy_sensor,
             overlay_edit);
          }
          else
          {
            /* If what we're trying to overwrite is a robot, only allow
             * the user to press "Delete" to remove it. Disallow rewrites
             * and toggling in this mode. Otherwise, just write through
             * as usual.
             */
            if(!((is_robot(level_id[offset]) && (!overlay_edit)) &&
                 !mzx_world->editor_conf.editor_space_toggles))
            {
              current_param = place_current_at_xy(mzx_world, current_id,
               current_color, current_param, cursor_board_x, cursor_board_y,
               &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
            }
          }
        }
        modified = 1;
        break;
      }

      case IKEY_BACKSPACE:
      {
        if(draw_mode == 2)
        {
          if(cursor_board_x)
            cursor_board_x--;

          if(((cursor_board_x - scroll_x) < 5) && scroll_x)
            scroll_x--;

          place_current_at_xy(mzx_world, __TEXT, current_color,
           ' ', cursor_board_x, cursor_board_y, &copy_robot,
           &copy_scroll, &copy_sensor, overlay_edit);
          modified = 1;
        }
        break;
      }

      case IKEY_INSERT:
      {
        grab_at_xy(mzx_world, &current_id, &current_color, &current_param,
         &copy_robot, &copy_scroll, &copy_sensor, cursor_board_x,
         cursor_board_y, overlay_edit);
        break;
      }

      case IKEY_TAB:
      {
        if(!get_alt_status(keycode_internal))
        {
          if(draw_mode)
          {
            draw_mode = 0;
          }
          else
          {
            draw_mode = 1;
            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
            modified = 1;
          }
        }

        break;
      }

      case IKEY_DELETE:
      {
        if(draw_mode == 2)
        {
          place_current_at_xy(mzx_world, __TEXT, current_color, ' ',
           cursor_board_x, cursor_board_y, &copy_robot,
           &copy_scroll, &copy_sensor, overlay_edit);
        }
        else
        {
          if(overlay_edit)
          {
            place_current_at_xy(mzx_world, SPACE, 7, 32, cursor_board_x,
             cursor_board_y, &copy_robot, &copy_scroll, &copy_sensor,
             overlay_edit);
          }
          else
          {
            place_current_at_xy(mzx_world, SPACE, 7, 0, cursor_board_x,
             cursor_board_y, &copy_robot, &copy_scroll, &copy_sensor,
             overlay_edit);
          }
        }
        modified = 1;
        break;
      }

      // Show invisible walls
      case IKEY_F1:
      {
        if(get_shift_status(keycode_internal))
        {
          if(!overlay_edit)
          {
            flash_thing(mzx_world, (int)INVIS_WALL, (int)INVIS_WALL,
             178, 176, scroll_x, scroll_y, edit_screen_height);
          }
        }
#ifdef CONFIG_HELPSYS
        else
        {
          m_show();
          help_system(mzx_world);
        }
#endif

        break;
      }

      // Show robots
      case IKEY_F2:
      {
        if(get_shift_status(keycode_internal))
        {
          if(!overlay_edit)
          {
            flash_thing(mzx_world, (int)ROBOT_PUSHABLE, (int)ROBOT,
             '!', 0, scroll_x, scroll_y, edit_screen_height);
          }
        }
        else
        {
          if(draw_mode != 2)
          {
            draw_mode = 2;
            text_start_x = cursor_board_x;
          }
          else
          {
            draw_mode = 0;
          }
        }
        break;
      }

      // Terrain
      case IKEY_F3:
      {
        if(!overlay_edit)
        {
          if(get_shift_status(keycode_internal))
          {
            // Show fakes
            flash_thing(mzx_world, (int)FAKE, (int)THICK_WEB, '#', 177,
             scroll_x, scroll_y, edit_screen_height);
          }
          else
          {
            thing_menu(mzx_world, 0, &current_id, &current_color,
             &current_param, &copy_robot, &copy_scroll, &copy_sensor,
             cursor_board_x, cursor_board_y);
            modified = 1;
          }
        }
        break;
      }

      // Item
      case IKEY_F4:
      {
        if(!overlay_edit)
        {
          if(get_shift_status(keycode_internal))
          {
            // Show spaces
            flash_thing(mzx_world, (int)SPACE, (int)SPACE, 'O', '*',
             scroll_x, scroll_y, edit_screen_height);
          }
          else
          {
            thing_menu(mzx_world, 1, &current_id, &current_color,
             &current_param, &copy_robot, &copy_scroll, &copy_sensor,
             cursor_board_x, cursor_board_y);
            modified = 1;
          }
        }
        break;
      }

      // Creature
      case IKEY_F5:
      {
        if(!overlay_edit)
        {
          thing_menu(mzx_world, 2, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      // Puzzle
      case IKEY_F6:
      {
        if(!overlay_edit)
        {
          thing_menu(mzx_world, 3, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      // Transport
      case IKEY_F7:
      {
        if(get_shift_status(keycode_internal))
        {
          save_editor_palette();
          set_screen_mode(get_screen_mode() + 1);
        }
        else

        if(!overlay_edit)
        {
          thing_menu(mzx_world, 4, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      // Element
      case IKEY_F8:
      {
        if(!overlay_edit)
        {
          thing_menu(mzx_world, 5, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      // Misc
      case IKEY_F9:
      {
        if(!overlay_edit)
        {
          thing_menu(mzx_world, 6, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      // Object
      case IKEY_F10:
      {
        if(!overlay_edit)
        {
          thing_menu(mzx_world, 7, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      case IKEY_c:
      {
        if(draw_mode != 2)
        {
          cursor_off();
          if(get_alt_status(keycode_internal))
          {
            char_editor(mzx_world);
            modified = 1;
          }
          else
          {
            int new_color = color_selection(current_color, 0);
            if(new_color >= 0)
              current_color = new_color;
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_o:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            if(!overlay_edit)
            {
              if(!src_board->overlay_mode)
              {
                error("Overlay mode is not on (see Board Info)",
                 0, 8, 0x1103);
              }
              else
              {
                overlay_edit = 1;
                current_param = 32;
                current_color = 7;
              }
            }
            else
            {
              overlay_edit = 0;
              current_id = SPACE;
              current_param = 0;
              current_color = 7;
            }
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_s:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            if(overlay_edit)
            {
              show_level ^= 1;
            }
            else
            {
              status_counter_info(mzx_world);
              modified = 1;
            }
          }
          else
          {
            char world_name[256];
            strcpy(world_name, current_world);
            if(!new_file(mzx_world, world_ext, ".mzx", world_name,
             "Save world", 1))
            {
              // Save entire game
              strcpy(current_world, world_name);
              save_world(mzx_world, current_world, 0);

              modified = 0;
            }
          }
        }
        else
        {
          text_place = 1;
        }

        break;
      }

      case IKEY_HOME:
      {
        cursor_board_x = 0;
        cursor_board_y = 0;
        scroll_x = 0;
        scroll_y = 0;

        if((cursor_board_x - scroll_x) < (debug_x + 25))
        {
          debug_x = 60;
          clear_screen_no_update();
        }

        break;
      }

      case IKEY_END:
      {
        cursor_board_x = board_width - 1;
        cursor_board_y = board_height - 1;
        scroll_x = board_width - 80;
        scroll_y = board_height - edit_screen_height;

        if(scroll_x < 0)
          scroll_x = 0;

        if(scroll_y < 0)
          scroll_y = 0;

        if((cursor_board_x - scroll_x) > (debug_x - 5))
        {
          debug_x = 0;
          clear_screen_no_update();
        }

        break;
      }

      case IKEY_b:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            block_x = cursor_board_x;
            block_y = cursor_board_y;
            draw_mode = 3;
          }
          else
          {
            int new_board =
             choose_board(mzx_world, mzx_world->current_board_id,
             "Select current board:", 0);

            if(new_board >= 0)
            {
              fix_board(mzx_world, new_board);

              synchronize_board_values(mzx_world, &src_board, &board_width,
               &board_height, &level_id, &level_param, &level_color, &overlay,
               &overlay_color);

              if(strcmp(src_board->mod_playing, "*") &&
               strcasecmp(src_board->mod_playing,
               mzx_world->real_mod_playing))
                fix_mod(mzx_world, src_board);

              if(!src_board->overlay_mode)
                overlay_edit = 0;

              fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
               &scroll_y, cursor_x, cursor_y, board_width, board_height,
               edit_screen_height);

              modified = 1;
            }
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_l:
      {
        if(get_alt_status(keycode_internal))
        {
          char test_wav[128] = { 0, };
          const char *const sam_ext[] = { ".WAV", ".SAM", ".OGG", NULL };

          if(!choose_file(mzx_world, sam_ext, test_wav,
           "Choose a wav file", 1))
          {
            play_sample(0, test_wav, false);
          }
        }
        else

        if(draw_mode != 2)
        {
          if(!modified || !confirm(mzx_world,
           "Load: World has not been saved, are you sure?"))
          {
            if(!choose_file_ch(mzx_world, world_ext, current_world,
             "Load World", 1))
            {
              int fade;
              // Load world curr_file
              end_module();
              if(!reload_world(mzx_world, current_world, &fade))
                create_blank_world(mzx_world);

              mzx_world->current_board_id = mzx_world->first_board;
              mzx_world->current_board =
               mzx_world->board_list[mzx_world->current_board_id];
              src_board = mzx_world->current_board;

              if(draw_mode > 3)
                draw_mode = 0;

              insta_fadein();

              synchronize_board_values(mzx_world, &src_board, &board_width,
               &board_height, &level_id, &level_param, &level_color,
               &overlay, &overlay_color);

              fix_mod(mzx_world, src_board);

              fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
               &scroll_y, cursor_x, cursor_y, board_width, board_height,
               edit_screen_height);

              if(!src_board->overlay_mode)
                overlay_edit = 0;

              modified = 0;
            }
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_i:
      {
        if(get_alt_status(keycode_internal))
        {
          int import_number = import_type(mzx_world);
          if(import_number >= 0)
          {
            char import_name[128];
            import_name[0] = 0;

            switch(import_number)
            {
              case 0:
              {
                if(!choose_file(mzx_world, mzb_ext, import_name,
                 "Choose board to import", 1))
                {
                  replace_current_board(mzx_world, import_name);

                  synchronize_board_values(mzx_world, &src_board, &board_width,
                   &board_height, &level_id, &level_param, &level_color,
                   &overlay, &overlay_color);

                  if(strcmp(src_board->mod_playing, "*") &&
                   strcasecmp(src_board->mod_playing,
                   mzx_world->real_mod_playing))
                    fix_mod(mzx_world, src_board);

                  fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
                   &scroll_y, cursor_x, cursor_y, board_width, board_height,
                   edit_screen_height);

                  if((draw_mode > 3) &&
                   (block_board == (mzx_world->current_board)))
                    draw_mode = 0;

                  modified = 1;
                }
                break;
              }

              case 1:
              {
                // Character set
                int char_offset = 0;
                struct element *elements[] =
                {
                  construct_number_box(21, 20, "Offset:  ",
                   0, 255, 0, &char_offset),
                };

                if(!file_manager(mzx_world, chr_ext, NULL, import_name,
                 "Choose character set to import", 1, 0,
                 elements, 1, 2, 0))
                {
                  ec_load_set_var(import_name, char_offset);
                }
                modified = 1;
                break;
              }

              case 2:
              {
                // World file
                if(!choose_file(mzx_world, world_ext, import_name,
                 "Choose world to import", 1))
                {
                  // FIXME: Check retval?
                  append_world(mzx_world, import_name);
                }

                if(draw_mode > 3)
                  draw_mode = 0;

                modified = 1;
                break;
              }

              case 3:
              {
                // Palette
                // Character set
                const char *pal_ext[] = { ".PAL", NULL };
                if(!choose_file(mzx_world, pal_ext, import_name,
                 "Choose palette to import", 1))
                {
                  load_palette(import_name);
                  update_palette();
                  modified = 1;
                }
                break;
              }

              case 4:
              {
                // Sound effects
                const char *sfx_ext[] = { ".SFX", NULL };
                if(!choose_file(mzx_world, sfx_ext, import_name,
                 "Choose SFX file to import", 1))
                {
                  FILE *sfx_file;

                  sfx_file = fopen_unsafe(import_name, "rb");
                  fread(mzx_world->custom_sfx, 69, 50, sfx_file);
                  mzx_world->custom_sfx_on = 1;
                  fclose(sfx_file);
                  modified = 1;
                }
                break;
              }

              case 5:
              {
                // MZM file
                const char *mzm_ext[] = { ".MZM", NULL };
                if(!choose_file(mzx_world, mzm_ext,
                 mzm_name_buffer, "Choose image file to import", 1))
                {
                  draw_mode = 5;
                  block_command = 9;
                }

                break;
              }
            }
          }
        }
        else

        if(draw_mode != 2)
        {
          board_info(mzx_world);
          // If this is the first board, patch the title into the world name
          if(mzx_world->current_board_id == 0)
            strcpy(mzx_world->name, src_board->board_name);

          if(!src_board->overlay_mode)
            overlay_edit = 0;

          modified = 1;
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_g:
      {
        if(draw_mode != 2)
        {
          global_info(mzx_world);
          modified = 1;
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_p:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            size_pos(mzx_world);
            set_update_done_current(mzx_world);
            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color,
             &overlay, &overlay_color);
            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             cursor_x, cursor_y, board_width, board_height, edit_screen_height);

            if(draw_mode > 3)
              draw_mode = 0;

            // Uh oh, we might need a new player
            if((mzx_world->player_x >= board_width) ||
             (mzx_world->player_y >= board_height))
              replace_player(mzx_world);

            modified = 1;
          }
          else

          if(!overlay_edit)
          {
            if(current_id < SENSOR)
            {
              int new_param = change_param(mzx_world, current_id,
               current_param, NULL, NULL, NULL);

              if(new_param >= 0)
                current_param = new_param;

              modified = 1;
            }
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_x:
      {
        if(get_alt_status(keycode_internal))
        {
          int export_number = export_type(mzx_world);
          if(export_number >= 0)
          {
            char export_name[128];
            export_name[0] = 0;

            switch(export_number)
            {
              case 0:
              {
                // Board file
                if(!new_file(mzx_world, mzb_ext, ".mzb", export_name,
                 "Export board file", 1))
                {
                  save_board_file(src_board, export_name);
                }
                break;
              }

              case 1:
              {
                // Character set
                int char_offset = 0;
                int char_size = 256;
                struct element *elements[] =
                {
                  construct_number_box(9, 20, "Offset:  ",
                   0, 255, 0, &char_offset),
                  construct_number_box(35, 20, "Size: ",
                   1, 256, 0, &char_size)
                };

                if(!file_manager(mzx_world, chr_ext, NULL, export_name,
                 "Export character set", 1, 1, elements, 2, 2, 0))
                {
                  add_ext(export_name, ".chr");
                  ec_save_set_var(export_name, char_offset,
                   char_size);
                }

                break;
              }

              case 2:
              {
                // Palette
                if(!new_file(mzx_world, pal_ext, ".pal", export_name,
                 "Export palette", 1))
                {
                  save_palette(export_name);
                }

                break;
              }

              case 3:
              {
                // Sound effects
                if(!new_file(mzx_world, sfx_ext, ".sfx", export_name,
                 "Export SFX file", 1))
                {
                  FILE *sfx_file;

                  sfx_file = fopen_unsafe(export_name, "wb");

                  if(sfx_file)
                  {
                    if(mzx_world->custom_sfx_on)
                      fwrite(mzx_world->custom_sfx, 69, 50, sfx_file);
                    else
                      fwrite(sfx_strs, 69, 50, sfx_file);

                    fclose(sfx_file);
                  }
                }
                break;
              }
            }
          }
        }
        else

        if(draw_mode != 2)
        {
          board_exits(mzx_world);
          modified = 1;
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_n:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            if(!src_board->mod_playing[0])
            {
              char new_mod[MAX_PATH] = { 0 };

              if(!choose_file(mzx_world, mod_ext, new_mod,
               "Choose a module file", 0))
              {
                strcpy(src_board->mod_playing, new_mod);
                strcpy(mzx_world->real_mod_playing, new_mod);
                load_board_module(src_board);
              }
            }
            else
            {
              end_module();
              src_board->mod_playing[0] = 0;
              mzx_world->real_mod_playing[0] = 0;
            }
            modified = 1;
          }
          else

          if(get_ctrl_status(keycode_internal))
          {
            if(!listening_flag)
            {
              char current_dir[MAX_PATH];
              char new_mod[128] = { 0 } ;

              getcwd(current_dir, MAX_PATH);
              chdir(current_listening_dir);

              if(!file_manager(mzx_world, mod_gdm_ext, NULL, new_mod,
               "Choose a module file (listening only)", 1, 0,
               NULL, 0, 0, 1))
              {
                load_module(new_mod, false, 255);
                strcpy(current_listening_mod, new_mod);
                getcwd(current_listening_dir, MAX_PATH);
                listening_flag = 1;
              }

              chdir(current_dir);
            }
            else
            {
              end_module();
              listening_flag = 0;
              if(mzx_world->real_mod_playing[0])
                load_module(mzx_world->real_mod_playing, true, 255);
            }
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_RETURN:
      {
        if(draw_mode == 3)
        {
          int start_x = block_x;
          int start_y = block_y;
          int block_width, block_height;

          block_board = src_board;

          if(start_x > cursor_board_x)
          {
            start_x = cursor_board_x;
            block_width = block_x - cursor_board_x + 1;
          }
          else
          {
            block_width = cursor_board_x - block_x + 1;
          }

          if(start_y > cursor_board_y)
          {
            start_y = cursor_board_y;
            block_height = block_y - cursor_board_y + 1;
          }
          else
          {
            block_height = cursor_board_y - block_y + 1;
          }

          // Select block command
          block_command = block_cmd(mzx_world);

          block_dest_x = cursor_board_x;
          block_dest_y = cursor_board_y;

          // Some block commands are done automatically
          switch(block_command)
          {
            case -1:
            {
              draw_mode = 0;
              break;
            }

            case 0:
            case 1:
            case 2:
            {
              draw_mode = 4;
              break;
            }

            case 3:
            {
              // Clear block
              if(overlay_edit)
              {
                clear_layer_block(start_x, start_y, block_width,
                 block_height, overlay, overlay_color, board_width);
              }
              else
              {
                clear_board_block(src_board, start_x, start_y,
                 block_width, block_height);
              }

              draw_mode = 0;
              modified = 1;
              break;
            }

            case 4:
            {
              // Flip block
              char *temp_buffer = malloc (sizeof(char) * block_width);
              int start_offset = start_x + (start_y * board_width);
              int end_offset = start_x + ((start_y + block_height - 1) *
               board_width);

              if(overlay_edit)
              {
                for(i = 0; i < (block_height / 2); i++, start_offset
                 += board_width, end_offset -= board_width)
                {
                  memcpy(temp_buffer, overlay + start_offset,
                   block_width);
                  memcpy(overlay + start_offset, overlay + end_offset,
                   block_width);
                  memcpy(overlay + end_offset, temp_buffer,
                   block_width);
                  memcpy(temp_buffer, overlay_color + start_offset,
                   block_width);
                  memcpy(overlay_color + start_offset,
                   overlay_color + end_offset, block_width);
                  memcpy(overlay_color + end_offset, temp_buffer,
                   block_width);
                }
              }
              else
              {
                char *level_under_id = src_board->level_under_id;
                char *level_under_color = src_board->level_under_color;
                char *level_under_param = src_board->level_under_param;

                for(i = 0; i < (block_height / 2); i++, start_offset
                 += board_width, end_offset -= board_width)
                {
                  memcpy(temp_buffer, level_id + start_offset,
                   block_width);
                  memcpy(level_id + start_offset,
                   level_id + end_offset, block_width);
                  memcpy(level_id + end_offset, temp_buffer,
                   block_width);
                  memcpy(temp_buffer, level_color + start_offset,
                   block_width);
                  memcpy(level_color + start_offset,
                   level_color + end_offset, block_width);
                  memcpy(level_color + end_offset, temp_buffer,
                   block_width);
                  memcpy(temp_buffer, level_param + start_offset,
                   block_width);
                  memcpy(level_param + start_offset,
                   level_param + end_offset, block_width);
                  memcpy(level_param + end_offset, temp_buffer,
                   block_width);
                  memcpy(temp_buffer, level_under_id + start_offset,
                   block_width);
                  memcpy(level_under_id + start_offset,
                   level_under_id + end_offset, block_width);
                  memcpy(level_under_id + end_offset, temp_buffer,
                   block_width);
                  memcpy(temp_buffer,
                   level_under_color + start_offset, block_width);
                  memcpy(level_under_color + start_offset,
                   level_under_color + end_offset, block_width);
                  memcpy(level_under_color + end_offset,
                   temp_buffer, block_width);
                  memcpy(temp_buffer,
                   level_under_param + start_offset, block_width);
                  memcpy(level_under_param + start_offset,
                   level_under_param + end_offset, block_width);
                  memcpy(level_under_param + end_offset, temp_buffer,
                   block_width);
                }
              }

              draw_mode = 0;
              modified = 1;

              free(temp_buffer);
              break;
            }

            case 5:
            {
              // Mirror block
              int temp, i2;
              int start_offset = start_x + (start_y * board_width);
              int previous_offset;
              int end_offset;

              if(overlay_edit)
              {
                for(i = 0; i < block_height; i++)
                {
                  previous_offset = start_offset;
                  end_offset = start_offset + block_width - 1;
                  for(i2 = 0; i2 < (block_width / 2);
                   i2++, start_offset++, end_offset--)
                  {
                    temp = overlay[start_offset];
                    overlay[start_offset] = overlay[end_offset];
                    overlay[end_offset] = temp;
                    temp = overlay_color[start_offset];
                    overlay_color[start_offset] = overlay_color[end_offset];
                    overlay_color[end_offset] = temp;
                  }
                  start_offset = previous_offset + board_width;
                }
              }
              else
              {
                char *level_under_id = src_board->level_under_id;
                char *level_under_color = src_board->level_under_color;
                char *level_under_param = src_board->level_under_param;

                for(i = 0; i < block_height; i++)
                {
                  previous_offset = start_offset;
                  end_offset = start_offset + block_width - 1;
                  for(i2 = 0; i2 < (block_width / 2);
                   i2++, start_offset++, end_offset--)
                  {
                    temp = level_id[start_offset];
                    level_id[start_offset] = level_id[end_offset];
                    level_id[end_offset] = temp;
                    temp = level_color[start_offset];
                    level_color[start_offset] = level_color[end_offset];
                    level_color[end_offset] = temp;
                    temp = level_param[start_offset];
                    level_param[start_offset] = level_param[end_offset];
                    level_param[end_offset] = temp;
                    temp = level_under_id[start_offset];
                    level_under_id[start_offset] = level_under_id[end_offset];
                    level_under_id[end_offset] = temp;
                    temp = level_under_color[start_offset];
                    level_under_color[start_offset] =
                     level_under_color[end_offset];
                    level_under_color[end_offset] = temp;
                    temp = level_under_param[start_offset];
                    level_under_param[start_offset] =
                     level_under_param[end_offset];
                    level_under_param[end_offset] = temp;
                  }
                  start_offset = previous_offset + board_width;
                }
              }

              draw_mode = 0;
              modified = 1;
              break;
            }

            case 6:
            {
              // Paint
              int i2;
              int skip = board_width - block_width;
              char *dest_offset;

              if(overlay_edit)
              {
                dest_offset =
                 overlay_color + start_x + (start_y * board_width);
              }
              else
              {
                dest_offset =
                 level_color + start_x + (start_y * board_width);
              }

              for(i = 0; i < block_height; i++, dest_offset += skip)
              {
                for(i2 = 0; i2 < block_width; i2++, dest_offset++)
                {
                  *dest_offset = current_color;
                }
              }
              draw_mode = 0;
              modified = 1;
              break;
            }

            case 7:
            {
              if(!overlay_edit)
              {
                if(!src_board->overlay_mode)
                {
                  error("Overlay mode is not on (see Board Info)", 0, 8,
                   0x1103);
                  draw_mode = 0;
                }
                else
                {
                  draw_mode = 4;
                  overlay_edit = 1;
                }
              }
              else
              {
                overlay_edit = 0;
                draw_mode = 4;
              }
              break;
            }

            case 8:
            {
              // Save as MZM
              mzm_name_buffer[0] = 0;

              if(!new_file(mzx_world, mzm_ext, ".mzm", mzm_name_buffer,
               "Export MZM", 1))
              {
                save_mzm(mzx_world, mzm_name_buffer, start_x,
                 start_y, block_width, block_height,
                 overlay_edit, 0);
              }
              modified = 1;
              draw_mode = 0;
              break;
            }
          }
        }
        else

        if(draw_mode == 2)
        {
          // Go to next line
          int dif_x = cursor_board_x - text_start_x;
          if(dif_x > 0)
          {
            for(i = 0; i < dif_x; i++)
            {
              if(!cursor_board_x)
                break;

              cursor_board_x--;

              if(((cursor_board_x - scroll_x) < 5) && scroll_x)
                scroll_x--;
            }

            if((cursor_board_x - scroll_x) < (debug_x + 25))
            {
              debug_x = 60;
              clear_screen_no_update();
            }
          }
          else

          if(dif_x < 0)
          {
            for(i = 0; i < -dif_x; i++)
            {
              if(cursor_board_x >= board_width)
                break;

              cursor_board_x++;

              if(((cursor_board_x - scroll_x) > 74) &&
               (scroll_x < (board_width - 80)))
                scroll_x++;

              if((cursor_board_x - scroll_x) > (debug_x - 5))
              {
                debug_x = 0;
                clear_screen_no_update();
              }
            }
          }

          if(cursor_board_y < (board_height - 1))
          {
            cursor_board_y++;

            // Scroll board position if there's room to
            if(((cursor_board_y - scroll_y) > (edit_screen_height - 5)) &&
             (scroll_y < (board_height - edit_screen_height)))
              scroll_y++;
          }
        }
        else

        if(overlay_edit)
        {
          if(draw_mode > 3)
          {
            int dest_x = cursor_board_x;
            int dest_y = cursor_board_y;
            int start_x = block_x;
            int start_y = block_y;
            int block_width, block_height;

            if(start_x > block_dest_x)
            {
              start_x = block_dest_x;
              block_width = block_x - block_dest_x + 1;
            }
            else
            {
              block_width = block_dest_x - block_x + 1;
            }

            if(start_y > block_dest_y)
            {
              start_y = block_dest_y;
              block_height = block_y - block_dest_y + 1;
            }
            else
            {
              block_height = block_dest_y - block_y + 1;
            }

            if((dest_x + block_width) > board_width)
              block_width = board_width - dest_x;

            if((dest_y + block_height) > board_height)
              block_height = board_height - dest_y;

            draw_mode = 0;

            switch(block_command)
            {
              case 0:
              case 1:
              {
                // Copy block
                char *char_buffer = cmalloc(block_width * block_height);
                char *color_buffer = cmalloc(block_width * block_height);
                copy_layer_to_buffer(start_x, start_y, block_width,
                 block_height, block_board->overlay, block_board->overlay_color,
                 char_buffer, color_buffer, block_board->board_width);
                copy_buffer_to_layer(dest_x, dest_y, block_width,
                 block_height, char_buffer, color_buffer, overlay,
                 overlay_color, board_width);

                // 1 is repeat copy
                if(block_command == 1)
                {
                  draw_mode = 4;
                  copy_repeat_width = block_width;
                  copy_repeat_height = block_height;
                }
                modified = 1;

                free(color_buffer);
                free(char_buffer);
                break;
              }

              case 2:
              {
                // Move block
                char *char_buffer = cmalloc(block_width * block_height);
                char *color_buffer = cmalloc(block_width * block_height);
                copy_layer_to_buffer(start_x, start_y, block_width,
                 block_height, block_board->overlay,
                 block_board->overlay_color, char_buffer,
                 color_buffer, block_board->board_width);
                clear_layer_block(start_x, start_y, block_width,
                 block_height, block_board->overlay,
                 block_board->overlay_color, block_board->board_width);
                copy_buffer_to_layer(dest_x, dest_y, block_width,
                 block_height, char_buffer, color_buffer, overlay,
                 overlay_color, board_width);

                modified = 1;
                free(color_buffer);
                free(char_buffer);
                break;
              }

              case 7:
              {
                // Copy from board
                int overlay_offset = dest_x + (dest_y * board_width);
                copy_board_to_layer(block_board, start_x, start_y,
                 block_width, block_height, overlay + overlay_offset,
                 overlay_color + overlay_offset, board_width);
                modified = 1;
                break;
              }

              case 9:
              {
                // Load MZM
                load_mzm(mzx_world, mzm_name_buffer, dest_x,
                 dest_y, 1, 0);
                modified = 1;
                break;
              }
            }
          }
          else
          {
            int new_param = char_selection(current_param);
            if(new_param >= 0)
            {
              current_param = new_param;
              modified = 1;
            }
          }
        }
        else
        {
          if(draw_mode > 2)
          {
            int dest_x = cursor_board_x;
            int dest_y = cursor_board_y;
            int start_x = block_x;
            int start_y = block_y;
            int block_width, block_height;
            int original_width;
            int original_height;

            if(start_x > block_dest_x)
            {
              start_x = block_dest_x;
              block_width = block_x - block_dest_x + 1;
            }
            else
            {
              block_width = block_dest_x - block_x + 1;
            }

            if(start_y > block_dest_y)
            {
              start_y = block_dest_y;
              block_height = block_y - block_dest_y + 1;
            }
            else
            {
              block_height = block_dest_y - block_y + 1;
            }

            original_width = block_width;
            original_height = block_height;

            if((dest_x + block_width) > board_width)
              block_width = board_width - dest_x;

            if((dest_y + block_height) > board_height)
              block_height = board_height - dest_y;

            draw_mode = 0;

            switch(block_command)
            {
              case 0:
              case 1:
              {
                // Copy block
                int block_size = block_width * block_height;
                char *id_buffer = cmalloc(block_size);
                char *param_buffer = cmalloc(block_size);
                char *color_buffer = cmalloc(block_size);
                char *under_id_buffer = cmalloc(block_size);
                char *under_param_buffer = cmalloc(block_size);
                char *under_color_buffer = cmalloc(block_size);
                copy_board_to_board_buffer(block_board, start_x, start_y,
                 block_width, block_height, id_buffer, param_buffer,
                 color_buffer, under_id_buffer, under_param_buffer,
                 under_color_buffer, src_board);
                copy_board_buffer_to_board(src_board, dest_x, dest_y,
                 block_width, block_height, id_buffer, param_buffer,
                 color_buffer, under_id_buffer, under_param_buffer,
                 under_color_buffer);

                // 1 is repeat copy
                if(block_command == 1)
                {
                  draw_mode = 4;
                  copy_repeat_width = block_width;
                  copy_repeat_height = block_height;
                }

                modified = 1;
                free(under_color_buffer);
                free(under_param_buffer);
                free(under_id_buffer);
                free(color_buffer);
                free(param_buffer);
                free(id_buffer);
                break;
              }

              case 2:
              {
                // Move block
                int block_size = block_width * block_height;
                char *id_buffer = cmalloc(block_size);
                char *param_buffer = cmalloc(block_size);
                char *color_buffer = cmalloc(block_size);
                char *under_id_buffer = cmalloc(block_size);
                char *under_param_buffer = cmalloc(block_size);
                char *under_color_buffer = cmalloc(block_size);
                copy_board_to_board_buffer(block_board, start_x, start_y,
                 block_width, block_height, id_buffer, param_buffer,
                 color_buffer, under_id_buffer, under_param_buffer,
                 under_color_buffer, src_board);
                clear_board_block(block_board, start_x, start_y,
                 original_width, original_height);

                // Work around to move the player
                if((mzx_world->player_x >= start_x) &&
                 (mzx_world->player_y >= start_y) &&
                 (mzx_world->player_x < (start_x + block_width)) &&
                 (mzx_world->player_y < (start_y + block_height)) &&
                 (block_board == src_board))
                {
                  place_player_xy(mzx_world,
                   mzx_world->player_x - start_x + dest_x,
                   mzx_world->player_y - start_y + dest_y);
                }

                copy_board_buffer_to_board(src_board, dest_x, dest_y,
                 block_width, block_height, id_buffer, param_buffer,
                 color_buffer, under_id_buffer, under_param_buffer,
                 under_color_buffer);

                modified = 1;
                free(under_color_buffer);
                free(under_param_buffer);
                free(under_id_buffer);
                free(color_buffer);
                free(param_buffer);
                break;
              }

              case 7:
              {
                // Copy from overlay
                int convert_select = rtoo_obj_type(mzx_world);
                enum thing convert_id = NO_ID;

                switch(convert_select)
                {
                  case 0:
                  {
                    // Custom Block
                    convert_id = CUSTOM_BLOCK;
                    break;
                  }

                  case 1:
                  {
                    convert_id = CUSTOM_FLOOR;
                    break;
                  }

                  case 2:
                  {
                    convert_id = __TEXT;
                    break;
                  }
                }

                if(convert_select != -1)
                {
                  int overlay_offset = start_x +
                   (start_y * block_board->board_width);
                  copy_layer_to_board(src_board, dest_x, dest_y,
                   block_width, block_height, block_board->overlay +
                   overlay_offset, block_board->overlay_color +
                   overlay_offset, block_board->board_width,
                   convert_id);

                  modified = 1;
                }
                break;
              }

              case 9:
              {
                // Load MZM
                load_mzm(mzx_world, mzm_name_buffer, dest_x, dest_y, 0, 0);
                modified = 1;
                break;
              }
            }
          }
          else
          {
            int new_param;

            grab_at_xy(mzx_world, &current_id, &current_color,
             &current_param, &copy_robot, &copy_scroll, &copy_sensor,
             cursor_board_x, cursor_board_y, overlay_edit);

            copy_robot.xpos = cursor_board_x;
            copy_robot.ypos = cursor_board_y;
            new_param =
             change_param(mzx_world, current_id, current_param,
             &copy_robot, &copy_scroll, &copy_sensor);

            // Kinda a hack, but should get the job done.
            if(is_storageless(current_id) && (new_param >= 0))
            {
              src_board->level_param[(cursor_board_y * board_width) +
               cursor_board_x] = new_param;

              current_param = new_param;
            }
            else
            {
              current_param = place_current_at_xy(mzx_world,
               current_id, current_color, new_param, cursor_board_x,
               cursor_board_y, &copy_robot, &copy_scroll, &copy_sensor, 0);
            }

            modified = 1;
          }
        }

        break;
      }

      case IKEY_F11:
      {
        // SMZX Mode
        save_editor_palette();
        set_screen_mode(get_screen_mode() + 1);
        break;
      }

      case IKEY_e:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            palette_editor(mzx_world);
            modified = 1;
          }
        }
        else
        {
          text_place = 1;
        }

        break;
      }

      case IKEY_f:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            sfx_edit(mzx_world);
            modified = 1;
          }
          else
          {
            fill_area(mzx_world, current_id, current_color,
             current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit);
            modified = 1;
          }
        }
        else
        {
          text_place = 1;
        }

        break;
      }

      case IKEY_ESCAPE:
      {
        if(draw_mode)
        {
          draw_mode = 0;
          copy_repeat_width = -1;
          copy_repeat_height = -1;
          key = 0;
        }
        else

        if(overlay_edit)
        {
          overlay_edit = 0;
          current_id = SPACE;
          current_param = 0;
          current_color = 7;
          key = 0;
        }
        else

        if(modified &&
         confirm(mzx_world, "Exit: World has not been saved, are you sure?"))
        {
          key = 0;
        }
        break;
      }

      case IKEY_v:
      {
        if(draw_mode != 2)
        {
          // View mode
          int viewport_width = src_board->viewport_width;
          int viewport_height = src_board->viewport_height;
          int v_scroll_x = 0, v_scroll_y = 0;
          int v_key;

          cursor_off();
          m_hide();

          do
          {
            draw_viewport(mzx_world);
            draw_game_window(src_board, v_scroll_x, v_scroll_y);
            update_screen();

            update_event_status_delay();
            v_key = get_key(keycode_internal);

            switch(v_key)
            {
              case IKEY_LEFT:
              {
                if(v_scroll_x)
                  v_scroll_x--;
                break;
              }

              case IKEY_RIGHT:
              {
                if(v_scroll_x < (board_width - viewport_width))
                  v_scroll_x++;
                break;
              }

              case IKEY_UP:
              {
                if(v_scroll_y)
                  v_scroll_y--;
                break;
              }

              case IKEY_DOWN:
              {
                if(v_scroll_y < (board_height - viewport_height))
                  v_scroll_y++;
                break;
              }
            }
          } while(v_key != IKEY_ESCAPE);

          m_show();
          clear_screen_no_update();
        }
        else
        {
          text_place = 1;
        }
      }

      case IKEY_t:
      {
        if(get_alt_status(keycode_internal))
        {
          int fade;
          int current_board_id = mzx_world->current_board_id;

          if(!save_world(mzx_world, "__test.mzx", 0))
          {
            save_editor_palette();
            vquick_fadeout();
            cursor_off();
            src_board = mzx_world->board_list[current_board_id];
            fix_board(mzx_world, current_board_id);
            set_counter(mzx_world, "TIME", src_board->time_limit, 0);
            send_robot_def(mzx_world, 0, 11);
            send_robot_def(mzx_world, 0, 10);
            find_player(mzx_world);

            mzx_world->player_restart_x = mzx_world->player_x;
            mzx_world->player_restart_y = mzx_world->player_y;

            strcpy(mzx_world->real_mod_playing, src_board->mod_playing);
            load_board_module(src_board);

            mzx_world->editing = true;

            play_game(mzx_world);

            mzx_world->editing = false;

            if(!reload_world(mzx_world, "__test.mzx", &fade))
            {
              create_blank_world(mzx_world);
              current_board_id = mzx_world->current_board_id;
              modified = 0;
            }

            load_editor_palette();

            scroll_color = 15;
            mzx_world->current_board_id = current_board_id;
            mzx_world->current_board =
             mzx_world->board_list[current_board_id];

            if(draw_mode > 3)
              draw_mode = 0;

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color, &overlay,
             &overlay_color);

            insta_fadein();

            fix_mod(mzx_world, src_board);

            if(listening_flag)
            {
              getcwd(current_dir, MAX_PATH);
              chdir(current_listening_dir);
              load_module(current_listening_mod, true, 255);
              chdir(current_dir);
            }

            unlink("__test.mzx");
          }
        }
        else

        if(draw_mode == 2)
        {
          text_place = 1;
        }

        break;
      }

      case IKEY_a:
      {
        if(get_alt_status(keycode_internal))
        {
          int charset_load = choose_char_set(mzx_world);

          switch(charset_load)
          {
            case 0:
            {
              ec_load_mzx();
              break;
            }

            case 1:
            {
              ec_load_ascii();
              break;
            }

            case 2:
            {
              ec_load_smzx();
              break;
            }

            case 3:
            {
              ec_load_blank();
              break;
            }
          }

          modified = 1;
        }
        else

        if(draw_mode != 2)
        {
          // Add board, find first free
          for(i = 0; i < mzx_world->num_boards; i++)
          {
            if(mzx_world->board_list[i] == NULL)
              break;
          }

          if(i < MAX_BOARDS)
          {
            if(add_board(mzx_world, i) >= 0)
            {
              fix_board(mzx_world, i);

              synchronize_board_values(mzx_world, &src_board, &board_width,
               &board_height, &level_id, &level_param, &level_color, &overlay,
               &overlay_color);

              fix_mod(mzx_world, src_board);

              fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
               &scroll_y, cursor_x, cursor_y, board_width, board_height,
               edit_screen_height);

              modified = 1;
            }
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_d:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            default_palette();
            update_palette();
          }
          else
          {
            int del_board =
             choose_board(mzx_world, mzx_world->current_board_id,
             "Delete board:", 0);

            if((del_board > 0) &&
             !confirm(mzx_world, "DELETE BOARD - Are you sure?"))
            {
              int current_board_id = mzx_world->current_board_id;
              clear_board(mzx_world->board_list[del_board]);
              mzx_world->board_list[del_board] = NULL;

              // Remove null board from list
              optimize_null_boards(mzx_world);

              if(del_board == current_board_id)
              {
                fix_board(mzx_world, 0);

                synchronize_board_values(mzx_world, &src_board, &board_width,
                 &board_height, &level_id, &level_param, &level_color,
                 &overlay, &overlay_color);

                if(strcmp(src_board->mod_playing, "*") &&
                 strcasecmp(src_board->mod_playing,
                 mzx_world->real_mod_playing))
                  fix_mod(mzx_world, src_board);

                fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
                 &scroll_y, cursor_x, cursor_y, board_width, board_height,
                 edit_screen_height);

                if(!src_board->overlay_mode)
                  overlay_edit = 0;
              }
              else
              {
                synchronize_board_values(mzx_world, &src_board, &board_width,
                 &board_height, &level_id, &level_param, &level_color,
                 &overlay, &overlay_color);
              }
            }
          }

          modified = 1;
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_z:
      {
        if(get_alt_status(keycode_internal))
        {
          // Clear board
          if(!confirm(mzx_world, "Clear board - Are you sure?"))
          {
            clear_board(src_board);
            src_board = create_blank_board();
            mzx_world->current_board->robot_list[0] = &mzx_world->global_robot;
            mzx_world->board_list[mzx_world->current_board_id] = src_board;
            mzx_world->current_board = src_board;
            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color, &overlay,
             &overlay_color);
            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             cursor_x, cursor_y, board_width, board_height, edit_screen_height);
            end_module();
            src_board->mod_playing[0] = 0;
            strcpy(mzx_world->real_mod_playing,
             src_board->mod_playing);

            if((draw_mode > 3) &&
             (block_board == (mzx_world->current_board)))
            {
              draw_mode = 0;
            }
          }

          modified = 1;
        }
        else

        if(draw_mode == 2)
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_r:
      {
        if(get_alt_status(keycode_internal))
        {
          // Clear world
          if(!confirm(mzx_world, "Clear ALL - Are you sure?"))
          {
            clear_world(mzx_world);
            create_blank_world(mzx_world);

            if(draw_mode > 3)
              draw_mode = 0;

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color, &overlay,
             &overlay_color);
            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             cursor_x, cursor_y, board_width, board_height, edit_screen_height);
            end_module();
          }

          modified = 1;
        }
        else

        if(draw_mode == 2)
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_PAGEDOWN:
      {
        current_menu++;

        if(current_menu == 6)
          current_menu = 0;

        break;
      }

      case IKEY_PAGEUP:
      {
        current_menu--;

        if(current_menu == -1)
          current_menu = 5;

        break;
      }

      case IKEY_8:
      case IKEY_KP_MULTIPLY:
      {
        if(draw_mode == 2)
        {
          text_place = 1;
        }
        else

        if(get_shift_status(keycode_internal) ||
         (key == IKEY_KP_MULTIPLY))
        {
          if(src_board->mod_playing[0])
            end_module();
          src_board->mod_playing[0] = '*';
          src_board->mod_playing[1] = 0;

          modified = 1;
        }

        break;
      }

      case IKEY_m:
      {
        if(get_alt_status(keycode_internal))
        {
          int offset = cursor_board_x + (cursor_board_y * board_width);
          enum thing d_id = (enum thing)level_id[offset];
          int d_param = level_param[offset];

          if(d_id == SENSOR)
          {
            edit_sensor(mzx_world, src_board->sensor_list[d_param]);
            modified = 1;
          }
          else

          if(is_robot(d_id))
          {
            edit_robot(mzx_world, src_board->robot_list[d_param]);
            modified = 1;
          }
          else

          if(is_signscroll(d_id))
          {
            edit_scroll(mzx_world, src_board->scroll_list[d_param]);
            modified = 1;
          }
        }
        else

        if(draw_mode == 2)
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_y:
      {
        if(get_alt_status(keycode_internal))
        {
          debug_mode = !debug_mode;
        }
        else

        if(draw_mode == 2)
        {
          text_place = 1;
        }

        break;
      }

      case IKEY_h:
      {
        if(get_alt_status(keycode_internal))
        {
          if(edit_screen_height == EDIT_SCREEN_NORMAL)
          {
            edit_screen_height = EDIT_SCREEN_EXTENDED;
            clear_screen_no_update();

            if((scroll_y + 25) > board_height)
              scroll_y = board_height - 25;

            if(scroll_y < 0)
              scroll_y = 0;
          }
          else
          {
            edit_screen_height = EDIT_SCREEN_NORMAL;
            if((cursor_board_y - scroll_y) > 18)
              scroll_y += cursor_board_y - scroll_y - 18;
          }
        }
        else

        if(draw_mode == 2)
        {
          text_place = 1;
        }

        break;
      }


      case 0:
      {
        break;
      }

      default:
      {
        if(draw_mode == 2)
          text_place = 1;
      }
    }

    if(text_place)
    {
      key = get_key(keycode_unicode);
      if(key != 0)
      {
        place_current_at_xy(mzx_world, __TEXT, current_color,
         key, cursor_board_x, cursor_board_y, &copy_robot,
         &copy_scroll, &copy_sensor, overlay_edit);

        if(cursor_board_x < (board_width - 1))
          cursor_board_x++;

        if((cursor_board_x > 74) && (cursor_board_x < (board_width - 5)) &&
         (scroll_x < (board_width - 80)))
          scroll_x++;

        if((cursor_board_x - scroll_x) > (debug_x - 5))
        {
          debug_x = 0;
          clear_screen_no_update();
        }

        modified = 1;
      }
    }
  } while(key != IKEY_ESCAPE);

  update_event_status();

  debug_mode = false;

  clear_world(mzx_world);
  clear_global_data(mzx_world);
  cursor_off();
  end_module();
  m_hide();
  clear_screen(32, 7);
  insta_fadeout();
  strcpy(curr_file, current_world);

  // Clear the copy stuff.
  if(copy_robot.used)
    clear_robot_contents(&copy_robot);
  if(copy_scroll.used)
    clear_scroll_contents(&copy_scroll);
}

void editor_init(void)
{
  edit_world = __edit_world;
  draw_debug_box = __draw_debug_box;
  debug_counters = __debug_counters;
  load_editor_charsets();
}

bool is_editor(void)
{
  return true;
}
