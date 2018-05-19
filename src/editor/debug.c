/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2012 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "debug.h"

#include "char_ed.h"
#include "pal_ed.h"
#include "robo_debug.h"
#include "window.h"

#include "../audio.h"
#include "../counter.h"
#include "../event.h"
#include "../graphics.h"
#include "../intake.h"
#include "../sprite.h"
#include "../str.h"
#include "../util.h"
#include "../window.h"
#include "../world.h"

#include <string.h>

#define TREE_LIST_X 62
#define TREE_LIST_Y 2
#define TREE_LIST_WIDTH 16
#define TREE_LIST_HEIGHT 15
#define VAR_LIST_X 2
#define VAR_LIST_Y 2
#define VAR_LIST_WIDTH 58
#define VAR_LIST_HEIGHT 21
#define VAR_LIST_VAR 60
#define BUTTONS_X 62
#define BUTTONS_Y 18
#define CVALUE_SIZE 11
#define SVALUE_SIZE 40
#define CVALUE_COL_OFFSET (VAR_LIST_WIDTH - CVALUE_SIZE - 1)
#define SVALUE_COL_OFFSET (VAR_LIST_WIDTH - SVALUE_SIZE - 1)

#define VAR_SEARCH_DIALOG_X 10
#define VAR_SEARCH_DIALOG_Y 9
#define VAR_SEARCH_DIALOG_W 60
#define VAR_SEARCH_DIALOG_H 5
#define VAR_SEARCH_MAX 47
#define VAR_SEARCH_NAMES    1
#define VAR_SEARCH_VALUES   2
#define VAR_SEARCH_CASESENS 4
#define VAR_SEARCH_REVERSE  8
#define VAR_SEARCH_WRAP     16
#define VAR_SEARCH_LOCAL    32
#define VAR_SEARCH_EXACT    64

#define VAR_ADD_DIALOG_X 20
#define VAR_ADD_DIALOG_Y 8
#define VAR_ADD_DIALOG_W 40
#define VAR_ADD_DIALOG_H 6
#define VAR_ADD_MAX 30

char asc[17] = "0123456789ABCDEF";

static void copy_substring_escaped(struct string *str, char *buf,
 unsigned int size)
{
  unsigned int i, j, left;

  for(i = 0, j = 0; j < str->length && i < size; i++, j++)
  {
    left = size - i;
    if(str->value[j] == '\\')
    {
      if(left < 3)
        break;

      buf[i++] = '\\';
      buf[i] = '\\';
    }
    else if(str->value[j] == '\n')
    {
      if(left < 3)
        break;

      buf[i++] = '\\';
      buf[i] = 'n';
    }
    else if(str->value[j] == '\r')
    {
      if(left < 3)
        break;

      buf[i++] = '\\';
      buf[i] = 'r';
    }
    else if(str->value[j] == '\t')
    {
      if(left < 3)
        break;

      buf[i++] = '\\';
      buf[i] = 't';
    }
    else if(str->value[j] < 32 || str->value[j] > 126)
    {
      if(left < 5)
        break;

      buf[i++] = '\\';
      buf[i++] = 'x';
      buf[i++] = asc[str->value[j] >> 4];
      buf[i] = asc[str->value[j] & 15];
    }
    else
      buf[i] = str->value[j];
  }

  buf[i] = 0;
}

static void unescape_string(char *buf, int *len)
{
  size_t i = 0, j, old_len = strlen(buf);
  char t;

  for(j = 0; j < old_len; i++, j++)
  {
    if(buf[j] != '\\')
    {
      buf[i] = buf[j];
      continue;
    }

    j++;

    if(j == old_len)
      break;

    if(buf[j] == 'n')
      buf[i] = '\n';
    else if(buf[j] == 'r')
      buf[i] = '\r';
    else if(buf[j] == 't')
      buf[i] = '\t';
    else if(buf[j] == '\\')
      buf[i] = '\\';
    else if(buf[j] == 'x')
    {
      if(j + 2 > old_len)
        break;

      t = buf[j + 3];
      buf[j + 3] = '\0';
      buf[i] = (char)strtol(buf + j + 1, NULL, 16);
      buf[j + 3] = t;
      j += 2;
    }
    else
      buf[i] = buf[j];
  }

  (*len) = i;
}


/***********************
 * Var reading/setting *
 ***********************/

// We'll read off of these when we construct the tree
static const char *config_var_list[] = {
  "random_seed0",
  "random_seed1",
};

static const char *world_var_list[] = {
  "bimesg", //no read
  "c_divisions",
  "commands",
  "commands_stop",
  "divider",
  "fread_delimiter", //no read
  "fwrite_delimiter", //no read
  "max_samples",
  "mod_frequency",
  "mod_length*",
  "mod_name*",
  "mod_order",
  "mod_position",
  "multiplier",
  "mzx_speed",
  "smzx_message",
  "smzx_mode*",
  "spacelock", //no read
  "vlayer_size*",
  "vlayer_width*",
  "vlayer_height*",
};

static const char *board_var_list[] = {
  "board_name*",
  "board_mod*", //no read
  "board_w*",
  "board_h*",
  "input", // as a counter
  "input*", // as a string (no read function; handled in interpolation code)
  "inputsize",
  "key",
  "overlay_mode*",
  "playerfacedir",
  "playerlastdir",
  "playerx*",
  "playery*",
  "scrolledx*",
  "scrolledy*",
  "timereset",
  "volume", //no read or write
};

// Locals are added onto the end later.
static const char *robot_var_list[] = {
  "robot_name*",
  "commands_total", //no read or write
  "commands_cycle*", //no read
  "bullettype",
  "lava_walk",
  "goop_walk",
  "lockself", //no read or write
  "loopcount",
  "thisx*",
  "thisy*",
  "this_char*",
  "this_color*",
  "playerdist*",
  "horizpld*",
  "vertpld*",
};

// Sprite parent has yorder, collisions, and clist#
// The following will all be added to the end of 'sprN_'
static const char *sprite_var_list[] = {
  "x",
  "y",
  "refx",
  "refy",
  "width",
  "height",
  "cx",
  "cy",
  "cwidth",
  "cheight",
  "ccheck", // no read
  "off", // no read
  "offset",
  "overlay", // no read
  "static", // no read
  "tcol",
  "unbound",
  "vlayer", // no read
};

static int num_config_vars = ARRAY_SIZE(config_var_list);
static int num_world_vars = ARRAY_SIZE(world_var_list);
static int num_board_vars = ARRAY_SIZE(board_var_list);
static int num_robot_vars = ARRAY_SIZE(robot_var_list);
static int num_sprite_vars = ARRAY_SIZE(sprite_var_list);

#define match_counter(_name) (strlen(_name) == len && !strcasecmp(name, _name))

int get_counter_safe(struct world *mzx_world, const char *name, int id)
{
  // This is a safe wrapper for get_counter(). Some counters, when read,
  // could alter world data in a way we don't want to happen here.
  // So detect and block them.

  size_t len = strlen(name);

  if(match_counter("fread"))          return -1;
  if(match_counter("fread_counter"))  return -1;

  return get_counter(mzx_world, name, id);
}

#define match_var(_name) (!strcmp(var, _name))

