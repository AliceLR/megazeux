/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../audio.h"
#include "../block.h"
#include "../const.h"
#include "../counter.h"
#include "../data.h"
#include "../error.h"
#include "../event.h"
#include "../game.h"
#include "../graphics.h"
#include "../helpsys.h"
#include "../idarray.h"
#include "../idput.h"
#include "../intake.h"
#include "../mzm.h"
#include "../platform.h"
#include "../scrdisp.h"
#include "../sfx.h"
#include "../util.h"
#include "../window.h"
#include "../world.h"

#include "block.h"
#include "board.h"
#include "char_ed.h"
#include "debug.h"
#include "edit.h"
#include "edit_di.h"
#include "fill.h"
#include "graphics.h"
#include "pal_ed.h"
#include "param.h"
#include "robo_debug.h"
#include "robot.h"
#include "select.h"
#include "sfx_edit.h"
#include "undo.h"
#include "window.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#define NEW_WORLD_TITLE       "Untitled world"

#define EDIT_SCREEN_EXTENDED  24
#define EDIT_SCREEN_NORMAL    19

#define DRAW_MEMORY_TIMER_MAX 120
#define DRAW_MOD_TIMER_MAX    300

static int draw_memory_timer = 0;
static int draw_mod_timer = 0;

static struct undo_history *board_history = NULL;
static struct undo_history *overlay_history = NULL;
static struct undo_history *vlayer_history = NULL;

void free_editor_config(struct world *mzx_world)
{
  int i;

  // Extended Macros
  if(mzx_world->editor_conf.extended_macros)
  {
    for(i = 0; i < mzx_world->editor_conf.num_extended_macros; i++)
      free_macro(mzx_world->editor_conf.extended_macros[i]);

    free(mzx_world->editor_conf.extended_macros);
    mzx_world->editor_conf.extended_macros = NULL;
  }
}

void load_editor_config(struct world *mzx_world, int *argc, char *argv[])
{
  default_editor_config(&mzx_world->editor_conf);
  set_editor_config_from_file(&mzx_world->editor_conf,
   mzx_res_get_by_id(CONFIG_TXT));
  set_editor_config_from_command_line(&mzx_world->editor_conf, argc, argv);

  // Backup the config
  memcpy(&mzx_world->editor_conf_backup, &mzx_world->editor_conf,
   sizeof(struct editor_config_info));
}

// Wrapper for reload_world so we can load an editor config
static bool editor_reload_world(struct world *mzx_world, const char *file,
 int *faded)
{
  struct stat file_info;
  struct editor_config_info *conf = &(mzx_world->editor_conf);
  struct editor_config_info *backup = &(mzx_world->editor_conf_backup);
  size_t file_name_len = strlen(file) - 4;
  char config_file_name[MAX_PATH];

  bool world_loaded = reload_world(mzx_world, file, faded);

  if(!world_loaded)
    return world_loaded;

  // Part 1: Reset as much of the config file as we can.

  // We don't want to lose these.  If they're any different, the backup
  // version has already been freed/reallocated, so just copy the pointer.
  backup->extended_macros = conf->extended_macros;

  memcpy(conf, backup, sizeof(struct editor_config_info));

  // Part 2: Now load the new world.editor.cnf.

  strncpy(config_file_name, file, file_name_len);
  strncpy(config_file_name + file_name_len, ".editor.cnf", 12);

  if(world_loaded && stat(config_file_name, &file_info) >= 0)
    set_editor_config_from_file(conf, config_file_name);

  draw_mod_timer = DRAW_MOD_TIMER_MAX;

  return world_loaded;
}

static void set_vlayer_mode(int target_mode, int *overlay_edit,
 int *cursor_board_x, int *cursor_board_y, int *scroll_x, int *scroll_y,
 int *cursor_vlayer_x, int *cursor_vlayer_y, int *vscroll_x, int *vscroll_y)
{
  // If switching to/from the vlayer, store the cursor position.

  if((*overlay_edit == EDIT_VLAYER && target_mode < EDIT_VLAYER) ||
   (*overlay_edit < EDIT_VLAYER && target_mode == EDIT_VLAYER))
  {
    int t;

    *overlay_edit = target_mode;

    t = *cursor_board_x;
    *cursor_board_x = *cursor_vlayer_x;
    *cursor_vlayer_x = t;

    t = *cursor_board_y;
    *cursor_board_y = *cursor_vlayer_y;
    *cursor_vlayer_y = t;

    t = *scroll_x;
    *scroll_x = *vscroll_x;
    *vscroll_x = t;

    t = *scroll_y;
    *scroll_y = *vscroll_y;
    *vscroll_y = t;
  }
}

static void synchronize_board_values(struct world *mzx_world,
 struct board **src_board, int *board_width, int *board_height,
 char **level_id, char **level_param, char **level_color,
 char **overlay, char **overlay_color, char **vlayer_chars,
 char **vlayer_colors, int overlay_edit)
{
  *src_board = mzx_world->current_board;
  *board_width = (*src_board)->board_width;
  *board_height = (*src_board)->board_height;
  *level_id = (*src_board)->level_id;
  *level_param = (*src_board)->level_param;
  *level_color = (*src_board)->level_color;
  *overlay = (*src_board)->overlay;
  *overlay_color = (*src_board)->overlay_color;
  *vlayer_chars = mzx_world->vlayer_chars;
  *vlayer_colors = mzx_world->vlayer_colors;

  if(overlay_edit == EDIT_VLAYER)
  {
    // During vlayer editing, treat the vlayer as if it's the board.
    *board_width = mzx_world->vlayer_width;
    *board_height = mzx_world->vlayer_height;
  }
}

static void fix_caption(struct world *mzx_world, int modified)
{
  // Fix the window caption for the editor
  set_caption(mzx_world, mzx_world->current_board, NULL, 1, modified);
}

static void fix_cursor(int *cursor_board_x, int *cursor_board_y,
 int scroll_x, int scroll_y, int *debug_x, int board_width, int board_height)
{
  if(*cursor_board_x >= board_width)
    *cursor_board_x = (board_width - 1);

  if(*cursor_board_y >= board_height)
    *cursor_board_y = (board_height - 1);

  if((*cursor_board_x - scroll_x) < (*debug_x + 25))
    *debug_x = 60;

  if((*cursor_board_x - scroll_x) > (*debug_x - 5))
    *debug_x = 0;
}

static void fix_scroll(int *cursor_board_x, int *cursor_board_y,
 int *scroll_x, int *scroll_y, int *debug_x, int board_width, int board_height,
 int edit_screen_height)
{
  // the bounds checks need to be done in this order
  if(*scroll_x + 80 > board_width)
    *scroll_x = (board_width - 80);

  if(*scroll_x < 0)
    *scroll_x = 0;

  if(*scroll_y + edit_screen_height > board_height)
    *scroll_y = (board_height - edit_screen_height);

  if(*scroll_y < 0)
    *scroll_y = 0;

  fix_cursor(cursor_board_x, cursor_board_y, *scroll_x, *scroll_y, debug_x,
   board_width, board_height);
}

static void fix_board(struct world *mzx_world, int new_board)
{
  mzx_world->current_board_id = new_board;
  mzx_world->current_board = mzx_world->board_list[new_board];
}

static void fix_mod(struct world *mzx_world, struct board *src_board,
 int listening_flag)
{
  // If the listening mod is playing, do not load the board mod.
  if(!listening_flag)
  {
    if(!strcmp(src_board->mod_playing, "*"))
    {
      end_module();
    }

    else
    {
      load_board_module(mzx_world, src_board);
    }
  }
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
  Note: in DOS, SAVE was highlighted white if the world was changed since last
  load or save.

  The commands with ! instead of : have not been implemented yet.

  The menu line turns to "Overlay editing-" and the lower portion shows
  overlay commands on overlay mode.
*/

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
    " L:Load   S:Save  G:Global Info  Alt+G:Global Robot  Alt+R:Reset   Alt+T:Test",
    " Alt+C:Char Edit  Alt+E:Palette  Alt+S:Status Info   Alt+V:Vlayer  Alt+F:SFX"
  },
  {
    " Alt+Z:Clear  Alt+I:Import  Alt+P:Size/Pos  I:Info   M:Move  A:Add  D:Delete",
    " Ctrl+G:Goto  Alt+X:Export  Alt+O:Overlay   X:Exits  V:View  B:Select Board"
  },
  {
    " F3:Terrain  F4:Item      F5:Creature  F6:Puzzle  F7:Transport  F8:Element",
    " F9:Misc     F10:Objects  P:Parameter  C:Color"
  },
  {
    " \x12\x1d:Move  Space:Place  Enter:Modify+Grab  Alt+M:Modify  Ins:Grab  Del:Delete",
          " F:Fill   Tab:Draw     F2:Text            Alt+B:Block   Alt+\x12\x1d:Move 10"
  },
  {
    " Shift+F1:Show InvisWalls   Shift+F2:Show Robots   Alt+F11:Robot Debugger",
    " Shift+F3:Show Fakes        Shift+F4:Show Spaces   Alt+Y:Debug Window"
  },
  {
    " F1:Help    Home/End:Corner  Alt+A:Select Char Set  Alt+D:Default Colors",
    " ESC:Exit   F11:Screen Mode  Alt+L:Test SAM         Alt+N:Music  *:Mod *"
  }
};

static const char *const overlay_menu_lines[4] =
{
  " OVERLAY EDITING- (Alt+O to end)",
  " \x12\x1d:Move  Enter:Modify+Grab  Space:Place  Ins:Grab  Del:Delete        F:Fill",
  " C:Color  Alt+\x12\x1d:Move 10     Alt+B:Block  Tab:Draw  Alt+S:Show level  F2:Text",
  "Character"
};

static const char *const vlayer_menu_lines[4] =
{
  " VLAYER EDITING- (Alt+V to end)",
  " \x12\x1d:Move  Enter:Modify+Grab  Space:Place  Ins:Grab  Del:Delete  F:Fill",
  " C:Color  Alt+\x12\x1d:Move 10     Alt+B:Block  Tab:Draw  Alt+P:Size  F2:Text",
  "Character"
};

