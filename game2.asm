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
; GAME2.ASM- Main update function and support functions
;

Ideal

include "runrobot.inc"
include "game.inc"
include "sfx.inc"
include "counter.inc"
include "game2.inc"
include "data.inc"
include "random.inc"
include "string.inc"
include "const.inc"
include "idarray.inc"
include "trig.inc"

p386
JUMPS
include "model.inc"

Dataseg

return_value dw ?
multiplier dw 10000
divider dw 10000
c_divisions dw 360

const_2pi dt 6.2831853071796

slow_down db 0							; Global slowdown for most objects
gem_name db "GEMS",0             ; Misc. strings
health_name db "HEALTH",0
ouch db "Ouch!",0

;For missile turning (directions)

cwturndir db 2,3,1,0
ccwturndir db 3,2,0,1

; OPEN DOOR movement directions, use bits 1,2,4,8,16 to index it.
; 0ffh=no movement.

label open_door_movement
db 3,0,2,0,3,1,2,1 ; From "open 1/2" to "open"
db 0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh ; From "open" to "wait"
db 2,1,3,1,2,0,3,0 ; From "wait" to "close 1/2"
db 1,2,1,3,0,2,0,3 ; From "close 1/2" to reg. door

; Bits for WAIT, in proper bit form, for opening doors. Use bits 1-16.

label open_door_max_wait
db 32,32,32,32,32,32,32,32
db 224,224,224,224,224,224,224,224
db 224,224,224,224,224,224,224,224
db 32,32,32,32,32,32,32,32

MACRO hurt_player_id _id
	push es
	pusha
	mov al,[ds:id_dmg+_id]
	xor ah,ah
	mov bx,OFFSET health_name
	call dec_counter,bx,ds,ax,0  ; Decrease health by ax
	call play_sfx,21				  ; Hurt sound
	mov bx,OFFSET ouch           ; Say OUCH
	call set_mesg,bx,ds
	popa
	pop es
ENDM hurt_player_id


MACRO dispmesg mesgname
	push ax bx cx dx
	mov dx,mesgname
	call set_mesg,dx,ds
	pop dx cx bx ax
ENDM

; Find seek dir if "_x","_y" is the location to "seek" from.
; Loads into "_dest".
; All must be registers. _dest cannot be al or ah. (ax okay)

MACRO find_seek _x,_y,_dest
LOCAL @@horiz,@@vert,@@end,@@up,@@left

	;; Only save ax if not the destination

IFDIFI <_dest>,<ax>
	push ax
ENDIF

	cmp _y,[player_y]
	je @@horiz
	cmp _x,[player_x]
	je @@vert

	;; Diagonal movement req.

	call random_num         ;; Get random number
	testflag ax,1           ;; Odd?
	jz @@horiz              ;; Horiz 50%
@@vert:                    ;; Vert 50%

	;; Dir becomes up or down.

	cmp [player_y],_y
	jb @@up

	;; down

	mov _dest,1
	jmp @@end
@@up:
	mov _dest,0
	jmp @@end
@@horiz:

	;; Dir becomes left or right.

	cmp [player_x],_x
	jb @@left

	;; right

	mov _dest,2
	jmp @@end
@@left:
	mov _dest,3
@@end:

IFDIFI <_dest>,<ax>
	pop ax
ENDIF
ENDM find_seek

; Reverse dir in val (dir is 0-3 in the two LSB)
MACRO flip_dir val
	flipflag val,1
ENDM flip_dir

MACRO sfxp num
	push ax bx cx dx es
	call play_sfx,num
	pop es dx cx bx ax
ENDM

; AH contains param, ES:DI contains level_param, BX contains array offset
; Increases current parameter and stores

MACRO inc_param max
LOCAL @@end1,@@end2
	cmp ah,max        					;; At end of anim
	je @@end1         					;; Yep.
	inc ah                          	;; Nope.
	mov [es:di+bx],ah 					;; Increase anim
	jmp @@end2
@@end1:
	mov [BYTE PTR es:di+bx],0 			;; Restart anim
@@end2:
ENDM inc_param

MACRO inc_param_and_end max
LOCAL @@end1
	cmp ah,max        					;; At end of anim
	je @@end1         					;; Yep.
	inc ah                          	;; Nope.
	mov [es:di+bx],ah 					;; Increase anim
	jmp @@inner_end
@@end1:
	mov [BYTE PTR es:di+bx],0 			;; Restart anim
	jmp @@inner_end
ENDM inc_param_and_end

Codeseg

;
; MACRO to take x/y pos and return array offset
; X in CX, Y in dX. Does NOT affect any registers except AX (dest.)
;

MACRO _xy2array2

	mov ax,[max_bxsiz]  ;; Load maxsize and
	push dx
	mul dx              ;; multiply by Y, storing in AX
							  ;; (the new DX is irrevelant)
	pop dx
	add ax,cx           ;; Add X.

ENDM _xy2array2
  
;
; Internal procedure to take an x/y pos and a direction and return
; a new x/y pos. Returns 0FFFFh if offscreen in that direction.
;
; X in CX, Y in DX, direction in AL.
;

proc _arraydir2 near

	cmp al,0 ; North?
	je @@n
	cmp al,1 ; South?
	je @@s
	cmp al,2 ; East?
	je @@e
	; West
	dec cx              ; Decrease. If < 0, auto 0FFFFh
	ret
@@n:
	dec dx              ; Decrease. If < 0, auto 0FFFFh
@@done:
	ret
@@s:
	inc dx              ; Increase
	cmp dx,[board_ysiz]
	jne @@done          ; Not too big.
	mov dx,0FFFFh       ; Overflow.
	ret
@@e:
	inc cx              ; Increase
	cmp cx,[board_xsiz]
	jne @@done          ; Not too big.
	mov cx,0FFFFh       ; Overflow.
	ret

endp _arraydir2

;
; Update ALL of the stuff on-screen
;

proc update_screen far
local @@ex_id:byte,@@ex_prm:byte

	pusha
	push ds es

	; Flip slow_down to slow certain things down

	xor [ds:slow_down],1

	; Clear all robots' status code

	les di,[robots]
	mov cx,NUM_ROBOTS
	add di,35
	xor ax,ax
	cld
@@carscl:
	mov [es:di],al
	add di,41
	dec cx
	jnz @@carscl

	; Clear update_done array

	les di,[update_done]
	mov cx,5000
	rep stosw
	xor dx,dx

	; BX = array offset, CX = X, DX = Y
	; X and Y already cleared from loop above

	xor bx,bx                    						; Clear offset
	push bx                         					; Save offset

@@outer:                                        ; Y loop
	xor cx,cx                     					; Clear X

@@inner:                                        ; X loop
	les di,[ds:level_id]
	mov al,[es:di+bx] 									; Load id
	cmp al,25
	jb @@inner_end 										; Space thru W Water don't
																; get updated!
	les di,[ds:update_done]
	test [BYTE PTR es:di+bx],1 						; Is update already done?
	jnz @@inner_end   									; Yep, done this square
																; already
	xor ah,ah       										; Fill AX with id
	push bx           									; Save offset
	mov bx,ax                       					; Make the id the offset
	add bl,bl         									; Double id
	mov si,[ds:flags+bx] 								; Get flags byte
	pop bx	          									; Restore array offset
	testflag si,A_UPDATE    							; Test flags for UPDATE
	jz @@inner_end    									; No? Continue inner loop
	les di,[ds:level_param]
	mov ah,[es:di+bx] 									; Load param

	; BX=offset, CX=X, DX=Y, AL=id, AH=param, SI=flags, ES:DI=level_param
	; Don't EVER change BX, CX, or DX within the loop!

	; Check id, take appropriate action

	cmp al,123
	je @@robot
	cmp al,124
	je @@robot
	cmp al,25
	je @@ice
	cmp al,26
	je @@lava
	cmp al,63
	je @@fire
	cmp al,61
	je @@bullet
	cmp al,38
	je @@explosion
	cmp al,45
	je @@cw
	cmp al,46
	je @@ccw
	cmp al,49
	je @@transport
	cmp al,78
	je @@shootingfire
	cmp al,62
	je @@missile
	cmp al,79
	je @@seeker
	cmp al,59
	je @@lazer
	cmp al,56
	je @@pusher
	cmp al,80
	je @@snake
	cmp al,81
	je @@eye
	cmp al,82
	je @@thief
	cmp al,83
	je @@slime
	cmp al,84
	je @@runner
	cmp al,85
	je @@ghost
	cmp al,86
	je @@dragon
	cmp al,87
	je @@fish
	cmp al,88
	je @@shark
	cmp al,89
	je @@spider
	cmp al,90
	je @@goblin
	cmp al,91
	je @@tiger
	cmp al,92
	je @@bulletgun
	cmp al,93
	je @@spinning
	cmp al,94
	je @@bear
	cmp al,95
	je @@cub
	cmp al,33
	je @@energizer
	cmp al,37
	je @@litbomb
	cmp al,42
	je @@opendoor
	cmp al,48
	je @@opengate
	cmp al,51
	jb @@not_move_wall
	cmp al,54
	jbe @@movingwall
@@not_move_wall:
	cmp al,60
	je @@lazgun
	cmp al,66
	je @@life
	cmp al,67
	jb @@not_wp
	cmp al,70
	jbe @@whirlpool
@@not_wp:
	cmp al,74
	je @@mine
	cmp al,97
	je @@misslegun
	jmp @@inner_end

;
; HERE ON, must preserve BX, CX, and DX. See above for register values.
;

@@lava:
	cmp [ds:slow_down],0
	jne @@inner_end
	mov si,ax
	call random_num   				; Get random num
	cmp al,20         				; Below 20?
	jb @@inner_end						; Yep. DON'T animate
	mov ax,si
	cmp ah,2        					; At end of anim
	je @@endlava     					; Yep.
	inc ah                       	; Nope.
	mov [es:di+bx],ah 				; Increase anim
	jmp @@inner_end
@@endlava:
	mov [BYTE PTR es:di+bx],0 		; Restart anim
	jmp @@inner_end

@@energizer:
	inc_param_and_end 7       		; Increase to 7 max

@@life:
	cmp [ds:slow_down],0
	jne @@inner_end
	inc_param_and_end 3       		; Increase to 3 max

@@seeker:
	cmp [ds:slow_down],0
	jne @@inner_end
	or ah,ah          				; At end of life?
	jz @@seeker_die   				; yes
	dec ah                        ; No, decrease anim
	mov [es:di+bx],ah
											; Move.
	find_seek cx,dx,si
											; Seek dir in dl
											; Call move- X/Y=CX, Dir=DX, Flags=Immed.
	call _move,cx,dx,si,1+4+8+16+128+1024
	cmp ax,2           				; Player?
	jne @@inner_end					; Nope
											; Yeah- hurt 'n' die!
											; COMMON LABEL for enemies, etc
	mov si,79
@@hurt_n_die:
											; SI must hold id of what's dmging
	hurt_player_id si
	call id_remove_top,cx,dx
	jmp @@inner_end

@@seeker_die:
	call id_remove_top,cx,dx
	jmp @@inner_end

@@cw:
@@ccw:
	cmp [ds:slow_down],0
	jne @@inner_end
	inc_param 3       				; Increase to 3 max
	sub al,45
	xor ah,ah
	push dx bx cx
	call rotate,cx,dx,ax           ; Rotate
	pop cx bx dx
	jmp @@inner_end

@@litbomb:
	cmp [ds:slow_down],0     		; SLOW anim
	jne @@inner_end
	testflag ah,2         			; At end of anim (Both bits 2 AND 4 set)
	jz @@noblow
	testflag ah,4
	jnz @@blow        				; Yep.
@@noblow:
	inc ah
	mov [BYTE PTR es:di+bx],ah  	; Increase anim
	jmp @@inner_end
@@blow:
	testflag ah,128       			; High?
	jnz @@highblow    				; Yep
	mov [BYTE PTR es:di+bx],0+2*16; Set low param
@@blow2:
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],38		; Set id
	sfxp 36
	jmp @@inner_end
@@highblow:
	mov [BYTE PTR es:di+bx],0+4*16; Set param
	jmp @@blow2

@@explosion:
	testflag ah,15        			; Check stage
	jz @@outward      				; Climb outward- Stage 0!
@@exp2:
	mov al,ah
	maskflag al,15
	cmp al,3          				; Check for final stage
	je @@expremove    				; Die!
	les di,[level_param]
	inc ah                        ; Nope.
	mov [es:di+bx],ah 				; Increase anim
	jmp @@inner_end
@@expremove:

	; Get flags of below to check for entrance

	les di,[level_under_id]
	mov al,[es:di+bx]

	cmp al,34
	je @@e_spc										 ;	Goop is spaced
	cmp al,20
	jb @@notespc
	cmp al,26
	jbe @@e_spc										 ; Water, lava, ice are spaced

