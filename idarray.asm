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
; This file contains functions for manipulating the level arrays
;

Ideal

include "idarray.inc"
include "data.inc"
include "const.inc"

p186
JUMPS
include "model.inc"

Codeseg

;
; Place an id/color/param combo in an array position, moving current to
; "under" status if possible, and clearing original "under". If placing an
; "under", then automatically clear lower no matter what. Also mark as
; updated.
;

proc id_place far
arg array_x:word,array_y:word,id:byte,color:byte,param:byte
local @@saved_id:byte

	push ax bx dx ds es di

	mov ax,SEG update_done
	mov ds,ax

	;
	; Calculate offset within arrays
	;

	mov ax,[max_bxsiz]                      ; Multiply 100 by
	mul [array_y]                           ; Y pos,
	add ax,[array_x]                        ; add X pos,
	mov bx,ax                               ; and move into BX.
	push bx
offset_entry:
														 ; BX = Array offset
	les di,[ds:update_done]                 ; Mark as "updated"
	or [BYTE PTR es:di+bx],1

	les di,[ds:level_id]     					 ; Load address of level_id in es:di
	mov bl,[es:di+bx]								 ; Load current id
	mov [@@saved_id],bl							 ; Save id

	mov di,OFFSET flags							 ; Load offset of flags array

	flipflag bh,bh									 ; Clear upper bx
	add bl,bl										 ; BX * 2 (id<128)
														 ; BX=offset into flags array,
														 ; in ds:di
	mov ax,[ds:di+bx]								 ; Get flags word
	pop bx											 ; Restore array offset
														 ; AX contains flags word for id
	testflag ax,A_SPEC_STOOD
	jnz @@sensor
	and ax,A_UNDER									 ; Can id "go under"?
	jz @@no_under									 ; No...
														 ; Get flags for id being PLACED.
	push bx
	mov bl,[id]
	add bl,bl
	flipflag bh,bh
	mov ax,[ds:di+bx]
	pop bx
	testflag ax,A_UNDER
	jnz @@no_under                          ; If placing an UNDER, then ERASE
														 ; current under.
														 ; Yes. Move id/param/color to
														 ; "under"
@@under_ok:
	cmp [id],127
	jne @@under_ok_normal
														 ; Save double under
	mov al,[@@saved_id]							 ; Load id
	les di,[ds:level_under_id]					 ; Load location of under_id's
	xchg [es:di+bx],al							 ; Save id in "under"
	mov [under_player_id],al

	les di,[ds:level_param]
	mov al,[es:di+bx]						 	    ; Load param
	les di,[ds:level_under_param]
	xchg [es:di+bx],al							 ; Save param in "under"
	mov [under_player_param],al

	les di,[ds:level_color]
	mov al,[es:di+bx]								 ; Load color
	les di,[ds:level_under_color]
	xchg [es:di+bx],al							 ; Save color in "under"
	mov [under_player_color],al

	jmp @@under_done								 ; Skip @@no_under part

@@under_ok_normal:
	mov al,[@@saved_id]							 ; Load id
	les di,[ds:level_under_id]					 ; Load location of under_id's
	mov [es:di+bx],al								 ; Save id in "under"

	les di,[ds:level_param]
	mov al,[es:di+bx]						 	    ; Load param
	les di,[ds:level_under_param]
	mov [es:di+bx],al								 ; Save param in "under"

	les di,[ds:level_color]
	mov al,[es:di+bx]								 ; Load color
	les di,[ds:level_under_color]
	mov [es:di+bx],al								 ; Save color in "under"

	jmp @@under_done								 ; Skip @@no_under part

@@sensor:
	cmp [id],127
	jne @@no_under
	; Player -> sensor = ok
	jmp @@under_ok

@@no_under:
	les di,[ds:level_under_id]					 ; Load location of under_id's
														 ; al=0 'cause of prev. and
	xor al,al
	mov [es:di+bx],al								 ; Clear "under" id

	les di,[ds:level_under_param]
	mov [es:di+bx],al								 ; Clear "under" param

	les di,[ds:level_under_color]
	mov [BYTE PTR es:di+bx],7					 ; Clear "under" color

@@under_done:

	les di,[ds:level_id]
	mov al,[id]
	mov [es:di+bx],al								 ; Store new id

	les di,[ds:level_color]
	mov al,[color]
	mov [es:di+bx],al								 ; Store new color

	les di,[ds:level_param]
	mov al,[param]
	mov [es:di+bx],al								 ; Store new param

	pop di es ds dx bx ax
	ret

endp id_place

; "blank" is just that- irrevelant and usually blank.