static void read_var(struct world *mzx_world, char *var_buffer)
{
  struct board *cur_board = mzx_world->current_board;

  int int_value = 0;
  char *char_value = NULL;
  char buf[SVALUE_SIZE + 1] = { 0 };

  // Var info is stored after the visible portion of the buffer
  char *var = var_buffer + VAR_LIST_VAR;

  if(var[-2])
  {
    var[-1] = 1;

    if(var[0] == '$')
    {
      struct string temp;
      if(!get_string(mzx_world, var, &temp, 0))
        return;
      copy_substring_escaped(&temp, buf, SVALUE_SIZE);
      char_value = buf;
      int_value = SVALUE_SIZE;

      if(!temp.length)
        var[-1] = 0;
    }
    else
    {
      int_value = get_counter_safe(mzx_world, var, 0);
      if(int_value == 0)
        var[-1] = 0;
    }
  }

  // It's a built-in var.  Since some of these don't have
  // read functions, we have to map several manually.
  else
  {
    size_t len = strlen(var);
    char *real_var = cmalloc(len + 1);
    int index = var[-1];

    strncpy(real_var, var, len);
    real_var[len] = 0;

    if(real_var[len - 1] == '*')
      real_var[len - 1] = 0;

    if(match_var("bimesg"))
    {
      int_value = mzx_world->bi_mesg_status;
    }
    else

    if(match_var("spacelock"))
    {
      int_value = mzx_world->bi_shoot_status;
    }
    else

    if(match_var("fread_delimiter"))
    {
      int_value = mzx_world->fread_delimiter;
    }
    else

    if(match_var("fwrite_delimiter"))
    {
      int_value = mzx_world->fwrite_delimiter;
    }
    else

    if(match_var("mod_name*"))
    {
      char_value = mzx_world->real_mod_playing;
      int_value = strlen(char_value);
    }
    else

    if(match_var("board_name*"))
    {
      char_value = cur_board->board_name;
      int_value = strlen(char_value);
    }
    else

    if(match_var("board_mod*"))
    {
      char_value = cur_board->mod_playing;
      int_value = strlen(char_value);
    }
    else

    if(match_var("input*"))
    {
      // the starred version of input is the input string!
      char_value = cur_board->input_string;
      int_value = strlen(char_value);
    }
    else

    if(match_var("volume"))
    {
      int_value = cur_board->volume;
    }
    else

    if(match_var("robot_name*"))
    {
#ifdef CONFIG_DEBYTECODE
      // In debytecode, it is possible to encounter situations where
      // debug information is missing, so add a display for it.

      struct robot *cur_robot = cur_board->robot_list[index];
      const char *yn[] = { "N", "Y" };

      memset(buf, ' ', SVALUE_SIZE);
      snprintf(buf, strlen(cur_robot->robot_name), cur_robot->robot_name);
      sprintf(buf + SVALUE_SIZE - CVALUE_SIZE, "(debug: %s)",
       yn[cur_robot->command_map != NULL]);

      buf[SVALUE_SIZE] = 0;
      char_value = buf;
#else
      char_value = cur_board->robot_list[index]->robot_name;
#endif

      int_value = strlen(char_value);
    }
    else

    if(match_var("commands_total"))
    {
      int_value = cur_board->robot_list[index]->commands_total;
    }
    else

    if(match_var("commands_cycle*"))
    {
      int_value = cur_board->robot_list[index]->commands_cycle;
    }
    else

    if(match_var("lockself"))
    {
      int_value = cur_board->robot_list[index]->is_locked;
    }
    else

    if(!strncmp(var, "spr", 3))
    {
      int sprite_num = strtol(var + 3, &var, 10) & (MAX_SPRITES - 1);

      if(match_var("_off"))
      {
        if(!(mzx_world->sprite_list[sprite_num]->flags & SPRITE_INITIALIZED))
          int_value = 1;
      }
      else

      if(match_var("_overlay"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_OVER_OVERLAY)
          int_value = 1;
      }
      else

      if(match_var("_static"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_STATIC)
          int_value = 1;
      }
      else

      if(match_var("_vlayer"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_VLAYER)
          int_value = 1;
      }
      else

      if(match_var("_ccheck"))
      {
        int flags = mzx_world->sprite_list[sprite_num]->flags;
        int_value = 0;

        if(flags & SPRITE_CHAR_CHECK)
          int_value |= 1;

        if(flags & SPRITE_CHAR_CHECK2)
          int_value |= 2;
      }

      // Matched sprN but no var matched
      else
      {
        int_value = get_counter_safe(mzx_world, real_var, index);
      }
    }

    // No special var matched
    else
    {
      int_value = get_counter_safe(mzx_world, real_var, index);
    }

    free(real_var);
  }

  if(char_value)
  {
    memcpy(var_buffer + SVALUE_COL_OFFSET, char_value, MIN(SVALUE_SIZE, int_value));
    var_buffer[SVALUE_COL_OFFSET + MIN(SVALUE_SIZE, int_value)] = '\0';
  }

  else
  {
    snprintf(var_buffer + CVALUE_COL_OFFSET, CVALUE_SIZE, "%i", int_value);
  }

}

static void write_var(struct world *mzx_world, char *var_buffer, int int_val, char *char_val)
{
  char *var = var_buffer + VAR_LIST_VAR;

  if(var[-2])
  {
    if(var[0] == '$')
    {
      //set string -- int_val is the length here
      struct string temp;
      memset(&temp, '\0', sizeof(struct string));
      temp.length = int_val;
      temp.value = char_val;

      set_string(mzx_world, var, &temp, 0);
    }
    else
    {
      //set counter
      set_counter(mzx_world, var, int_val, 0);
    }
  }
  else
  {
    // It's a built-in var
    struct board *cur_board = mzx_world->current_board;
    int index = var[-1];

    if(var[strlen(var) - 1] != '*')
    {
      // Special cases: not an actual counter, but needs to be writable

      if(match_var("commands_total"))
      {
        cur_board->robot_list[index]->commands_total = int_val;
      }
      else

      if(match_var("volume"))
      {
        int_val = int_val & 255;

        cur_board->volume = int_val;
        cur_board->volume_target = int_val;

        volume_module(int_val);
      }
      else

      if(match_var("lockself"))
      {
        cur_board->robot_list[index]->is_locked = (int_val != 0);
      }

      // Everything else
      else
      {
        set_counter(mzx_world, var, int_val, index);
      }
    }
  }

  // Now update var_buffer to reflect the new value.
  read_var(mzx_world, var_buffer);
}

// int_value is the char_value's length when a char_value is present.
static void build_var_buffer(char **var_buffer, const char *name,
 int int_value, char *char_value, int index)
{
  char *var;

  (*var_buffer) = cmalloc(VAR_LIST_VAR + strlen(name) + 1);
  var = (*var_buffer) + VAR_LIST_VAR;

  // Counter/String
  if(index == -1)
  {
    var[-1] = 1;
    var[-2] = 1;
    // If counter==0 or string length==0, either way
    if(!int_value)
      var[-1] = 0;
  }
  // Variable
  else
  {
    var[-1] = (char)index;
    var[-2] = 0;
  }

  // Display
  memset((*var_buffer), ' ', VAR_LIST_WIDTH);
  (*var_buffer)[VAR_LIST_WIDTH - 1] = '\0';

  // Internal
  strcpy(var, name);

  if(char_value)
  {
    snprintf(*var_buffer, SVALUE_COL_OFFSET, "%s", name);
    snprintf(*var_buffer + SVALUE_COL_OFFSET, SVALUE_SIZE, "%.*s",
     int_value, char_value);
  }
  else
  {
    snprintf(*var_buffer, CVALUE_COL_OFFSET, "%s", name);
    snprintf(*var_buffer + CVALUE_COL_OFFSET, CVALUE_SIZE, "%i", int_value);
  }
}

/************************
 * Debug tree functions *
 ************************/

