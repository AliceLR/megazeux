/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
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

// Below is a very simple example of the stubs you must implement to port
// to another non-SDL platform. Please note that this file exists to have
// non-SDL builds compile, but is in no way functional.

#include "platform.h"
#include "event.h"

#ifdef CONFIG_AUDIO
#include "audio/audio.h"
#endif

#include <sys/time.h>

#include <unistd.h>
#include <stdio.h>
#include <time.h>

void delay(Uint32 ms)
{
  usleep(1000 * ms);
}

Uint32 get_ticks(void)
{
  struct timeval tv;

  if(gettimeofday(&tv, NULL) < 0)
  {
    perror("gettimeofday");
    return 0;
  }

  return (Uint32)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

boolean platform_init(void)
{
  // stub
  return true;
}

void platform_quit(void)
{
  // stub
}

void initialize_joysticks(void)
{
  // stub
}

boolean __update_event_status(void)
{
  // stub
  return false;
}

boolean __peek_exit_input(void)
{
  // stub
  return false;
}

void __wait_event(void)
{
  // stub
}

void __warp_mouse(int x, int y)
{
  // stub
}

#ifdef CONFIG_AUDIO

void init_audio_platform(struct config_info *conf)
{
  // stub
}

void quit_audio_platform(void)
{
  // stub
}

#endif // CONFIG_AUDIO
