/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

#include "buffer.h"
#include "edit.h"
#include "param.h"
#include "robo_ed.h"
#include "robot.h"
#include "undo.h"
#include "window.h"

#include "../data.h"
#include "../error.h"
#include "../event.h"
#include "../graphics.h"
#include "../idarray.h"
#include "../idput.h"
#include "../robot.h"
#include "../scrdisp.h"
#include "../world_struct.h"

// This can be increased if necessary.
#define MAX_CHOICES 20

struct thing_menu_item
{
  const char *const option;
  int id;
};

/**
 * Titles of the thing menus. The order must correspond to the enum in buffer.h.
 */

static const char *const thing_menu_titles[NUM_THING_MENUS] =
{
  "Terrains",
  "Items",
  "Creatures",
  "Puzzle Pieces",
  "Transport",
  "Elements",
  "Miscellaneous",
  "Objects",
};

/**
 * Contents of the menus. The order must correspond to the enum in buffer.h.
 * Keep these NULL-terminated.
 */

static struct thing_menu_item thing_menus[NUM_THING_MENUS][MAX_CHOICES] =
{
  // Terrain (F3)
  {
    { "Space           ~1 ",      SPACE           },
    { "Normal          ~E\xB2",   NORMAL          },
    { "Solid           ~D\xDB",   SOLID           },
    { "Tree            ~A\x06",   TREE            },
    { "Line            ~B\xCD",   LINE            },
    { "Custom Block    ~F?",      CUSTOM_BLOCK    },
    { "Breakaway       ~C\xB1",   BREAKAWAY       },
    { "Custom Break    ~F?",      CUSTOM_BREAK    },
    { "Fake            ~9\xB2",   FAKE            },
    { "Carpet          ~4\xB1",   CARPET          },
    { "Floor           ~6\xB0",   FLOOR           },
    { "Tiles           ~0\xFE",   TILES           },
    { "Custom Floor    ~F?",      CUSTOM_FLOOR    },
    { "Web             ~7\xC5",   WEB             },
    { "Thick Web       ~7\xCE",   THICK_WEB       },
    { "Forest          ~2\xB2",   FOREST          },
    { "Invis. Wall     ~1 ",      INVIS_WALL      },
    { NULL }
  },

  // Item (F4)
  {
    { "Gem             ~A\x04",   GEM             },
    { "Magic Gem       ~E\x04",   MAGIC_GEM       },
    { "Health          ~C\x03",   HEALTH          },
    { "Ring            ~E\x09",   RING            },
    { "Potion          ~B\x96",   POTION          },
    { "Energizer       ~D\x07",   ENERGIZER       },
    { "Ammo            ~3\xA4",   AMMO            },
    { "Bomb            ~8\x0B",   BOMB            },
    { "Key             ~F\x0C",   KEY             },
    { "Lock            ~F\x0A",   LOCK            },
    { "Coin            ~E\x07",   COIN            },
    { "Life            ~B\x9B",   LIFE            },
    { "Pouch           ~7\x9F",   POUCH           },
    { "Chest           ~6\xA0",   CHEST           },
    { NULL }
  },

  // Creature (F5)
  {
    { "Snake           ~2\xE7",   SNAKE           },
    { "Eye             ~F\xE8",   EYE             },
    { "Thief           ~C\x05",   THIEF           },
    { "Slime Blob      ~A*",      SLIMEBLOB       },
    { "Runner          ~4\x02",   RUNNER          },
    { "Ghost           ~7\xE6",   GHOST           },
    { "Dragon          ~4\x15",   DRAGON          },
    { "Fish            ~E\xE0",   FISH            },
    { "Shark           ~7\x7F",   SHARK           },
    { "Spider          ~7\x95",   SPIDER          },
    { "Goblin          ~D\x05",   GOBLIN          },
    { "Spitting Tiger  ~B\xE3",   SPITTING_TIGER  },
    { "Bear            ~6\xAC",   BEAR            },
    { "Bear Cub        ~6\xAD",   BEAR_CUB        },
    { "Lazer Gun       ~4\xCE",   LAZER_GUN       },
    { "Bullet Gun      ~F\x1B",   BULLET_GUN      },
    { "Spinning Gun    ~F\x1B",   SPINNING_GUN    },
    { "Missile Gun     ~8\x11",   MISSILE_GUN     },
    { NULL }
  },

