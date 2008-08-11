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
; ARROWKEY.ASM- Code to intercept int 9 to handle the keyboard and keep
;               track of the status of arrow keys, space, and delete.
;

Ideal

include "arrowkey.inc"

p186
JUMPS
include "model.inc"

Codeseg

keyb_mode db 0
key_code db 0
up_pressed db 0
dn_pressed db 0
lf_pressed db 0
rt_pressed db 0
sp_pressed db 0
del_pressed db 0
old_vector_09 dd 0
kb_installed db 0
curr_table db 1

; 256 byte table for keyboard codes-
;	0 means pass on to normal handler
;	1 means discard
;	2 means dump and loop for another keypress
;	3 means special (arrow, space, or delete)

action_table db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;000h - 00Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0 ;010h - 01Fh
				 db 0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0 ;020h - 02Fh
				 db 0,0,0,0,0,0,1,1,1,3,0,0,0,0,0,0 ;030h - 03Fh
				 db 0,0,0,0,0,1,1,0,3,0,0,3,0,3,0,0 ;040h - 04Fh
				 db 3,0,0,3,1,0,0,0,0,0,0,0,0,0,0,0 ;050h - 05Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ;060h - 06Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;070h - 07Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;080h - 08Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0 ;090h - 09Fh
				 db 0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0 ;0A0h - 0AFh
				 db 0,0,0,0,0,0,1,1,1,3,0,0,0,0,0,0 ;0B0h - 0BFh
				 db 0,0,0,0,0,1,1,0,3,0,0,3,0,3,0,0 ;0C0h - 0CFh
				 db 3,0,0,3,1,0,0,0,0,0,0,0,0,0,0,0 ;0D0h - 0DFh
				 db 1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ;0E0h - 0EFh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;0F0h - 0FFh

; Table #2 is a more normal table- It lets arrows, space, delete, alt,
;  and shift through normally.

action_tbl2  db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;000h - 00Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0 ;010h - 01Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;020h - 02Fh
				 db 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0 ;030h - 03Fh
				 db 0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0 ;040h - 04Fh
				 db 0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0 ;050h - 05Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ;060h - 06Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;070h - 07Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;080h - 08Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0 ;090h - 09Fh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;0A0h - 0AFh
				 db 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0 ;0B0h - 0BFh
				 db 0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0 ;0C0h - 0CFh
				 db 0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0 ;0D0h - 0DFh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ;0E0h - 0EFh
				 db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ;0F0h - 0FFh

;
; Function- intercept_09
;
; New keyboard handler. (game orientated) Above variables will now contain
; current status (1=pressed, 0=not pressed) of the arrow keys, (both numpad
; and regular) delete, and space. Pause, scroll-lock, num-lock, print screen,
; sysrq, ctrl-c, and ctrl-s are all surpressed. Alt, Ctrl, and Shift are all
; ignored from now on. All other keypressed are interpreted as normal and
; eventually sent to the keyboard buffer to be read as normal. The above
; variables are set to 1 and held there if a key is held, but if you change
; one to another number other than 0 it will stay there instead of flipping
; back to 0. IE if you set rt_pressed to 2 and right is held, it will NOT
; change rt_pressed to 1. Release will still set it to 0 however.
;

proc intercept_09 far
	pusha

@@key_loop:
	; Get scan code
	sti
	in al,60h
	mov [key_code],al

	; Set dl to 0, which means no loop
;	xor dl,dl

	; AL: (according to table above)
	;  48h UP			C8h UP RELEASED
	;  50h DOWN			D0h DOWN RELEASED
	;  4Bh LEFT			CBh LEFT RELEASED
	;  4Dh RIGHT		CDh RIGHT RELEASED
	;  39h SPACE		B9h SPACE RELEASED
	;  53h DELETE		D3h DELETE RELEASED
	;
	;  E0h or E1h : Dump and loop
	;
	; Discard:
	;	1Dh, 2Ah, 36h, 37h, 38h, 45h, 46h, 54h, 6Fh,
	;	9Dh, AAh, B6h, B7h, B8h, C5h, C6h, D4h, EFh

	; Save al in cl, get correct table in CS:BX
	mov cl,al
	mov bx,OFFSET cs:action_table
	cmp [curr_table],0
	je @@table_correct
	add bx,256         ; Table 2.
@@table_correct:

	; xlat to get code
	xlat [cs:bx]

	; Check code- 0 to pass on, 1 to dump, 2 to dump and continue
	cmp al,0
	je @@normal_handler
	cmp al,1
	je @@discard_key
;	cmp al,2
;	jne @@special_key

	; Dump and continue- set dl to 1 to note a loop, then go to dump
;	inc dl
;	jmp @@discard_key

@@special_key:
	; Key press-
	cmp cl,53h
	je @@del
	cmp cl,48h
	je @@up
	cmp cl,50h
	je @@dn
	cmp cl,4Bh
	je @@lf
	cmp cl,4Dh
	je @@rt
	cmp cl,39h
	je @@space

	; Key release-
	cmp cl,0D3h
	je @@del_off
	cmp cl,0C8h
	je @@up_off
	cmp cl,0D0h
	je @@dn_off
	cmp cl,0CBh
	je @@lf_off
	cmp cl,0CDh
	je @@rt_off

	; Must be 0B9h, or @@space_off

@@space_off:
	mov [cs:sp_pressed],0
	jmp @@discard_key
@@del:
	; Only set if not already set somehow
	cmp [cs:del_pressed],0
	jne @@discard_key
	mov [cs:del_pressed],1
	jmp @@discard_key
@@del_off:
	mov [cs:del_pressed],0
	jmp @@discard_key
@@up:
	cmp [cs:up_pressed],0
	jne @@discard_key
	mov [cs:up_pressed],1
	jmp @@discard_key
