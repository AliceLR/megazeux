/* MegaZeux
 *
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

/* MegaZeux main loop and context handling code. */

#include <stdlib.h>
#include <string.h>

#include "configure.h"
#include "core.h"
#include "error.h"
#include "event.h"
#include "graphics.h"
#include "helpsys.h"
#include "world_struct.h"

// Contains context stack information.
// Used as a context to help create the first actual context.
struct core_context
{
  context ctx;
  context **ctx_stack;
  int ctx_stack_alloc;
  int ctx_stack_size;
  boolean full_exit;
  boolean context_destroyed;
};

/**
 * Configures a provided context and adds it to the context stack to be run
 * by the main loop. The assumption here is that ctx will be some other local
 * struct casted to a context.
 *
 * At least one update function needs to be provided capable of destroying the
 * context; otherwise, MegaZeux would hang up trying to execute this context.
 */

void run_context(context *ctx, context *parent,
 enum help_index help_index,
 void (*draw_function)(context *),
 boolean (*key_function)(struct context *, int *key),
 boolean (*click_function)(struct context *, int *key,
  int button, int x, int y),
 boolean (*drag_function)(struct context *, int *key,
  int button, int x, int y),
 void (*idle_function)(context *),
 void (*destroy_function)(context *))
{
  core_context *root = parent->root;

  if(key_function == NULL && click_function == NULL &&
   drag_function == NULL && idle_function == NULL)
    error("Context code bug", 2, 4, 0x2B01);

  ctx->root = root;
  ctx->data = parent->data;
  ctx->world = parent->world;
  ctx->help_index = help_index;
  ctx->is_subcontext = false;
  ctx->allow_help_system = true;
  ctx->allow_configure_dialog = true;
  ctx->allow_events_to_interrupt = false;
  ctx->draw_function = draw_function;
  ctx->key_function = key_function;
  ctx->click_function = click_function;
  ctx->drag_function = drag_function;
  ctx->idle_function = idle_function;
  ctx->destroy_function = destroy_function;

  if(root->ctx_stack_size >= root->ctx_stack_alloc)
  {
    if(root->ctx_stack_alloc == 0)
      root->ctx_stack_alloc = 8;

    while(root->ctx_stack_size >= root->ctx_stack_alloc)
      root->ctx_stack_alloc *= 2;

    root->ctx_stack =
     crealloc(root->ctx_stack, root->ctx_stack_alloc * sizeof(context *));
  }

  root->ctx_stack[root->ctx_stack_size] = ctx;
  root->ctx_stack_size++;
}

/**
 * Destroy the target context and all contexts above it.
 * Flag the core_context to abort further execution of the cycle.
 */

void destroy_context(context *target)
{
  core_context *root = target->root;
  context *ctx;

  if(!root->ctx_stack_size)
    return;

  do
  {
    root->ctx_stack_size--;
    ctx = root->ctx_stack[root->ctx_stack_size];
    if(ctx->destroy_function)
      ctx->destroy_function(ctx);
    free(ctx);
  }
  while(root->ctx_stack_size && ctx != target);

  root->context_destroyed = true;
}

/**
 * Generate the core_context struct. This struct imitates a regular context so
 * it can be used as a parent, but it should not be started like a context.
 */

core_context *core_init(struct world *mzx_world, struct global_data *data)
{
  core_context *root = cmalloc(sizeof(core_context));
  context *ctx = (context *)root;

  ctx->root = root;
  ctx->data = data;
  ctx->world = mzx_world;

  root->ctx_stack = NULL;
  root->ctx_stack_size = 0;
  root->ctx_stack_alloc = 0;
  root->full_exit = false;
  root->context_destroyed = false;

  return root;
}

/**
 * Draw all current contexts. Drawing starts at the lowest context allowed to
 * draw and progresses upward to the current context.
 */

static void core_draw(core_context *root)
{
  int i = root->ctx_stack_size - 1;
  context *ctx;

  // Find the first context to draw.
  while(i >= 0 && root->ctx_stack[i]->is_subcontext)
  {
    i--;
  }

  for(; i < root->ctx_stack_size; i++)
  {
    ctx = root->ctx_stack[i];
    if(ctx->draw_function)
      ctx->draw_function(ctx);
  }
}

/**
 * Update all current contexts, handling input and idle activity.
 * This update starts at the top of the stack and continues downward as allowed
 * by the settings of the contexts.
 */

