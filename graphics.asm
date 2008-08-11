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
; GRAPHICS.ASM- Simple text graphics code
;

Ideal

include "graphics.inc"

p186
JUMPS
include "model.inc"

Codeseg

;
; Function- color_string
;
; Reasonably optimized. (186)
;
; Code to output a string direct to video memory, interpreting \n (0x0A) as
; a carraige return + linefeed combo, ~ as an "attribute" setting
; pre-code (follow with 0-F to set foreground), @ as a "background"
; setting pre-code (follow with 0-F to set background), and \0 (0x00) as an
; EOS (end-of-string) code. Carraige returns move down one line (without
; scrolling) and reset the x position to it's value upon entering
; color_string. Reaching the last space onscreen returns to the start of
; that line. If ~ or @ is followed by a \0 it counts as the end of the
; string, and the ~ or @ is ignored. If ~ or @ is followed by anything
; else, the first ~ or @ is ignored and the second character is printed
; as normal. This allows imbedding of actual ~ and @ signs.
;
; A tab (0x05) will NOT SKIP spaces.
;
; Arguments: far pointer to string (byte array)
;            x position (word)
;            y position (word)
;            default color (byte)
;				 video segment (word)
;

proc color_string far
arg string_offs:word,string_segm:word,x_start:word,y_start:word,color:byte,segm:word

	push ax bx cx dx es si di ds	; Conserve registers

	mov ax,[segm]		; Load in segment of video memory
	mov es,ax

	mov bx,[x_start]		; Load current x/y positions
	mov cx,[y_start]

	mov si,[string_offs]				; Offset into string

	mov ax,[string_segm]
	mov ds,ax			; Load in segment of string

	mov dl,[color]		; Load default color

	cld					; Move increasing in string instructions

	; Contents/usage of registers-
	;   DS:SI = string, ES:DI = video, DL = color,
	;   BX = X position, CX = Y position, AL = temp/char

@@recalc_loop:

	; The following code multiplies current y position by 80, adds
	; x position, doubles the total, and stores in di (IE recalculates
	; the current offset)

	mov di,cx
	shl di,2
	add di,cx
	shl di,4
	add di,bx
	add di,di

@@main_loop:
	lodsb					; Read next byte of string and update
	cmp al,0Ah			; Check for return
	je @@return			; Carraige return + line feed
	cmp al,'~'			; Check for ~
	je @@attrib			; Attribute code
	cmp al,'@'			; Check for @
	je @@backgr			; Background code
	cmp al,00h			; Check for EOS
	je @@done			; Done with string

@@printchr:
	; Print character in al with color in dl

	mov [es:di],al		; Output character
	mov [es:di+1],dl  ; Output color

@@nextspot:
	; Update cursor position

	inc bx					; Increase x position
	cmp bx,80   			; Compare to # of columns (are we offscreen?)
	je @@return          ; Do a carraige return
	add di,2             ; Otherwise, fix offset into vid mem and..
	jmp @@main_loop      ; loop.

@@return:
	; Reset X position to starting position, Y position to next lower or same
	; if at lowest position already.

	mov bx,[x_start]		; Load starting x position
	cmp cx,24            ; Last line?
	je @@recalc_loop     ; Yep.
	inc cx               ; Nope, move to next line
	jmp @@recalc_loop    ; Go and recalc

@@attrib:
	; Read next byte for foreground color (0-9, a-f, A-F)
	lodsb					; Read next byte of string and update
	cmp al,0				; EOS?
	je @@done
	cmp al,'0'
	jae @@a_a0
	jmp @@printchr ; Illegal attrib char

@@a_a0:
	cmp al,'A'
	jae @@a_aa
	cmp al,'9'
	ja @@printchr		; Illegal attrib char
							; al is '0' through '9'
	sub al,48         ; Translate to 0-9
	jmp @@aset			; Now set attrib

@@a_aa:
	cmp al,'a'
	jae @@a_aua
	cmp al,'F'
	ja @@printchr	; Illegal attrib char
							; al is 'A' through 'F'
	sub al,55         ; Translate to 0-9
	jmp @@aset			; Now set attrib

@@a_aua:
	cmp al,'f'
	ja @@printchr	; Illegal attrib char
							; al is 'a' through 'f'
	sub al,87         ; Translate to 0-9

@@aset:
	; Set foreground attrib to al
	maskflag dl,0f0h			; Mask out old foreground
	add dl,al         ; Add in new foreground
	jmp @@main_loop ; Loop

@@backgr:
	; Read next byte for background color (0-9, a-f, A-F)
	lodsb					; Read next byte of string and update
	cmp al,0				; EOS?
	je @@done
	cmp al,'0'
	jae @@b_a0
	jmp @@printchr ; Illegal attrib char

