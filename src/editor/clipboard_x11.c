/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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
#include <limits.h>

#if defined(CONFIG_SDL)
#include "SDL_syswm.h"
#include "render_sdl.h"
#endif

#include "clipboard.h"
#include "../compat.h"

static char **copy_buffer;
static int copy_buffer_lines;
static int copy_buffer_total_length;

#if SDL_VERSION_ATLEAST(2,0,0)
static int copy_buffer_to_X11_selection(void *userdata, SDL_Event *event)
#else
static int copy_buffer_to_X11_selection(const SDL_Event *event)
#endif
{
  XSelectionRequestEvent *request;
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_Window *window = userdata;
#else
  SDL_Window *window = NULL;
#endif
  char *dest_data, *dest_ptr;
  XEvent response, *xevent;
  SDL_SysWMinfo info;
  int i, line_length;
  Display *display;

  if(event->type != SDL_SYSWMEVENT)
    return 1;

  xevent = SDL_SysWMmsg_GetXEvent(event->syswm.msg);
  if(xevent->type != SelectionRequest || !copy_buffer)
    return 0;

  SDL_VERSION(&info.version);
  SDL_GetWindowWMInfo(window, &info);

  display = info.info.x11.display;
  dest_data = cmalloc(copy_buffer_total_length + 1);
  dest_ptr = dest_data;

  for(i = 0; i < copy_buffer_lines - 1; i++)
  {
    line_length = strlen(copy_buffer[i]);
    memcpy(dest_ptr, copy_buffer[i], line_length);
    dest_ptr += line_length;
    dest_ptr[0] = '\n';
    dest_ptr++;
  }

  line_length = strlen(copy_buffer[i]);
  memcpy(dest_ptr, copy_buffer[i], line_length);
  dest_ptr[line_length] = 0;

  request = &(SDL_SysWMmsg_GetXEvent(event->syswm.msg)->xselectionrequest);
  response.xselection.type = SelectionNotify;
  response.xselection.display = request->display;
  response.xselection.selection = request->selection;
  response.xselection.target = XA_STRING;
  response.xselection.property = request->property;
  response.xselection.requestor = request->requestor;
  response.xselection.time = request->time;

  XChangeProperty(display, request->requestor, request->property,
    XA_STRING, 8, PropModeReplace, (const unsigned char *)dest_data,
    copy_buffer_total_length);

  free(dest_data);

  XSendEvent(display, request->requestor, True, 0, &response);
  XFlush(display);
  return 0;
}

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
  SDL_SysWMinfo info;

  SDL_VERSION(&info.version);

  if(!SDL_GetWindowWMInfo(window, &info) || (info.subsystem != SDL_SYSWM_X11))
    return;

  copy_buffer = buffer;
  copy_buffer_lines = lines;
  copy_buffer_total_length = total_length;

  XSetSelectionOwner(info.info.x11.display, XA_PRIMARY,
    info.info.x11.window, CurrentTime);

  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
  SDL_SetEventFilter(copy_buffer_to_X11_selection, window);
}

char *get_clipboard_buffer(void)
{
  SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
  int selection_format, ret_type;
  unsigned long int nbytes, overflow;
  unsigned char *src_data;
  char *dest_data = NULL;
  Window xwindow, owner;
  Atom selection_type;
  SDL_SysWMinfo info;
  Display *display;

  SDL_VERSION(&info.version);

  if(!SDL_GetWindowWMInfo(window, &info) || (info.subsystem != SDL_SYSWM_X11))
    return NULL;

  display = info.info.x11.display;
  xwindow = info.info.x11.window;
  owner = XGetSelectionOwner(display, XA_PRIMARY);

  if((owner == None) || (owner == xwindow))
    return NULL;

  XConvertSelection(display, XA_PRIMARY, XA_STRING, None,
    owner, CurrentTime);

  XLockDisplay(display);

  ret_type =
    XGetWindowProperty(display, owner,
    XA_STRING, 0, INT_MAX / 4, False, AnyPropertyType, &selection_type,
    &selection_format, &nbytes, &overflow, &src_data);

  if((ret_type != Success) || ((selection_type != XA_STRING) &&
    (selection_type != XInternAtom(display, "TEXT", False)) &&
    (selection_type != XInternAtom(display, "COMPOUND_TEXT", False)) &&
    (selection_type != XInternAtom(display, "UTF8_STRING", False))))
    goto err_unlock;

  dest_data = cmalloc(nbytes + 1);
  memcpy(dest_data, src_data, nbytes);
  dest_data[nbytes] = 0;

  XFree(src_data);

err_unlock:
  XUnlockDisplay(display);
  return dest_data;
}
