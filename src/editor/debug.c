/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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
#include "../window.h"
#include "../intake.h"
#include "../world.h"

#include <string.h>

#define CVALUE_COL_OFFSET 63

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
    else
      buf[i] = str->value[j];
  }

  buf[i] = 0;
}

static void unescape_string(char *buf)
{
  size_t i, j, len = strlen(buf);

  for(i = 0, j = 0; j < len + 1; i++, j++)
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
    else
      buf[i] = buf[j];
  }
}

void __debug_counters(struct world *mzx_world)
{
  // +1 for mzx_speed
  int num_vars = mzx_world->num_counters + mzx_world->num_strings + 1;
  char **var_list = ccalloc(num_vars, sizeof(char *));
  int dialog_result;
  size_t cp_len;
  int selected = 0;
  int i, i2;
  struct dialog di;
  struct element *elements[3];

  m_show();

  for(i = 0; i < mzx_world->num_counters; i++)
  {
    var_list[i] = cmalloc(76);
    cp_len = strlen(mzx_world->counter_list[i]->name);
    memset(var_list[i], ' ', 75);

    if(cp_len > CVALUE_COL_OFFSET)
      cp_len = CVALUE_COL_OFFSET;

    memcpy(var_list[i], mzx_world->counter_list[i]->name, cp_len);

    var_list[i][cp_len] = ' ';
    sprintf(var_list[i] + CVALUE_COL_OFFSET, "%d",
     mzx_world->counter_list[i]->value);
  }

  // mzx_speed isn't a real counter, so special case here
  var_list[i] = cmalloc(76);
  memset(var_list[i], ' ', 75);
  memcpy(var_list[i], "mzx_speed", 9);
  sprintf(var_list[i] + CVALUE_COL_OFFSET, "%d", mzx_world->mzx_speed);
  i++;

  for(i2 = 0; i2 < mzx_world->num_strings; i2++, i++)
  {
    var_list[i] = cmalloc(76);
    cp_len = strlen(mzx_world->string_list[i2]->name);
    memset(var_list[i], ' ', 75);

    if(cp_len > 16)
      cp_len = 16;

    memcpy(var_list[i],
     mzx_world->string_list[i2]->name, cp_len);

    var_list[i][cp_len] = ' ';

    copy_substring_escaped(mzx_world->string_list[i2], var_list[i] + 17, 58);
  }

  do
  {
    elements[0] = construct_list_box(2, 2, (const char **)var_list,
     num_vars, 19, 75, 0, &selected, false);
    elements[1] = construct_button(23, 22, "Export", 1);
    elements[2] = construct_button(45, 22, "Done", -1);

    construct_dialog_ext(&di, "Debug Variables", 0, 0,
     80, 25, elements, 3, 0, 0, 0, NULL);

    dialog_result = run_dialog(mzx_world, &di);

    if(dialog_result == 0)
    {
      char new_value[70];
      char name[70] = "Edit ";
      int edit_type = 0;
      int offset = selected;

      if(selected > mzx_world->num_counters)
      {
        offset -= mzx_world->num_counters + 1;
        edit_type = 1;

        snprintf(name + 5, 70 - 5, "string %s",
         mzx_world->string_list[offset]->name);

        copy_substring_escaped(mzx_world->string_list[offset], new_value, 68);
      }
      else
      {
        if(selected == mzx_world->num_counters)
        {
          edit_type = -1;
          strncpy(name + 5, "counter mzx_speed", 70 - 5);
          sprintf(new_value, "%d", mzx_world->mzx_speed);
        }
        else
        {
          snprintf(name + 5, 70 - 5, "counter %s",
           mzx_world->counter_list[offset]->name);
          sprintf(new_value, "%d",
           mzx_world->counter_list[offset]->value);
        }
      }

      name[69] = 0;
      save_screen();
      draw_window_box(4, 12, 76, 14, DI_DEBUG_BOX, DI_DEBUG_BOX_DARK,
       DI_DEBUG_BOX_CORNER, 1, 1);
      write_string(name, 6, 12, DI_DEBUG_LABEL, 0);

      if(intake(mzx_world, new_value, 68, 6, 13, 15, 1, 0,
       NULL, 0, NULL) != IKEY_ESCAPE)
      {
        if(edit_type > 0)
        {
          struct string src;

          unescape_string(new_value);
          memset(&src, 0, sizeof(struct string));
          src.length = strlen(new_value);
          src.value = new_value;

          set_string(mzx_world,
           mzx_world->string_list[offset]->name, &src, 0);

          copy_substring_escaped(mzx_world->string_list[offset],
           var_list[selected] + 17, 58);
        }
        else
        {
          int counter_value = strtol(new_value, NULL, 10);
          if(edit_type == -1)
          {
            if(counter_value < 1)
              counter_value = 1;
            if(counter_value > 9)
              counter_value = 9;
            mzx_world->mzx_speed = counter_value;
          }
          else
          {
            set_counter(mzx_world,
             mzx_world->counter_list[offset]->name, counter_value, 0);
          }

          sprintf(var_list[offset] + CVALUE_COL_OFFSET, "%d", counter_value);
        }
      }

      restore_screen();
    }
    else

    if(dialog_result == 1)
    {
      char export_name[128];
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
    }

    destruct_dialog(&di);

  } while(dialog_result != -1);

  m_hide();

  for(i=0;i<num_vars;i++)
    free(var_list[i]);

  free(var_list);
}

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
