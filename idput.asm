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
; This file contains fuctions and data for displaying boards (ids)
;

Ideal

include "data.inc"
include "const.inc"
include "idput.inc"

p186
JUMPS
include "model.inc"

Dataseg

label id_chars byte    ; Table of characters for each id, a 0 means
							 ; that the ID is based on the parameter.
							 ; 255 means the ID is exactly the parameter.

		  db      32,178,219,6,0,255,177,255,233,254,255,254,255,178,177,176; 0-15
		  db      254,255,0,0,176,24,25,26,27,0,0,160,4,4,3,9; 16-31
		  db      150,7,176,0,11,0,177,12,10,0,0,162,161,0,0,22; 32-47
		  db      196,0,7,255,255,255,255,159,0,18,29,0,206,0,0,0; 48-63
		  db      '?',178,0,151,152,153,154,32,0,42,0,0,255,255,0,0; 64-79
		  db      235,236,5,42,2,234,21,224,127,149,5,227,0,0,172,173; 80-95
		  db      '?',0; 96-97
		  db      24 dup ('?'); 98-121
		  db      0,0,0,226,232,0; 122-127
;
; Note: Bullets and players reference global data for characters
;

label thin_line byte    ; Table of characters for thin lines. Bits:
							  ; 0=Line to north
							  ; 1=Line to south
							  ; 2=Line to east
							  ; 3=Line to west

		  db      249,179,179,179 ; None, N, S, NS
		  db      196,192,218,195 ; E, NE, SE, NSE
		  db      196,217,191,180 ; W, NW, SW, NSW
		  db      196,193,194,197 ; EW, NEW, SEW, NSEW

label thick_line byte    ; Table of characters for thick lines.

		  db      249,186,186,186   ; None, N, S, NS
		  db      205,200,201,204 ; E, NE, SE, NSE
		  db      205,188,187,185 ; W, NW, SW, NSW
		  db      205,202,203,206 ; EW, NEW, SEW, NSEW

ice_anim db      32,252,253,255 ; Ice animation table

lava_anim db     176,177,178 ; Lava animation table

low_ammo db      163 ; < 10 ammunition pic
hi_ammo db       164 ; > 9 ammunition pic

lit_bomb db      171,170,169,168,167,166,165 ; Lit bomb animation

energizer_glow db 1,9,3,11,15,11,3,9 ; Energizer Glow

explosion_colors db 239,206,76,4 ; Explosion color table

horiz_door db    196 ; Horizontal door pic
vert_door db     179 ; Vertical door pic

cw_anim db       47,196,92,179 ; CW animation table
ccw_anim db      47,179,92,196 ; CCW animation table

label open_door byte     ; Table of door animation pics, use bits
			; 1 2 4 8 and 16 of the door parameter
			; to offset properly.

	db      47,47 ; Open 1/2 of type 0
	db      92,92 ; Open 1/2 of type 1
	db      92,92 ; Open 1/2 of type 2
	db      47,47 ; Open 1/2 of type 3
	db      179,196,179,196,179,196,179,196 ; Open full of all types
	db      179,196,179,196,179,196,179,196 ; Open full of all types
	db      47,47 ; Close 1/2 of type 0
	db      92,92 ; Close 1/2 of type 1
	db      92,92 ; Close 1/2 of type 2
	db      47,47 ; Close 1/2 of type 3

label transport_anims byte        ; Table of transporter animations

trans_north db    45,94,126,94 ; North
trans_south db   86,118,95,118 ; South
trans_east db      93,41,62,41 ; East
trans_west db      91,40,60,40 ; West
trans_all db      94,62,118,60 ; All directions

thick_arrow db   30,31,16,17 ; Thick arrows (pusher-style) NSEW
thin_arrow db    24,25,26,27 ; Thin arrows (gun-style) NSEW

horiz_lazer db   130,196,131,196 ; Horizontal Lazer Anim Table
vert_lazer db    128,179,129,179 ; Vertical Lazer Anim Table

fire_anim db     177,178,178,178,177,176 ; Fire animation
fire_colors db            4,6,12,14,12,6 ; Fire colors

life_anim db     155,156,157,158 ; Free life animation
life_colors db    15,11,3,11 ; Free life colors

ricochet_panels db       28,23 ; Ricochet pics

mine_anim db     143,144 ; Mine animation

shooting_fire_anim db     15,42 ; Shooting Fire Anim
shooting_fire_colors db   12,14 ; Shooting Fire Colors

seeker_anim db        "/-\|" ; Seeker animation
seeker_colors db 11,14,10,12 ; Seeker colors

whirlpool_glow db 11,3,9,15 ; Whirlpool colors

