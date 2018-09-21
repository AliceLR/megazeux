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

#include "caption.h"
#include "counter.h"
#include "configure.h"
#include "core.h"
#include "error.h"
#include "event.h"
#include "graphics.h"
#include "helpsys.h"
#include "world.h"
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
  boolean context_changed;
};

/**
 * Contains internal context data used to run contexts.
 * Subcontexts also use this, though they take functions that use subcontext *.
 * To avoid warnings, internally cast to functions that take context *.
 */

struct context_data
{
  boolean is_subcontext;
  enum context_type context_type;
  enum framerate_type framerate;
  subcontext **children;
  int num_children;
  int num_children_alloc;
  struct context_spec functions;
};

#ifdef CONFIG_FPS
#define FPS_HISTORY_SIZE 5
#define FPS_INTERVAL 1000

static double average_fps;

/**
 * Track the current speed MegaZeux is running in frames.
 */

static void update_fps(int current_ticks)
{
  static int fps_previous_ticks = -1;
  static int fps_history[FPS_HISTORY_SIZE];
  static int fps_history_count;
  static int frames_counted;
  int total_fps;
  int min_fps;
  int max_fps;
  int i;

  int delta_ticks = current_ticks - fps_previous_ticks;

  if(fps_previous_ticks == -1)
  {
    fps_previous_ticks = current_ticks;
    frames_counted = 0;
    for(i = 0; i < FPS_HISTORY_SIZE; i++)
      fps_history[i] = -1;
  }
  else

  if(delta_ticks >= FPS_INTERVAL)
  {
    for(i = FPS_HISTORY_SIZE - 1; i >= 1; i--)
    {
      fps_history[i] = fps_history[i - 1];
    }

    fps_history[0] = frames_counted;
    min_fps = fps_history[0];
    max_fps = fps_history[0];
    total_fps = 0;
    fps_history_count = 0;

    for(i = 0; i < FPS_HISTORY_SIZE; i++)
    {
      if(fps_history[i] > -1)
      {
        if(fps_history[i] > max_fps)
          max_fps = fps_history[i];

        if(fps_history[i] < min_fps)
          min_fps = fps_history[i];

        total_fps += fps_history[i];
        fps_history_count++;
      }
    }
    // Subtract off highest and lowest scores (outliers)
    total_fps -= max_fps;
    total_fps -= min_fps;
    if(fps_history_count > 2)
    {
      average_fps =
        1.0 * total_fps / (fps_history_count - 2) * (1000.0 / FPS_INTERVAL);

      caption_set_fps(average_fps);
    }
    fps_previous_ticks += FPS_INTERVAL;

    frames_counted = 0;
  }

  else
  {
    frames_counted++;
  }
}
#endif

#ifdef DEBUG
#define CTX_NAME_MAX_SIZE 16

/**
 * Get the name of a context from its ID (see core.h)
 */