  // Puzzle (F6)
  {
    { "Boulder         ~7\xE9",   BOULDER         },
    { "Crate           ~6\xFE",   CRATE           },
    { "Custom Push     ~F?",      CUSTOM_PUSH     },
    { "Box             ~E\xFE",   BOX             },
    { "Custom Box      ~F?",      CUSTOM_BOX      },
    { "Pusher          ~D\x1F",   PUSHER          },
    { "Slider NS       ~D\x12",   SLIDER_NS       },
    { "Slider EW       ~D\x1D",   SLIDER_EW       },
    { NULL }
  },

  // Tranport (F7)
  {
    { "Stairs          ~A\xA2",   STAIRS          },
    { "Cave            ~6\xA1",   CAVE            },
    { "Transport       ~E<",      TRANSPORT       },
    { "Whirlpool       ~B\x97",   WHIRLPOOL_1     },
    { "CWRotate        ~9/",      CW_ROTATE       },
    { "CCWRotate       ~9\\",     CCW_ROTATE      },
    { NULL }
  },

  // Element (F8)
  {
    { "Still Water     ~9\xB0",   STILL_WATER     },
    { "N Water         ~9\x18",   N_WATER         },
    { "S Water         ~9\x19",   S_WATER         },
    { "E Water         ~9\x1A",   E_WATER         },
    { "W Water         ~9\x1B",   W_WATER         },
    { "Ice             ~B\xFD",   ICE             },
    { "Lava            ~C\xB2",   LAVA            },
    { "Fire            ~E\xB1",   FIRE            },
    { "Goop            ~8\xB0",   GOOP            },
    { "Lit Bomb        ~8\xAB",   LIT_BOMB        },
    { "Explosion       ~E\xB1",   EXPLOSION       },
    { NULL }
  },

  // Misc (F9)
  {
    { "Door            ~6\xC4",   DOOR            },
    { "Gate            ~8\x16",   GATE            },
    { "Ricochet Panel  ~9/",      RICOCHET_PANEL  },
    { "Ricochet        ~A*",      RICOCHET        },
    { "Mine            ~C\x8F",   MINE            },
    { "Spike           ~8\x1E",   SPIKE           },
    { "Custom Hurt     ~F?",      CUSTOM_HURT     },
    { "Text            ~F?",      __TEXT          },
    { "N Moving Wall   ~F?",      N_MOVING_WALL   },
    { "S Moving Wall   ~F?",      S_MOVING_WALL   },
    { "E Moving Wall   ~F?",      E_MOVING_WALL   },
    { "W Moving Wall   ~F?",      W_MOVING_WALL   },
    { NULL }
  },

  // Objects (F10)
  {
    { "Robot           ~F?",      ROBOT           },
    { "Pushable Robot  ~F?",      ROBOT_PUSHABLE  },
    { "Player          ~B\x02",   PLAYER          },
    { "Scroll          ~F\xE4",   SCROLL          },
    { "Sign            ~6\xE2",   SIGN            },
    { "Sensor          ~F?",      SENSOR          },
    { "Bullet          ~F\xF9",   BULLET          },
    { "Missile         ~8\x10",   MISSILE         },
    { "Seeker          ~A/",      SEEKER          },
    { "Shooting Fire   ~E\x0F",   SHOOTING_FIRE   },
    { NULL }
  }
};

/**
 * Default colors for each type ID.
 */

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

// It's important that after changing the param the thing is placed (using
// place_current_at_xy) so that scrolls/sensors/robots get copied.

int change_param(struct world *mzx_world, struct buffer_info *buffer)
{
  if(buffer->id == SENSOR)
  {
    return edit_sensor(mzx_world, buffer->sensor);
  }
  else

  if(is_robot(buffer->id))
  {
    return edit_robot(mzx_world, buffer->robot);
  }
  else

  if(is_signscroll(buffer->id))
  {
    return edit_scroll(mzx_world, buffer->scroll);
  }

  else
  {
    return edit_param(mzx_world, buffer->id, buffer->param);
  }
}

/**
 * Place the contents of the buffer onto the board or a layer.
 * Returns the parameter or char of the newly placed item.
 */

