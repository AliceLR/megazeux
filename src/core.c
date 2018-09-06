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

#include "counter.h"
#include "configure.h"
#include "core.h"
#include "error.h"
#include "event.h"
#include "graphics.h"
#include "helpsys.h"
#include "world.h"
#include "world_struct.h"

#define MAX_CAPTION_SIZE 120
#define CAPTION_SPACER " :: "

#ifdef CONFIG_FPS
static double average_fps;
#endif

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

typedef void (*resume_fn)(context *);
typedef void (*draw_fn)(context *);
typedef boolean (*idle_fn)(context *);
typedef boolean (*key_fn)(context *, int *key);
typedef boolean (*click_fn)(context *, int *key, int button, int x, int y);
typedef boolean (*drag_fn)(context *, int *key, int button, int x, int y);
typedef void (*destroy_fn)(context *);

struct context_data
{
  enum context_type context_type;
  enum framerate_type framerate;
  subcontext **children;
  int num_children;
  int num_children_alloc;

  resume_fn   resume_function;
  draw_fn     draw_function;
  idle_fn     idle_function;
  key_fn      key_function;
  click_fn    click_function;
  drag_fn     drag_function;
  destroy_fn  destroy_function;
};

/**
 * Strip a string for display in the caption.
 */

static boolean strip_caption_string(char *output, char *input)
{
  int len = strlen(input);
  int i, j;
  output[0] = '\0';

  for(i = 0, j = 0; i < len; i++)
  {
    if(input[i] < 32 || input[i] > 126)
      continue;

    if(input[i] == '~' || input[i] == '@')
    {
      i++;
      if(input[i - 1] != input[i])
        continue;
    }

    output[j] = input[i];

    if(output[j] != ' ' || (j > 0 && output[j - 1] != ' '))
      j++;
  }

  if(j > 0 && output[j - 1] == ' ')
    j--;

  output[j] = '\0';
  return (j > 0);
}

/**
 * Append a string to the caption.
 */

#define caption_append(caption, ...)                                \
{                                                                   \
  size_t len = strlen(caption);                                     \
  if(len < MAX_CAPTION_SIZE - 1)                                    \
    snprintf(caption + len, MAX_CAPTION_SIZE - len, __VA_ARGS__);   \
}

/**
 * Sets the caption to reflect current MegaZeux state information.
 */

void set_caption(struct world *mzx_world, struct board *board,
 struct robot *robot, boolean modified)
{
  char caption[MAX_CAPTION_SIZE];
  char stripped_name[MAX_CAPTION_SIZE];
  caption[0] = '\0';

  if(modified)
    strcpy(caption, "*");

  if(robot)
  {
    if(!strip_caption_string(stripped_name, robot->robot_name))
      strcpy(stripped_name, "Untitled robot");

    caption_append(caption, "%s (%i,%i)" CAPTION_SPACER,
     stripped_name, robot->xpos, robot->ypos);
  }

  if(board)
  {
    if(!strip_caption_string(stripped_name, board->board_name))
      strcpy(stripped_name, "Untitled board");

    caption_append(caption, "%s" CAPTION_SPACER, stripped_name);
  }

  if(mzx_world->active)
  {
    if(!strip_caption_string(stripped_name, mzx_world->name))
      strcpy(stripped_name, "Untitled world");

    caption_append(caption, "%s" CAPTION_SPACER, stripped_name);
  }

  caption_append(caption, "%s", get_default_caption());

  if(mzx_world->editing)
  {
    caption_append(caption, " (editor)");
  }

#ifdef CONFIG_UPDATER
  if(mzx_world->conf.update_available) // FIXME
  {
    caption_append(caption, " *** UPDATES AVAILABLE ***");
  }
#endif

#ifdef CONFIG_FPS
  if(mzx_world->active && !robot && !board)
  {
    caption_append(caption, CAPTION_SPACER "FPS: %.2f", average_fps);
  }
#endif /* CONFIG_FPS */

  caption[MAX_CAPTION_SIZE - 1] = 0;
  set_window_caption(caption);
}

#ifdef DEBUG
/**
 * Debug: print a line of the context stack.
 */
