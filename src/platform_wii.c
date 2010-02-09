/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#include "platform.h"
#undef main

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define BOOL _BOOL
#include <ogc/system.h>
#include <ogc/lwp.h>
#include <ogc/video.h>
#include <fat.h>
#undef BOOL

#define STACKSIZE 8192

static volatile Uint32 tickcount = 0;
static lwp_t tick_thread;
static u8 tick_stack[STACKSIZE];

static lwpq_t reset_queue;
static lwp_t reset_thread;
static u8 reset_stack[STACKSIZE];

static void* pal_thread(void *dud)
{
  while(1)
  {
    VIDEO_WaitVSync();
    tickcount += 20;
  }
  return 0;
}

static void* ntsc_thread(void *dud)
{
  static Uint32 delta = 0;
  while(1)
  {
    VIDEO_WaitVSync();
    delta += 1001;
    tickcount += delta / 60;
    delta %= 60;
  }
  return 0;
}

void delay(Uint32 ms)
{
  static Uint32 err = 0;
  Uint32 i = tickcount + ms - err;
  while(tickcount < i)
    VIDEO_WaitVSync();
  err = tickcount - i;
  if(err > 20) err = 20;
}

Uint32 get_ticks(void)
{
  return tickcount;
}

// Emergency exit
static void *wii_reset_thread(void *dud)
{
  LWP_ThreadSleep(reset_queue);
  platform_quit();
  exit(0);
  return 0;
}

static void reset_callback(void)
{
  static volatile int callcount = 0;
  callcount++;
  if(callcount > 5)
    *(int *)NULL = 0xDEADCAFE; // Try to cause an exception if pressed 6 times
  LWP_ThreadSignal(reset_queue);
}

int platform_init(void)
{
  GXRModeObj *rmode = NULL;

  VIDEO_Init();
  rmode = VIDEO_GetPreferredMode(NULL);
  if (rmode->viTVMode == VI_TVMODE_PAL_INT)
    LWP_CreateThread(&tick_thread, pal_thread, NULL, tick_stack, STACKSIZE,
     120);
  else
    LWP_CreateThread(&tick_thread, ntsc_thread, NULL, tick_stack, STACKSIZE,
     120);

  LWP_InitQueue(&reset_queue);
  LWP_CreateThread(&reset_thread, wii_reset_thread, NULL, reset_stack,
   STACKSIZE, 127);
  SYS_SetResetCallback(reset_callback);

  if(!fatInit(1024, true)) // Allocate 512kB of cache
    return 0;

  return 1;
}

void platform_quit(void)
{
  if (!fatUnmount(PI_DEFAULT))
    fatUnsafeUnmount(PI_DEFAULT);
}

// argc/argv may be invalid on the Wii :(
int main(int argc, char *argv[])
{
  static char _argv0[] = SHAREDIR "megazeux";
  static char *_argv[] = {_argv0};

  real_main(1, _argv);
}

// clock() is unlinkable
clock_t clock(void)
{
  return 0;
}