@@b_a0:
	cmp al,'A'
	jae @@b_aa
	cmp al,'9'
	ja @@printchr	; Illegal attrib char
							; al is '0' through '9'
	sub al,48         ; Translate to 0-9
	jmp @@bset			; Now set attrib

@@b_aa:
	cmp al,'a'
	jae @@b_aua
	cmp al,'F'
	ja @@printchr	; Illegal attrib char
							; al is 'A' through 'F'
	sub al,55         ; Translate to 0-9
	jmp @@bset			; Now set attrib

@@b_aua:
	cmp al,'f'
	ja @@printchr	; Illegal attrib char
							; al is 'a' through 'f'
	sub al,87         ; Translate to 0-9

@@bset:
	; Set background attrib to al
	maskflag dl,00fh			; Mask out old background
	sal al,4          ; Multiply new background by 16
	add dl,al         ; Add it in
	jmp @@main_loop ; Loop

@@done:
	pop ds di si es dx cx bx ax	; Restore registers
	ret

endp color_string

;
; Function- write_string
;
; Reasonably optimized (186)
;
; Code to output a string direct to video memory, interpreting \n (0x0A) as
; a carraige return + linefeed combo and \0 (0x00) as an EOS (end-of-string)
; code. Carraige returns move down one line (without scrolling) and reset the
; x position to it's value upon entering write_string. Reaching the last
; space onscreen exits the routine.
;
; A tab (0x05) will skip five spaces.
;
; Arguments: far pointer to string (byte array)
;            x position (word)
;            y position (word)
;				 color (byte)
;            video segment (word)
;

proc write_string far
arg string_offs:word,string_segm:word,x_start:word,y_start:word,color:byte,segm:word,taballowed:byte

	push ax bx cx es si di ds	; Conserve registers

	cmp [x_start],79
	ja @@done

	mov bx,[segm]			; Load in segment of video memory
	mov es,bx

	mov si,[string_offs]	; Offset into string

	mov bx,[string_segm]
	mov ds,bx				; Load in segment of string

	mov bx,[x_start]		; Load current x/y positions
	mov cx,[y_start]

	mov ah,[color]		; Read color

	cld					; Move increasing in string instructions

@@recalc_loop:

	; Set di to video offset

	mov di,cx
	shl di,2
	add di,cx
	shl di,4
	add di,bx
	add di,di

	;bx=current x, cx=current y
	;es:di=current video, ds:si=current string
	;al=char, ah=color

@@main_loop:

	lodsb					; Read next byte of string and update
	cmp al,0Ah			; Check for return
	je @@return			; Carraige return + line feed
	cmp al,5				; Tab
	je @@tab
	cmp al,00h			; Check for EOS
	je @@done			; Done with string

@@printchr:
	; Print character in al with color in ah

	stosw

@@nextspot:
	; Update cursor position

	inc bx					; Increase x position
	cmp bx,80 				; Compare to # of columns (are we offscreen?)
	jne @@main_loop    	; Nope, loop
	jmp @@return         ; Yep, do a carraige return

@@tab:
								; SKIP five spaces
	cmp [taballowed],1
	jne @@printchr
	add bx,5
	add di,10
	cmp bx,80            ; Too far?
	jb @@main_loop       ; No, loop. Yes, fall thru to return.

@@return:
	; Reset X position to starting position, Y position to next lower or same
	; if at lowest position already.

	mov bx,[x_start]		; Load starting x position

	inc cx					; Increase y position
	cmp cx,25				; Compare to 25 (offscreen)
	jne @@recalc_loop		; Nope, loop
	; Yep, done

@@done:
	pop ds di si es cx bx ax	; Restore registers
	ret

endp write_string

;
; Function- write_line
;
; Reasonably optimized. (186)
;
; Code to output a string direct to video memory, interpreting \n (0x0A) or
; \0 (0x00) as an EOS (end-of-string) code. Reaching end of line ends the
; routine.
;
; A tab (0x05) will skip five spaces.
;
; A null (0x00) is displayed as a character.
;
; Arguments: far pointer to string (byte array)
;            x position (word)
;            y position (word)
;				 color (byte)
;            video segment (word)
;

proc write_line far
arg string_offs:word,string_segm:word,x_start:word,y_start:word,color:byte,segm:word,taballowed:byte

	push ax bx cx es si di ds	; Conserve registers

	cmp [x_start],79
	ja @@done

	mov bx,[segm]			; Load in segment of video memory
	mov es,bx

	mov si,[string_offs]	; Offset into string

	mov bx,[string_segm]
	mov ds,bx				; Load in segment of string

	mov bx,[x_start]		; Load current x/y positions
	mov cx,[y_start]

	mov ah,[color]		; Read color

	cld					; Move increasing in string instructions

	; Set di to video offset

	mov di,cx
	shl di,2
	add di,cx
	shl di,4
	add di,bx
	add di,di

	;bx=current x, cx=current y
	;es:di=current video, ds:si=current string
	;al=char, ah=color