@@notespc:
	add al,al

	push bx
	mov bl,al
	xor bh,bh
	mov ax,[ds:flags+bx]
	pop bx

	testflag ax,A_ENTRANCE
	jnz @@e_spc                             ; Leaves space if entrance

	les di,[level_id]
	cmp [explosions_leave],EXPL_LEAVE_SPACE ; Leave space?
	je @@e_spc
	cmp [explosions_leave],EXPL_LEAVE_ASH   ; Leave ash?
	je @@e_ash
														 ; Leave fire.
	mov [BYTE PTR es:di+bx],63
	les di,[level_param]
	mov [BYTE PTR es:di+bx],0
	jmp @@inner_end
@@e_spc:
	call id_remove_top,cx,dx
	jmp @@inner_end
@@e_ash:
	mov [BYTE PTR es:di+bx],15
	les di,[level_color]
	mov [BYTE PTR es:di+bx],8
	jmp @@inner_end
@@outward:           				; Move explosion outward
	mov al,ah
	and al,240
	jz @@exp2         				; Not SUPPOSED to move outward!
	sub al,16         				; Decrease by 1 "outward"
	xor ah,ah
	mov si,ax
											; SI holds param for outward expansion
	push ax bx
	mov ax,3
@@outloop:
	push ax cx dx
	call _arraydir2    				; Move in dir
	cmp cx,0ffffh       				; Out of bounds?
	je @@ol_end
	cmp dx,0ffffh
	je @@ol_end
	_xy2array2    						; Get offset
	mov bx,ax
	les di,[ds:level_param]
	mov al,[es:di+bx] 				; Get param in that direction (for SPEC_BLOW)
	mov [@@ex_prm],al
	les di,[ds:level_id]
	mov bl,[es:di+bx]				 	; Get id in that direction
	mov [@@ex_id],bl
	add bl,bl
	xor bh,bh
	mov ax,[ds:flags+bx] 			; Get flags

	testflag ax,A_BLOW_UP      	; Blow up?
	jnz @@ex2
	testflag ax,A_EXPLODE      	; Explode?
	jnz @@ex3
	testflag ax,A_UNDER        	; Under?
	jnz @@ex1
	testflag ax,A_SPEC_BOMB       ; SPEC_BOMB ?
	jz @@ol_end              		; Can't move there

	; Handle players, robots, etc.
	; 34-goop, 74-mine, 83-slime, 85-ghost, 86-dragon, 127-player,
	; 123/124-robot

	cmp [@@ex_id],34
	je @@ol_end
	cmp [@@ex_id],127
	je @@ex_player
	cmp [@@ex_id],74
	je @@ex_mine
	cmp [@@ex_id],83
	je @@ex_slime
	cmp [@@ex_id],85
	je @@ex_ghost
	cmp [@@ex_id],86
	je @@ex_dragon

	; robot bombing
	; send_robot_def,[@@ex_prm],[WORD] 1

	pusha
	mov al,[@@ex_prm]
	xor ah,ah
	call send_robot_def,ax,[WORD] 1
	popa
	jmp @@ol_end

@@ex_slime:

	; If bit 128 is set, don't die.

	testflag [@@ex_prm],128
	jnz @@ol_end
	jmp @@ex2

@@ex_ghost:

	; If bit 8 is set, don't die.

	testflag [@@ex_prm],8
	jnz @@ol_end
	jmp @@ex2

@@ex_dragon:
	testflag [@@ex_prm],128+64+32
	jz @@ex2        					; If HP=0, die
	_xy2array2		    				; Get offset
	mov bx,ax
	mov al,[@@ex_prm]
	sub al,32
	les di,[level_param]
	mov [es:di+bx],al
	jmp @@ol_end

@@ex_mine:
	call id_remove_top,bx
	mov al,[@@ex_prm]
	and al,240        					; Use mine radius
	call id_place_asm,cx,dx,38,ax 	; Place explosion!
	jmp @@ol_end

@@ex_player:
	hurt_player_id 38
	jmp @@ol_end

@@ex3:
	call id_remove_top,cx,dx
	call id_place_asm,cx,dx,38,64 	; Place NEW explosion!
	jmp @@ol_end
@@ex2:
	call id_remove_top,cx,dx
@@ex1:
	call id_place_asm,cx,dx,38,si 	; Place our explosion!

@@ol_end:
	pop dx cx ax
	dec al
	jns @@outloop
	pop bx ax
	jmp @@exp2

@@transport:
	cmp [ds:slow_down],0
	jne @@inner_end
	testflag ah,8
	jz @@transanim
	testflag ah,16
	jz @@transanim
											; At end of anim
	maskflag ah,231        			; Erase anim bits
	mov [es:di+bx],ah
	jmp @@inner_end
@@transanim:
	add ah,8          				; Increase anim
	mov [es:di+bx],ah
	jmp @@inner_end

@@lazer:
	sub ah,8          				; Decrease "time-till-die"
	cmp ah,8
	jb @@lazdie       				; At 0- die! :)
	; Only anim every OTHER cycle
	cmp [ds:slow_down],0
	jne @@inner_end
	testflag ah,2
	jz @@lazanim
	testflag ah,4
	jz @@lazanim
	; At end of anim
	maskflag ah,249        			; Erase anim bits
	mov [es:di+bx],ah
	jmp @@inner_end
@@lazanim:
	add ah,2          				; Increase anim
	mov [es:di+bx],ah
	jmp @@inner_end
@@lazdie:
	call id_remove_top,cx,dx
	jmp @@inner_end

@@lazgun:
	mov al,ah
	maskflag al,28
	shr al,2          				; AL=start time
	cmp [ds:lazwall_start],al		; Correct time?
	jne @@inner_end   				; Nope.
	; Yep.
	les di,[ds:level_color]
	mov si,bx
	mov bl,[es:di+bx] 				; Get color,
	xor bh,bh         				; fill bx.
	mov al,ah
	maskflag ah,224
	shr ah,5          				; AH=length
	inc ah
	maskflag al,3          			; AL=direction
	call _shoot_lazer,cx,dx,ax,bx
	mov bx,si
	jmp @@inner_end

@@opengate:
	cmp ah,0          				; End of wait?
	je @@closegate    				; Close.
	dec ah
	mov [es:di+bx],ah 				; dec. wait
	jmp @@inner_end
@@closegate:
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],47 	; Close gate
	sfxp 25
	jmp @@inner_end

@@mine:
	testflag ah,2
	jz @@mineinc
	testflag ah,4
	jz @@mineinc
	testflag ah,8
	jnz @@mineanim
@@mineinc:
	add ah,2
	mov [es:di+bx],ah    			; Increase count
	jmp @@inner_end
@@mineanim:
	maskflag ah,241      			; Clear count
	flipflag ah,1        			; Flip anim bit
	mov [es:di+bx],ah    			; Set new param
	jmp @@inner_end

@@whirlpool:
	cmp [ds:slow_down],0    		; Slow down?
	jne @@inner_end
	les di,[level_id]
	cmp al,70            			; Last whirl?
	je @@lastwhirl
	inc al               			; Next "frame" (id) ! :)
	mov [es:di+bx],al
	jmp @@inner_end
@@lastwhirl:
	mov [BYTE PTR es:di+bx],67		; First id frame
	jmp @@inner_end

@@pusher:

	; BX=offset, CX=X, DX=Y, AL=id, AH=param, SI=flags, ES:DI=level_param

	cmp [ds:slow_down],0
	jne @@inner_end
	mov al,ah
	xor ah,ah     						; AX=dir
											; CX/DX=_xy
	call _move,cx,dx,ax,1
	jmp @@inner_end

@@movingwall:
	xor ah,ah
	sub al,51     						; AX=dir
											; CX=_xy
	mov si,ax
	call _move,cx,dx,ax,1+2+4+8+16
	cmp ax,0      						; Move ok?
	mov ax,si
	je @@inner_end
											; Reverse dir
	flip_dir al
	add al,51     						; Make into id
	les di,[ds:level_id] 			; Change id
	mov [es:di+bx],al 				; Save new id
	jmp @@inner_end

@@bullet:
	; Erase old-

	call id_remove_top,cx,dx

	; Place new!

	mov al,ah
	xor ah,ah
	mov si,ax
	shr al,2              			; AX=typ of bullet (bits 4 & 8 of param)
	and si,1+2            			; DX=dir
	call _shoot,cx,dx,si,ax
	jmp @@inner_end

@@opendoor:
	mov al,ah							; Copy param
	mov si,ax 							; 2 copys
	and al,224							; AL=curr wait
	and ah,31 							; AH=stage

	push bx

	mov bx,OFFSET open_door_max_wait
	add bl,ah
	adc bh,0  							; BX=offset to get max wait
	mov ah,[ds:bx]

	pop bx

	cmp ah,al 							; Compare curr_wait and max_wait
	jne @@door_inc_wait				; INCREASE WAIT

	; Move door, reset wait, and increase stage.

	mov ax,si 							; Use copy of param
	and ah,24 							; Stage

	; Reset wait & increase stage, unless stage 3.

	cmp ah,24 							; Stage 3?
	je @@door_3

	mov ax,si
											; reset wait,inc stage
	and ah,31 							; Remove wait
	add ah,8  							; Next stage
	mov [es:di+bx],ah 				; Put param
	mov ax,si
	jmp @@move_door

@@door_3:
	mov ax,si
	and ah,7  							; Open/orient
	mov [es:di+bx],ah
	mov ax,si
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],41 	; Reg. door

@@move_door:
	mov si,bx
	mov di,ax                     ; Save param in case no movey

	mov bx,OFFSET open_door_movement
	mov al,ah
	and al,31 							; Stage,open,& orient
	xlat
	cmp al,0ffh 						; No movement?
	je @@no_door_move					; Jump to fix bx, then haul off to end
	xor ah,ah   						; Dir fills ax
	call _move,cx,dx,ax,1+4+8+16
	or ax,ax
@@no_door_move:
	mov bx,si
	jz @@inner_end                ; It moved, go to over
											; No movey! Reset param and id
	mov ax,di
	les di,[ds:level_param]
	mov [es:di+bx],ah  				; Reset param
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],42    ; Reset id (for door_3)
	jmp @@inner_end

@@door_inc_wait:
	; Increase param's wait portion and store

	mov ax,si
	add al,32
	mov [es:di+bx],al
	jmp @@inner_end

@@fire:
	mov si,ax
	call random_num   				; Get random num
	cmp al,20         				; Below 20?
	jb @@fire2        				; Yep. DON'T animate
	mov ax,si
	cmp ah,5        					; At end of anim
	je @@endfr1         				; Yep.
	inc ah                       	; Nope.
	mov [es:di+bx],ah 				; Increase anim
	jmp @@fire2
@@endfr1:
	mov [BYTE PTR es:di+bx],0 		; Restart anim
@@fire2:
	; Check under for water, ice, or lava.
	; Lava and water kills fire.
	; Ice kills fire but melts into stillwater first.

	les di,[ds:level_under_id]
	mov al,[es:di+bx]
	cmp al,20
	jb @@fire3
	cmp al,26
	ja @@fire3
	cmp al,25
	jne @@fire_die
	mov [BYTE PTR es:di+bx],20
	les di,[ds:level_under_color]
	mov [BYTE PTR es:di+bx],25
@@fire_die:
	call id_remove_top,cx,dx
	jmp @@inner_end

@@fire3:
	; Die? Spread?

	call random_num
	cmp al,8          				; > 8?
	ja @@inner_end						; No spread or die
	cmp al,1          				; = 2-8
	ja @@die_done     				; No die
	cmp [ds:fire_burns],FIRE_BURNS_FOREVER
	je @@die_done       				; No death here
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],15 	; ASH
	les di,[ds:level_color]
	mov [BYTE PTR es:di+bx],8  	; (dk. grey)
@@die_done:
	; Spread?

	push bx
	mov al,3
@@spreadloop:
	push ax cx dx
	call _arraydir2    				; Move in dir
	cmp cx,0ffffh       				; Out of bounds?
	je @@sl_end
	cmp dx,0ffffh
	je @@sl_end
	_xy2array2   						; Get offset
	mov bx,ax
	les di,[ds:level_id]
	mov al,[es:di+bx] 				; Get id in that direction
	les di,[ds:level_color]
	mov ah,[es:di+bx] 				; Get color

	; AL=id AH=color, check for- space, brown, fake/carpet/tiles/custom, trees

	cmp al,0
	je @@fire_space
	cmp al,13
	jb @@not_fake_fire
	cmp al,19
	jbe @@fire_fake
@@not_fake_fire:
	cmp al,127
	je @@fire_player
	cmp ah,6
	je @@fire_brown
	cmp al,3
	je @@fire_tree
	jmp @@sl_end

@@fire_player:
	cmp [ds:firewalker_dur],0
	jne @@sl_end
	hurt_player_id 63
	jmp @@sl_end