/* (root)-----16-S|---- for 58 now instead of 75.
 * - Counters
 *     a
 *     b
 *   - c
 *       ca (TODO IN FUTURE)
 *       cn
 * - Strings
 *     $t
 * - Sprites
 *     spr0
 *     spr1
 *   World
 *   Board
 * + Robots
 *     0 Global
 *     1 (12,184)
 *     2 (139,18)
 */

// Tree entry format:
// Display 0-16, NULL at 17

// Var entry format:
// Display 0-54, NULL at 57
// Ind 58==0 indicates var instead of counter.
// In that case, ind 59 is robot id and real name starts at 60
// Otherwise counter name starts at 58.

struct debug_node
{
   char name[15];
   bool opened;
   bool refresh_on_focus;
   bool show_child_contents;
   int num_nodes;
   int num_counters;
   struct debug_node *parent;
   struct debug_node *nodes;
   char **counters;
};

// Build the tree display on the left side of the screen
static void build_tree_list(struct debug_node *node,
 char ***tree_list, int *tree_size, int level)
{
  int i;
  int level_offset = 1;
  char *name;

  // Skip empty nodes entirely
  if(node->num_nodes == 0 && node->num_counters == 0)
    return;

  if(level > 0)
  {
    // Display name and a real name so the menu can find it later.
    name = cmalloc(TREE_LIST_WIDTH + strlen(node->name) + 1);
    memset(name, ' ', level * level_offset);
    if(node->num_nodes)
    {
      if(node->opened)
        name[(level-1) * level_offset] = '-';
      else
        name[(level-1) * level_offset] = '+';
    }

    // Display name
    strncpy(name + level * level_offset, node->name, TREE_LIST_WIDTH - level * level_offset);
    name[TREE_LIST_WIDTH - 1] = '\0';

    // Real name
    strcpy(name + TREE_LIST_WIDTH, node->name);
    name[TREE_LIST_WIDTH + strlen(node->name)] = '\0';

    (*tree_list) = crealloc(*tree_list, sizeof(char *) * (*tree_size + 1));

    (*tree_list)[*tree_size] = name;
    (*tree_size)++;
  }

  if(node->num_nodes && node->opened)
    for(i = 0; i < node->num_nodes; i++)
      build_tree_list(&(node->nodes[i]), tree_list, tree_size, level+1);

}
// Free the tree list and all of its lines.
static void free_tree_list(char **tree_list, int tree_size)
{
  int i;

  for(i = 0; i < tree_size; i++)
    free(tree_list[i]);

  free(tree_list);
}
// Free and build in one package
static void rebuild_tree_list(struct debug_node *node,
 char ***tree_list, int *tree_size)
{
  if(*tree_list)
  {
    free_tree_list(*tree_list, *tree_size);
    *tree_list = NULL;
    *tree_size = 0;
  }
  build_tree_list(node, tree_list, tree_size, 0);
}

// From a node, build the list of all counters that should appear
// This list is just built out of pointers to the tree, so it
// should never have its contents freed.
static void build_var_list(struct debug_node *node,
 char ***var_list, int *num_vars, int hide_empty)
{
  if(node->num_counters)
  {
    size_t vars_size = (*num_vars) * sizeof(char *);
    size_t added_size = node->num_counters * sizeof(char *);
    int added_num = node->num_counters;
    char **copy_src = node->counters;

    // If we're hiding the empties, we need to make a buffer and copy the visibles to it
    if(hide_empty)
    {
      int i, j;
      copy_src = cmalloc(node->num_counters * sizeof(char *));
      for(i = 0, j = 0; i < node->num_counters; i++)
      {
        // If it's a counter/string, not built-in
        if(node->counters[i][VAR_LIST_VAR - 2])
          // If empty
          if(!(node->counters[i][VAR_LIST_VAR - 1]))
            continue;

        copy_src[j] = node->counters[i];
        j++;
      }
      added_size = j * sizeof(char *);
      added_num = j;
    }

    if(added_num)
    {
      (*var_list) = crealloc(*var_list, vars_size + added_size);

      memcpy((*var_list) + *num_vars, copy_src, added_size);
      (*num_vars) += added_num;
    }

    // If we turned copy_src into a buffer we need to free it.
    if(copy_src != node->counters)
      free(copy_src);
  }

  if (node->num_nodes && node->show_child_contents)
  {
    int i;
    for(i = 0; i < node->num_nodes; i++)
      build_var_list(&(node->nodes[i]), var_list, num_vars, hide_empty);
  }
}
static void rebuild_var_list(struct debug_node *node,
 char ***var_list, int *num_vars, int hide_empty)
{
  if(*var_list)
  {
    free(*var_list);
    *var_list = NULL;
    *num_vars = 0;
  }
  build_var_list(node, var_list, num_vars, hide_empty);
}

// If we're not deleting the entire tree we only wipe the counter lists
static void clear_debug_node(struct debug_node *node, bool delete_all)
{
  int i;
  if(node->num_counters)
  {
    for(i = 0; i < node->num_counters; i++)
      free(node->counters[i]);

    free(node->counters);
    node->counters = NULL;
    node->num_counters = 0;
  }

  if(node->num_nodes)
  {
    for(i = 0; i < node->num_nodes; i++)
      clear_debug_node(&(node->nodes[i]), delete_all);

    if(delete_all)
    {
      free(node->nodes);
      node->nodes = NULL;
      node->num_nodes = 0;
    }
  }
}

/****************************/
/* Tree Searching functions */
/****************************/

// Use this when somebody selects something from the tree list
static struct debug_node *find_node(struct debug_node *node, char *name)
{
  if(!strcmp(node->name, name))
    return node;

  if(node->nodes)
  {
    int i;
    struct debug_node *result;
    for(i = 0; i < node->num_nodes; i++)
    {
      result = find_node(&(node->nodes[i]), name);
      if(result)
        return result;
    }
  }

  return NULL;
}

static void get_node_name(struct debug_node *node, char *label, int max_length)
{
  if(node->parent && node->parent->parent)
  {
    get_node_name(node->parent, label, max_length);
    snprintf(label + strlen(label), max_length - strlen(label), " :: ");
  }

  snprintf(label + strlen(label), max_length - strlen(label), "%s", node->name);
}

static int select_var_buffer(struct debug_node *node, char *var_name,
 struct debug_node **new_focus, int *new_var_pos)
{
  int i;

  if(node->num_counters)
  {
    for(i = 0; i < node->num_counters; i++)
    {
      if(!strcmp(node->counters[i] + VAR_LIST_VAR, var_name))
      {
        *new_focus = node;
        *new_var_pos = i;
        return 1;
      }
    }
  }

  if(node->num_nodes)
    for(i = 0; i < node->num_nodes; i++)
      if(select_var_buffer(&(node->nodes[i]), var_name, new_focus, new_var_pos))
        return 1;

  return 0;
}



/******************/
/* Counter search */
/******************/

