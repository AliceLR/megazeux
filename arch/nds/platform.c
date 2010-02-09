/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
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

#include "../../src/platform.h"
#include "../../src/util.h"

#include <fat.h>
#include "render.h"
#include "ram.h"
#include "malloc.h"
#include "exception.h"
#include "memory_warning_pcx.h"

// from arch/nds/event.c
extern void nds_inject_input(void);

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
  TIMER0_DATA = 0; 
  TIMER1_DATA = 0; 
  TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1024; 
  TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;
}

static void nds_on_vblank(void)
{
  /* Handle sleep mode. */
  nds_sleep_check();

  /* Do all special video handling. */
  nds_video_rasterhack();
  nds_video_jitter();

  /* Handle the virtual keyboard and mouse. */
  nds_inject_input();
}

bool platform_init(void)
{
  powerOn(POWER_ALL);
  //setMzxExceptionHandler();
  fatInitDefault();

  // If the "extra RAM" is missing, warn the user
  if(!nds_ram_init(DETECT_RAM))
    warning_screen((u8*)memory_warning_pcx);

  nds_ext_lock();
  timer_init();

  irqSet(IRQ_VBLANK, nds_on_vblank);
  irqEnable(IRQ_VBLANK);

  return true;
}

void platform_quit(void)
{
  nds_ext_unlock();
}
