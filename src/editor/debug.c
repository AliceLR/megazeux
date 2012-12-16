/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "debug.h"

#include "../graphics.h"
#include "../counter.h"
#include "../sprite.h"
#include "../window.h"
#include "../intake.h"
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

#define BUTTONS_X 66
#define BUTTONS_Y 18

#define CVALUE_SIZE 11
#define SVALUE_SIZE 44
#define CVALUE_COL_OFFSET (VAR_LIST_WIDTH - CVALUE_SIZE - 1)
#define SVALUE_COL_OFFSET (VAR_LIST_WIDTH - SVALUE_SIZE - 1)

char asc[17] = "0123456789ABCDEF";

static void copy_substring_escaped(struct string *str, char *buf,
 unsigned int size)
{
  unsigned int i, j;

  for(i = 0, j = 0; j < str->length && i < size; i++, j++)
  {
    if(str->value[j] == '\\')
    {
      buf[i++] = '\\';
      buf[i] = '\\';
    }
    else if(str->value[j] == '\n')
    {
      buf[i++] = '\\';
      buf[i] = 'n';
    }
    else if(str->value[j] == '\r')
    {
      buf[i++] = '\\';
      buf[i] = 'r';
    }
    else if(str->value[j] == '\t')
    {
      buf[i++] = '\\';
      buf[i] = 't';
    }
    else if(str->value[j] < 32 || str->value[j] > 126)
    {
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
  size_t i, j, old_len = strlen(buf);

  for(i = 0, j = 0; j < old_len + 1; i++, j++)
  {
    if(buf[j] != '\\')
    {
      buf[i] = buf[j];
      continue;
    }

    j++;

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
      char t = buf[j + 3];
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
int num_world_vars = 16;
const char world_var_list[16][20] = {
  "bimesg", //no read
  "commands",
  "divider",
  "fread_delimiter", //no read
  "fwrite_delimiter", //no read
  "mod_frequency",
  "mod_name*",
  "mod_order",
  "mod_position",
  "multiplier",
  "mzx_speed",
  "smzx_mode*",
  "spacelock", //no read
  "vlayer_size*",
  "vlayer_height*",
  "vlayer_width*",
  };
int num_board_vars = 11;
const char board_var_list[11][20] = {
  "board_name*",
  "board_h*",
  "board_w*",
  "input*",
  "inputsize*",
  "playerfacedir",
  "playerlastdir",
  "playerx*",
  "playery*",
  "scrolledx*",
  "scrolledy*",
};
// Locals are added onto the end later.
int num_robot_vars = 11;
const char robot_var_list[11][12] = {
  "robot_name*",
  "bullettype",
  "lava_walk",
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
int num_sprite_vars = 15;
const char sprite_var_list[15][10] = {
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
  "ccheck",
  "off",
  "overlaid",
  "static",
  "vlayer",
};

static void read_var(struct world *mzx_world, char *var_buffer)
{
  int int_value = 0;
  char *char_value = NULL;

  // Var info is stored after the visible portion of the buffer
  char *var = var_buffer + VAR_LIST_WIDTH;

  if(var[0])
  {
    if(var[0] == '$')
    {
      struct string temp;
      if(!get_string(mzx_world, var, &temp, 0))
        return;
      char_value = temp.value;
      int_value = temp.length;
    }
    else
    {
      int_value = get_counter(mzx_world, var, 0);
    }
  }
  else
  // It's a built-in var.  Since some of these don't have
  // read functions, we have to map several manually.
  {
    size_t real_len = strlen(var + 2);
    char *real_var = cmalloc(real_len + 1);
    int index = var[1];

    strncpy(real_var, var + 2, real_len);
    real_var[real_len] = 0;

    if(real_var[real_len - 1] == '*')
      real_var[real_len - 1] = 0;

    if(!strcmp(real_var, "bimesg"))
      int_value = mzx_world->bi_mesg_status;
    else
    if(!strcmp(real_var, "spacelock"))
      int_value = mzx_world->bi_shoot_status;
    else
    if(!strcmp(real_var, "fread_delimiter"))
      int_value = mzx_world->fread_delimiter;
    else
    if(!strcmp(real_var, "fwrite_delimiter"))
      int_value = mzx_world->fwrite_delimiter;
    else
    if(!strcmp(real_var, "mod_name"))
      char_value = mzx_world->real_mod_playing;
    else
    if(!strcmp(real_var, "board_name"))
      char_value = mzx_world->current_board->board_name;
    else
    if(!strcmp(real_var, "robot_name"))
      char_value = mzx_world->current_board->robot_list[index]->robot_name;
    else
    if(!strncmp(real_var, "spr", 3))
    {
      char *sub_var;
      int sprite_num = strtol(real_var + 3, &sub_var, 10) & (MAX_SPRITES - 1);

      if(!strcmp(sub_var, "_off"))
      {
        if(!(mzx_world->sprite_list[sprite_num]->flags & SPRITE_INITIALIZED))
          int_value = 1;
      }
      else
      if(!strcmp(sub_var, "_overlaid"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_OVER_OVERLAY)
          int_value = 1;
      }
      else
      if(!strcmp(sub_var, "_static"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_STATIC)
          int_value = 1;
      }
      else
      if(!strcmp(sub_var, "_vlayer"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_VLAYER)
          int_value = 1;
      }
      else
      if(!strcmp(sub_var, "_ccheck"))
      {
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_CHAR_CHECK)
          int_value = 1;
        else
        if(mzx_world->sprite_list[sprite_num]->flags & SPRITE_CHAR_CHECK2)
          int_value = 2;
      }
      else
        int_value = get_counter(mzx_world, real_var, index);
    }
    else
      int_value = get_counter(mzx_world, real_var, index);

    free(real_var);
  }

  if(char_value)
    memcpy(var_buffer + SVALUE_COL_OFFSET, char_value, MIN(SVALUE_SIZE, int_value));
  else
    snprintf(var_buffer + CVALUE_COL_OFFSET, CVALUE_SIZE, "%i", int_value);
}

static void write_var(struct world *mzx_world, char *var_buffer, int int_val, char *char_val)
{
  char *var = var_buffer + VAR_LIST_WIDTH;

  if(var[0])
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
    size_t real_len = strlen(var + 2);
    char *real_var = cmalloc(real_len + 1);
    int index = var[1];

    strncpy(real_var, var + 2, real_len);
    real_var[real_len] = 0;

    if(real_var[real_len - 1] != '*')
      set_counter(mzx_world, real_var, int_val, index);

    free(real_var);
  }

  // Now update var_buffer to reflect the new value.
  read_var(mzx_world, var_buffer);
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
  char *name;
  int i;

  // Skip empty nodes entirely
  if(node->num_nodes == 0 && node->num_counters == 0)
    return;

  if(level > 0)
  {
    // Display name and a real name so the menu can find it later.
    name = cmalloc(TREE_LIST_WIDTH + strlen(node->name) + 1);
    memset(name, ' ', level * 2);
    if(node->num_nodes)
    {
      if(node->opened)
        name[(level-1) * 2] = '-';
      else
        name[(level-1) * 2] = '+';
    }

    // Display name
    strncpy(name + level * 2, node->name, TREE_LIST_WIDTH - level * 2);
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

// From a node, build the list of all counters that should appear
// This list is just built out of pointers to the tree, so it
// should never have its contents freed.
static void build_var_list(struct debug_node *node,
 char ***var_list, int *num_vars)
{
  if(node->num_counters)
  {
    size_t vars_size = (*num_vars) * sizeof(char *);
    size_t added_size = node->num_counters * sizeof(char *);

    debug("%i %i %i %i\n", vars_size, added_size, (*num_vars), node->num_counters);

    (*var_list) = crealloc(*var_list, vars_size + added_size);

    memcpy((*var_list) + *num_vars, node->counters, added_size);
    (*num_vars) += node->num_counters;
  }

  if(node->num_nodes && node->show_child_contents)
    for(int i = 0; i < node->num_nodes; i++)
      build_var_list(&(node->nodes[i]), var_list, num_vars);

}

// int_value is the char_value's length when a char_value is present.
static void build_var_buffer(char **var_buffer, const char *name,
 int int_value, char *char_value, int index)
{
  char *var;

  if(index == -1)
  {
    (*var_buffer) = cmalloc(VAR_LIST_WIDTH + strlen(name) + 1);
    var = (*var_buffer) + VAR_LIST_WIDTH;
  }
  else
  {
    (*var_buffer) = cmalloc(VAR_LIST_WIDTH + 2 + strlen(name) + 1);
    (*var_buffer)[VAR_LIST_WIDTH] = '\0';
    (*var_buffer)[VAR_LIST_WIDTH + 1] = (char)index;
    var = (*var_buffer) + VAR_LIST_WIDTH + 2;
  }

  // Display
  memset((*var_buffer), ' ', VAR_LIST_WIDTH);
  strncpy((*var_buffer), name, strlen(name));
  (*var_buffer)[VAR_LIST_WIDTH - 1] = '\0';

  // Internal
  strcpy(var, name);

  if(char_value)
    memcpy((*var_buffer) + SVALUE_COL_OFFSET, char_value, MIN(SVALUE_SIZE, int_value));
  else
    snprintf((*var_buffer) + CVALUE_COL_OFFSET, CVALUE_SIZE, "%i", int_value);
}

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

// Free the tree list and all of its lines.
static void free_tree_list(char **tree_list, int tree_size)
{
  for(int i = 0; i < tree_size; i++)
    free(tree_list[i]);

  free(tree_list);
}



/**********************************/
/******* MAKE THE TREE CODE *******/
/**********************************/

// targ is a pointer to list size (num) of anything size (size_of_1)
static void merge_sort_pp(const void *targ, size_t num, size_t size_of_1,
 int(* fcn)(const void *a, const void *b))
{
  unsigned int i, a, b, j, w;
  char *t = (char *)targ, *A, *B, *buf = cmalloc(num * size_of_1);

  for(w = 1; w < num; w *= 2) {
    for(i = 0; i < num; i += 2 * w) {
      for(a = i, b = i + w, j = i; j < i + 2 * w && j < num; j++) {
        A = t + a * size_of_1;
        B = t + b * size_of_1;
        if((b < MIN(i + 2 * w, num)) && ((a >= i + w) || (fcn((void *)A, (void *)B) > 0)))
        {
          memcpy(buf + j, B, size_of_1);
          b++;
        }
        else
        {
          memcpy(buf + j, A, size_of_1);
          a++;
        }
      }
      memcpy(t + i * size_of_1, buf, w * 2 * size_of_1);
    }
  }

  free(buf);
}


static int counter_sort_fcn(const void *a, const void *b)
{
  return strcasecmp(
   (*(const struct counter **)a)->name,
   (*(const struct counter **)b)->name);
}
static int string_sort_fcn(const void *a, const void *b)
{
  return strcasecmp(
   (*(const struct string **)a)->name,
   (*(const struct string **)b)->name);
}

// Create new counter lists.
// (Re)make the child nodes
static void repopulate_tree(struct world *mzx_world, struct debug_node *root)
{
  int i, j, n, *num, alloc;

  char ***list;
  char var[20] = { 0 };

  char name[28] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  int num_counters = mzx_world->num_counters;
  int num_strings = mzx_world->num_strings;

  struct debug_node *counters = &(root->nodes[0]);
  struct debug_node *strings = &(root->nodes[1]);
  struct debug_node *sprites = &(root->nodes[2]);
  struct debug_node *world = &(root->nodes[3]);
  struct debug_node *board = &(root->nodes[4]);
  struct debug_node *robots = &(root->nodes[5]);

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

  qsort(
  //merge_sort_pp(
   mzx_world->counter_list, (size_t)num_counters, sizeof(struct counter *),
   counter_sort_fcn);
  qsort(
  //merge_sort_pp(
   mzx_world->string_list, (size_t)num_strings, sizeof(struct string *),
   string_sort_fcn);

  // We want to start off on a clean slate here
  clear_debug_node(counters, true);
  clear_debug_node(strings, true);
  clear_debug_node(sprites, true);
  clear_debug_node(world, true);
  clear_debug_node(board, true);
  clear_debug_node(robots, true);

  /************/
  /* Counters */
  /************/

  for(i = 0, n = 0; i < num_counters; i++)
  {
    char first = toupper(mzx_world->counter_list[i]->name[0]);
    // We need to switch child nodes
    if((first > n+64 && n<27) || i == 0)
    {
      if(first < 'A')
        n = 0;
      else if(first > 'Z')
        n = 27;
      else
        n = (int)first - 64;
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

  // IMPORTANT: Make sure we reset each string's list_ind since we just
  // sorted them, it's only polite (and also will make MZX not crash)
  for(i = 0, n = 0; i < num_strings; i++)
  {
    char first = toupper(mzx_world->string_list[i]->name[1]);
    mzx_world->string_list[i]->list_ind = i;

    // We need to switch child nodes
    if((first > n+64 && n<27) || i == 0)
    {
      if(first < 'A')
        n = 0;
      else if(first > 'Z')
        n = 27;
      else
        n = (int)first - 64;
      num = &(string_nodes[n % 27].num_counters);
      list = &(string_nodes[n % 27].counters);
      alloc = *num;
    }

    if(*num == alloc)
      *list = crealloc(*list, (alloc = MAX(alloc * 2, 32)) * sizeof(char *));

    build_var_buffer( &(*list)[*num], mzx_world->string_list[i]->name,
     mzx_world->string_list[i]->length, mzx_world->string_list[i]->value, -1);

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
       crealloc(counter_nodes[i].counters, counter_nodes[i].num_counters * sizeof(char *));
    }
  }
  // And finish strings
  strings->num_nodes = num_string_nodes;
  strings->nodes = string_nodes;

  /***********/
  /* Sprites */
  /***********/

  sprites->counters = ccalloc(mzx_world->collision_count + 2, sizeof(char *));

  build_var_buffer( &(sprites->counters)[0], "spr_yorder", mzx_world->sprite_y_order, NULL, 0);
  build_var_buffer( &(sprites->counters)[1], "spr_collisions*", mzx_world->collision_count, NULL, 0);

  for(i = 0; i < mzx_world->collision_count; i++)
  {
    snprintf(var, 20, "spr_collision%i*", i);
    build_var_buffer( &(sprites->counters)[i + 2], var,
     mzx_world->collision_list[i], NULL, 0);
  }
  sprites->num_counters = i + 2;

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

  // And finish sprites

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
    struct robot *robot;
    if(i == 0)
       robot = &(mzx_world->global_robot);
    else
       robot = robot_list[i - 1];
    
    if(!robot)
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
       robot->local[n], NULL, i&255);
    }

    snprintf(var, 14, "%i:%s", i, robot_list[i]->robot_name);
    strcpy(robot_nodes[j].name, var);
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
  int num_root_nodes = 6;
  struct debug_node *root_nodes =
   ccalloc(num_root_nodes, sizeof(struct debug_node));

  struct debug_node root_node =
  {
    "ROOT",
    true,
    false,
    num_root_nodes,
    0,
    NULL,
    root_nodes,
    NULL
  };

  struct debug_node counters = {
    "Counters",
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
    0,
    0,
    root,
    NULL,
    NULL
  };

  struct debug_node world = {
    "World",
    false,
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
  memcpy(&root_nodes[3], &world, sizeof(struct debug_node));
  memcpy(&root_nodes[4], &board, sizeof(struct debug_node));
  memcpy(&root_nodes[5], &robots, sizeof(struct debug_node));

  repopulate_tree(mzx_world, root);
}


/**********************
 * It's morphin' time *
 **********************/

// The editor will have to clear_debug_node
// this thing after testing ends.
struct debug_node root;
struct board *debug_board = NULL;

// build_debug_tree
// build_tree_list
// build_var_list
// (create dialog)
// run dialog:
//   If a tree is chosen (expanded or retracted):
//     free_tree_list
//     build_tree_list
//   If a new tree node is selected:
//     build_var_list
//   If a var is chosen:
//     input dialog
//     write_var
// clear_debug_node(root, true)
// free_tree_list
// free(var_list), all members are already freed by clear_debug_node

void __debug_counters(struct world *mzx_world)
{
  int i;

  int num_vars = 0, tree_size = 0;
  char **var_list = NULL, **tree_list = NULL;

  int window_focus = 0;
  int node_selected = 0, var_selected = 0;
  int dialog_result;
  struct dialog di;
  struct element *elements[5];

  m_show();

  // also known as crash_stack
  build_debug_tree(mzx_world, &root);

  build_tree_list(&root, &tree_list, &tree_size, 0);

  build_var_list(&(root.nodes[node_selected]), &var_list, &num_vars);

  do
  {
    int last_node_selected = node_selected;

    elements[0] = construct_list_box(
     VAR_LIST_X, VAR_LIST_Y, (const char **)var_list, num_vars,
     VAR_LIST_HEIGHT, VAR_LIST_WIDTH, 1, &var_selected, false);
    elements[1] = construct_list_box(
     TREE_LIST_X, TREE_LIST_Y, (const char **)tree_list, tree_size,
     TREE_LIST_HEIGHT, TREE_LIST_WIDTH, 2, &node_selected, false);
    elements[2] = construct_button(BUTTONS_X + 0, BUTTONS_Y + 0, "Search", 3);
    elements[3] = construct_button(BUTTONS_X + 0, BUTTONS_Y + 2, "Export", 4);
    elements[4] = construct_button(BUTTONS_X + 1, BUTTONS_Y + 4, "Done", -1);

    construct_dialog_ext(&di, "Debug Variables", 0, 0,
     80, 25, elements, 5, 0, 0, window_focus, NULL);

    dialog_result = run_dialog(mzx_world, &di);

    switch(dialog_result)
    {
      // Tree node
      case 2:
      {
        // The real node name is in the tree list beyond where it will display.
        struct debug_node *focus =
         find_node(&root, tree_list[node_selected] + TREE_LIST_WIDTH);

        // Switch to a new view
        if(last_node_selected != node_selected)
        {
          free(var_list);
          var_list = NULL;
          num_vars = 0;

          build_var_list(focus, &var_list, &num_vars);

          dialog_result = 0; //nothing to see here
          var_selected = 0;
        }
        else
        // Collapse/Expand selected view if applicable
        if(focus->num_nodes)
        {
          focus->opened = !(focus->opened);

          // Get rid of the old one
          free_tree_list(tree_list, tree_size);
          tree_list = NULL;
          tree_size = 0;

          // ...and make a new one.
          build_tree_list(&root, &tree_list, &tree_size, 0);
        }
        window_focus = 1; // Tree list
        break;
      }

      // Var list
      case 1:
      {
        // We keep the actual var name hidden here.
        char *var = var_list[var_selected] + VAR_LIST_WIDTH;

        char new_value[70];
        char name[70] = { 0 };
        int id = 0;

        window_focus = 0; // Var list

        if(var[0] == '$')
        {
          int offset;
          struct string temp;
          get_string(mzx_world, var, &temp, 0);
          offset = temp.list_ind;

          snprintf(name, 69, "Edit: string %s", var);

          copy_substring_escaped(&temp, new_value, 68);
        }
        else
        {
          if(var[0])
          {
            snprintf(name, 69, "Edit: counter %s", var);
          }
          else
          {
            id = var[1];
            var += 2;

            if(var[strlen(var)-1] == '*')
              break;

            snprintf(name, 69, "Edit: variable %s", var);
          }

          sprintf(new_value, "%d", get_counter(mzx_world, var, id));
        }

        save_screen();
        draw_window_box(4, 12, 76, 14, DI_DEBUG_BOX, DI_DEBUG_BOX_DARK,
         DI_DEBUG_BOX_CORNER, 1, 1);
        write_string(name, 6, 12, DI_DEBUG_LABEL, 0);

        if(intake(mzx_world, new_value, 68, 6, 13, 15, 1, 0,
         NULL, 0, NULL) != IKEY_ESCAPE)
        {
          if(var[0] == '$')
          {
            int len;
            unescape_string(new_value, &len);
            write_var(mzx_world, var_list[var_selected], len, new_value);
          }
          else
          {
            int counter_value = strtol(new_value, NULL, 10);
            write_var(mzx_world, var_list[var_selected], counter_value, NULL);
          }
        }

        restore_screen();
        break;
      }

      // Search
      case 3:
      {
        window_focus = 2; // Search button
        break;
      }

      // Export
      case 4:
      {
        char export_name[MAX_PATH];
        const char *txt_ext[] = { ".TXT", NULL };

        export_name[0] = 0;

        if(!new_file(mzx_world, txt_ext, ".txt", export_name,
         "Export counters/strings", 1))
        {
          FILE *fp;

          fp = fopen_unsafe(export_name, "wb");

          for(i = 0; i < mzx_world->num_counters; i++)
          {
            fprintf(fp, "set \"%s\" to %d\n",
             mzx_world->counter_list[i]->name,
             mzx_world->counter_list[i]->value);
          }

          fprintf(fp, "set \"mzx_speed\" to %d\n", mzx_world->mzx_speed);

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

        window_focus = 0; //Var list
        break;
      }
    }

    destruct_dialog(&di);

  } while(dialog_result != -1);

  m_hide();

  // Clear the big dumb tree first
  clear_debug_node(&root, true);

  // Get rid of the tree view
  free_tree_list(tree_list, tree_size);

  // We don't need to free anything this list points
  // to because clear_debug_tree already did it.
  free(var_list);
}




/********************/
/* HAHAHA DEBUG BOX */
/********************/

void __draw_debug_box(struct world *mzx_world, int x, int y, int d_x, int d_y)
{
  struct board *src_board = mzx_world->current_board;
  int i;
  int robot_mem = 0;

  draw_window_box(x, y, x + 19, y + 5, DI_DEBUG_BOX, DI_DEBUG_BOX_DARK,
   DI_DEBUG_BOX_CORNER, 0, 1);

  write_string
  (
    "X/Y:        /     \n"
    "Board:            \n"
    "Robot mem:      kb\n",
    x + 1, y + 1, DI_DEBUG_LABEL, 0
  );

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
