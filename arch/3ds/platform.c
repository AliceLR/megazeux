/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007-2009 Kevin Vance <kvance@kvance.com>
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

#include <stdarg.h>

#include "platform.h"

#include "../../src/platform.h"
#undef main

#include "../../src/util.h"
#include "../../src/event.h"
#include "../../src/error.h"
#include "../../src/graphics.h"

#include <unistd.h>
#include <stdio.h>

#include <3ds.h>
#include <citro3d.h>

#include "keyboard.h"

static u8 isNot2DS;

FILE *popen(const char *command, const char *type) {
	return NULL;
}

int pclose(FILE *stream) {
	return 0;
}

void delay(Uint32 ms)
{
  if(ms > 0)
  {
    svcSleepThread(1e6 * ms);
  }
}

bool ctr_is_2d(void)
{
  return isNot2DS == 0;
}

Uint32 get_ticks(void)
{
  return (Uint32) osGetTime();
}

bool platform_init(void)
{
  cfguInit();
  romfsInit();
  osSetSpeedupEnable(1);
  APT_SetAppCpuTimeLimit(80);

  gfxInitDefault();
  gfxSet3D(false);
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

  CFGU_GetModelNintendo2DS(&isNot2DS);
  return true;
}

void platform_quit(void)
{
  C3D_Fini();
  gfxExit();

  romfsExit();
  cfguExit();
}

void initialize_joysticks(void)
{
  // stub - hardcoded
}

void real_warp_mouse(int x, int y)
{
  // Since we can't warp a touchscreen stylus, focus there instead.
  focus_pixel(x, y);
}

#ifdef CONFIG_CHECK_ALLOC

static void out_of_linear_memory_check(void *p, const char *file, int line)
{
  char msgbuf[128];
  if(!p)
  {
    snprintf(msgbuf, sizeof(msgbuf), "Out of linear memory in %s:%d", file, line);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    error(msgbuf, 2, 4, 0);
  }
}

void *check_linearAlloc(size_t size, size_t alignment, const char *file, int line)
{
  void *result = linearMemAlign(size, alignment);
  out_of_linear_memory_check(result, file, line);
  return result;
}

#endif

int main(int argc, char *argv[])
{
  static char *_argv[] = { (char *)"/" };
  chdir("/");
  real_main(1, _argv);
  return 0;
}