static int find_variable(struct world *mzx_world, struct debug_node *node,
 char **search_var, struct debug_node **search_node, int *search_pos,
 char *match_text, int *match_text_index, size_t match_length,
 int search_flags, char **stop_var)
{
  int i;
  char *var;
  void *v = NULL;
  int start = 0, stop = node->num_counters, inc = 1;
  bool ignore_case = (search_flags & VAR_SEARCH_CASESENS) == 0;

  if(search_flags & VAR_SEARCH_REVERSE)
  {
    for(i = node->num_nodes - 1; i >= 0; i--)
    {
      int r = find_variable(mzx_world, &(node->nodes[i]), search_var,
       search_node, search_pos, match_text, match_text_index, match_length,
       search_flags, stop_var);
      if(r)
        return r;
    }
    start = node->num_counters - 1;
    stop = -1;
    inc = -1;
  }

  if(*search_node)
  {
    if(node == *search_node)
    {
      *search_var = NULL;
      *search_node = NULL;
    }
  }

  for(i = start; i != stop; i += inc)
  {
    if(*search_var || *search_node)
    {
      if(node->counters[i] == *search_var)
        *search_var = NULL;

      continue;
    }

    var = node->counters[i] + VAR_LIST_VAR;

    if(search_flags & VAR_SEARCH_NAMES)
    {
      if(search_flags & VAR_SEARCH_EXACT)
      {
        if((ignore_case && !strcasecmp(var, match_text)) ||
         !strcmp(var, match_text))
          v = var;
      }
      else
      {
        v = boyer_moore_search(var, strlen(var), match_text, match_length,
         match_text_index, ignore_case);
      }
    }
    if((search_flags & VAR_SEARCH_VALUES) && !v)
    {
      int length = 0;
      char *value = NULL;
      struct string temp;
      if(var[0] == '$')
      {
        get_string(mzx_world, var, &temp, 0);
        length = temp.length;
        value = temp.value;
      }
      else
      {
        // We'll use the version printed on the var list itself.
        // The only time this should go wrong is with excessively long mod paths.
        if(var[-2])
          value = node->counters[i] + CVALUE_COL_OFFSET;
        else
        {
          if(match_var("robot_name*") ||
             match_var("board_name*") ||
             match_var("board_mod*") ||
             match_var("mod_name*") ||
             match_var("input*"))
             value = node->counters[i] + SVALUE_COL_OFFSET;
          else
            value = node->counters[i] + CVALUE_COL_OFFSET;
        }

        length = strlen(value);
      }

      if(search_flags & VAR_SEARCH_EXACT)
      {
        if((ignore_case && !strcasecmp(value, match_text)) ||
         !strcmp(value, match_text))
          v = value;
      }
      else
      {
        v = boyer_moore_search(value, length, match_text, match_length,
         match_text_index, ignore_case);
      }
    }

    if(v)
    {
      *search_var = node->counters[i];
      *search_node = node;
      *search_pos = i;
      return 1;
    }

    if(node->counters[i] == *stop_var)
      return -1;

    if(!*stop_var)
      *stop_var = node->counters[i];
  }

  if(!(search_flags & VAR_SEARCH_REVERSE))
  {
    for(i = 0; i < node->num_nodes; i++)
    {
      int r = find_variable(mzx_world, &(node->nodes[i]), search_var,
       search_node, search_pos, match_text, match_text_index, match_length,
       search_flags, stop_var);
      if(r)
        return r;
    }
  }
  return 0;
}

static char *find_first_var(struct debug_node *node)
{
  char *res = NULL;

  if(node->num_counters)
    res = node->counters[0];

  if(node->num_nodes && !res)
  {
    int i;
    for(i = 0; i < node->num_nodes && !res; i++)
      res = find_first_var(&(node->nodes[i]));
  }

  return res;
}
static char *find_last_var(struct debug_node *node)
{
  char *res = NULL;

  if(node->num_nodes)
  {
    int i;
    for(i = node->num_nodes - 1; i >= 0 && !res; i--)
      res = find_last_var(&(node->nodes[i]));
  }

  if(node->num_counters && !res)
    res = node->counters[node->num_counters - 1];

  return res;
}

static int start_var_search(struct world *mzx_world, struct debug_node *node,
 char **search_var, struct debug_node **search_node, int *search_pos,
 char *match_text, size_t match_length, int search_flags)
{
  int result = 0;
  char *stop_var = NULL;

  // Build the index that's gonna save our bacon when we search through 1m counters
  int index[256] = { 0 };
  bool ignore_case = (search_flags & VAR_SEARCH_CASESENS) == 0;

  boyer_moore_index(match_text, match_length, index, ignore_case);

  // Set up where the search should stop
  if(search_flags & VAR_SEARCH_WRAP)
  {
    stop_var = *search_var;
  }
  else
  // No wrap, use start if reverse, otherwise use end
  if(search_flags & VAR_SEARCH_REVERSE)
  {
    stop_var = find_first_var(node);
  }
  else
  {
    stop_var = find_last_var(node);
  }

  result = find_variable(mzx_world, node, search_var, search_node,
   search_pos, match_text, index, match_length, search_flags, &stop_var);

  // Wrap? Try again from the start
  if(!result && (search_flags & VAR_SEARCH_WRAP))
    result = find_variable(mzx_world, node, search_var, search_node,
     search_pos, match_text, index, match_length, search_flags, &stop_var);

  return result;
}

/**********************************/
/******* MAKE THE TREE CODE *******/
/**********************************/