@@fire_tree:
	cmp [ds:fire_burn_trees],1
	jne @@sl_end
	jmp @@fire_place

@@fire_brown:
	cmp al,120
	ja @@sl_end     					; Don't burn scrolls, signs, sensors,
											; robots, or the player!
	cmp [ds:fire_burn_brown],1
	jne @@sl_end
	jmp @@fire_place

@@fire_fake:
	cmp [ds:fire_burn_fakes],1
	jne @@sl_end
	jmp @@fire_place

@@fire_space:
	cmp [ds:fire_burn_space],1
	jne @@sl_end

@@fire_place:
	call id_place_asm,cx,dx,63,0

@@sl_end:
	pop dx cx ax
	dec al
	jns @@spreadloop
	pop bx
	jmp @@inner_end

@@snake:
	testflag ah,4
	jz @@sn_move       				; Speed = 0
											; Speed = 1
	flipflag ah,8      				; Flip count  and
	mov [es:di+bx],ah  				; store.

	testflag ah,8
	jz @@inner_end 					; Count = 0
											; Count = 1
@@sn_move:
	mov di,bx
	mov bl,ah
	maskflag bl,3      				; dx=dir
	xor bh,bh
	mov si,ax
	call _move,cx,dx,bx,128				; move
	xchg ax,si
	mov bx,di
	cmp si,2
	jne @@sn_n_hurt_n_die
	mov si,80							; hurt 'n' die!
	jmp @@hurt_n_die
@@sn_n_hurt_n_die:
	cmp si,0           				; moved?
	je @@inner_end 					; done.
											; Choose new dir
	maskflag ah,252	       		; AH=param minus direction
	mov bl,ah
	maskflag bl,64+32+16    		; BL=intellegence
	shr bl,4
	mov bh,ah          				; BH=param
	call random_num
	maskflag al,7           		; AL=0-7
	cmp al,bl
	jb @@sn_smart      				; Rand < Int = Smart Move
											; Dumb Move
	call random_num
	maskflag al,3           		; AL=new dir
	setflag bh,al           		; Place into bh
	mov ah,bh
	mov bx,di
	les di,[level_param]
	mov [es:di+bx],ah  				; store.
	jmp @@inner_end
@@sn_smart:
											; Smart move, load seek into bl
	find_seek cx,dx,bl
	setflag bh,bl           		; Add dir into param
	mov ah,bh
	mov bx,di
	les di,[level_param]
	mov [es:di+bx],ah  				; store.
	jmp @@inner_end

@@eye:
	testflag ah,64
	jz @@i_move       				; Speed = 0
											; Speed = 1
	flipflag ah,128         		; Flip count and
	mov [es:di+bx],ah  				; store.
	testflag ah,128
	jz @@inner_end  					; Count = 0
											; Count = 1
@@i_move:
	mov di,bx
	mov bl,ah
	maskflag bl,7           		; dl=intel
	mov si,ax
	call random_num
	maskflag al,7           		; al=0-7
	cmp al,bl
	jb @@i_smart       				; Smart move if rand < intel
											; Dumb move
	call random_num
	mov bl,al
	maskflag al,3           		; dl=dir
	jmp @@i_dir
@@i_smart:
	find_seek cx,dx,al
@@i_dir:
	xor ah,ah     						; ax=dir
	call _move,cx,dx,ax,4+8+16+128+1024
	cmp al,2           				; player?
	mov ax,si
	mov bx,di
	jne @@inner_end
	maskflag ah,8+16+32     		; ah=radius
	add ah,ah          				; put in explosion radius pos.
	les di,[level_param]
	mov [BYTE PTR es:di+bx],ah
	les di,[level_id]
	mov [BYTE PTR es:di+bx],38 	; Become explosion!
	sfxp 36                    	; Expl. sound
	jmp @@inner_end

@@thief:
	mov si,bx
	mov bl,ah
	mov bh,ah
	and bl,8+16
	and bh,32+64
	shr bh,2
											; BL=speed, BH=count
	cmp bl,bh
	mov bx,si
	je @@th_move
	add ah,32
	mov [es:di+bx],ah
	jmp @@inner_end

@@th_move:
	and ah,255-64-32    				; Zero count
	mov [es:di+bx],ah
											; Move now
	mov bl,ah
	and bl,7           				; bl=intel
	push ax
	call random_num
	and al,7           				; al=0-7
	cmp al,bl
	pop ax
	jb @@th_smart       				; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3           				; bl=dir
	jmp @@th_dir
@@th_smart:
	find_seek cx,dx,bl
@@th_dir:
	flipflag bh,bh     				; bx=dir
	push ax
	call _move,cx,dx,bx,128
	cmp al,2           				; player?
	pop ax
	mov bx,si
	jne @@inner_end
											; Take gems
	mov al,ah
	flipflag ah,ah
	and al,128
	shr al,7       					; dx=gems to take
	inc al
	mov di,OFFSET gem_name
	push bx cx dx
	call dec_counter,di,ds,ax,0
	pop dx cx bx
	sfxp 44
	jmp @@inner_end

@@slime:
	cmp [ds:slow_down],0
	jne @@inner_end
	mov si,bx

	mov bl,ah
	mov bh,ah
											; BL= SPEED BITS << 2 + SPEED BITS
	and bl,1+2
	mov al,bl
	add bl,bl
	setflag bl,al
											; BH= COUNT BITS >> 2
	and bh,32+16+8+4
	shr bh,2
											; BL=speed, BH=count
	cmp bl,bh
	mov bx,si
	je @@slime_spread
	add ah,4
	mov [es:di+bx],ah
	jmp @@inner_end

@@slime_spread:
											; SPREAD LOOP
	push bx
											; Spread?

	les di,[ds:level_color]
	mov bh,[es:di+bx]
	mov bl,83
	mov si,bx
	mov bl,ah
	and bl,255-8-16-32

											; BL holds param for outward expansion,
											; and SI=id/color.

	flipflag ax,ax
@@slimeloop:
	push ax cx dx
	call _arraydir2    				; Move in dir
	cmp cx,0ffffh       				; Out of bounds?
	je @@ssl_end
	cmp dx,0ffffh
	je @@ssl_end
	push bx
	_xy2array2							; Get offset
	mov bx,ax
	les di,[ds:level_id]
	mov al,[es:di+bx] 				; Get id in that direction
	pop bx

	; AL=id

	cmp al,0
	je @@slime_fake
	cmp al,13
	je @@slime_fake
	cmp al,14
	je @@slime_fake
	cmp al,15
	je @@slime_fake
	cmp al,16
	je @@slime_fake
	cmp al,17
	je @@slime_fake
	cmp al,18
	je @@slime_fake
	cmp al,19
	je @@slime_fake
	cmp al,127
	je @@slime_player
	jmp @@ssl_end

@@slime_player:
	testflag bl,64
	jz @@ssl_end    					; No hurt.
	hurt_player_id 83
	jmp @@ssl_end

@@slime_fake:
											; BX=XY,BL=param SI=id/color
	call id_place_asm,cx,dx,si,bx

@@ssl_end:
	pop dx cx ax
	inc al
	cmp al,4
	jb @@slimeloop
	pop bx
	maskflag si,65535-255  			; Clear id
	setflag si,6
	call id_place_asm,cx,dx,si,0
	jmp @@inner_end

@@runner:
	mov si,bx

	mov bl,ah
	mov bh,ah
	and bl,4+8
	and bh,32+16
	shr bh,2
											; BL=speed, BH=count
	cmp bl,bh
	mov bx,si
	je @@run_run
	add ah,16
	mov [es:di+bx],ah
	jmp @@inner_end

@@run_run:
	and ah,255-32-16
	mov [es:di+bx],ah
	mov bl,ah
	xor bh,bh
	and bl,3      						; BX=dir
	push ax
	call _move,cx,dx,bx,1+2+128
	xchg ax,si
	mov bx,ax
	pop ax
	cmp si,0      						; Move ok?
	je @@inner_end
	cmp si,2      						; Player?
	jne @@run_n_hurt_n_die
	mov si,84
	jmp @@hurt_n_die

@@run_n_hurt_n_die:
	; Reverse dir
	xor ah,1
	mov [es:di+bx],ah 				; Save new param
	jmp @@inner_end

@@ghost:
	mov si,bx
	mov bl,ah
	mov bh,ah
	and bl,16+32
	and bh,64+128
	shr bh,2
											; BL=speed, BH=count
	cmp bl,bh
	mov bx,si
	je @@gh_move
	add ah,64
	mov [es:di+bx],ah
	jmp @@inner_end

@@gh_move:
	and ah,255-64-128    			; Zero count
	mov [es:di+bx],ah
											; Move now
	mov bl,ah
	and bl,7           				; dl=intel
	push ax
	call random_num
	and al,7           				; al=0-7
	cmp al,bl
	pop ax
	jb @@gh_smart       				; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3           				; dl=dir
	jmp @@gh_dir
@@gh_smart:
	find_seek cx,dx,bl
@@gh_dir:
	flipflag bh,bh     				; dx=dir
	push ax
	call _move,cx,dx,bx,4+8+16+128+1024
	cmp al,2           				; player?
	pop ax
	mov bx,si
	jne @@inner_end
	; Hit player
	mov si,85
	testflag ah,8
	jz @@hurt_n_die 					; Not invinco
	hurt_player_id 85   				; Invinco
	jmp @@inner_end

@@dragon:
	testflag ah,4
	jnz @@move_ok
	push cx dx
	jmp @@dr_try_fire 				; No move, try fire
@@move_ok:
	mov si,bx
	mov bh,ah
	and bh,16+8
											; BH=count
	cmp bh,16+8
	mov bx,si
	je @@dr_move
	add ah,8
	mov [es:di+bx],ah
	push cx dx
	jmp @@dr_try_fire

@@dr_move:
	and ah,255-16-8    				; Zero count
	mov [es:di+bx],ah
											; Move now
	push ax
	call random_num
	testflag al,7
	jz @@dr_dumb_move					; 1 out of 8 moves is random

	find_seek cx,dx,bl
	flipflag bh,bh     				; bx=dir
	jmp @@dr_move2
@@dr_dumb_move:
	call random_num               ; Random dir
	maskflag ax,3
	mov bx,ax                     ; bx=dir
@@dr_move2:
	call _move,cx,dx,bx,4+8+128
	mov bh,al
	pop ax
	cmp bh,2           				; player?
	je @@dr_player
	cmp bh,0
	xchg bx,si
	jne @@inner_end  					; Blocked.
											; get new dir and fire
	xchg bx,si
	push cx dx ax
	mov ax,bx
	call _arraydir2
	pop ax
	mov bx,si

@@dr_try_fire:
	mov si,bx
	mov bl,ah
	and bl,3           				; bl=fire rate
	push ax
	call random_num
	and al,15           				; al=0-15
	cmp al,bl
	pop ax
	jb @@dr_fire2      		 		; Fire seek if rand < rate
	pop dx cx
	mov bx,si
	jmp @@inner_end
@@dr_fire2:
	find_seek cx,dx,bx
	call _shoot_fire,cx,dx,bx
	pop dx cx
	mov bx,si
	sfxp 46
	jmp @@inner_end

@@dr_player:
											; Hit player
	mov bx,si
	hurt_player_id 86   				; Invinco
	jmp @@inner_end

@@shootingfire:

	xor ah,1  							; Flip anim
	mov [es:di+bx],ah 				; And store
											; Move in dir
	mov al,ah
	shr al,1
	xor ah,ah         				; ax=dir
	call _move,cx,dx,ax,4+8+16+128+1024+2048
	cmp al,2
	jne @@sf_n_hurt_n_die   		; Hit player
	mov si,78
	jmp @@hurt_n_die

@@sf_n_hurt_n_die:
	or al,al
	jz @@inner_end						; Done if moved ok.
											; Become fire
	les di,[ds:level_under_id]
	cmp [BYTE PTR es:di+bx],43
	je @@_sf_die      				; entrance
	cmp [BYTE PTR es:di+bx],44
	je @@_sf_die      				; entrance
	cmp [BYTE PTR es:di+bx],67
	je @@_sf_die      				; entrance
	cmp [BYTE PTR es:di+bx],68
	je @@_sf_die      				; entrance
	cmp [BYTE PTR es:di+bx],69
	je @@_sf_die      				; entrance
	cmp [BYTE PTR es:di+bx],70
	je @@_sf_die      				; entrance
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],63 	; id of fire
	les di,[ds:level_param]
	mov [BYTE PTR es:di+bx],0  	; param of fire
	jmp @@inner_end

@@_sf_die:

	call id_remove_top,cx,dx
	jmp @@inner_end