proc offs_place_id far
arg offs:word,junk:word,id:byte,color:byte,param:byte

	push ax bx dx ds es di

	;
	; Calculate offset within arrays, jump within above function
	;

	mov ax,SEG update_done
	mov ds,ax

	mov bx,[offs]
	push bx
	jmp offset_entry

endp offs_place_id

proc id_place_asm near
arg array_x:word,array_y:word,id_color:word,param:byte
local @@saved_id:byte

	push ax bx cx dx es di

	mov ax,SEG level_id
	mov ds,ax

	;
	; Calculate offset within arrays
	;

	mov bx,[array_y]
	mov ax,[max_bxsiz]                       ; Multiply 100 by
	mul bx                                   ; Y pos,
	add ax,[array_x]

	mov bx,ax
	mov cx,bx

	;
	; BX = Array offset
	;

	les di,[ds:update_done]                ; Mark as "updated"
	or [BYTE PTR es:di+bx],1

	les di,[ds:level_id]    					; Load address of level_id in es:di
	mov bl,[es:di+bx]								; Load current id
	mov [@@saved_id],bl							; Save id
	mov di,OFFSET flags							; Load offset of flags array

	xor bh,bh										; Clear upper bx
	add bl,bl										; BX * 2 (id<128)

	;
	; BX=offset into flags array, in ds:di
	;

	mov ax,[ds:di+bx]								; Get flags word

	mov bx,cx										; Restore array offset
	; AX contains flags word for id
	testflag ax,A_SPEC_STOOD
	jnz @@sensor
	and ax,A_UNDER									; Can id "go under"?
	jz @@no_under									; No...

	; Get flags for id being PLACED.
	;
	mov bl,[BYTE LOW id_color]
	add bl,bl
	xor bh,bh
	mov ax,[ds:di+bx]
	mov bx,cx
	testflag ax,A_UNDER
	jnz @@no_under   ; If placing an UNDER, then ERASE current under.

	;
	; Yes. Move id/param/color to "under"
	;

@@under_ok:
	cmp [BYTE PTR id_color],127
	jne @@under_ok_normal
														 ; Save double under
	mov al,[@@saved_id]							 ; Load id
	les di,[ds:level_under_id]					 ; Load location of under_id's
	xchg [es:di+bx],al							 ; Save id in "under"
	mov [under_player_id],al

	les di,[ds:level_param]
	mov al,[es:di+bx]						 	    ; Load param
	les di,[ds:level_under_param]
	xchg [es:di+bx],al							 ; Save param in "under"
	mov [under_player_param],al

	les di,[ds:level_color]
	mov al,[es:di+bx]								 ; Load color
	les di,[ds:level_under_color]
	xchg [es:di+bx],al							 ; Save color in "under"
	mov [under_player_color],al

	jmp @@under_done								 ; Skip @@no_under part

@@under_ok_normal:
	mov al,[@@saved_id]							; Load id
	les di,[ds:level_under_id]					; Load location of under_id's
	mov [es:di+bx],al								; Save id in "under"

	les di,[ds:level_param]
	mov al,[es:di+bx]								; Load param
	les di,[ds:level_under_param]
	mov [es:di+bx],al								; Save param in "under"

	les di,[ds:level_color]
	mov al,[es:di+bx]								; Load color
	les di,[ds:level_under_color]
	mov [es:di+bx],al								; Save color in "under"

	jmp @@under_done								; Skip @@no_under part

@@sensor:
	cmp [BYTE PTR id_color],127
	jne @@no_under
	; Player -> sensor = ok
	jmp @@under_ok

@@no_under:
	les di,[ds:level_under_id]					; Load location of under_id's
	mov [BYTE PTR es:di+bx],0					; Clear "under" id

	les di,[ds:level_under_param]
	mov [BYTE PTR es:di+bx],0					; Clear "under" param

	les di,[ds:level_under_color]
	mov [BYTE PTR es:di+bx],7					; Clear "under" color

@@under_done:

	les di,[ds:level_id]
	mov ax,[id_color]
	mov [es:di+bx],al								; Store new id

	les di,[ds:level_color]
	mov [es:di+bx],ah								; Store new color

	les di,[ds:level_param]
	mov al,[param]
	mov [es:di+bx],al								; Store new param

	pop di es dx cx bx ax
	ret

endp id_place_asm

;
; Clears (both main and "under") an id in array position given by parameters.
;