int place_current_at_xy(struct world *mzx_world, struct buffer_info *buffer,
 int x, int y, enum editor_mode mode, struct undo_history *history)
{
  struct board *cur_board = mzx_world->current_board;
  int offset = x + (y * cur_board->board_width);
  int return_param = buffer->param;

  if(mode == EDIT_BOARD)
  {
    char *level_id = cur_board->level_id;
    enum thing old_id = (enum thing)level_id[offset];

    if(old_id != PLAYER)
    {
      if(history)
        add_board_undo_frame(mzx_world, history, buffer, x, y);

      if(buffer->id == PLAYER)
      {
        id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
        mzx_world->player_x = x;
        mzx_world->player_y = y;
      }
      else

      if(is_robot(buffer->id))
      {
        if(is_robot(old_id))
        {
          int old_param = cur_board->level_param[offset];
          replace_robot_source(mzx_world, cur_board, buffer->robot, old_param);
          cur_board->level_color[offset] = buffer->color;
          cur_board->level_id[offset] = buffer->id;

          if(history)
            update_undo_frame(history);

          return old_param;
        }

        return_param =
         duplicate_robot_source(mzx_world, cur_board, buffer->robot, x, y);

        if(return_param != -1)
        {
          (cur_board->robot_list[return_param])->xpos = x;
          (cur_board->robot_list[return_param])->ypos = y;
          (cur_board->robot_list[return_param])->used = true;
        }
      }
      else

      if(is_signscroll(buffer->id))
      {
        if(is_signscroll(old_id))
        {
          int old_param = cur_board->level_param[offset];
          replace_scroll(cur_board, buffer->scroll, old_param);
          cur_board->level_color[offset] = buffer->color;
          cur_board->level_id[offset] = buffer->id;

          if(history)
            update_undo_frame(history);

          return old_param;
        }

        return_param = duplicate_scroll(cur_board, buffer->scroll);
        if(return_param != -1)
          (cur_board->scroll_list[return_param])->used = true;
      }
      else

      if(buffer->id == SENSOR)
      {
        if(old_id == SENSOR)
        {
          int old_param = cur_board->level_param[offset];
          replace_sensor(cur_board, buffer->sensor, old_param);
          cur_board->level_color[offset] = buffer->color;

          if(history)
            update_undo_frame(history);

          return old_param;
        }

        return_param = duplicate_sensor(cur_board, buffer->sensor);
        if(return_param != -1)
          (cur_board->sensor_list[return_param])->used = true;
      }

      if(return_param != -1)
      {
        place_at_xy(mzx_world, buffer->id, buffer->color, return_param, x, y);
      }

      if(history)
        update_undo_frame(history);
    }
  }
  else

  if(mode == EDIT_OVERLAY)
  {
    if(history)
      add_layer_undo_frame(history, cur_board->overlay,
       cur_board->overlay_color, cur_board->board_width, offset, 1, 1);

    cur_board->overlay[offset] = buffer->param;
    cur_board->overlay_color[offset] = buffer->color;

    if(history)
      update_undo_frame(history);
  }

  else // EDIT_VLAYER
  {
    offset = x + (y * mzx_world->vlayer_width);

    if(history)
      add_layer_undo_frame(history, mzx_world->vlayer_chars,
       mzx_world->vlayer_colors, mzx_world->vlayer_width, offset, 1, 1);

    mzx_world->vlayer_chars[offset] = buffer->param;
    mzx_world->vlayer_colors[offset] = buffer->color;

    if(history)
      update_undo_frame(history);
  }

  return return_param;
}

/**
 * Copy the contents of the board/layer at a location into the buffer.
 */

