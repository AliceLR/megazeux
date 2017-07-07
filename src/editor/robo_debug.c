/* MegaZeux
 *
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "robo_debug.h"

#include "debug.h"
#include "robot.h"
#include "window.h"

#include "../counter.h"
#include "../event.h"
#include "../graphics.h"
#include "../robot.h"
#include "../window.h"
#include "../util.h"

#include "../world_struct.h"
#include "../board_struct.h"
#include "../robot_struct.h"


struct breakpoint
{
  char match_name[15];
  char match_string[61];
  int index[256];
  int line_number;
};

struct watchpoint
{
  char match_name[61];
  int last_value;
};

static int num_breakpoints = 0;
static int num_breakpoints_allocated = 0;
static struct breakpoint **breakpoints = NULL;

static int num_watchpoints = 0;
static int num_watchpoints_allocated = 0;
static struct watchpoint **watchpoints = NULL;

// Whether or not the robot debugger is enabled
static int robo_debugger_enabled = 0;

// Whether or not the user has explicitly enabled/disabled it yet
static int robo_debugger_override = 0;

// Whether or not the debugger is stepping through code
static bool step = false;

// Last positions selected
static int br_selected = 0;
static int wt_selected = 0;

// The last debugger option chosen
static int selected = 0;

// The debugger's position on-screen (1=bottom)
static int position = 1;


static inline int  hash_string(char *data, size_t length)
{
  unsigned int i;
  int x = *data;

  for(i = 1; i < length; i++)
  {
    data++;
    x ^= (x << 5) + (x >> 2) + *data;
  }

  return x;
}

static inline int get_watchpoint_value(struct world *mzx_world,
 struct watchpoint *wt)
{
  if(is_string(wt->match_name))
  {
    struct string src_string;

    if(get_string(mzx_world, wt->match_name, &src_string, -1))
    {
      return hash_string(src_string.value, src_string.length);
    }
  }

  else
  {
    return get_counter_safe(mzx_world, wt->match_name, -1);
  }

  return 0;
}

void update_watchpoint_last_values(struct world *mzx_world)
{
  struct watchpoint *wt;
  int i;

  for(i = 0; i < num_watchpoints; i++)
  {
    wt = watchpoints[i];
    wt->last_value = get_watchpoint_value(mzx_world, wt);
  }
}


/*********************/
/* Breakpoint editor */
/*********************/

static int edit_breakpoint_dialog(struct world *mzx_world,
 struct breakpoint *br, const char *title)
{
  char match_string[61];
  char match_name[15];
  int line_number;

  struct element *elements[5];
  struct dialog di;
  int result;

  memcpy(match_string, br->match_string, 61);
  memcpy(match_name, br->match_name, 15);
  line_number = br->line_number;

  elements[0] = construct_input_box(2, 1,
   "Match text:", 60, 0, match_string);

  elements[1] = construct_input_box(2, 3,
   "Match robot name:", 14, 0, match_name);

  elements[2] = construct_number_box(38, 3,
   "Match line number:", 0, 99999, 0, &line_number);

  elements[3] = construct_button(22, 5, "Confirm", 0);
  elements[4] = construct_button(45, 5, "Cancel", -1);
  
  construct_dialog(&di, title, 2, 7, 76, 7, elements, ARRAY_SIZE(elements), 0);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(!result)
  {
    memcpy(br->match_string, match_string, 61);
    memcpy(br->match_name, match_name, 15);
    br->line_number = line_number;
  }

  return result;
}

static int edit_watchpoint_dialog(struct world *mzx_world,
 struct watchpoint *wt, const char *title)
{
  char match_name[61];

  struct element *elements[3];
  struct dialog di;
  int result;

  memcpy(match_name, wt->match_name, 61);

  elements[0] = construct_input_box(3, 1,
   "Variable:", 60, 0, match_name);

  elements[1] = construct_button(22, 3, "Confirm", 0);
  elements[2] = construct_button(45, 3, "Cancel", -1);

