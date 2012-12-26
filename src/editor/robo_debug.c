/* MegaZeux
 *
 * Copyright (C) 2012 Alice Lauren Rowan <petrifiedrowan@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "robo_debug.h"
#include "debug.h"

#include "../event.h"
#include "../graphics.h"
#include "../window.h"
#include "../util.h"

#include "../world_struct.h"
#include "../robot_struct.h"
#include "../legacy_rasm.h"

static int robo_debugger_enabled = 0;
static int robo_debugger_override = 0;

static int num_break_points = 0;
static int num_break_points_allocated = 0;
static struct break_point **break_points = NULL;

static bool step = false;
static int selected = 0;

/*********************/
/* Breakpoint editor */
/*********************/

static int edit_breakpoint_dialog(struct world *mzx_world,
 struct break_point *br, const char *title)
{
  int result;
  int num_elements = 4;
  struct element *elements[num_elements];
  struct dialog di;

  elements[0] = construct_input_box(2, 2, "Match robot name (optional):", 14, 0, br->match_name);
  elements[1] = construct_input_box(2, 3, "Match text:", 60, 0, br->match_string);
  elements[2] = construct_button(9, 5, "Confirm", 0);
  elements[3] = construct_button(23, 5, "Cancel", -1);
  
  construct_dialog_ext(&di, title, 0, 6,
   80, 8, elements, num_elements, 0, 0, 0, NULL);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  return result;
}

static int edit_breakpoints_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  switch(key)
  {
    // Add 
    case IKEY_a:
    {
      if(get_alt_status(keycode_internal))
      {
        di->return_value = 1;
        di->done = 1;
        return 0;
      }
    }
    // Delete selection
    case IKEY_d:
    {
      if(get_alt_status(keycode_internal))
      {
        di->return_value = 3;
        di->done = 1;
        return 0;
      }
    }
    // Edit selection
    case IKEY_e:
    {
      if(get_alt_status(keycode_internal))
      {
        di->return_value = 2;
        di->done = 1;
        return 0;
      }
    }
  }

  return key;
}

void __edit_breakpoints(struct world *mzx_world)
{
  int i;

  int result = 0;
  int num_elements = 8;
  struct element *elements[num_elements];
  struct dialog di;

  int selected = 0;
  int focus = 0;

  const char *enable_text[] = {
   "Enable Robot Debugger ",
   "Disable Robot Debugger",
  };

  m_show();

  do {
    int list_size = num_break_points;
    char **list = ccalloc(list_size, sizeof(char *));
    for(i = 0; i < list_size; i++)
    {
      list[i] = cmalloc(77);
      memset(list[i], ' ', 76);
      list[i][76] = 0;

      strncpy(list[i], break_points[i]->match_name, strlen(break_points[i]->match_name));
      strcpy(list[i] + 15, break_points[i]->match_string);
    }

    elements[0] = construct_list_box(2, 2, (const char **)list, num_break_points,
     19, 76, 0, &selected, false);
    elements[1] = construct_button(4, 22, "Add", 1);
    elements[2] = construct_button(11, 22, "Edit", 2);
    elements[3] = construct_button(19, 22, "Delete", 3);
    elements[4] = construct_button(37, 22, enable_text[robo_debugger_enabled], 4);
    elements[5] = construct_button(70, 22, "Done", -1);
    elements[6] = construct_label(2, 1, "Name substr.");
    elements[7] = construct_label(17, 1, "Line substring");

    construct_dialog_ext(&di, "Edit Breakpoints", 0, 0, 80, 25, elements, num_elements, 0, 0, focus,
     edit_breakpoints_idle_function);

    result = run_dialog(mzx_world, &di);

    switch(result)
    {
      // Add
      case 1:
      {
        struct break_point *temp = ccalloc(sizeof(struct break_point), 1);

        if(!edit_breakpoint_dialog(mzx_world, temp, "Add Breakpoint"))
        {
          if(num_break_points == num_break_points_allocated)
          {
            num_break_points_allocated = MAX(32, num_break_points_allocated * 2);
            break_points = crealloc(break_points,
             num_break_points_allocated * sizeof(struct break_point *));
          }

          boyer_moore_index(temp->match_string, strlen(temp->match_string), temp->index, false);

          break_points[num_break_points] = temp;
          num_break_points++;

          if(!robo_debugger_override)
            robo_debugger_enabled = 1;
        }
        else
          free(temp);

        break;
      }
      // Edit
      case 0:
      case 2:
      {
        if(selected < num_break_points)
        {
          struct break_point *temp = break_points[selected];

          if(!edit_breakpoint_dialog(mzx_world, temp, "Edit Breakpoint"))
            boyer_moore_index(temp->match_string, strlen(temp->match_string), temp->index, false);
        }
        break;
      }
      // Delete
      case 3:
      {
        if(selected < num_break_points)
        {
          if(!confirm(mzx_world, "Delete breakpoint?"))
          {
            free(break_points[selected]);
            memmove(break_points + selected, break_points + selected + 1,
             (num_break_points - selected - 1) * sizeof(struct break_point *));
            num_break_points--;
            if(selected == num_break_points && selected > 0)
              selected--;
          }
        }
        break;
      }
      // Enable/Disable
      case 4:
      {
        robo_debugger_enabled ^= 1;
        robo_debugger_override = 1;
        break;
      }
    }

    destruct_dialog(&di);
    for(i = 0; i < list_size; i++)
      free(list[i]);
    free(list);

  } while (result > -1);

  m_hide();
}

