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

// OS-specific clipboard functions.

#include "clipboard.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifndef __WIN32__
#include <limits.h>
#endif

#if defined(CONFIG_SDL)
#include "SDL.h"
#endif

#if defined(CONFIG_X11) && defined(CONFIG_SDL)
#include "SDL_syswm.h"
#include "render_sdl.h"
#endif

#ifdef __WIN32__

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  HANDLE global_memory;
  size_t line_length;
  char *dest_data;
  char *dest_ptr;
  int i;

  // Room for \rs
  total_length += lines - 1;

  global_memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, total_length);
  if(!global_memory)
    return;

  if(!OpenClipboard(NULL))
    return;

  dest_data = (char *)GlobalLock(global_memory);
  dest_ptr = dest_data;

  for(i = 0; i < lines - 1; i++)
  {
    line_length = strlen(buffer[i]);
    memcpy(dest_ptr, buffer[i], line_length);
    dest_ptr += line_length;
    dest_ptr[0] = '\r';
    dest_ptr[1] = '\n';
    dest_ptr += 2;
  }

  line_length = strlen(buffer[i]);
  memcpy(dest_ptr, buffer[i], line_length);
  dest_ptr[line_length] = 0;

  GlobalUnlock(global_memory);
  EmptyClipboard();
  SetClipboardData(CF_TEXT, global_memory);

  CloseClipboard();
}

char *get_clipboard_buffer(void)
{
  HANDLE global_memory;
  char *dest_data;
  char *src_data;
  size_t length;

  if(!IsClipboardFormatAvailable(CF_TEXT) || !OpenClipboard(NULL))
    return NULL;

  global_memory = GetClipboardData(CF_TEXT);

  if(global_memory != NULL)
  {
    src_data = (char *)GlobalLock(global_memory);

    // CF_TEXT is guaranteed to be null-terminated.
    length = strlen(src_data);

    dest_data = cmalloc(length + 1);
    strcpy(dest_data, src_data);

    GlobalUnlock(global_memory);
    CloseClipboard();

    return dest_data;
  }

  return NULL;
}

#elif defined(CONFIG_X11) && defined(CONFIG_SDL)

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

#elif defined(SDL_VIDEO_DRIVER_QUARTZ)

#define decimal decimal_
#define Random Random_
#include <Carbon/Carbon.h>

static const CFStringRef PLAIN = CFSTR("com.apple.traditional-mac-plain-text");

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  PasteboardSyncFlags syncFlags;
  char *dest_data, *dest_ptr;
  PasteboardRef clipboard;
  CFDataRef textData;
  int i, line_length;

  if(PasteboardCreate(kPasteboardClipboard, &clipboard) != noErr)
    return;

  if(PasteboardClear(clipboard) != noErr)
    goto err_release;

  syncFlags = PasteboardSynchronize(clipboard);
  if((syncFlags & kPasteboardModified) ||
    !(syncFlags & kPasteboardClientIsOwner))
    goto err_release;

  dest_data = cmalloc(total_length + 1);
  dest_ptr = dest_data;

  for(i = 0; i < lines - 1; i++)
  {
    line_length = strlen(buffer[i]);
    memcpy(dest_ptr, buffer[i], line_length);
    dest_ptr += line_length;
    dest_ptr[0] = '\n';
    dest_ptr++;
  }

  line_length = strlen(buffer[i]);
  memcpy(dest_ptr, buffer[i], line_length);
  dest_ptr[line_length] = 0;

  textData = CFDataCreate(kCFAllocatorDefault,
   (const UInt8 *)dest_data, total_length + 1);
  free(dest_data);

  if(!textData)
    goto err_release;

  if(PasteboardPutItemFlavor(clipboard, (PasteboardItemID)1,
   PLAIN, textData, 0) != noErr)
    goto err_release;

err_release:
  CFRelease(clipboard);
}

char *get_clipboard_buffer(void)
{
  PasteboardRef clipboard;
  ItemCount itemCount;
  UInt32 itemIndex;
  char *dest_data = NULL;

  if(PasteboardCreate(kPasteboardClipboard, &clipboard) != noErr)
    return NULL;

  if(PasteboardGetItemCount(clipboard, &itemCount) != noErr)
    goto err_release;

  for(itemIndex = 1; itemIndex <= itemCount; itemIndex++)
  {
    CFIndex flavorCount, flavorIndex;
    CFArrayRef flavorTypeArray;
    PasteboardItemID itemID;

    if(PasteboardGetItemIdentifier(clipboard, itemIndex, &itemID) != noErr)
      goto err_release;

    if(PasteboardCopyItemFlavors(clipboard, itemID, &flavorTypeArray) != noErr)
      goto err_release;

    flavorCount = CFArrayGetCount(flavorTypeArray);

    for(flavorIndex = 0; flavorIndex < flavorCount; flavorIndex++)
    {
      CFDataRef flavorData;
      char *src_data;
      size_t length;

      CFStringRef flavorType =
       (CFStringRef)CFArrayGetValueAtIndex(flavorTypeArray, flavorIndex);

      if(!UTTypeConformsTo(flavorType, PLAIN))
        continue;

      if(PasteboardCopyItemFlavorData(clipboard, itemID,
       flavorType, &flavorData) != noErr)
        continue;

      src_data = (char *)CFDataGetBytePtr(flavorData);
      length = (size_t)CFDataGetLength(flavorData);

      dest_data = cmalloc(length + 1);
      memcpy(dest_data, src_data, length);
      dest_data[length] = 0;

      CFRelease(flavorData);
    }
  }

err_release:
  CFRelease(clipboard);
  return dest_data;
}

#elif SDL_VERSION_ATLEAST(2,0,0)

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
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

char *get_clipboard_buffer(void)
{
  char *buffer;
  
  if(!SDL_HasClipboardText())
    return NULL;
  
  buffer = SDL_GetClipboardText();
  return buffer;
}

#else // No system clipboard handler

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length) { }

char *get_clipboard_buffer(void)
{
  return NULL;
}

#endif