static void print_ctx_line(context_data *ctx_data, boolean is_sub)
{
  char name[17] = "  -> subcontext";
  const char *framerate_str = "-";
  boolean click_drag_same = false;

  if(!is_sub)
  {
    sprintf(name, "%d", ctx_data->context_type);
    switch(ctx_data->framerate)
    {
      case FRAMERATE_UI:            framerate_str = "UI "; break;
      case FRAMERATE_UI_INTERRUPT:  framerate_str = "Int"; break;
      case FRAMERATE_MZX_SPEED:     framerate_str = "MZX"; break;
      default:                      framerate_str = "???"; break;
    }
  }

  if(ctx_data->click_function == ctx_data->drag_function)
    click_drag_same = true;

  fprintf(stderr, "%-*.*s | %3s %3s %3s %3s %3s %3s %3s | %-3s \n",
    16, 16, name,
    ctx_data->resume_function   ? "Yes" : "",
    ctx_data->draw_function     ? "Yes" : "",
    ctx_data->idle_function     ? "Yes" : "",
    ctx_data->key_function      ? "Yes" : "",
    ctx_data->click_function    ? "Yes" : "",
    ctx_data->drag_function     ? click_drag_same ? "<- " : "Yes" : "",
    ctx_data->destroy_function  ? "Yes" : "",
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
      sub_data = ctx_data->children[i2]->ctx.internal_data;
      print_ctx_line(sub_data, true);
    }

    print_ctx_line(ctx_data, false);
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
 enum context_type context_type,
 void (*resume_function)(context *),
 void (*draw_function)(context *),
 boolean (*idle_function)(context *),
 boolean (*key_function)(context *, int *key),
 boolean (*click_function)(context *, int *key, int button, int x, int y),
 boolean (*drag_function)(context *, int *key, int button, int x, int y),
 void (*destroy_function)(context *))
{
  core_context *root = parent->root;
  context_data *ctx_data;

  if(key_function == NULL && click_function == NULL &&
   drag_function == NULL && idle_function == NULL)
    error("Context code bug", 2, 4, 0x2B01);

  if(!ctx) ctx = cmalloc(sizeof(struct context));
  ctx_data = cmalloc(sizeof(struct context_data));

  ctx->root = root;
  ctx->internal_data = ctx_data;
  ctx->world = parent->world;
  ctx->data = parent->data;
  ctx_data->context_type = context_type;
  ctx_data->framerate = FRAMERATE_UI;
  ctx_data->resume_function = resume_function;
  ctx_data->draw_function = draw_function;
  ctx_data->key_function = key_function;
  ctx_data->click_function = click_function;
  ctx_data->drag_function = drag_function;
  ctx_data->idle_function = idle_function;
  ctx_data->destroy_function = destroy_function;
  ctx_data->children = NULL;
  ctx_data->num_children = 0;
  ctx_data->num_children_alloc = 0;

  if(!root)
    error("Context code bug", 2, 4, 0x2B07);

  add_stack((void ***) &(root->ctx_stack),
   &(root->ctx_stack_size), &(root->ctx_stack_alloc), ctx);

  root->context_changed = true;
}

/**
 * Destroy the target context from the context stack.
 * Flag the core_context to abort further execution of the cycle.
 */

void destroy_context(context *ctx)
{
  core_context *root = ctx->root;
  context_data *ctx_data = ctx->internal_data;

  // If the context isn't on the stack, this will error.
  remove_stack((void **)(root->ctx_stack), &(root->ctx_stack_size), ctx);

  // Also, destroy all children.
  if(ctx_data->num_children)
  {
    int i;
    for(i = ctx_data->num_children - 1; i >= 0; i--)
      destroy_subcontext(ctx_data->children[i]);
  }

  if(ctx_data->destroy_function)
    ctx_data->destroy_function(ctx);

  free(ctx_data->children);
  free(ctx_data);
  free(ctx);

  root->context_changed = true;
}

/**
 * Creates a subcontext and adds it to the parent context.
 */

CORE_LIBSPEC void create_subcontext(subcontext *sub, context *parent,
 void (*resume_function)(subcontext *),
 void (*draw_function)(subcontext *),
 boolean (*idle_function)(subcontext *),
 boolean (*key_function)(subcontext *, int *key),
 boolean (*click_function)(subcontext *, int *key, int button, int x, int y),
 boolean (*drag_function)(subcontext *, int *key, int button, int x, int y),
 void (*destroy_function)(subcontext *))
{
  core_context *root = parent->root;
  context_data *parent_data = parent->internal_data;
  context_data *sub_data;
  context *ctx;

  if(!sub) sub = cmalloc(sizeof(struct subcontext));
  sub_data = cmalloc(sizeof(struct context_data));

  ctx = &(sub->ctx);
  ctx->root = root;
  ctx->internal_data = sub_data;
  ctx->world = parent->world;
  ctx->data = parent->data;
  sub->parent = parent;
  sub_data->resume_function = (resume_fn)resume_function;
  sub_data->draw_function = (draw_fn)draw_function;
  sub_data->key_function = (key_fn)key_function;
  sub_data->click_function = (click_fn)click_function;
  sub_data->drag_function = (drag_fn)drag_function;
  sub_data->idle_function = (idle_fn)idle_function;
  sub_data->destroy_function = (destroy_fn)destroy_function;

  if(!parent_data)
    error("Context code bug", 2, 4, 0x2B08);

  add_stack((void ***) &(parent_data->children),
   &(parent_data->num_children), &(parent_data->num_children_alloc), ctx);
}