// Turn off the debugger without clearing the breakpoints
// For now we'll just have this disable the step, since starting
// with the debugger on may have very useful applications
void pause_robot_debugger(void)
{
  //robo_debugger_enabled = 0;
  step = false;
}

// Called when the editor exits
void free_breakpoints(void)
{
  for(int i = 0; i < num_break_points; i++)
    free(break_points[i]);

  if(break_points)
    free(break_points);

  break_points = NULL;
  num_break_points = 0;
  num_break_points_allocated = 0;

  robo_debugger_enabled = 0;
  robo_debugger_override = 0;
  step = false;
}

/********************/
/* Robotic debugger */
/********************/

static int debug_robot_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  switch(key)
  {
    case IKEY_c:
    {
      di->return_value = -1;
      di->done = 1;
      return 0;
    }
    case IKEY_n:
    {
      di->return_value = 0;
      di->done = 1;
      return 0;
    }
    case IKEY_s:
    {
      if(get_alt_status(keycode_internal))
        di->return_value = 2;
      else
        di->return_value = 1;
      di->done = 1;
      return 0;
    }
    case IKEY_F11:
    {
      if(get_alt_status(keycode_internal))
        di->return_value = 4;
      else
        di->return_value = 3;
      di->done = 1;
      return 0;
    }
  }
  return key;
}

// FIXME: make it NOT come here every command while editing, maybe. Just a thought
// If the return value != 0, the robot ignores the current command and ends
int __debug_robot(struct world *mzx_world, struct robot *cur_robot, int id)
{
  int i;
  bool done = false;
  bool stop_robot = false;

  char *program;
  char *cmd_ptr;
  char *cmd_text;
  int line_num;
  int size = 0;

  if(!robo_debugger_enabled)
    return 0;

  program = cur_robot->program_bytecode;
  cmd_ptr = program + cur_robot->cur_prog_line;

#ifdef CONFIG_DEBYTECODE
  {
    confirm(mzx_world, "Robotic debugger not yet supported by debytecode");
    robo_debugger_enabled = 0;
    return 0;
  }
#else //!CONFIG_DEBYTECODE
  {
    char *c;
    char *next;
    int print_ignores = 1, base = 10;
    cmd_text = cmalloc(256);
    cmd_text[255] = 0;

    disassemble_line(cmd_ptr, &next, cmd_text, NULL, &size, print_ignores, NULL, NULL, base);

    if(!step)
    {
      bool match = false;
      for(i = 0; i < num_break_points; i++)
      {
        struct break_point *b = break_points[i];
        if(strlen(b->match_name) && !strstr(cur_robot->robot_name, b->match_name))
          continue;

        if(!boyer_moore_search((void *)cmd_text, strlen(cmd_text),
         (void *)b->match_string, strlen(b->match_string), b->index, false))
          continue;

        match = true;
        break;
      }

      if(!match)
        return 0;
    }

    // figure out our line number
    line_num = 1;
    c = program + 1;
    while(c < cmd_ptr)
    {
      c += *c + 2;
      line_num++;
    }
  }
#endif

  // Open debug dialog
  do {
    int dialog_result;
    int num_elements = 11;
    struct element *elements[num_elements];
    struct dialog di;

    char info[76] = { 0 };
    char label[4][76] = { { 0 } };
    char *cmd_pos = cmd_text;

    snprintf(info, 75, "Matched robot `%s` (#%i) at line %i (size %i):",
     cur_robot->robot_name, id, line_num, size);
    info[75] = 0;
    for(i = 0; cmd_pos < cmd_text + strlen(cmd_text) && i < 4; i++)
    {
      strncpy(label[i], cmd_pos, MIN(strlen(cmd_pos) + 1, 75));
      label[i][75] = 0;
      cmd_pos += 75;
    }

    elements[0]  = construct_button(3,  7, "Continue", -1);
    elements[1]  = construct_button(15, 7, "Next", 0);
    elements[2]  = construct_button(23, 7, "Stop", 1);
    elements[3]  = construct_button(31, 7, "Stop all", 2);
    elements[4]  = construct_button(52, 7, "Counters", 3);
    elements[5]  = construct_button(64, 7, "Breakpoints", 4);
    elements[6]  = construct_label(2, 1, info);
    elements[7]  = construct_label(2, 2, label[0]);
    elements[8]  = construct_label(2, 3, label[1]);
    elements[9]  = construct_label(2, 4, label[2]);
    elements[10] = construct_label(2, 5, label[3]);

    construct_dialog_ext(&di, "Robot Debugger", 0, 6,
     80, 10, elements, num_elements, 0, 0, selected,
     debug_robot_idle_function);

    m_show();
    dialog_result = run_dialog(mzx_world, &di);
    m_hide();

    destruct_dialog(&di);

    selected = dialog_result + 1;

    switch(dialog_result)
    {
      // Escape/Continue
      case -1:
      {
        step = false;
        done = true;
        break;
      }
      // Step
      case 0:
      {
        step = true;
        done = true;
        break;
      }
      // Stop all
      case 1:
      {
        for(i = 0; i < mzx_world->current_board->num_robots; i++)
        {
          mzx_world->current_board->robot_list[i]->status = 1;
          mzx_world->current_board->robot_list[i]->cur_prog_line = 0;
        }
      }
      // Stop
      case 2:
      {
        stop_robot = true;
        step = false;
        done = true;
        break;
      }
      // Counter debugger
      case 3:
      {
        __debug_counters(mzx_world);
        break;
      }
      // Edit breakpoints
      case 4:
      {
        __edit_breakpoints(mzx_world);
        break;
      }
    }
  } while(!done);

  free(cmd_text);

  return stop_robot;
}