  construct_dialog(&di, title, 2, 8, 76, 5, elements, ARRAY_SIZE(elements), 0);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(!result)
  {
    memcpy(wt->match_name, match_name, 61);
  }

  return result;
}

static void new_breakpoint(struct world *mzx_world)
{
  struct breakpoint *br = ccalloc(sizeof(struct breakpoint), 1);

  if(!edit_breakpoint_dialog(mzx_world, br, "New Breakpoint"))
  {
    if(num_breakpoints == num_breakpoints_allocated)
    {
      num_breakpoints_allocated = MAX(32, num_breakpoints_allocated * 2);
      breakpoints = crealloc(breakpoints,
       num_breakpoints_allocated * sizeof(struct breakpoint *));
    }

    boyer_moore_index(br->match_string, strlen(br->match_string),
     br->index, true);

    breakpoints[num_breakpoints] = br;
    num_breakpoints++;

    if(!robo_debugger_override)
      robo_debugger_enabled = 1;
  }

  else
  {
    free(br);
  }
}

static void new_watchpoint(struct world *mzx_world)
{
  struct watchpoint *wt = ccalloc(sizeof(struct watchpoint), 1);

  if(!edit_watchpoint_dialog(mzx_world, wt, "New Watchpoint"))
  {
    if(num_watchpoints == num_watchpoints_allocated)
    {
      num_watchpoints_allocated = MAX(32, num_watchpoints_allocated * 2);
      watchpoints = crealloc(watchpoints,
       num_watchpoints_allocated * sizeof(struct watchpoint *));
    }

    watchpoints[num_watchpoints] = wt;
    num_watchpoints++;

    if(!robo_debugger_override)
      robo_debugger_enabled = 1;
  }

  else
  {
    free(wt);
  }
}

static int debug_config_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  switch(key)
  {
    // Add 
    case IKEY_a:
    case IKEY_n:
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
        di->return_value = 2;
        di->done = 1;
        return 0;
      }
    }
    // Edit selection
    case IKEY_e:
    {
      if(get_alt_status(keycode_internal))
      {
        di->return_value = 0;
        di->done = 1;
        return 0;
      }
    }
  }

  return key;
}

/* Results:
 * -1 : close
 *  0 : edit
 *  1 : add/new
 *  2 : delete
 *  3 : enable/disable debugger
 */

