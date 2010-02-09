/* MegaZeux
 *
 * Copyright (C) 2008 Josh Matthews
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

#include "resource.h"
#include "../board_struct.h"
#include "../robot_struct.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

#define HASH_TABLE_SIZE         100

typedef enum status
{
  SUCCESS = 0,
  MALLOC_FAILED
}
status_t;

static char **hash_table[HASH_TABLE_SIZE];

//From http://nothings.org/stb.h, by Sean
static unsigned int stb_hash(char *str)
{
   unsigned int hash = 0;
   while(*str)
      hash = (hash << 7) + (hash >> 25) + *str++;
   return hash + (hash >> 16);
}

static status_t add_to_hash_table(char *stack_str)
{
  unsigned int slot;
  int count = 0;
  size_t len;
  char *str;

  len = strlen(stack_str);
  if(!len)
    return MALLOC_FAILED;

  str = malloc(len + 1);
  if(!str)
    return MALLOC_FAILED;

  strncpy(str, stack_str, len);
  str[len] = '\0';

  slot = stb_hash(stack_str) % HASH_TABLE_SIZE;

  if(!hash_table[slot])
  {
    hash_table[slot] = malloc(sizeof(char *) * 2);
    if(!hash_table[slot])
      return MALLOC_FAILED;
  }
  else
  {
    while(hash_table[slot][count])
    {
      if(!strcasecmp(stack_str, hash_table[slot][count]))
      {
        free(str);
        return SUCCESS;
      }
      count++;
    }

    hash_table[slot] = realloc(hash_table[slot], sizeof(char *) * (count + 2));
  }

  hash_table[slot][count] = str;
  hash_table[slot][count + 1] = NULL;

  return SUCCESS;
}

static int sgetc(char ** const s)
{
  int c = **s;
  (*s)++;
  return c;
}

static unsigned short sgetus(char ** const s)
{
  return (sgetc(s) << 0) | (sgetc(s) << 8);
}

static void sread(char *dest, int unused, int len, char ** const src)
{
  strncpy(dest, *src, len);
  *src += len;
}

static status_t parse_robot_program(char *program, int program_length)
{
  int j;
  char tmp[256], tmp2[256], *str;
  char *orig_program = program;
  status_t ret = SUCCESS;
  sgetc(&program);
  while(1)
  {
    int command_length = sgetc(&program);
    if(!command_length)
      break;
    int command = sgetc(&program);
    int str_len;

    switch(command)
    {
      case 0xa:
        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        str_len = sgetc(&program);

        if(str_len == 0)
        {
          sgetus(&program);
          break;
        }

        sread(tmp2, 1, str_len, &program);

        if(!strcasecmp(tmp2, "FREAD_OPEN")
          || !strcasecmp(tmp2, "SMZX_PALETTE")
          || !strcasecmp(tmp2, "LOAD_GAME")
          || !strncasecmp(tmp2, "LOAD_BC", 7)
          || !strncasecmp(tmp2, "LOAD_ROBOT", 10))
        {
          debug("SET: %s (%s)\n", tmp, tmp2);
          ret = add_to_hash_table(tmp);
          if(ret != SUCCESS)
            return ret;
        }
        break;

      case 0x26:
        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        // ignore MOD *
        if(!strcmp(tmp, "*"))
          break;

        // FIXME: Should only match pairs?
        if(strstr(tmp, "&"))
          break;

        debug("MOD: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0x27:
        str_len = sgetc(&program);

        if(str_len == 0)
          sgetus(&program);
        else
        {
          for(j = 0; j < str_len; j++)
            sgetc(&program);
        }

        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        // FIXME: Should only match pairs?
        if(strstr(tmp, "&"))
            break;

        debug("SAM: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0x2b:
      case 0x2d:
      case 0x31:
        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        str = strtok(tmp, "&");

        // no & enclosed tokens were found
        if(strlen(str) == strlen(tmp))
          break;

        while(str)
        {
          debug("PLAY (class): %s\n", str);
          ret = add_to_hash_table(str);
          if(ret != SUCCESS)
            return ret;

          // tokenise twice, because we only care about
          // every _other '&'; e.g. &programam.sam&ff&programam2.sam&ee
          str = strtok(NULL, "&");
          str = strtok(NULL, "&");
        }
        break;

      case 0xc8:
        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        debug("MOD FADE IN: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0xd8:
        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        debug("LOAD CHAR SET: %s\n", tmp);

        if(tmp[0] == '+')
        {
          char *rest, tempc = tmp[3];
          tmp[3] = 0;
          strtol(tmp + 1, &rest, 16);
          tmp[3] = tempc;
          memmove(tmp, rest, str_len - (rest - tmp));
        }
        else if(tmp[0] == '@')
        {
          char *rest, tempc = tmp[4];
          tmp[4] = 0;
          strtol(tmp + 1, &rest, 10);
          tmp[4] = tempc;
          memmove(tmp, rest, str_len - (rest - tmp));
        }

        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0xde:
        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        debug("LOAD PALETTE: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      case 0xe2:
        str_len = sgetc(&program);

        sread(tmp, 1, str_len, &program);

        debug("SWAP WORLD: %s\n", tmp);
        ret = add_to_hash_table(tmp);
        if(ret != SUCCESS)
          return ret;
        break;

      default:
        program += command_length - 1;
        break;
    }
    sgetc(&program);
    
    if(program - orig_program >= program_length)
      break;
  }
  
  return ret;
}

static status_t parse_board_direct(Board *board)
{
  int i;
  status_t ret;

  if(strlen(board->mod_playing) > 0 && strcmp(board->mod_playing, "*"))
  {
    debug("BOARD MOD: %s\n", board->mod_playing);
    ret = add_to_hash_table(board->mod_playing);
    if(ret != SUCCESS)
      return ret;
  }

  for(i = 0; i <= board->num_robots; i++)
    parse_robot_program(board->robot_list[i]->program, board->robot_list[i]->program_length);

  return SUCCESS;
}

static void free_resource_list(void)
{
  int i;
  for(i = 0; i < HASH_TABLE_SIZE; i++)
  {
    char **p = hash_table[i];

    if(!p)
      continue;

    while(*p)
    {
      free(*p);
      p++;
    }

    free(hash_table[i]);
    hash_table[i] = NULL;
  }
}

static int count_resources(void)
{
  int count = 0;
  int i;
  for(i = 0; i < HASH_TABLE_SIZE; i++)
  {
    char **p = hash_table[i];

    if(!p)
      continue;

    while(*p)
    {
      count++;
      p++;
    }
  }
  return count;
}

void list_resources(World *mzx_world, char ***resources, int *num_resources)
{
  int i, j = 0;
  char **p;
  
  for(i = 0; i < mzx_world->num_boards; i++)
    parse_board_direct(mzx_world->board_list[i]);
  
  *num_resources = count_resources();
  *resources = calloc(*num_resources, sizeof(char *));
  p = hash_table[j];
  for(i = 0; i < *num_resources; i++)
  {
    while(!p || !*p)
      p = hash_table[++j];
    
    *(*resources + i) = strdup(*p);
    p++;
  }
  
  free_resource_list();
}
