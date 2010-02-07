/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

// internal help system

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrdisp.h"
#include "helpsys.h"
#include "data.h"
#include "graphics.h"
#include "world.h"

static int contexts[128];

int context = 72; // 72 = "No" context link
char *help = NULL;
int curr_context = 0;

void set_context(int con)
{
  contexts[curr_context++] = context;
  context = con;
}

void pop_context(void)
{
  context = contexts[--curr_context];
}

void help_load(World *mzx_world, char *file_name)
{
  help = (char *)malloc(1024 * 64);
  // Search context links
  mzx_world->help_file = fopen(file_name, "rb");

  if(mzx_world->help_file == NULL)
    free(help);
}

void help_system(World *mzx_world)
{
  FILE *fp = mzx_world->help_file;

  if(fp)
  {
    int t1, t2;
    cursor_mode_types old_cmode = get_cursor_mode();
    int where;
    int offs;
    int size;
    char file[512];
    char file2[512];
    char label[512];

    rewind(fp);

    t1 = fgetw(fp);
    fseek(fp, t1 * 21 + 4 + context * 12, SEEK_SET);
    // At proper context info
    where = fgetd(fp);    // Where file to load is
    size = fgetd(fp);     // Size of file to load
    offs = fgetd(fp);     //Offset within file of link

    // Jump to file
    fseek(fp, where, SEEK_SET);
    // Read it in
    fread(help, 1, size, fp);
    // Display it
    cursor_off();

    labelled:

    help_display(mzx_world, help, offs, file, label);

    // File?
    if(file[0])
    {
      //Yep. Search for file.
      fseek(fp, 2, SEEK_SET);
      for(t2 = 0; t2 < t1; t2++)
      {
        fread(file2, 1, 13, fp);
        if(!strcmp(file, file2)) break;
        fseek(fp, 8, SEEK_CUR);
      }
      if(t2 < t1)
      {
        //Found file.
        where = fgetd(fp);
        size = fgetd(fp);
        fseek(fp, where, SEEK_SET);
        fread(help, 1, size, fp);
        // Search for label
        for(t2 = 0; t2 < size; t2++)
        {
          if(help[t2] != 255) continue;
          if(help[t2 + 1] != ':') continue;
          if(!strcmp(help + t2 + 3, label))
            break; // Found label!
        }
        if(t2 < size)
        {
          //Found label. t2 is offset.
          offs = t2;
          goto labelled;
        }
      }
    }
    set_cursor_mode(old_cmode);
  }
}