void __debug_robot_config(struct world *mzx_world)
{
  const char *enable_text[] = {
   "Enable Debugger ",
   "Disable Debugger",
  };

  struct breakpoint *br;
  struct watchpoint *wt;
  int i;

  int result = 0;
  struct element *elements[10];
  struct dialog di;

  int br_element = 6;
  int wt_element = 7;

  int focus = br_element;

  br_selected = MIN(br_selected, num_breakpoints);
  wt_selected = MIN(wt_selected, num_watchpoints);

  set_context(CTX_BREAKPOINT_EDITOR);

  m_show();

  do
  {
    char **br_list = ccalloc(num_breakpoints + 1, sizeof(char *));
    char **wt_list = ccalloc(num_watchpoints + 1, sizeof(char *));
    int br_list_size = num_breakpoints;
    int wt_list_size = num_watchpoints;

    for(i = 0; i < num_breakpoints; i++)
    {
      char *line = ccalloc(77, 1);
      memset(line, ' ', 60);

      br = breakpoints[i];

      memcpy(line, br->match_string, strlen(br->match_string));

      if(br->line_number)
      {
        sprintf(line + 53, "%d", br->line_number);
        line[strlen(line)] = ' ';
      }

      strcpy(line + 60, br->match_name);

      br_list[i] = line;
    }

    for(i = 0; i < num_watchpoints; i++)
    {
      char *line = ccalloc(77, 1);
      memset(line, ' ', 60);

      wt = watchpoints[i];

      memcpy(line, wt->match_name, strlen(wt->match_name));

      if(is_string(wt->match_name))
      {
        sprintf(line + 60, "<string>");
      }
      else
      {
        snprintf(line + 60, 12, "%d", wt->last_value);
      }

      wt_list[i] = line;
    }

    br_list[num_breakpoints] = (char *) "(new)";
    wt_list[num_watchpoints] = (char *) "(new)";

    elements[0] = construct_label(3, 23,
     "~9Alt+N : New   Enter : Edit   Alt+D : Delete");

    elements[1] = construct_label(2, 1, "Breakpoint substring");
    elements[2] = construct_label(55, 1, "Line");
    elements[3] = construct_label(62, 1, "Robot name");

    elements[4] = construct_label(2, 12, "Watchpoint variable");
    elements[5] = construct_label(62, 12, "Last value");

    elements[br_element] = construct_list_box(2, 2,
     (const char **)br_list, num_breakpoints + 1,
     9, 76, 0, &br_selected, NULL, false);

    elements[wt_element] = construct_list_box(2, 13,
     (const char **)wt_list, num_watchpoints + 1,
     9, 76, 0, &wt_selected, NULL, false);

    elements[8] = construct_button(50, 23,
     enable_text[robo_debugger_enabled], 3);

    elements[9] = construct_button(70, 23, "Done", -1);

    construct_dialog_ext(&di, "Configure Robot Debugger", 0, 0, 80, 25,
     elements, ARRAY_SIZE(elements), 0, 0, focus, debug_config_idle_function);

    result = run_dialog(mzx_world, &di);
    focus = di.current_element;

    switch(result)
    {
      // Edit
      case 0:
      {
        if(focus == br_element)
        {
          if(br_selected < num_breakpoints)
          {
            br = breakpoints[br_selected];

            if(!edit_breakpoint_dialog(mzx_world, br, "Edit Breakpoint"))
            {
              boyer_moore_index(br->match_string, strlen(br->match_string),
               br->index, true);
            }
          }
          else

          if(br_selected == num_breakpoints)
          {
            new_breakpoint(mzx_world);
          }
        }
        else

        if(focus == wt_element)
        {
          if(wt_selected < num_watchpoints)
          {
            wt = watchpoints[wt_selected];

            edit_watchpoint_dialog(mzx_world, wt, "Edit Watchpoint");
          }
          else

          if(wt_selected == num_watchpoints)
          {
            new_watchpoint(mzx_world);
          }
        }
        break;
      }

      // Add
      case 1:
      {
        if(focus == br_element)
        {
          new_breakpoint(mzx_world);
        }
        else

        if(focus == wt_element)
        {
          new_watchpoint(mzx_world);
        }
        break;
      }

      // Delete
      case 2:
      {
        if(focus == br_element && br_selected < num_breakpoints)
        {
          if(!confirm(mzx_world, "Delete breakpoint?"))
          {
            free(breakpoints[br_selected]);
            memmove(breakpoints + br_selected, breakpoints + br_selected + 1,
             (num_breakpoints - br_selected - 1) * sizeof(struct breakpoint *));

            num_breakpoints--;
            if(br_selected == num_breakpoints && br_selected > 0)
              br_selected--;
          }
        }
        else

        if(focus == wt_element && wt_selected < num_watchpoints)
        {
          if(!confirm(mzx_world, "Delete watchpoint?"))
          {
            free(watchpoints[wt_selected]);
            memmove(watchpoints + wt_selected, watchpoints + wt_selected + 1,
             (num_watchpoints - wt_selected - 1) * sizeof(struct watchpoint *));

            num_watchpoints--;
            if(wt_selected == num_watchpoints && wt_selected > 0)
              wt_selected--;
          }
        }
        break;
      }

      // Enable/Disable
      case 3:
      {
        robo_debugger_enabled ^= 1;
        robo_debugger_override = 1;
        break;
      }
    }

    destruct_dialog(&di);

    update_watchpoint_last_values(mzx_world);

    for(i = 0; i < br_list_size; i++)
      free(br_list[i]);

    for(i = 0; i < wt_list_size; i++)
      free(wt_list[i]);

    free(br_list);
    free(wt_list);
  }
  while(result > -1);

  m_hide();
  pop_context();
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
  int i;

  if(breakpoints)
    for(i = 0; i < num_breakpoints; i++)
      free(breakpoints[i]);

  if(watchpoints)
    for(i = 0; i < num_watchpoints; i++)
      free(watchpoints[i]);

  free(breakpoints);
  free(watchpoints);

  breakpoints = NULL;
  num_breakpoints = 0;
  num_breakpoints_allocated = 0;

  watchpoints = NULL;
  num_watchpoints = 0;
  num_watchpoints_allocated = 0;

  robo_debugger_enabled = 0;
  robo_debugger_override = 0;
  step = false;
}


