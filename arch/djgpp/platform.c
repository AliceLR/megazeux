/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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

#define delay delay_dos
#include <errno.h>
#include <pc.h>
#include <dos.h>
#include <dpmi.h>
#include <sys/segments.h>
#include <sys/exceptn.h>
#undef delay
#include "../../src/util.h"
#include "../../src/platform.h"
#include "platform_djgpp.h"

static const int ps2_cards[] =
{
  DISPLAY_ADAPTER_NONE,
  DISPLAY_ADAPTER_MDA,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_EGA_COLOR,
  DISPLAY_ADAPTER_EGA_MONO,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_VGA_MONO,
  DISPLAY_ADAPTER_VGA_COLOR,
  DISPLAY_ADAPTER_CGA,
  DISPLAY_ADAPTER_MCGA_COLOR,
  DISPLAY_ADAPTER_MCGA_MONO,
  DISPLAY_ADAPTER_MCGA_COLOR
};

const char *disp_names[] =
{
  "None",
  "MDA",
  "CGA",
  "EGA mono",
  "EGA",
  "VGA mono",
  "VGA+",
  "MCGA mono",
  "MCGA"
};

int detect_graphics(void)
{
  __dpmi_regs reg;

  // Try calling VGA Identity Adapter function
  reg.x.ax = 0x1A00;
  __dpmi_int(0x10, &reg);

  // Do we have PS/2 video BIOS?
  if(reg.h.al == 0x1A)
  {
    // BL > 0x0C => CGA hardware
    if(reg.h.bl > 0x0C)
      return DISPLAY_ADAPTER_CGA;
    return ps2_cards[reg.h.bl];
  }
  else
  {
    // Set alternate function service
    reg.h.ah = 0x12;
    // Set to return EGA information
    reg.x.bx = 0x0010;
    __dpmi_int(0x10, &reg);
    // Is EGA there?
    if(reg.x.bx != 0x0010)
    {
      // Since we have EGA BIOS, get details
      reg.h.ah = 0x12;
      reg.h.bl = 0x10;
      __dpmi_int(0x10, &reg);
      // Do we have color EGA?
      if(!reg.h.bh)
        return DISPLAY_ADAPTER_EGA_COLOR;
      else
        return DISPLAY_ADAPTER_EGA_MONO;
    }
    else
    {
      // Let's try equipment determination service
      __dpmi_int(0x11, &reg);
      switch(reg.h.al & 0x30)
      {
        // No graphics card at all? This is a stupid machine!
        case 0x00: return DISPLAY_ADAPTER_NONE;
        case 0x30: return DISPLAY_ADAPTER_MDA;
        default: return DISPLAY_ADAPTER_CGA;
      }
    }
  }
}

#define TIMER_CLOCK  3579545
#define TIMER_LENGTH 8
#define TIMER_COUNT  (TIMER_LENGTH * TIMER_CLOCK / 3000)
#define TIMER_NORMAL 65536

// Defined in interrupt.S
extern int int_lock_start, int_lock_end;
extern unsigned short int_ds;

extern int timer_handler;
extern __dpmi_paddr timer_old_handler;
extern volatile Uint32 timer_ticks;
extern volatile Uint32 timer_offset;
extern Uint32 timer_length;
extern Uint32 timer_count;
extern Uint32 timer_normal;

extern __dpmi_paddr kbd_old_handler;

static boolean yieldable;

void delay(Uint32 ms)
{
  ms += timer_ticks;
  while(timer_ticks < ms)
    if(yieldable)
      __dpmi_yield();
}

Uint32 get_ticks(void)
{
  return timer_ticks;
}

static void set_timer(Uint32 count)
{
  outportb(0x43, 0x34);
  outportb(0x40, count & 0xFF);
  outportb(0x40, count >> 8);
}

boolean platform_init(void)
{
  __dpmi_meminfo region;
  __dpmi_paddr handler;
  unsigned long base;

  // Disable exception on Ctrl-C
  __djgpp_set_ctrl_c(0);

  // Check if DPMI yield function is supported
  errno = 0;
  __dpmi_yield();
  yieldable = (errno != ENOSYS);

  int_ds = _my_ds();

  if(__dpmi_get_segment_base_address(_my_ds(), &base))
  {
    warn("Failed to get segment base address.");
    return false;
  }

  region.address = base + (unsigned long)&int_lock_start;
  region.size = (unsigned long)&int_lock_end - (unsigned long)&int_lock_start;
  if(__dpmi_lock_linear_region(&region))
  {
    warn("Failed to lock interrupt handler region.");
    return false;
  }

  timer_length = TIMER_LENGTH;
  timer_count = TIMER_COUNT;
  timer_normal = TIMER_NORMAL;
  __dpmi_get_protected_mode_interrupt_vector(0x08, &timer_old_handler);

  handler.offset32 = (unsigned long)&timer_handler;
  handler.selector = _my_cs();

  disable();
  if(__dpmi_set_protected_mode_interrupt_vector(0x08, &handler))
  {
    enable();
    warn("Failed to hook timer interrupt.");
    return false;
  }
  set_timer(timer_count);
  enable();

  __dpmi_get_protected_mode_interrupt_vector(0x09, &kbd_old_handler);
  return true;
}

void platform_quit(void)
{
  __dpmi_regs reg;

  // TODO: Add deinit function for event system
  // Unhook keyboard interrupt
  if(__dpmi_set_protected_mode_interrupt_vector(0x09, &kbd_old_handler))
    warn("Failed to unhook keyboard interrupt.");
  // Reset mouse driver
  reg.x.ax = 0;
  __dpmi_int(0x33, &reg);

  disable();
  if(__dpmi_set_protected_mode_interrupt_vector(0x08, &timer_old_handler))
    warn("Failed to unhook timer interrupt.");
  set_timer(timer_normal);
  enable();
}