@@missile:
	; Move in dir

	mov al,ah
	xor ah,ah         				; ax=dir
	push ax
	call _move,cx,dx,ax,4+8+16+128+1024
	mov si,ax
	pop ax
	cmp si,2
	je @@miss_blow   					; Hit player
	cmp si,0
	je @@inner_end						; Done if moved ok.

	; Try to turn- CW then CCW

	mov ah,al 							; Save dir
	push bx
	mov bx,OFFSET cwturndir
	xlat
	pop bx
	mov [es:di+bx],al 				; Store new param
											; Try the new dir
	push ax
	call _move,cx,dx,ax,4+8+16+128+1024
	mov si,ax
	pop ax
	cmp si,2
	je @@miss_blow						; Player
	cmp si,0
	je @@inner_end						; Moved
											; Now try CCW of orig. dir
	mov al,dh
	push bx                       ; Get CCW
	mov bx,OFFSET ccwturndir
	xlat
	pop bx
	mov [es:di+bx],al 				; Store new param
											; Try the new dir
	call _move,cx,dx,ax,4+8+16+128+1024
	cmp al,0
	je @@inner_end 					; Okay.
											; Player or dead-end - blow up!

@@miss_blow:
	mov [BYTE PTR es:di+bx],48 	; param of explosion
	les di,[level_id]
	mov [BYTE PTR es:di+bx],38 	; id of explosion
	sfxp 36
	jmp @@inner_end

@@fish:

	testflag ah,8
	jz @@f_move       				; Speed = 0
											; Speed = 1
	testflag ah,16
	jnz @@f_move  						; Count = 1
											; Count = 0
	xor ah,16           				; Set count 1 and
	mov [es:di+bx],ah  				; store.
	jmp @@inner_end

@@f_move:
	and ah,255-16      				; Set count 0 and
	mov [es:di+bx],ah  				; store.
	testflag ah,32     				; Affected by current?
	jz @@f_move_ok     				; Nope.
											; Yep, get under.
	les di,[ds:level_under_id]
	mov al,[es:di+bx]
	sub al,21
											; DL=0-3 for dir, >3 for reg. move
	cmp al,3
	jbe @@f_move_dir

@@f_move_ok:
	mov si,bx
	mov bl,ah
	and bl,7           				; dl=intel
	push ax
	call random_num
	and al,7           				; al=0-7
	cmp al,bl
	pop ax
	jb @@f_smart       				; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3           				; dl=dir
	mov al,bl
	mov bx,si
	jmp @@f_move_dir
@@f_smart:
	find_seek cx,dx,ax
	mov bx,si
@@f_move_dir:
											; Move in dir al
	push ax
	xor ah,ah
	call _move,cx,dx,ax,16+128+256
	cmp al,2
	pop ax
	je @@f_player
	jmp @@inner_end

@@f_player:
	testflag ah,64
	jz @@inner_end
	; Hurts.
	mov si,87
	jmp @@hurt_n_die

@@shark:
	; Move

	cmp [ds:slow_down],0
	jne @@inner_end

	mov si,bx

	mov bl,ah
	and bl,7           				; dl=intel
	push ax
	call random_num
	and al,7          	 			; al=0-7
	cmp al,bl
	pop ax
	jb @@sk_smart       				; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3           				; dl=dir
	jmp @@sk_move_dir
@@sk_smart:
	find_seek cx,dx,bl
@@sk_move_dir:
	flipflag bh,bh     				; dx=dir
	push ax
	call _move,cx,dx,bx,512+128+4
	mov bh,al
	pop ax
	cmp bh,2           				; player?
	jne @@sk_n_hurt_n_die
	mov bx,si
	mov si,88
	jmp @@hurt_n_die

@@sk_n_hurt_n_die:
	cmp bh,0
	mov bx,si
	je @@sk_ok_to_fire 				; not Blocked.
	push cx dx
	jmp @@sk_dir_gotten 				; Fire even if blocked

@@sk_ok_to_fire:
											; get new dir and fire
	push cx dx
	call _arraydir2

@@sk_dir_gotten:
											; Attempt to fire
	mov si,bx
	mov bl,ah
	and bl,32+64+128    				; bl=fire rate
	shr bl,5
	push ax
	call random_num
	and al,31           				; al=0-31
	cmp al,bl
	pop ax
	jbe @@sk_fire2       			; Fire seek if rand <= rate
	pop dx cx
	mov bx,si
	jmp @@inner_end
@@sk_fire2:
	find_seek cx,dx,bl
	mov bh,ah
	and bh,16+8
@@fire_time:

	; General loc.-
	; Fires dir BL of AX (x/y) according to BH then ends.
	; BH=0 for bullets, 8 for seeker, 16 for fire, 24 for cancel
	; BX is stored in SI and CX DX is on stack

	cmp bh,24
	je @@fire_time_end
	cmp bh,0
	je @@ft_shoot
	cmp bh,8
	je @@ft_seeker
											; bh=16, @@ft_fire
	xor bh,bh
	call _shoot_fire,cx,dx,bx
	sfxp 46
	jmp @@fire_time_end
@@ft_shoot:
	xor bh,bh
	mov di,ENEMY_BULLET
	call _shoot,cx,dx,bx,di
	jmp @@fire_time_end
@@ft_seeker:
	xor bh,bh
	call _shoot_seeker,cx,dx,bx
@@fire_time_end:
	mov bx,si
	pop dx cx
	jmp @@inner_end

@@spider:
	testflag ah,32
	jz @@spdr_move       			; Speed = 0
											; Speed = 1
	testflag ah,64
	jnz @@spdr_move  					; Count = 1
											; Count = 0
	xor ah,64           				; Set count 1 and
	mov [es:di+bx],ah  				; store.
	jmp @@inner_end

@@spdr_move:
	and ah,255-64      				; Set count 0 and
	mov [es:di+bx],ah  				; store.

	mov si,bx

	mov bl,ah
	and bl,7           				; dl=intel
	push ax
	call random_num
	and al,7          				; al=0-7
	cmp al,bl
	pop ax
	jb @@spdr_smart      			; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3           				; dl=dir
	jmp @@spdr_move_dir
@@spdr_smart:
	find_seek cx,dx,bl
@@spdr_move_dir:
	xor bh,bh							; Move in dir dl
	mov di,128         				; si=flags
											; Get "web" setting
	mov al,ah
	and al,16+8
	shl al,2
	add al,32
											; al=32 reg, 64 thck, 96 both, 128 anything
	cmp al,128
	je @@spdr_flags_done
	flipflag ah,ah
	add di,ax  							; Add "web flags" to flags
@@spdr_flags_done:
	call _move,cx,dx,bx,di
	cmp al,2
	mov bx,si
	mov si,89
	je @@hurt_n_die
	jmp @@inner_end

@@tiger:

	cmp [ds:slow_down],0
	jne @@inner_end
											; Move
	mov si,bx

	mov bl,ah
	and bl,7           				; dl=intel
	push ax
	call random_num
	and al,7           				; al=0-7
	cmp al,bl
	pop ax
	jb @@tg_smart       				; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3           				; dl=dir
	jmp @@tg_move_dir
@@tg_smart:
	find_seek cx,dx,bl
@@tg_move_dir:
	flipflag bh,bh     				; dx=dir
	push ax
	call _move,cx,dx,bx,16+128
	mov bh,al
	pop ax
	cmp bh,2           				; player?
	jne @@sk_n_hurt_n_die         ; REST OF TIGER LOGIC IS SAME AS SHARK
	mov bx,si
	mov si,91
	jmp @@hurt_n_die

@@goblin:
	cmp [ds:slow_down],0
	jne @@inner_end

	mov si,bx

	mov bl,ah
	testflag bl,32
	jz @@gb_move  						; Move
											; Rest
	mov bl,ah
	mov bh,ah
	and bl,2+1
	add bl,bl
	inc bl
	and bh,16+8+4
	shr bh,2
											; BL=rest length, BH=rest count
	cmp bl,bh
	mov bx,si
	je @@gb_time_to_move
	add ah,4
	mov [es:di+bx],ah
	jmp @@inner_end
@@gb_time_to_move:
											; Reset count and flip switch to moving
	and ah,255-4-8-16-32
	mov [es:di+bx],ah
	jmp @@inner_end

@@gb_move:
											; Increase count, if 5 reset and flip
											; switch to resting
	add ah,4
	mov bl,ah
	and bl,4+8+16
	cmp bl,20
	jne @@gb_move_now
											; Reset & flip
	and ah,255-4-8-16
	or ah,32
@@gb_move_now:
											; Store new parameter
	xchg si,bx
	mov [es:di+bx],ah
	xchg si,bx
											; Move now
	mov bl,ah
	and bl,64+128           		; dl=intel
	shr bl,6
	push ax
	call random_num
	and al,3           				; al=0-3
	cmp al,bl
	pop ax
	jb @@gb_smart       				; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3          		 		; dl=dir
	jmp @@gb_dir
@@gb_smart:
	find_seek cx,dx,bl
@@gb_dir:
	flipflag bh,bh     				; bx=dir
											; cx/dx=x/y
	push ax
	call _move,cx,dx,bx,16+128
	cmp al,2           				; player?
	pop ax
	mov bx,si
	jne @@inner_end
											; Hit player
	mov si,90
	jmp @@hurt_n_die

@@cub:
	cmp [ds:slow_down],0
	jne @@inner_end
											; Increase count
	mov si,bx

	mov bl,ah
	mov bh,ah
	and bl,8+4
	shr bl,1
	inc bl
	and bh,16+32+64
	shr bh,4
											; BL=length, BH=count
	cmp bl,bh
	je @@cub_switch_typ
	add ah,16
	jmp @@cub_time_to_move

@@cub_switch_typ:
											; Reset count and flip switch
	and ah,255-16-32-64
	xor ah,128

@@cub_time_to_move:
	mov bx,si
	mov [es:di+bx],ah
											; Move now
	mov bl,ah
	and bl,1+2           			; bl=intel
	push ax
	call random_num
	and al,3           				; al=0-3
	cmp al,bl
	pop ax
	jb @@cub_smart       			; Smart move if rand < intel
											; Dumb move
	push ax
	call random_num
	mov bl,al
	pop ax
	and bl,3           				; dl=dir
	jmp @@cub_dir
@@cub_smart:
	find_seek cx,dx,bl
@@cub_dir:
											; if "run" mode then reverse dir
	testflag ah,128
	jz @@cub_not_run
	flip_dir bl
@@cub_not_run:
	flipflag bh,bh     				; bx=dir
											; cx=x/y
	call _move,cx,dx,bx,16+128
	cmp al,2           				; player?
	mov bx,si
	jne @@inner_end
											; Hit player
	mov si,95
	jmp @@hurt_n_die

@@bear:
	mov si,bx

	mov bl,ah
	mov bh,ah
	and bl,8+16
	and bh,32+64
	shr bh,2
											; BL=speed, BH=count
	cmp bl,bh
	mov bx,si
	je @@bear_move
	add ah,32
	mov [es:di+bx],ah
	jmp @@inner_end

@@bear_move:
	and ah,255-64-32    				; Zero count
	mov [es:di+bx],ah
											; Move now
	push cx dx
	sub cx,[player_x]
	jns @@cx_positive
	neg cx
@@cx_positive:
	sub dx,[player_y]
	jns @@dx_positive
	neg dx
@@dx_positive:
											; CX=x offset of player, DX=y offset
											; of player
	add cx,dx
											; CX=player distance
	mov dl,ah
	and dl,7
	inc dl       						; Increase (so sens. 0 is really 2)
	add dl,dl    						; Double sensitivity
											; DH=bear sensitivity
	xor dh,dh
	cmp cx,dx
	pop dx cx
	jbe @@bear_can_move
											; Not close enough
	jmp @@inner_end

@@bear_can_move:
											; Distance is below or equal to sensitivity.
	find_seek cx,dx,bl
	flipflag bh,bh     				; bx=dir
											; cx=x/y
	call _move,cx,dx,bx,16+128
	cmp al,2           				; player?
	mov bx,si
	jne @@inner_end
											; Hit player
	mov si,94
	jmp @@hurt_n_die

@@bulletgun:
	cmp [ds:slow_down],0
	jne @@inner_end

@@spingun_entry:
	mov si,bx

	mov bl,ah
	mov bh,ah
	and bl,7              			; Get rate
	call random_num
	and al,15
	cmp al,bl
	xchg si,bx
	ja @@inner_end   					; Fire if rand <= rate
	xchg si,bx
	mov bl,bh
	and bl,64+32          			; Get intel.
	call random_num
	and al,64+32
	flipflag di,di        			; DI=0 for 1 FIRE (if aimed)
	cmp al,bl
	jbe @@bg_smart
	xor di,1              			; NO FIRE (if aimed)
@@bg_smart:
	find_seek cx,dx,bl    			; Get seek
	mov ah,bh
	and ah,16+8
	shr ah,3              			; Get dir.
	cmp bl,ah             			; Same?
	je @@bg_same
	xor di,1              			; If NOT same, then flip whether to fire
