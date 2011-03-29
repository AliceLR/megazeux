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

#include "../../src/platform.h"
#undef main

#include "../../src/util.h"

#include <fat.h>
#include "render.h"
#include "ram.h"
#include "dlmalloc.h"
#include "exception.h"

// from arch/nds/event.c
extern void nds_update_input(void);

// Timer code stolen from SDL (LGPL)

#define timers2ms(tlow,thigh)(tlow | (thigh<<16)) >> 5

void delay(Uint32 ms)
{
  Uint32 now;
  now = timers2ms(TIMER0_DATA, TIMER1_DATA);
  while((Uint32)timers2ms(TIMER0_DATA, TIMER1_DATA) < now + ms)
    ;
}

Uint32 get_ticks(void)
{
  return timers2ms(TIMER0_DATA, TIMER1_DATA);
}

static void timer_init(void)
{
  // TIMER0, TIMER1: Tick timer
  TIMER0_DATA = 0; 
  TIMER1_DATA = 0; 
  TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1024; 
  TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;

  // TIMER2: Input handler IRQ
  // We could do this in vblank, but sometimes we need to block new input
  // events when the event queue is in use.  Using a hardware timer, we can
  // keep the graphics flicker running.
  TIMER2_DATA = TIMER_FREQ_1024(60);
  TIMER2_CR = TIMER_ENABLE | TIMER_DIV_1024 | TIMER_IRQ_REQ;
  irqSet(IRQ_TIMER2, nds_update_input);
  irqEnable(IRQ_TIMER2);
}

/*
 * Special handling for debug output
 */

static __attribute__((format(printf, 2, 0)))
void handle_debug_info(const char *type, const char *format, va_list args)
{
  if(!has_video_initialized())
  {
    iprintf("%s: ", type);
    viprintf(format, args);
    fflush(stdout);
  }
  // TODO: Have a debug console on the subscreen show these.
}

void info(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  handle_debug_info("INFO", format, args);
  va_end(args);
}

void warn(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  handle_debug_info("WARNING", format, args);
  va_end(args);
}

#ifdef DEBUG
void debug(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  handle_debug_info("DEBUG", format, args);
  va_end(args);
}
#endif // DEBUG

bool platform_init(void)
{
  powerOn(POWER_ALL);
  setMzxExceptionHandler();

  if(!fatInitDefault())
  {
    iprintf("Unable to initialize FAT.\n"
            "Check your DLDI driver.\n");
    return false;
  }

  // If the "extra RAM" is missing, warn the user
  if(!REG_DSIMODE)
    if(nds_ram_init(DETECT_RAM))
      nds_ext_lock();

  timer_init();

  // Enable vblank interrupts, but don't install the handler until the
  // graphics have initialized.
  irqEnable(IRQ_VBLANK);

  return true;
}

void platform_quit(void)
{
  nds_ext_unlock();
}

// argc/argv is unreliable on NDS.  On my R4, argv[0] is an invalid pointer.

int main(int argc, char *argv[])
{
  static char _argv0[] = SHAREDIR "/mzxrun.nds";
  static char *_argv[] = {_argv0};
  consoleDemoInit();
  real_main(1, _argv);
  while(true)
    ;
}
