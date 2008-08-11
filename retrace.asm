;  $Id$
;  MegaZeux
;
;  Copyright (C) 1996 Greg Janson
;  Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
;
;  This program is free software; you can redistribute it and/or
;  modify it under the terms of the GNU General Public License as
;  published by the Free Software Foundation; either version 2 of
;  the License, or (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;  General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program; if not, write to the Free Software
;  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

;
; RETRACE.ASM- Code to wait for the vertical retrace.
;

Ideal

include "retrace.inc"

p186
JUMPS
include "model.inc"

Codeseg

;
; Function- wait_retrace
;
; Waits for the vertical retrace start. Won't pick it up in the middle.
;

proc wait_retrace far

	push ax dx

	mov     dx,03DAh
WaitNotVsync:
	in      al,dx
	test    al,08h
	jnz     WaitNotVsync
WaitVsync:
	in      al,dx
	test    al,08h
	jz      WaitVsync

	pop dx ax

	ret

endp wait_retrace

ends

end