@@main_loop:

	lodsb					; Read next byte of string and update
	cmp al,0Ah			; Check for return
	je @@done			; Done with string
	cmp al,0
	je @@done
	cmp al,5				; Tab
	je @@tab

@@printchr:
	; Print character in al with color in ah

	stosw

@@nextspot:
	; Update cursor position

	inc bx					; Increase x position
	cmp bx,80 				; Compare to # of columns (are we offscreen?)
	jne @@main_loop    	; Nope, loop
	jmp @@done				; Yep, done

@@tab:
								; SKIP five spaces
	cmp [taballowed],1
	jne @@printchr
	add bx,5
	add di,10
	cmp bx,80            ; Too far?
	jb @@main_loop       ; No, loop. Yes, fall thru to done.

@@done:
	pop ds di si es cx bx ax	; Restore registers
	ret

endp write_line

;
; Function- color_line
;
; Reasonably optimized. (186)
;
; Code to fill a given x/y with color for length
;
; Arguments: length (word)
;            x position (word)
;            y position (word)
;				 color (byte)
;            video segment (word)
;

proc color_line far
arg len:word,x_start:word,y_start:word,color:byte,segm:word

	push ax di cx es 	; Conserve registers

	mov ax,[segm]		; Load in segment of video memory
	mov es,ax

	mov ax,[x_start]		; Load current x/y positions
	mov cx,[y_start]

	;
	; Calculate offset within screen
	;

	mov di,cx
	shl di,2
	add di,cx
	shl di,4
	add di,ax
	add di,di

	inc di				; Point at COLOR

	mov cx,[len]

	mov al,[color]		; Read color

@@col_loop:
	mov [es:di],al  ; Output color
	add di,2			 ; Next color position
	loop @@col_loop ; Loop for CX (length)

	pop es cx di ax	; Restore registers
	ret

endp color_line

;
; Function- fill_line
;
; Reasonably optimized. (186)
;
; Code to fill a given x/y with color and character for length
;
; Arguments: length (word)
;            x position (word)
;            y position (word)
;            char/colr combo (word)
;            video segment (word)
;

proc fill_line far
arg len:word,x_start:word,y_start:word,char_col:word,segm:word

	push ax di cx es 	; Conserve registers

	mov ax,[segm]		; Load in segment of video memory
	mov es,ax

	mov ax,[x_start]		; Load current x/y positions
	mov cx,[y_start]

	;
	; Calculate offset within screen
	;

	mov di,cx
	shl di,2
	add di,cx
	shl di,4
	add di,ax
	add di,di

	mov cx,[len]
	mov ax,[char_col]		; Read char/col

	cld
	rep stosw

	pop es cx di ax	; Restore registers
	ret

endp fill_line

;
; Function- draw_char
;
; Reasonably optimized. (186)
;
; Code to output a single character direct to video memory.
;
; Arguments: character (word)
;				 color (word)
;            x position (word)
;            y position (word)
;            video segment (word)
;

proc draw_char far
arg chr:word,color:word,x_pos:word,y_pos:word,segm:word

	push ax bx es di	; Conserve registers

	cmp [x_pos],79
	ja @@done

	mov ax,[segm]		; Load in segment of video memory
	mov es,ax

	mov bx,[y_pos]
	mov di,bx
	shl di,2
	add di,bx
	shl di,4
	add di,[x_pos]
	add di,di

	mov bx,[color]		; Read color
	mov ax,[chr]

	mov [es:di],al		; Output character
	mov [es:di+1],bl  ; Output color

@@done:
	pop di es bx ax	; Restore registers
	ret

endp draw_char

;
; Function- page_flip
;
; Optimization note- Interrupt casing. (186)
;
; Flips to a different text page. (0 through 3 on EGA text) Does not do any
; vertical retrace waiting.
;
; Arguments: page (char)
;

proc page_flip far
arg pagenum:word

	pusha

	mov ax,[pagenum]
	mov ah,5
	int 10h

	popa

	ret

endp page_flip

;
; Function- clear_screen
;
; Fills the screen with any char/color combo.
;
; Arguments: char/color combo (word)
;            video segment (word)

proc clear_screen
arg c_c_c:word,segm:word

	push es di ax cx

	mov di,[segm]
	mov es,di
	xor di,di
	mov ax,[c_c_c]
	mov cx,2000

	cld
	rep stosw

	pop cx ax di es

	ret

endp

ends

end
