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
; DETECT.ASM- Code to detect processor and graphics card. Runs on 8088.
;

Ideal

include "detect.inc"

p8086
JUMPS
include "model.inc"

Codeseg

NONE      equ 0
MDA       equ 1
CGA       equ 2
EGAMono   equ 3
EGAColor  equ 4
VGAMono   equ 5
VGAColor  equ 6
MCGAMono  equ 7
MCGAColor equ 8

PS2_CARDS db  0,1,2,2,4,3,2,5,6,2,8,7,8

i86       equ 0
i186      equ 1
i286      equ 2
i386sx    equ 3
i386dx    equ 4
i486      equ 5

;
; Function- detect_graphics
;
; Returns graphics card type as a numeric code.
;

proc detect_graphics far

	mov  ax,1A00h            ; Try calling VGA Identity Adapter function
	int  10h
	cmp  al,1Ah              ; Do we have PS/2 video bios ?
	jne  @@not_PS2           ; No!

	cmp  bl,0Ch              ; bl > 0Ch => CGA hardware
	jg   @@is_CGA            ; Jump if we have CGA
	flipflag  bh,bh
	flipflag  ah,ah
	mov  al,[cs:PS2_CARDS+bx] ; Load ax from PS/2 hardware table
	jmp  short @@done        ; return ax
@@is_CGA:
	mov  ax,CGA              ; Have detected CGA, return id
	jmp  short @@done
@@not_PS2:                       ; OK We don't have PS/2 Video bios
	mov  ah,12h              ; Set alternate function service
	mov  bx,10h              ; Set to return EGA information
	int  10h                 ; call video service
	cmp  bx,10h              ; Is EGA there ?
	je   @@simple_adapter    ; Nop!
	mov  ah,12h              ; Since we have EGA bios, get details
	mov  bl,10h
	int  10h
	or   bh,bh               ; Do we have colour EGA ?
	jz   @@ega_color         ; Yes
	mov  ax,EGAMono          ; Otherwise we have Mono EGA
	jmp  short @@done
@@ega_color:
	mov  ax,EGAColor         ; Have detected EGA Color, return id
	jmp  short @@done
@@simple_adapter:
	int  11h                 ; Lets try equipment determination service
	maskflag  al,30h
	shr  al,4
	flipflag  ah,ah
	or   al,al               ; Do we have any graphics card at all ?
	jz   @@done              ; No ? This is a stupid machine!
	cmp  al,3                ; Do We have a Mono adapter
	jne  @@is_CGA            ; No
	mov  ax,MDA              ; Have detected MDA, return id
@@done:
	ret
endp detect_graphics

ends

end
