/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#include <stdio.h>
#include <stdlib.h>

#include "legacy_robot.h"

#include "const.h"
#include "error.h"
#include "legacy_rasm.h"
#include "rasm.h"
#include "robot.h"
#include "util.h"
#include "world.h"
#include "world_struct.h"


struct robot *legacy_load_robot_allocate(struct world *mzx_world, FILE *fp,
 int savegame, int file_version)
{
  struct robot *cur_robot = cmalloc(sizeof(struct robot));

  cur_robot->stack = NULL;
  cur_robot->label_list = NULL;
  cur_robot->program_bytecode = NULL;
  cur_robot->program_source = NULL;
  cur_robot->num_labels = 0;

  legacy_load_robot(mzx_world, cur_robot, fp, savegame, file_version);
  return cur_robot;
}

// Most of this stuff does not have to be loaded unless savegame
// is set.
void legacy_load_robot_from_memory(struct world *mzx_world,
 struct robot *cur_robot, const void *buffer, int savegame, int version,
 int robot_location)
{
  int program_length, validated_length;
  int i;

  const unsigned char *bufferPtr = buffer;

  cur_robot->world_version = mzx_world->version;

  cur_robot->stack = NULL;
  cur_robot->label_list = NULL;
  cur_robot->program_bytecode = NULL;
  cur_robot->program_source = NULL;
  cur_robot->num_labels = 0;

#ifdef CONFIG_DEBYTECODE
  if(version >= VERSION_SOURCE)
  {
    program_length = mem_getd(&bufferPtr);
  }
  else
#endif
  {
    program_length = mem_getw(&bufferPtr);
    bufferPtr += 2;
  }

#ifdef CONFIG_EDITOR
  cur_robot->command_map = NULL;
  cur_robot->command_map_length = 0;
  cur_robot->commands_total = 0;
  cur_robot->commands_cycle = 0;
  cur_robot->commands_caught = 0;
#endif

  memcpy(cur_robot->robot_name, bufferPtr, 15);
  bufferPtr += 15;

  cur_robot->robot_char = mem_getc(&bufferPtr);
  cur_robot->cur_prog_line = mem_getw(&bufferPtr);
  cur_robot->pos_within_line = mem_getc(&bufferPtr);
  cur_robot->robot_cycle = mem_getc(&bufferPtr);
  cur_robot->cycle_count = mem_getc(&bufferPtr);
  cur_robot->bullet_type = mem_getc(&bufferPtr);
  cur_robot->is_locked = mem_getc(&bufferPtr);
  cur_robot->can_lavawalk = mem_getc(&bufferPtr);
  cur_robot->walk_dir = (enum dir)mem_getc(&bufferPtr);
  cur_robot->last_touch_dir = (enum dir)mem_getc(&bufferPtr);
  cur_robot->last_shot_dir = (enum dir)mem_getc(&bufferPtr);
  cur_robot->xpos = mem_getw(&bufferPtr);
  cur_robot->ypos = mem_getw(&bufferPtr);
  cur_robot->status = mem_getc(&bufferPtr);
  // Skip local - these are in the save files now
  bufferPtr += 2;
  cur_robot->used = mem_getc(&bufferPtr);
  if(version <= V283)
    cur_robot->loop_count = mem_getw(&bufferPtr);
  else
    bufferPtr += 2;

  // Fix xpos and ypos for global robot
  cur_robot->xpos = -(cur_robot->xpos >= 32768) | cur_robot->xpos;
  cur_robot->ypos = -(cur_robot->ypos >= 32768) | cur_robot->ypos;

  // If savegame, there's some additional information to get
  if(savegame)
  {
    int stack_size;

    if(version >= V284)
      cur_robot->loop_count = mem_getd(&bufferPtr);

    for(i = 0; i < 32; i++)
      cur_robot->local[i] = mem_getd(&bufferPtr);

    // Double so we can fit pos_within_line values in.
    stack_size = mem_getd(&bufferPtr) * 2;
    cur_robot->stack_pointer = mem_getd(&bufferPtr) * 2;

    cur_robot->stack = ccalloc(stack_size, sizeof(int));
    for(i = 0; i < stack_size; i += 2)
    {
      cur_robot->stack[i] = mem_getd(&bufferPtr);
      cur_robot->stack[i+1] = 0;
    }

    cur_robot->stack_size = stack_size;

#ifdef CONFIG_DEBYTECODE
    // We're allowing legacy save robots to be loaded because lancer-x is
    // a bad person and relies on savegame type MZMs (even though he claims
    // to have made them in the editor)

    if(version < VERSION_SOURCE)
    {
      // The program is bytecode and we have to convert it to sourcecode.
      char *program_legacy_bytecode = cmalloc(program_length);
      memcpy(program_legacy_bytecode, bufferPtr, program_length);
      bufferPtr += program_length;

      validated_length = validate_legacy_bytecode(program_legacy_bytecode,
       program_length);

      if(validated_length <= 0)
      {
        free(program_legacy_bytecode);
        goto err_invalid;
      }

      else if(validated_length < program_length)
      {
        program_legacy_bytecode = crealloc(program_legacy_bytecode,
         validated_length);
        program_length = validated_length;
      }

      cur_robot->program_bytecode = NULL;
      cur_robot->program_source =
       legacy_disassemble_program(program_legacy_bytecode, program_length,
       &(cur_robot->program_source_length), true, 10);

      free(program_legacy_bytecode);
      prepare_robot_bytecode(mzx_world, cur_robot);

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

      // TODO: We need to validate new bytecode too
      cur_robot->program_bytecode = cmalloc(program_length);
      cur_robot->program_bytecode_length = program_length;
      memcpy(cur_robot->program_bytecode, bufferPtr, program_length);
      bufferPtr += program_length;
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
    cur_robot->loop_count = 0;
    memset(cur_robot->local, 0, sizeof(int) * 32);

    // Give an empty stack.
    cur_robot->stack_size = 0;
    cur_robot->stack = NULL;

    // Initialize the stack pointer to the bottom
    cur_robot->stack_pointer = 0;

#ifdef CONFIG_DEBYTECODE
    // World file loads source code.
    if(version < VERSION_SOURCE)
    {
      if((cur_robot->used) || (program_length >= 2))
      {
        // The program is bytecode and we have to convert it to sourcecode.
        char *program_legacy_bytecode = cmalloc(program_length);
        memcpy(program_legacy_bytecode, bufferPtr, program_length);
        bufferPtr += program_length;

        validated_length = validate_legacy_bytecode(program_legacy_bytecode,
         program_length);

        if(validated_length <= 0)
        {
          free(program_legacy_bytecode);
          goto err_invalid;
        }

        else if(validated_length < program_length)
        {
          program_legacy_bytecode = crealloc(program_legacy_bytecode,
           validated_length);
          program_length = validated_length;
        }

        cur_robot->program_source =
         legacy_disassemble_program(program_legacy_bytecode, program_length,
         &(cur_robot->program_source_length), true, 10);
        free(program_legacy_bytecode);
      }
      else
      {
        // Skip over program, we're not going to use it.
        bufferPtr += program_length;
        cur_robot->program_source = NULL;
        cur_robot->program_source_length = 0;
      }
    }
    else
    {
      // This program is source, just load it straight
      cur_robot->program_source = cmalloc(program_length + 1);
      cur_robot->program_source_length = program_length;

      memcpy(cur_robot->program_source, bufferPtr, program_length);
      bufferPtr += program_length;

      cur_robot->program_source[program_length] = 0;
    }

    // This will become non-null when the robot's executed.
    cur_robot->program_bytecode = NULL;
    cur_robot->program_bytecode_length = 0;
#endif /* CONFIG_DEBYTECODE */
  }

#ifndef CONFIG_DEBYTECODE
  cur_robot->program_bytecode_length = program_length;
  if(program_length > 0)
  {
    cur_robot->program_bytecode = cmalloc(program_length);

    memcpy(cur_robot->program_bytecode, bufferPtr, program_length);
    bufferPtr += program_length;

    validated_length = validate_legacy_bytecode(cur_robot->program_bytecode,
     program_length);

    if(validated_length <= 0)
      goto err_invalid;

    else if(validated_length < program_length)
    {
      cur_robot->program_bytecode = crealloc(cur_robot->program_bytecode,
       validated_length);
      cur_robot->program_bytecode_length = validated_length;
    }

    // Now create a label cache IF the robot is in use
    if(cur_robot->used)
    {
      cur_robot->label_list =
       cache_robot_labels(cur_robot, &cur_robot->num_labels);
    }
  }
#endif /* !CONFIG_DEBYTECODE */

  return;

err_invalid:
  // Don't worry about the unused source and bytecode
  // that were allocated, they'll be cleared later.
  error_message(E_BOARD_ROBOT_CORRUPT, robot_location, NULL);

  clear_robot_contents(cur_robot);
  create_blank_robot(cur_robot);
  create_blank_robot_program(cur_robot);
  strcpy(cur_robot->robot_name, "<<error>>");
  cur_robot->cur_prog_line = 0;
}

size_t legacy_calculate_partial_robot_size(int savegame, int version)
{
  // Calculate the amount of a robot that needs to be read in order to
  // calculate its size

  size_t partial_robot_size = 41;

  if(savegame)
  {
    if(version >= V284)
      partial_robot_size += 4; // loopcount
    partial_robot_size += 32 * 4; // 32 local counters
    partial_robot_size += 2 * 4; // stack size and position
  }
  return partial_robot_size;
}

size_t legacy_load_robot_calculate_size(const void *buffer, int savegame,
 int version)
{
  // This function is used to calculate a robot's total size based on an
  // incomplete load of that robot. This allows the robot size to be
  // calculated before the entire thing is loaded

  const unsigned char *bufferPtr;
  int program_length;
  int stack_size;
  size_t robot_size;

  // First, read the program length (0 bytes in)
  bufferPtr = (unsigned char *)buffer + 0;
  program_length = mem_getd(&bufferPtr);

  // Prior to DBC / VERSION_SOURCE the last two bytes are junk
  #ifdef CONFIG_DEBYTECODE
  if(version < VERSION_SOURCE)
  #endif
    program_length &= 0xFFFF;
  
  // Next, if this is a savegame robot, read the stack size
  if(savegame)
  {
    bufferPtr = (unsigned char *)buffer + 41;
    if(version >= V284) bufferPtr += 4; // Skip over loopcount
    bufferPtr += 32 * 4; // Skip over 32 local counters
    stack_size = mem_getd(&bufferPtr);
  }

  // Now calculate the robot size
  robot_size = 41;
  if(savegame)
  {
    if(version >= V284) robot_size += 4;
    robot_size += 32 * 4 + 2 * 4 + stack_size * 4;
  }
  robot_size += program_length;
  return robot_size;
}

void legacy_load_robot(struct world *mzx_world, struct robot *cur_robot,
 FILE *fp, int savegame, int version)
{
  int robot_location = ftell(fp);
  size_t partial_size = legacy_calculate_partial_robot_size(savegame, version);
  void *buffer = cmalloc(partial_size);
  size_t total_read = 0;
  size_t full_size;

  total_read += fread(buffer, partial_size, 1, fp) * partial_size;
  full_size = legacy_load_robot_calculate_size(buffer, savegame, version);

  buffer = crealloc(buffer, full_size);
  total_read += fread((unsigned char *)buffer + partial_size,
   full_size - partial_size, 1, fp) * (full_size - partial_size);

  create_blank_robot(cur_robot);

  if(total_read != full_size)
  {
    error_message(E_BOARD_ROBOT_CORRUPT, robot_location, NULL);
    create_blank_robot_program(cur_robot);
    strcpy(cur_robot->robot_name, "<<error>>");
    fseek(fp, 0, SEEK_END);
  }
  else
  {
    legacy_load_robot_from_memory(mzx_world, cur_robot, buffer, savegame,
     version, robot_location);
  }
  free(buffer);
}

static void legacy_load_scroll(struct scroll *cur_scroll, FILE *fp)
{
  int scroll_size;

  cur_scroll->mesg = NULL;
  cur_scroll->num_lines = fgetw(fp);
  fseek(fp, 2, SEEK_CUR); // Skip junk
  scroll_size = fgetw(fp);
  cur_scroll->mesg_size = scroll_size;
  cur_scroll->used = fgetc(fp);

  if(scroll_size < 0)
    goto scroll_err;

  cur_scroll->mesg = cmalloc(scroll_size);
  if(!fread(cur_scroll->mesg, scroll_size, 1, fp))
    goto scroll_err;

  return;

scroll_err:
  // Something screwed up, slip in an empty scroll.
  cur_scroll->num_lines = 1;
  cur_scroll->mesg_size = 3;
  cur_scroll->used = 1;

  free(cur_scroll->mesg);
  cur_scroll->mesg = cmalloc(3);
  strcpy(cur_scroll->mesg, "\x01\x0A");
}

struct scroll *legacy_load_scroll_allocate(FILE *fp)
{
  struct scroll *cur_scroll = cmalloc(sizeof(struct scroll));
  legacy_load_scroll(cur_scroll, fp);
  return cur_scroll;
}

static void legacy_load_sensor(struct sensor *cur_sensor, FILE *fp)
{
  if(!fread(cur_sensor->sensor_name, 15, 1, fp))
    goto sensor_err;

  cur_sensor->sensor_char = fgetc(fp);

  if(!fread(cur_sensor->robot_to_mesg, 15, 1, fp))
    goto sensor_err;

  cur_sensor->used = fgetc(fp);

  return;

sensor_err:
  // Something screwed up, insert filler
  strcpy(cur_sensor->sensor_name, "");
  strcpy(cur_sensor->robot_to_mesg, "");
  cur_sensor->sensor_char = 'S';
  cur_sensor->used = 1;
}

struct sensor *legacy_load_sensor_allocate(FILE *fp)
{
  struct sensor *cur_sensor = cmalloc(sizeof(struct sensor));
  legacy_load_sensor(cur_sensor, fp);
  return cur_sensor;
}
