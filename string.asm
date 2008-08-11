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
; STRING.ASM- Simple functions to work with strings, including
;             color strings for use with color_string in GRAPHICS.ASM.
;

Ideal

include "string.inc"

p186
JUMPS
include "model.inc"

Codeseg

;
; Function- str_len_color
;
; Reasonably optimized. (186)
;
; Get length of a color string- Don't count ~, @, or their following bytes,
; unless the following byte is itself a ~ or @.
; If a ~ or @ is followed by \0, end string count w/o counting either byte.
; These strings are the strings used with color_string in GRAPHICS.ASM.
;
; Arguments: string (char array)
;
; Returns: Length of string (in AX)
;

proc str_len_color far
arg string:dword

	push cx si ds

	lds si,[string]
	xor cx,cx         ; Length count

@@loop:
	lodsb             ; Get char
	cmp al,0				; EOS?
	je @@end          ; YEP.
	cmp al,'~'        ; Check for special characters
	je @@spec
	cmp al,'@'
	je @@spec
	cmp al,5
	je @@tab
@@count:
	inc cx            ; Size up by one
	jmp @@loop

@@spec:
	lodsb             ; Get next character
	cmp al,0          ; EOS?
	je @@end          ; Yep.
	cmp al,'0'
	jb @@count
	cmp al,'9'
	jbe @@loop
	cmp al,'A'
	jb @@count
	cmp al,'F'
	jbe @@loop
	cmp al,'a'
	jb @@count
	cmp al,'f'
	jbe @@loop
	jmp @@count       ; Blah.

@@tab:
	add cx,5
	jmp @@loop

@@end:

	mov ax,cx         ; Return value
	pop ds si cx
	ret

endp str_len_color

;
; Function- str_len
;
; Reasonably optimized. (186)
;
; Gets and returns the length of a normal string, until the first NUL.
;
; Arguments: string (char array)
;
; Returns: Length of string (in AX)
;

proc str_len far
arg string:dword

	push cx di es

	cld
	les di,[string]
	mov cx,65535      ; Search "forever"

	xor al,al
	repne scasb       ; Find a 0

	flipflag cx,65535      ; Get length
	mov ax,cx
	dec ax            ; Decrease, 'cause CX is -- even if 0 is found immed.

	pop es di cx
	ret

endp str_len

;
; Function- str_cpy
;
; Reasonably optimized. (186)
;
; Copies one string to another.
;
; Arguments: destination (char array)
;            source (char array)
;

proc str_cpy far
arg string_dest:dword,string_source:dword

	push ax cx si di es ds

	les di,[string_source]
	lds si,[string_source]

	cld
	mov cx,65535      ; Search "forever"

	xor al,al
	repne scasb       ; Find a 0

	flipflag cx,65535      ; Get length

	les di,[string_dest]

	rep movsb         ; Copy CX bytes

	pop ds es di si cx ax

	ret

endp str_cpy

;
; Function- mem_cpy
;
; Reasonably optimized. (186)
;
; Copies one section of memory to another.
;
; Arguments: destination (char array)
;            source (char array)
;            length (word)
;
; Does it by words. No attempt to word-align is made- All large transfers
; (robots, screens) should already BE word aligned.
;

proc mem_cpy far
arg mem_dest:dword,mem_source:dword,len:word

	push ax cx si di es ds

	mov cx,[len]
	cld
	les di,[mem_dest]
	lds si,[mem_source]

	; Half length and check for an extra byte (in carry flag)

	shr cx,1
	lahf

	rep movsw         ; Copy CX words

	test ah,1
	jz @@end

	movsb             ; Extra byte

@@end:
	pop ds es di si cx ax

	ret

endp mem_cpy

;
; Function- str_cat
;
; Optimization note- Could be optimized more at the expense of space by
;                    inlining the str_len functions.
;
; Copies one string onto the end of another.
;
; Arguments: destination (char array)
;            source (char array)
;

proc str_cat far
arg string_dest:dword,string_source:dword

	push ax si di ds es cx

	les di,[string_dest]
	lds si,[string_source]

	call str_len,di,es ;Also does a CLD
	add di,ax         ; Add length to dest.
	call str_len,si,ds
	mov cx,ax         ; Use as count
	inc cx            ; Copy 0 as well

	rep movsb         ; Copy string

	pop cx es ds di si ax
	ret

endp str_cat

