/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "const.h"
#include "game.h"
#include "game2.h"
#include "idarray.h"
#include "graphics.h"
#include "event.h"
#include "expr.h"
#include "window.h"
#include "scrdisp.h"
#include "robot.h"
#include "board.h"
#include "world.h"
#include "util.h"
#include "rasm.h"

static int cmp_labels(const void *dest, const void *src)
{
  struct label *lsrc = *((struct label **)src);
  struct label *ldest = *((struct label **)dest);
  int cmp_primary = strcasecmp(ldest->name, lsrc->name);

  // A match needs to go on a secondary criteria.
  if(!cmp_primary)
  {
    if(ldest->position == 0)
      return 1;

    return ldest->position - lsrc->position;
  }
  else
  {
    return cmp_primary;
  }
}

// TODO: If bytecode isn't valid then this is done at a bad time. It should
// really be done when robots are assembled, rather than when they're loaded.
// So it's bundled with the function for that.

#ifdef CONFIG_DEBYTECODE
static
#endif
struct label **cache_robot_labels(struct robot *robot, int *num_labels)
{
  int labels_allocated = 16;
  int labels_found = 0;
  int cmd;
  int next;
  int i;

  char *robot_program = robot->program_bytecode;
  struct label **label_list = ccalloc(16, sizeof(struct label *));
  struct label *current_label;

  for(i = 1; i < (robot->program_bytecode_length - 1); i++)
  {
    // Is it a label?
    cmd = robot_program[i + 1];
    next = i + robot_program[i] + 1;

    if((cmd == ROBOTIC_CMD_LABEL) || (cmd == ROBOTIC_CMD_ZAPPED_LABEL))
    {
      current_label = cmalloc(sizeof(struct label));
      current_label->name = robot_program + i + 3;

      if(next >= (robot->program_bytecode_length - 2))
        current_label->position = 0;
      else
        current_label->position = i;

      if(cmd == ROBOTIC_CMD_ZAPPED_LABEL)
        current_label->zapped = 1;
      else
        current_label->zapped = 0;

      // Do we need more room?
      if(labels_found == labels_allocated)
      {
        labels_allocated *= 2;
        label_list = crealloc(label_list,
         sizeof(struct label *) * labels_allocated);
      }
      label_list[labels_found] = current_label;
      labels_found++;
    }

    // Go to next command
    i = next;
  }

  if(!labels_found)
  {
    *num_labels = 0;
    free(label_list);
    return NULL;
  }

  if(labels_found != labels_allocated)
  {
    label_list =
     crealloc(label_list, sizeof(struct label *) * labels_found);
  }

  // Now sort the list
  qsort(label_list, labels_found, sizeof(struct label *), cmp_labels);

  *num_labels = labels_found;
  return label_list;
}

int get_robot_id(struct board *src_board, const char *name)
{
  int first, last;

  if(find_robot(src_board, name, &first, &last))
  {
    struct robot *cur_robot = src_board->robot_list_name_sorted[first];
    // This is a cheap trick for now since robots don't have
    // a back-reference for ID's
    int offset = cur_robot->xpos +
     (cur_robot->ypos * src_board->board_width);
    enum thing d_id = (enum thing)src_board->level_id[offset];

    if(is_robot(d_id))
    {
      return src_board->level_param[offset];
    }
    else
    {
      int i;
      for(i = 1; i <= src_board->num_robots; i++)
      {
        if(!src_board->robot_list[i])
          continue;
        if(!strcasecmp(name, (src_board->robot_list[i])->robot_name))
          return i;
      }
    }
  }

  return -1;
}

struct robot *load_robot_allocate(FILE *fp, int savegame, int version)
{
  struct robot *cur_robot = cmalloc(sizeof(struct robot));
  load_robot(cur_robot, fp, savegame, version);
  return cur_robot;
}

// Most of this stuff does not have to be loaded unless savegame
// is set.
void load_robot(struct robot *cur_robot, FILE *fp, int savegame, int version)
{
  int program_length;
  int i;

#ifdef CONFIG_DEBYTECODE
  if(version >= VERSION_PROGRAM_SOURCE)
  {
    program_length = fgetd(fp);
  }
  else
#endif
  {
    program_length = fgetw(fp);
    fseek(fp, 2, SEEK_CUR);
  }

  fread(cur_robot->robot_name, 15, 1, fp);
  cur_robot->robot_char = fgetc(fp);
  cur_robot->cur_prog_line = fgetw(fp);
  cur_robot->pos_within_line = fgetc(fp);
  cur_robot->robot_cycle = fgetc(fp);
  cur_robot->cycle_count = fgetc(fp);
  cur_robot->bullet_type = fgetc(fp);
  cur_robot->is_locked = fgetc(fp);
  cur_robot->can_lavawalk = fgetc(fp);
  cur_robot->walk_dir = (enum dir)fgetc(fp);
  cur_robot->last_touch_dir = (enum dir)fgetc(fp);
  cur_robot->last_shot_dir = (enum dir)fgetc(fp);
  cur_robot->xpos = fgetw(fp);
  cur_robot->ypos = fgetw(fp);
  cur_robot->status = fgetc(fp);
  // Skip local - these are in the save files now
  fseek(fp, 2, SEEK_CUR);
  cur_robot->used = fgetc(fp);
  cur_robot->loop_count = fgetw(fp);

  // If savegame, there's some additional information to get
  if(savegame)
  {
    int stack_size;

    for(i = 0; i < 32; i++)
      cur_robot->local[i] = fgetd(fp);

    stack_size = fgetd(fp);
    cur_robot->stack_pointer = fgetd(fp);

    cur_robot->stack = ccalloc(stack_size, sizeof(int));
    for(i = 0; i < stack_size; i++)
      cur_robot->stack[i] = fgetd(fp);

    cur_robot->stack_size = stack_size;

#ifdef CONFIG_DEBYTECODE
    // We're allowing legacy save robots to be loaded because lancer-x is
    // a bad person and relies on savegame type MZMs (even though he claims
    // to have made them in the editor)

    if(version < VERSION_PROGRAM_SOURCE)
    {
      // The program is bytecode and we have to convert it to sourcecode.
      char *program_legacy_bytecode = cmalloc(program_length);

      fread(program_legacy_bytecode, program_length, 1, fp);

      cur_robot->program_bytecode = NULL;
      cur_robot->program_source =
       legacy_disassemble_program(program_legacy_bytecode, program_length,
       &(cur_robot->program_source_length), true, 10);

      free(program_legacy_bytecode);
      prepare_robot_bytecode(cur_robot);

      // And if you thought that lancer-x was a bad person for relying
      // on in-game saved MZMs then you should also know that these
      // these had robots that MZMs weren't at start position. Fortunately,
      // they were working more in spite of it than because of it, so
      // they're safe to nudge back.

      if(cur_robot->cur_prog_line)
        cur_robot->cur_prog_line = 1;
    }
    else
    {
      // Save file loads bytecode. This also means that we can't ever
      // ever edit save files, so make them null. In practice this
      // shouldn't be possible though.
      cur_robot->program_bytecode = cmalloc(program_length);
      cur_robot->program_bytecode_length = program_length;
      fread(cur_robot->program_bytecode, program_length, 1, fp);
    }

    cur_robot->program_source = NULL;
    cur_robot->program_source_length = 0;

    // TODO: This has to be made part of what's saved one day.
    cur_robot->label_list =
     cache_robot_labels(cur_robot, &cur_robot->num_labels);
#endif /* CONFIG_DEBYTECODE */
  }
  else
  {
    // Otherwise, allocate some stuff; local counters are 0
    memset(cur_robot->local, 0, sizeof(int) * 32);

    // Give an empty stack.
    cur_robot->stack_size = 0;
    cur_robot->stack = NULL;

    // Initialize the stack pointer to the bottom
    cur_robot->stack_pointer = 0;

#ifdef CONFIG_DEBYTECODE
    // World file loads source code.
    if(version < VERSION_PROGRAM_SOURCE)
    {
      if((cur_robot->used) || (program_length >= 2))
      {
        // The program is bytecode and we have to convert it to sourcecode.
        char *program_legacy_bytecode = cmalloc(program_length);
        fread(program_legacy_bytecode, program_length, 1, fp);

        // All programs length 2 are supposed to be blank, 0xFF 0x0. But
        // there's a corruption in Catacombs - this fixes that. Also somehow
        // corrupt global robots that don't start with 0xFF.

        if((program_length == 2) || (program_legacy_bytecode[0] != 0xFF))
        {
          program_legacy_bytecode[0] = 0xFF;
          program_legacy_bytecode[1] = 0x0;
        }

        cur_robot->program_source =
         legacy_disassemble_program(program_legacy_bytecode, program_length,
         &(cur_robot->program_source_length), true, 10);
        free(program_legacy_bytecode);
      }
      else
      {
        // Skip over program, we're not going to use it.
        fseek(fp, program_length, SEEK_CUR);
        cur_robot->program_source = NULL;
        cur_robot->program_source_length = 0;
      }
    }
    else
    {
      // This program is source, just load it straight
      cur_robot->program_source = cmalloc(program_length + 1);
      cur_robot->program_source_length = program_length;
      fread(cur_robot->program_source, program_length, 1, fp);
      cur_robot->program_source[program_length] = 0;
    }

    // This will become non-null when the robot's executed.
    cur_robot->program_bytecode = NULL;
    cur_robot->program_bytecode_length = 0;
#endif /* CONFIG_DEBYTECODE */
  }

#ifndef CONFIG_DEBYTECODE
  cur_robot->program_bytecode = cmalloc(program_length);
  cur_robot->program_bytecode_length = program_length;
  fread(cur_robot->program_bytecode, program_length, 1, fp);

  // Now create a label cache IF the robot is in use
  if(cur_robot->used)
  {
    cur_robot->label_list =
     cache_robot_labels(cur_robot, &cur_robot->num_labels);
  }
#endif /* !CONFIG_DEBYTECODE */
}

