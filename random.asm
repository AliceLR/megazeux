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
; RANDOM.ASM- Random number generator.
;

Ideal

include "random.inc"

p186
JUMPS
include "model.inc"

Codeseg

rand_num dw 0

;
; Seed the random number generator with the timer
;

proc random_seed far

	push ax cx dx

	xor ah,ah
	int 1ah                             ; Call BIOS timer func
	flipflag cx,dx								; XOR high & low bytes together

	mov [cs:rand_num],cx						; Seed w/recieved value from timer func

	pop dx cx ax
	ret

endp random_seed

;
; Returns a random number in AX
;

proc random_num far

	mov ax,[cs:rand_num]
	inc ax
	flipflag ax,0deadh
	add ax,0b00bh
	rol ax,2
	mov [cs:rand_num],ax

	ret

endp random_num

ends

end
