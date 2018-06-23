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
#include "world_struct.h"

#ifdef CONFIG_EDITOR
#include "editor/configure.h"
#endif

/**
 * The type of a given context. Used to identify particular contexts and to open
 * help files contextually. Do not sort is enum by value.
 *
 * Numbers >0 correspond directly to a hyperlink in the help system.
 * Numbers <=0 are used by contexts with no unique link in the help system.
 */

enum context_type
{
  // Core contexts.
  CTX_DEFAULT               = 0,
  CTX_SUBCONTEXT            = -1,
  CTX_TITLE_SCREEN          = -2,
  CTX_MAIN                  = 72,
  CTX_PLAY_GAME             = 91,
  CTX_CONFIGURE             = 92,
  CTX_DIALOG_BOX            = 98,
  CTX_HELP_SYSTEM           = -3,
  CTX_MAIN_MENU             = -4,
  CTX_GAME_MENU             = -5,

  // Network contexts.
  CTX_UPDATER               = 99,

  // Editor contexts.
  CTX_BLOCK_CMD             = 73,
  CTX_BLOCK_TYPE            = 74,
  CTX_CHOOSE_CHARSET        = 75,
  CTX_IMPORTEXPORT_TYPE     = 77,
  CTX_CHAR_EDIT             = 79,
  CTX_STATUS_COUNTERS       = 82,
  CTX_BOARD_EXITS           = 83,
  CTX_BOARD_SIZES           = 84,
  CTX_BOARD_INFO            = 85,
  CTX_CHANGE_CHAR_IDS       = 89,
  CTX_CHANGE_DAMAGE         = 90,
  CTX_GLOBAL_SETTINGS       = 86,
  CTX_GLOBAL_SETTINGS_2     = 88,
  CTX_ROBO_ED               = 87,
  CTX_PALETTE_EDITOR        = 93,
  CTX_SENSOR_EDITOR         = 94,
  CTX_SUPER_MEGAZEUX        = 95,
  CTX_SFX_EDITOR            = 97,
  CTX_COUNTER_DEBUG         = 100,
  CTX_ROBOT_DEBUG           = 101,
  CTX_BREAKPOINT_EDITOR     = 102,
  CTX_VLAYER_SIZES          = 103,
};

/** The framerate rule that should be used for the active context. */
enum framerate_type
{
  FRAMERATE_UI,             // Standard delay time.
  FRAMERATE_UI_INTERRUPT,   // Standard delay time or until an event occurs.
  FRAMERATE_MZX_SPEED,      // Delay according to mzx_speed.
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

typedef struct context context;
typedef struct context_data context_data;
typedef struct core_context core_context;

/** Contains information related to the current MegaZeux state/interface/etc. */
struct context
{
  core_context *root;
  context_data *internal_data;
  struct global_data *data;
  struct world *world;
};

/**
 * Sets MegaZeux's caption according to various parameters.
 *
 * @param mzx_world         The active world.
 * @param board             Optional: the active editor board.
 * @param robot             Optional: the active editor robot.
 * @param modified          Indicates whether the world has been modified.
 */

CORE_LIBSPEC void set_caption(struct world *mzx_world, struct board *board,
 struct robot *robot, boolean modified);

/**
 * Sets up a new context and adds it to the context stack. The new context
 * will be drawn last and update first, taking input precedence. At least one
 * update function must be provided.
 *
 * @param ctx               Optional newly allocated context to be initialized.
 * @param parent            The context which created this context.
 * @param context_type      Used to identify contexts and by the help system.
 * @param draw_function     Optional function to draw this context every frame.
 * @param key_function      Update function called to handle a keypress.
 * @param click_function    Update function called to handle a mouse click.
 * @param drag_function     Update function called to handle a mouse drag.
 * @param idle_function     Update function called every frame.
 * @param destroy_function  Optional function to be called on destruction.
 */

CORE_LIBSPEC void create_context(context *ctx, context *parent,
 enum context_type context_type,
 void (*resume_function)(context *),
 void (*draw_function)(context *),
 boolean (*idle_function)(context *),
 boolean (*key_function)(context *, int *key),
 boolean (*click_function)(context *, int *key, int button, int x, int y),
 boolean (*drag_function)(context *, int *key, int button, int x, int y),
 void (*destroy_function)(context *));

/**
 * Destroys a context and every context above it on the context stack.
 * This will call the destroy function for every context destroyed. Contexts
 * will be destroys from the top of the stack down.
 *
 * @param ctx           The context to destroy.
 */

CORE_LIBSPEC void destroy_context(context *ctx);

/**
 * Get config information for a particular context.
 */

static inline struct config_info *get_config(context *ctx)
{
  // FIXME return &(ctx->data->conf);
  return &(ctx->world->conf);
}

/**
 * Sets the framerate mode of the current context. See enum framerate_type for
 * a list of valid values.
 *
 * @param ctx           The current context.
 * @param framerate     The new framerate mode.
 */

void set_context_framerate_mode(context *ctx, enum framerate_type framerate);

/**
 * Initializes data for the core.
 *
 * @param mzx_world     MegaZeux world information struct.
 * @param data          Global information struct.
 * @return              A core context struct.
 */

CORE_LIBSPEC core_context *core_init(struct world *mzx_world,
 struct global_data *data);

/**
 * Run MegaZeux using given context stack information.
 *
 * @param root          The core context to run.
 */

CORE_LIBSPEC void core_run(core_context *root);

/**
 * Signal the core to abort all contexts and exit the current game loop.
 *
 * @param ctx           The current context.
 */

CORE_LIBSPEC void core_exit(context *ctx);

/**
 * Clean up MegaZeux all context data for a given core context.
 *
 * @param root          The core context to free.
 */

CORE_LIBSPEC void core_free(core_context *root);

// Deprecated functions.

CORE_LIBSPEC enum context_type get_context(context *);
CORE_LIBSPEC void set_context(enum context_type idx);
CORE_LIBSPEC void pop_context(void);

// Editor external function pointers.

CORE_LIBSPEC extern void (*edit_world)(struct world *mzx_world,
 boolean reload_curr_file);
CORE_LIBSPEC extern void (*debug_counters)(struct world *mzx_world);
CORE_LIBSPEC extern void (*draw_debug_box)(struct world *mzx_world,
 int x, int y, int d_x, int d_y, int show_keys);
CORE_LIBSPEC extern int (*debug_robot_break)(struct world *mzx_world,
 struct robot *cur_robot, int id, int lines_run);
CORE_LIBSPEC extern int (*debug_robot_watch)(struct world *mzx_world,
 struct robot *cur_robot, int id, int lines_run);
CORE_LIBSPEC extern void (*debug_robot_config)(struct world *mzx_world);

// Network external function pointers.

CORE_LIBSPEC extern void (*check_for_updates)(struct world *mzx_world,
 struct config_info *conf, int is_automatic);

__M_END_DECLS

#endif /* __CORE_H */