static void robot_stack_push(struct robot *cur_robot, int value)
{
  int stack_pointer = cur_robot->stack_pointer;
  int stack_size = cur_robot->stack_size;
  int *stack = cur_robot->stack;

  if((stack_pointer + 1) >= stack_size)
  {
    // Initialize or double the stack. Don't let it get too large though!
    if(stack_size == 0)
      stack_size = 1;
    else
      stack_size *= 2;

    if(stack_size > ROBOT_MAX_STACK)
      return;

    cur_robot->stack = crealloc(stack, stack_size * sizeof(int));
    stack = cur_robot->stack;
    cur_robot->stack_size = stack_size;
  }

  stack[stack_pointer] = value;
  cur_robot->stack_pointer = stack_pointer + 1;
}

static int robot_stack_pop(struct robot *cur_robot)
{
  int stack_pointer = cur_robot->stack_pointer;

  if(stack_pointer)
  {
    stack_pointer--;
    cur_robot->stack_pointer = stack_pointer;
    return cur_robot->stack[stack_pointer];
  }
  else
  {
    return -1;
  }
}

static void load_scroll(struct scroll *cur_scroll, FILE *fp)
{
  int scroll_size;

  cur_scroll->num_lines = fgetw(fp);
  // Skip junk
  fseek(fp, 2, SEEK_CUR);
  scroll_size = fgetw(fp);
  cur_scroll->mesg_size = scroll_size;
  cur_scroll->used = fgetc(fp);

  cur_scroll->mesg = cmalloc(scroll_size);
  fread(cur_scroll->mesg, scroll_size, 1, fp);
}

struct scroll *load_scroll_allocate(FILE *fp)
{
  struct scroll *cur_scroll = cmalloc(sizeof(struct scroll));
  load_scroll(cur_scroll, fp);
  return cur_scroll;
}

static void load_sensor(struct sensor *cur_sensor, FILE *fp)
{
  fread(cur_sensor->sensor_name, 15, 1, fp);
  cur_sensor->sensor_char = fgetc(fp);
  fread(cur_sensor->robot_to_mesg, 15, 1, fp);

  cur_sensor->used = fgetc(fp);
}

struct sensor *load_sensor_allocate(FILE *fp)
{
  struct sensor *cur_sensor = cmalloc(sizeof(struct sensor));
  load_sensor(cur_sensor, fp);
  return cur_sensor;
}

void save_robot(struct robot *cur_robot, FILE *fp, int savegame)
{
  int program_length;
  int i;

#ifdef CONFIG_DEBYTECODE
  // Write the program's source code if it's a world file, or the
  // bytecode if it's a save file. For save files we currently must write
  // bytecode because it stores the zap status inside the programs
  // themselves (even though it's in the label cache as well).

  if(savegame)
  {
    prepare_robot_bytecode(cur_robot);
    program_length = cur_robot->program_bytecode_length;
  }
  else
  {
    program_length = cur_robot->program_source_length;
  }

  // As of 2.83 we're writing out 4 byte sizes.
  fputd(program_length, fp);
#else /* !CONFIG_DEBYTECODE */
  program_length = cur_robot->program_bytecode_length;
  fputw(program_length, fp);
  // Junk
  fputw(0, fp);
#endif /* !CONFIG_DEBYTECODE */

  fwrite(cur_robot->robot_name, 15, 1, fp);
  fputc(cur_robot->robot_char, fp);
  if(savegame)
  {
    fputw(cur_robot->cur_prog_line, fp);
    fputc(cur_robot->pos_within_line, fp);
    fputc(cur_robot->robot_cycle, fp);
    fputc(cur_robot->cycle_count, fp);
    fputc(cur_robot->bullet_type, fp);
    fputc(cur_robot->is_locked, fp);
    fputc(cur_robot->can_lavawalk, fp);
    fputc(cur_robot->walk_dir, fp);
    fputc(cur_robot->last_touch_dir, fp);
    fputc(cur_robot->last_shot_dir, fp);
    fputw(cur_robot->xpos, fp);
    fputw(cur_robot->ypos, fp);
    fputc(cur_robot->status, fp);
  }
  else
  {
    // Put some "default" values here instead
    fputw(1, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(1, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputw(cur_robot->xpos, fp);
    fputw(cur_robot->ypos, fp);
    fputc(0, fp);
  }

  // Junk local
  fputw(0, fp);
  fputc(cur_robot->used, fp);
  fputw(cur_robot->loop_count, fp);

  // If savegame, there's some additional information to get
  if(savegame)
  {
    int stack_size = cur_robot->stack_size;

    // Write the local counters
    for(i = 0; i < 32; i++)
    {
      fputd(cur_robot->local[i], fp);
    }

    // Put the stack size
    fputd(stack_size, fp);
    // Put the stack pointer
    fputd(cur_robot->stack_pointer, fp);
    // Put the stack

    for(i = 0; i < stack_size; i++)
    {
      fputd(cur_robot->stack[i], fp);
    }
  }

#ifdef CONFIG_DEBYTECODE
  // NOTE: This will one day be a good thing to move out of the save game;
  // this will require that the programs become immutable and thus have
  // the zap status stored somewhere else.

  if(!savegame)
    fwrite(cur_robot->program_source, program_length, 1, fp);
  else
#endif
    fwrite(cur_robot->program_bytecode, program_length, 1, fp);
}

void save_scroll(struct scroll *cur_scroll, FILE *fp, int savegame)
{
  int scroll_size = (int)cur_scroll->mesg_size;

  fputw(cur_scroll->num_lines, fp);
  fputw(0, fp);
  fputw(scroll_size, fp);
  fputc(cur_scroll->used, fp);

  fwrite(cur_scroll->mesg, scroll_size, 1, fp);
}

void save_sensor(struct sensor *cur_sensor, FILE *fp, int savegame)
{
  fwrite(cur_sensor->sensor_name, 15, 1, fp);
  fputc(cur_sensor->sensor_char, fp);
  fwrite(cur_sensor->robot_to_mesg, 15, 1, fp);
  fputc(cur_sensor->used, fp);
}

#ifdef CONFIG_DEBYTECODE
static
#endif
void clear_label_cache(struct label **label_list, int num_labels)
{
  int i;

  if(label_list)
  {
    for(i = 0; i < num_labels; i++)
    {
      free(label_list[i]);
    }

    free(label_list);
  }
}

void clear_robot_contents(struct robot *cur_robot)
{
  free(cur_robot->stack);

#ifdef CONFIG_DEBYTECODE
  // If it was created by the game or loaded via a save file
  // then it won't have source code.
  if(cur_robot->program_source)
  {
    free(cur_robot->program_source);
    cur_robot->program_source = NULL;
    cur_robot->program_source_length = 0;
  }
#endif

  // It could be in the editor, or possibly it was never executed.
  if(cur_robot->program_bytecode)
  {
    if(cur_robot->used)
      clear_label_cache(cur_robot->label_list, cur_robot->num_labels);
    free(cur_robot->program_bytecode);
    cur_robot->program_bytecode = NULL;
    cur_robot->program_bytecode_length = 0;
  }
}

void clear_robot(struct robot *cur_robot)
{
  clear_robot_contents(cur_robot);
  free(cur_robot);
}

void clear_scroll(struct scroll *cur_scroll)
{
  free(cur_scroll->mesg);
  free(cur_scroll);
}

// Does not remove entry from the normal list
static void remove_robot_name_entry(struct board *src_board,
 struct robot *cur_robot, char *name)
{
  // Find the position
  int first, last;
  int active = src_board->num_robots_active;
  struct robot **name_list = src_board->robot_list_name_sorted;

  find_robot(src_board, name, &first, &last);

  // Find the one that matches the robot
  while(name_list[first] != cur_robot)
    first++;

  // Remove from name list
  active--;

  if(first != active)
  {
    memmove(name_list + first, name_list + first + 1,
     (active - first) * sizeof(struct robot *));
  }
  src_board->num_robots_active = active;
}

void clear_robot_id(struct board *src_board, int id)
{
  struct robot *cur_robot = src_board->robot_list[id];

  if(id)
  {
    remove_robot_name_entry(src_board, cur_robot, cur_robot->robot_name);
    clear_robot(cur_robot);
    src_board->robot_list[id] = NULL;
  }
  else
  {
    clear_robot_contents(cur_robot);
    cur_robot->used = 0;
  }
}

void clear_scroll_id(struct board *src_board, int id)
{
  clear_scroll(src_board->scroll_list[id]);
  src_board->scroll_list[id] = NULL;
}

void clear_sensor_id(struct board *src_board, int id)
{
  clear_sensor(src_board->sensor_list[id]);
  src_board->sensor_list[id] = NULL;
}

void clear_sensor(struct sensor *cur_sensor)
{
  free(cur_sensor);
}

#ifndef CONFIG_DEBYTECODE

void reallocate_robot(struct robot *robot, int size)
{
  robot->program_bytecode = crealloc(robot->program_bytecode, size);
  robot->program_bytecode_length = size;
}

#endif /* CONFIG_DEBYTECODE */

void reallocate_scroll(struct scroll *scroll, size_t size)
{
  scroll->mesg = crealloc(scroll->mesg, size);
  scroll->mesg_size = size;
}

static struct label *find_label(struct robot *cur_robot, const char *name)
{
  int total = cur_robot->num_labels - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  struct label **base = cur_robot->label_list;
  struct label *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];

    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
        // Found a match, see if there's an earlier one
        while(middle)
        {
          current = base[middle - 1];
          if(!strcasecmp(current->name, name))
          {
            middle--;
          }
          else
          {
            break;
          }
        }

        current = base[middle];

        // Now find a non-zapped one
        while(middle <= total)
        {
          if(current->zapped)
          {
            if(middle == total)
              return NULL;

            middle++;
            current = base[middle];
            if(strcasecmp(current->name, name))
            {
              return NULL;
            }
          }
          else
          {
            break;
          }
        }

        return current;
      }
    }
  }

  return NULL;
}