;
; Function- mem_mov
;
; Reasonably optimized. (186)
;
; Copies one section of memory to another. The sections of memory can
; overlap and still be handled properly.
;
; Arguments: destination (char array)
;            source (char array)
;            length (word)
;            code bit (byte) 0 if dest is lower in mem, 1 if dest is higher
;
; Done in words. No attempt to word-align is made, as the major use of this
; (robot re-allocation) should already be aligned.
;

proc mem_mov far
arg mem_dest:dword,mem_source:dword,len:word,code_bit:byte

	push ax cx si di es ds

	mov cx,[len]

	les di,[mem_dest]
	lds si,[mem_source]

	cmp [code_bit],1
	je @@mem_dest_high

	; This code is fine if mem_dest is below mem_source in memory
	; Get number of WORDS

	shr cx,1
	lahf

	cld

	rep movsw         ; Copy CX words

	test ah,1
	jz @@end

	movsb

@@end:
	pop ds es di si cx ax

	ret

	; This code is for when mem_dest is higher in memory
@@mem_dest_high:

	std

	dec cx

	add di,cx
	add si,cx

	inc cx

	rep movsb         ; Copy CX bytes, backwards

	cld
	pop ds es di si cx ax

	ret

endp mem_mov

;
; Function- mem_xor
;
; Reasonably optimized. (186)
;
; XOR a section of memory with a given code.
;
; Arguments: memory (char array)
;            length (word)
;            xor code (byte)
;

proc mem_xor far
arg mem:dword,len:word,xor_with:byte

	push ax cx si di es ds

	mov cx,[len]
	cld
	les di,[mem]
	lds si,[mem]
	mov ah,[xor_with]

@@loop:
	lodsb             ; Load a byte...
	xor al,ah         ; ...XOR it...
	stosb             ; ...Store it...
	loop @@loop       ; ...and loop!

	pop ds es di si cx ax

	ret

endp mem_xor

;
; Function- str_cmp
;
; Reasonably optimized (186)
;
; Compare two strings. Returns 0 if equal, non-0 if not equal
;
; Arguments: two strings
;

;proc _fstricmp far
;arg string_dest:dword,string_source:dword
;
;	push bx si di es ds
;
;	les di,[string_dest]
;	lds si,[string_source]
;
;	cld
;
;@@scnloop:
;	lodsb
;	mov bl,[es:di]
;	inc di
;	cmp al,'A'
;	jb @@ok1
;	cmp al,'Z'
;	ja @@ok1
;	xor al,32
;@@ok1:
;	cmp bl,'A'
;	jb @@ok2
;	cmp bl,'Z'
;	ja @@ok2
;	xor bl,32
;@@ok2:
;	cmp al,bl
;	jne @@bad
;	or al,al
;	jz @@good
;	jmp @@scnloop
;
;@@bad:
;
;	mov ax,0FFFFh
;	pop ds es di si bx
;
;	ret
;
;@@good:
;
;	xor ax,ax
;	pop ds es di si
;
;	ret
;
;endp _fstricmp

proc _fstricmp far
arg string_dest:dword,string_source:dword

	push ds es di si
	lds si,[string_source]
	les di,[string_dest]
@@cloop:
	mov al,[ds:si]
	mov ah,al
	xor al,[es:di]
	and al,223
	jnz @@done
	or ah,ah
	jz @@done
	inc di
	inc si
	jmp @@cloop
@@done:
	pop si di es ds
	ret

endp

;
; Function- str_lwr
;
; Reasonably optimized. (186)
;
; Converts an entire string to lowercase
;
; Arguments: string (char array)
;

proc _fstrlwr far
arg string:dword

	push ax es ds si di

	cld
	les di,[string]
	lds si,[string]

@@lwrloop:
	lodsb
	or al,al
	jz @@done
	cmp al,'A'
	jb @@ok
	cmp al,'Z'
	ja @@ok
	xor al,32
@@ok:
	stosb
	jmp @@lwrloop

@@done:

	pop di si ds es ax
	ret

endp _fstrlwr

proc _fstrupr far
arg string:dword

	push ax es ds si di

	cld
	les di,[string]
	lds si,[string]

@@uprloop:
	lodsb
	or al,al
	jz @@done
	cmp al,'a'
	jb @@ok
	cmp al,'z'
	ja @@ok
	xor al,32
@@ok:
	stosb
	jmp @@uprloop

@@done:

	pop di si ds es ax
	ret

endp _fstrupr

ends

end