@@bg_same:
	cmp di,0              			; Fire?
	xchg bx,si
	jne @@inner_end
	xchg bx,si
	; Fire in dir ah
	mov bl,ah
	testflag bh,128       			; Fire or bullet?
	jz @@bg_bullet
	flipflag bh,bh
	call _shoot_fire,cx,dx,bx
	xchg si,bx
	jmp @@inner_end
	xchg si,bx

@@bg_bullet:
	flipflag bh,bh
	call _shoot,cx,dx,bx,ENEMY_BULLET
	mov bx,si
	jmp @@inner_end

@@spinning:
	cmp [ds:slow_down],0
	jne @@inner_end

	testflag ah,8
	jz @@sg_anim
	testflag ah,16
	jz @@sg_anim
	and ah,255-16-8
	jmp @@sg2
@@sg_anim:
	add ah,8
@@sg2:
	mov [es:di+bx],ah
	jmp @@spingun_entry

@@misslegun:
	cmp [ds:slow_down],0
	jne @@inner_end

	mov si,bx

	mov bl,ah
	mov bh,ah
	and bl,7              			; Get rate
	cmp bl,7
	je @@mg_rate_ok       			; ALWAYS fire if rate 7
	call random_num
	and al,31
	cmp al,bl
	mov al,bh
	mov bx,si
	ja @@inner_end   					; Fire if rand <= rate
	mov bh,al

@@mg_rate_ok:
	find_seek cx,dx,bl
											; BL holds player dir
	mov al,bh
	shr al,3        					; AL=missile dir
	and al,3
	cmp al,bl
	xchg bx,si
	jne @@inner_end 					; Wrong direction
	xchg bx,si
											; Right dir- fire!
	mov bl,al
	mov al,bh
	flipflag bh,bh      				; BX=dir, CX/DX=source pos
	call _shoot_missile,cx,dx,bx
	testflag al,128
	mov bx,si
	jnz @@inner_end
											; Only one shot- kill self.
	call id_remove_top,cx,dx
	jmp @@inner_end

@@ice:
	cmp ah,0
	je @@ice_start    				; Check random_num to start ice anim
	cmp ah,3
	je @@ice_end      				; End ice anim
	inc ah
	mov [es:di+bx],ah 				; Increase anim
	jmp @@inner_end
@@ice_end:
	mov [BYTE PTR es:di+bx],0 		; Clear anim
	jmp @@inner_end
@@ice_start:
	call random_num   				; Get random num
	cmp al,0          				; If zero,
	jne @@inner_end   				; then start anim-
	inc al            				; al=1
	mov [es:di+bx],al 				; First frame
	jmp @@inner_end

@@robot:
	push bx cx dx
											; ah=param=id of robot!
	mov al,ah
	xor ah,ah
											; x/y in cx/dx
											; call sub
	call run_robot,ax,cx,dx
	pop dx cx bx
	les di,[update_done]
	mov ax,[es:di]
	and ax,254
	cmp ax,254
	je @@all_done						; Abort loop on Swap World

@@inner_end:
	inc cx            				; X+=1
	inc bx            				; offset+=1
	cmp cx,[ds:board_xsiz] 			; out of bounds?
	jne @@inner       				; Nope, loop
											; Yep-
	pop bx            				; Load old offset
	add bx,[max_bxsiz]        		; offset+=max_bxsiz (down one row)
	push bx                       ; Save new offset
	inc dx                        ; Y+=1
	cmp dx,[ds:board_ysiz] 			; out of bounds?
	jne @@outer                   ; Nope, loop (X=0 auto)
@@all_done:
	pop bx                        ; Done, pop offset

	; Do REVERSE robot loop

	mov cx,[ds:board_xsiz]
	dec cx
	mov bx,[ds:board_ysiz]
	dec bx

	; Calculate offset

	mov ax,[max_bxsiz]                      ; Multiply 100 by
	mul bx                                  ; Y pos,
	mov dx,bx
	add ax,cx                               ; add X pos,
	mov bx,ax
	mov si,bx

	les di,[ds:level_id]

@@rrl_loop:
	mov al,[es:di+bx]
	cmp al,123
	jb @@rrl_not
	cmp al,124
	ja @@rrl_not

	les di,[ds:level_param]
	mov al,[es:di+bx]
	xor ah,ah
	neg ax
	push si bx cx dx
	call run_robot,ax,cx,dx
	pop dx cx bx si
	les di,[ds:level_id]

@@rrl_not:
	dec bx
	dec cx
	jns @@rrl_loop

	mov cx,[ds:board_xsiz]
	dec cx

	mov bx,si
	sub bx,[ds:max_bxsiz]
	mov si,bx
	dec dx
	jns @@rrl_loop

	; Done!

	call find_player

	pop es ds
	popa
	ret

endp update_screen

proc _shoot_lazer near
arg _cx_x:word,_cx_y:word,_ax_pd:word,color:word
; _cx_x/_cx_y- x/y pos
; _ax_pd- al=dir ah=length

	pusha
	push es

	mov cx,[_cx_x]
	mov dx,[_cx_y]

@@laz_loop:
	mov ax,[_ax_pd]    				; Load al with dir
	call _arraydir2    	 			; Move x/y in direction
	cmp cx,0ffffh       				; Out of bounds? Quit loop.
	je @@end_laz
	cmp dx,0ffffh
	je @@end_laz
											; Get offset
	_xy2array2 			   			; Offset in AX
	mov bx,ax          				; Offset in BX & AX
	les di,[ds:level_id]  			; level ids
	mov bl,[es:di+bx]  				; Get id
											; Check for player
	cmp bl,127
	jne @@not_lplayer

	; Take health and try to move player in opposite of last dir

	hurt_player_id 59
	cmp [restart_if_zapped],1
	je @@end_laz
	mov al,[player_last_dir]
	and al,15
	dec al
	js @@end_laz
	xor ah,ah							; AX=last dir (must reverse)
	flip_dir al
	call _move,cx,dx,ax,1+2+16
	jmp @@end_laz

@@not_lplayer:
	cmp bl,123
	je @@lrobot
	cmp bl,124
	jne @@not_lrobot
@@lrobot:
	; Send robot to lazer label
	les di,[ds:level_param]
	mov bx,ax
	mov al,[es:di+bx]
	xor ah,ah
	call send_robot_def,ax,[WORD] 8
	jmp @@end_laz

@@not_lrobot:
	flipflag bh,bh
	add bl,bl          				; For flags offset
	mov bx,[ds:flags+bx]				; Get flags
	testflag bx,A_UNDER
	jz @@end_laz        				; If not BK, quit loop
											; Dirs=cx/dx
	mov ax,[_ax_pd]     				; Loads AH with length
	flipflag al,al           		; length in 8 4 2 1 of AH
	xchg al,ah
	shl al,3            				; length in 64 32 16 8 of AL
	mov bx,[_ax_pd]     				; Get dir
	cmp bl,1            				; Check dir
	ja @@direw          				; horiz
	setflag al,1            		; vert
@@direw:
	mov bh,[BYTE color]      		; Color in bh
	mov bl,59           				; Load id
	call id_place_asm,cx,dx,bx,ax
	jmp @@laz_loop      				; Loop and do it again!
@@end_laz:

	; All done!

	pop es
	popa
	ret

endp _shoot_lazer

proc _shoot_lazer_c far
arg x:word,y:word,dir:word,len:word,color:word

	mov cx,[dir]
	mov dx,[len]
	mov ch,dl
	call _shoot_lazer,[x],[y],cx,[color]
	ret

endp _shoot_lazer_c

;
; Try to transport in dir DIR of xy _XY. Places id ID/COLOR/PARAM. Only pushes
; if CAN_PUSH > 0. Returns 1 for not possible, 0 for did/is possible. Does
; not affect id at xy _XY-DIR. Note, _XY is the location of the TRANSPORTER.
;
; RECURSIZE
;

proc _transport far   				; ID=0 means just tell whether it is possible
arg _x:word,_y:word,dir:word,id:word,param:word,color:word,can_push:word

	; Check for deep recursion

	cmp sp,1500               		; SP < 1500 ?
	jae @@okey              		; No
	mov ax,1
	ret                     		; Yep, return negative
@@okey:

	push bx cx dx di si es

	mov cx,[_x]	            		; Load xy
	mov dx,[_y]
	mov ax,[dir]            		; Load dir,
	xor ah,ah               		; clear top,
	mov si,A_PUSHNS           		; set SI to flag for testing
	cmp al,2                		; Check for NS or EW
	jb @@NS
	mov si,A_PUSHEW           		; Load EW flag
@@NS:
	mov bx,ax               		; Dir into bl
	_xy2array2		        			; Create offset
	xchg bx,ax               		; Move into bx, dir into al
	les di,[ds:level_param]   		; Load location of params
	mov ah,[es:di+bx]       		; Get param of transport
											; Check for proper dir/all
	maskflag ah,7           		; Use dir bits only
	cmp ah,4                		; Any dir?
	je @@dir_okey
	cmp ah,al               		; Same dir?
	jne @@neg               		; NOPE!