/********************/
/* Robotic debugger */
/********************/

static int goto_send_dialog(struct world *mzx_world, int robot_id)
{
  const char *target_strs[] = {
    "Goto",
    "Send",
    "Send all",
  };

  const char *ignore_strs[] = {
    "Send/send all ignores locked status?"
  };

  int target = 1;
  int ignore[] = { 0 };

  char name_in[52] = { 0 };
  char label_in[52] = { 0 };

  char *name_tr;
  char *label_tr;

  struct element *elements[6];
  struct dialog di;
  int result;

  elements[0] = construct_radio_button(2, 1,
   target_strs, ARRAY_SIZE(target_strs), 8, &target);

  elements[1] = construct_input_box(17, 1,
   "Name:", 51, 0, name_in);

  elements[2] = construct_input_box(16, 2,
   "Label:", 51, 0, label_in);

  elements[3] = construct_check_box(21, 3,
   ignore_strs, ARRAY_SIZE(ignore_strs), 50, ignore);

  elements[4] = construct_button(26, 4, "Confirm", 0);
  elements[5] = construct_button(42, 4, "Cancel", -1);

  construct_dialog(&di, "Goto/Send", 2, 9, 76, 6,
   elements, ARRAY_SIZE(elements), 0);

  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(!result)
  {
    label_tr = cmalloc(ROBOT_MAX_TR);
    tr_msg(mzx_world, label_in, robot_id, label_tr);

    switch(target)
    {
      // Goto
      case 0:
      {
        send_robot_id(mzx_world, robot_id, label_tr, 1);
        break;
      }

      // Send
      case 1:
      {
        name_tr = cmalloc(ROBOT_MAX_TR);
        tr_msg(mzx_world, name_in, robot_id, name_tr);

        send_robot(mzx_world, name_tr, label_tr, ignore[0]);

        free(name_tr);
        break;
      }

      // Send all
      case 2:
      {
        sprintf(name_in, "all");
        send_robot(mzx_world, name_in, label_tr, ignore[0]);
        break;
      }
    }

    free(label_tr);
  }

  return result;
}

enum operation {
  OP_DO_NOTHING = -2,
  OP_CONTINUE   = -1,
  OP_STEP       =  0,
  OP_GOTO       =  1,
  OP_HALT       =  2,
  OP_HALT_ALL   =  3,
  OP_COUNTERS   =  4,
  OP_CONFIG     =  5,
};

static int debug_robot_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  // For most of these, do nothing if the key is being held.
  int is_press = (get_key_status(keycode_internal_wrt_numlock, key) == 1);

  switch(key)
  {
    case IKEY_c:
    {
      if(!is_press)
        return 0;

      di->return_value = OP_CONTINUE;
      di->done = 1;
      break;
    }
    case IKEY_s:
    {
      if(!is_press)
        return 0;

      di->return_value = OP_STEP;
      di->done = 1;
      break;
    }
    case IKEY_g:
    {
      if(!is_press)
        return 0;

      di->return_value = OP_GOTO;
      di->done = 1;
      break;
    }
    case IKEY_h:
    {
      if(!is_press)
        return 0;

      if(get_alt_status(keycode_internal))
        di->return_value = OP_HALT_ALL;
      else
        di->return_value = OP_HALT;
      di->done = 1;
      break;
    }
    case IKEY_F11:
    {
      if(!is_press)
        return 0;

      if(get_alt_status(keycode_internal))
        di->return_value = OP_CONFIG;
      else
        di->return_value = OP_COUNTERS;
      di->done = 1;
      return 0;
    }
    case IKEY_PAGEUP:
    {
      if(!is_press)
        return 0;

      position = 0;
      di->return_value = OP_DO_NOTHING;
      di->done = 1;
      break;
    }
    case IKEY_PAGEDOWN:
    {
      if(!is_press)
        return 0;

      position = 1;
      di->return_value = OP_DO_NOTHING;
      di->done = 1;
      break;
    }
  }

  if(di->done)
    return 0;

  return key;
}