static const char *get_ctx_name(enum context_type id)
{
  switch(id)
  {
    // Core contexts.
    case CTX_DEFAULT:           return "(default)";
    case CTX_TITLE_SCREEN:      return "Title screen";
    case CTX_MAIN:              return "(help main page)";
    case CTX_PLAY_GAME:         return "Gameplay";
    case CTX_CONFIGURE:         return "Settings ed.";
    case CTX_DIALOG_BOX:        return "Dialog";
    case CTX_HELP_SYSTEM:       return "Help system";
    case CTX_MAIN_MENU:         return "Main menu";
    case CTX_GAME_MENU:         return "Game menu";
    case CTX_INTAKE:            return "(intake string)";
    case CTX_INTAKE_NUM:        return "(intake number)";

    // Network contexts.
    case CTX_UPDATER:           return "Updater";

    // Editor contexts.
    case CTX_EDITOR:            return "Editor";
    case CTX_EDITOR_VIEW_BOARD: return "(view board)";
    case CTX_BLOCK_CMD:         return "Block command";
    case CTX_BLOCK_TYPE:        return "Block type";
    case CTX_CHOOSE_CHARSET:    return "Select charset";
    case CTX_IMPORTEXPORT_TYPE: return "Import/export";
    case CTX_CHAR_EDIT:         return "Char editor";
    case CTX_STATUS_COUNTERS:   return "Status counters";
    case CTX_BOARD_EXITS:       return "Board exits";
    case CTX_BOARD_SIZES:       return "Board sizes";
    case CTX_BOARD_INFO:        return "Board info";
    case CTX_CHANGE_CHAR_IDS:   return "Char ID table";
    case CTX_CHANGE_DAMAGE:     return "Damage table";
    case CTX_GLOBAL_SETTINGS:   return "Global settings1";
    case CTX_GLOBAL_SETTINGS_2: return "Global settings2";
    case CTX_ROBO_ED:           return "Robot editor";
    case CTX_PALETTE_EDITOR:    return "Palette editor";
    case CTX_SENSOR_EDITOR:     return "Sensor param";
    case CTX_SUPER_MEGAZEUX:    return "Select SMZX mode";
    case CTX_SFX_EDITOR:        return "SFX editor";
    case CTX_COUNTER_DEBUG:     return "Counter debugger";
    case CTX_ROBOT_DEBUG:       return "Robot debugger";
    case CTX_BREAKPOINT_EDITOR: return "Robot dbg. conf.";
    case CTX_VLAYER_SIZES:      return "Vlayer sizes";
  }
  return "?????";
}

/**
 * Debug: print a line of the context stack.
 */
static void print_ctx_line(context_data *ctx_data)
{
  char name[CTX_NAME_MAX_SIZE + 1] = "  -> subcontext";
  const char *framerate_str = "-";
  boolean click_drag_same = false;

  if(!ctx_data->is_subcontext)
  {
    snprintf(name, CTX_NAME_MAX_SIZE, "%s",
     get_ctx_name(ctx_data->context_type));

    switch(ctx_data->framerate)
    {
      case FRAMERATE_UI:            framerate_str = "UI "; break;
      case FRAMERATE_UI_INTERRUPT:  framerate_str = "Int"; break;
      case FRAMERATE_MZX_SPEED:     framerate_str = "MZX"; break;
      default:                      framerate_str = "???"; break;
    }
  }

  if(ctx_data->functions.click == ctx_data->functions.drag)
    click_drag_same = true;

  fprintf(stderr, "%-*.*s | %3s %3s %3s %3s %3s %3s %3s | %-3s \n",
    16, 16, name,
    ctx_data->functions.resume  ? "Yes" : "",
    ctx_data->functions.draw    ? "Yes" : "",
    ctx_data->functions.idle    ? "Yes" : "",
    ctx_data->functions.key     ? "Yes" : "",
    ctx_data->functions.click   ? "Yes" : "",
    ctx_data->functions.drag    ? click_drag_same ? "<- " : "Yes" : "",
    ctx_data->functions.destroy ? "Yes" : "",
    framerate_str
  );
}

/**
 * Debug: print the entire context stack to the terminal.
 */