@@dir_okey:
	xor ah,ah
	call _arraydir2        	 		; Get loc after transporter (x/y)
	cmp cx,0ffffh             		; Out of bounds?
	je @@neg
	cmp dx,0ffffh
	je @@neg
	_xy2array2 			      		; Create offset
	mov bx,ax              	 		; Move into bx
	les di,[ds:level_id]       	; Load location of ids
	mov al,[es:di+bx]       		; Get id after transporter

	cmp al,34                     ; Goop- special case
	je @@loop_part_1

	;
	; Finally, get flags of id after transporter
	;

	mov bx,ax
	add bl,bl               		; Double id
	flipflag bh,bh          		; Clear upper of offset
	mov di,OFFSET flags     		; Load flags location
	mov bx,[ds:di+bx]       		; Get flags after transporter

	; DX is loc of x/y of dest. trans.

	testflag bx,A_UNDER       		; Under?
	jnz @@checks_ok               ; Yeppo.
	testflag bx,si          		; Pushable?
	jnz @@check_push
	cmp al,49                     ; Transporter (so SPEC_PUSH doesn't get)
	je @@loop_part_1b
	testflag bx,A_SPEC_PUSH       ; Special pushable?
	jnz @@check_push

	; Continue searching

	jmp @@loop_part_1       		; Loop to get next id

@@check_push:
	; sensor w/player?

	cmp al,122
	jne @@cpt2
	cmp [id],127
	je @@checks_ok
@@cpt2:

	; Are we ALLOWED to push?

	cmp [can_push],0
	je @@loop_part_1        		; Aren't allowed to push. (?)
											; Pushable- Check if truly.
											; Actually push so we don't have to later.
	push cx dx
	mov ax,[dir]
	flip_dir ax
	call _arraydir2
	call _push,cx,dx,[dir],[id]	; Is it possible?
	pop dx cx
	cmp ax,0
	je @@checks_ok          		; Ok
	jmp @@loop_part_1b       		; Not Ok

@@loop_part_1:
	;
	; Loop, searching for a proper dest.
	;
	; Get next id in sequence (CX/DX hold curr pos)

	mov ax,[dir]
	call _arraydir2          		; Get next loc
	cmp cx,0ffffh						; Out of bounds?
	je @@neg
	cmp dx,0ffffh
	je @@neg

@@loop_part_1b:

	_xy2array2 	        				; Create offset
	mov bx,ax               		; Move into bx
	les di,[ds:level_id]       	; Load location of ids
	mov al,[es:di+bx]       		; Get next ID
											; AL holds ID
	cmp al,49                     ; Transporter?
	jne @@loop_part_1       		; Nope, continue!
											; Check dir- get param
	les di,[ds:level_param]    	; Load param loc
	mov al,[es:di+bx]       		; get param
	maskflag al,7           		; Use dir bits only
	cmp al,4                		; Any dir?
	je @@found_tr_dir_ok
											; Load ah with OPPOSITE dir for cmp
	mov bx,[dir]
	flip_dir bl                   ; Flip the dir
	cmp al,bl               		; Opp dir?
	jne @@loop_part_1       		; nope, cont.
@@found_tr_dir_ok:
											; Dir okay, check other side of it!
	mov ax,[dir]
	call _arraydir2          		; Get loc after transporter (x/y)
	cmp cx,0ffffh             		; Out of bounds?
	je @@neg
	cmp dx,0ffffh
	je @@neg
	_xy2array2   	       			; Create offset
	mov bx,ax               		; Move into bx
	les di,[ds:level_id]       	; Load location of ids
	mov bl,[es:di+bx]       		; Get id after transporter

	cmp bl,34							; Goop- special
	je @@loop_part_1

	;
	; Finally, get flags of id after transporter
	;

	push bx                 		; save id
	add bl,bl               		; Double id
	flipflag bh,bh          		; Clear upper of offset
	mov di,OFFSET flags     		; Load flags location
	mov ax,[ds:di+bx]       		; Get flags after transporter
	pop bx                  		; restore id

	;
	; Check for UNDER or PUSHABLE.
	;

	testflag ax,A_UNDER       		; Under?
	jnz @@checks_ok               ; Yeppo. OK!
	testflag ax,si          		; Pushable?
	jnz @@test_push         		; Must test.
	cmp bl,49
	je @@loop_part_1b
	testflag ax,A_SPEC_PUSH       ; Pushable?
	jnz @@test_push               ; Must test.
	jmp @@loop_part_1b       		; loop

	;
	; Check if can push after transporter.
	;

@@test_push:
	; Sensor?
	cmp bl,122
	jne @@tpt2
	cmp [id],127
	je @@checks_ok
@@tpt2:

	; Are we ALLOWED to push?

	cmp [can_push],0
	je @@loop_part_1b        		; Aren't allowed to push. (?)
											; Pushable- Check if truly.
	push cx dx
	mov ax,[dir]
	flip_dir ax
	call _arraydir2
	call _push,cx,dx,[dir],[id]	; Is it possible?
	pop dx cx
	cmp ax,0
	je @@checks_ok          		; Ok
	jmp @@loop_part_1       		; Not Ok, loop to 1st (THIS isn't a trans, so
											; even though this was never read within the
											; loop, we update the loc AGAIN to skip
											; checking this id for being a transporter.
											; whew! <g> )
@@checks_ok:
	cmp [id],0              		; Just checking?
	je @@pos                		; Yeppo.
	sfxp 27

	; CX/DX holds x y of dest., and any pushing has been done.

	mov al,[BYTE id]             	; Get id
	mov si,[param]          		; Get param
	mov ah,[BYTE color]          	; Get color
	call id_place_asm,cx,dx,ax,si 		; PLACE IT
											; Return.
@@pos:
	pop es si di dx cx bx
	xor ax,ax
	ret

@@neg:
	pop es si di dx cx bx
	mov ax,1
	ret

endp _transport

;
; Push DIR of _XY. Doesn't affect actaul id at _XY. Returns 1 for not
; possible, 0 for did/is possible.
;
; RECURSIZE
;

proc _push far  ; checking=1 means just tell whether possible.
arg _x:word,_y:word,dir:word,checking:word
local @@moving_id:byte,@@moving_color:byte,@@moving_param:byte,@@offset:word,@@pushsensor:byte

											; Check for deep recursion
	cmp sp,1500               		; SP < 1500 ?
	jae @@okey              		; No
	mov ax,1
	ret                     		; Yep, return negative
@@okey:

	mov [@@pushsensor],0FFh

	push bx cx dx di si es

	mov cx,[_x]            			; Load xy
	mov dx,[_y]
	mov ax,[dir]            		; Load dir,
	mov si,A_PUSHNS           		; set SI to flag for testing
	cmp al,2                		; Check for NS or EW
	jb @@NS
	mov si,A_PUSHEW           		; Load EW flag
@@NS:

@@loop_1:
	mov [@@moving_id],ah          ; Keep track of ID for sensor check w/player
	;
	; Loop- Read IDs in dir, cont. for PUSH or SPEC_PUSH, end pos. for UNDER,
	;       end neg. for EDGE or anything else.
	; CX/DX=dirs AL=dir SI=flag (pushns or pushew)

	call _arraydir2          		; Get next location
	cmp cx,0ffffh             		; Check for edges
	je @@neg
	cmp dx,0ffffh
	je @@neg

	;
	; Now get ID
	;

	_xy2array2 	         			; Create offset
	mov bx,ax               		; Move into bx
	les di,[ds:level_id]       	; Load location of ids
	mov ah,[es:di+bx]       		; Get id

	;
	; Finally, get flags
	;

	mov bl,ah               		; Put into offset
	add bl,bl               		; Double id
	xor bh,bh               		; Clear upper of offset
	mov di,OFFSET flags     		; Load flags location
	mov bx,[ds:di+bx]       		; Get flag
	mov al,[BYTE PTR dir]

	; BX=flag AH=id AL=dir SI=bit to test for push CX/DX=x/y
	; Test for UNDER

	testflag bx,A_UNDER
	jz @@not_under         			; Not under
											; Under- Lava or water?
	cmp ah,26               		; Lava?
	je @@neg                		; Yep!
	cmp ah,34							; Goop?
	je @@neg								; Negative
	cmp al,20               		; Below water?
	jb @@pos                		; Yep! POSITIVE
	cmp ah,24               		; Above water?
	ja @@pos                		; Yep! POSITIVE
	jmp @@neg               		; NEGATIVE
@@not_under:
											; Test for pushable
	testflag bx,si
	jz @@not_push           		; Jeez, not pushable either!
											; Pushable- continue loop
	jmp @@loop_1
@@not_push:
											; Test for special-when-pushed
	testflag bx,A_SPEC_PUSH
	jz @@neg                		; Well, guess we CAN'T push!
											; Is it a transport or a robot?
	cmp ah,49               		; Transport?
	je @@ptrans             		; Yeah.
											; Robot or sensor- continue loop
	cmp ah,122
	je @@sensor

	jmp @@loop_1
@@sensor:
											; Was LAST ID a player?
	cmp [@@moving_id],127
	je @@pos
	jmp @@loop_1                  ; If not, treat as a pushable
@@ptrans:
	xor ah,ah
	call _transport,cx,dx,ax,0,0,0,1 ; Call transport, on a test-w/push only
	or ax,ax	                		; Negative?
	jnz @@neg                		; yep.
	jmp @@pos               		; Nope!

@@pos:
											; We CAN move- but are we "just checking" ?
	cmp [checking],1      			; ???
	je @@pos3               		; Yeah.

@@actual:
	;
	; Now actually PUSH the crap!
	;

	mov cx,[_x]	            		; Load xy
	mov dx,[_y]
	mov ax,[dir]            		; Load dir,
	xor ah,ah               		; clear top.
	mov [@@moving_id],0FFh    		; clear moving_id.

@@loop_2:
	;
	; Move in dir, swapping ids, etc.
	; CX/DX=x/y AL=dir or ?? SI=flag to test for push
	;
	mov ax,[dir]

	call _arraydir2          		; Get next location

	_xy2array2         				; Create offset
	mov bx,ax               		; Move into bx
	les di,[ds:level_id]       	; Load location of ids
	mov ax,[dir]
	mov ah,[es:di+bx]       		; Get id
	mov [@@offset],bx       		; Save offset

	;
	; Finally, get flags
	;

	mov bl,ah               		; Put into offset
	add bl,bl               		; Double id
	xor bh,bh               		; Clear upper of offset
	mov di,OFFSET flags     		; Load flags location
	mov bx,[ds:di+bx]       		; Get flag

	; BX=flag AH=id AL=dir CX/DX=x/y SI=flag
	; Test for UNDER

	testflag bx,A_UNDER
	jz @@not_under2       	 		; Not under
											; Prepare arguments to "place" moving id.
@@likeunder:
	mov al,[@@moving_id]
	mov ah,[@@moving_color]  		; AX=id/color
	mov bl,[@@moving_param]
	xor bh,bh                     ; BX=param
	call id_place_asm,cx,dx,ax,bx
	cmp al,127							; Player?
	jne @@pos2
											; Yep. Sensor prev.?
	mov ax,[dir]
	flip_dir ax
	call _arraydir2          		; Get OLD location
	_xy2array2         				; Create offset
	mov bx,ax               		; Move into bx
	les di,[ds:level_under_id]   	; Load location of ids
	cmp [BYTE PTR es:di+bx],122 	; Get id
	jne @@pos2                    ; Not a sensor back there
	mov al,[under_player_id]
	mov [BYTE PTR es:di+bx],al
	les di,[ds:level_under_color]
	mov al,[under_player_color]
	xchg al,[es:di+bx]
	les di,[ds:level_under_param]
	mov ah,[under_player_param]
	xchg ah,[es:di+bx]
	mov [@@pushsensor],ah
	mov [under_player_id],0
	mov [under_player_color],7
	mov [under_player_param],0
	mov bx,[@@offset]
	mov [es:di+bx],ah
	les di,[ds:level_under_color]
	mov [es:di+bx],al
	les di,[ds:level_under_id]
	mov [BYTE PTR es:di+bx],122	; All moved!
	jmp @@pos2

@@not_under2:
	; Test for pushable

	testflag bx,si
	jz @@not_push2          		; Jeez, not pushable either!
@@push_it:
	mov bx,[@@offset]       		; get array offset
	cmp [@@moving_id],0ffh    		; Already got a moving id?
	je @@load_m_i           		; Load moving id cause none currently
											; Swap w/moving id
											; AH=id AL=dir CX/DX=x/y SI=flag BX=offs
	push cx
	les di,[level_id]
	mov al,[@@moving_id]
	mov cl,al							; Save in case player is moving there
	xchg [es:di+bx],al
	mov [@@moving_id],al
	les di,[level_color]
	mov ah,[@@moving_color]
	xchg [es:di+bx],ah
	mov [@@moving_color],ah
	les di,[level_param]
	mov ah,[@@moving_param]
	xchg [es:di+bx],ah
	mov [@@moving_param],ah
											; Check (al=id, ah=param) for sending robot
											; to PUSHED label
	cmp cl,127							; Did we JUST move the player??
	pop cx
	je @@pl_m_sensor
	cmp al,122
	je @@pushed_a_sensor
	cmp al,123
	jne @@loop_2
											; push
	pusha
	mov al,ah
	xor ah,ah
	call send_robot_def,ax,[WORD] 3
	popa
											; Done, loop
	jmp @@loop_2

@@pushed_a_sensor:
	pusha                         ; A sensor NOT near the player was pushed.
	mov al,ah
	xor ah,ah
	call push_sensor,ax
	popa
	jmp @@loop_2

@@pl_m_sensor:							; Get sensor from prev. and move it,
											; if present
	push ax cx dx
	mov ax,[dir]
	flip_dir ax
	call _arraydir2          		; Get OLD location
	_xy2array2         				; Create offset
	mov bx,ax               		; Move into bx
	les di,[ds:level_under_id]   	; Load location of ids
	cmp [BYTE PTR es:di+bx],122 	; Get id
	je @@plms2
	pop dx cx ax
	jmp @@loop_2                  ; Not a sensor back there
@@plms2:
	mov al,[under_player_id]
	mov [es:di+bx],al
	les di,[ds:level_under_color]
	mov al,[under_player_color]
	xchg al,[es:di+bx]
	les di,[ds:level_under_param]
	mov ah,[under_player_param]
	xchg ah,[es:di+bx]
	mov [@@pushsensor],ah
	mov [under_player_id],0
	mov [under_player_color],7
	mov [under_player_param],0
	mov bx,[@@offset]
	mov [es:di+bx],ah
	les di,[ds:level_under_color]
	mov [es:di+bx],al
	les di,[ds:level_under_id]
	mov [BYTE PTR es:di+bx],122	; All moved!
	pop dx cx ax
	jmp @@loop_2

@@load_m_i:
	mov [@@moving_id],ah
	les di,[level_param]
	mov al,[es:di+bx]
	mov [@@moving_param],al
	les di,[level_color]
	mov al,[es:di+bx]
	mov [@@moving_color],al
	cmp ah,127                    ; DON'T remove top if player/sensor combo
	jne @@lmi2
	les di,[level_under_id]
	cmp [BYTE PTR es:di+bx],122
	je @@loop_2

@@lmi2:
	call id_remove_top,cx,dx 		; Remove the id
											; Done,  loop
	cmp ah,122
	jne @@loop_2
											; Pushed a sensor...
	mov ah,[@@moving_param]
	jmp @@pushed_a_sensor

@@not_push2:
											; MUST be spec_push.
	cmp ah,49               		; Transport?
	je @@ptrans2            		; Yeah.
	cmp ah,122							; Sensor?
	je @@psens2
											; Robot- like push
	jmp @@push_it
@@psens2:
	cmp [@@moving_id],127			; Player?
	jne @@push_it						; Push the sensor
											; Put player ON it
	pusha
	mov bx,[@@offset]       		; get array offset
	les di,[level_param]
	mov al,[es:di+bx]
	xor ah,ah
	call step_sensor,ax           ; Send sensor's robot to SENSORON
	popa
	jmp @@likeunder

@@ptrans2:
											; Prepare arguments to "transport" moving id.
	xor ah,ah							; AX=dir
	mov di,cx              			; DI=x
	mov si,dx							; SI=y
	mov bl,[@@moving_id]
	xor bh,bh              			; BX=id
	mov cl,[@@moving_color]  		; CX=color
	xor ch,ch
	mov dl,[@@moving_param]
	xor dh,dh              			; DX=param
	call _transport,di,si,ax,bx,dx,cx,1 ; Call transport!

@@pos2:
	sfxp 26
											; Did we push a sensor?
	cmp [@@pushsensor],0ffh
	je @@pos3
	mov al,[@@pushsensor]
	xor ah,ah
	call push_sensor,ax           ; Yep! Notify it.

@@pos3:
	xor ax,ax
	jmp @@eeee
@@neg:
	mov ax,1
@@eeee:
	pop es si di dx cx bx
	ret

endp _push

proc _shoot_c far
arg _x:word,_y:word,dir:word,typ:word
	call _shoot,[_x],[_y],[dir],[typ]
	ret
endp _shoot_c

;
; Shoot DIR of _X/_Y. Works w/ricochets, ricochet panels, robots, etc.
; Type is SHOT_PLAYER, SHOT_NEUTRAL, or SHOT_ENEMY.
;
; RECURSIZE
;

proc _shoot near
arg _x:word,_y:word,dir:word,typ:word

											; Check for deep recursion
	cmp sp,1500               		; SP < 1500 ?
	jae @@okey              		; No
	ret                     		; Yep, return
@@okey:

	pusha
	push es

	; If enemy_hurt_enemy and typ==enemy, typ==neutral.

	cmp [typ],ENEMY_BULLET
	jne @@typ_ok
	cmp [ds:enemy_hurt_enemy],1
	jne @@typ_ok
	mov [typ],NEUTRAL_BULLET

@@typ_ok:
	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
	cmp cx,0ffffh             		; Check for offscreen
	je @@done
	cmp dx,0ffffh
	je @@done
	_xy2array2
	mov bx,ax               		; Offset in bx
	les di,[ds:level_id]
	mov al,[es:di+bx]      			; Load bl with id [dir] of [_xy]

	; First give 3 points for shooting anything from 80-95 except 83,85,92,93

	cmp [typ],PLAYER_BULLET
	jne @@no_pts
	cmp al,80
	jb @@no_pts
	cmp al,95
	ja @@no_pts
	cmp al,83
	je @@no_pts
	cmp al,85
	je @@no_pts
	cmp al,92
	je @@no_pts
	cmp al,93
	je @@no_pts

	; Give 3 points

	add [WORD LOW score],3
	adc [WORD HIGH score],0
	jnc @@no_pts

	; Overflow

	mov [WORD LOW score],255
	mov [WORD HIGH score],255

@@no_pts:
	mov si,bx               		; Save offset
	mov bl,al
	xor bh,bh               		; bx=id
	add bl,bl               		; bx=id*2
	mov bx,[ds:flags+bx]    		; ax=flags of id
	testflag bx,A_UNDER       		; Is it an under?
	jnz @@move              		; Yep.
	testflag bx,A_SHOOTABLE   		; Is it shootable?
	jz @@notshootable       		; Nope.
											; Yep. Remove THAT id then @@erase.
@@shootable:
	; Check for enemy bullet if not a "breakable"

	cmp al,61
	je @@ok_to_shoot
	cmp al,28
	je @@ok_to_shoot
	cmp al,29
	je @@ok_to_shoot
	cmp al,6
	je @@ok_to_shoot
	cmp al,7
	je @@ok_to_shoot
	cmp [typ],ENEMY_BULLET
	je @@done
@@ok_to_shoot:
	call id_remove_top,cx,dx		; Remove that id
	sfxp 29
	jmp @@done

@@notshootable:
	testflag bx,A_SPEC_SHOT   		; Special when shot?
	jz @@done              			; No, die!
											; Yep- but what is it? (cl=id)
	cmp al,72
	je @@panel
	cmp al,73
	je @@ricochet
	cmp al,74
	je @@mine
	cmp al,127
	je @@player
	cmp al,124
	je @@robot
	cmp al,123
	je @@robot
	cmp al,81
	je @@eye
	cmp al,83
	je @@slime
	cmp al,84
	je @@runner
	cmp al,85
	je @@ghost
	cmp al,86
	je @@dragon
	cmp al,87
	je @@fish
	cmp al,89
	je @@fish          				; Spider's HP works in the same way
	cmp al,94
	je @@fish          				; Bear's HP works same

@@ricochet:
	sfxp 31
	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
	flip_dir al
	call _shoot,cx,dx,ax,NEUTRAL_BULLET
	jmp @@done

@@panel:
	sfxp 31
	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
	mov bx,si
	les di,[ds:level_param]
	mov bl,[es:di+bx]       		; Get panel's orientation  1 is /
	cmp bl,1
	je @@p2                 		; Other orient
											; \ so 0->3 1->2 2->1 3->0
	xor ax,3
	jmp @@pdone
@@p2:
											; / so 0->2 1->3 2->0 3->1
	xor ax,2
@@pdone:
	call _shoot,cx,dx,ax,NEUTRAL_BULLET
	jmp @@done

@@eye:
	cmp [typ],ENEMY_BULLET
	je @@done               		; Can't be shot by enemy bullets!
											; Get offset
	mov bx,si               		; in bx
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],38    ; Expl. ID
	les di,[ds:level_param]
	maskflag [BYTE PTR es:di+bx],8+16+32
	shl [BYTE PTR es:di+bx],1
	sfxp 36
	jmp @@done