enum actions {
  ACTION_STEPPED,
  ACTION_MATCHED,
  ACTION_CAUGHT,
  ACTION_CAUGHT_AUTO,
  ACTION_WATCH,
};

// Don't exceed 8 chars.
static const char *action_strings[] = {
  "next:",
  "matched",
  "caught",
  "enabled;",
  "watch:",
};

static void debug_robot_title(char buffer[77], struct robot *cur_robot, int id,
 int action, int line_number)
{
  snprintf(buffer, 76, "Robot Debugger - %s `%s` (%i@%i,%i) at line %i:",
   action_strings[action],
   cur_robot->robot_name, id, cur_robot->xpos, cur_robot->ypos, line_number);

  buffer[76] = 0;
}

// If the return value != 0, the robot ignores the current command and ends
static int debug_robot(struct world *mzx_world, struct robot *cur_robot, int id,
 char title[77], char info[77], char *src_ptr, int src_length, int lines_run)
{
  // TODO: when limitations on command length are lifted,
  // this needs to be extended.
  char buffer[ROBOT_MAX_TR] = { 0 };
  int buffer_len = ARRAY_SIZE(buffer);

  char *buffer_pos;
  int buffer_left;

  char *src_end = src_ptr + src_length;
  char *src_pos;

  struct dialog di;
  struct element *elements[8];
  int num_elements = ARRAY_SIZE(elements);

  int dialog_result;
  int dialog_y = 0;

  int ret_val = 0;
  int height;
  int ypos;
  int len;

  char t;
  int i;

  // Build the code/info text display
  src_pos = src_ptr;
  buffer_pos = buffer;
  buffer_left = buffer_len - 1;
  ypos = 0;

  if(info && info[0])
  {
    strncpy(buffer_pos, info, 76);
    buffer_pos[76] = 0;

    len = strlen(buffer_pos);
    buffer_pos[len++] = '\n';
    buffer_pos[len++] = '\n';
    buffer_pos[len] = 0;

    buffer_pos += len;
    buffer_left -= len;
    ypos += 2;
  }

  strcpy(buffer_pos, "~b");
  len = strlen(buffer_pos);
  buffer_pos += len;
  buffer_left -= len;

  while(buffer_left && src_pos < src_end)
  {
    t = *src_pos;
    src_pos++;

    switch(t)
    {
      case '~':
      {
        if(buffer_left > 1)
        {
          *buffer_pos = '~';
          buffer_pos++;
          buffer_left--;
        }
        break;
      }

      case '@':
      {
        if(buffer_left > 1)
        {
          *buffer_pos = '@';
          buffer_pos++;
          buffer_left--;
        }
        break;
      }

      case '\n':
        ypos++;
    }

    *buffer_pos = t;
    buffer_pos++;
    buffer_left--;
  }

  *buffer_pos = 0;
  ypos += 3;

  height = ypos + 2;

  // Keep running track of the last position we were caught at.
  cur_robot->commands_caught = lines_run;

  // Update these temporarily for the counter debugger.
  cur_robot->commands_total += lines_run;
  cur_robot->commands_cycle = lines_run;

  dialog_fadein();

  set_context(CTX_ROBOT_DEBUG);

  // Open debug dialog
  do
  {
    if(position)
      dialog_y = 25 - height;

    elements[0]  = construct_button( 3, ypos, "Continue", OP_CONTINUE);
    elements[1]  = construct_button(15, ypos, "Step", OP_STEP);
    elements[2]  = construct_button(23, ypos, "Goto", OP_GOTO);
    elements[3]  = construct_button(31, ypos, "Halt", OP_HALT);
    elements[4]  = construct_button(39, ypos, "Halt all", OP_HALT_ALL);
    elements[5]  = construct_button(57, ypos, "Counters", OP_COUNTERS);
    elements[6]  = construct_button(69, ypos, "Config", OP_CONFIG);
    elements[7]  = construct_label(2, 1, buffer);

    construct_dialog_ext(&di, title, 0, dialog_y, 80, height,
     elements, num_elements, 0, 0, selected, debug_robot_idle_function);

    m_show();
    dialog_result = run_dialog(mzx_world, &di);

    if(dialog_result > OP_DO_NOTHING)
      selected = dialog_result + 1;

    switch(dialog_result)
    {
      case OP_DO_NOTHING:
      {
        break;
      }

      case OP_CONTINUE:
      {
        ret_val = DEBUG_EXIT;
        step = false;
        break;
      }

      case OP_STEP:
      {
        ret_val = DEBUG_EXIT;
        step = true;
        break;
      }

      case OP_GOTO:
      {
        if(!goto_send_dialog(mzx_world, id))
        {
          ret_val = DEBUG_GOTO;
          step = true;
        }
        break;
      }

      case OP_HALT_ALL:
      {
        struct board *cur_board = mzx_world->current_board;
        // We pretty much need to simulate exactly what happens when
        // a robot hits the end command, here.
        for(i = 0; i < cur_board->num_robots + 1; i++)
        {
          struct robot *r = cur_board->robot_list[i];
          if(r)
          {
            r->status = 1;
            r->walk_dir = 0;
            r->cur_prog_line = 0;
            r->pos_within_line = 0;
          }
        }
        // continue to OP_HALT
      }

      case OP_HALT:
      {
        cur_robot->status = 1;
        cur_robot->walk_dir = 0;
        cur_robot->cur_prog_line = 0;
        cur_robot->pos_within_line = 0;

        ret_val = DEBUG_HALT;
        step = false;
        break;
      }

      case OP_COUNTERS:
      {
        __debug_counters(mzx_world);
        update_screen();
        break;
      }

      case OP_CONFIG:
      {
        __debug_robot_config(mzx_world);
        update_screen();
        break;
      }
    }

    destruct_dialog(&di);
  }
  while(!ret_val);

  pop_context();

  m_hide();
  dialog_fadeout();

  // These aren't final yet, so change them back.
  cur_robot->commands_total -= lines_run;
  cur_robot->commands_cycle = 0;

  update_event_status();

  return ret_val;
}

