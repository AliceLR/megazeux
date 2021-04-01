/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "clipboard.h"

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{ @autoreleasepool
{
  char *buf = cmalloc(total_length + 1);
  char *pos = buf;
  for(int i = 0; i < lines; i++)
  {
    size_t len = strlen(buffer[i]);
    memcpy(pos, buffer[i], len);
    pos += len;
    *(pos++) = '\n';
  }
  buf[total_length] = '\0';

  /* Convert to NSString. TODO: this function requires 10.3+, haven't
   * found a compelling non-UTF8 way to do this for 10.0.
   */
  NSString *string = [NSString initWithBytes:buf length:total_length
   encoding:NSISOLatin1StringEncoding];
  free(buf);

  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060

  [pasteboard clearContents];
  [pasteboard writeObjects:@[string]];

#else /* VERSION_MIN < 1060 */

  [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pasteboard setString:string forType:NSStringPboardType];

#endif /* VERSION_MIN < 1060 */
}}

char *get_clipboard_buffer(void)
{ @autoreleasepool
{
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  NSArray *chrdata = nil;

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060

  NSArray *values = [[NSArray alloc] initWithObjects:[NSString class], nul];
  NSArray *copiedItems = [pasteboard readObjects];
  // FIXME

#else /* VERSION_MIN < 1060 */

  if(![pasteboard availableTypeFromArray:@[NSStringPboardType, nil]])
    return NULL;

  NSString *string = [pasteboard stringForType:NSStringPboardType];
  chrdata = [string dataUsingEncoding:NSISOLatin1StringEncoding allowLossyConversion:true];

#endif /* VERSION_MIN < 1060 */

  if(!chrdata)
    return NULL;

  size_t buf_len = [array length] + 1;
  char *buf = cmalloc(buf_len);

  [chrdata getBytes:buf length:buf_len];
  buf[buf_len] = '\0';
  return buf;
}}