@@mine:
											; Get offset
	mov bx,si               		; in bx
	les di,[ds:level_id]
	mov [BYTE PTR es:di+bx],38    ; Expl. ID
	les di,[ds:level_param]
	maskflag [BYTE PTR es:di+bx],240 ; Get rid of "count" and "anim"
	sfxp 36
	jmp @@done

@@player:
	cmp [typ],PLAYER_BULLET
	je @@done               		; Can't be shot by player bullets!
	hurt_player_id 61
	call send_robot_def,ax,[WORD] 7
	jmp @@done

@@slime:
	cmp [typ],ENEMY_BULLET
	je @@done               		; Can't be shot by enemy bullets!
											; Get offset
	mov bx,si               		; in bx
	les di,[ds:level_param]
	mov al,[es:di+bx]
											; al = param
	testflag al,128
	jnz @@done 							; Invinco
	jmp @@shootable

@@runner:
	cmp [typ],ENEMY_BULLET
	je @@done               		; Can't be shot by enemy bullets!
											; Get offset
	mov bx,si               		; in bx
	les di,[ds:level_param]
	mov al,[es:di+bx]
											; Cl = param
	testflag al,64+128
	jz @@shootable       		 	; HP = 0, so die
	sub al,64
	mov [es:di+bx],al
	sfxp 45
	jmp @@done

@@ghost:
	cmp [typ],ENEMY_BULLET
	je @@done               		; Can't be shot by enemy bullets!
											; Get offset
	mov bx,si               		; in bx
	les di,[ds:level_param]
	mov al,[es:di+bx]
											; Cl = param
	testflag al,8
	jnz @@done 							; Invinco
	jmp @@shootable

@@dragon:
	cmp [typ],ENEMY_BULLET
	je @@done               		; Can't be shot by enemy bullets!
											; Get offset
	mov bx,si               		; in bx
	les di,[ds:level_param]
	mov al,[es:di+bx]
											; Cl = param
	testflag al,128+64+32
	jz @@shootable 					; HP gone
	sub al,32      					; HP dec
	mov [es:di+bx],al
	sfxp 45
	jmp @@done

@@fish:
	cmp [typ],ENEMY_BULLET
	je @@done               		; Can't be shot by enemy bullets!
											; Get offset
	mov bx,si               		; in bx
	les di,[ds:level_param]
	mov al,[es:di+bx]
											; Cl = param
	testflag al,128
	jz @@shootable 					; HP gone
	xor al,128      					; HP dec
	mov [es:di+bx],al
	sfxp 45
	jmp @@done

@@robot:
	mov bx,si
	les di,[ds:level_param]
	mov bl,[es:di+bx]       		; Get robot's id
											; Send typ of message, then done
	mov cx,[typ]
	add cl,4
	xor bh,bh
	push bx
	call send_robot_def,bx,cx
	pop bx
											; Now set robot's last_shot_dir variable
	les di,[ds:robots]

	mov ax,ROBOT_SIZE
	mul bx
	mov bx,ax
	add bx,30 							; Reference last_shot_dir variable

	mov ax,[dir]
	flip_dir ax 						; We need which side of ROBOT was shot
	inc ax
	mov [es:di+bx],al
	jmp @@done

@@move:
	mov ax,[typ]
	mov si,ax                     ; si=bullet type

	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
											; BX=X/Y
	mov ax,[typ] 						; AX=typ, shifted
	sal al,2      						; left twice.
	add ax,[dir]  						; AX+=dir (AX:=param)

	mov bh,[ds:bullet_color+si]	; dh=color
	mov bl,61             			; dl=id
	call id_place_asm,cx,dx,bx,ax
@@done:
	pop es
	popa
	ret

endp _shoot

;
; Shoot FIRE DIR of _X/_Y.
;

proc _shoot_fire near
arg _x:word,_y:word,dir:word

	push es
	pusha

	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
	cmp cx,0ffffh             		; Check for offscreen
	je @@done
	cmp dx,0ffffh
	je @@done
	_xy2array2
	mov bx,ax               		; Offset in bx
	les di,[ds:level_id]
	mov si,bx               		; Save offset
	mov bl,[es:di+bx]       		; Load bl with id [dir] of [_x/_y]
	xor bh,bh               		; bx=id
	add bl,bl               		; bx=id*2
	mov ax,[ds:flags+bx]    		; ax=flags of id
	shr bl,1
	testflag ax,A_UNDER       		; Is it an under?
	jnz @@move              		; Yep.
	cmp bl,127
	je @@player
	cmp bl,123
	je @@robot
	cmp bl,124
	jne @@done

@@robot:
	; Robot!

	mov bx,si
	les di,[ds:level_param]
	mov bl,[es:di+bx]
	xor bh,bh
	call send_robot_def,bx,[WORD] 9
	jmp @@done

@@player:
	; Player!

	hurt_player_id 78
	jmp @@done

@@move:
	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
											; cx/dx=x/Y
	add al,al     						; put dir in bits 2,4 (AX:=Param)
	call id_place_asm,cx,dx,78,ax

@@done:
	popa
	pop es
	ret

endp _shoot_fire

proc _shoot_fire_c far
arg x:word,y:word,dir:word
	call _shoot_fire,[x],[y],[dir]
	ret
endp _shoot_fire_c

;
; Shoot SEEKER DIR of _XY.
;

proc _shoot_seeker near
arg _x:word,_y:word,dir:word

	push es
	pusha

	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
	cmp cx,0ffffh             		; Check for offscreen
	je @@done
	cmp dx,0ffffh
	je @@done
	_xy2array2
	mov bx,ax               		; Offset in bx
	les di,[ds:level_id]
	mov si,bx               		; Save offset
	mov bl,[es:di+bx]       		; Load cl with id [dir] of [_xy]
	xor bh,bh               		; bx=id
	add bl,bl               		; bx=id*2
	mov ax,[ds:flags+bx]    		; ax=flags of id
	add bl,bl
	testflag ax,A_UNDER       		; Is it an under?
	jnz @@move              		; Yep.
	cmp bl,127
	jne @@done

	; Player!

	hurt_player_id 79
	jmp @@done

@@move:
	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
											; BX=X/Y
	call id_place_asm,cx,dx,79,127
@@done:
	popa
	pop es
	ret

endp _shoot_seeker

proc _shoot_seeker_c far
arg x:word,y:word,dir:word
	call _shoot_seeker,[x],[y],[dir]
	ret
endp _shoot_seeker_c

;
; Shoot MISSILE DIR of _X/_Y.
;

proc _shoot_missile near
arg _x:word,_y:word,dir:word

	push es
	pusha

	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
	cmp cx,0ffffh             		; Check for offscreen
	je @@done
	cmp dx,0ffffh
	je @@done
	_xy2array2
	mov bx,ax               		; Offset in bx
	les di,[ds:level_id]
	mov si,bx               		; Save offset
	mov bl,[es:di+bx]       		; Load cl with id [dir] of [_xy]
	xor bh,bh               		; bx=id
	sal bl,1                		; bx=id*2
	mov ax,[ds:flags+bx]    		; ax=flags of id
	shr bl,1
	testflag ax,A_UNDER       		; Is it an under?
	jnz @@move              		; Yep.
	cmp bl,127
	jne @@done

	; Player!

	hurt_player_id	62
	jmp @@done

@@move:
	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2          		; Get loc in dir
											; cx/dx=X/Y
	mov ax,[dir]  						; AX==dir (param)
	;mov [ds:323],0
	;mov bh,[ds:323]		; BX=color used to be missile_color
	mov bh, [missile_color]
	mov bl,62             			;    id
	call id_place_asm,cx,dx,bx,ax
@@done:
	popa
	pop es
	ret

endp _shoot_missile

proc _shoot_missile_c far
arg x:word,y:word,dir:word
	call _shoot_missile,[x],[y],[dir]
	ret
endp _shoot_missile_c

