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

;
; Function- detect_processor
;
; Returns processor numeric code from detection
;

proc detect_processor far
	pushf                    ; Save flags
	xor  ax,ax               ; Clear AX
	push ax                  ; Push it on the stack
	popf                     ; Zero the flags
	pushf                    ; Try to zero bits 12-15
	pop  ax                  ; Recover flags
	and  ax,0F000h           ; If bits 12-15 are 1 => i86 or i286
	cmp  ax,0F000h
	jne  @@1

	push cx                  ; save CX
	mov  ax,0FFFFh           ; Set all AX bits
	mov  cl,33               ; Will shift once on 80186
	shl  ax,cl               ; or 33 x on 8086
	pop  cx
	mov  ax,i186
	jnz  @@done
	mov  ax,i86              ; 0 => 8086/8088
	jmp  short @@done

@@1:
	mov  ax,07000h           ; Try to set bits 12-14
	push ax
	popf
	pushf
	pop  ax
	and  ax,07000h           ; If bits 12-14 are 0 => i286
	mov  ax,i286
	jz   @@done

	; 386/486 resolution code taken from WHATCPU.ASM by
	; Dave M. Walker


P386
	mov  eax,cr0
	mov  ebx,eax                 ;Original CR0 into EBX
	or   al,10h                  ;Set bit
	mov  cr0,eax                 ;Store it
	mov  eax,cr0                 ;Read it back
	mov  cr0,ebx                 ;Restore CR0
	test al,10h                  ;Did it set?
	mov  ax,i386sx
	jz   @@done                  ;Jump if 386SX

       ;*** Test AC bit in EFLAGS (386DX won't change)
	mov     ecx,esp                 ;Original ESP in ECX
	pushfd                          ;Original EFLAGS in EBX
	pop     ebx
	and     esp,not 3               ;Align stack to prevent 486
					;  fault when AC is flipped
	mov     eax,ebx                 ;EFLAGS => EAX
	xor     eax,40000h              ;Flip AC flag
	push    eax                     ;Store it
	popfd
	pushfd                          ;Read it back
	pop     eax
	push    ebx                     ;Restore EFLAGS
	popfd
	mov     esp,ecx                 ;Restore ESP
	cmp     eax,ebx                 ;Compare old/new AC bits
	mov     ax,i386dx
	je      @@done
is_486:                                 ;Until the Pentium appears...
	mov   ax,i486
@@done:
	popf
p8086
	ret
endp detect_processor

ends

end