static inline void get_src_line(struct robot *cur_robot, char **_src_ptr,
 int *_src_length, int *_real_line_num)
{
  struct command_mapping *cmd_map = cur_robot->command_map;
  int line_num = get_current_program_line(cur_robot);

  char *src_ptr;
  int src_length;
  int offset;

  if(cmd_map)
  {
    offset = cmd_map[line_num].src_pos;
    src_ptr = cur_robot->program_source + offset;

    if(line_num < cur_robot->command_map_length)
    {
      src_length = cmd_map[line_num + 1].src_pos - offset - 1;
    }
    else
    {
      src_length = strlen(src_ptr);
    }

    // Remove any trailing newlines that might have made it in
    while(src_length > 0 && isspace(src_ptr[src_length - 1]))
      src_length--;

    *_src_ptr = src_ptr;
    *_src_length = src_length;
    *_real_line_num = cmd_map[line_num].real_line;
  }

  else
  {
    *_src_ptr = (char *) "<no source available>";
    *_src_length = strlen(*_src_ptr);
    *_real_line_num = line_num;
  }
}

int __debug_robot_break(struct world *mzx_world, struct robot *cur_robot,
 int id, int lines_run)
{
  char title[77];
  char info[77];

  int action = ACTION_STEPPED;
  int line_number = 0;
  int src_length = 0;
  char *src_ptr = NULL;

  int i;

  info[0] = 0;

  // Enable and trigger a break if a robot seems to be out of control.
  if(lines_run - cur_robot->commands_caught >= mzx_world->commands_stop)
  {
    if(!robo_debugger_enabled)
    {
      action = ACTION_CAUGHT_AUTO;
    }

    else
    {
      action = ACTION_CAUGHT;
    }

    sprintf(info, "~9Commands run exceeded ~c`COMMANDS_STOP`~9 value ~a%d~9.",
     mzx_world->commands_stop);

    robo_debugger_enabled = 1;
    step = 1;
  }

  if(!robo_debugger_enabled)
    return 0;

#ifndef CONFIG_DEBYTECODE
  // Make sure source exists for debugging.
  prepare_robot_source(cur_robot);
#endif

  // Get the current line.
  get_src_line(cur_robot, &src_ptr, &src_length, &line_number);

  if(cur_robot->program_source)
  {
    // Attempt to match a breakpoint
    if(!step)
    {
      struct breakpoint *b;
      size_t match_string_len;
      size_t match_name_len;
      int match_line;

      bool match = false;

      for(i = 0; i < num_breakpoints; i++)
      {
        b = breakpoints[i];
        match_string_len = strlen(b->match_string);
        match_name_len = strlen(b->match_name);
        match_line = b->line_number;

        // Make sure the line number is correct
        if(match_line && match_line != line_number)
          continue;

        // Make sure the robot name is correct
        if(match_name_len &&
         strcasecmp(cur_robot->robot_name, b->match_name))
          continue;

        // Try to find the match pattern in the line
        if(match_string_len && src_length)
          if(!boyer_moore_search((void *)src_ptr, src_length,
           (void *)b->match_string, match_string_len, b->index, true))
            continue;

        // Verify a meaningful match has actually occured
        if((!match_string_len || !src_length) &&
         !match_name_len && !match_line)
          continue;

        action = ACTION_MATCHED;
        match = true;
        break;
      }

      if(!match)
        return 0;
    }
  }

  else
  {
    // If there isn't any source information available, we can't do much.
    if(!step)
      return 0;
  }

  // Run the robot debugger
  debug_robot_title(title, cur_robot, id, action, line_number);
  return debug_robot(mzx_world, cur_robot, id, title, info, src_ptr, src_length,
   lines_run);
}

