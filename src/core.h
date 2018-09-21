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
  CTX_TITLE_SCREEN          = -1,
  CTX_MAIN                  = 72,
  CTX_PLAY_GAME             = 91,
  CTX_CONFIGURE             = 92,
  CTX_DIALOG_BOX            = 98,
  CTX_HELP_SYSTEM           = -2,
  CTX_MAIN_MENU             = -3,
  CTX_GAME_MENU             = -4,
  CTX_INTAKE                = -5,
  CTX_INTAKE_NUM            = -6,

  // Network contexts.
  CTX_UPDATER               = 99,

  // Editor contexts.
  CTX_EDITOR                = -100,
  CTX_EDITOR_VIEW_BOARD     = -101,
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

/**
 * Core types.
 * "subcontext" is an alternate type for context that should be used for
 * functions that deal with subcontexts instead of contexts.
 */
typedef struct context context;
typedef struct context subcontext;
typedef struct context_data context_data;
typedef struct core_context core_context;

/**
 * Contains information related to the current MegaZeux state/interface/etc.
 * NOTE: "parent" will only be set for subcontexts and will be NULL otherwise.
 */
struct context
{
  struct global_data *data;
  struct world *world;
  context *parent;
  core_context *root;
  context_data *internal_data;
};

/**
 * Contains extra function/variable information for a new context or subcontext.
 * At least one update function should be provided; all other variables may be
 * NULL or zero by default.
 *
 * draw             Optional function to draw this context every frame.
 * key              Update function called to handle a keypress.
 * click            Update function called to handle a mouse click.
 * drag             Update function called to handle a mouse drag.
 * idle             Update function called every frame.
 * destroy          Optional function to be called on destruction.
 * framerate_mode   Framerate mode this context should use (context only).
 */
struct context_spec
{
  void (*resume)(context *);
  void (*draw)(context *);
  boolean (*idle)(context *);
  boolean (*key)(context *, int *key);
  boolean (*click)(context *, int *key, int button, int x, int y);
  boolean (*drag)(context *, int *key, int button, int x, int y);
  void (*destroy)(context *);
  enum framerate_type framerate_mode;
};

/**
 * Sets up a new context and adds it to the context stack. At least one
 * update function must be provided. If "parent" is a subcontext, this function
 * will use its parent context as the parent instead.
 *
 * @param ctx               Optional newly allocated context to be initialized.
 * @param parent            The context which created this context.
 * @param ctx_spec          Specification for context functions/variables.
 * @param context_type      Used to identify contexts (also, by the help system)
 */

CORE_LIBSPEC void create_context(context *ctx, context *parent,
 struct context_spec *ctx_spec, enum context_type context_type);

/**
 * Sets up a new subcontext and adds it to a parent context. The new subcontext
 * will be drawn last and update first, taking input precedence. If "parent" is
 * a subcontext, this function will use the parent of that subcontext as the
 * parent of the new subcontext instead.
 *
 * @param sub               Optional newly allocated subcontext.
 * @param parent            The context which created this context.
 * @param sub_spec          Specification for subcontext functions/variables.
 */

CORE_LIBSPEC void create_subcontext(subcontext *sub, context *parent,
 struct context_spec *sub_spec);

/**
 * Destroys a context and its subcontexts. This will call the destroy function
 * for every subcontext destroyed, and finally for this context itself.
 * Subcontexts will be destroyed from the top of the stack down. If given a
 * subcontext, this will destroy the given subcontext only.
 *
 * @param ctx           The context or subcontext to destroy.
 */

CORE_LIBSPEC void destroy_context(context *ctx);

/**
 * Determine if a context change has occurred.
 *
 *
 * @param ctx           Current context
 * @return              true if a context change has occurred.
 */

CORE_LIBSPEC boolean has_context_changed(context *ctx);

/**
 * Determine if the given context matches an expected type.
 *
 * @param ctx           A context
 * @param context_type  A context type
 * @return              true if the context has the given context type.
 */

boolean is_context(context *ctx, enum context_type context_type);

/**
 * Get config information from a particular context.
 */

static inline struct config_info *get_config(context *ctx)
{
  // FIXME return &(ctx->data->conf);
  return &(ctx->world->conf);
}

#ifdef CONFIG_EDITOR
/**
 * Get editor config information from a particular context.
 */

static inline struct editor_config_info *get_editor_config(context *ctx)
{
  // FIXME return &(ctx->data->editor_conf);
  return &(ctx->world->editor_conf);
}
#endif

/**
 * Sets the framerate mode of the current context. See enum framerate_type for
 * a list of valid values. This function is meaningless for subcontexts.
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

CORE_LIBSPEC extern void (*edit_world)(context *parent,
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