;
; General move sub- move ID at _X/_Y in DIR according to FLAGS. Erase original.
; Assumes DS=Near Seg
;

proc _move far
arg _x:word,_y:word,dir:word,movflags:word
local @@temp:word

; Flags:
;   1-Can push
;   2-Can transport
;   4-Can lava walk
;   8-Can fire walk
;  16-Can water walk
;  32-Must web       ¿Together is:
;  64-Must thick web Ù  Must any web
; 128-React to player BEFORE push
; 256-Must water
; 512-Must lava OR goop
;1024-Can goop walk
;2048-Shooting fire (IE send robot to label and return 1)

; Returns:
;  0-Moved
;  1-Blocked
;  2-Player (and didn't move)
;  3-Edge

	push es
	push di si dx cx bx

	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]          			; Load x/y & dir
	call _arraydir2        			; Get new x/y
	cmp cx,0ffffh          			; Check for edge
	je @@edge
	cmp dx,0ffffh
	je @@edge
	_xy2array2       					; Get offset
	mov bx,ax             			; Load offset
	mov si,bx             			; "store" offset
	les di,[ds:level_id]     		; Get level id
	mov bl,[es:di+bx]
	add bl,bl             			; Make an offset
	xor bh,bh
	mov ax,[ds:flags+bx]  			; Get flags
											; AX=FLAGS BL=ID
	shr bl,1
	testflag [movflags],128
	jz @@dont_test_player_yet
											; Test for player BEFORE push
	cmp bl,127 							; Player
	je @@player

@@dont_test_player_yet:
	testflag ax,A_UNDER     		; Under?
	jz @@notunder         			; no
	testflag [movflags],96    		; Check web status
	jz @@notmustweb           		; Web not neccesary
											; Web neccesary
	testflag [movflags],32
	jnz @@regweb
	jmp @@thickweb
											; Any web
@@anyweb:
	cmp bl,18             			; Thin web
	je @@move
@@thickweb:
	cmp bl,19
	je @@move
	jmp @@dont
@@regweb:
	testflag [movflags],64
	jnz @@anyweb
	cmp bl,18
	je @@move
	jmp @@dont

@@notmustweb:
											; Must HýO?
	testflag [movflags],256
	jz @@notmustwater
											; Must be 20-24
	cmp bl,20
	jb @@dont
	cmp bl,24
	ja @@dont
	jmp @@move

@@notmustwater:
											; Must lava/goop?
	testflag [movflags],512
	jz @@notmustlava
	cmp bl,26
	jne @@notlavaisitgoop
	jmp @@move
@@notlavaisitgoop:
	cmp bl,34
	jne @@dont
	jmp @@move

@@notmustlava:
											; Under
	cmp bl,26     						; Lava
	jne @@notlava
	testflag [movflags],4 			; Can we lavawalk?
	jz @@dont
	jmp @@move
@@notlava:
	cmp bl,63     						; Fire
	jne @@notfire
	testflag [movflags],8			; Can we firewalk?
	jz @@dont
	jmp @@move
@@notfire:
	cmp bl,34                     ; Goop
	jne @@notgoop
	testflag [movflags],1024		; Can we goopwalk?
	jz @@dont
	jmp @@move
@@notgoop:
	cmp bl,20
	jb @@move
	cmp bl,24
	ja @@move
	; Water
	testflag [movflags],16 			; Can we waterwalk?
	jz @@dont
	jmp @@move

@@notunder:
	; Spit fire code

	testflag [movflags],2048
	jz @@notspitty
	cmp bl,123
	je @@sprob
	cmp bl,124
	jne @@notspitty
@@sprob:
	; Spit fire has hit a robot
	mov bx,si
	les di,[ds:level_param]			; Get robot id
	mov al,[es:di+bx]
	xor ah,ah
	call send_robot_def,ax,[WORD] 9      ; Send to SPITFIRE
	jmp @@dont                    ; and don't move any more

@@notspitty:
	; Check for PUSH/TRANS
	; BX=FLAGS AL=ID

	cmp bl,49  							; Transporter
	je @@trytrans

	testflag ax,A_PUSHNS
	jnz @@trypush
	testflag ax,A_PUSHEW
	jnz @@trypush
	testflag ax,A_SPEC_PUSH
	jnz @@trypush
@@dont:
	mov ax,1
	jmp @@ret
@@player:
	mov ax,2
	jmp @@ret
@@edge:
	mov ax,3
	jmp @@ret

@@trytrans:
	testflag [movflags],2 			; CAN we trans?
	jz @@dont
											; Prepare arguments
	mov cx,[_x]
	mov dx,[_y]
	_xy2array2
	mov bx,ax          				; BX=offset
	les di,[ds:level_id]
	mov al,[es:di+bx]
	xor ah,ah          				; AX=id
	les di,[ds:level_color]
	mov cl,[es:di+bx]
	xor ch,ch          				; CX=color
	les di,[ds:level_param]
	mov dl,[es:di+bx]
	xor dh,dh          				; DX=param
	mov si,ax          				; SI=id
	mov di,cx          				; DI=color
	xor bx,bx          				; BX=can_push
	mov [@@temp],dx					; temp=param
	testflag [movflags],1 			; Can we push?
	jz @@trans_no_push
	flipflag bx,1    					; can push
@@trans_no_push:
											; SI=id DI=color DX=param CX=can_push
	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2     				; CX/DX=_xy  AX=dir
	call _transport,cx,dx,ax,si,[@@temp],di,bx
	or ax,ax           				; Make it?
	jz @@erase         				; Yeah.
	jmp @@dont         				; Nope.

@@trypush:
	testflag [movflags],1 			; Can we push?
	jz @@dont
	mov cx,[_x]       				; Prepare arguments
	mov dx,[_y]
	mov ax,[dir]						; Not just checking
	call _push,cx,dx,ax,0
	or ax,ax           				; Made it?
	jnz @@dont         				; No dont move
											; Okay, move
@@move:
	mov cx,[_x]
	mov dx,[_y]
	_xy2array2
	mov bx,ax
	les di,[ds:level_id]
	mov al,[es:di+bx]
	les di,[ds:level_color]
	mov ah,[es:di+bx]
	mov si,ax
	les di,[ds:level_param]
	mov al,[es:di+bx]
	xor ah,ah
	mov di,ax
											; cX=param
	mov cx,[_x]
	mov dx,[_y]
	call id_remove_top,cx,dx

	mov cx,[_x]
	mov dx,[_y]
	mov ax,[dir]
	call _arraydir2     				; Get dest. loc
	call id_place_asm,cx,dx,si,di
	jmp @@era2

@@erase:
	mov cx,[_x]
	mov dx,[_y]
	call id_remove_top,cx,dx

@@era2:
	xor ax,ax

@@ret:
	pop bx cx dx si di
	pop es
	ret

endp _move

proc parsedir far
arg old_dir:word,x:word,y:word,flow_dir:word,bln:word,bls:word,ble:word,blw:word

	mov bx,[old_dir]
	and bx,15 							; base dir
	mov cx,[x]
	mov dx,[y]
											; ax for dest
	cmp bx,0
	je @@use_bx
	cmp bx,11
	je @@use_bx
	cmp bx,12
	je @@use_bx
	cmp bx,13
	je @@flow
	cmp bx,14
	je @@use_bx
	ja @@randb
	cmp bx,5
	jb @@bx_base
	je @@randns
	cmp bx,7
	jb @@randew
	je @@randne
	cmp bx,9
	je @@seek
	jb @@randnb
	; randany
	call random_num
	mov bx,ax
	and bx,3
	inc bx
	jmp @@bx_base
@@randb:
@@randnb:
	; RANDNB- XOR bln thru blw with 1

	cmp bx,15
	je @@mustberandb
	xor [bln],1
	xor [bls],1
	xor [ble],1
	xor [blw],1
@@mustberandb:
	; First, figure # allowed (number dirs blocked)

	mov ax,[bln]
	add ax,[bls]
	add ax,[ble]
	add ax,[blw]

	; If 0, return NO_DIR

	cmp ax,0
	ja @@notnodir
	mov bx,14
	jmp @@use_bx
@@notnodir:
											; Random...
	mov bx,ax
	call random_num
											; Make rand. num 0-255
	xor ah,ah
											; Get modulo
	div bl
											; Remainder in ah
	mov bl,ah
	xor bh,bh
											; Increase if bln=0
											; Increase if bls=0 and > 0
											; Increase if ble=0 and > 1
	cmp [bln],0
	jne @@nott_n
	inc bx
@@nott_n:
	cmp [bls],0
	jne @@nott_s
	cmp bx,0
	je @@nott_s
	inc bx
@@nott_s:
	cmp [ble],0
	jne @@nott_e
	cmp bx,1
	jbe @@nott_e
	inc bx
@@nott_e:
											; Finally, increase bx to range 1-4
	inc bx
	jmp @@bx_base
@@flow:
	mov bx,[flow_dir]
	jmp @@bx_base
@@randns:
	call random_num
	mov bx,ax
	and bx,1
	inc bx
	jmp @@bx_base
@@randew:
	call random_num
	mov bx,ax
	and bx,1
	add bx,3
	jmp @@bx_base
@@randne:
	call random_num
	mov bx,ax
	and bx,2
	inc bx
	jmp @@bx_base
@@seek:
	find_seek cx,dx,bx
	inc bx
	jmp @@bx_base

@@bx_base:
											; bx holds base dir (1-4)
	mov ax,bx
	mov bx,[old_dir]
											; check for modifiers
	test bx,32
	jz @@nocw
	dec ax

	; turn cw
	; 0 > 2
	; 1 > 3
	; 2 > 1
	; 3 > 0
	; 0/1/2/3, flip 2
	; 2/3, flip 1

	cmp al,2
	jb @@cw3
	xor al,1
@@cw3:
	xor al,2
	xor ah,ah
	inc ax

@@nocw:
	test bx,64
	je @@noopp
	dec ax
	xor ax,1
	inc ax

@@noopp:
	test bx,16
	je @@norandp
	dec al
											; flip ns / ew
	xor al,2
											; take random number whether to flip ne / sw
	mov bx,ax
	call random_num
	test ax,1
	je @@randp2
	xor bx,1
@@randp2:
	mov ax,bx
	inc ax
	jmp @@done

@@norandp:
	test bx,128
	je @@done
											; RandNot
	mov bx,ax
	call random_num 					; Get # 0-2
	xor ah,ah
	mov cl,3
	div cl   							; AL % 3 -> AH (0-2)
	inc ah   							; (1-3)
	cmp ah,bl							; If AH != BL, use, else use 4
	jne @@use_ah
	mov ax,4
	jmp @@done
@@use_ah:
	mov al,ah
	xor ah,ah
	jmp @@done

@@use_bx:
	mov ax,bx
@@done:
	ret

endp parsedir

proc sin far
arg theta:word

  shl [theta], 1
  fild [theta]
  fldpi
  fmulp st(1), st(0)
  fidiv [c_divisions]

  fsin
  fimul [multiplier]
  fistp [return_value]
  mov ax, [return_value]
  ret

endp sin

proc cos far
arg theta:word

  shl [theta], 1
  fild [theta]
  fldpi
  fmulp st(1), st(0)
  fidiv [c_divisions]
  fcos
  fimul [multiplier]
  fistp [return_value]
  mov ax, [return_value]
  ret

endp cos

proc tan far
arg theta:word

  shl theta, 1
  fild [theta]
  fldpi
  fmulp st(1), st(0)
  fidiv [c_divisions]
  fptan
  fstp st(0)
  fimul [multiplier]
  fistp [return_value]
  mov ax, [return_value]
  ret
  
endp tan
  
proc atan far
arg val:word
          
 fild [val]
 fidiv [divider]
 fld1
 fpatan
 fimul [c_divisions]
 fld const_2pi
 fdivp
 fistp [return_value]
 mov ax, [return_value]
 ret
 
endp atan

proc asin far
arg val:word
          
  fild [val]
  fidiv [divider]    
  fld st(0)                 ; put a in st(1)
  fmul st(0), st(0)         ; square a
  fld1                      ; load 1
  fsubrp                    ; 1 - a^2
  fsqrt                     ; take sqrt
  fpatan
  fimul [c_divisions]
  fld const_2pi
  fdivp
  fistp [return_value]
  mov ax, [return_value]
  ret
   
endp asin

proc acos far
arg val:word
          
  fild [val]
  fidiv [divider]    
  fld st(0)                 ; put a in st(1)
  fmul st(0), st(0)         ; squar a
  fld1                      ; load 1
  fsubrp                    ; 1 - a^2
  fsqrt                     ; take sqrt
  fxch                      ; swap st(0) and st(1)
  fpatan
  fimul [c_divisions]
  fld [const_2pi]
  fdivp
  fistp [return_value]
  mov ax, [return_value]
  ret

endp acos

ends

end
