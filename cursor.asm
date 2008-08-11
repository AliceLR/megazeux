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
; CURSOR.ASM- Functions to change the cursor's shape and position.
;

Ideal

include "cursor.inc"

p186
JUMPS
include "model.inc"

Codeseg

cursor_mode db CURSOR_UNDERL

;
; Function- cursor_underline
;
; Activate traditional underline cursor.
;

proc cursor_underline far

	pusha

	mov cx,12+11*256
	mov ax,0103h
	int 10h
	mov [cursor_mode],CURSOR_UNDERL

	popa
	ret

endp cursor_underline

;
; Function- cursor_solid
;
; Activate solid cursor for editing.
;

proc cursor_solid far

	pusha

	mov cx,13+0*256
	mov ax,0103h
	int 10h
	mov [cursor_mode],CURSOR_BLOCK

	popa
	ret

endp cursor_solid

;
; Function- cursor_off
;
; Turns cursor off.
;

proc cursor_off far

	pusha

	mov cx,0+31*256
	mov ax,0103h
	int 10h
	mov [cursor_mode],CURSOR_INVIS

	popa
	ret

endp cursor_off

;
; Funciton- move_cursor
;
; Position cursor on all four pages.
;
; Arguments: x position (word)
;            y position (word)
;

proc move_cursor far
arg x_pos:word,y_pos:word

	pusha

	mov ax,0203h						; Service 2 of int 10h (al=3 for video mode)
	mov dx,[x_pos]					; column in dl.
	mov bx,[y_pos]					; Row in dh.
	mov dh,bl
	xor bx,bx						; Page 0
	push ax bx dx
	int 10h							; Call BIOS int
	pop dx bx ax
	inc bh                     ; Do for pages 1, 2, and 3
	push ax bx dx
	int 10h
	pop dx bx ax
	inc bh
	push ax bx dx
	int 10h
	pop dx bx ax
	inc bh
	int 10h

	popa
	ret

endp move_cursor

ends

end