void grab_at_xy(struct world *mzx_world, struct buffer_info *buffer,
 int x, int y, enum editor_mode mode)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int offset = x + (y * board_width);
  enum thing old_id = buffer->id;
  int old_param = buffer->param;

  if(mode == EDIT_BOARD)
  {
    enum thing grab_id = (enum thing)src_board->level_id[offset];
    int grab_param = src_board->level_param[offset];

    // Hack.
    if(grab_id != PLAYER)
      buffer->color = src_board->level_color[offset];
    else
      buffer->color = get_id_color(src_board, offset);

    // First, clear the existing copies, unless the new id AND param
    // matches, because in that case it's the same thing

    if(is_robot(old_id) &&
     ((old_id != grab_id) || (old_param != grab_param)))
    {
      clear_robot_contents(buffer->robot);
    }

    if(is_signscroll(old_id) &&
     ((old_id != grab_id) || (old_param != grab_param)))
    {
      clear_scroll_contents(buffer->scroll);
    }

    if(is_robot(grab_id))
    {
      struct robot *src_robot = src_board->robot_list[grab_param];
      duplicate_robot_direct_source(mzx_world, src_robot, buffer->robot, 0, 0);
    }
    else

    if(is_signscroll(grab_id))
    {
      struct scroll *src_scroll = src_board->scroll_list[grab_param];
      duplicate_scroll_direct(src_scroll, buffer->scroll);
    }
    else

    if(grab_id == SENSOR)
    {
      struct sensor *src_sensor = src_board->sensor_list[grab_param];
      duplicate_sensor_direct(src_sensor, buffer->sensor);
    }

    buffer->id = grab_id;
    buffer->param = grab_param;
  }
  else

  if(mode == EDIT_OVERLAY)
  {
    buffer->param = src_board->overlay[offset];
    buffer->color = src_board->overlay_color[offset];
  }

  else // EDIT_VLAYER
  {
    offset = x + (y * mzx_world->vlayer_width);

    buffer->param = mzx_world->vlayer_chars[offset];
    buffer->color = mzx_world->vlayer_colors[offset];
  }
}

/**
 * Select and configure type from one of the thing menus into the buffer.
 * Place it onto the board afterward.
 */

void thing_menu(context *ctx, enum thing_menu_id menu_number,
 struct buffer_info *buffer, boolean use_default_color, int x, int y,
 struct undo_history *history)
{
  struct editor_config_info *editor_conf = get_editor_config(ctx);
  struct world *mzx_world = ctx->world;
  struct board *cur_board = mzx_world->current_board;
  enum thing old_id;
  int old_color;
  int old_param;
  enum thing chosen_id;
  int chosen_color;
  int chosen_param;
  int pos;

  const char *options[MAX_CHOICES];
  int count;

  old_id = (enum thing)cur_board->level_id[x + (y * cur_board->board_width)];
  if(old_id == PLAYER && editor_conf->editor_thing_menu_places)
  {
    error("Cannot overwrite the player- move it first", 0, 8, 0x0000);
    return;
  }

  // Find the size of the current menu and make a list readable by the list menu
  for(count = 0; thing_menus[menu_number][count].option; count++)
    options[count] = thing_menus[menu_number][count].option;

  pos = list_menu(options, 20, thing_menu_titles[menu_number], 0, count, 27, 0);

  if(pos >= 0)
  {
    chosen_id = thing_menus[menu_number][pos].id;

    if(def_colors[chosen_id] && use_default_color)
    {
      chosen_color = def_colors[chosen_id];
    }
    else
    {
      chosen_color = buffer->color;
    }

    // Perhaps put a new blank scroll, robot, or sensor in one of the copies
    if(chosen_id == SENSOR)
    {
      create_blank_sensor_direct(buffer->sensor);
    }
    else

    if(is_robot(chosen_id))
    {
      if(is_robot(buffer->id))
        clear_robot_contents(buffer->robot);

      create_blank_robot_direct(buffer->robot, x, y);
    }

    if(is_signscroll(chosen_id))
    {
      if(is_signscroll(buffer->id))
        clear_scroll_contents(buffer->scroll);

      create_blank_scroll_direct(buffer->scroll);
    }

    old_id = buffer->id;
    old_param = buffer->param;
    old_color = buffer->color;

    buffer->id = chosen_id;
    buffer->param = -1;
    chosen_param = change_param(mzx_world, buffer);

    if(chosen_param >= 0)
    {
      buffer->param = chosen_param;
      buffer->color = chosen_color;

      if(editor_conf->editor_thing_menu_places)
      {
        buffer->param = place_current_at_xy(mzx_world, buffer,
         x, y, EDIT_BOARD, history);
      }
    }
    else
    {
      // Restore the old buffer if param editing failed.
      // Note this never happens for the storage types.
      buffer->id = old_id;
      buffer->param = old_param;
      buffer->color = old_color;
    }
  }
}
