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

#include "core.h"
#include "error.h"
#include "event.h"
#include "graphics.h"
#include "world_struct.h"

// Contains context stack information.
// Used as a context to help create the first actual context.
struct core_context
{
  context ctx;
  context **ctx_stack;
  int ctx_stack_alloc;
  int ctx_stack_size;
};

void create_context(context *ctx, context *parent,
 enum help_index help_index,
 void (*draw_function)(context *),
 boolean (*update_function)(context *),
 void (*destroy_function)(context *))
{
  core_context *root = parent->root;

  if(update_function == NULL)
    error("Context code bug", 2, 4, 0x2B01);

  ctx->root = root;
  ctx->data = parent->data;
  ctx->mzx_world = parent->mzx_world;
  ctx->draw_while_under = false;
  ctx->update_while_under = false;
  ctx->allow_events_to_interrupt = false;
  ctx->help_index = help_index;
  ctx->draw_function = draw_function;
  ctx->update_function = update_function;
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

void destroy_context(context *target)
{
  // Destroy the target context and all children.
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
}

core_context *core_init(struct world *mzx_world, struct global_data *global_data)
{
  core_context *root = cmalloc(sizeof(core_context));
  context *ctx = (context *)root;

  ctx->root = root;
  ctx->data = global_data;
  ctx->mzx_world = mzx_world;

  root->ctx_stack = NULL;
  root->ctx_stack_size = 0;
  root->ctx_stack_alloc = 0;

  return root;
}

void core_run(core_context *root)
{
  context *ctx;
  int i;

  if(root->ctx_stack_size <= 0)
    return;

  do
  {
    // Find the first context to draw.
    i = root->ctx_stack_size - 1;
    do
    {
      i--;
      if(i >= 0)
        ctx = root->ctx_stack[i];
    }
    while(i >= 0 && ctx->draw_while_under);
    i++;

    // Draw current context(s) from bottom to top.
    while(i < root->ctx_stack_size)
    {
      ctx = root->ctx_stack[i];
      if(ctx->draw_function)
        ctx->draw_function(ctx);
    }

    // Update the display.
    update_screen();

    if(root->ctx_stack_size <= 0)
      return;

    // Delay and then handle events.
    // Optionally allow the active context to stop the delay mid-frame.
    ctx = root->ctx_stack[root->ctx_stack_size - 1];
    if(ctx->allow_events_to_interrupt)
      update_event_status_intake();
    else
      update_event_status_delay();

    if(root->ctx_stack_size <= 0)
      return;

    // Update current context(s) from top to bottom.
    do
    {
      ctx = root->ctx_stack[i];
      if(ctx->update_function)
        if(ctx->update_function(ctx))
          break;
      i--;
    }
    while(i >= 0 && i < root->ctx_stack_size &&
     root->ctx_stack[i]->update_while_under);
  }
  while(root->ctx_stack_size > 0);
}

void core_quit(core_context *root)
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
  indices[curr_index++] = idx;
}

void pop_context(void)
{
  if(curr_index > 0)
    curr_index--;
}

// Editor external function pointers.

#ifdef CONFIG_EDITOR
void (*edit_world)(struct world *mzx_world, int reload_curr_file);
void (*debug_counters)(struct world *mzx_world);
void (*draw_debug_box)(struct world *mzx_world, int x, int y, int d_x, int d_y,
 int show_keys);
int (*debug_robot_break)(struct world *mzx_world, struct robot *cur_robot,
 int id, int lines_run);
int (*debug_robot_watch)(struct world *mzx_world, struct robot *cur_robot,
 int id, int lines_run);
void (*debug_robot_config)(struct world *mzx_world);
#endif

// Network external function pointers.

#ifdef CONFIG_UPDATER
void (*check_for_updates)(struct world *mzx_world, struct config_info *conf,
 int is_automatic);
#endif