static int find_label_position(struct robot *cur_robot, const char *name)
{
  struct label *cur_label = find_label(cur_robot, name);

  if(cur_label)
  {
    return cur_label->position;
  }
  else
  {
    return -1;
  }
}

// This should return the last zapped label found, for restore label
// to work correctly.

static struct label *find_zapped_label(struct robot *cur_robot, char *name)
{
  int total = cur_robot->num_labels - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  struct label **base = cur_robot->label_list;
  struct label *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
       // Found a match, see if there's a later one
        while(middle < total)
        {
          current = base[middle + 1];
          if(!strcasecmp(current->name, name))
          {
            middle++;
          }
          else
          {
            break;
          }
        }

        current = base[middle];

        // Now find a zapped one
        while(middle >= 0)
        {
          if(!current->zapped)
          {
            if(!middle)
              return NULL;

            middle--;
            current = base[middle];
            if(strcasecmp(current->name, name))
            {
              return NULL;
            }
          }
          else
          {
            break;
          }
        }

        return current;
      }
    }
  }

  return NULL;
}

// Returns 1 if found, first is the first robot in the list,
// last is the last. If not found, first and last are the position to place
// into.

int find_robot(struct board *src_board, const char *name,
 int *first, int *last)
{
  int total = src_board->num_robots_active - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  struct robot **base = src_board->robot_list_name_sorted;
  struct robot *current;
  int f, l;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->robot_name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
        // Find any prior occurances
        f = middle;
        l = middle;

        while((f > 0) && (!strcasecmp(name, (base[f - 1])->robot_name)))
        {
          f--;
        }

        *first = f;

        while((l < total) && (!strcasecmp(name, (base[l + 1])->robot_name)))
        {
          l++;
        }

        *last = l;
        return 1;
      }
    }
  }

  if(cmpval > 0)
  {
    *first = middle + 1;
    *last = middle + 1;
  }
  else
  {
    *first = middle;
    *last = middle;
  }

  return 0;
}