// Create new counter lists.
// (Re)make the child nodes
static void repopulate_tree(struct world *mzx_world, struct debug_node *root)
{
  int i, j, n, *num = NULL, alloc = 0;

  char ***list = NULL;
  char var[20] = { 0 };
  char buf[80] = { 0 };

  char name[28] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  int num_counters = mzx_world->num_counters;
  int num_strings = mzx_world->num_strings;

  struct debug_node *counters = &(root->nodes[0]);
  struct debug_node *strings = &(root->nodes[1]);
  struct debug_node *sprites = &(root->nodes[2]);
  struct debug_node *config = &(root->nodes[3]);
  struct debug_node *world = &(root->nodes[4]);
  struct debug_node *board = &(root->nodes[5]);
  struct debug_node *robots = &(root->nodes[6]);

  int num_counter_nodes = 27;
  int num_string_nodes = 27;
  int num_sprite_nodes = 256;
  int num_robot_nodes = mzx_world->current_board->num_robots + 1;

  struct debug_node *counter_nodes =
   ccalloc(num_counter_nodes, sizeof(struct debug_node));
  struct debug_node *string_nodes =
   ccalloc(num_string_nodes, sizeof(struct debug_node));
  struct debug_node *sprite_nodes =
   ccalloc(num_sprite_nodes, sizeof(struct debug_node));
  struct debug_node *robot_nodes =
   ccalloc(num_robot_nodes, sizeof(struct debug_node));

  struct robot **robot_list = mzx_world->current_board->robot_list;

  sort_counter_list(mzx_world->counter_list, mzx_world->num_counters);
  sort_string_list(mzx_world->string_list, mzx_world->num_strings);

  // We want to start off on a clean slate here
  clear_debug_node(counters, true);
  clear_debug_node(strings, true);
  clear_debug_node(sprites, true);
  clear_debug_node(config, true);
  clear_debug_node(world, true);
  clear_debug_node(board, true);
  clear_debug_node(robots, true);

  /************/
  /* Counters */
  /************/

  for(i = 0, n = 0; i < num_counters; i++)
  {
    char first = tolower((int)mzx_world->counter_list[i]->name[0]);
    // We need to switch child nodes
    if((first > n+96 && n<27) || i == 0)
    {
      if(first < 'a')
        n = 0;
      else if(first > 'z')
        n = 27;
      else
        n = (int)first - 96;
      num = &(counter_nodes[n % 27].num_counters);
      list = &(counter_nodes[n % 27].counters);
      alloc = *num;
    }

    if(*num == alloc)
      (*list) = crealloc(*list, (alloc = MAX(alloc * 2, 32)) * sizeof(char *));

    build_var_buffer( &(*list)[*num], mzx_world->counter_list[i]->name,
     mzx_world->counter_list[i]->value, NULL, -1);

    (*num)++;
  }
  // Set everything else and optimize the allocs
  for(i = 0; i < num_counter_nodes; i++)
  {
    if(counter_nodes[i].num_counters)
    {
      counter_nodes[i].name[0] = name[i];
      counter_nodes[i].name[1] = '\0';
      counter_nodes[i].parent = counters;
      counter_nodes[i].num_nodes = 0;
      counter_nodes[i].nodes = NULL;
      counter_nodes[i].counters =
       crealloc(counter_nodes[i].counters, counter_nodes[i].num_counters * sizeof(char *));
    }
  }
  // And finish counters
  counters->num_nodes = num_counter_nodes;
  counters->nodes = counter_nodes;

  /***********/
  /* Strings */
  /***********/

  for(i = 0, n = 0; i < num_strings; i++)
  {
    char first = tolower((int)mzx_world->string_list[i]->name[1]);

    // We need to switch child nodes
    if((first > n+96 && n<27) || i == 0)
    {
      if(first < 'a')
        n = 0;
      else if(first > 'z')
        n = 27;
      else
        n = (int)first - 96;
      num = &(string_nodes[n % 27].num_counters);
      list = &(string_nodes[n % 27].counters);
      alloc = *num;
    }

    if(*num == alloc)
      *list = crealloc(*list, (alloc = MAX(alloc * 2, 32)) * sizeof(char *));

    copy_substring_escaped(mzx_world->string_list[i], buf, 79);

    build_var_buffer( &(*list)[*num], mzx_world->string_list[i]->name,
     strlen(buf), buf, -1);

    (*num)++;
  }
  // Set everything else and optimize the allocs
  for(i = 0; i < num_string_nodes; i++)
  {
    if(string_nodes[i].num_counters)
    {
      string_nodes[i].name[0] = '$';
      string_nodes[i].name[1] = name[i];
      string_nodes[i].name[2] = '\0';
      string_nodes[i].parent = strings;
      string_nodes[i].num_nodes = 0;
      string_nodes[i].nodes = NULL;
      string_nodes[i].counters =
       crealloc(string_nodes[i].counters, string_nodes[i].num_counters * sizeof(char *));
    }
  }
  // And finish strings
  strings->num_nodes = num_string_nodes;
  strings->nodes = string_nodes;

  /***********/
  /* Sprites */
  /***********/

  sprites->counters = ccalloc(mzx_world->collision_count + 3, sizeof(char *));

  build_var_buffer( &(sprites->counters)[0], "spr_num", mzx_world->sprite_num, NULL, 0);
  build_var_buffer( &(sprites->counters)[1], "spr_yorder", mzx_world->sprite_y_order, NULL, 0);
  build_var_buffer( &(sprites->counters)[2], "spr_collisions*", mzx_world->collision_count, NULL, 0);

  for(i = 0; i < mzx_world->collision_count; i++)
  {
    snprintf(var, 20, "spr_clist%i*", i);
    build_var_buffer( &(sprites->counters)[i + 3], var,
     mzx_world->collision_list[i], NULL, 0);
  }
  sprites->num_counters = i + 3;

  for(i = 0, j = 0; j < num_sprite_nodes; i++, j++)
  {
    if(mzx_world->sprite_list[i]->width == 0 && mzx_world->sprite_list[i]->height == 0)
    {
      j--;
      num_sprite_nodes--;
      continue;
    }

    list = &(sprite_nodes[j].counters);
    (*list) = ccalloc(num_sprite_vars, sizeof(char *));

    for(n = 0; n < num_sprite_vars; n++)
    {
      snprintf(var, 20, "spr%i_%s", i, sprite_var_list[n]);
      build_var_buffer( &(*list)[n], var, 0, NULL, 0);
      read_var(mzx_world, (*list)[n]);
    }
    snprintf(var, 14, "spr%i", i);
    strcpy(sprite_nodes[j].name, var);
    sprite_nodes[j].parent = sprites;
    sprite_nodes[j].num_nodes = 0;
    sprite_nodes[j].nodes = NULL;
    sprite_nodes[j].num_counters = num_sprite_vars;
  }
  if(num_sprite_nodes)
  {
    sprite_nodes = crealloc(sprite_nodes, num_sprite_nodes * sizeof(struct debug_node));
    sprites->num_nodes = num_sprite_nodes;
    sprites->nodes = sprite_nodes;
  }
  else
    free(sprite_nodes);

  /**********/
  /* Config */
  /**********/

  config->counters = ccalloc(num_config_vars, sizeof(char *));
  for(n = 0; n < num_config_vars; n++)
  {
    build_var_buffer( &(config->counters[n]), config_var_list[n], 0, NULL, 0);
    read_var(mzx_world, config->counters[n]);
  }
  config->num_counters = num_config_vars;

  /*********/
  /* World */
  /*********/

  world->counters = ccalloc(num_world_vars, sizeof(char *));
  for(n = 0; n < num_world_vars; n++)
  {
    build_var_buffer( &(world->counters)[n], world_var_list[n], 0, NULL, 0);
    read_var(mzx_world, (world->counters)[n]);
  }
  world->num_counters = num_world_vars;

  /*********/
  /* Board */
  /*********/

  board->counters = ccalloc(num_board_vars, sizeof(char *));
  for(n = 0; n < num_board_vars; n++)
  {
    build_var_buffer( &(board->counters)[n], board_var_list[n], 0, NULL, 0);
    read_var(mzx_world, (board->counters)[n]);
  }
  board->num_counters = num_board_vars;

  /**********/
  /* Robots */
  /**********/

  for(i = 0, j = 0; j < num_robot_nodes; i++, j++)
  {
    struct robot *robot = robot_list[i];

    if(!robot || !robot->used)
    {
      j--;
      num_robot_nodes--;
      continue;
    }
    list = &(robot_nodes[j].counters);
    *list = ccalloc(num_robot_vars + 32, sizeof(char *));

    for(n = 0; n < num_robot_vars; n++)
    {
      build_var_buffer( &(*list)[n], robot_var_list[n], 0, NULL, i&255);
      read_var(mzx_world, (*list)[n]);
    }
    for(n = 0; n < 32; n++)
    {
      sprintf(var, "local%i", n);
      build_var_buffer( &(*list)[n + num_robot_vars], var,
       robot->local[(n-1)&31], NULL, i&255);
    }

    snprintf(var, 20, "%i:%s", i, robot_list[i]->robot_name);
    strncpy(robot_nodes[j].name, var, 14);
    robot_nodes[j].name[14] = 0;
    robot_nodes[j].parent = robots;
    robot_nodes[j].num_nodes = 0;
    robot_nodes[j].nodes = NULL;
    robot_nodes[j].num_counters = num_robot_vars + 32;
  }
  // And finish robots... all done!
  if(num_robot_nodes)
  {
    robot_nodes = crealloc(robot_nodes, num_robot_nodes * sizeof(struct debug_node));
    robots->num_nodes = num_robot_nodes;
    robots->nodes = robot_nodes;
  }
  else
    free(robot_nodes);

}

