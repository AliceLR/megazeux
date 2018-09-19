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

#include "caption.h"
#include "graphics.h"
#include "world_struct.h"

// TODO this might benefit from a dirty flag + graphics.c hook and/or
// integration into mzx_world to remove the statics.

#define MAX_CAPTION_SIZE 120
#define CAPTION_SPACER " :: "

static struct world *caption_world;
static struct board *caption_board;
static struct robot *caption_robot;
static boolean caption_updates;
static boolean caption_modified;
#ifdef CONFIG_FPS
static double caption_fps;
#endif

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

// Make GCC shut up
#if __GNUC__ >= 4
static inline void caption_append(char caption[MAX_CAPTION_SIZE],
 const char *f, ...) __attribute__((format(gnu_printf, 2, 3)));
#endif

static inline void caption_append(char caption[MAX_CAPTION_SIZE],
 const char *f, ...)
{
  size_t len = strlen(caption);

  if(len < MAX_CAPTION_SIZE - 1)
  {
    va_list args;
    va_start(args, f);

    vsnprintf(caption + len, MAX_CAPTION_SIZE - len, f, args);

    va_end(args);
  }
}

/**
 * Sets the caption to reflect current MegaZeux state information.
 */

static void update_caption(void)
{
  char caption[MAX_CAPTION_SIZE];
  char stripped_name[MAX_CAPTION_SIZE];
  caption[0] = '\0';

  if(caption_modified)
    strcpy(caption, "*");

  if(caption_robot)
  {
    if(!strip_caption_string(stripped_name, caption_robot->robot_name))
      strcpy(stripped_name, "Untitled robot");

    caption_append(caption, "%s (%i,%i)" CAPTION_SPACER,
     stripped_name, caption_robot->xpos, caption_robot->ypos);
  }

  if(caption_board)
  {
    if(!strip_caption_string(stripped_name, caption_board->board_name))
      strcpy(stripped_name, "Untitled board");

    caption_append(caption, "%s" CAPTION_SPACER, stripped_name);
  }

  if(caption_world && caption_world->active)
  {
    if(!strip_caption_string(stripped_name, caption_world->name))
      strcpy(stripped_name, "Untitled world");

    caption_append(caption, "%s" CAPTION_SPACER, stripped_name);
  }

  caption_append(caption, "%s", get_default_caption());

  if(caption_world && caption_world->editing)
    caption_append(caption, " (editor)");

#ifdef CONFIG_UPDATER
  if(caption_updates)
    caption_append(caption, " *** UPDATES AVAILABLE ***");
#endif

#ifdef CONFIG_FPS
  if(caption_fps > 0.001)
    caption_append(caption, CAPTION_SPACER "FPS: %.2f", caption_fps);
#endif /* CONFIG_FPS */

  caption[MAX_CAPTION_SIZE - 1] = 0;
  set_window_caption(caption);
}

void caption_set_world(struct world *mzx_world)
{
  caption_world = mzx_world;
  caption_board = NULL;
  caption_robot = NULL;
  caption_modified = false;
  update_caption();
}

#ifdef CONFIG_EDITOR
void caption_set_board(struct world *mzx_world, struct board *board)
{
  caption_world = mzx_world;
  caption_board = board;
  caption_robot = NULL;
  update_caption();
}

void caption_set_robot(struct world *mzx_world, struct robot *robot)
{
  caption_world = mzx_world;
  caption_board = mzx_world->current_board;
  caption_robot = robot;
  update_caption();
}

void caption_set_modified(boolean modified)
{
  caption_modified = modified;
  update_caption();
}
#endif

#ifdef CONFIG_UPDATER
void caption_set_updates_available(boolean available)
{
  caption_updates = available;
  update_caption();
}
#endif

#ifdef CONFIG_FPS
void caption_set_fps(double fps)
{
  caption_fps = fps;
  update_caption();
}
#endif