static const char *const minimal_help_mode_mesg[3] =
{
  "Alt+H : Toggle Help",
  "Alt+H : Overlay Help",
  "Alt+H : Vlayer Help",
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
{ ".ogg", ".mod", ".s3m", ".xm", ".it", ".gdm",
  ".669", ".amf", ".dsm", ".far", ".med",
  ".mtm", ".okt", ".stm", ".ult", ".wav",
  NULL
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
    int r_value = edit_robot(mzx_world, copy_robot);

    draw_memory_timer = DRAW_MEMORY_TIMER_MAX;
    fix_caption(mzx_world, 1);
    return r_value;
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
 int param, int x, int y, struct robot *copy_robot, struct scroll *copy_scroll,
 struct sensor *copy_sensor, int overlay_edit, int save_history)
{
  struct board *src_board = mzx_world->current_board;
  int offset = x + (y * src_board->board_width);
  char *level_id = src_board->level_id;
  enum thing old_id = (enum thing)level_id[offset];

  if(overlay_edit == EDIT_BOARD)
  {
    if(old_id != PLAYER)
    {
      if(save_history)
        add_board_undo_frame(mzx_world, board_history, id, color, param, x, y,
         copy_robot, copy_scroll, copy_sensor);

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
          replace_robot_source(mzx_world, src_board, copy_robot, old_param);
          src_board->level_color[offset] = color;
          src_board->level_id[offset] = id;

          if(save_history)
            update_undo_frame(board_history);

          return old_param;
        }

        param = duplicate_robot_source(mzx_world, src_board, copy_robot, x, y);
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

          if(save_history)
            update_undo_frame(board_history);

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

          if(save_history)
            update_undo_frame(board_history);

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

      if(save_history)
        update_undo_frame(board_history);
    }
  }
  else

  if(overlay_edit == EDIT_OVERLAY)
  {
    if(save_history)
      add_layer_undo_frame(overlay_history, src_board->overlay,
       src_board->overlay_color, src_board->board_width, offset, 1, 1);

    src_board->overlay[offset] = param;
    src_board->overlay_color[offset] = color;

    if(save_history)
      update_undo_frame(overlay_history);
  }

  else // EDIT_VLAYER
  {
    int offset = x + (y * mzx_world->vlayer_width);

    if(save_history)
      add_layer_undo_frame(vlayer_history, mzx_world->vlayer_chars,
       mzx_world->vlayer_colors, mzx_world->vlayer_width, offset, 1, 1);

    mzx_world->vlayer_chars[offset] = param;
    mzx_world->vlayer_colors[offset] = color;

    if(save_history)
      update_undo_frame(vlayer_history);
  }

  return param;
}

void grab_at_xy(struct world *mzx_world, enum thing *new_id,
 int *new_color, int *new_param, struct robot *copy_robot,
 struct scroll *copy_scroll, struct sensor *copy_sensor,
 int x, int y, int overlay_edit)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int offset = x + (y * board_width);
  enum thing old_id = *new_id;
  int old_param = *new_param;

  if(overlay_edit == EDIT_BOARD)
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
      duplicate_robot_direct_source(mzx_world, src_robot, copy_robot, 0, 0);
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

  if(overlay_edit == EDIT_OVERLAY)
  {
    *new_param = src_board->overlay[offset];
    *new_color = src_board->overlay_color[offset];
  }

  else // EDIT_VLAYER
  {
    offset = x + (y * mzx_world->vlayer_width);

    *new_param = mzx_world->vlayer_chars[offset];
    *new_color = mzx_world->vlayer_colors[offset];
  }
}

static void do_block_command(struct world *mzx_world, int block_command,
 int block_edit, int block_x, int block_y, int block_width, int block_height,
 struct board *block_board, int overlay_edit, int dest_x, int dest_y,
 char *mzm_name_buffer, int current_color)
{
  char *src_char;
  char *src_color;
  int src_width;
  int src_offset;

  struct board *dest_board;
  char *dest_char;
  char *dest_color;
  int dest_width;
  int dest_height;
  int dest_offset;

  // The history size is generally this, but move block adds complications
  int history_x = dest_x;
  int history_y = dest_y;
  int history_width;
  int history_height;
  int history_offset;

  // Trim the block size to fit the destination, but keep the old
  // size so the old block can be cleared during the move command
  int original_width = block_width;
  int original_height = block_height;
  int place_width = block_width;
  int place_height = block_height;

  // Set up source
  switch(block_edit)
  {
    case EDIT_BOARD:
      src_char = NULL;
      src_color = block_board->level_color;
      src_width = block_board->board_width;
      break;

    case EDIT_OVERLAY:
      src_char = block_board->overlay;
      src_color = block_board->overlay_color;
      src_width = block_board->board_width;
      break;

    case EDIT_VLAYER:
      src_char = mzx_world->vlayer_chars;
      src_color = mzx_world->vlayer_colors;
      src_width = mzx_world->vlayer_width;
      break;

    default:
      return;
  }

  // Set up destination
  switch(overlay_edit)
  {
    case EDIT_BOARD:
      dest_board = mzx_world->current_board;
      dest_char = NULL;
      dest_color = NULL;
      dest_width = dest_board->board_width;
      dest_height = dest_board->board_height;
      break;

    case EDIT_OVERLAY:
      dest_board = mzx_world->current_board;
      dest_char = dest_board->overlay;
      dest_color = dest_board->overlay_color;
      dest_width = dest_board->board_width;
      dest_height = dest_board->board_height;
      break;

    case EDIT_VLAYER:
      dest_board = NULL;
      dest_char = mzx_world->vlayer_chars;
      dest_color = mzx_world->vlayer_colors;
      dest_width = mzx_world->vlayer_width;
      dest_height = mzx_world->vlayer_height;
      break;

    default:
      return;
  }

  // Trim the block for the destination
  if((dest_x + place_width) > dest_width)
    place_width = dest_width - dest_x;

  if((dest_y + place_height) > dest_height)
    place_height = dest_height - dest_y;

  history_width = place_width;
  history_height = place_height;

  // TODO In-place move block adds some complications to history
  // For now, just save the smallest possible space containing both blocks
  if((block_command == 2) &&
   (block_edit == overlay_edit) &&
   ((overlay_edit == EDIT_VLAYER) || (block_board == dest_board)))
  {
    if(block_x > dest_x)
    {
      history_x = dest_x;
      history_width = place_width + block_x - dest_x;
    }
    else
    {
      history_x = block_x;
      history_width = place_width + dest_x - block_x;
    }

    if(block_y > dest_y)
    {
      history_y = dest_y;
      history_height = place_height + block_y - dest_y;
    }
    else
    {
      history_y = block_y;
      history_height = place_height + dest_y - block_y;
    }
  }

  src_offset = block_x + (block_y * src_width);
  dest_offset = dest_x + (dest_y * dest_width);
  history_offset = history_x + (history_y * dest_width);

  // Perform the block action
  if(overlay_edit == EDIT_BOARD)
  {
    add_block_undo_frame(mzx_world, board_history, dest_board,
     history_offset, history_width, history_height);

    switch(block_command)
    {
      case 0:
      case 1:
      {
        // Copy block
        copy_board_to_board(mzx_world,
         block_board, src_offset,
         dest_board, dest_offset,
         place_width, place_height);
        break;
      }

      case 2:
      {
        // Move block
        move_board_block(mzx_world,
         block_board, src_offset,
         dest_board, dest_offset,
         place_width, place_height,
         original_width, original_height);

        // Work around to move the player
        if((mzx_world->player_x >= block_x) &&
         (mzx_world->player_y >= block_y) &&
         (mzx_world->player_x < (block_x + block_width)) &&
         (mzx_world->player_y < (block_y + block_height)) &&
         (block_board == dest_board))
        {
          place_player_xy(mzx_world,
           mzx_world->player_x - block_x + dest_x,
           mzx_world->player_y - block_y + dest_y);
        }
        break;
      }

      case 3:
      {
        // Clear block
        clear_board_block(block_board, src_offset,
         block_width, block_height);
        break;
      }

      case 4:
      {
        // Flip block
        flip_board_block(block_board, src_offset,
         block_width, block_height);
        break;
      }

      case 5:
      {
        // Mirror block
        mirror_board_block(block_board, src_offset,
         block_width, block_height);
        break;
      }

      case 6:
      {
        // Paint block
        paint_layer_block(
         src_color, src_width, src_offset,
         block_width, block_height, current_color);
        break;
      }

      case 7:
      {
        // Copy from overlay/vlayer
        if(block_edit != EDIT_BOARD)
        {
          int convert_select = layer_to_board_object_type(mzx_world);
          enum thing convert_id = NO_ID;

          switch(convert_select)
          {
            case 0:
              convert_id = CUSTOM_BLOCK;
              break;

            case 1:
              convert_id = CUSTOM_FLOOR;
              break;

            case 2:
              convert_id = __TEXT;
              break;
          }

          if(convert_select != -1)
          {
            copy_layer_to_board(
             src_char, src_color, src_width, src_offset,
             dest_board, dest_offset,
             place_width, place_height, convert_id);
          }
        }
        else
        {
          warn("Invalid block action! (board, 7)\n");
        }
        break;
      }

      case 8:
      {
        // There is no block action 8 with the board as the destination
        warn("Invalid block action! (board, 8)\n");
        break;
      }

      case 10:
      {
        // Load MZM
        load_mzm(mzx_world, mzm_name_buffer, dest_x, dest_y, overlay_edit, 0);
        break;
      }
    }

    update_undo_frame(board_history);
  }
  else

  if(overlay_edit == EDIT_OVERLAY)
  {
    add_layer_undo_frame(overlay_history, dest_char, dest_color, dest_width,
     history_offset, history_width, history_height);

    switch(block_command)
    {
      case 0:
      case 1:
      {
        // Copy block
        copy_layer_to_layer(
         src_char, src_color, src_width, src_offset,
         dest_char, dest_color, dest_width, dest_offset,
         place_width, place_height);
        break;
      }

      case 2:
      {
        move_layer_block(
         src_char, src_color, src_width, src_offset,
         dest_char, dest_color, dest_width, dest_offset,
         place_width, place_height,
         original_width, original_height);
        break;
      }

      case 3:
      {
        // Clear block
        clear_layer_block(
         src_char, src_color, src_width, src_offset,
         block_width, block_height);
        break;
      }

      case 4:
      {
        // Flip block
        flip_layer_block(
         src_char, src_color, src_width, src_offset,
         block_width, block_height);
        break;
      }

      case 5:
      {
        // Mirror block
        mirror_layer_block(
         src_char, src_color, src_width, src_offset,
         block_width, block_height);
        break;
      }

      case 6:
      {
        // Paint block
        paint_layer_block(
         src_color, src_width, src_offset,
         block_width, block_height, current_color);
        break;
      }

      case 7:
      {
        // Copy from board
        copy_board_to_layer(
         block_board, src_offset,
         dest_char, dest_color, dest_width, dest_offset,
         place_width, place_height);
        break;
      }

      case 8:
      {
        // Copy from vlayer
        copy_layer_to_layer(
         src_char, src_color, src_width, src_offset,
         dest_char, dest_color, dest_width, dest_offset,
         place_width, place_height);
        break;
      }

      case 10:
      {
        // Load MZM
        load_mzm(mzx_world, mzm_name_buffer, dest_x, dest_y, overlay_edit, 0);
        break;
      }
    }

    update_undo_frame(overlay_history);
  }

  else // EDIT_VLAYER
  {
    add_layer_undo_frame(vlayer_history, dest_char, dest_color, dest_width,
     history_offset, history_width, history_height);

    switch(block_command)
    {
      case 0:
      case 1:
      {
        // Copy block
        copy_layer_to_layer(
         src_char, src_color, src_width, src_offset,
         dest_char, dest_color, dest_width, dest_offset,
         place_width, place_height);
        break;
      }

      case 2:
      {
        // Move block
        move_layer_block(
         src_char, src_color, src_width, src_offset,
         dest_char, dest_color, dest_width, dest_offset,
         place_width, place_height,
         original_width, original_height);
        break;
      }

      case 3:
      {
        // Clear block
        clear_layer_block(
         src_char, src_color, src_width, src_offset,
         block_width, block_height);
        break;
      }

      case 4:
      {
        // Flip block
        flip_layer_block(
         src_char, src_color, src_width, src_offset,
         block_width, block_height);
        break;
      }

      case 5:
      {
        // Mirror block
        mirror_layer_block(
         src_char, src_color, src_width, src_offset,
         block_width, block_height);
        break;
      }

      case 6:
      {
        // Paint block
        paint_layer_block(
         src_color, src_width, src_offset,
         block_width, block_height, current_color);
        break;
      }

      case 7:
      {
        // No block action 7 with a destination of the vlayer exists
        warn("Invalid block action! (vlayer, 7)\n");
        break;
      }

      case 8:
      {
        // Copy from board/overlay
        if(block_edit == EDIT_BOARD)
        {
          copy_board_to_layer(
           block_board, src_offset,
           dest_char, dest_color, dest_width, dest_offset,
           place_width, place_height);
        }
        else

        if(block_edit == EDIT_OVERLAY)
        {
          copy_layer_to_layer(
           src_char, src_color, src_width, src_offset,
           dest_char, dest_color, dest_width, dest_offset,
           place_width, place_height);
        }
        break;
      }

      case 10:
      {
        // Load MZM
        load_mzm(mzx_world, mzm_name_buffer, dest_x, dest_y, overlay_edit, 0);
        break;
      }
    }

    update_undo_frame(vlayer_history);
  }
}