void send_robot_def(struct world *mzx_world, int robot_id, int mesg_id)
{
  switch(mesg_id)
  {
    case 0:
      send_robot_id(mzx_world, robot_id, "TOUCH", 0);
      break;

    case 1:
      send_robot_id(mzx_world, robot_id, "BOMBED", 0);
      break;

    case 2:
      send_robot_all(mzx_world, "INVINCO");
      break;

    case 3:
      send_robot_id(mzx_world, robot_id, "PUSHED", 0);
      break;

    case 4:
      if(send_robot_id(mzx_world, robot_id, "PLAYERSHOT", 0))
      {
        send_robot_id(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case 5:
      if(send_robot_id(mzx_world, robot_id, "NEUTRALSHOT", 0))
      {
        send_robot_id(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case 6:
      if(send_robot_id(mzx_world, robot_id, "ENEMYSHOT", 0))
      {
        send_robot_id(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case 7:
      send_robot_all(mzx_world, "PLAYERHIT");
      break;

    case 8:
      send_robot_id(mzx_world, robot_id, "LAZER", 0);
      break;

    case 9:
      send_robot_id(mzx_world, robot_id, "SPITFIRE", 0);
      break;

    case 10:
      send_robot_all(mzx_world, "JUSTLOADED");
      break;

    case 11:
      send_robot_all(mzx_world, "JUSTENTERED");
      break;

    case 12:
      send_robot_all(mzx_world, "GOOPTOUCHED");
      break;

    case 13:
      send_robot_all(mzx_world, "PLAYERHURT");
      break;
  }
}

static void send_sensor_command(struct world *mzx_world, int id, int command)
{
  struct board *src_board = mzx_world->current_board;
  struct sensor *cur_sensor = src_board->sensor_list[id];
  int x = -1, y = -1, under = -1;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int player_offset = player_x + (player_y * board_width);
  int offset;
  int move_status;

  // Find sensor
  if(!(command & 0x100))
  {
    // Don't bother for a char cmd
    under = 0;
    if((level_under_id[player_offset] == 122) &&
     (level_under_param[player_offset] == id))
    {
      under = 1;
      x = player_x;
      y = player_y;
    }
    else
    {
      int found = 0;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          if((level_id[offset] == 122) &&
           (level_param[offset] == id))
          {
            found = 1;
            break;
          }
        }
        if(found)
          break;
      }
    }
  }

  // Okay, this means the sensor is unlinked. Due to some
  // errors in world 1 format files or ver1to2 or something
  // this can happen (see Caverns). It's very unpleasant.

  if(y == board_height)
    return;

  // Cmd
  switch(command)
  {
    case 0:
    case 1:
    case 2:
    case 3:
    {
      move_status = 0;
      if(under)
      {
        // Attempt to move player, then if ok,
        // put sensor underneath and delete from
        // original space.
        move_status = move(mzx_world, x, y, command, CAN_PUSH |
         CAN_TRANSPORT | CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK);

        if(move_status == NO_HIT)
        {
          // Find player...
          find_player(mzx_world);
          player_x = mzx_world->player_x;
          player_y = mzx_world->player_y;
          player_offset = player_x + (player_y * board_width);

          move_status = HIT_PLAYER;
        }
        else
        {
          send_robot(mzx_world, cur_sensor->robot_to_mesg, "SENSORTHUD", 0);
        }
      }
      else
      {
        // Attempt to move sensor.
        move_status = move(mzx_world, x, y, command,
         CAN_PUSH | CAN_TRANSPORT | CAN_LAVAWALK | CAN_FIREWALK |
         CAN_WATERWALK | REACT_PLAYER);

        if(move_status == HIT_PLAYER)
        {
          step_sensor(mzx_world, id);
        }
      }

      if(move_status == HIT_PLAYER)
      {
        // Met player- so put under player!
        mzx_world->under_player_id = level_under_id[player_offset];
        mzx_world->under_player_param = level_under_param[player_offset];
        mzx_world->under_player_color = level_under_color[player_offset];
        level_under_id[player_offset] = 122;
        level_under_param[player_offset] = id;
        level_under_color[player_offset] = level_color[x + (y * board_width)];
        id_remove_top(mzx_world, x, y);
      }
      else

      if(move_status != NO_HIT)
      {
        // Sensorthud!
        send_robot(mzx_world, cur_sensor->robot_to_mesg, "SENSORTHUD", 0);
      }
      break;
    }

    case 4:
    {
      if(under)
      {
        id_remove_under(mzx_world, x, y);
        clear_sensor_id(src_board, id);
      }
      else
      {
        id_remove_top(mzx_world, x, y);
        clear_sensor_id(src_board, id);
      }
      break;
    }

    default:
    {
      if(command & 0x100)
      {
        cur_sensor->sensor_char = command - 256;
      }
      else
      {
        if(under)
        {
          level_under_color[player_offset] = command - 512;
        }
        else
        {
          src_board->level_color[x + (y * board_width)] =
           command - 512;
        }
      }
      break;
    }
  }
}

static void send_sensors(struct world *mzx_world, char *name, const char *mesg)
{
  struct board *src_board = mzx_world->current_board;

  if(src_board->num_sensors)
  {
    // Sensors
    // Set command- 0-3 move, 4 die, 256 | # char, 512 | # color (hex)
    int command = -1; // No command yet

    // Check movement commands
    if(mesg[1] == 0)
    {
      char first_letter = mesg[0];
      if((first_letter >= 'a') && (first_letter <= 'z'))
        first_letter -= 32;

      switch(first_letter)
      {
        case 'N':
          command = 0;
          break;

        case 'S':
          command = 1;
          break;

        case 'E':
          command = 2;
          break;

        case 'W':
          command = 3;
          break;
      }
    }

    // Die?
    if(!strcasecmp("DIE", mesg))
      command = 4;

    // Char___? (___ can be ### or 'c')
    if(!strncasecmp("CHAR", mesg, 4))
    {
     command = 256;
      if(mesg[4] == '\'')
        command = 0x100 | mesg[5];
      else
        command = 0x100 | (strtol(mesg + 4, NULL, 10) & 0xFF);
    }

    // Color__? (__ is hex)
    if(!strncasecmp("COLOR", mesg, 5))
    {
      command = 512 | (strtol(mesg + 5, NULL, 16) & 0xFF);
    }

    if(command != -1)
    {
      struct sensor **sensor_list = src_board->sensor_list;
      struct sensor *current_sensor;
      int i;

      if(!strcasecmp(name, "ALL"))
      {
        for(i = 1; i <= src_board->num_sensors; i++)
        {
          current_sensor = sensor_list[i];

          if(current_sensor)
          {
            send_sensor_command(mzx_world, i, command);
          }
        }
      }
      else
      {
        for(i = 1; i <= src_board->num_sensors; i++)
        {
          current_sensor = sensor_list[i];
          if((current_sensor != NULL) &&
           !strcasecmp(name, current_sensor->sensor_name))
          {
            send_sensor_command(mzx_world, i, command);
          }
        }
      }
    }
  }
}

static void set_robot_position(struct robot *cur_robot, int position)
{
  cur_robot->cur_prog_line = position;
  cur_robot->pos_within_line = 0;
  cur_robot->cycle_count = cur_robot->robot_cycle - 1;

  // Popping a subroutine's retval from the stack may legitimately
  // try to position another robot's code beyond the end of its
  // program. Ensure that if this happens, the robot's program
  // will terminate in the next cycle. We need -2 here to ignore
  // the initial 0xff and terminal 0x00 signature bytes.

  if(cur_robot->cur_prog_line > cur_robot->program_bytecode_length - 2)
    cur_robot->cur_prog_line = 0;

  if(cur_robot->status == 1)
    cur_robot->status = 2;
}

static int send_robot_direct(struct robot *cur_robot, const char *mesg,
 int ignore_lock, int send_self)
{
  char *robot_program;
  int new_position;

#ifdef CONFIG_DEBYTECODE
  prepare_robot_bytecode(cur_robot);
#endif
  robot_program = cur_robot->program_bytecode;

  if((cur_robot->is_locked) && (!ignore_lock))
    return 1; // Locked

  if(cur_robot->program_bytecode_length < 3)
    return 2; // No program!

  // Are we going to a subroutine? Returning?
  if(mesg[0] == '#')
  {
    // returning?
    if(!strcasecmp(mesg + 1, "return"))
    {
      if(cur_robot->stack_pointer)
      {
        int return_pos = robot_stack_pop(cur_robot);
        set_robot_position(cur_robot, return_pos);
      }
      else

      if(send_self)
      {
        // Contextually invalid return; skip the command
        cur_robot->cur_prog_line +=
         1 + robot_program[cur_robot->cur_prog_line] + 1;
      }
    }
    else

    // returning to the TOP?
    if(!strcasecmp(mesg + 1, "top"))
    {
      if(cur_robot->stack_pointer)
      {
        set_robot_position(cur_robot, cur_robot->stack[0]);
        cur_robot->stack_pointer = 0;
      }
      else

      if(send_self)
      {
        // Contextually invalid top; skip the command
        cur_robot->cur_prog_line +=
         1 + robot_program[cur_robot->cur_prog_line] + 1;
      }
    }
    else
    {
      new_position = find_label_position(cur_robot, mesg);

      if(new_position != -1)
      {
        // Push the current address onto the stack
        // If a maximum overflow happened, this simply won't work.
        int robot_position = cur_robot->cur_prog_line;
        int return_position;

        if(robot_position)
        {
          return_position = robot_position;

          if(send_self)
            return_position += robot_program[robot_position] + 2;
        }
        else
        {
          return_position = 0;
        }

        robot_stack_push(cur_robot, return_position);
        // Do the jump
        set_robot_position(cur_robot, new_position);
        return 0;
      }

      return 2;
    }
  }
  else
  {
    new_position = find_label_position(cur_robot, mesg);

    if(new_position != -1)
    {
      set_robot_position(cur_robot, new_position);
      return 0;
    }

    return 2;
  }

  return 0;
}

void send_robot(struct world *mzx_world, char *name, const char *mesg,
 int ignore_lock)
{
  struct board *src_board = mzx_world->current_board;
  int first, last;

  if(!strcasecmp(name, "all"))
  {
    send_robot_all(mzx_world, mesg);
  }
  else
  {
    // See if it's the global robot
    if(!strcasecmp(name, mzx_world->global_robot.robot_name) &&
     mzx_world->global_robot.used)
    {
      send_robot_direct(&mzx_world->global_robot, mesg,
       ignore_lock, 0);
    }

    if(find_robot(src_board, name, &first, &last))
    {
      while(first <= last)
      {
        send_robot_direct(src_board->robot_list_name_sorted[first],
         mesg, ignore_lock, 0);
        first++;
      }
    }
  }

  send_sensors(mzx_world, name, mesg);
}

int send_robot_id(struct world *mzx_world, int id, const char *mesg,
 int ignore_lock)
{
  struct robot *cur_robot = mzx_world->current_board->robot_list[id];
  return send_robot_direct(cur_robot, mesg, ignore_lock, 0);
}

int send_robot_self(struct world *mzx_world, struct robot *src_robot,
 const char *mesg)
{
  return send_robot_direct(src_robot, mesg, 1, 1);
}

void send_robot_all(struct world *mzx_world, const char *mesg)
{
  struct board *src_board = mzx_world->current_board;
  int i;

  if(mzx_world->global_robot.used)
  {
    send_robot_direct(&mzx_world->global_robot, mesg, 0, 0);
  }

  for(i = 0; i < src_board->num_robots_active; i++)
  {
    send_robot_direct(src_board->robot_list_name_sorted[i], mesg, 0, 0);
  }
}

// Run a set of x/y pairs through the prefixes
void prefix_first_last_xy(struct world *mzx_world, int *fx, int *fy,
 int *lx, int *ly, int robotx, int roboty)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tfx = *fx;
  int tfy = *fy;
  int tlx = *lx;
  int tly = *ly;

  switch(mzx_world->first_prefix)
  {
    case 1:
    case 5:
    {
      tfx += robotx;
      tfy += roboty;
      break;
    }

    case 2:
    case 6:
    {
      find_player(mzx_world);
      tfx += mzx_world->player_x;
      tfy += mzx_world->player_y;
      break;
    }

    case 3:
    {
      tfx += get_counter(mzx_world, "FIRSTXPOS", 0);
      tfy += get_counter(mzx_world, "FIRSTYPOS", 0);
      break;
    }

    case 7:
    {
      tfx += get_counter(mzx_world, "XPOS", 0);
      tfy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  switch(mzx_world->last_prefix)
  {
    case 1:
    case 5:
    {
      tlx += robotx;
      tly += roboty;
      break;
    }

    case 2:
    case 6:
    {
      find_player(mzx_world);
      tlx += mzx_world->player_x;
      tly += mzx_world->player_y;
      break;
    }

    case 3:
    {
      tlx += get_counter(mzx_world, "LASTXPOS", 0);
      tly += get_counter(mzx_world, "LASTYPOS", 0);
      break;
    }

    case 7:
    {
      tlx += get_counter(mzx_world, "XPOS", 0);
      tly += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tfx < 0)
    tfx = 0;

  if(tfy < 0)
    tfy = 0;

  if(tlx < 0)
    tlx = 0;

  if(tly < 0)
    tly = 0;

  if(tfx >= board_width)
    tfx = board_width - 1;

  if(tfy >= board_height)
    tfy = board_height - 1;

  if(tlx >= board_width)
    tlx = board_width - 1;

  if(tly >= board_height)
    tly = board_height - 1;

  *fx = tfx;
  *fy = tfy;
  *lx = tlx;
  *ly = tly;
}

// These are primarily for copy {overlay} block. Allows a variable
// width/height so that coordinates can be fixed against more than
// simply the board (for instance vlayer). In the future this could
// also be used to copy inbetween boards, perhaps.

void prefix_first_xy_var(struct world *mzx_world, int *fx, int *fy,
 int robotx, int roboty, int width, int height)
{
  int tfx = *fx;
  int tfy = *fy;

  switch(mzx_world->first_prefix)
  {
    case 1:
    case 5:
    {
      tfx += robotx;
      tfy += roboty;
      break;
    }

    case 2:
    case 6:
    {
      find_player(mzx_world);
      tfx += mzx_world->player_x;
      tfy += mzx_world->player_y;
      break;
    }

    case 3:
    {
      tfx += get_counter(mzx_world, "FIRSTXPOS", 0);
      tfy += get_counter(mzx_world, "FIRSTYPOS", 0);
      break;
    }

    case 7:
    {
      tfx += get_counter(mzx_world, "XPOS", 0);
      tfy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tfx < 0)
    tfx = 0;

  if(tfy < 0)
    tfy = 0;

  if(tfx >= width)
    tfx = width - 1;

  if(tfy >= height)
    tfy = height - 1;

  *fx = tfx;
  *fy = tfy;
}

void prefix_last_xy_var(struct world *mzx_world, int *lx, int *ly,
 int robotx, int roboty, int width, int height)
{
  int tlx = *lx;
  int tly = *ly;

  switch(mzx_world->last_prefix)
  {
    case 1:
    case 5:
    {
      tlx += robotx;
      tly += roboty;
      break;
    }

    case 2:
    case 6:
    {
      find_player(mzx_world);
      tlx += mzx_world->player_x;
      tly += mzx_world->player_y;
      break;
    }

    case 3:
    {
      tlx += get_counter(mzx_world, "LASTXPOS", 0);
      tly += get_counter(mzx_world, "LASTYPOS", 0);
      break;
    }

    case 7:
    {
      tlx += get_counter(mzx_world, "XPOS", 0);
      tly += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tlx < 0)
    tlx = 0;

  if(tly < 0)
    tly = 0;

  if(tlx >= width)
    tlx = width - 1;

  if(tly >= height)
    tly = height - 1;

  *lx = tlx;
  *ly = tly;
}

void prefix_mid_xy_var(struct world *mzx_world, int *mx, int *my,
 int robotx, int roboty, int width, int height)
{
  int tmx = *mx;
  int tmy = *my;

  switch(mzx_world->first_prefix)
  {
    case 1:
    {
      tmx += robotx;
      tmy += roboty;
      break;
    }

    case 2:
    {
      find_player(mzx_world);
      tmx += mzx_world->player_x;
      tmy += mzx_world->player_y;
      break;
    }

    case 3:
    {
      tmx += get_counter(mzx_world, "XPOS", 0);
      tmy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tmx < 0)
    tmx = 0;

  if(tmy < 0)
    tmy = 0;

  if(tmx >= width)
    tmx = width - 1;

  if(tmy >= height)
    tmy = height - 1;

  *mx = tmx;
  *my = tmy;
}

// Just does the middle prefixes, since those are all that's usually
// needed...
void prefix_mid_xy(struct world *mzx_world, int *mx, int *my, int x, int y)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tmx = *mx;
  int tmy = *my;

  switch(mzx_world->mid_prefix)
  {
    case 1:
    {
      tmx += x;
      tmy += y;
      break;
    }

    case 2:
    {
      find_player(mzx_world);
      tmx += mzx_world->player_x;
      tmy += mzx_world->player_y;
      break;
    }

    case 3:
    {
      tmx += get_counter(mzx_world, "XPOS", 0);
      tmy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tmx < 0)
    tmx = 0;
  if(tmy < 0)
    tmy = 0;
  if(tmx >= board_width)
    tmx = board_width - 1;
  if(tmy >= board_height)
    tmy = board_height - 1;

  *mx = tmx;
  *my = tmy;
}

// Move an x/y pair in a given direction. Returns non-0 if edge reached.
int move_dir(struct board *src_board, int *x, int *y, enum dir dir)
{
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tx = *x;
  int ty = *y;

  switch(dir)
  {
    case NORTH:
    {
      if(ty == 0)
      {
        return 1;
      }
      ty--;
      break;
    }

    case SOUTH:
    {
      if(ty == (board_height - 1))
      {
        return 1;
      }
      ty++;
      break;
    }

    case EAST:
    {
      if(tx == (board_width - 1))
      {
        return 1;
      }
      tx++;
      break;
    }

    case WEST:
    {
      if(tx == 0)
      {
        return 1;
      }
      tx--;
      break;
    }

    default:
      break;
  }

  *x = tx;
  *y = ty;

  return 0;
}

// Returns the numeric value pointed to OR the numeric value represented
// by the counter string pointed to. (the ptr is at the param within the
// command)
// Sign extends the result, for now...

int parse_param(struct world *mzx_world, char *program, int id)
{
  char ibuff[ROBOT_MAX_TR];

  if(program[0] == 0)
  {
    // Numeric
    return (signed short)((int)program[1] | (int)(program[2] << 8));
  }

  // Expressions - Exo
  if((program[1] == '(') && mzx_world->version >= 0x244)
  {
    char *e_ptr = program + 2;
    int val, error;

    val = parse_expression(mzx_world, &e_ptr, &error, id);
    if(!error && !(*e_ptr))
      return val;
  }

  tr_msg(mzx_world, program + 1, id, ibuff);

  return get_counter(mzx_world, ibuff, id);
}

// These will always return numeric values
enum thing parse_param_thing(struct world *mzx_world, char *program)
{
  return (enum thing)
   ((int)program[1] | (int)(program[2] << 8));
}

enum dir parse_param_dir(struct world *mzx_world, char *program)
{
  return (enum dir)
   ((int)program[1] | (int)(program[2] << 8));
}

enum equality parse_param_eq(struct world *mzx_world, char *program)
{
  return (enum equality)
   ((int)program[1] | (int)(program[2] << 8));
}

enum condition parse_param_cond(struct world *mzx_world, char *program,
 enum dir *direction)
{
  *direction = (enum dir)program[2];
  return (enum condition)program[1];
}

// Returns location of next parameter (pos is loc of current parameter)
int next_param(char *ptr, int pos)
{
  if(ptr[pos])
  {
    return ptr[pos] + 1;
  }
  else
  {
    return 3;
  }
}

char *next_param_pos(char *ptr)
{
  int index = *ptr;
  if(index)
  {
    return ptr + index + 1;
  }
  else
  {
    return ptr + 3;
  }
}

// Internal only. NOTE- IF WE EVER ALLOW ZAPPING OF LABELS NOT IN CURRENT
// ROBOT, USE A COPY OF THE *LABEL BEFORE THE PREPARE_ROBOT_MEM!

// TODO: What we'll want to do is use the label cache instead, so that
// we're not actually modifying the program.

// Both of these can only be done from an actively executing program,
// so they will have valid bytecode.

int restore_label(struct robot *cur_robot, char *label)
{
  struct label *dest_label = find_zapped_label(cur_robot, label);

  if(dest_label)
  {
    cur_robot->program_bytecode[dest_label->position + 1] = ROBOTIC_CMD_LABEL;
    dest_label->zapped = 0;
    return 1;
  }

  return 0;
}

int zap_label(struct robot *cur_robot, char *label)
{
  struct label *dest_label = find_label(cur_robot, label);

  if(dest_label)
  {
    cur_robot->program_bytecode[dest_label->position + 1] = ROBOTIC_CMD_ZAPPED_LABEL;
    dest_label->zapped = 1;
    return 1;
  }

  return 0;
}

// Turns a color (including those w/??) to a real color (0-255)
int fix_color(int color, int def)
{
  if(color < 256)
    return color;
  if(color < 272)
    return (color & 0x0F) + (def & 0xF0);
  if(color < 288)
    return ((color - 272) << 4) + (def & 0x0F);

  return def;
}

static int robot_box_down(char *program, int pos, int count)
{
  int i, cur_cmd = -1;
  int old_pos;
  int done = 0;

  for(i = 0; (i < count) && (!done); i++)
  {
    old_pos = pos;
    do
    {
      // Go forward a line (if possible)
      pos += program[pos] + 2;
      if(program[pos] == 0)
      {
        pos = old_pos;
        done = 1;
      }
      else
      {
        cur_cmd = program[pos + 1];
        if(((cur_cmd < 103) && (cur_cmd != 47)) ||
         ((cur_cmd > 106) && (cur_cmd < 116)) ||
         ((cur_cmd > 117) && (cur_cmd != 249)))
        {
          pos = old_pos;
          done = 1;
        }
      }
    } while(((cur_cmd == 106) || (cur_cmd == 249)) && (!done));

    if(i == 100000)
      i = 99999;
  }

  return pos;
}

static int robot_box_up(char *program, int pos, int count)
{
  int i, cur_cmd = -1;
  int old_pos;
  int done = 0;

  for(i = 0; (i < count) && (!done); i++)
  {
    old_pos = pos;
    do
    {
      // Go backwards a line (if possible)
      if(program[pos - 1] == 0xFF)
      {
        pos = old_pos;
        done = 1;
      }
      else
      {
        pos -= program[pos - 1] + 2;
        cur_cmd = program[pos + 1];
        if(((cur_cmd < 103) && (cur_cmd != 47)) ||
         ((cur_cmd > 106) && (cur_cmd < 116)) ||
         ((cur_cmd > 117) && (cur_cmd != 249)))
        {
          pos = old_pos;
          done = 1;
        }
      }
    } while(((cur_cmd == 106) || (cur_cmd == 249)) && (!done));

    if(i == 100000)
      i = 99999;
  }

  return pos;
}

static int num_ccode_chars(char *str)
{
  char current_char = *str;
  char *str_pos = str;
  int count = 0;

  while(current_char)
  {
    if((current_char == '~') || (current_char == '@'))
    {
      count++;

      if(isxdigit((int)str_pos[1]))
      {
        count++;
        str_pos++;
      }
    }

    str_pos++;
    current_char = *str_pos;
  }

  return count;
}

static void display_robot_line(struct world *mzx_world, char *program,
 int y, int id)
{
  char ibuff[ROBOT_MAX_TR];
  char *next;
  int scroll_base_color = mzx_world->scroll_base_color;
  int scroll_arrow_color = mzx_world->scroll_arrow_color;

  switch(program[1])
  {
    case 103: // Normal message
    {
      tr_msg(mzx_world, program + 3, id, ibuff);
      ibuff[64] = 0; // Clip
      write_string_ext(ibuff, 8, y, scroll_base_color, 1, 0, 0);
      break;
    }

    case 104: // Option
    {
      // Skip over label...
      // next is pos of string
      next = next_param_pos(program + 2);
      tr_msg(mzx_world, next + 1, id, ibuff);
      ibuff[62] = 0; // Clip
      color_string_ext(ibuff, 10, y, scroll_base_color, 0, 0, true);
      draw_char_ext('\x10', scroll_arrow_color, 8, y, 0, 0);
      break;
    }

    case 105: // Counter-based option
    {
      // Check counter
      int val = parse_param(mzx_world, program + 2, id);
      if(val)
      {
        // Skip over counter and label...
        // next is pos of string
        next = next_param_pos(program + 2);
        next = next_param_pos(next);
        tr_msg(mzx_world, next + 1, id, ibuff);
        ibuff[62] = 0; // Clip
        color_string_ext(ibuff, 10, y, scroll_base_color, 0, 0, true);
        draw_char_ext('\x10', scroll_arrow_color, 8, y, 0, 0);
      }
      break;
    }

    case 116: // Colored message
    {
      tr_msg(mzx_world, program + 3, id, ibuff);
      ibuff[64 + num_ccode_chars(ibuff)] = 0; // Clip
      color_string_ext(ibuff, 8, y, scroll_base_color, 0, 0, true);
      break;
    }

    case 117: // Centered message
    {
      int length, x_position;
      tr_msg(mzx_world, program + 3, id, ibuff);
      ibuff[64 + num_ccode_chars(ibuff)] = 0; // Clip
      length = strlencolor(ibuff);
      x_position = 40 - (length / 2);
      color_string_ext(ibuff, x_position, y, scroll_base_color, 0, 0, true);
      break;
    }
  }

  // Others, like 47 and 106, are blank lines
}

static void robot_frame(struct world *mzx_world, char *program, int id)
{
  // Displays one frame of a robot. The scroll edging, arrows, and title
  // must already be shown. Simply prints each line. The pointer points
  // to the center line.
  int scroll_base_color = mzx_world->scroll_base_color;
  int i, pos = 0;
  int old_pos;
  // Display center line
  fill_line_ext(64, 8, 12, 32, scroll_base_color, 0, 0);
  display_robot_line(mzx_world, program, 12, id);

  // Display lines above center line
  for(i = 11; i >= 6; i--)
  {
    fill_line_ext(64, 8, i, 32, scroll_base_color, 0, 0);
    // Go backward to previous line
    old_pos = pos;
    pos = robot_box_up(program, pos, 1);
    if(old_pos != pos)
      display_robot_line(mzx_world, program + pos, i, id);
  }

  // Display lines below center line
  pos = 0;

  for(i = 13; i <= 18; i++)
  {
    fill_line_ext(64, 8, i, 32, scroll_base_color, 0, 0);
    old_pos = pos;
    pos = robot_box_down(program, pos, 1);
    if(old_pos != pos)
      display_robot_line(mzx_world, program + pos, i, id);
  }
}

void robot_box_display(struct world *mzx_world, char *program,
 char *label_storage, int id)
{
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot = src_board->robot_list[id];
  int pos = 0, key, mouse_press;

  label_storage[0] = 0;

  // Draw screen
  save_screen();
  m_show();

  dialog_fadein();

  scroll_edging_ext(mzx_world, 4, 0, 0);
  // Write robot name
  if(!cur_robot->robot_name[0])
  {
    write_string_ext("Interaction", 35, 4,
     mzx_world->scroll_title_color, 0, 0, 0);
  }
  else
  {
    write_string_ext(cur_robot->robot_name,
     40 - (Uint32)strlen(cur_robot->robot_name) / 2, 4,
     mzx_world->scroll_title_color, 1, 0, 0);
  }

  // Scan section and mark all invalid counter-controlled options as codes
  // 249.

  do
  {
    if(program[pos + 1] == 249)
      program[pos + 1] = 105;

    if(program[pos + 1] == 105)
    {
      if(!parse_param(mzx_world, program + (pos + 2), id))
        program[pos + 1] = 249;
    }

    pos += program[pos] + 2;
  } while(program[pos]);

  pos = 0;

  // Backwards
  do
  {
    if(program[pos + 1] == 249)
      program[pos + 1] = 105;

    if(program[pos + 1] == 105)
    {
      if(!parse_param(mzx_world, program + (pos + 2), id))
        program[pos + 1] = 249;
    }

    if(program[pos - 1] == 0xFF)
      break;

    pos -= program[pos - 1] + 2;
  } while(1);

  pos = 0;

  // Loop
  do
  {
    // Display scroll
    robot_frame(mzx_world, program + pos, id);
    update_screen();

    update_event_status_delay();
    key = get_key(keycode_internal);

    mouse_press = get_mouse_press_ext();

    if(mouse_press && (mouse_press <= MOUSE_BUTTON_RIGHT))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_y >= 6) && (mouse_y <= 18) &&
       (mouse_x >= 8) && (mouse_x <= 71))
      {
        int count = mouse_y - 12;
        if(!count)
        {
          key = IKEY_RETURN;
        }
        else
        {
          if(count > 0)
            pos = robot_box_down(program, pos, count);
          else
            pos = robot_box_up(program, pos, -count);

          warp_mouse(mouse_x, mouse_y - count);
        }
      }
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELUP)
    {
      pos = robot_box_up(program, pos, 3);
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
    {
      pos = robot_box_down(program, pos, 3);
    }

    switch(key)
    {
      case IKEY_UP:
        pos = robot_box_up(program, pos, 1);
        break;

      case IKEY_DOWN:
        pos = robot_box_down(program, pos, 1);
        break;

      case IKEY_RETURN:
      {
        key = IKEY_ESCAPE;

        if((program[pos + 1] == 104) || (program[pos + 1] == 105))
        {
          char *next;
          if(program[pos + 1] == 105)
            next = next_param_pos(program + pos + 2) + 1;
          else
            next = program + pos + 3;

          // Goto option! Stores in label_storage
          strcpy(label_storage, next);
        }
        break;
      }

      case IKEY_PAGEDOWN:
        pos = robot_box_down(program, pos, 6);
        break;

      case IKEY_PAGEUP:
        pos = robot_box_up(program, pos, 6);
        break;

      case IKEY_HOME:
        pos = robot_box_up(program, pos, 100000);
        break;

      case IKEY_END:
        pos = robot_box_down(program, pos, 100000);
        break;

      default:
      case IKEY_ESCAPE:
      case 0:
        break;
    }
  } while(key != IKEY_ESCAPE);

  // Scan section and mark all invalid counter-controlled options as codes
  // 105.

  pos = 0;

  do
  {
    if(program[pos + 1] == 249)
    {
      program[pos + 1] = 105;
    }

    pos += program[pos] + 2;

  } while(program[pos]);

  pos = 0;

  // Backwards
  do
  {
    if(program[pos + 1] == 249)
    {
      program[pos + 1] = 105;
    }
    if(program[pos - 1] == 0xFF)
      break;
    pos -= program[pos - 1] + 2;

  } while(1);

  dialog_fadeout();

  // Restore screen and exit
  m_hide();
  restore_screen();
  update_event_status();
}

void push_sensor(struct world *mzx_world, int id)
{
  struct board *src_board = mzx_world->current_board;
  send_robot(mzx_world, (src_board->sensor_list[id])->robot_to_mesg,
   "SENSORPUSHED", 0);
}

void step_sensor(struct world *mzx_world, int id)
{
  struct board *src_board = mzx_world->current_board;
  send_robot(mzx_world, (src_board->sensor_list[id])->robot_to_mesg,
   "SENSORON", 0);
}

// Translates message at target to the given buffer, returning location
// of this buffer. && becomes &, &INPUT& becomes the last input string,
// and &COUNTER& becomes the value of COUNTER. The size of the string is
// clipped to 512 chars.

#ifdef CONFIG_DEBYTECODE

char *tr_msg_ext(struct world *mzx_world, char *mesg, int id, char *buffer,
 char terminating_char)
{
  struct board *src_board = mzx_world->current_board;
  char *src_ptr = mesg;
  char current_char = *src_ptr;

  int dest_pos = 0;
  int val, error;

  while((current_char != terminating_char) && (dest_pos < ROBOT_MAX_TR))
  {
    switch(current_char)
    {
      case '\\':
        if((src_ptr[1] == '(') || (src_ptr[1] == '<') ||
         (src_ptr[1] == '\\') || (src_ptr[1] == terminating_char))
        {
          buffer[dest_pos] = src_ptr[1];
          src_ptr += 2;
        }
        else
        {
          buffer[dest_pos] = current_char;
          src_ptr++;
        }
        dest_pos++;
        break;

      case '(':
        src_ptr++;
        if(*src_ptr == '#')
        {
          src_ptr++;
          val = parse_expression(mzx_world, &src_ptr, &error, id);
          dest_pos += sprintf(buffer + dest_pos, "%02x", val);
        }
        else

        if(*src_ptr == '+')
        {
          src_ptr++;
          val = parse_expression(mzx_world, &src_ptr, &error, id);
          dest_pos += sprintf(buffer + dest_pos, "%x", val);
        }
        else

        if(!strncasecmp(src_ptr, "input", 5))
        {
          dest_pos += sprintf(buffer + dest_pos, "%s",
           src_board->input_string);
          src_ptr += 6;
        }
        else
        {
          val = parse_expression(mzx_world, &src_ptr, &error, id);
          dest_pos += sprintf(buffer + dest_pos, "%d", val);
        }
        break;

      case '<':
      {
        int expr_length;
        src_ptr++;
        expr_length = parse_string_expression(mzx_world, &src_ptr, id,
         buffer + dest_pos);
        dest_pos += expr_length;
        break;
      }

      default:
        buffer[dest_pos] = current_char;
        src_ptr++;
        dest_pos++;
    }

    current_char = *src_ptr;
  }

  buffer[dest_pos] = 0;
  return src_ptr + 1;
}

#else /* !CONFIG_DEBYTECODE */

char *tr_msg_ext(struct world *mzx_world, char *mesg, int id, char *buffer,
 char terminating_char)
{
  struct board *src_board = mzx_world->current_board;
  char name_buffer[256];
  char number_buffer[16];
  char *name_ptr;
  char current_char;
  char *src_ptr = mesg;
  char *old_ptr;

  size_t dest_pos = 0;
  size_t name_length;
  int error;
  int val;

  do
  {
    current_char = *src_ptr;

    if((current_char == '(') && (mzx_world->version >= 0x244))
    {
      src_ptr++;
      old_ptr = src_ptr;

      val = parse_expression(mzx_world, &src_ptr, &error, id);
      if(!error)
      {
        sprintf(number_buffer, "%d", val);
        strcpy(buffer + dest_pos, number_buffer);
        dest_pos += strlen(number_buffer);
      }
      else
      {
        buffer[dest_pos] = '(';
        dest_pos++;
        src_ptr = old_ptr;
      }

      current_char = *src_ptr;
    }
    else

    if(current_char == '&')
    {
      src_ptr++;
      current_char = *src_ptr;

      if(current_char == '&')
      {
        src_ptr++;
        buffer[dest_pos] = '&';
        dest_pos++;
      }
      else
      {
        // Input or Counter?
        name_ptr = name_buffer;

        while(current_char)
        {
          if(current_char == '(' && (mzx_world->version >= 0x244))
          {
            src_ptr++;
            val = parse_expression(mzx_world, &src_ptr, &error, id);
            if(!error)
            {
              sprintf(number_buffer, "%d", val);
              strcpy(name_ptr, number_buffer);
              name_ptr += strlen(number_buffer);
            }
          }
          else
          {
            *name_ptr = *src_ptr;
            name_ptr++;
            src_ptr++;
          }

          current_char = *src_ptr;

          if(current_char == '&')
          {
            src_ptr++;
            current_char = *src_ptr;
            break;
          }
        }

        *name_ptr = 0;

        if(!strcasecmp(name_buffer, "INPUT"))
        {
          // Input
          name_length = strlen(src_board->input_string);
          if(dest_pos + name_length >= ROBOT_MAX_TR)
            name_length = ROBOT_MAX_TR - dest_pos - 1;

          memcpy(buffer + dest_pos, src_board->input_string,
           name_length);
          dest_pos += name_length;
        }
        else

        // Counter or string
        if(is_string(name_buffer))
        {
          // Write the value of the counter name
          struct string str_src;

          get_string(mzx_world, name_buffer, &str_src, 0);

          name_length = str_src.length;

          if(dest_pos + name_length >= ROBOT_MAX_TR)
            name_length = ROBOT_MAX_TR - dest_pos - 1;

          memcpy(buffer + dest_pos, str_src.value, name_length);
          dest_pos += name_length;
        }
        else

        // #(counter) is a hex representation.
        if(name_buffer[0] == '+')
        {
          sprintf(number_buffer, "%x",
           get_counter(mzx_world, name_buffer + 1, id));

          name_length = strlen(number_buffer);

          if(dest_pos + name_length >= ROBOT_MAX_TR)
            name_length = ROBOT_MAX_TR - dest_pos - 1;

          memcpy(buffer + dest_pos, number_buffer, name_length);
          dest_pos += name_length;
        }
        else

        if(name_buffer[0] == '#')
        {
          name_length = 2;
          if(dest_pos + name_length >= ROBOT_MAX_TR)
            name_length = ROBOT_MAX_TR - dest_pos - 1;

          sprintf(number_buffer, "%02x",
           get_counter(mzx_world, name_buffer + 1, id));

          memcpy(buffer + dest_pos, number_buffer, name_length);
          dest_pos += name_length;
        }
        else
        {
          sprintf(number_buffer, "%d",
           get_counter(mzx_world, name_buffer, id));

          name_length = strlen(number_buffer);
          if(dest_pos + name_length >= ROBOT_MAX_TR)
            name_length = ROBOT_MAX_TR - dest_pos - 1;

          memcpy(buffer + dest_pos, number_buffer, name_length);
          dest_pos += name_length;
        }
      }
    }
    else
    {
      buffer[dest_pos] = current_char;
      src_ptr++;
      dest_pos++;
    }
  } while(current_char && (dest_pos < ROBOT_MAX_TR));

  buffer[dest_pos] = 0;
  return buffer;
}

#endif /* !CONFIG_DEBYTECODE */

// Don't do this if the entry is not in the normal list
void add_robot_name_entry(struct board *src_board, struct robot *cur_robot,
 char *name)
{
  // Find the position
  int first, last;
  int active = src_board->num_robots_active;
  struct robot **name_list = src_board->robot_list_name_sorted;

  find_robot(src_board, name, &first, &last);
  // Insert into name list, if it's not at the end
  if(first != active)
  {
    memmove(name_list + first + 1, name_list + first,
     (active - first) * sizeof(struct robot *));
  }
  name_list[first] = cur_robot;
  src_board->num_robots_active = active + 1;
}

// This could probably be done in a more efficient manner.
void change_robot_name(struct board *src_board, struct robot *cur_robot,
 char *new_name)
{
  // Remove the old one
  remove_robot_name_entry(src_board, cur_robot, cur_robot->robot_name);
  // Add the new one
  add_robot_name_entry(src_board, cur_robot, new_name);
  // And change the actual name
  strcpy(cur_robot->robot_name, new_name);
}

// Works with the ID-list. Will make room for a new one if there aren't any.
int find_free_robot(struct board *src_board)
{
  int num_robots = src_board->num_robots;
  int i;
  struct robot **robot_list = src_board->robot_list;

  for(i = 1; i <= num_robots; i++)
  {
    if(robot_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_robots)
    {

      int num_robots_allocated = src_board->num_robots_allocated;
      if(num_robots == num_robots_allocated)
      {
        if(num_robots_allocated)
          num_robots_allocated *= 2;
        else
          num_robots_allocated = 1;

        src_board->robot_list = crealloc(robot_list,
         (num_robots_allocated + 1) * sizeof(struct robot *));

        src_board->robot_list_name_sorted =
         crealloc(src_board->robot_list_name_sorted,
         (num_robots_allocated) * sizeof(struct robot *));
        src_board->num_robots_allocated = num_robots_allocated;
      }
      src_board->num_robots = num_robots + 1;
    }
    return i;
  }
  return -1;
}

// Like find_free_robot, but for scrolls. Will also expand the list if
// necessary.
static int find_free_scroll(struct board *src_board)
{
  int num_scrolls = src_board->num_scrolls;
  int i;
  struct scroll **scroll_list = src_board->scroll_list;

  for(i = 1; i <= num_scrolls; i++)
  {
    if(scroll_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_scrolls)
    {
      int num_scrolls_allocated = src_board->num_scrolls_allocated;
      if(num_scrolls == num_scrolls_allocated)
      {
        if(num_scrolls_allocated)
          num_scrolls_allocated *= 2;
        else
          num_scrolls_allocated = 1;

        src_board->scroll_list = crealloc(scroll_list,
         (num_scrolls_allocated + 1) * sizeof(struct scroll *));
        src_board->num_scrolls_allocated = num_scrolls_allocated;
      }
      src_board->num_scrolls = num_scrolls + 1;
    }
    return i;
  }

  return -1;
}

// Like find_free_robot, but for sensors. Will also expand the list if
// necessary.
static int find_free_sensor(struct board *src_board)
{
  int num_sensors = src_board->num_sensors;
  int i;
  struct sensor **sensor_list = src_board->sensor_list;

  for(i = 1; i <= num_sensors; i++)
  {
    if(sensor_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_sensors)
    {
      int num_sensors_allocated = src_board->num_sensors_allocated;
      if(num_sensors == num_sensors_allocated)
      {
        if(num_sensors_allocated)
          num_sensors_allocated *= 2;
        else
          num_sensors_allocated = 1;

        src_board->sensor_list = crealloc(sensor_list,
         (num_sensors_allocated + 1) * sizeof(struct sensor *));
        src_board->num_sensors_allocated = num_sensors_allocated;
      }
      src_board->num_sensors = num_sensors + 1;
    }
    return i;
  }

  return -1;
}

// Duplicates the contents of one robot to a new one, returning the ID
// of the duplicate. Supply the x/y position of where you want it to be
// placed. Returns the ID of location. Does NOT place the robot on the
// board (so be sure to do that). The given id is the slot to add it in;
// be sure that this is a valid (NULL) entry!

// This function must not be called in the editor because it won't copy
// source code. It's only for runtime (use duplicate_robot_direct_source
// there instead)

#ifdef CONFIG_DEBYTECODE
static
#else
__editor_maybe_static
#endif
void duplicate_robot_direct(struct robot *cur_robot,
 struct robot *copy_robot, int x, int y)
{
  char *dest_program_location, *src_program_location;
  int program_length;
  int num_labels;

#ifdef CONFIG_DEBYTECODE
  prepare_robot_bytecode(cur_robot);
#endif
  program_length = cur_robot->program_bytecode_length;
  num_labels = cur_robot->num_labels;

  // Copy all the contents
  memcpy(copy_robot, cur_robot, sizeof(struct robot));
  // We need unique copies of the program and the label cache.
  copy_robot->program_bytecode = cmalloc(program_length);

  src_program_location = cur_robot->program_bytecode;
  dest_program_location = copy_robot->program_bytecode;

  memcpy(dest_program_location, src_program_location, program_length);

  copy_robot->label_list =
   cache_robot_labels(cur_robot, &cur_robot->num_labels);

#ifdef CONFIG_DEBYTECODE
  copy_robot->program_source = NULL;
#endif

  // Give the robot an empty stack.
  copy_robot->stack = NULL;
  copy_robot->stack_size = 0;
  copy_robot->stack_pointer = 0;
  copy_robot->xpos = x;
  copy_robot->ypos = y;

  if(cur_robot->cur_prog_line)
    copy_robot->cur_prog_line = 1;

  copy_robot->pos_within_line = 0;
  copy_robot->status = 0;
}

// Finds a robot ID then duplicates a robot there. Must not be called
// in the editor (use duplicate_robot_source instead).

int duplicate_robot(struct board *src_board, struct robot *cur_robot,
 int x, int y)
{
  int dest_id = find_free_robot(src_board);
  if(dest_id != -1)
  {
    struct robot *copy_robot = cmalloc(sizeof(struct robot));
    duplicate_robot_direct(cur_robot, copy_robot, x, y);
    add_robot_name_entry(src_board, copy_robot, copy_robot->robot_name);
    src_board->robot_list[dest_id] = copy_robot;
  }

  return dest_id;
}

// Makes the dest robot a replication of the source. ID's are given
// so that it can modify the ID table. The dest position is assumed
// to already contain something, and is thus cleared first.
// Will not allow replacing the global robot.
void replace_robot(struct board *src_board, struct robot *src_robot,
 int dest_id)
{
  char old_name[64];
  int x = (src_board->robot_list[dest_id])->xpos;
  int y = (src_board->robot_list[dest_id])->ypos;
  struct robot *cur_robot = src_board->robot_list[dest_id];

  strcpy(old_name, cur_robot->robot_name);

  clear_robot_contents(cur_robot);
  duplicate_robot_direct(src_robot, cur_robot, x, y);
  strcpy(cur_robot->robot_name, old_name);

  if(dest_id)
    change_robot_name(src_board, cur_robot, src_robot->robot_name);
}

// Like duplicate_robot_direct, but for scrolls.
__editor_maybe_static void duplicate_scroll_direct(struct scroll *cur_scroll,
 struct scroll *copy_scroll)
{
  size_t mesg_size = cur_scroll->mesg_size;

  // Copy all the contents
  memcpy(copy_scroll, cur_scroll, sizeof(struct scroll));
  // We need unique copies of the program and the label cache.
  copy_scroll->mesg = cmalloc(mesg_size);
  memcpy(copy_scroll->mesg, cur_scroll->mesg, mesg_size);
}

// Like duplicate_robot_direct, but for sensors.
__editor_maybe_static void duplicate_sensor_direct(struct sensor *cur_sensor,
 struct sensor *copy_sensor)
{
  // Copy all the contents
  memcpy(copy_sensor, cur_sensor, sizeof(struct sensor));
}

int duplicate_scroll(struct board *src_board, struct scroll *cur_scroll)
{
  int dest_id = find_free_scroll(src_board);
  if(dest_id != -1)
  {
    struct scroll *copy_scroll = cmalloc(sizeof(struct scroll));
    duplicate_scroll_direct(cur_scroll, copy_scroll);
    src_board->scroll_list[dest_id] = copy_scroll;
  }

  return dest_id;
}

int duplicate_sensor(struct board *src_board, struct sensor *cur_sensor)
{
  int dest_id = find_free_sensor(src_board);
  if(dest_id != -1)
  {
    struct sensor *copy_sensor = cmalloc(sizeof(struct sensor));
    duplicate_sensor_direct(cur_sensor, copy_sensor);
    src_board->sensor_list[dest_id] = copy_sensor;
  }

  return dest_id;
}

// This function will remove any null entries in the object lists
// (for robots, scrolls, and sensors), and adjust all of the board
// params to compensate. This should always be used before saving
// the world/game, and ideally when loading too.

void optimize_null_objects(struct board *src_board)
{
  int num_robots = src_board->num_robots;
  int num_scrolls = src_board->num_scrolls;
  int num_sensors = src_board->num_sensors;
  struct robot **robot_list = src_board->robot_list;
  struct scroll **scroll_list = src_board->scroll_list;
  struct sensor **sensor_list = src_board->sensor_list;
  struct robot **optimized_robot_list =
   ccalloc(num_robots + 1, sizeof(struct robot *));
  struct scroll **optimized_scroll_list =
   ccalloc(num_scrolls + 1, sizeof(struct scroll *));
  struct sensor **optimized_sensor_list =
   ccalloc(num_sensors + 1, sizeof(struct sensor *));
  int *robot_id_translation_list = ccalloc(num_robots + 1, sizeof(int));
  int *scroll_id_translation_list = ccalloc(num_scrolls + 1, sizeof(int));
  int *sensor_id_translation_list = ccalloc(num_sensors + 1, sizeof(int));
  struct robot *cur_robot;
  struct scroll *cur_scroll;
  struct sensor *cur_sensor;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int i, i2;
  int x, y, offset;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  enum thing d_id;
  int d_param, d_new_param;
  int do_modify = 0;

  for(i = 1, i2 = 1; i <= num_robots; i++)
  {
    cur_robot = robot_list[i];
    if(cur_robot != NULL)
    {
      optimized_robot_list[i2] = cur_robot;
      robot_id_translation_list[i] = i2;
      i2++;
    }
    else
    {
     // FIXME: Hack, probably bogus!
     robot_id_translation_list[i] = 0;
    }
  }

  if(i2 != i)
  {
    do_modify |= 1;
    optimized_robot_list[0] = robot_list[0];
    free(robot_list);
    src_board->robot_list =
     crealloc(optimized_robot_list, sizeof(struct robot *) * i2);
    src_board->num_robots = i2 - 1;
    src_board->num_robots_allocated = i2 - 1;
  }
  else
  {
    free(optimized_robot_list);
  }

  for(i = 1, i2 = 1; i <= num_scrolls; i++)
  {
    cur_scroll = scroll_list[i];
    if(cur_scroll != NULL)
    {
      optimized_scroll_list[i2] = cur_scroll;
      scroll_id_translation_list[i] = i2;
      i2++;
    }
  }

  if(i2 != i)
  {
    do_modify |= 1;
    optimized_scroll_list[0] = scroll_list[0];
    free(scroll_list);
    src_board->scroll_list =
     crealloc(optimized_scroll_list, sizeof(struct scroll *) * i2);
    src_board->num_scrolls = i2 - 1;
    src_board->num_scrolls_allocated = i2 - 1;
  }
  else
  {
    free(optimized_scroll_list);
  }

  for(i = 1, i2 = 1; i <= num_sensors; i++)
  {
    cur_sensor = sensor_list[i];
    if(cur_sensor != NULL)
    {
      // If there's a gap, fill it up
      optimized_sensor_list[i2] = cur_sensor;
      sensor_id_translation_list[i] = i2;
      i2++;
    }
  }

  if(i2 != i)
  {
    do_modify |= 1;
    optimized_sensor_list[0] = sensor_list[0];
    free(sensor_list);
    src_board->sensor_list =
     crealloc(optimized_sensor_list, sizeof(struct sensor *) * i2);
    src_board->num_sensors = i2 - 1;
    src_board->num_sensors_allocated = i2 - 1;
  }
  else
  {
    free(optimized_sensor_list);
  }

  // Make sure this is up to date
  robot_list = src_board->robot_list;

  if(do_modify)
  {
    // Now, physically modify all references on the board
    for(y = 0, offset = 0; y < board_height; y++)
    {
      for(x = 0; x < board_width; x++, offset++)
      {
        d_id = (enum thing)level_id[offset];
        // Is it a robot?
        if(is_robot(d_id))
        {
          d_param = level_param[offset];
          d_new_param = robot_id_translation_list[d_param];
          level_param[offset] = d_new_param;
          // Also, as a service, set the x/y coordinates, just in case
          // they haven't been initialized (this is a potential pitfall)
          cur_robot = robot_list[d_new_param];
          cur_robot->xpos = x;
          cur_robot->ypos = y;
        }
        else

        // Is it a scoll?
        if(is_signscroll(d_id))
        {
          d_param = level_param[offset];
          d_new_param = scroll_id_translation_list[d_param];
          level_param[offset] = d_new_param;
        }
        else

        // Is it a sensor?
        if(d_id == SENSOR)
        {
          d_param = level_param[offset];
          d_new_param = sensor_id_translation_list[d_param];
          level_param[offset] = d_new_param;
        }
      }
    }
  }

  // Free the lists
  free(robot_id_translation_list);
  free(scroll_id_translation_list);
  free(sensor_id_translation_list);
}

#ifdef CONFIG_DEBYTECODE

void prepare_robot_bytecode(struct robot *cur_robot)
{
  if(cur_robot->program_bytecode == NULL)
  {
    cur_robot->program_bytecode =
     assemble_program(cur_robot->program_source,
     &(cur_robot->program_bytecode_length));

    // This was moved here from load robot - only build up the labels once the
    // robot's actually used. But eventually this should be combined with
    // assemble_program.
    cur_robot->label_list =
     cache_robot_labels(cur_robot, &cur_robot->num_labels);

    // Can free source code now.
    free(cur_robot->program_source);
    cur_robot->program_source = NULL;
    cur_robot->program_source_length = 0;
  }
}

#endif /* CONFIG_DEBYTECODE */