/**
 * Destroys a subcontext.
 */

CORE_LIBSPEC void destroy_subcontext(subcontext *sub)
{
  context *parent = sub->parent;
  context_data *parent_data = parent->internal_data;
  context_data *sub_data = sub->ctx.internal_data;

  // If the subcontext isn't on the stack, this will error.
  remove_stack((void **)(parent_data->children), &(parent_data->num_children),
   sub);

  // This function actually takes subcontext *, but was previously cast.
  if(sub_data->destroy_function)
    sub_data->destroy_function((void *)sub);

  free(sub_data);
  free(sub);
}

/**
 * Set the framerate mode for the current context.
 */

void set_context_framerate_mode(context *ctx, enum framerate_type framerate)
{
  if(ctx->internal_data->framerate != framerate)
  {
    ctx->internal_data->framerate = framerate;
  }
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

  if(ctx_data->resume_function)
    ctx_data->resume_function(ctx);

  for(i = 0; i < ctx_data->num_children; i++)
  {
    // This function actually takes subcontext *, but was previously cast.
    sub = ctx_data->children[i];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->resume_function)
      sub_data->resume_function((void *)sub);
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

  if(ctx_data->draw_function)
    ctx_data->draw_function(ctx);

  for(i = 0; i < ctx_data->num_children; i++)
  {
    // This function actually takes subcontext *, but was previously cast.
    sub = ctx_data->children[i];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->draw_function)
    {
      select_layer(UI_LAYER);
      sub_data->draw_function((void *)sub);
    }
  }
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
  void *cur;

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
      cur = ctx_data->children[i];
      cur_data = ((context *)cur)->internal_data;
      i--;
    }

    if(cur_data->idle_function)
    {
      if(cur_data->idle_function(cur))
      {
        mouse_handled = true;
        key_handled = true;
      }

      if(root->context_changed || root->full_exit)
        return;
    }

    if(!mouse_handled)
    {
      if(mouse_drag_state && cur_data->drag_function)
      {
        mouse_handled |=
         cur_data->drag_function(cur, &key, mouse_press, mouse_x, mouse_y);
      }
      else

      if(mouse_press && cur_data->click_function)
      {
        mouse_handled |=
         cur_data->click_function(cur, &key, mouse_press, mouse_x, mouse_y);
      }

      if(root->context_changed || root->full_exit)
        return;
    }

    if(!key_handled)
    {
      if((key || exit_status) && cur_data->key_function)
        key_handled |= cur_data->key_function(cur, &key);

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
          help_system(ctx->world);
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

#ifdef CONFIG_FPS
#define FPS_HISTORY_SIZE 5
#define FPS_INTERVAL 1000
  int fps_history[FPS_HISTORY_SIZE];
  int fps_previous_ticks = -1;
  int fps_history_count;
  int frames_counted;
  int total_fps;
  int min_fps;
  int max_fps;
  int i;
#endif

  if(root->ctx_stack_size <= 0)
    return;

  do
  {
    if(root->context_changed)
    {
      print_core_stack((context *)root);
      force_release_all_keys();
      core_resume(root);
      root->context_changed = false;
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
    delta_ticks = start_ticks - fps_previous_ticks;

    if(fps_previous_ticks == -1)
    {
      fps_previous_ticks = start_ticks;
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

        if(!ctx->world->editing)
          set_caption(ctx->world, NULL, NULL, 0);
      }
      fps_previous_ticks += FPS_INTERVAL;

      frames_counted = 0;
    }

    else
    {
      frames_counted++;
    }
#endif

    // This should not change at any point before the update function.
    if(root->full_exit)
      error("Context code bug", 2, 4, 0x2B02);

    // This should not change at any point before the update function.
    if(root->context_changed)
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