// Create the base tree structure, except for sprites and robots
static void build_debug_tree(struct world *mzx_world, struct debug_node *root)
{
  int num_root_nodes = 7;
  struct debug_node *root_nodes =
   ccalloc(num_root_nodes, sizeof(struct debug_node));

  struct debug_node root_node =
  {
    "",
    true,  //opened
    false, //refresh_on_focus
    false, //show_child_contents
    num_root_nodes,
    0,
    NULL,  //parent
    root_nodes,
    NULL
  };

  struct debug_node counters = {
    "Counters",
    false,
    false,
    true,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node strings = {
    "Strings",
    false,
    false,
    true,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node sprites = {
    "Sprites",
    false,
    false,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node config = {
    "Universal",
    false,
    true,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node world = {
    "World",
    false,
    true,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node board = {
    "Board",
    false,
    true,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node robots = {
    "Robots",
    false,
    false,
    false,
    0,
    0,
    root,
    NULL,
    NULL
  };

  memcpy(root, &root_node, sizeof(struct debug_node));
  memcpy(&root_nodes[0], &counters, sizeof(struct debug_node));
  memcpy(&root_nodes[1], &strings, sizeof(struct debug_node));
  memcpy(&root_nodes[2], &sprites, sizeof(struct debug_node));
  memcpy(&root_nodes[3], &config, sizeof(struct debug_node));
  memcpy(&root_nodes[4], &world, sizeof(struct debug_node));
  memcpy(&root_nodes[5], &board, sizeof(struct debug_node));
  memcpy(&root_nodes[6], &robots, sizeof(struct debug_node));

  repopulate_tree(mzx_world, root);
}

/*****************************/
/* Set Counter/String Dialog */
/*****************************/

static void input_counter_value(struct world *mzx_world, char *var_buffer)
{
  char *var = var_buffer + VAR_LIST_VAR;
  char new_value[70];
  char name[70] = { 0 };
  int id = 0;

  if(var[0] == '$')
  {
    struct string temp;
    get_string(mzx_world, var, &temp, 0);

    snprintf(name, 69, "Edit: string %s", var);

    copy_substring_escaped(&temp, new_value, 68);
  }
  else
  {
    if(var[-2])
    {
      snprintf(name, 69, "Edit: counter %s", var);
      sprintf(new_value, "%d", get_counter_safe(mzx_world, var, id));
    }
    else
    {
      id = var[-1];

      if(var[strlen(var)-1] == '*')
        return;

      snprintf(name, 69, "Edit: variable %s", var);

      // Just strcpy it off of the var_buffer so we don't need
      // a special if/else chain for the write-only variables
      strcpy(new_value, var_buffer + CVALUE_COL_OFFSET);
    }
  }

  save_screen();
  draw_window_box(4, 12, 76, 14, DI_DEBUG_BOX, DI_DEBUG_BOX_DARK,
   DI_DEBUG_BOX_CORNER, 1, 1);
  write_string(name, 6, 12, DI_DEBUG_LABEL, 0);

  if(intake(mzx_world, new_value, 68, 6, 13, 15, 1, 0,
   NULL, 0, NULL) != IKEY_ESCAPE && !get_exit_status())
  {
    if(var[0] == '$')
    {
      int len;
      unescape_string(new_value, &len);
      write_var(mzx_world, var_buffer, len, new_value);
    }
    else
    {
      int counter_value = strtol(new_value, NULL, 10);
      write_var(mzx_world, var_buffer, counter_value, NULL);
    }
  }

  restore_screen();
}

/*****************/
/* Search Dialog */
/*****************/

static int search_dialog_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  if((key == IKEY_RETURN) && (di->current_element == 0))
  {
    di->return_value = 0;
    di->done = 1;
    return 0;
  }

  return key;
}

static int search_dialog(struct world *mzx_world,
 const char *title, char *string, int *search_flags)
{
  int result = 0;
  struct element *elements[8];
  struct dialog di;

  const char *name_opt[] = { "Search names" };
  const char *value_opt[] = { "Search values" };
  const char *case_opt[] = { "Case sensitive" };
  const char *exact_opt[] = { "Exact" };
  const char *reverse_opt[] = { "Reverse" };
  const char *wrap_opt[] = { "Wrap" };
  const char *local_opt[] = { "Current list" };

  int names =    (*search_flags & VAR_SEARCH_NAMES) > 0;
  int values =   (*search_flags & VAR_SEARCH_VALUES) > 0;
  int casesens = (*search_flags & VAR_SEARCH_CASESENS) > 0;
  int exact =    (*search_flags & VAR_SEARCH_EXACT) > 0;
  int reverse =  (*search_flags & VAR_SEARCH_REVERSE) > 0;
  int wrap =     (*search_flags & VAR_SEARCH_WRAP) > 0;
  int local =    (*search_flags & VAR_SEARCH_LOCAL) > 0;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  elements[0] = construct_input_box( 2, 1, "Search: ", VAR_SEARCH_MAX, 0, string);
  elements[1] = construct_check_box( 2, 2, name_opt,    1, strlen(name_opt[0]),    &names);
  elements[2] = construct_check_box(20, 2, value_opt,   1, strlen(value_opt[0]),   &values);
  elements[3] = construct_check_box(39, 2, case_opt,    1, strlen(case_opt[0]),    &casesens);
  elements[4] = construct_check_box( 4, 3, exact_opt,   1, strlen(exact_opt[0]),   &exact);
  elements[5] = construct_check_box(15, 3, reverse_opt, 1, strlen(reverse_opt[0]), &reverse);
  elements[6] = construct_check_box(28, 3, wrap_opt,    1, strlen(wrap_opt[0]),    &wrap);
  elements[7] = construct_check_box(38, 3, local_opt,   1, strlen(local_opt[0]),   &local);

  construct_dialog_ext(&di, title, VAR_SEARCH_DIALOG_X, VAR_SEARCH_DIALOG_Y,
   VAR_SEARCH_DIALOG_W, VAR_SEARCH_DIALOG_H, elements, ARRAY_SIZE(elements),
   0, 0, 0, search_dialog_idle_function);

  result = run_dialog(mzx_world, &di);

  *search_flags = (names * VAR_SEARCH_NAMES) + (values * VAR_SEARCH_VALUES) +
   (casesens * VAR_SEARCH_CASESENS) + (reverse * VAR_SEARCH_REVERSE) +
   (wrap * VAR_SEARCH_WRAP) + (exact * VAR_SEARCH_EXACT) +
   (local * VAR_SEARCH_LOCAL);

  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  return result;
}

/**********************/
/* New Counter Dialog */
/**********************/

// Returns pointer to name if successful or NULL if cancelled or already existed
static int new_counter_dialog(struct world *mzx_world, char *name)
{
  int result;
  struct element *elements[3];
  struct dialog di;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  elements[0] = construct_input_box(2, 2, "Name:", VAR_ADD_MAX, 0, name);
  elements[1] = construct_button(9, 4, "Confirm", 0);
  elements[2] = construct_button(23, 4, "Cancel", -1);

  construct_dialog_ext(&di, "New Counter/String", VAR_ADD_DIALOG_X, VAR_ADD_DIALOG_Y,
   VAR_ADD_DIALOG_W, VAR_ADD_DIALOG_H, elements, ARRAY_SIZE(elements),
   0, 0, 0, NULL);

  result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(!result && (strlen(name) > 0))
  {
    // String
    if(name[0] == '$')
    {
      struct string temp;
      memset(&temp, '\0', sizeof(struct string));

      if(strchr(name, '.') || strchr(name, '+') || strchr(name, '#'))
      {
        confirm(mzx_world, "Characters .+# not allowed in string name.");
        return 0;
      }

      if(!get_string(mzx_world, name, &temp, 0))
      {
        //Doesn't exist -- set string
        temp.value = (char *)""; //tee hee
        temp.length = 0;

        set_string(mzx_world, name, &temp, 0);
      }

      return 1;
    }
    // Counter
    else
    {
      if(!get_counter_safe(mzx_world, name, 0))
        set_counter(mzx_world, name, 0, 0);

      return 1;
    }
  }

  return 0;
}

/********************/
/* Counter Debugger */
/********************/

/*-3 - Repeat search
 *-2 - Update selected tree
 *-1 - Close
 * 0 - Selected var in var list
 * 1 - Selected node in tree list
 * 2 - Search
 * 3 - Add new counter
 * 4 - Hide empties
 * 5 - Export
 */
int last_node_selected = 0;

static int counter_debugger_idle_function(struct world *mzx_world,
 struct dialog *di, int key)
{
  struct list_box *tree_list = (struct list_box *)di->elements[1];

  if((di->current_element == 1) && (*(tree_list->result) != last_node_selected))
  {
    di->return_value = -2;
    di->done = 1;
  }

  switch(key)
  {
    // Char editor
    case IKEY_c:
    {
      if(get_alt_status(keycode_internal))
      {
        char_editor(mzx_world);
        return 0;
      }
      break;
    }
    // Palette editor
    case IKEY_e:
    {
      if(get_alt_status(keycode_internal))
      {
        palette_editor(mzx_world);
        return 0;
      }
      break;
    }
    // Search dialog
    case IKEY_f:
    {
      if(get_ctrl_status(keycode_internal))
      {
        di->return_value = 2;
        di->done = 1;
        return 0;
      }
      break;
    }
    // Repeat search
    case IKEY_r:
    {
      if(get_ctrl_status(keycode_internal))
      {
        di->return_value = -3;
        di->done = 1;
        return 0;
      }
      break;
    }
    // Hide empties
    case IKEY_h:
    {
      if(get_alt_status(keycode_internal))
      {
        di->return_value = 4;
        di->done = 1;
        return 0;
      }
      break;
    }
    // New counter/string
    case IKEY_n:
    {
      if(get_alt_status(keycode_internal))
      {
        di->return_value = 3;
        di->done = 1;
        return 0;
      }
      break;
    }
  }

  return key;
}

static struct debug_node root;
static char previous_var[VAR_SEARCH_MAX + 1];
static char previous_node_name[15];

void __debug_counters(struct world *mzx_world)
{
  int i;

  int num_vars = 0, tree_size = 0;
  char **var_list = NULL, **tree_list = NULL;

  int window_focus = 0;
  int var_selected = 0;
  int node_selected = 0;
  int node_scroll_offset = 0;
  int dialog_result;
  struct element *elements[8];
  struct dialog di;

  char label[80] = "Counters";
  struct debug_node *focus;
  int hide_empty_vars = 0;

  char search_text[VAR_SEARCH_MAX + 1] = { 0 };
  int search_flags = VAR_SEARCH_NAMES + VAR_SEARCH_VALUES + VAR_SEARCH_WRAP;
  int search_pos = 0;

  int reopened = 0;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_COUNTER_DEBUG);

  m_show();

  // also known as crash_stack
  build_debug_tree(mzx_world, &root);

  focus = &(root.nodes[node_selected]);

  // If the debugger was opened before, try to pick up where we left off.
  if(previous_var[0] && previous_node_name[0])
  {
    struct debug_node *previous_node = find_node(&root, previous_node_name);

    if(previous_node)
    {
      focus = previous_node;
      snprintf(search_text, VAR_SEARCH_MAX, "%s", previous_var);
      search_flags =
       VAR_SEARCH_NAMES | VAR_SEARCH_LOCAL | VAR_SEARCH_WRAP | VAR_SEARCH_EXACT;

      reopened = 1;
    }
  }

  rebuild_tree_list(&root, &tree_list, &tree_size);
  rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);

  do
  {
    last_node_selected = node_selected;

    if(!reopened)
    {
      elements[0] = construct_list_box(
       VAR_LIST_X, VAR_LIST_Y, (const char **)var_list, num_vars,
       VAR_LIST_HEIGHT, VAR_LIST_WIDTH, 0, &var_selected, NULL, false);
      elements[1] = construct_list_box(
       TREE_LIST_X, TREE_LIST_Y, (const char **)tree_list, tree_size,
       TREE_LIST_HEIGHT, TREE_LIST_WIDTH, 1, &node_selected, &node_scroll_offset, false);
      elements[2] = construct_button(BUTTONS_X + 0, BUTTONS_Y + 0, "Search", 2);
      elements[3] = construct_button(BUTTONS_X +11, BUTTONS_Y + 0, "New", 3);
      elements[4] = construct_button(BUTTONS_X + 0, BUTTONS_Y + 2, "Toggle Empties", 4);
      elements[5] = construct_button(BUTTONS_X + 0, BUTTONS_Y + 4, "Export", 5);
      elements[6] = construct_button(BUTTONS_X +10, BUTTONS_Y + 4, "Done", -1);
      elements[7] = construct_label(VAR_LIST_X, VAR_LIST_Y - 1, label);

      construct_dialog_ext(&di, "Debug Variables", 0, 0,
       80, 25, elements, ARRAY_SIZE(elements), 0, 0, window_focus,
       counter_debugger_idle_function);

      dialog_result = run_dialog(mzx_world, &di);
    }

    else
    {
      // Reopened? Force a search
      dialog_result = -3;
    }

    if(node_selected != last_node_selected)
      focus = find_node(&root, tree_list[node_selected] + TREE_LIST_WIDTH);

    switch(dialog_result)
    {
      // Var list
      case 0:
      {
        window_focus = 0;
        if(var_selected < num_vars)
          input_counter_value(mzx_world, var_list[var_selected]);

        break;
      }

      // Switch to a new view (called by idle function)
      case -2:
      {
        // Tree list
        window_focus = 1;

        if(last_node_selected != node_selected)// && (focus->num_counters || focus->show_child_contents))
        {
          rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);
          var_selected = 0;

          label[0] = '\0';
          get_node_name(focus, label, 80);
        }
        break;
      }

      // Tree node
      case 1:
      {
        window_focus = 1;

        // Collapse/Expand selected view if applicable
        if(focus->num_nodes)
        {
          focus->opened = !(focus->opened);

          rebuild_tree_list(&root, &tree_list, &tree_size);
        }
        break;
      }

      // Search (Ctrl+F)
      case 2:
      {
        if(search_dialog(mzx_world, "Search variables (Ctrl+F, repeat Ctrl+R)", search_text, &search_flags))
        {
          break;
        }
      }

      /* fallthrough */

      // Repeat search (Ctrl+R)
      case -3:
      {
        int res = 0;
        char *search_var = NULL;
        struct debug_node *search_node = NULL;
        struct debug_node *search_targ = &root;
        char search_text_unescaped[VAR_SEARCH_MAX + 1] = { 0 };
        size_t search_text_length = 0;

        // This will be almost always
        if(var_selected < num_vars)
          search_var = var_list[var_selected];
        // Only if the current view is empty
        else
          search_node = focus;

        strcpy(search_text_unescaped, search_text);
        unescape_string(search_text_unescaped, (int *)(&search_text_length));

        if(!search_text_length)
          break;

        // Local search?
        if(search_flags & VAR_SEARCH_LOCAL)
          search_targ = focus;

        res = start_var_search(mzx_world, search_targ,
         &search_var, &search_node, &search_pos,
         search_text_unescaped, search_text_length, search_flags);

        switch(res)
        {
          case 1:
          {
            struct debug_node *node;

            // This could result in some not-so-good things happening
            hide_empty_vars = 0;

            // First, is it in the current list? Override!
            for(i = 0; i < num_vars; i++)
            {
              if(var_list[i] == search_var)
              {
                search_pos = i;
                search_node = focus;
                break;
              }
            }

            // Nothing in the local context in a local search?  Abandon ship!
            if((search_flags & VAR_SEARCH_LOCAL) && (search_node != focus))
              break;

            // Open all parents
            node = search_node;
            while(node && node->parent)
            {
              node = node->parent;
              node->opened = true;
            }

            // Clear and rebuild tree list
            rebuild_tree_list(&root, &tree_list, &tree_size);

            for(i = 0; i < tree_size; i++)
            {
              if(!strcmp(tree_list[i] + TREE_LIST_WIDTH, search_node->name))
              {
                node_selected = i;
                node_scroll_offset = node_selected - (TREE_LIST_HEIGHT/2);
                break;
              }
            }

            // Open search_node in var list
            rebuild_var_list(search_node, &var_list, &num_vars, hide_empty_vars);
            var_selected = search_pos;

            // Fix label name
            label[0] = '\0';
            get_node_name(search_node, label, 80);

            window_focus = 0; // Var list
            focus = search_node;
          }
        }
        break;
      }

      // Add counter/string
      case 3:
      {
        char add_name[VAR_ADD_MAX + 1] = { 0 };
        window_focus = 3;

        if(new_counter_dialog(mzx_world, add_name))
        {
          struct debug_node *node;

          // Hope it was worth it!
          repopulate_tree(mzx_world, &root);

          // The new counter is empty, so
          hide_empty_vars = 0;

          // Find the counter/string we just made
          select_var_buffer(&root, add_name, &focus, &var_selected);

          // Open all parents
          node = focus;
          while(node->parent)
          {
            node = node->parent;
            node->opened = true;
          }

          rebuild_tree_list(&root, &tree_list, &tree_size);
          rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);

          // Fix the tree list focus
          for(i = 0; i < tree_size; i++)
          {
            if(!strcmp(tree_list[i] + TREE_LIST_WIDTH, focus->name))
            {
              node_selected = i;
              node_scroll_offset = node_selected - (TREE_LIST_HEIGHT/2);
              break;
            }
          }
          // Fix label name
          label[0] = '\0';
          get_node_name(focus, label, 80);

          window_focus = 0;
        }
        break;
      }

      // Toggle Empties
      case 4:
      {
        char *current = NULL;
        hide_empty_vars = !hide_empty_vars;

        if(num_vars)
          current = var_list[var_selected];

        rebuild_var_list(focus, &var_list, &num_vars, hide_empty_vars);
        var_selected = 0;

        for(i = 0; i < num_vars; i++)
        {
          if(var_list[i] == current)
          {
            var_selected = i;
            break;
          }
        }

        label[0] = '\0';
        get_node_name(focus, label, 80);

        window_focus = 4;
        break;
      }

      // Export
      case 5:
      {
        char export_name[MAX_PATH];
        const char *txt_ext[] = { ".TXT", NULL };

        export_name[0] = 0;

        if(!new_file(mzx_world, txt_ext, ".txt", export_name,
         "Export counters/strings text file...", 1))
        {
          FILE *fp;

          fp = fopen_unsafe(export_name, "wb");

          for(i = 0; i < mzx_world->num_counters; i++)
          {
            fprintf(fp, "set \"%s\" to %d\n",
             mzx_world->counter_list[i]->name,
             mzx_world->counter_list[i]->value);
          }

          for(i = 0; i < mzx_world->num_strings; i++)
          {
            fprintf(fp, "set \"%s\" to \"",
             mzx_world->string_list[i]->name);

            fwrite(mzx_world->string_list[i]->value,
             mzx_world->string_list[i]->length, 1, fp);

            fprintf(fp, "\"\n");
          }

          fclose(fp);
        }

        window_focus = 5;
        break;
      }
    }
    if(focus->refresh_on_focus)
      for(i = 0; i < focus->num_counters; i++)
        read_var(mzx_world, focus->counters[i]);

    // If the debug menu was just reopened, reset searching
    if(reopened)
    {
      search_text[0] = 0;
      search_flags = VAR_SEARCH_NAMES + VAR_SEARCH_VALUES + VAR_SEARCH_WRAP;
      reopened = 0;
      // Also don't destruct the (uninitialized) dialog
      continue;
    }

    destruct_dialog(&di);

  } while(dialog_result != -1);

  m_hide();

  pop_context();

  // Make sure the robot debugger watchpoints have updated values
  update_watchpoint_last_values(mzx_world);

  // Copy the last selected var to the previous field.
  if(var_selected < num_vars)
  {
    strncpy(previous_var, var_list[var_selected] + VAR_LIST_VAR, VAR_SEARCH_MAX);
    strcpy(previous_node_name, focus->name);
  }

  // Clear the big dumb tree first
  clear_debug_node(&root, true);

  // Get rid of the tree view
  free_tree_list(tree_list, tree_size);

  // We don't need to free anything this list points
  // to because clear_debug_tree already did it.
  free(var_list);

  // Prevent UI keys from carrying through.
  force_release_all_keys();
}

/************************************************/
/* Debug box, which shows actually useful stuff */
/************************************************/

void __draw_debug_box(struct world *mzx_world, int x, int y, int d_x, int d_y,
 int show_keys)
{
  struct board *src_board = mzx_world->current_board;
  char version_string[16];
  int version_string_len;
  char key_string[4];
  int key;
  int robot_mem = 0;
  int i;

  draw_window_box(x, y, x + 19, y + 5, DI_DEBUG_BOX, DI_DEBUG_BOX_DARK,
   DI_DEBUG_BOX_CORNER, 0, 1);

  write_string
  (
    "X/Y:        /     \n"
    "Board:            \n"
    "Robot mem:      kb\n",
    x + 1, y + 1, DI_DEBUG_LABEL, 0
  );

  version_string_len = get_version_string(version_string, mzx_world->version);
  write_string(version_string, x + 19 - version_string_len, y, DI_DEBUG_BOX, 0);

  if(show_keys)
  {
    // key_code
    key = get_last_key(keycode_pc_xt);
    if(key && show_keys)
    {
      sprintf(key_string, "%d", key);
      write_string(key_string, x + 15 - strlen(key_string), y + 5,
       DI_DEBUG_BOX_DARK + 0x0A, 0);
    }

    // key_pressed

    // In 2.82X through 2.84X, this erroneously applied
    // numlock translations to the keycode.
    if(mzx_world->version >= V282 && mzx_world->version <= V284)
      key = get_last_key(keycode_internal_wrt_numlock);

    else
      key = get_last_key(keycode_internal);

    if(key && show_keys)
    {
      sprintf(key_string, "%d", key);
      write_string(key_string, x + 19 - strlen(key_string), y + 5,
       DI_DEBUG_BOX_DARK + 0x0D, 0);
    }
  }

  write_number(d_x, DI_DEBUG_NUMBER, x + 8, y + 1, 5, 0, 10);
  write_number(d_y, DI_DEBUG_NUMBER, x + 14, y + 1, 5, 0, 10);
  write_number(mzx_world->current_board_id, DI_DEBUG_NUMBER,
   x + 18, y + 2, 0, 1, 10);

  for(i = 0; i < src_board->num_robots_active; i++)
  {
    robot_mem +=
#ifdef CONFIG_DEBYTECODE
     (src_board->robot_list_name_sorted[i])->program_source_length;
#else
     (src_board->robot_list_name_sorted[i])->program_bytecode_length;
#endif
  }

  write_number((robot_mem + 512) / 1024, DI_DEBUG_NUMBER,
   x + 12, y + 3, 5, 0, 10);

  if(*(src_board->mod_playing) != 0)
  {
    if(strlen(src_board->mod_playing) > 18)
    {
      char tempc = src_board->mod_playing[18];
      src_board->mod_playing[18] = 0;
      write_string(src_board->mod_playing, x + 1, y + 4,
       DI_DEBUG_NUMBER, 0);
      src_board->mod_playing[18] = tempc;
    }
    else
    {
      write_string(src_board->mod_playing, x + 1, y + 4,
       DI_DEBUG_NUMBER, 0);
    }
  }
  else
  {
    write_string("(no module)", x + 2, y + 4, DI_DEBUG_NUMBER, 0);
  }
}