static void core_update(core_context *root)
{
  context *ctx;
  boolean mouse_handled = false;
  boolean key_handled = false;

  int i = root->ctx_stack_size - 1;

  int key = get_key(keycode_internal_wrt_numlock);
  int mouse_press = get_mouse_press_ext();
  int mouse_drag_state = get_mouse_drag();
  int mouse_x;
  int mouse_y;

  get_mouse_position(&mouse_x, &mouse_y);

  do
  {
    ctx = root->ctx_stack[i];
    i--;

    if(!mouse_handled)
    {
      if(mouse_drag_state)
      {
        if(ctx->drag_function && (mouse_press <= MOUSE_BUTTON_RIGHT))
          mouse_handled |=
            ctx->drag_function(ctx, &key, mouse_press, mouse_x, mouse_y);
      }
      else

      if(mouse_press)
      {
        if(ctx->click_function)
          mouse_handled |=
            ctx->click_function(ctx, &key, mouse_press, mouse_x, mouse_y);
      }

      if(root->context_destroyed || root->full_exit)
        return;
    }

    if(!key_handled)
    {
      if(key && ctx->key_function)
        key_handled |= ctx->key_function(ctx, &key);

      if(root->context_destroyed || root->full_exit)
        return;
    }

    if(ctx->idle_function)
    {
      ctx->idle_function(ctx);

      if(root->context_destroyed || root->full_exit)
        return;
    }
  }
  while(i >= 0 && ctx->is_subcontext);

  // Global key handler.
  if(!key_handled)
  {
    ctx = root->ctx_stack[root->ctx_stack_size - 1];

    switch(key)
    {
#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        // Display help.
        // FIXME HELP_MENU, standalone, etc
        if(ctx->allow_help_system)
        {
          m_show();
          help_system(ctx->world);
        }
        break;
      }
#endif

      case IKEY_F2:
      {
        // Display settings menu.
        // FIXME F2_MENU, standalone, etc
        if(ctx->allow_configure_dialog)
        {
          game_settings(ctx->world);
        }
        break;
      }

      case IKEY_F12:
      {
        // Take screenshot.
        // FIXME
        break;
      }
    }
  }
}

// FIXME debug context
static boolean key_debug_context(context *ctx, int *key)
{
  switch(*key)
  {
    case IKEY_ESCAPE:
      core_exit(ctx);
      break;

    default:
      return false;
  }
  return true;
}
// FIXME end debug context

/**
 * Run the main game loop.
 */

void core_run(core_context *root)
{
  context *ctx;

  // FIXME debug context
  context *debug = cmalloc(sizeof(struct context));
  run_context(debug, (context *)root, CTX_MAIN,
   NULL, key_debug_context, NULL, NULL, NULL, NULL);
  // FIXME end debug context

  if(root->ctx_stack_size <= 0)
    return;

  do
  {
    root->context_destroyed = false;

    core_draw(root);
    update_screen();

    // Delay and then handle events.
    // Optionally allow the active context to accept events mid-frame.
    ctx = root->ctx_stack[root->ctx_stack_size - 1];

    if(ctx->allow_events_to_interrupt)
      update_event_status_intake();
    else
      update_event_status_delay();

    // This should not change at any point before the update function.
    if(root->full_exit)
      error("Context code bug", 2, 4, 0x2B02);

    // This should not change at any point before the update function.
    if(root->context_destroyed)
      error("Context code bug", 2, 4, 0x2B03);

    core_update(root);
  }
  while(!root->full_exit && root->ctx_stack_size > 0);
}

/**
 * Indicate that the core loop should exit.
 */

void core_exit(context *ctx)
{
  if(!ctx)
    error("Context code bug", 2, 4, 0x2B04);

  ctx->root->full_exit = true;
}

/**
 * Free all context data that still exists.
 */

void core_free(core_context *root)
{
  // Destroy all contexts on the stack.
  if(root->ctx_stack)
    destroy_context(root->ctx_stack[0]);

  free(root->ctx_stack);
  free(root);
}

// Deprecated crap.

static enum help_index indices[128] = { CTX_MAIN };
static enum help_index curr_index = 0;

enum help_index get_context(context *ctx)
{
  if(!curr_index && ctx)
    return ctx->help_index;

  return indices[curr_index];
}

void set_context(enum help_index idx)
{
  curr_index++;
  indices[curr_index] = idx;
}

void pop_context(void)
{
  if(curr_index > 0)
    curr_index--;
}

// Editor external function pointers (NULL by default).

void (*edit_world)(struct world *mzx_world, int reload_curr_file);
void (*debug_counters)(struct world *mzx_world);
void (*draw_debug_box)(struct world *mzx_world, int x, int y, int d_x, int d_y,
 int show_keys);
int (*debug_robot_break)(struct world *mzx_world, struct robot *cur_robot,
 int id, int lines_run);
int (*debug_robot_watch)(struct world *mzx_world, struct robot *cur_robot,
 int id, int lines_run);
void (*debug_robot_config)(struct world *mzx_world);

// Network external function pointers (NULL by default).

void (*check_for_updates)(struct world *mzx_world, struct config_info *conf,
 int is_automatic);