bullet_char       db 145,146,147,148 ; Player
						db 145,146,147,148 ; Neutral
						db 145,146,147,148 ; Enemy

player_char       db 2,2,2,2 ;N S E W

player_color      db 27
missile_color     db 8
bullet_color      db 15 ; Player
						db 15 ; Neutral
						db 15 ; Enemy

; Damage done by each id
label id_dmg byte     ; Table of damage for each id, a 0 means
							 ; that the ID does no damage.

		  db      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0; 0-15
		  db      0,0,0,0,0,0,0,0,0,0,100,0,0,0,0,0; 16-31
		  db      0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0; 32-47
		  db      0,0,0,0,0,0,0,0,0,0,0,10,0,10,5,5; 48-63
		  db      0,0,0,0,0,0,0,0,0,0,0,10,10,0,10,10; 64-79
		  db      10,0,0,10,10,10,10,10,10,10,10,10,0,0,10,10; 80-95
		  db      0,0; 96-97
		  db      24 dup (0); 98-121
		  db      0,0,0,0,0,0; 122-127

;
; The defaults for id_chars, id_dmg, and all inbetween, so that CHANGE
; CHAR ID can be used. (a total of 455 bytes, from ID's 0 to 454.)
;

label def_id_chars byte ;* 455 bytes *
	; id_chars
		  db      32,178,219,6,0,255,177,255,233,254,255,254,255,178,177,176; 0-15
		  db      254,255,0,0,176,24,25,26,27,0,0,160,4,4,3,9; 16-31
		  db      150,7,176,0,11,0,177,12,10,0,0,162,161,0,0,22; 32-47
		  db      196,0,7,255,255,255,255,159,0,18,29,0,206,0,0,0; 48-63
		  db      '?',178,0,151,152,153,154,32,0,42,0,0,255,255,0,0; 64-79
		  db      235,236,5,42,2,234,21,224,127,149,5,227,0,0,172,173; 80-95
		  db      '?',0; 96-97
		  db      24 dup ('?'); 98-121
		  db      0,0,0,226,232,0; 122-127
	; thin_line
		  db      249,179,179,179 ; None, N, S, NS
		  db      196,192,218,195 ; E, NE, SE, NSE
		  db      196,217,191,180 ; W, NW, SW, NSW
		  db      196,193,194,197 ; EW, NEW, SEW, NSEW
	; thick_line
		  db      249,186,186,186   ; None, N, S, NS
		  db      205,200,201,204 ; E, NE, SE, NSE
		  db      205,188,187,185 ; W, NW, SW, NSW
		  db      205,202,203,206 ; EW, NEW, SEW, NSEW
	; ice_anim
		  db      32,252,253,255 ; Ice animation table
	; lava_anim
		  db     176,177,178 ; Lava animation table
	; low_ammo, hi_ammo
		  db      163 ; < 10 ammunition pic
		  db       164 ; > 9 ammunition pic
	; lit_bomb
		  db      171,170,169,168,167,166,165 ; Lit bomb animation
	; energizer_glow
		  db 1,9,3,11,15,11,3,9 ; Energizer Glow
	; explosion_colors
		  db 239,206,76,4 ; Explosion color table
	; horiz_door, vert_door
		  db    196 ; Horizontal door pic
		  db     179 ; Vertical door pic
	; cw_anim, ccw_anim
		  db       47,196,92,179 ; CW animation table
		  db      47,179,92,196 ; CCW animation table
	; open_door
		  db      47,47 ; Open 1/2 of type 0
		  db      92,92 ; Open 1/2 of type 1
		  db      92,92 ; Open 1/2 of type 2
		  db      47,47 ; Open 1/2 of type 3
		  db      179,196,179,196,179,196,179,196 ; Open full of all types
		  db      179,196,179,196,179,196,179,196 ; Open full of all types
		  db      47,47 ; Close 1/2 of type 0
		  db      92,92 ; Close 1/2 of type 1
		  db      92,92 ; Close 1/2 of type 2
		  db      47,47 ; Close 1/2 of type 3
	; transport_anims
		  db    45,94,126,94 ; North
		  db   86,118,95,118 ; South
		  db      93,41,62,41 ; East
		  db      91,40,60,40 ; West
		  db      94,62,118,60 ; All directions
	; thick_arrow, thin_arrow
		  db   30,31,16,17 ; Thick arrows (pusher-style) NSEW
		  db    24,25,26,27 ; Thin arrows (gun-style) NSEW
	; horiz_lazer, vert_lazer
		  db   130,196,131,196 ; Horizontal Lazer Anim Table
		  db    128,179,129,179 ; Vertical Lazer Anim Table
	; fire_anim, fire_colors
		  db     177,178,178,178,177,176 ; Fire animation
		  db            4,6,12,14,12,6 ; Fire colors
	; life_anim, life_colors
		  db     155,156,157,158 ; Free life animation
		  db    15,11,3,11 ; Free life colors
	; ricochet_panels
		  db       28,23 ; Ricochet pics
	; mine_anim
		  db     143,144 ; Mine animation
	; shooting_fire_anim, shooting_fire_colors
		  db     15,42 ; Shooting Fire Anim
		  db   12,14 ; Shooting Fire Colors
	; seeker_anim, seeker_colors
		  db        "/-\|" ; Seeker animation
		  db 11,14,10,12 ; Seeker colors
	; whirlpool_glow
		  db 11,3,9,15 ; Whirlpool colors (this ends at 306 bytes, where ver
							; 1.02 used to end)
	; bullet_char
		  db 145,146,147,148 ; Player
		  db 145,146,147,148 ; Neutral
		  db 145,146,147,148 ; Enemy
	; player_char
		  db 2,2,2,2 ;N S E W
	; player_color, missile_color, bullet_color
		  db 27
		  db 8
		  db 15 ; Player
		  db 15 ; Neutral
		  db 15 ; Enemy
	; id_dmg
		  db      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0; 0-15
		  db      0,0,0,0,0,0,0,0,0,0,100,0,0,0,0,0; 16-31
		  db      0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0; 32-47
		  db      0,0,0,0,0,0,0,0,0,0,0,10,0,10,5,5; 48-63
		  db      0,0,0,0,0,0,0,0,0,0,0,10,10,0,10,10; 64-79
		  db      10,0,0,10,10,10,10,10,10,10,10,10,0,0,10,10; 80-95
		  db      0,0; 96-97
		  db      24 dup (0); 98-121
		  db      0,0,0,0,0,0; 122-127

Codeseg

;
;Code to place any ID/PARAM/COLOR combo in MegaZeux at any SCREEN x/y
;
;Note: Special color handling for: 33 (Energizer)
;                                  38 (Explosion)
;                                  63 (Fire)
;                                  66 (Free Life)
;                                  78 (Shooting Fire)
;                                  79 (Seeker)
;                                  126 (Scroll)
;
; Shows overlay if overlay mode is 1 or 2.
;
; Ignores overlay if bit 128 set.
;                                                                                         127 (Player) (read from global variable)
;

proc id_put
arg x_pos:byte,y_pos:byte,array_x:word,array_y:word,ovr_x:word,ovr_y:word,vid_seg:word

	pusha
	push es ds

	mov bx,SEG level_id						 ; Load data segment (global)
	mov ds,bx

	;
	; Calculate offset within arrays
	;

	; Put Ypos in BX, Xpos in CX
	mov ax,[array_y]
	mov cx,[array_x]
	; Multiply Ypos by max_bxsiz into SI
	mul [max_bxsiz]
	mov si,ax
	; Add Xpos
	add si,cx
	; SI holds offset

	;
	; Calculate offset within screen
	;

	; Put Ypos in BX, Xpos in AX
	mov bl,[y_pos]
	mov al,[x_pos]
	xor bh,bh
	mov ah,bh
	; Put into DI, multiply by 80, and add X position, multiply by 2.
	mov di,bx
	shl di,2
	add di,bx
	shl di,4
	add di,ax
	add di,di
	; Offset in DI

	; Screen offset in DI, array offset in SI
	; Check overlay mode

	mov ah,[ds:overlay_mode]

	; Overlay mode in AH- ignore if 0/3 or bit 128 is set.

	test ah,3
	jpe @@no_overlay                     ; If both or neither set, it is 0/3

	test ah,128
	jnz @@no_overlay

	; Save old si

	push si

	; Overlay- Use si as offset UNLESS bit 2 set

	test ah,2
	jz @@normal_overlay

	; Calculate offset from DL/DH

	; Put Ypos in BX, Xpos in DX

	mov bx,[ovr_y]
	mov dx,[ovr_x]

	; Multiply Ypos by max_bxsiz into SI

	push ax dx
	mov ax,bx
	mul [max_bxsiz]
	mov si,ax
	pop dx ax

	; Add Xpos

	add si,dx

	; Offset in SI of overlay array- load segment

@@normal_overlay:
	push ds
	lds bx,[ds:overlay]

	; Get code

	mov al,[ds:bx+si]

	; Space = no overlay

	cmp al,32
	jne @@not_space_overlay
	pop ds
	pop si
	jmp @@no_overlay
@@not_space_overlay:

	; Code in al... load color to dh

	pop ds
	lds bx,[ds:overlay_color]
	mov dh,[ds:bx+si]
	pop si

	; Al=char, Ah=color

	mov cx,[vid_seg]
	mov es,cx                     ; Video seg
	mov [es:di],al                ; Store character
	test dh,240                   ; Does current color have a background
											; color?
	jz @@overlay_needs_bk         ; Nope
@@overlay_has_bk:
	mov [es:di+1],dh					; Yep- Store color

	; Done!

	pop ds es
	popa
	ret

@@overlay_needs_bk:
	test [overlay_mode],64
	jnz @@overlay_has_bk

	; Overlay color in ah. Jump to color det. code to get background color IF
	; we are showing non-overlay

	xor dh,128  ; Set bit 128 to denote special color code
	mov ax,SEG level_id
	mov ds,ax
	les bx,[ds:level_id] ; Load level_id into es:bx
	mov ah,[es:bx+si]    ; Get id in AH
	les bx,[ds:level_param] ; Load level_param into es:bx
	mov dl,[es:bx+si]       ; Get param in Dl

	; All vars set, go do it!

	jmp @@do_color_from_overlay

	; Calculate offset as screen, by using array offset minus
	;

@@no_overlay:
@@overlay_all_done:
	test [overlay_mode],64
	jz @@okokok
	mov al,32
	jmp @@place_char
@@okokok:
	; Overlay done... screen offset in DI, array offset in SI,

	push ds
	les bx,[ds:level_id] ; Load level_id into es:bx
	mov al,[es:bx+si]    ; Get id in AL

	; Screen offset in DI, array offset in SI, id in AL
	; Load DS:BX with address of Id_chars

	mov bx,SEG id_chars
	mov ds,bx
	mov bx,OFFSET id_chars
	mov ah,al ; Save ID in AH
	xlat      ; AL holds character for id of AL

	; Screen offset in DI, array offset in SI, id in AH, character in AL

	pop es
	lds bx,[es:level_param] ; Load DS:BX w/level parameters
	mov dl,[ds:bx+si]

	mov bx,SEG id_chars
	mov ds,bx

	; Screen at DI, array offset in SI, id in AH, character in AL,
	; parameter in DL, ES holds global data, DS holds local data.

	cmp al,0ffh
	je @@check_2
	cmp al,0h
	jne @@place_char

	;
	; Process al=0 (parameter-based characters)
	;

	cmp ah,123
	je @@cRobot
	cmp ah,124
	je @@cRobot                ; Non-pushable robot
	cmp ah,4
	je @@cLine
	cmp ah,18
	je @@cWeb
	cmp ah,19
	je @@cWeb                  ; Web code differentiates between thin/thick
	cmp ah,25
	je @@cIce
	cmp ah,26
	je @@cLava
	cmp ah,35
	je @@cAmmo
	cmp ah,37
	je @@cLitBomb
	cmp ah,41
	je @@cDoor
	cmp ah,45
	je @@cCw
	cmp ah,46
	je @@cCcw
	cmp ah,42
	je @@cOpenDoor
	cmp ah,49
	je @@cTransport
	cmp ah,56
	je @@cPusher
	cmp ah,59
	je @@cLazer
	cmp ah,61
	je @@cBullet
	cmp ah,62
	je @@cPusher               ; Missile uses same shapes as pusher
	cmp ah,63
	je @@cFire
	cmp ah,66
	je @@cLife
	cmp ah,72
	je @@cRicochet
	cmp ah,74
	je @@cMine
	cmp ah,75
	je @@cPusher               ; Spike uses same shapes as pusher
	cmp ah,78
	je @@cShootingFire
	cmp ah,79
	je @@cSeeker
	cmp ah,92
	je @@cBulletGun
	cmp ah,93
	je @@cBulletGun            ; Spinning gun uses same shapes as bullet gun
	cmp ah,97
	je @@cMissileGun
	cmp ah,122
	je @@cSensor

	;
	; Must be player...
	;

	xor bh,bh
	mov bl,[es:player_last_dir] ; Get direction to FACE
	shr bl,4
	mov al,[ds:player_char+bx]; Load player character
	jmp @@place_char

@@cLine:

	;
	; Place line wall- Find out if other lines to n/s/e/w, then calculate
	; proper ThickLine character to use.
	;

	lds bx,[es:level_id]   ; Load seg:offs of level ids

	xor al,al                     ; Clear AL to fill with "direction" bits
	sub si,[es:max_bxsiz]
	cmp [array_y],0               ; Check to see if at top of array
	je @@cLineNorth               ; ADD NORTH
	cmp [BYTE PTR ds:bx+si],4     ; Check north
	jne @@cLine2
@@cLineNorth:
	setflag al,1                  ; "or" in NORTH bit
@@cLine2:
	add si,[es:max_bxsiz]
	add si,[es:max_bxsiz]
	mov dx,[es:board_ysiz]
	dec dx
	cmp [array_y],dx              ; Check to see if at bottom of array
	je @@cLineSouth               ; ADD SOUTH
	cmp [BYTE PTR ds:bx+si],4     ; Check south
	jne @@cLine3
@@cLineSouth:
	setflag al,2                  ; "or" in SOUTH bit
@@cLine3:
	sub si,[es:max_bxsiz]
											; Check for west edge of screen
	cmp [array_x],0               ; Check for west edge
	je @@cLineWest                ; ADD WEST
	cmp [BYTE PTR ds:bx-1+si],4   ; Check west
	jne @@cLine4                  ; NO WEST
@@cLineWest:
	setflag al,8                  ; "or" in WEST bit
@@cLine4:
											; Check for east edge of screen
	mov dx,[es:board_xsiz]
	dec dx
	cmp [array_x],dx
	je @@cLineEast                ; ADD EAST
	cmp [BYTE PTR ds:bx+1+si],4   ; Check east
	jne @@cLine5                  ; NO EAST
@@cLineEast:
	setflag al,4                  ; "or" in EAST bit
@@cLine5:
	;AL contains array index into "thick_line" matrix
	mov bx,SEG thick_line
	mov ds,bx
	mov bx,OFFSET thick_line      ; Load table offset
	xlat                          ; AL contains character!
	jmp @@place_char

@@cWeb:

	;
	; Place web- Find out if any non-spaces to n/s/e/w then calculate ThinLine.
	;

	lds bx,[es:level_id]

	xor al,al                     ; Clear AL to fill with "direction" bits
	sub si,[es:max_bxsiz]
	cmp [array_y],0               ; Check to see if at top of array
	je @@cWebNorth                ; ADD NORTH
	cmp [BYTE PTR ds:bx+si],0     ; Check north for any non-space
	je @@cWeb2
@@cWebNorth:
	setflag al,1                  ; "or" in NORTH bit
@@cWeb2:
	add si,[es:max_bxsiz]
	add si,[es:max_bxsiz]
	mov dx,[es:board_ysiz]
	dec dx
	cmp [array_y],dx              ; Check to see if at bottom of array
	je @@cWebSouth                ; ADD SOUTH
	cmp [BYTE PTR ds:bx+si],0     ; Check south for any non-space
	je @@cWeb3
@@cWebSouth:
	setflag al,2                  ; "or" in SOUTH bit
@@cWeb3:
	sub si,[es:max_bxsiz]
											; Check for west edge of screen
	cmp [array_x],0
	je @@cWebWest                 ; ADD WEST
	cmp [BYTE PTR ds:bx-1+si],0   ; Check west for any non-space (0)
	je @@cWeb4                    ; NO WEST
@@cWebWest:
	setflag al,8                  ; "or" in WEST bit
@@cWeb4:
											; Check for east edge of screen
	mov dx,[es:board_xsiz]
	dec dx
	cmp [array_x],dx
	je @@cWebEast                 ; ADD EAST
	cmp [BYTE PTR ds:bx+1+si],0   ; Check east for any non-space
	je @@cWeb5                    ; NO EAST
@@cWebEast:
	setflag al,4                  ; "or" in EAST bit
@@cWeb5:
											; AL contains array index into web matrix
	mov bx,SEG thick_line     ; Reload local data segment
	mov ds,bx
	cmp ah,18                     ; Check which type of web; Is it thin?
	je @@cThinWeb                 ; Yes
	mov bx,OFFSET thick_line      ; No, load offset of thick lines
	xlat
	jmp @@place_char
@@cThinWeb:
	mov bx,OFFSET thin_line       ; Load table offset
	xlat                          ; AL contains character!
	jmp @@place_char

@@cIce:
	mov al,dl                     ; Load array index
	mov bx,OFFSET ice_anim        ; Load array offset
	xlat                          ; Load character
	jmp @@place_char

@@cLava:
	mov al,dl                     ; Load array index
	mov bx,OFFSET lava_anim       ; Load array offset
	xlat
	jmp @@place_char

@@cAmmo:
	cmp dl,10                     ; Check amount of ammo-
	jb @@cAmmo2                   ; Is it less than 10?
	mov al,[hi_ammo]              ; Nope, use high_ammo pic
	jmp @@place_char
@@cAmmo2:                        ; Yep.
	mov al,[low_ammo]             ; Use low_ammo pic
	jmp @@place_char

@@cLitBomb:
	mov al,dl                     ; Load array index
	mov bx,OFFSET lit_bomb        ; Load array offset
	maskflag al,15                ; Use only lower nybble
	xlat
	jmp @@place_char

@@cDoor:
	testflag dl,1                 ; Check bit 1
	jz @@cDoor2                   ; Jump if horiz
	mov al,[vert_door]            ; Nope.
	jmp @@place_char
@@cDoor2:                        ; Horiz.
	mov al,[horiz_door]
	jmp @@place_char

@@cCw:
	mov al,dl                     ; Load array index
	mov bx,OFFSET cw_anim         ; Load array offset
	xlat
	jmp @@place_char

@@cCcw:
	mov al,dl
	mov bx,OFFSET ccw_anim
	xlat
	jmp @@place_char

@@cOpenDoor:
	mov al,dl                     ; Load array offset
	maskflag al,31                ; Use bits 1, 2, 4, 8, & 16
	mov bx,OFFSET open_door       ; Use this cool table!
	xlat
	jmp @@place_char

@@cTransport:
	mov al,dl                     ; Load parameter for checking
	maskflag dl,7h                ; Mask out unneeded bits
											; and check direction
	cmp dl,0                      ; North?
	je @@cTransN
	cmp dl,1                      ; South?
	je @@cTransS
	cmp dl,2                      ; East?
	je @@cTransE
	cmp dl,3                      ; West?
	je @@cTransW
											; Nope, use "all" direction
	mov bx,OFFSET trans_all       ; Load array offset,
	jmp @@cTrans2                 ; Jump to common transporter code
@@cTransN:
	mov bx,OFFSET trans_north
	jmp @@cTrans2
@@cTransS:
	mov bx,OFFSET trans_south
	jmp @@cTrans2
@@cTransE:
	mov bx,OFFSET trans_east
	jmp @@cTrans2
@@cTransW:
	mov bx,OFFSET trans_west
@@cTrans2:                       ; Common transporter char code
	maskflag al,24                ; Use only bits 8 & 16
	shr al,3                      ; Shift to be in bits 1 & 2
	xlat                          ; BX already loaded; Index array!
	jmp @@place_char

@@cPusher:
	mov al,dl                     ; Load array index
	mov bx,OFFSET thick_arrow     ; Use thick_arrow table
	xlat
	jmp @@place_char

@@cLazer:
	mov al,dl                     ; Load array index
											; Determine orientation w/test
	testflag al,1                 ; Is it horiz?
	jz @@cLazer2
	mov bx,OFFSET vert_lazer      ; Nope, vertical
	jmp @@cLazer3                 ; Jump to array indexing section
@@cLazer2:
	mov bx,OFFSET horiz_lazer     ; Load horiz table
@@cLazer3:                       ; Array indexing section
	maskflag al,6                 ; Use only bits 2 & 4
	shr al,1                      ; Translate to bits 1 & 2
	xlat
	jmp @@place_char

@@cBullet:
	mov al,dl
	mov bx,OFFSET bullet_char     ; Param already a perfect translation
	xlat                          ; of direction and type.
	jmp @@place_char

@@cFire:
	mov al,dl                     ; Load array index
	mov bx,OFFSET fire_anim       ; Load table offset
	xlat
	jmp @@place_char

@@cLife:
	mov al,dl                     ; Load array index
	mov bx,OFFSET life_anim       ; Load table offset
	xlat
	jmp @@place_char

@@cRicochet:
	mov al,dl                     ; Load array index
	mov bx,OFFSET ricochet_panels ; Load table offset
	xlat
	jmp @@place_char

@@cMine:
	mov al,dl                     ; Load array index
	maskflag al,1                 ; Use only first bit
	mov bx,OFFSET mine_anim       ; Load table offset
	xlat
	jmp @@place_char

@@cShootingFire:
	mov al,dl                     ; Load array index
	maskflag al,1                 ; Use only first bit
	mov bx,OFFSET shooting_fire_anim
	xlat
	jmp @@place_char

@@cSeeker:
	mov al,dl                     ; Load array index
	maskflag al,03h
	mov bx,OFFSET seeker_anim     ; Load table offset
	xlat
	jmp @@place_char

@@cBulletGun:
	mov al,dl                     ; Load array index
	maskflag al,24                ; Use only bits 8 & 16
	shr al,3                      ; Translate to bits 1 & 2
	mov bx,OFFSET thin_arrow      ; Use thin_arrow table
	xlat
	jmp @@place_char

@@cMissileGun:
	mov al,dl                     ; Load array index
	maskflag al,24                ; Use only bits 8 & 16
	shr al,3                      ; Translate to bits 1 & 2
	mov bx,OFFSET thick_arrow     ; Use thick_arrow table
	xlat
	jmp @@place_char

@@cSensor:

	;
	; Look up sensor #param in table and grab character
	;

	mov bx,SEG sensors
	mov es,bx
	les bx,[sensors]

	push ax                       ; save ax (for ah- id)

	mov al,dl                     ; Load param
	mov ah,SENSOR_SIZE            ; Load sensor size, and
	mul ah                        ; Multiply.
	add bx,ax                     ; Move into bx for offset

	pop ax                        ; restore ax

	add bx,15                     ; Point to sensor_char location
	mov al,[es:bx]                ; Load sensor character

	jmp @@place_char

@@cRobot:

	;
	; Look up robot #param in table and grab character
	;

	mov bx,SEG robots
	mov es,bx
	les bx,[robots]

	push ax                       ; save ax (for ah- id)

	mov al,dl                     ; Load param
	mov ah,ROBOT_SIZE             ; Load robot size, and
	mul ah                        ; Multiply.
	add bx,ax                     ; Move into bx for offset

	pop ax                        ; restore ax

	add bx,19                     ; Point to robot_char location
	mov al,[es:bx]                ; Load robot character

	jmp @@place_char

@@check_2:

	;
	; "char:=param" code in char byte ( FF )
	;

	mov al,dl                     ; Yep, character is now parameter byte

@@place_char:
	mov cx,[vid_seg]
	mov es,cx							; Video seg
	mov [es:di],al                ; Store character
	xor dh,dh
	test [overlay_mode],64
	jz @@do_color_from_overlay
	mov ch,7
	jmp @@no_add_overlay_color

@@do_color_from_overlay:

	; Screen in ES:DI, array offset in SI, id in AH, AL now unused,
	; parameter in DL, DS undefined but set to global data below

	; If DH is 0, normal color code. If DH is non-zero, it already holds
	; overlay color and is only waiting for a bk color. (bit 128 is also
	; set, clear it first)

	mov bx,SEG level_color
	mov ds,bx
	les bx,[ds:level_color]       ; Load seg:offs of level colors

	mov ch,[es:bx+si]             ; Load color

	;
	; Load segments- ES for global, DS for local
	mov bx,ds
	mov es,bx
	mov bx,SEG id_chars
	mov ds,bx

	;
	; Check for special cases where color is not based on "color" value
	;

	cmp ah,67                     ; Check for 67-70 (whirlpool) first
	jb @@no_wp
	cmp ah,70
	jbe @@color_whirlpool
@@no_wp:
	cmp ah,33
	je @@color_energizer
	cmp ah,38
	je @@color_explosion
	cmp ah,63
	je @@color_fire
	cmp ah,66
	je @@color_life
	cmp ah,78
	je @@color_shooting_fire
	cmp ah,79
	je @@color_seeker
	cmp ah,126
	je @@color_scroll
	cmp ah,127
	jne @@normal_color

	; Color_player

	mov al,[ds:player_color]
	jmp @@spec_color_bk_and_fg

@@color_energizer:

	mov al,dl                     ; Load parameter for array index
	mov bx,OFFSET energizer_glow
	xlat                          ; Index into energizerGlow[] (near data)
	jmp @@end_spec_color          ; Go to keep the orig. background color

@@color_explosion:

	mov al,dl                     ; Load parameter for array index
	maskflag al,0fh               ; and limit to first 4 bits
	mov bx,OFFSET explosion_colors
	xlat
	jmp @@spec_color_bk_and_fg

@@color_fire:

	mov al,dl                     ; Load array index
	mov bx,OFFSET fire_colors
	xlat
	jmp @@end_spec_color

@@color_whirlpool:

	mov al,ah                     ; Copy id
	sub al,67                     ; Convert id of 67-70 to array offset of 0-3
	mov bx,OFFSET whirlpool_glow
	xlat
	jmp @@end_spec_color

@@color_life:

	mov al,dl
	mov bx,OFFSET life_colors
	xlat
	jmp @@end_spec_color

@@color_shooting_fire:

	mov al,dl
	maskflag al,1
	mov bx,OFFSET shooting_fire_colors
	xlat
	jmp @@end_spec_color

@@color_seeker:

	mov al,dl
	maskflag al,03h
	mov bx,OFFSET seeker_colors
	xlat
	jmp @@end_spec_color

@@color_scroll:

	mov al,[ds:scroll_color]

@@end_spec_color:
											; Color is in al; copy to FOREGROUND portion of ch
											; Copy bk if non 0, also
	test al,0f0h
	jz @@spec_color_fg_only

	; Copy fg AND bk
@@spec_color_bk_and_fg:
	mov ch,al
	jmp @@normal_color

@@spec_color_fg_only:
	maskflag al,0fh               ; Mask for foreground only
	maskflag ch,0f0h              ; Mask for background only
	or ch,al                      ; Add together

@@normal_color:
	les bx,[es:level_under_color] ; Load seg:offs of level under colors
	mov cl,[es:bx+si]             ; Load under color

	; Screen offset in DI, array offset in SI, id in AH,
	; under color in CL, color in CH, parameter in DL

	cmp ch,0fh                    ; Check for whether color has a background
	ja @@no_add_bk                ; Yep! Already has a background color
											; Nope, add in bk from under color
	maskflag cl,0f0h
	add ch,cl

@@no_add_bk:

	; Check- add overlay color?
	or dh,dh
	jz @@no_add_overlay_color
	; Replace foreground with overlay foreground (remembering to reset bit
	; 128 of overlay color in DH first)
	xor dh,128
	and ch,240
	add ch,dh

@@no_add_overlay_color:
	mov bx,[vid_seg]
	mov es,bx                     ; Video Segment
	mov [es:di+1],ch              ; Store in color memory

	pop ds es
	popa
	ret

endp id_put

;
; Draw edit window according to global variables; Size is 80x19 or VIRTUAL
; SIZE, whichever is smaller, drawn at 0/0.
; Top left corner is indexed into array according to parameters.
;

proc draw_edit_window far
arg array_x:word,array_y:word,vid_seg:word

	push ax bx cx dx di si ds

	; Load global data seg
	mov bx,SEG board_xsiz
	mov ds,bx

	; Determine x size (in bl) and y size (in bh)

	mov cx,[board_xsiz]
	cmp cx,80
	jb @@use_virt_xs              ; Size is below 80; use it.
	mov cx,80                     ; Use size of 80
@@use_virt_xs:
	mov dx,[board_ysiz]
	cmp dx,19
	jb @@use_virt_ys              ; Size is below 19; use it.
	mov dx,19                     ; Use size of 19
@@use_virt_ys:

	mov si,[array_x]              ; Load starting array position
	mov di,[array_y]
	xor ax,ax                     ; Load starting screen position (0/0)
	xor bx,bx

	; AX=screen pos
	; BX=x/y count
	; SI=array x
	; DI=array y
	; CX=array x max
	; DX=array y max

@@e_draw_loop1:

	push ax bx ds
	mov bl,ah
	xor ah,ah
	mov bh,ah
	call id_put,ax,bx,si,di,si,di,[vid_seg]; Print ID
	pop ds bx ax

	inc al                        ; One to right
	inc si
	inc bl                        ; Increase width count
	cmp bl,cl
	jb @@e_draw_loop1
	xor bl,bl                     ; Reset width count
	xor al,al                     ; Reset screen X
	mov si,[array_x]              ; and array X
	inc ah                        ; One down
	inc di
	inc bh                        ; Increase height count
	cmp bh,dl
	jb @@e_draw_loop1

	; Done

	pop ds si di dx cx bx ax
	ret

endp draw_edit_window

;
; Draw game window according to global variables; Doesn't draw edge lines.
; Top left corner is indexed into array according to parameters.
;

proc draw_game_window far
arg array_x:word,array_y:word,vid_seg:word

	push ax bx cx dx si di ds

	; Load global data seg
	mov bx,SEG board_xsiz
	mov ds,bx

	mov cl,[viewport_xsiz]
	mov dl,[viewport_ysiz]

	mov si,[array_x]              ; Load starting array position
	mov di,[array_y]
	mov al,[viewport_x]
	mov ah,[viewport_y]
	xor bx,bx

	; AX=screen pos
	; BX=x/y count
	; SI=array x
	; DI=array y
	; CX=count max (x)
	; DX=count max (y)

@@e_draw_loop1:

	push ax bx cx dx ds
	mov cl,bl
	mov dl,bh
	mov bl,ah
	xor ah,ah
	mov bh,ah
	mov ch,ah
	mov dh,ah
	call id_put,ax,bx,si,di,cx,dx,[vid_seg]; Print ID
	pop ds dx cx bx ax

	inc al                        ; One to right
	inc si
	inc bl                        ; Increase width count
	cmp bl,cl
	jb @@e_draw_loop1
	xor bl,bl                     ; Reset width count
	mov al,[viewport_x]           ; Reset screen X
	mov si,[array_x]              ; and array X
	inc ah                        ; One down
	inc di
	inc bh                        ; Increase height count
	cmp bh,dl
	jb @@e_draw_loop1

	; Done

	pop ds di si dx cx bx ax
	ret

endp draw_game_window

ends

end

end