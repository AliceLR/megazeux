/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

#include "buffer.h"
#include "configure.h"
#include "edit.h"
#include "param.h"
#include "robot.h"
#include "undo.h"
#include "window.h"

#include "../core.h"
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
    { NULL, 0 }
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
    { NULL, 0 }
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
    { NULL, 0 }
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
    { NULL, 0 }
  },

  // Tranport (F7)
  {
    { "Stairs          ~A\xA2",   STAIRS          },
    { "Cave            ~6\xA1",   CAVE            },
    { "Transport       ~E<",      TRANSPORT       },
    { "Whirlpool       ~B\x97",   WHIRLPOOL_1     },
    { "CWRotate        ~9/",      CW_ROTATE       },
    { "CCWRotate       ~9\\",     CCW_ROTATE      },
    { NULL, 0 }
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
    { NULL, 0 }
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
    { NULL, 0 }
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
    { NULL, 0 }
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

void change_param(context *ctx, struct buffer_info *buffer, int *new_param)
{
  struct world *mzx_world = ctx->world;

  if(buffer->id == SENSOR)
  {
    *new_param = edit_sensor(mzx_world, buffer->sensor);
  }
  else

  if(is_robot(buffer->id))
  {
    edit_robot(ctx, buffer->robot, new_param);
  }
  else

  if(is_signscroll(buffer->id))
  {
    *new_param = edit_scroll(mzx_world, buffer->scroll);
  }

  else
  {
    *new_param = edit_param(mzx_world, buffer->id, buffer->param);
  }
}

void free_edit_buffer(struct buffer_info *buffer)
{
  clear_robot_contents(buffer->robot);
  clear_scroll_contents(buffer->scroll);

  memset(buffer->robot, 0, sizeof(struct robot));
  memset(buffer->scroll, 0, sizeof(struct scroll));
  memset(buffer->sensor, 0, sizeof(struct sensor));
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
        int player_id = buffer->param;
        struct player *player = &mzx_world->players[player_id];
        id_remove_top(mzx_world, player->x, player->y);
        player->x = x;
        player->y = y;
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
 * Replace the contents of the board or layer at a position with the contents
 * of the provided buffer. Preserves anything under the replaced thing on the
 * the board.
 */

int replace_current_at_xy(struct world *mzx_world, struct buffer_info *buffer,
 int x, int y, enum editor_mode mode, struct undo_history *history)
{
  struct board *cur_board = mzx_world->current_board;
  int offset = x + (y * cur_board->board_width);

  if(mode == EDIT_BOARD)
  {
    enum thing old_id = cur_board->level_id[offset];

    /**
     * Need special handling for storageless objects or storage objects of
     * different types--if placed normally, they could destroy whatever is
     * under them, and we don't want this. We also want the under included in
     * the history, which requires the use a block frame instead of a regular
     * frame (which would just place_current_at_xy when redo is used).
     */
    if(is_storageless(old_id) || old_id != buffer->id)
    {
      if(history)
        add_block_undo_frame(mzx_world, history, cur_board, offset, 1, 1);

      id_remove_top(mzx_world, x, y);
      place_current_at_xy(mzx_world, buffer, x, y, mode, NULL);

      if(history)
        update_undo_frame(history);

      return buffer->param;
    }
  }

  // Our requirements here are actually equivalent to the behavior of
  // place_current_xy in most cases, so use it directly.
  return place_current_at_xy(mzx_world, buffer, x, y, mode, history);
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

  if(mode == EDIT_BOARD)
  {
    enum thing grab_id = (enum thing)src_board->level_id[offset];
    int grab_param = src_board->level_param[offset];

    // Hack.
    if(grab_id != PLAYER)
      buffer->color = src_board->level_color[offset];
    else
      buffer->color = get_id_color(src_board, offset);

    // First, clear the existing copies. Previously, this would conditionally
    // be ignored if the buffer thing and param were the same, but since it's
    // always being duplicated over the buffer now, always clear it.

    if(is_robot(old_id))
    {
      clear_robot_contents(buffer->robot);
    }
    else

    if(is_signscroll(old_id))
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

struct thing_menu_context
{
  context ctx;
  enum thing_menu_id menu_number;
  struct buffer_info *buffer;
  int chosen_pos;
  int chosen_color;
  int chosen_param;

  // Placement info
  struct undo_history *place_history;
  boolean use_default_color;
  int place_x;
  int place_y;

  // Backup buffer info (if the user cancels)
  enum thing old_id;
  int old_color;
  int old_param;
};

/**
 * Update the buffer with the new color and parameter (and place the buffer on
 * the board if that feature is enabled).
 */

static void thing_menu_place_callback(context *ctx, context_callback_param *p)
{
  struct thing_menu_context *thing_menu = (struct thing_menu_context *)ctx;
  struct editor_config_info *editor_conf = get_editor_config();
  struct world *mzx_world = ctx->world;
  struct buffer_info *buffer = thing_menu->buffer;

  if(thing_menu->chosen_param >= 0)
  {
    // id was already set for change_param.
    buffer->param = thing_menu->chosen_param;
    buffer->color = thing_menu->chosen_color;

    if(editor_conf->editor_thing_menu_places)
    {
      buffer->param = place_current_at_xy(mzx_world, buffer,
       thing_menu->place_x, thing_menu->place_y, EDIT_BOARD,
       thing_menu->place_history);
    }
  }
  else
  {
    // Restore the old buffer if param editing failed.
    // Note this should never happen for the storage types.
    buffer->id = thing_menu->old_id;
    buffer->param = thing_menu->old_param;
    buffer->color = thing_menu->old_color;
  }
}

/**
 * Now that a thing has been chosen, set up the buffer for it and request a
 * new parameter.
 */

static void thing_menu_edit_callback(context *ctx, context_callback_param *p)
{
  struct thing_menu_context *thing_menu = (struct thing_menu_context *)ctx;
  struct buffer_info *buffer = thing_menu->buffer;
  enum thing chosen_id;

  // If <0 was chosen, the user canceled without selecting anything.
  if(thing_menu->chosen_pos < 0)
    return;

  chosen_id = thing_menus[thing_menu->menu_number][thing_menu->chosen_pos].id;

  if(def_colors[chosen_id] && thing_menu->use_default_color)
  {
    thing_menu->chosen_color = def_colors[chosen_id];
  }
  else
  {
    thing_menu->chosen_color = buffer->color;
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

    create_blank_robot_direct(buffer->robot,
     thing_menu->place_x, thing_menu->place_y);
  }
  else

  if(is_signscroll(chosen_id))
  {
    if(is_signscroll(buffer->id))
      clear_scroll_contents(buffer->scroll);

    create_blank_scroll_direct(buffer->scroll);
  }

  buffer->id = chosen_id;
  buffer->param = -1;
  change_param(ctx, buffer, &(thing_menu->chosen_param));

  context_callback(ctx, NULL, thing_menu_place_callback);
}

/**
 * Select a thing from the specified thing menu.
 */

static void thing_menu_choose_thing(struct thing_menu_context *thing_menu,
 enum thing_menu_id menu_number)
{
  const char *options[MAX_CHOICES];
  int count;
  int pos;

  // Find the size of the current menu and make a list readable by the list menu
  for(count = 0; thing_menus[menu_number][count].option; count++)
    options[count] = thing_menus[menu_number][count].option;

  pos = list_menu(options, 20, thing_menu_titles[menu_number], 0, count, 27, 0);
  thing_menu->chosen_pos = pos;

  context_callback((context *)thing_menu, NULL, thing_menu_edit_callback);
}

/**
 * Exit this context once the callback chain finishes.
 */

static void thing_menu_resume(context *ctx)
{
  destroy_context(ctx);
}

/**
 * Wrapper for list_menu. Select and configure type from one of the thing menus
 * into the buffer. Place it onto the board afterward.
 */

void thing_menu(context *parent, enum thing_menu_id menu_number,
 struct buffer_info *buffer, boolean use_default_color, int x, int y,
 struct undo_history *history)
{
  struct thing_menu_context *thing_menu;
  struct context_spec spec;

  struct editor_config_info *editor_conf = get_editor_config();
  struct world *mzx_world = parent->world;
  struct board *cur_board = mzx_world->current_board;

  enum thing thing_at_pos =
   (enum thing)cur_board->level_id[x + (y * cur_board->board_width)];

  if(thing_at_pos == PLAYER && editor_conf->editor_thing_menu_places)
  {
    error_message(E_CANT_OVERWRITE_PLAYER, 0, NULL);
    return;
  }

  thing_menu = cmalloc(sizeof(struct thing_menu_context));
  thing_menu->menu_number = menu_number;
  thing_menu->buffer = buffer;
  thing_menu->place_x = x;
  thing_menu->place_y = y;
  thing_menu->place_history = history;
  thing_menu->use_default_color = use_default_color;
  thing_menu->old_id = buffer->id;
  thing_menu->old_param = buffer->param;
  thing_menu->old_color = buffer->color;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.resume = thing_menu_resume;

  create_context((context *)thing_menu, parent, &spec, CTX_THING_MENU);

  thing_menu_choose_thing(thing_menu, menu_number);
}