proc id_clear
arg array_x:word,array_y:word

	push ax bx dx es ds di

	mov ax,SEG level_id
	mov ds,ax

	;
	; Calculate offset within arrays
	;

	mov ax,[max_bxsiz]						 ; Multiply 100 by
	mul [array_y]                        ; Y pos,
	add ax,[array_x]                     ; add X pos,
	mov bx,ax                            ; and move into BX.

	les di,[ds:level_id]						 ; Load address of level_id in es:di
	xor al,al

	;
	; BX = Array offset  ES:DI = Level Id   DS: = Near segment
	;

	mov [BYTE PTR es:di+bx],al							; Clear ID
	les di,[ds:level_param]
	mov [BYTE PTR es:di+bx],al							; Clear param
	les di,[ds:level_under_id]
	mov [BYTE PTR es:di+bx],al							; Clear under id
	les di,[ds:level_under_param]
	mov [BYTE PTR es:di+bx],al							; Clear under param
	les di,[ds:level_color]
	mov [BYTE PTR es:di+bx],7							; Clear color (grey)
	les di,[ds:level_under_color]
	mov [BYTE PTR es:di+bx],7							; Clear under color (grey)
	les di,[ds:update_done]
	or [BYTE PTR es:di+bx],1                    ; Mark as updated

	pop di ds es dx bx ax
	ret

endp id_clear

;
; Removes the top at the given array position, moving any "under" up.
;

proc id_remove_top
arg array_x:word,array_y:word

	push ax bx dx es ds di

	mov ax,SEG level_id
	mov ds,ax

	;
	; Calculate offset within arrays
	;

	mov ax,[max_bxsiz]                      ; Multiply 100 by
	mul [array_y]                           ; Y pos,
	add ax,[array_x]                        ; add X pos,
	mov bx,ax                               ; and move into BX.

offset_enter_remid:

	;
	; BX = Array offset  ES:DI = Level Under Id   DS: = Near segment
	;
	les di,[ds:level_id]
	cmp [BYTE PTR es:di+bx],127
	jne @@normal_clear
	les di,[ds:level_under_id]             ; Load seg:offs of level_under_id
	xor al,al
	xchg al,[under_player_id]
	xchg al,[es:di+bx]                   ; Load under id
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],al							; Save in normal position
	les di,[ds:level_under_param]
	xor al,al
	xchg al,[under_player_param]
	xchg al,[es:di+bx]							; Load under param
	les di,[ds:level_param]
	mov [BYTE PTR es:di+bx],al							; Save in normal position
	les di,[ds:level_under_color]
	mov al,7
	xchg al,[under_player_color]
	xchg al,[es:di+bx]							; Load under color
	les di,[ds:level_color]
	mov [BYTE PTR es:di+bx],al							; Save in normal position
	jmp @@thingydone

@@normal_clear:
	les di,[ds:level_under_id]             ; Load seg:offs of level_under_id
	mov al,[es:di+bx]                   ; Load under id
	mov [BYTE PTR es:di+bx],0							; then clear it
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],al							; Save in normal position
	les di,[ds:level_under_param]
	mov al,[es:di+bx]							; Load under param
	mov [BYTE PTR es:di+bx],0							; then clear it
	les di,[ds:level_param]
	mov [BYTE PTR es:di+bx],al							; Save in normal position
	les di,[ds:level_under_color]
	mov al,[es:di+bx]							; Load under color
	mov [BYTE PTR es:di+bx],7							; then clear it (with grey)
	les di,[ds:level_color]
	mov [BYTE PTR es:di+bx],al							; Save in normal position
@@thingydone:
	les di,[ds:update_done]
	or [BYTE PTR es:di+bx],1                    ; Mark as done

	pop di ds es dx bx ax
	ret

endp id_remove_top

; "blank" is a junk word

proc offs_remove_id far
arg offs:word,blank:word

	push ax bx dx es ds di

	;
	; Calculate offset within arrays and jump to within above
	;

	mov ax,SEG level_id
	mov ds,ax

	mov bx,[offs]
	jmp offset_enter_remid

endp offs_remove_id

;
; Removes any "under" from the given array position
;

proc id_remove_under
arg array_x:word,array_y:word

	push ax bx dx es ds di

	mov ax,SEG level_id
	mov ds,ax

	;
	; Calculate offset within arrays
	;

	mov ax,[max_bxsiz]                      ; Multiply 100 by
	mul [array_y]                           ; Y pos,
	add ax,[array_x]                        ; add X pos,
	mov bx,ax                               ; and move into BX.

	les di,[ds:level_under_id]
	xor al,al

	;
	; BX = Array offset  ES:DI = Level Under Id   DS: = Near segment
	;

	mov [BYTE PTR es:di+bx],al							; Clear under id
	les di,[ds:level_under_param]
	mov [BYTE PTR es:di+bx],al							; Clear under param
	les di,[ds:level_under_color]
	mov [BYTE PTR es:di+bx],7							; Clear under color (grey)
	les di,[ds:update_done]
	or [BYTE PTR es:di+bx],1                    ; Mark as done

	pop di ds es dx bx ax
	ret

endp id_remove_under

ends

end