static void print_core_stack(context *_ctx)
{
  core_context *root = _ctx->root;
  context_data *ctx_data;
  context_data *sub_data;
  int i;
  int i2;

  fprintf(stderr, "CTX STACK        | Res Drw Idl Key Clk Drg Dst | Fr. \n");
  fprintf(stderr, "-----------------|-----------------------------|-----\n");

  for(i = root->ctx_stack_size - 1; i >= 0; i--)
  {
    ctx_data = root->ctx_stack[i]->internal_data;

    for(i2 = ctx_data->num_children - 1; i2 >= 0; i2--)
    {
      sub_data = ctx_data->children[i2]->internal_data;
      print_ctx_line(sub_data);
    }

    print_ctx_line(ctx_data);
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}
#else // !DEBUG
static inline void print_core_stack(context *_ctx) {}
#endif // !DEBUG

/**
 * Add an element to a stack
 */
static void add_stack(void ***_stack, int *_num, int *_alloc, void *add)
{
  void **stack = *_stack;
  int num = *_num;
  int alloc = *_alloc;

  if(num >= alloc)
  {
    if(alloc == 0)
      alloc = 8;

    while(num >= alloc)
      alloc *= 2;

    stack = crealloc(stack, alloc * sizeof(void *));
    *_stack = stack;
    *_alloc = alloc;
  }

  stack[num] = add;
  *_num = (num + 1);
}

/**
 * Remove an element from a stack.
 */
static void remove_stack(void **stack, int *_num, void *del)
{
  int num = *_num;
  int i;

  for(i = num - 1; i >= 0; i--)
  {
    if(stack[i] == del)
    {
      if(i < num - 1)
        memmove(stack + i, stack + i + 1, num - i - 1);

      *_num = (num - 1);
      return;
    }
  }

  error("Context code bug", 2, 4, 0x2B06);
}

/**
 * Configures a provided context and adds it to the context stack to be run
 * by the main loop. The assumption here is that ctx will be some other local
 * struct casted to a context.
 *
 * At least one update function needs to be provided capable of destroying the
 * context; otherwise, MegaZeux would hang up trying to execute this context.
 */

void create_context(context *ctx, context *parent,
 struct context_spec *ctx_spec, enum context_type context_type)
{
  core_context *root;
  context_data *ctx_data;

  if(parent == NULL || !ctx_spec ||
   (ctx_spec->key == NULL && ctx_spec->click == NULL &&
    ctx_spec->drag == NULL && ctx_spec->idle == NULL))
    error("Context code bug", 2, 4, 0x2B01);

  // If the parent is a subcontext, try to find the real parent context.
  while(parent->parent && parent->internal_data &&
   parent->internal_data->is_subcontext)
    parent = parent->parent;

  // Root needs to exist so this context can be added to the stack.
  if(!parent->root)
    error("Context code bug", 2, 4, 0x2B07);

  if(!ctx) ctx = cmalloc(sizeof(struct context));
  ctx_data = cmalloc(sizeof(struct context_data));

  ctx->root = parent->root;
  ctx->internal_data = ctx_data;
  ctx->world = parent->world;
  ctx->data = parent->data;
  ctx->parent = NULL;
  ctx_data->context_type = context_type;
  ctx_data->framerate = ctx_spec->framerate_mode;
  ctx_data->is_subcontext = false;
  ctx_data->children = NULL;
  ctx_data->num_children = 0;
  ctx_data->num_children_alloc = 0;
  memcpy(&(ctx_data->functions), ctx_spec, sizeof(struct context_spec));

  // Add the new context to the stack.
  root = parent->root;
  add_stack((void ***) &(root->ctx_stack),
   &(root->ctx_stack_size), &(root->ctx_stack_alloc), ctx);

  root->context_changed = true;
}

/**
 * Creates a subcontext and adds it to the parent context.
 */

CORE_LIBSPEC void create_subcontext(subcontext *sub, context *parent,
 struct context_spec *sub_spec)
{
  core_context *root;
  context_data *parent_data;
  context_data *sub_data;

  // If the parent is a subcontext, try to find the real parent context.
  while(parent && parent->internal_data && parent->internal_data->is_subcontext)
    parent = parent->parent;

  // Root context must exit, parent must not be the root context, and make sure
  // this isn't some weird glitched context.
  if(!parent || !parent->root || parent == (context *)(parent->root) ||
   parent->parent || !parent->internal_data || !sub_spec)
    error("Context code bug", 2, 4, 0x2B08);

  root = parent->root;
  parent_data = parent->internal_data;

  if(!sub) sub = cmalloc(sizeof(struct context));
  sub_data = cmalloc(sizeof(struct context_data));

  sub->root = root;
  sub->internal_data = sub_data;
  sub->world = parent->world;
  sub->data = parent->data;
  sub->parent = parent;
  sub_data->is_subcontext = true;
  memcpy(&(sub_data->functions), sub_spec, sizeof(struct context_spec));

  // Add the subcontext to the parent's stack.
  add_stack((void ***) &(parent_data->children),
   &(parent_data->num_children), &(parent_data->num_children_alloc), sub);
}

/**
 * Destroy the target context or subcontext from its parent stack.
 * Flag the core_context to abort further execution of the cycle if the root
 * context stack is changed.
 */

void destroy_context(context *ctx)
{
  core_context *root = ctx->root;
  context_data *ctx_data = ctx->internal_data;

  if(!ctx->parent && ctx_data && !ctx_data->is_subcontext)
  {
    // This is a root-level context, so remove it from the context stack.

    // If the context isn't on the stack, this will error.
    remove_stack((void **)(root->ctx_stack), &(root->ctx_stack_size), ctx);
    root->context_changed = true;

    // Also, destroy all children.
    if(ctx_data->num_children)
    {
      int i;
      for(i = ctx_data->num_children - 1; i >= 0; i--)
        destroy_context(ctx_data->children[i]);
    }

    free(ctx_data->children);
  }
  else
  {
    // This is a subcontext, so remove it from its parent context.
    context_data *parent_data = ctx->parent->internal_data;

    // If the subcontext isn't on the stack, this will error.
    remove_stack((void **)(parent_data->children), &(parent_data->num_children),
     ctx);
  }

  if(ctx_data->functions.destroy)
    ctx_data->functions.destroy(ctx);

  free(ctx_data);
  free(ctx);
}

/**
 * Find out if a context change has occurred.
 */

boolean has_context_changed(context *ctx)
{
  if(!ctx || !ctx->root)
    error("Context code bug", 2, ERROR_OPT_EXIT, 0x2B09);

  return (ctx->root->context_changed);
}

/**
 * Find out if the given context is a particular context.
 */

boolean is_context(context *ctx, enum context_type context_type)
{
  if(!ctx || !ctx->internal_data)
    error("Context code bug", 2, ERROR_OPT_EXIT, 0x2B02);

  return (ctx->internal_data->context_type == context_type);
}

/**
 * Set the framerate mode for the current context.
 */

void set_context_framerate_mode(context *ctx, enum framerate_type framerate)
{
  if(!ctx || !ctx->internal_data)
    error("Context code bug", 2, ERROR_OPT_EXIT, 0x2B03);

  ctx->internal_data->framerate = framerate;
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
  ctx->internal_data = NULL;

  root->ctx_stack = NULL;
  root->ctx_stack_size = 0;
  root->ctx_stack_alloc = 0;
  root->full_exit = false;
  root->context_changed = false;

  return root;
}

/**
 * Execute the resume function for the current context after a context change.
 */

static void core_resume(core_context *root)
{
  context *ctx = root->ctx_stack[root->ctx_stack_size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *sub_data;
  subcontext *sub;
  int i;

  if(ctx_data->functions.resume)
    ctx_data->functions.resume(ctx);

  for(i = 0; i < ctx_data->num_children; i++)
  {
    sub = ctx_data->children[i];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->functions.resume)
      sub_data->functions.resume((context *)sub);
  }
}

/**
 * Draw the current context.
 */

static void core_draw(core_context *root)
{
  context *ctx = root->ctx_stack[root->ctx_stack_size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *sub_data;
  subcontext *sub;
  int i;

  if(ctx_data->functions.draw)
    ctx_data->functions.draw(ctx);

  for(i = 0; i < ctx_data->num_children; i++)
  {
    sub = ctx_data->children[i];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->functions.draw)
    {
      select_layer(UI_LAYER);
      sub_data->functions.draw((context *)sub);
    }
  }

#ifdef CONFIG_FPS
  if(is_fullscreen() && ctx_data->context_type != CTX_EDITOR)
  {
    // If we're in fullscreen mode, draw an onscreen FPS display.
    char fpsbuf[32];
    snprintf(fpsbuf, 32, "  %.2f  ", average_fps);

    select_layer(UI_LAYER);
    write_string(fpsbuf, 0, 0, 0x0f, false);
  }
#endif
}

/**
 * Determine if a given context exists on the stack.
 */
static boolean is_on_stack(core_context *root, enum context_type type)
{
  int i = root->ctx_stack_size - 1;
  context *ctx;

  for(; i >= 0; i--)
  {
    ctx = root->ctx_stack[i];
    if(ctx->internal_data->context_type == type)
      return true;
  }

  return false;
}

#ifdef CONFIG_HELPSYS
/**
 * Determine if the help system is currently allowed.
 */
static boolean allow_help_system(core_context *root)
{
  struct world *mzx_world = ((context *)root)->world;
  struct config_info *conf = get_config((context *)root);

  if(is_on_stack(root, CTX_HELP_SYSTEM))
    return false;

  if(mzx_world->active && mzx_world->version >= V260)
  {
    if(is_on_stack(root, CTX_PLAY_GAME) ||
     (is_on_stack(root, CTX_TITLE_SCREEN) && conf->standalone_mode))
    {
      if(!get_counter(mzx_world, "HELP_MENU", 0))
        return false;
    }
  }

  return true;
}
#endif

/**
 * Determine if the configure menu is currently allowed.
 */
static boolean allow_configure(core_context *root)
{
  struct world *mzx_world = ((context *)root)->world;
  struct config_info *conf = get_config((context *)root);

  if(is_on_stack(root, CTX_CONFIGURE))
    return false;

  if(is_on_stack(root, CTX_HELP_SYSTEM))
    return false;

  // Bypass F2_MENU counter.
  if(get_ctrl_status(keycode_internal) && !conf->standalone_mode)
    return true;

  if(mzx_world->active && mzx_world->version >= V260)
  {
    if(is_on_stack(root, CTX_PLAY_GAME) ||
     (is_on_stack(root, CTX_TITLE_SCREEN) && conf->standalone_mode))
    {
      if(!get_counter(mzx_world, "F2_MENU", 0))
        return false;
    }
  }

  return true;
}

/**
 * Update the current context, handling input and idle activity.
 * If any subcontexts are attached to the context, they will be handled from
 * most recent to oldest, and the context will be handled last. Notes:
 *
 * 1) Execution order: Idle -> Click OR Drag -> Key
 * 2) A true return value of idle cancels both key and mouse handling.
 * 3) A true return value of key cancels key handling.
 * 4) A true return value of click or drag cancels mouse handling.
 * 5) Click function retriggers on autorepeats.
 * 6) Drag triggers after first click frame.
 * 7) Drag always takes precedence over click, including autorepeats.
 */

static void core_update(core_context *root)
{
  context *ctx = root->ctx_stack[root->ctx_stack_size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *cur_data;
  context *cur;

  boolean mouse_handled = false;
  boolean key_handled = false;

  int i = ctx_data->num_children - 1;

  boolean exit_status = get_exit_status();
  int key = get_key(keycode_internal_wrt_numlock);
  int mouse_press = get_mouse_press_ext();
  int mouse_drag_state = get_mouse_drag();
  int mouse_x;
  int mouse_y;

  get_mouse_position(&mouse_x, &mouse_y);

  do
  {
    // Count down to -1 and break when cur equals ctx.
    // Slightly convoluted, but better than duplicating this loop's logic.
    if(i < 0)
    {
      cur = ctx;
      cur_data = ctx_data;
    }
    else
    {
      cur = (context *)ctx_data->children[i];
      cur_data = cur->internal_data;
      i--;
    }

    if(cur_data->functions.idle)
    {
      if(cur_data->functions.idle(cur))
      {
        mouse_handled = true;
        key_handled = true;
      }

      if(root->context_changed || root->full_exit)
        return;
    }

    if(!mouse_handled)
    {
      if(mouse_drag_state && cur_data->functions.drag)
      {
        mouse_handled |=
         cur_data->functions.drag(cur, &key, mouse_press, mouse_x, mouse_y);
      }
      else

      if(mouse_press && cur_data->functions.click)
      {
        mouse_handled |=
         cur_data->functions.click(cur, &key, mouse_press, mouse_x, mouse_y);
      }

      if(root->context_changed || root->full_exit)
        return;
    }

    if(!key_handled)
    {
      if((key || exit_status) && cur_data->functions.key)
        key_handled |= cur_data->functions.key(cur, &key);

      if(root->context_changed || root->full_exit)
        return;
    }
  }
  while(cur != ctx);

  // Global key handler.
  if(!key_handled)
  {
    switch(key)
    {
#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        // Display help.
        if(allow_help_system(root))
        {
          m_show(); // FIXME
          help_system(ctx, ctx->world);
        }
        break;
      }
#endif

      case IKEY_F2:
      {
        // Display settings menu.
        if(allow_configure(root))
        {
          m_show(); // FIXME
          game_settings(ctx->world);
        }
        break;
      }

#ifdef CONFIG_ENABLE_SCREENSHOTS
      case IKEY_F12:
      {
        // Take screenshot.
        if(get_config(ctx)->allow_screenshots)
          dump_screen();
        break;
      }
#endif
    }
  }
}

/**
 * Run the main game loop.
 */

void core_run(core_context *root)
{
  context *ctx;
  int start_ticks = get_ticks();
  int delta_ticks;
  int total_ticks;

  if(root->ctx_stack_size <= 0)
    return;

  do
  {
    // Resume might trigger additional context changes.
    while(root->context_changed)
    {
      root->context_changed = false;
      print_core_stack((context *)root);
      force_release_all_keys();
      core_resume(root);

      // Resume might also trigger an exit.
      if(root->full_exit)
        return;
    }

    core_draw(root);
    update_screen();

    // Delay and then handle events.
    ctx = root->ctx_stack[root->ctx_stack_size - 1];

    switch(ctx->internal_data->framerate)
    {
      case FRAMERATE_UI:
      {
        // Delay for a standard (fixed) amount of time.
        update_event_status_delay();
        break;
      }

      case FRAMERATE_UI_INTERRUPT:
      {
        // Delay for the standard amount of time or until an event is detected.
        // Use for interfaces that need precise keypress detection, like typing.
        update_event_status_intake();
        break;
      }

      case FRAMERATE_MZX_SPEED:
      {
        // Delay according to mzx_speed and execution time.
        if(ctx->world->mzx_speed > 1)
        {
          // Number of ms the update cycle took
          delta_ticks = get_ticks() - start_ticks;
          total_ticks = (16 * (ctx->world->mzx_speed - 1)) - delta_ticks;
          if(total_ticks < 0)
            total_ticks = 0;

          // Delay for 16 * (speed - 1) since the beginning of the update
          delay(total_ticks);
        }

        update_event_status();
        break;
      }

      default:
      {
        error("Context code bug", 2, 4, 0x2B05);
      }
    }

    start_ticks = get_ticks();

#ifdef CONFIG_FPS
    update_fps(start_ticks);
#endif

    // These might have been triggered during the draw.
    if(!root->full_exit && !root->context_changed)
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
  int i;

  // Destroy all contexts on the stack.
  for(i = root->ctx_stack_size - 1; i >= 0; i--)
    destroy_context(root->ctx_stack[i]);

  free(root->ctx_stack);
  free(root);
}

// Deprecated.
static enum context_type indices[128] = { CTX_MAIN };
static int curr_index = 0;

/**
 * Get the most recent context ID associated with the help system.
 * TODO rename after the deprecated features are no longer necessary.
 */

enum context_type get_context(context *ctx)
{
  if(!curr_index && ctx)
  {
    core_context *root = ctx->root;
    enum context_type ctx_type;
    int i;

    for(i = root->ctx_stack_size - 1; i >= 0; i--)
    {
      ctx_type = root->ctx_stack[i]->internal_data->context_type;

      // Help system only cares about positive context values.
      if(ctx_type > 0)
        return ctx_type;
    }

    return CTX_MAIN;
  }

  return indices[curr_index];
}

// Deprecated.
void set_context(enum context_type idx)
{
  curr_index++;
  indices[curr_index] = idx;
}

// Deprecated.
void pop_context(void)
{
  if(curr_index > 0)
    curr_index--;
}

// Editor external function pointers (NULL by default).

void (*edit_world)(context *parent, boolean reload_curr_file);
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
