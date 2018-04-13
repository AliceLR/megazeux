/* MegaZeux
 *
 * Copyright (C) 2017 Ian Burgmyer <spectere@gmail.com>
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

#include "clipboard_sdl2.h"

void clipboard_set_sdl2(char **buffer, int lines, int total_length)
{
  int i;
  unsigned long line_length;
  char *dest_data, *dest_ptr;
  
  dest_data = cmalloc(total_length + 1);
  dest_ptr = dest_data;
  
  for(i = 0; i < lines; i++)
  {
    line_length = strlen(buffer[i]);
    memcpy(dest_ptr, buffer[i], line_length);
    dest_ptr += line_length;
    dest_ptr[0] = '\n';
    dest_ptr++;
  }
  
  dest_ptr[-1] = 0;
  SDL_SetClipboardText(dest_data);
  
  free(dest_data);
}

char *clipboard_get_sdl2(void)
{
  return SDL_GetClipboardText();
}
