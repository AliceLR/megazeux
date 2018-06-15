/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#ifndef __CORE_H
#define __CORE_H

#include "compat.h"

__M_BEGIN_DECLS

#include "configure.h"
#include "platform.h"
#include "world_struct.h"

#include "editor/configure.h"

enum help_index
{
  CTX_MAIN = 72,
  CTX_BLOCK_CMD = 73,
  CTX_BLOCK_TYPE = 74,
  CTX_CHOOSE_CHARSET = 75,
  CTX_IMPORTEXPORT_TYPE = 77,
  CTX_CHAR_EDIT = 79,
  CTX_STATUS_COUNTERS = 82,
  CTX_BOARD_EXITS = 83,
  CTX_BOARD_SIZES = 84,
  CTX_BOARD_INFO = 85,
  CTX_CHANGE_CHAR_IDS = 89,
  CTX_CHANGE_DAMAGE = 90,
  CTX_GLOBAL_SETTINGS = 86,
  CTX_GLOBAL_SETTINGS_2 = 88,
  CTX_ROBO_ED = 87,
  CTX_PALETTE_EDITOR = 93,
  CTX_SENSOR_EDITOR = 94,
  CTX_SUPER_MEGAZEUX = 95,
  CTX_SFX_EDITOR = 97,
  CTX_DIALOG_BOX = 98,
  CTX_F2_MENU = 92,
  CTX_PLAY_GAME = 91,
  CTX_UPDATER = 99,
  CTX_COUNTER_DEBUG = 100,
  CTX_ROBOT_DEBUG = 101,
  CTX_BREAKPOINT_EDITOR = 102,
  CTX_VLAYER_SIZES = 103,
};

/** Contains global MegaZeux information that needs to be readily accessible. */
struct global_data
{
  struct config_info conf;
  struct config_info conf_backup;
#ifdef CONFIG_EDITOR
  struct editor_config_info editor_conf;
  struct editor_config_info editor_conf_backup;
#endif
};

typedef struct core_context core_context;

/** Contains information related to the current MegaZeux state/interface/etc. */
typedef struct context
{
  struct global_data *data;
  struct world *mzx_world;
  core_context *root;
  boolean draw_while_under;
  boolean update_while_under;
  boolean allow_events_to_interrupt;
  enum help_index help_index;
  void (*draw_function)(struct context *);
  boolean (*update_function)(struct context *);
  void (*destroy_function)(struct context *);
}
context;

/**
 * Sets up a new context and adds it to the context stack. The new context
 * will be drawn last and update first, taking input precedence.
 *
 * @param ctx               The new context to be initialized.
 * @param parent            The context which created this context.
 * @param help_index        A help file index associated with this context.
 * @param draw_function     Optional function to draw this context every frame.
 * @param update_function   Update function to be called every frame.
 * @param destroy_function  Optional function to be called on destruction.
 */

CORE_LIBSPEC void create_context(context *ctx, context *parent,
 enum help_index help_index,
 void (*draw_function)(context *),
 boolean (*update_function)(context *),
 void (*destroy_function)(context *));

/**
 * Destroys a context and every context above it on the context stack.
 * This will call the destroy function for every context destroyed. Contexts
 * will be destroys from the top of the stack down.
 *
 * @param ctx   The context to destroy.
 */

CORE_LIBSPEC void destroy_context(context *ctx);

/**
 * Initializes data for the core.
 *
 * @param mzx_world     A struct to contain MegaZeux world information.
 * @param global_data   A struct to contain global MegaZeux information.
 * @return              A core context struct.
 */

CORE_LIBSPEC core_context *core_init(struct world *mzx_world,
 struct global_data *global_data);

/**
 * Run MegaZeux using given context stack information.
 *
 * @param root          The core context to run.
 */

CORE_LIBSPEC void core_run(core_context *root);

/**
 * Clean up MegaZeux core data for a given core context.
 *
 * @param root          The core context to clean up.
 */

CORE_LIBSPEC void core_quit(core_context *root);

// Deprecated functions.

CORE_LIBSPEC enum help_index get_context(context *);
CORE_LIBSPEC void set_context(enum help_index idx);
CORE_LIBSPEC void pop_context(void);

// Editor external function pointers.

#ifdef CONFIG_EDITOR

CORE_LIBSPEC extern void (*edit_world)(struct world *mzx_world,
 int reload_curr_file);
CORE_LIBSPEC extern void (*debug_counters)(struct world *mzx_world);
CORE_LIBSPEC extern void (*draw_debug_box)(struct world *mzx_world,
 int x, int y, int d_x, int d_y, int show_keys);
CORE_LIBSPEC extern int (*debug_robot_break)(struct world *mzx_world,
 struct robot *cur_robot, int id, int lines_run);
CORE_LIBSPEC extern int (*debug_robot_watch)(struct world *mzx_world,
 struct robot *cur_robot, int id, int lines_run);
CORE_LIBSPEC extern void (*debug_robot_config)(struct world *mzx_world);

#else
static inline void edit_world(struct world *mzx_world, int r) {}
static inline void debug_counters(struct world *mzx_world) {}
static inline void draw_debug_box(struct world *mzx_world,
 int x, int y, int d_x, int d_y, int show_keys) {}
static inline int (*debug_robot_break)(struct world *mzx_world,
 struct robot *cur_robot, int id, int lines_run) { return 0; }
static inline int (*debug_robot_watch)(struct world *mzx_world,
 struct robot *cur_robot, int id, int lines_run) { return 0; }
static inline void (*debug_robot_config)(struct world *mzx_world) {}
#endif

// Network external function pointers.

#ifdef CONFIG_UPDATER
CORE_LIBSPEC extern void (*check_for_updates)(struct world *mzx_world,
 struct config_info *conf, int is_automatic);
#else
static inline void check_for_updates(struct world *mzx_world,
 struct config_info *conf, int is_automatic) {}
#endif

__M_END_DECLS

#endif /* __CORE_H */