int __debug_robot_watch(struct world *mzx_world, struct robot *cur_robot,
 int id, int lines_run)
{
  char title[77];
  char info[77];

  int line_number = 0;
  int src_length = 0;
  char *src_ptr = NULL;

  struct watchpoint *wt;
  bool match = false;
  int value = 0;
  int i;

  if(!robo_debugger_enabled)
    return 0;

  info[0] = 0;

  for(i = 0; i < num_watchpoints; i++)
  {
    wt = watchpoints[i];

    value = get_watchpoint_value(mzx_world, wt);

    // String
    if(is_string(wt->match_name))
    {
      snprintf(info, 76, "~a@0 changed ~9@1: watch ~c`%s`", wt->match_name);
      info[76] = 0;
    }

    // Counter
    else
    {
      snprintf(info, 76, "~a@0 %d ~8\x1A ~a%d ~9@1: watch ~c`%s`",
       wt->last_value, value, wt->match_name);

      info[76] = 0;
    }

    if(value != wt->last_value)
    {
      wt->last_value = value;
      match = true;
      break;
    }
  }

  if(!match)
    return 0;

  // Get the current line.
  get_src_line(cur_robot, &src_ptr, &src_length, &line_number);

  // Run the robot debugger
  debug_robot_title(title, cur_robot, id, ACTION_WATCH, line_number);
  return debug_robot(mzx_world, cur_robot, id, title, info, src_ptr, src_length,
   lines_run);
}