@@dn:
	cmp [cs:dn_pressed],0
	jne @@discard_key
	mov [cs:dn_pressed],1
	jmp @@discard_key
@@lf:
	cmp [cs:lf_pressed],0
	jne @@discard_key
	mov [cs:lf_pressed],1
	jmp @@discard_key
@@rt:
	cmp [cs:rt_pressed],0
	jne @@discard_key
	mov [cs:rt_pressed],1
	jmp @@discard_key
@@space:
	cmp [cs:sp_pressed],0
	jne @@discard_key
	mov [cs:sp_pressed],1
	jmp @@discard_key
@@up_off:
	mov [cs:up_pressed],0
	jmp @@discard_key
@@dn_off:
	mov [cs:dn_pressed],0
	jmp @@discard_key
@@lf_off:
	mov [cs:lf_pressed],0
	jmp @@discard_key
@@rt_off:
	mov [cs:rt_pressed],0

@@discard_key:

	; Two modes of discarding keys. (second chosen via command line)
	cmp [cs:keyb_mode],1
	je @@discard_key2
	in al,061h
	or al,80h
	out 061h,al
	and al,07Fh
	out 061h,al

	; Loop?
;	cmp dl,1
;	je @@key_loop

	; Reset interrupt
	cli
	mov al,020h
	out 020h,al
	sti
	popa
	iret

@@discard_key2:
	in al,061h
	mov ah,al
	or al,80h
	out 061h,al
	mov al,ah
	out 061h,al

	; Loop?
;	cmp dl,1
;	je @@key_loop

	; Reset interrupt
	cli
	mov al,020h
	out 020h,al
	sti
	popa
	iret

@@normal_handler:

	; Reset ALT+Number work area

	mov bx,ds
	mov ax,0040h
	mov ds,ax
	mov [BYTE PTR ds:0019h],0
	mov ds,bx

	; Not a key important to us- cycle through normal handler.
	popa
	jmp [DWORD PTR cs:old_vector_09]
endp intercept_09

;
; Function- install_i09
;
; Installs new keyboard handler. Necessary before up_pressed, dn_pressed,
; etc. have any meaning.
;

proc install_i09 far
	push ds es
	pusha
	; All keys are released
	xor al,al
	mov [cs:dn_pressed],al
	mov [cs:up_pressed],al
	mov [cs:lf_pressed],al
	mov [cs:rt_pressed],al
	mov [cs:sp_pressed],al
	mov [cs:del_pressed],al
	; Turn off all BIOS shift flags and "Last was E0, E1" flags
	mov ax,0040h
	mov ds,ax
	mov si,0017h
	and [BYTE PTR ds:si],0F0h
	inc si
	and [BYTE PTR ds:si],0E0h
	mov si,0096h
	and [BYTE PTR ds:si],0F0h
	; Intercept int 9
	cli
	mov ah,35h
	mov al,09h
	int 21h
	mov si,OFFSET cs:old_vector_09
	mov [cs:si],bx
	mov [cs:si+2],es
	mov ah,25h
	mov al,09h
	push cs
	pop ds
	mov dx,OFFSET cs:intercept_09
	int 21h
	sti
	mov [cs:kb_installed],1
	popa
	pop es ds
	ret
endp install_i09

;
; Function- uninstall_i09
;
; Uninstalls new keyboard handler. Must be called before exiting program!
;

proc uninstall_i09 far
	push ds es
	pusha
	; Turn off all BIOS shift flags and "Last was E0, E1" flags
	mov ax,0040h
	mov ds,ax
	mov si,0017h
	and [BYTE PTR ds:si],0F0h
	inc si
	and [BYTE PTR ds:si],0E0h
	mov si,0096h
	and [BYTE PTR ds:si],0F0h
	; Restore int 9
	cli
	mov ah,25h
	mov al,09h
	lds dx,[cs:old_vector_09]
	int 21h
	sti
	mov [cs:kb_installed],0
	popa
	pop es ds
	ret
endp uninstall_i09

;
; Function- switch_keyb_table
;
; Changes to table 0 or 1. Table 1 is less strict, letting arrow keys,
; space, delete, alt, and shift through.
;

proc switch_keyb_table far
arg table_num:byte
	pusha
	push ds es
	; All keys are released
	xor al,al
	mov [cs:dn_pressed],al
	mov [cs:up_pressed],al
	mov [cs:lf_pressed],al
	mov [cs:rt_pressed],al
	mov [cs:sp_pressed],al
	mov [cs:del_pressed],al
	; Turn off all BIOS shift flags and "Last was E0, E1" flags
	mov ax,0040h
	mov ds,ax
	mov si,0017h
	and [BYTE PTR ds:si],0F0h
	inc si
	and [BYTE PTR ds:si],0E0h
	mov si,0096h
	and [BYTE PTR ds:si],0F0h
	; Change table number
	mov al,[table_num]
	and al,1
	mov [curr_table],al
	; Done!
	pop es ds
	popa
	ret
endp switch_keyb_table

;
; Replacements for kbhit and getch
;

proc kbhit far
	mov ah,11h
	int 16h
	jz @@nokey
	mov ax,1
	ret
@@nokey:
	xor ax,ax
	ret
endp kbhit

next_press db 0
proc getch far
	cmp [cs:next_press],0
	je @@norm_press
	mov al,[cs:next_press]
	mov [cs:next_press],0
	xor ah,ah
	ret
@@norm_press:
	mov ah,10h
	int 16h
	or al,al
	jz @@ext
	cmp al,224
	jne @@ok
@@ext:
	mov [cs:next_press],ah
	xor al,al
@@ok:
	xor ah,ah
	ret
endp getch

public kbhit
public getch

ends

end