static void thing_menu(struct world *mzx_world, int menu_number,
 enum thing *new_id, int *new_color, int *new_param,
 struct robot *copy_robot, struct scroll *copy_scroll,
 struct sensor *copy_sensor, int use_default_color, int x, int y)
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

    if(def_colors[id] && use_default_color)
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
       x, y, copy_robot, copy_scroll, copy_sensor, EDIT_BOARD, 1);

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

  blank_layers();

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
  select_layer(UI_LAYER);
}

static void draw_vlayer_window(struct world *mzx_world, int array_x, int array_y,
 int window_height)
{
  int offset = array_x + (array_y * mzx_world->vlayer_width);
  int skip;

  char *vlayer_chars = mzx_world->vlayer_chars;
  char *vlayer_colors = mzx_world->vlayer_colors;
  int viewport_height = window_height;
  int viewport_width = 80;
  int x;
  int y;

  blank_layers();

  select_layer(BOARD_LAYER);

  if(mzx_world->vlayer_width < viewport_width)
    viewport_width = mzx_world->vlayer_width;

  if(mzx_world->vlayer_height < viewport_height)
   viewport_height = mzx_world->vlayer_height;

  skip = mzx_world->vlayer_width - viewport_width;

  for(y = 0; y < viewport_height; y++)
  {
    for(x = 0; x < viewport_width; x++)
    {
      draw_char_ext(vlayer_chars[offset], vlayer_colors[offset], x, y, 0, 0);
      offset++;
    }
    offset += skip;
  }

  select_layer(UI_LAYER);
}

static void draw_out_of_bounds(int in_x, int in_y, int in_width, int in_height)
{
  int offset = 0;
  int start;
  int skip;
  int x;
  int y;

  if(in_x + in_width > SCREEN_W)
    in_width = SCREEN_W - in_x;

  if(in_y + in_height > SCREEN_H)
    in_height = SCREEN_H - in_y;

  start = in_x + (in_y * in_width);
  skip = SCREEN_W - in_width;

  // Clear everything before the in-bounds area
  while(offset < start)
  {
    draw_char_linear_ext(1, 177, offset, PRO_CH, 16);
    offset++;
  }

  // Clear everything between the in-bounds area
  for(y = 0; y < in_height; y++)
  {
    offset += in_width;

    for(x = 0; x < skip; x++)
    {
      draw_char_linear_ext(1, 177, offset, PRO_CH, 16);
      offset++;
    }
  }

  // Clear everything after the in-bounds area
  while(offset < SCREEN_W * SCREEN_H)
  {
    draw_char_linear_ext(1, 177, offset, PRO_CH, 16);
    offset++;
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

static void draw_menu_status(int overlay_edit, int line, int draw_mode,
 int current_color, int current_id, char sensor_char, char robot_char,
 char *level_id, char *level_param, int current_param, struct board *src_board,
 int use_default_color)
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
    erase_char(display_next_pos+1, line);

    select_layer(OVERLAY_LAYER);
    draw_char_ext(display_char, display_color,
     display_next_pos + 1, line, 0, 0);
    select_layer(UI_LAYER);

    draw_char(' ', 7, display_next_pos + 2, line);
    display_next_pos += 4;

    draw_char('(', EC_CURR_THING, display_next_pos, line);
    draw_color_box(display_color, 0, display_next_pos + 1, line, 80);
    display_next_pos += 5;

    if(overlay_edit)
    {
      // "Character" is in the overlay/vlayer arrays; could be pulled from there
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
    display_next_pos++;

    if(!use_default_color)
    {
      draw_char('\x07', EC_DEFAULT_COLOR, display_next_pos, line);
    }
  }
}

static void draw_menu_normal(int overlay_edit, int draw_mode, int current_menu,
 int current_color, int current_id, char sensor_char, char robot_char,
 char *level_id, char *level_param, int current_param, struct board *src_board,
 int use_default_color)
{
  draw_window_box(0, 19, 79, 24, EC_MAIN_BOX, EC_MAIN_BOX_DARK,
   EC_MAIN_BOX_CORNER, 0, 1);
  draw_window_box(0, 21, 79, 24, EC_MAIN_BOX, EC_MAIN_BOX_DARK,
   EC_MAIN_BOX_CORNER, 0, 1);

  if(overlay_edit == EDIT_BOARD)
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
    write_string("Pgup/Pgdn:Menu", 64, 24, EC_CURR_PARAM, 1);
  }
  else

  if(overlay_edit == EDIT_OVERLAY)
  {
    write_string(overlay_menu_lines[0], 1, 20, EC_MENU_NAME, 1);
    write_string(overlay_menu_lines[1], 1, 22, EC_OPTION, 1);
    write_string(overlay_menu_lines[2], 1, 23, EC_OPTION, 1);
  }

  else // EDIT_VLAYER
  {
    write_string(vlayer_menu_lines[0], 1, 20, EC_MENU_NAME, 1);
    write_string(vlayer_menu_lines[1], 1, 22, EC_OPTION, 1);
    write_string(vlayer_menu_lines[2], 1, 23, EC_OPTION, 1);
  }

  draw_menu_status(overlay_edit, EDIT_SCREEN_NORMAL + 1, draw_mode,
   current_color, current_id, sensor_char, robot_char, level_id,
   level_param, current_param, src_board, use_default_color);

  draw_char(196, EC_MAIN_BOX_CORNER, 78, 21);
  draw_char(217, EC_MAIN_BOX_DARK, 79, 21);
}

static void draw_menu_minimal(int overlay_edit, int draw_mode,
 int current_color, int current_id, char sensor_char, char robot_char,
 char *level_id, char *level_param, int current_param, struct board *src_board,
 int use_default_color, int cursor_board_x, int cursor_board_y)
{
  int i;

  for(i = 0; i < 80; i++)
    draw_char_ext(' ', EC_MAIN_BOX, i, EDIT_SCREEN_EXTENDED, PRO_CH, 16);

  // Display robot memory where the Alt+H message would usually go.
  if(draw_memory_timer > 0)
  {
    int robot_mem = 0;

    for(i = 0; i < src_board->num_robots_active; i++)
    {
      robot_mem +=
#ifdef CONFIG_DEBYTECODE
       (src_board->robot_list_name_sorted[i])->program_source_length;
#else
       (src_board->robot_list_name_sorted[i])->program_bytecode_length;
#endif
    }

    robot_mem = (robot_mem + 512) / 1024;

    write_string("Robot mem:       kb", 2, EDIT_SCREEN_EXTENDED, EC_MODE_STR, 1);
    write_number(robot_mem, 31, 2+11, EDIT_SCREEN_EXTENDED, 6, 0, 10);
  }
  else

  // Display the board mod where the Alt+H message would usually go.
  if(draw_mod_timer > 0)
  {
    char *mod_name = src_board->mod_playing;
    int len = strlen(mod_name);
    int off = 0;
    char temp;

    write_string("Mod:               ", 2, EDIT_SCREEN_EXTENDED, EC_MODE_STR, 1);

    if(len > 14)
      off = MIN((DRAW_MOD_TIMER_MAX - draw_mod_timer)/10, len-14);

    temp = mod_name[off+14];
    mod_name[off+14] = 0;
    write_string(mod_name+off, 2+5, EDIT_SCREEN_EXTENDED, 31, 1);
    mod_name[off+14] = temp;
  }

  // Display the Alt+H message.
  else
  {
    write_string(minimal_help_mode_mesg[overlay_edit], 2,
     EDIT_SCREEN_EXTENDED, EC_OPTION, 1);
  }

  write_string("X/Y:      /     ", 3+21, EDIT_SCREEN_EXTENDED,
   EC_MODE_STR, 1);

  write_number(cursor_board_x, 31, 3+21+5, EDIT_SCREEN_EXTENDED, 5, 0, 10);
  write_number(cursor_board_y, 31, 3+21+11, EDIT_SCREEN_EXTENDED, 5, 0, 10);

  draw_menu_status(overlay_edit, EDIT_SCREEN_EXTENDED, draw_mode,
   current_color, current_id, sensor_char, robot_char, level_id,
   level_param, current_param, src_board, use_default_color);
}

