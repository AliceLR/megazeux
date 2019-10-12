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
#include <crt0.h>
#include <sys/segments.h>
#include <sys/exceptn.h>
#undef delay
#include "../../src/util.h"
#include "../../src/platform.h"
#include "platform_djgpp.h"

int djgpp_display_adapter_detect(void)
{
  __dpmi_regs reg;

  // VESA SuperVGA BIOS (VBE) - GET SuperVGA INFORMATION
  // Generally supported by SVGA cards
  reg.x.ax = 0x4F00;
  __dpmi_int(0x10, &reg);

  if (reg.x.ax == 0x004F)
    return DISPLAY_ADAPTER_SVGA;

  // VIDEO - GET DISPLAY COMBINATION CODE (PS,VGA/MCGA)
  // Generally supported by VGA cards
  reg.x.ax = 0x1A00;
  __dpmi_int(0x10, &reg);

  if (reg.h.al == 0x1A)
  {
    switch (reg.h.bl) {
      case 0x04: case 0x05:
        return DISPLAY_ADAPTER_EGA;
      case 0x07: case 0x08:
        return DISPLAY_ADAPTER_VGA;
      default:
        return DISPLAY_ADAPTER_UNSUPPORTED;
    }
  }

  // VIDEO - ALTERNATE FUNCTION SELECT (PS, EGA, VGA, MCGA) - GET EGA INFO
  // Generally supported by EGA cards
  reg.h.ah = 0x12;
  reg.x.bx = 0xFF10;
  __dpmi_int(0x10, &reg);

  if (reg.h.bh != 0xFF)
    return DISPLAY_ADAPTER_EGA;

  return DISPLAY_ADAPTER_UNSUPPORTED;
}

const char *disp_adapter_names[] =
{
  "Unsupported",
  "EGA",
  "VGA",
  "SVGA"
};

const char *djgpp_display_adapter_name(int adapter)
{
  return disp_adapter_names[adapter];
}

int djgpp_malloc_boundary(int len_bytes, int boundary_bytes, int *selector)
{
  int len_segment = (len_bytes + 15) >> 4;
  int boundary_mask = ~((boundary_bytes - 1) >> 4);
  int segment = __dpmi_allocate_dos_memory((len_bytes + 7) >> 3, selector);
  if (((segment + len_segment - 1) & boundary_mask) != (segment & boundary_mask))
    segment += len_segment;
  return segment;
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
  {
    if(yieldable)
      __dpmi_yield();
  }
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