static void __edit_world(struct world *mzx_world, int reload_curr_file)
{
  struct board *src_board;
  struct robot copy_robot;
  struct scroll copy_scroll;
  struct sensor copy_sensor;

  // Editor state
  int i;
  int key;
  int fade;
  int exit = 0;
  int modified = 0;
  int prev_modified = 0;
  int new_board = -1;
  int first_board_prompt = 0;

  // Current board and cursor
  int overlay_edit = EDIT_BOARD;
  int board_width, board_height;
  int cursor_board_x = 0, cursor_board_y = 0;
  int cursor_move_x = 0, cursor_move_y = 0;
  int cursor_x = 0, cursor_y = 0;
  int scroll_x = 0, scroll_y = 0;
  char *level_id;
  char *level_param;
  char *level_color;
  char *overlay;
  char *overlay_color;

  // Vlayer
  char *vlayer_chars;
  char *vlayer_colors;
  // Buffers for board values while in vlayer editing and vice versa
  int cursor_vlayer_x = 0, cursor_vlayer_y = 0;
  int vscroll_x = 0, vscroll_y = 0;

  // Buffer
  enum thing current_id = SPACE;
  int current_color = 7;
  int current_param = 0;
  int use_default_color = 1;

  // Text mode
  int draw_mode = 0;
  int text_place;
  int text_start_x = -1;

  // Block mode
  int block_command = -1;
  int block_edit = -1;
  int block_x = -1;
  int block_y = -1;
  int block_width = -1;
  int block_height = -1;
  struct board *block_board = NULL;

  // Undo
  int clear_board_history = 1;
  int clear_overlay_history = 1;
  int clear_vlayer_history = 1;
  int continue_mouse_history = 0;

  // Interface
  int saved_overlay_mode;
  int edit_screen_height;
  int current_menu = 0;
  int show_level = 1;
  int debug_x = 60;

  // Backups
  int backup_count = mzx_world->editor_conf.backup_count;
  unsigned int backup_interval = mzx_world->editor_conf.backup_interval;
  char *backup_name = mzx_world->editor_conf.backup_name;
  char *backup_ext = mzx_world->editor_conf.backup_ext;
  int backup_timestamp = get_ticks();
  int backup_num = 0;

  char current_world[MAX_PATH] = { 0 };
  char mzm_name_buffer[MAX_PATH] = { 0 };
  char current_listening_dir[MAX_PATH] = { 0 };
  char current_listening_mod[MAX_PATH] = { 0 };
  int listening_flag = 0;

  const char *mzb_ext[] = { ".MZB", NULL };
  const char *mzm_ext[] = { ".MZM", NULL };
  const char *sfx_ext[] = { ".SFX", NULL };
  const char *chr_ext[] = { ".CHR", NULL };
  const char *pal_ext[] = { ".PAL", NULL };
  const char *idx_ext[] = { ".PALIDX", NULL };

  getcwd(current_listening_dir, MAX_PATH);

  chdir(config_dir);
  set_config_from_file(&(mzx_world->conf), "editor.cnf");
  chdir(current_listening_dir);

  copy_robot.used = 0;
  copy_sensor.used = 0;
  copy_scroll.used = 0;

  mzx_world->version = WORLD_VERSION;

  set_palette_intensity(100);

  end_module();

  debug("%s\n", curr_file);

  {
    struct stat stat_res;

    if(curr_file[0] &&
     stat(curr_file, &stat_res))
      curr_file[0] = '\0';

    if(
     reload_curr_file &&
     curr_file[0] &&
     editor_reload_world(mzx_world, curr_file, &fade))
    {
      strncpy(current_world, curr_file, MAX_PATH);

      mzx_world->current_board_id = mzx_world->first_board;
      mzx_world->current_board =
       mzx_world->board_list[mzx_world->current_board_id];
      src_board = mzx_world->current_board;

      fix_mod(mzx_world, src_board, listening_flag);
    }
    else
    {
      current_world[0] = 0;

      if(mzx_world->active)
      {
        clear_world(mzx_world);
        clear_global_data(mzx_world);
      }
      mzx_world->active = 1;

      create_blank_world(mzx_world);

      // Prompt for the creation of a first board.
      // Holding alt while opening the editor bypasses this.
      if(!get_alt_status(keycode_internal))
        first_board_prompt = 1;

      default_palette();
    }
  }

  m_show();

  synchronize_board_values(mzx_world, &src_board, &board_width, &board_height,
   &level_id, &level_param, &level_color, &overlay, &overlay_color,
   &vlayer_chars, &vlayer_colors, overlay_edit);

  fix_caption(mzx_world, modified);
  update_screen();

  insta_fadein();

  if(mzx_world->editor_conf.bedit_hhelp)
    edit_screen_height = EDIT_SCREEN_EXTENDED;
  else
    edit_screen_height = EDIT_SCREEN_NORMAL;

  mzx_world->editing = true;

  do
  {
    find_player(mzx_world);

    if((backup_count) &&
     ((get_ticks() - backup_timestamp) > (backup_interval * 1000)))
    {
      char backup_name_formatted[MAX_PATH];
      snprintf(backup_name_formatted, MAX_PATH,
       "%s%d%s", backup_name, backup_num + 1, backup_ext);

      backup_name_formatted[MAX_PATH - 1] = 0;

      create_path_if_not_exists(backup_name);

      save_world(mzx_world, backup_name_formatted, 0, WORLD_VERSION);
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

    if(overlay_edit == EDIT_BOARD)
    {
      src_board->overlay_mode = 0;
      draw_edit_window(src_board, scroll_x, scroll_y,
       edit_screen_height);
    }
    else

    if(overlay_edit == EDIT_OVERLAY)
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

    else // EDIT_VLAYER
    {
      draw_vlayer_window(mzx_world, scroll_x, scroll_y,
       edit_screen_height);
    }

    draw_out_of_bounds(0, 0, board_width, board_height);

    src_board->overlay_mode = saved_overlay_mode;

    if(edit_screen_height == EDIT_SCREEN_NORMAL)
    {
      draw_memory_timer = 0;
      draw_mod_timer = 0;
      draw_menu_normal(overlay_edit, draw_mode, current_menu, current_color,
       current_id, copy_sensor.sensor_char, copy_robot.robot_char,
       level_id, level_param, current_param, src_board, use_default_color);
    }
    else
    {
      draw_menu_minimal(overlay_edit, draw_mode, current_color, current_id,
       copy_sensor.sensor_char, copy_robot.robot_char, level_id, level_param,
       current_param, src_board, use_default_color, cursor_board_x,
       cursor_board_y);
    }

    if(draw_memory_timer > 0)
      draw_memory_timer--;

    if(draw_mod_timer > 0)
      draw_mod_timer--;

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
       cursor_board_x, cursor_board_y, 0);
    }

    if(clear_board_history)
    {
      clear_board_history = 0;
      destruct_undo_history(board_history);
      board_history =
       construct_board_undo_history(mzx_world->editor_conf.undo_history_size);
    }

    if(clear_overlay_history)
    {
      clear_overlay_history = 0;
      destruct_undo_history(overlay_history);
      overlay_history =
       construct_layer_undo_history(mzx_world->editor_conf.undo_history_size);
    }

    if(clear_vlayer_history)
    {
      clear_vlayer_history = 0;
      destruct_undo_history(vlayer_history);
      vlayer_history =
       construct_layer_undo_history(mzx_world->editor_conf.undo_history_size);
    }

    text_place = 0;

    update_screen();

    if(first_board_prompt)
    {
      // If the user creates a second board, name the title board
      // for disambiguation.

      if(add_board(mzx_world, 1) >= 0)
      {
        strcpy(mzx_world->name, NEW_WORLD_TITLE);
        strcpy(mzx_world->board_list[0]->board_name, NEW_WORLD_TITLE);
        mzx_world->first_board = 1;
        new_board = 1;
      }

      first_board_prompt = 0;
      continue;
    }

    update_event_status_delay();
    key = get_key(keycode_internal_wrt_numlock);

    // Exit event - ignore other input
    exit = get_exit_status();
    if(exit)
      key = 0;

    // Mouse history frame interrupted (by key or non-left press)?
    if(continue_mouse_history && (key ||
     get_mouse_status() != MOUSE_BUTTON(MOUSE_BUTTON_LEFT)))
    {
      // Finalize frame
      update_undo_frame(board_history);
      continue_mouse_history = 0;
    }

    if(get_mouse_status() && !key)
    {
      int mouse_press = get_mouse_press_ext();
      int mouse_status = get_mouse_status();
      int mouse_x;
      int mouse_y;

      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_y == 20) && (edit_screen_height < 20))
      {
        if((mouse_x >= 1) && (mouse_x <= 41))
        {
          // Select current help menu
          current_menu = menu_positions[mouse_x - 1] - '1';
        }
        else

        if((mouse_x >= 56) && (mouse_x <= 58))
        {
          // Change current color
          int new_color = color_selection(current_color, 0);
          if(new_color >= 0)
            current_color = new_color;
        }
      }
      else

      if(mouse_y < edit_screen_height)
      {
        cursor_board_x = mouse_x + scroll_x;
        cursor_board_y = mouse_y + scroll_y;

        fix_cursor(&cursor_board_x, &cursor_board_y, scroll_x, scroll_y,
         &debug_x, board_width, board_height);

        if((cursor_board_x == mouse_x + scroll_x) &&
         (cursor_board_y == mouse_y + scroll_y))
        {
          // Mouse is in-bounds

          if(mouse_status & MOUSE_BUTTON(MOUSE_BUTTON_RIGHT))
          {
            grab_at_xy(mzx_world, &current_id, &current_color,
             &current_param, &copy_robot, &copy_scroll, &copy_sensor,
             cursor_board_x, cursor_board_y, overlay_edit);
          }
          else

          if(mouse_press == MOUSE_BUTTON_LEFT)
          {
            if(!continue_mouse_history)
            {
              // Add new frame
              add_board_undo_frame(mzx_world, board_history, current_id,
               current_color, current_param, cursor_board_x, cursor_board_y,
               &copy_robot, &copy_scroll, &copy_sensor);
              continue_mouse_history = 1;
            }
            else
            {
              // Continue frame
              add_board_undo_position(board_history,
               cursor_board_x, cursor_board_y);
            }

            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit, 0);
            modified = 1;
          }
        }
      }
    }

    // Saved positions
    // These don't really make sense on the vlayer as-is, but could be adapted.
    if((key >= IKEY_0) && (key <= IKEY_9) && overlay_edit != EDIT_VLAYER)
    {
      struct editor_config_info *conf = &(mzx_world->editor_conf);
      char mesg[80];

      int s_num = key - IKEY_0;

      if(get_ctrl_status(keycode_internal))
      {
        sprintf(mesg, "Save '%s' @ %d,%d to pos %d?",
         mzx_world->current_board->board_name, cursor_board_x,
         cursor_board_y, s_num);

        if(!confirm(mzx_world, mesg))
        {
          conf->saved_board[s_num] = mzx_world->current_board_id;
          conf->saved_cursor_x[s_num] = cursor_board_x;
          conf->saved_cursor_y[s_num] = cursor_board_y;
          conf->saved_scroll_x[s_num] = scroll_x;
          conf->saved_scroll_y[s_num] = scroll_y;
          conf->saved_debug_x[s_num] = debug_x;
          save_local_editor_config(conf, curr_file);
        }
      }
      else

      if(get_alt_status(keycode_internal))
      {
        int s_board = conf->saved_board[s_num];

        if(s_board < mzx_world->num_boards &&
         mzx_world->board_list[s_board])
        {
          sprintf(mesg, "Go to '%s' @ %d,%d (pos %d)?",
           mzx_world->board_list[s_board]->board_name,
           conf->saved_cursor_x[s_num],
           conf->saved_cursor_y[s_num],
           s_num);

          if(!confirm(mzx_world, mesg))
          {
            cursor_board_x = conf->saved_cursor_x[s_num];
            cursor_board_y = conf->saved_cursor_y[s_num];
            scroll_x = conf->saved_scroll_x[s_num];
            scroll_y = conf->saved_scroll_y[s_num];
            debug_x = conf->saved_debug_x[s_num];

            if(mzx_world->current_board_id != s_board)
            {
              fix_board(mzx_world, s_board);
              synchronize_board_values(mzx_world, &src_board, &board_width,
               &board_height, &level_id, &level_param, &level_color, &overlay,
               &overlay_color, &vlayer_chars, &vlayer_colors, overlay_edit);
              fix_mod(mzx_world, src_board, listening_flag);
            }

            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &debug_x, board_width, board_height, edit_screen_height);
          }
        }
      }
    }

    switch(key)
    {
      case IKEY_UP:
      {
        if(get_shift_status(keycode_internal))
        {
          // Move to adjacent board
          if(src_board->board_dir[0] != NO_BOARD && overlay_edit != EDIT_VLAYER)
            new_board = src_board->board_dir[0];
          break;
        }
        else

        cursor_move_y = -1;

        if(get_alt_status(keycode_internal))
          cursor_move_y = -10;

        if(get_ctrl_status(keycode_internal) &&
         (draw_mode == 4) && (block_height >= 0))
          cursor_move_y = -block_height;

        break;
      }

      case IKEY_DOWN:
      {
        if(get_shift_status(keycode_internal))
        {
          // Move to adjacent board
          if(src_board->board_dir[1] != NO_BOARD && overlay_edit != EDIT_VLAYER)
            new_board = src_board->board_dir[1];
          break;
        }

        cursor_move_y = 1;

        if(get_alt_status(keycode_internal))
          cursor_move_y = 10;

        if(get_ctrl_status(keycode_internal) &&
         (draw_mode == 4) && (block_height >= 0))
          cursor_move_y = block_height;

        break;
      }

      case IKEY_LEFT:
      {
        if(get_shift_status(keycode_internal))
        {
          // Move to adjacent board
          if(src_board->board_dir[3] != NO_BOARD && overlay_edit != EDIT_VLAYER)
            new_board = src_board->board_dir[3];
          break;
        }

        cursor_move_x = -1;

        if(get_alt_status(keycode_internal))
          cursor_move_x = -10;

        if(get_ctrl_status(keycode_internal) &&
         (draw_mode == 4) && (block_width >= 0))
          cursor_move_x = -block_width;

        break;
      }

      case IKEY_RIGHT:
      {
        if(get_shift_status(keycode_internal))
        {
          // Move to adjacent board
          if(src_board->board_dir[2] != NO_BOARD && overlay_edit != EDIT_VLAYER)
            new_board = src_board->board_dir[2];
          break;
        }

        cursor_move_x = 1;

        if(get_alt_status(keycode_internal))
          cursor_move_x = 10;

        if(get_ctrl_status(keycode_internal) &&
         (draw_mode == 4) && (block_width >= 0))
          cursor_move_x = block_width;

        break;
      }

      case IKEY_SPACE:
      {
        if(draw_mode == 2)
        {
          place_current_at_xy(mzx_world, __TEXT, current_color,
           ' ', cursor_board_x, cursor_board_y, &copy_robot,
           &copy_scroll, &copy_sensor, overlay_edit, 1);

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
             overlay_edit, 1);
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
               &copy_robot, &copy_scroll, &copy_sensor, overlay_edit, 1);
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
           &copy_scroll, &copy_sensor, overlay_edit, 1);

          modified = 1;
        }
        else
        {
          int offset = cursor_board_x + (board_width * cursor_board_y);

          if(overlay_edit)
          {
            // Delete under cursor (overlay and vlayer)
            place_current_at_xy(mzx_world, SPACE, 7, 32, cursor_board_x,
             cursor_board_y, &copy_robot, &copy_scroll, &copy_sensor,
             overlay_edit, 1);

            modified = 1;
          }
          else

          if(level_id[offset] != PLAYER)
          {
            // Remove from top (board only)
            add_block_undo_frame(mzx_world, board_history,
             src_board, offset, 1, 1);

            id_remove_top(mzx_world, cursor_board_x, cursor_board_y);

            update_undo_frame(board_history);
            modified = 1;
          }
        }
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
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit, 1);
            modified = 1;
          }
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

      case IKEY_DELETE:
      {
        if(draw_mode == 2)
        {
          place_current_at_xy(mzx_world, __TEXT, current_color, ' ',
           cursor_board_x, cursor_board_y, &copy_robot,
           &copy_scroll, &copy_sensor, overlay_edit, 1);
        }
        else
        {
          int new_param = 0;

          // On layers, the blank char is a space char
          if(overlay_edit) new_param = 32;

          place_current_at_xy(mzx_world, SPACE, 7, new_param, cursor_board_x,
           cursor_board_y, &copy_robot, &copy_scroll, &copy_sensor,
           overlay_edit, 1);
        }
        modified = 1;
        break;
      }

      case IKEY_HOME:
      {
        cursor_board_x = 0;
        cursor_board_y = 0;
        scroll_x = 0;
        scroll_y = 0;

        if((cursor_board_x - scroll_x) < (debug_x + 25))
          debug_x = 60;

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
          debug_x = 0;

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

      case IKEY_ESCAPE:
      {
        if(draw_mode)
        {
          draw_mode = 0;
          key = 0;
        }
        else

        if(overlay_edit)
        {
          if(overlay_edit == EDIT_VLAYER)
          {
            // Disable vlayer mode and fix the display
            set_vlayer_mode(EDIT_BOARD, &overlay_edit,
             &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color,
             &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
             overlay_edit);

            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
             &scroll_y, &debug_x, board_width, board_height,
             edit_screen_height);
          }

          overlay_edit = EDIT_BOARD;
          current_id = SPACE;
          current_param = 0;
          current_color = 7;
          key = 0;
        }

        else
        {
          exit = 1;
        }

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
             use_default_color, cursor_board_x, cursor_board_y);
            modified = 1;
          }
        }
        break;
      }

      // Item
      case IKEY_F4:
      {
        // ALT+F4 - do nothing.
        if(get_alt_status(keycode_internal))
          break;

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
             use_default_color, cursor_board_x, cursor_board_y);
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
           use_default_color, cursor_board_x, cursor_board_y);
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
           use_default_color, cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      // Transport
      case IKEY_F7:
      {
        if(!overlay_edit)
        {
          thing_menu(mzx_world, 4, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           use_default_color, cursor_board_x, cursor_board_y);
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
           use_default_color, cursor_board_x, cursor_board_y);
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
           use_default_color, cursor_board_x, cursor_board_y);
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
           use_default_color, cursor_board_x, cursor_board_y);
          modified = 1;
        }
        break;
      }

      case IKEY_F11:
      {
        if(get_alt_status(keycode_internal))
        {
          // Robot debugger configuration
          debug_robot_config(mzx_world);
        }
        else
        {
          // SMZX Mode
          int selected_mode = select_screen_mode(mzx_world);

          if(selected_mode >= 0)
          {
            set_screen_mode(selected_mode);

            modified = 1;
          }
        }
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
          src_board->mod_playing[0] = '*';
          src_board->mod_playing[1] = 0;
          fix_mod(mzx_world, src_board, listening_flag);
          draw_mod_timer = DRAW_MOD_TIMER_MAX;

          modified = 1;
        }

        break;
      }

      case IKEY_KP_MINUS:
      case IKEY_MINUS:
      {
        if(draw_mode != 2)
        {
          // Previous board
          // Doesn't make sense on the vlayer.
          if(mzx_world->current_board_id > 0 && overlay_edit != EDIT_VLAYER)
            new_board = mzx_world->current_board_id - 1;
        }
        else
          text_place = 1;

        break;
      }

      case IKEY_KP_PLUS:
      case IKEY_EQUALS:
      {
        if(draw_mode != 2)
        {
          // Next board
          // Doesn't make sense on the vlayer.
          if(mzx_world->current_board_id < mzx_world->num_boards - 1
           && overlay_edit != EDIT_VLAYER)
            new_board = mzx_world->current_board_id + 1;
        }
        else
          text_place = 1;

        break;
      }

      case IKEY_RETURN:
      {
        if(draw_mode == 3)
        {
          // Block selection
          block_command = select_block_command(mzx_world, overlay_edit);

          block_edit = overlay_edit;
          block_board = src_board;

          // Fix coords
          if(block_x > cursor_board_x)
          {
            block_width = block_x - cursor_board_x + 1;
            block_x = cursor_board_x;
          }
          else
          {
            block_width = cursor_board_x - block_x + 1;
          }

          if(block_y > cursor_board_y)
          {
            block_height = block_y - cursor_board_y + 1;
            block_y = cursor_board_y;
          }
          else
          {
            block_height = cursor_board_y - block_y + 1;
          }

          // Some of these are done automatically
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
              // Copy/Copy repeated/Move
              draw_mode = 4;
              break;
            }

            case 3:
            case 4:
            case 5:
            case 6:
            {
              // Clear block
              // Flip block
              // Mirror block
              // Paint block

              do_block_command(mzx_world, block_command, block_edit,
               block_x, block_y, block_width, block_height, block_board,
               overlay_edit, block_x, block_y, mzm_name_buffer, current_color);

              draw_mode = 0;
              modified = 1;
              break;
            }

            case 7:
            case 8:
            {
              // Copy to board/overlay/vlayer
              int target_mode;

              if((block_command == 7 && overlay_edit == EDIT_BOARD) ||
               (block_command == 8 && overlay_edit == EDIT_VLAYER))
              {
                if(!src_board->overlay_mode)
                {
                  error("Overlay mode is not on (see Board Info)", 0, 8,
                   0x1103);
                  draw_mode = 0;
                  break;
                }
                else
                {
                  target_mode = EDIT_OVERLAY;
                }
              }
              else

              if(block_command == 7 && overlay_edit != EDIT_BOARD)
              {
                target_mode = EDIT_BOARD;
              }

              else // block_command == 8, overlay_edit != EDIT_VLAYER
              {
                target_mode = EDIT_VLAYER;
              }

              // Exit vlayer mode if necessary.
              set_vlayer_mode(target_mode, &overlay_edit,
               &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
               &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

              synchronize_board_values(mzx_world, &src_board, &board_width,
               &board_height, &level_id, &level_param, &level_color,
               &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
               overlay_edit);

              fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
               &scroll_y, &debug_x, board_width, board_height,
               edit_screen_height);

              overlay_edit = target_mode;
              draw_mode = 4;
              break;
            }

            case 9:
            {
              // Save as MZM
              mzm_name_buffer[0] = 0;

              if(!new_file(mzx_world, mzm_ext, ".mzm", mzm_name_buffer,
               "Export MZM", 1))
              {
                save_mzm(mzx_world, mzm_name_buffer, block_x,
                 block_y, block_width, block_height,
                 overlay_edit, 0);
              }
              draw_mode = 0;
              break;
            }
          }
        }
        else

        if(draw_mode > 3)
        {
          // Block placement
          do_block_command(mzx_world, block_command, block_edit,
           block_x, block_y, block_width, block_height, block_board,
           overlay_edit, cursor_board_x, cursor_board_y, mzm_name_buffer, 0);

          // Reset the block mode unless this is repeat copy
          if(block_command != 1)
            draw_mode = 0;

          modified = 1;
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
              debug_x = 60;
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
                debug_x = 0;
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

        // Normal/draw - modify+get
        else
        {
          int edited_storage = 0;
          int new_param;

          grab_at_xy(mzx_world, &current_id, &current_color,
           &current_param, &copy_robot, &copy_scroll, &copy_sensor,
           cursor_board_x, cursor_board_y, overlay_edit);

          if(overlay_edit == EDIT_BOARD)
          {
            // Board- param
            copy_robot.xpos = cursor_board_x;
            copy_robot.ypos = cursor_board_y;

            new_param =
             change_param(mzx_world, current_id, current_param,
             &copy_robot, &copy_scroll, &copy_sensor);

            if(!is_storageless(current_id))
              edited_storage = 1;
          }
          else
          {
            // Overlay and vlayer- char
            new_param = char_selection(current_param);
          }

          // Ignore non-storage objects with no change.
          if((new_param >= 0 && new_param != current_param) || edited_storage)
          {
            // Place the buffer back on the board/layer
            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, new_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit, 1);

            modified = 1;
          }
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
              new_board = i;
          }
        }
        else
        {
          text_place = 1;
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
            new_board =
             choose_board(mzx_world, mzx_world->current_board_id,
             "Select current board:", 0);

        }
        else
        {
          text_place = 1;
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

      case IKEY_d:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            // Toggle default built-in colors
            use_default_color ^= 1;
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
                new_board = 0;
              }
              else
              {
                synchronize_board_values(mzx_world, &src_board, &board_width,
                 &board_height, &level_id, &level_param, &level_color,
                 &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
                 overlay_edit);
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
            struct undo_history *h = NULL;

            switch(overlay_edit)
            {
              case EDIT_BOARD:
                h = board_history;
                break;

              case EDIT_OVERLAY:
                h = overlay_history;
                break;

              case EDIT_VLAYER:
                h = vlayer_history;
                break;
            }

            fill_area(mzx_world, h, current_id, current_color, current_param,
             cursor_board_x, cursor_board_y, &copy_robot, &copy_scroll,
             &copy_sensor, overlay_edit);
            modified = 1;
          }
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
          if(get_ctrl_status(keycode_internal))
          {
            // Goto board position
            if(!board_goto(mzx_world, overlay_edit,
             &cursor_board_x, &cursor_board_y))
            {
              // This will get fixed by fix_scroll
              scroll_x = cursor_board_x - 39;
              scroll_y = cursor_board_y - edit_screen_height / 2;

              fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
               &debug_x, board_width, board_height, edit_screen_height);
            }
          }
          else

          if(get_alt_status(keycode_internal))
          {
            global_robot(mzx_world);
          }

          else
          {
            global_info(mzx_world);
          }

          fix_caption(mzx_world, modified);
          modified = 1;
        }
        else
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

            if((scroll_y + 24) > board_height)
              scroll_y = board_height - 24;

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

      case IKEY_i:
      {
        if(get_alt_status(keycode_internal))
        {
          int import_number = import_type(mzx_world);
          if(import_number >= 0)
          {
            char import_name[MAX_PATH];
            import_name[0] = 0;

            switch(import_number)
            {
              case 0:
              {
                if(!choose_file(mzx_world, mzb_ext, import_name,
                 "Choose board to import", 1))
                {
                  replace_current_board(mzx_world, import_name);

                  // Exit vlayer mode if necessary.
                  set_vlayer_mode(EDIT_BOARD, &overlay_edit,
                   &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
                   &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

                  synchronize_board_values(mzx_world, &src_board, &board_width,
                   &board_height, &level_id, &level_param, &level_color,
                   &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
                   overlay_edit);

                  fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
                   &scroll_y, &debug_x, board_width, board_height,
                   edit_screen_height);

                  // fixme load_mod_check
                  if(strcmp(src_board->mod_playing, "*") &&
                   strcasecmp(src_board->mod_playing,
                   mzx_world->real_mod_playing))
                    fix_mod(mzx_world, src_board, listening_flag);

                  if((draw_mode > 3) &&
                   (block_board == (mzx_world->current_board)))
                    draw_mode = 0;

                  clear_board_history = 1;
                  clear_overlay_history = 1;

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
                 elements, 1, 2))
                {
                  ec_load_set_var(import_name, char_offset, 0);
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
                if(!choose_file(mzx_world, pal_ext, import_name,
                 "Choose palette to import", 1))
                {
                  load_palette(import_name);
                  update_palette();
                  modified = 1;
                }

                import_name[0] = 0;
                if((get_screen_mode() == 3) &&
                 !choose_file(mzx_world, idx_ext, import_name,
                  "Choose indices to import (.PALIDX)", 1))
                {
                  load_index_file(import_name);
                  update_palette();
                  modified = 1;
                }

                break;
              }

              case 4:
              {
                // Sound effects
                if(!choose_file(mzx_world, sfx_ext, import_name,
                 "Choose SFX file to import", 1))
                {
                  FILE *sfx_file;

                  sfx_file = fopen_unsafe(import_name, "rb");
                  fread(mzx_world->custom_sfx, SFX_SIZE, NUM_SFX, sfx_file);
                  mzx_world->custom_sfx_on = 1;
                  fclose(sfx_file);
                  modified = 1;
                }
                break;
              }

              case 5:
              {
                // MZM file
                if(!choose_file(mzx_world, mzm_ext,
                 mzm_name_buffer, "Choose image file to import", 1))
                {
                  draw_mode = 5;
                  block_command = 10;
                }

                break;
              }
            }
          }
        }
        else

        if(draw_mode != 2)
        {
          if(overlay_edit != EDIT_VLAYER)
          {
            board_info(mzx_world);
            // If this is the first board, patch the title into the world name
            if(mzx_world->current_board_id == 0)
              strcpy(mzx_world->name, src_board->board_name);

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color,
             &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
             overlay_edit);

            fix_caption(mzx_world, modified);

            if(!src_board->overlay_mode && overlay_edit == EDIT_OVERLAY)
            {
              clear_overlay_history = 1;
              overlay_edit = EDIT_BOARD;
            }

            modified = 1;
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
          char test_wav[MAX_PATH] = { 0, };
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
            char prev_world[MAX_PATH];
            char load_world[MAX_PATH];
            strcpy(prev_world, current_world);
            strcpy(load_world, current_world);

            if(!choose_file_ch(mzx_world, world_ext, load_world,
             "Load World", 1))
            {
              int fade;

              // Load world curr_file
              strcpy(current_world, load_world);
              if(!editor_reload_world(mzx_world, current_world, &fade))
              {
                strcpy(current_world, prev_world);

                fix_mod(mzx_world, src_board, listening_flag);
                break;
              }
              strcpy(curr_file, current_world);

              mzx_world->current_board_id = mzx_world->first_board;
              mzx_world->current_board =
               mzx_world->board_list[mzx_world->current_board_id];
              src_board = mzx_world->current_board;

              insta_fadein();

              // Exit vlayer mode if necessary.
              set_vlayer_mode(EDIT_BOARD, &overlay_edit,
               &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
               &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

              synchronize_board_values(mzx_world, &src_board, &board_width,
               &board_height, &level_id, &level_param, &level_color,
               &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
               overlay_edit);

              fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
               &scroll_y, &debug_x, board_width, board_height,
               edit_screen_height);

              fix_mod(mzx_world, src_board, listening_flag);

              if(!src_board->overlay_mode && overlay_edit == EDIT_OVERLAY)
                overlay_edit = EDIT_BOARD;

              if(draw_mode > 3)
                draw_mode = 0;

              clear_board_history = 1;
              clear_overlay_history = 1;
              clear_vlayer_history = 1;

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

      case IKEY_m:
      {
        if(draw_mode != 2)
        {
          // Modify
          if(get_alt_status(keycode_internal))
          {
            int offset = cursor_board_x + (cursor_board_y * board_width);

            if(overlay_edit == EDIT_BOARD)
            {
              enum thing d_id = (enum thing)level_id[offset];
              int d_param = level_param[offset];
              int new_param;

              if(is_storageless(d_id))
              {
                new_param =
                 change_param(mzx_world, d_id, d_param, NULL, NULL, NULL);

                if(new_param >= 0 && new_param != d_param)
                {
                  add_block_undo_frame(mzx_world, board_history,
                   src_board, offset, 1, 1);

                  level_param[offset] = new_param;

                  update_undo_frame(board_history);
                  modified = 1;
                }
              }
              else

              if(d_id != PLAYER)
              {
                add_block_undo_frame(mzx_world, board_history,
                 src_board, offset, 1, 1);

                if(d_id == SENSOR)
                {
                  edit_sensor(mzx_world, src_board->sensor_list[d_param]);
                }
                else

                if(is_robot(d_id))
                {
                  edit_robot(mzx_world, src_board->robot_list[d_param]);
                  draw_memory_timer = DRAW_MEMORY_TIMER_MAX;
                  fix_caption(mzx_world, modified);
                }
                else

                if(is_signscroll(d_id))
                {
                  edit_scroll(mzx_world, src_board->scroll_list[d_param]);
                }

                update_undo_frame(board_history);
                modified = 1;
              }
            }
            else

            if(overlay_edit == EDIT_OVERLAY)
            {
              int new_ch = char_selection(overlay[offset]);

              if(new_ch >= 0)
              {
                add_layer_undo_frame(overlay_history, overlay, overlay_color,
                 board_width, offset, 1, 1);

                overlay[offset] = new_ch;

                update_undo_frame(overlay_history);
                modified = 1;
              }
            }

            else // EDIT_VLAYER
            {
              int new_ch = char_selection(vlayer_chars[offset]);

              if(new_ch >= 0)
              {
                add_layer_undo_frame(vlayer_history, vlayer_chars,
                 vlayer_colors, board_width, offset, 1, 1);

                vlayer_chars[offset] = new_ch;

                update_undo_frame(vlayer_history);
                modified = 1;
              }
            }
          }
          else

          // Move current board
          // Doesn't make sense on the vlayer
          if(overlay_edit != EDIT_VLAYER)
          {
            int new_position;

            if(mzx_world->current_board_id == 0)
              break;

            new_position =
             choose_board(mzx_world, mzx_world->current_board_id,
             "Move board to position:", 0);

            if((new_position > 0) && (new_position < mzx_world->num_boards) &&
             (new_position != mzx_world->current_board_id))
            {
              move_current_board(mzx_world, new_position);
              new_board = new_position;
              modified = 1;
            }
          }
        }
        else
          text_place = 1;

        break;
      }

      case IKEY_n:
      {
        if(draw_mode != 2)
        {
          // Board mod
          // Doesn't make sense on the vlayer
          if(get_alt_status(keycode_internal) && overlay_edit != EDIT_VLAYER)
          {
            if(!src_board->mod_playing[0])
            {
              char new_mod[MAX_PATH] = { 0 };

              if(!choose_file(mzx_world, mod_ext, new_mod,
               "Choose a module file", 2)) // 2:subdirsonly
              {
                strcpy(src_board->mod_playing, new_mod);
                strcpy(mzx_world->real_mod_playing, new_mod);
                fix_mod(mzx_world, src_board, listening_flag);
                draw_mod_timer = DRAW_MOD_TIMER_MAX;
              }
            }
            else
            {
              src_board->mod_playing[0] = 0;
              mzx_world->real_mod_playing[0] = 0;
              fix_mod(mzx_world, src_board, listening_flag);
              draw_mod_timer = DRAW_MOD_TIMER_MAX;
            }
            modified = 1;
          }
          else

          if(get_ctrl_status(keycode_internal))
          {
            if(!listening_flag)
            {
              char current_dir[MAX_PATH];
              char new_mod[MAX_PATH] = { 0 } ;

              getcwd(current_dir, MAX_PATH);
              chdir(current_listening_dir);

              if(!choose_file(mzx_world, mod_gdm_ext, new_mod,
               "Choose a module file (listening only)", 1))
              {
                load_module(new_mod, false, 255);
                strcpy(current_listening_mod, new_mod);
                get_path(new_mod, current_listening_dir, MAX_PATH);
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

      case IKEY_o:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            if(overlay_edit != EDIT_OVERLAY)
            {
              if(!src_board->overlay_mode)
              {
                error("Overlay mode is not on (see Board Info)",
                 0, 8, 0x1103);
              }
              else
              {
                if(overlay_edit == EDIT_VLAYER)
                {
                  // Disable vlayer mode and fix the display
                  set_vlayer_mode(EDIT_OVERLAY, &overlay_edit,
                   &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
                   &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

                  synchronize_board_values(mzx_world, &src_board, &board_width,
                   &board_height, &level_id, &level_param, &level_color,
                   &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
                   overlay_edit);

                  fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
                   &scroll_y, &debug_x, board_width, board_height,
                   edit_screen_height);
                }

                draw_mode = 0;
                overlay_edit = EDIT_OVERLAY;
                current_param = 32;
                current_color = 7;
              }
            }
            else
            {
              draw_mode = 0;
              overlay_edit = EDIT_BOARD;
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

      case IKEY_p:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            // Size and position
            if(overlay_edit == EDIT_VLAYER)
            {
              if(size_pos_vlayer(mzx_world))
                clear_vlayer_history = 1;
            }
            else
            {
              if(size_pos(mzx_world))
              {
                set_update_done_current(mzx_world);

                clear_board_history = 1;
                clear_overlay_history = 1;
              }
            }

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color,
             &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
             overlay_edit);

            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &debug_x, board_width, board_height, edit_screen_height);

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
            // Board- buffer param
            if(current_id < SENSOR)
            {
              int new_param = change_param(mzx_world, current_id,
               current_param, NULL, NULL, NULL);

              if(new_param >= 0)
                current_param = new_param;
            }
          }

          else
          {
            // Layer- buffer char
            int new_param = char_selection(current_param);

            if(new_param >= 0)
              current_param = new_param;
          }
        }
        else
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

            // Disable vlayer editing if we're in it
            set_vlayer_mode(EDIT_BOARD, &overlay_edit,
             &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color, &overlay,
             &overlay_color, &vlayer_chars, &vlayer_colors, overlay_edit);

            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &debug_x, board_width, board_height, edit_screen_height);

            clear_board_history = 1;
            clear_overlay_history = 1;
            clear_vlayer_history = 1;

            end_module();

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

      case IKEY_s:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            if(overlay_edit == EDIT_OVERLAY)
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
            char world_name[MAX_PATH];
            char new_path[MAX_PATH];
            strcpy(world_name, current_world);
            if(!new_file(mzx_world, world_ext, ".mzx", world_name,
             "Save world", 1))
            {
              debug("Save path: %s\n", world_name);

              // It's now officially WORLD_VERSION
              mzx_world->version = WORLD_VERSION;

              // Save entire game
              strcpy(current_world, world_name);
              strcpy(curr_file, current_world);
              save_world(mzx_world, current_world, 0, WORLD_VERSION);

              get_path(world_name, new_path, MAX_PATH);
              if(new_path[0])
                chdir(new_path);

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

      case IKEY_t:
      {
        if(get_alt_status(keycode_internal))
        {
          // Test world
          int fade;
          int current_board_id = mzx_world->current_board_id;

          // Clear undo histories and prepare to reset them
          // We don't want the undo buffer wasting memory while testing
          destruct_undo_history(board_history);
          destruct_undo_history(overlay_history);
          destruct_undo_history(vlayer_history);
          board_history = NULL;
          overlay_history = NULL;
          vlayer_history = NULL;
          clear_board_history = 1;
          clear_overlay_history = 1;
          clear_vlayer_history = 1;

          if(!save_world(mzx_world, "__test.mzx", 0, WORLD_VERSION))
          {
            int world_version = mzx_world->version;
            char *return_dir = cmalloc(MAX_PATH);
            getcwd(return_dir, MAX_PATH);

            vquick_fadeout();
            cursor_off();

            set_update_done(mzx_world);

            // Changes to the board, duplicates if reset on entry
            change_board(mzx_world, current_board_id);
            change_board_load_assets(mzx_world);
            src_board = mzx_world->current_board;

            set_counter(mzx_world, "TIME", src_board->time_limit, 0);
            send_robot_def(mzx_world, 0, LABEL_JUSTENTERED);
            send_robot_def(mzx_world, 0, LABEL_JUSTLOADED);
            find_player(mzx_world);

            mzx_world->player_restart_x = mzx_world->player_x;
            mzx_world->player_restart_y = mzx_world->player_y;

            load_board_module(mzx_world, src_board);

            reset_robot_debugger();

            play_game(mzx_world);

            chdir(return_dir);

            if(!reload_world(mzx_world, "__test.mzx", &fade))
            {
              if(!editor_reload_world(mzx_world, current_world, &fade))
                create_blank_world(mzx_world);

              current_board_id = mzx_world->current_board_id;
              board_width = mzx_world->current_board->board_width;
              board_height = mzx_world->current_board->board_height;

              // Disable vlayer editing if necessary
              set_vlayer_mode(EDIT_BOARD, &overlay_edit,
               &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
               &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

              modified = 0;
            }

            else
            {
              // Set it back to the original version.
              mzx_world->version = world_version;
            }

            m_show();

            scroll_color = 15;
            mzx_world->current_board_id = current_board_id;
            mzx_world->current_board =
             mzx_world->board_list[current_board_id];

            if(draw_mode > 3)
              draw_mode = 0;

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color, &overlay,
             &overlay_color, &vlayer_chars, &vlayer_colors, overlay_edit);

            // Do this in case a different world was loaded/created
            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &debug_x, board_width, board_height, edit_screen_height);

            insta_fadein();

            if(listening_flag)
            {
              load_module(current_listening_mod, false, 255);
            }
            else
            {
              fix_mod(mzx_world, src_board, listening_flag);
            }

            unlink("__test.mzx");

            free(return_dir);
          }
        }
        else

        if(draw_mode == 2)
        {
          text_place = 1;
        }

        break;
      }

      case IKEY_v:
      {
        if(draw_mode != 2)
        {
          if(get_alt_status(keycode_internal))
          {
            // Toggle vlayer editing
            int target_mode = EDIT_VLAYER;

            if(overlay_edit == EDIT_VLAYER)
            {
              target_mode = EDIT_BOARD;
            }

            set_vlayer_mode(target_mode, &overlay_edit,
             &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color,
             &overlay, &overlay_color, &vlayer_chars, &vlayer_colors,
             overlay_edit);

            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
             &scroll_y, &debug_x, board_width, board_height,
             edit_screen_height);

            draw_mode = 0;
            current_id = SPACE;
            current_param = 32;
            current_color = 7;
            break;
          }
          else

          if(overlay_edit != EDIT_VLAYER)
          {
            // View mode
            int viewport_width = src_board->viewport_width;
            int viewport_height = src_board->viewport_height;
            int v_scroll_x = scroll_x;
            int v_scroll_y = CLAMP(scroll_y, 0,
             src_board->board_height - src_board->viewport_height);
            int v_key;

            cursor_off();
            m_hide();

            do
            {
              blank_layers();
              draw_viewport(mzx_world);
              draw_game_window(src_board, v_scroll_x, v_scroll_y);
              update_screen();

              update_event_status_delay();
              v_key = get_key(keycode_internal_wrt_numlock);

              if(get_exit_status())
                v_key = IKEY_ESCAPE;

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
            char export_name[MAX_PATH];
            export_name[0] = 0;

            switch(export_number)
            {
              case 0:
              {
                // Board file
                if(!new_file(mzx_world, mzb_ext, ".mzb", export_name,
                 "Export board file", 1))
                {
                  save_board_file(mzx_world, src_board, export_name);
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
                 "Export character set", 1, 1, elements, 2, 2))
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

                export_name[0] = 0;
                if((get_screen_mode() == 3) &&
                 !new_file(mzx_world, idx_ext, ".palidx", export_name,
                  "Export indices (.PALIDX)", 1))
                {
                  save_index_file(export_name);
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
                      fwrite(mzx_world->custom_sfx, SFX_SIZE, NUM_SFX, sfx_file);
                    else
                      fwrite(sfx_strs, SFX_SIZE, NUM_SFX, sfx_file);

                    fclose(sfx_file);
                  }
                }
                break;
              }

              case 4:
              {
                // Downver. world
                char title[80];

                sprintf(title, "Export world to previous version (%d.%d)",
                 (WORLD_VERSION_PREV >> 8) & 0xFF, WORLD_VERSION_PREV & 0xFF);

                if(!new_file(mzx_world, world_ext, ".mzx", export_name,
                 title, 1))
                {
                  save_world(mzx_world, export_name, 0, WORLD_VERSION_PREV);
                }
              }
            }
          }
        }
        else

        if(draw_mode != 2)
        {
          // Doesn't make sense on the vlayer
          if(overlay_edit != EDIT_VLAYER)
          {
            board_exits(mzx_world);
            modified = 1;
          }
        }
        else
        {
          text_place = 1;
        }
        break;
      }

      case IKEY_y:
      {
        if(get_ctrl_status(keycode_internal))
        {
          // Redo
          if(overlay_edit == EDIT_BOARD)
            modified |= apply_redo(board_history);

          else if(overlay_edit == EDIT_OVERLAY)
            modified |= apply_redo(overlay_history);

          else if(overlay_edit == EDIT_VLAYER)
            modified |= apply_redo(vlayer_history);
        }
        else

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

      case IKEY_z:
      {
        if(get_ctrl_status(keycode_internal))
        {
          // Undo
          if(overlay_edit == EDIT_BOARD)
            modified |= apply_undo(board_history);

          else if(overlay_edit == EDIT_OVERLAY)
            modified |= apply_undo(overlay_history);

          else if(overlay_edit == EDIT_VLAYER)
            modified |= apply_undo(vlayer_history);
        }
        else

        if(get_alt_status(keycode_internal))
        {
          if(overlay_edit == EDIT_VLAYER)
          {
            // Clear vlayer
            if(!confirm(mzx_world, "Clear vlayer - Are you sure?"))
            {
              memset(vlayer_chars, 0, mzx_world->vlayer_size);
              memset(vlayer_colors, 0, mzx_world->vlayer_size);

              if(draw_mode > 3 && (block_command <= 2))
              {
                draw_mode = 0;
              }

              clear_vlayer_history = 1;
            }
          }
          else

          // Clear board
          if(!confirm(mzx_world, "Clear board - Are you sure?"))
          {
            clear_board(src_board);
            src_board = create_blank_board(&(mzx_world->editor_conf));
            mzx_world->current_board = src_board;
            mzx_world->current_board->robot_list[0] = &mzx_world->global_robot;
            mzx_world->board_list[mzx_world->current_board_id] = src_board;
            synchronize_board_values(mzx_world, &src_board, &board_width,
             &board_height, &level_id, &level_param, &level_color, &overlay,
             &overlay_color, &vlayer_chars, &vlayer_colors, overlay_edit);
            fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
             &debug_x, board_width, board_height, edit_screen_height);
            end_module();
            src_board->mod_playing[0] = 0;
            strcpy(mzx_world->real_mod_playing,
             src_board->mod_playing);

            clear_board_history = 1;
            clear_overlay_history = 1;

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

    if(new_board >= 0)
    {
      // Hopefully we won't be getting anything out of range but just in case
      new_board = CLAMP(new_board, 0, mzx_world->num_boards - 1);
      fix_board(mzx_world, new_board);

      if(!src_board->overlay_mode && overlay_edit == EDIT_OVERLAY)
        overlay_edit = EDIT_BOARD;

      // It doesn't make sense to stay in vlayer mode after a board change.
      set_vlayer_mode(EDIT_BOARD, &overlay_edit,
       &cursor_board_x, &cursor_board_y, &scroll_x, &scroll_y,
       &cursor_vlayer_x, &cursor_vlayer_y, &vscroll_x, &vscroll_y);

      synchronize_board_values(mzx_world, &src_board, &board_width,
       &board_height, &level_id, &level_param, &level_color, &overlay,
       &overlay_color, &vlayer_chars, &vlayer_colors, overlay_edit);

      fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
       &scroll_y, &debug_x, board_width, board_height,
       edit_screen_height);

      fix_caption(mzx_world, modified);

      if(strcmp(src_board->mod_playing, "*") &&
       strcasecmp(src_board->mod_playing,
       mzx_world->real_mod_playing))
        fix_mod(mzx_world, src_board, listening_flag);

      // Load the board charset and palette
      if(mzx_world->editor_conf.editor_load_board_assets)
        change_board_load_assets(mzx_world);

      // Clear the local undo histories
      clear_board_history = 1;
      clear_overlay_history = 1;

      draw_mod_timer = DRAW_MOD_TIMER_MAX;

      new_board = -1;
      modified = 1;
    }

    if(cursor_move_x || cursor_move_y)
    {
      // Move the cursor
      // Bound movement within the dimensions of the board
      cursor_move_x =
       CLAMP(cursor_move_x, -cursor_board_x, board_width - cursor_board_x - 1);

      cursor_move_y =
       CLAMP(cursor_move_y, -cursor_board_y, board_height - cursor_board_y - 1);

      if(cursor_move_x)
        cursor_move_y = 0;

      if(cursor_move_x || cursor_move_y)
      {
        struct undo_history *h = NULL;
        int offset = cursor_board_x + (board_width * cursor_board_y);
        int move_x = SGN(cursor_move_x);
        int move_y = SGN(cursor_move_y);
        int width = abs(cursor_move_x) + 1;
        int height = abs(cursor_move_y) + 1;

        if(move_x < 0)
          offset -= width;

        if(move_y < 0)
          offset -= board_width * height;

        if(draw_mode == 1)
        {
          switch(overlay_edit)
          {
            case EDIT_BOARD:
              h = board_history;
              add_block_undo_frame(
               mzx_world, h, src_board, offset, width, height);
              break;

            case EDIT_OVERLAY:
              h = overlay_history;
              add_layer_undo_frame(h,
               overlay, overlay_color, board_width, offset, width, height);
              break;

            case EDIT_VLAYER:
              h = vlayer_history;
              add_layer_undo_frame(h,
               vlayer_chars, vlayer_colors, board_width, offset, width, height);
              break;
          }
        }

        do
        {
          cursor_board_x += move_x;
          cursor_board_y += move_y;
          cursor_move_x -= move_x;
          cursor_move_y -= move_y;

          if(draw_mode == 1)
          {
            current_param = place_current_at_xy(mzx_world, current_id,
             current_color, current_param, cursor_board_x, cursor_board_y,
             &copy_robot, &copy_scroll, &copy_sensor, overlay_edit, 0);
            modified = 1;
          }

          if(cursor_board_x - scroll_x < 5)
            scroll_x--;

          if(cursor_board_x - scroll_x > 74)
            scroll_x++;

          if(cursor_board_y - scroll_y < 3)
            scroll_y--;

          if(cursor_board_y - scroll_y > (edit_screen_height - 5))
            scroll_y++;
        }
        while(cursor_move_x || cursor_move_y);

        if(draw_mode == 1)
          update_undo_frame(h);

        fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
         &scroll_y, &debug_x, board_width, board_height,
         edit_screen_height);
      }
    }

    if(draw_mode == 1 && mzx_world->editor_conf.editor_tab_focuses_view)
    {
      scroll_x = cursor_board_x - 40;
      scroll_y = cursor_board_y - (edit_screen_height / 2);

      fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
       &scroll_y, &debug_x, board_width, board_height,
       edit_screen_height);
    }

    if(text_place)
    {
      key = get_key(keycode_unicode);
      if(key != 0)
      {
        place_current_at_xy(mzx_world, __TEXT, current_color,
         key, cursor_board_x, cursor_board_y, &copy_robot,
         &copy_scroll, &copy_sensor, overlay_edit, 1);

        if(cursor_board_x < (board_width - 1))
          cursor_board_x++;

        if(cursor_board_x - scroll_x > 74)
          scroll_x++;

        fix_scroll(&cursor_board_x, &cursor_board_y, &scroll_x,
         &scroll_y, &debug_x, board_width, board_height,
         edit_screen_height);

        modified = 1;
      }
    }

    if(modified != prev_modified)
    {
      fix_caption(mzx_world, modified);
      prev_modified = modified;
    }

    // Exit event and Escape
    if(exit)
    {
      if(modified &&
       confirm(mzx_world, "Exit: World has not been saved, are you sure?"))
      {
        exit = 0;
      }
    }

  } while(!exit);

  update_event_status();

  mzx_world->editing = false;
  debug_mode = false;

  clear_world(mzx_world);
  clear_global_data(mzx_world);
  cursor_off();
  end_module();
  m_hide();
  clear_screen(32, 7);
  insta_fadeout();
  strcpy(curr_file, current_world);

  // Free the undo data
  destruct_undo_history(board_history);
  destruct_undo_history(overlay_history);
  destruct_undo_history(vlayer_history);
  board_history = NULL;
  overlay_history = NULL;
  vlayer_history = NULL;

  // Free the robot debugger data
  free_breakpoints();

  // Fix the screen, if the file wasn't saved then the
  // loader can't do this later
  set_screen_mode(0);
  default_palette();

  set_caption(mzx_world, NULL, NULL, 0, 0);

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
  debug_robot_break = __debug_robot_break;
  debug_robot_watch = __debug_robot_watch;
  debug_robot_config = __debug_robot_config;
  load_editor_charsets();
}

bool is_editor(void)
{
  return true;
}
