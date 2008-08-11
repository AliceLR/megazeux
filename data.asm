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
; This file contains most global data, initialized and uninitialized, far.
;

Ideal

include "data.inc"
include "const.inc"

p186
JUMPS
include "model.inc"

Dataseg

ALIGN BYTE

board_where			db NUM_BOARDS dup ( W_NOWHERE ) ;Array of where boards are
curr_board        db 0                            ;Current board
curr_file         db "CAVERNS.MZX",0,FILENAME_SIZE-12 dup ( ? )
																  ;Current MZX file
																  ;(or startup file)
curr_sav				db	"SAVED.SAV",0,FILENAME_SIZE-10 dup ( ? )
																  ;Current SAV file
help_file         db 0,PATHNAME_SIZE dup ( ? )    ;Drive + Path + Filename
config_file       db 0,PATHNAME_SIZE dup ( ? )    ;Drive + Path + Filename
MSE_file       	db 0,PATHNAME_SIZE dup ( ? )    ;Drive + Path + Filename
mzx_blank_mod_file db 0,PATHNAME_SIZE dup ( ? )   ;Drive + Path + Filename
mzx_convert_mod_file db 0,PATHNAME_SIZE dup ( ? ) ;Drive + Path + Filename
megazeux_dir      db 0,PATHNAME_SIZE dup ( ? )    ;Directory started in
current_dir       db 0,PATHNAME_SIZE dup ( ? )    ;Current directory
megazeux_drive    db 2                            ;Startup drive (0=A...)
current_drive     db 2                            ;Current drive (0=A...)
quicksave_file    db 0,FILENAME_SIZE dup ( ? )    ;Current quicksave filename
board_xsiz        dw 80                           ;Size of board in memory
board_ysiz        dw 25
overlay_mode      db OVERLAY_OFF						  ;Mode for overlay
max_bsiz_mode		db 2
max_bxsiz			dw 100
max_bysiz			dw 100

; Mouse state stuff
saved_mouse_x  dw 0
saved_mouse_y  dw 0
saved_mouse_buttons dw 0

; Board "save chunk"

mod_playing       db 0,FILENAME_SIZE-1 dup ( ? )  ;Mod currently playing
real_mod_playing  db 0,FILENAME_SIZE-1 dup ( ? )  ;Real mod currently playing
refresh_mod_playing db 1                          ;Load mod no matter what
viewport_x        db 3                            ;X pos of UL corner of view
viewport_y        db 2
viewport_xsiz     db 74                           ;Size of view
viewport_ysiz     db 21
can_shoot         db 1                            ;Allowed to shoot on board
can_bomb          db 1									  ;Allowed to bomb on board
fire_burn_brown 	db 0
fire_burn_space 	db 1
fire_burn_fakes 	db 1
fire_burn_trees 	db 1
explosions_leave  db EXPL_LEAVE_ASH
save_mode         db CAN_SAVE
forest_becomes    db FOREST_TO_EMPTY
collect_bombs     db 1                            ;If zero, bombs auto set
fire_burns        db FIRE_BURNS_FOREVER
board_dir         db 4 dup ( NO_BOARD )           ;Boards to dirs
restart_if_zapped db 0
time_limit        dw 0                            ;Stays the same, for
																  ;resetting time when it
																  ;runs out.
last_key          db '?'                          ;Last key pressed
num_input         dw 0                            ;Numeric form of input
input_size        db 0                            ;Size of input
input_string      db 0,80 dup ( ? )               ;Last inputted string
player_last_dir 	db 16                           ;Last direction of player
bottom_mesg       db 0,80 dup ( ? )            	  ;Message at bottom of screen
b_mesg_timer      db 0                            ;Timer for bottom message
lazwall_start     db 7                            ;Current LazWalls to fire
b_mesg_row        db 24                           ;Row to display message on
b_mesg_col 			db 255                          ;Column (255=centered)
scroll_x          dw 0                            ;+/- to x from scrollview
scroll_y          dw 0                            ;+/- to y
locked_x          dw -1									  ;Position in array of ul
locked_y				dw -1                           ;corner of viewport when
																  ;scroll locked (-1=none)
player_ns_locked  db 0
player_ew_locked  db 0
player_attack_locked	db 0
volume            db 127
volume_inc        db 0                            ;For fading (negative okay)
volume_target     db 127                          ;Target volume for fading

; World "save chunk"

edge_color        db 8                            ;Color OUTSIDE viewport
first_board       db 0                            ;First board to play
endgame_board     db NO_ENDGAME_BOARD             ;No board=Game Over
death_board       db NO_DEATH_BOARD               ;No board=Restart same
endgame_x         dw 0
endgame_y         dw 0
game_over_sfx		db 1
death_x           dw 0
death_y           dw 0
starting_lives    dw 5
lives_limit       dw 99                           ;Limit's Limit- 65535
starting_health   dw 100
health_limit      dw 200                          ;Limit's Limit- 65535
enemy_hurt_enemy  db 0
clear_on_exit     db 0                            ;1=Clear message/projeciles between screens
only_from_swap    db 0                            ;Set if only playable from
																  ;a SWAP robot command
; Save game "save chunk"

keys              db NUM_KEYS dup ( NO_KEY)       ;Array of keys
score             dd 0
blind_dur         db 0                            ;Cycles left
firewalker_dur    db 0
freeze_time_dur 	db 0
slow_time_dur     db 0
wind_dur          db 0
pl_saved_x 			dw 0,0,0,0,0,0,0,0              ;Array of 8
pl_saved_y 			dw 0,0,0,0,0,0,0,0              ;Array of 8
pl_saved_board 	db 0,0,0,0,0,0,0,0              ;Array of 8
saved_pl_color    db 27									  ;Saves color for energizer
under_player_id	db 0
under_player_color	db 7
under_player_param	db 0
mesg_edges        db 1

;

current_page      db 0                            ;0 or 1
current_pg_seg    dw VIDEO_SEG                    ;Segment of 0 or 1
status_shown_counters db NUM_STATUS_CNTRS dup ( 0,COUNTER_NAME_SIZE-1 dup ( ? ) )
																  ;Counters shown on status screen
music_on          db 1                            ;If music is on
sfx_on            db 1                            ;If pc sfx is on
music_device      db 0                            ;Music device
mixing_rate			dw 30 								  ;Mixing rate
sfx_channels		db 2									  ;# of sfx channels
music_gvol			db 8									  ;Global Music volume (x/8)
sound_gvol			db 8									  ;Global Sound volume (x/8)
overall_speed     db 4                            ;1 through 7
player_x          dw 0                            ;Not updated during "update"
player_y          dw 0                            ; "     "      "       "

scroll_color      db 15                           ;Current scroll color
protection_method db NO_PROTECTION
password          db 0,15 dup ( ? )
cheats_active     db 0                            ;(additive flag)
current_help_sec  db 0									  ;Use for context-sens.help


label flags word                    ; Aray of thing's flags (Note- A_ITEM is
												; simply a A_SPEC_TOUCH) A_SPEC_PUSH
												; implies A_PUSHABLE in most cases.
	dw A_UNDER                       ; Space
	dw 0,0,0,0,0                     ; Normal, Solid, Tree, Line, CustomBlock
	dw 2 dup (A_SHOOTABLE+A_BLOW_UP) ; Breakaway, CustomBreak
	dw 3 dup (A_PUSHABLE+A_BLOW_UP)  ; Boulder, Crate, CustomPush
	dw 2 dup (A_PUSHABLE)            ; Box, CustomSolidPush
	dw 5 dup (A_UNDER)               ; Fake, Carpet, Floor, Tiles, CustomFloor
	dw 2 dup (A_UNDER+A_BLOW_UP)		; Web, ThickWeb
	dw A_UNDER								; StillWater (20)
	dw 4 dup (A_UNDER+A_AFFECT_IF_STOOD) ; NWater, SWater, EWater, WWater
	dw 2 dup (A_UNDER+A_AFFECT_IF_STOOD+A_UPDATE) ; Ice, Lava
	dw A_ITEM+A_BLOW_UP              ; Chest
	dw 2 dup (A_ITEM+A_PUSHABLE+A_BLOW_UP+A_SHOOTABLE) ; Gem, MagicGem
	dw A_ITEM+A_PUSHABLE             ; Health
	dw 2 dup (A_ITEM+A_PUSHABLE)     ; Ring, Potion
	dw A_ITEM+A_PUSHABLE+A_UPDATE    ; Energizer
	dw A_UNDER+A_ITEM					   ; Goop
	dw A_ITEM+A_PUSHABLE             ; Ammo
	dw A_ITEM+A_PUSHABLE+A_EXPLODE   ; Bomb
	dw A_PUSHABLE+A_EXPLODE+A_UPDATE ; LitBomb
	dw A_HURTS+A_UPDATE              ; Explosion
	dw A_ITEM+A_PUSHABLE             ; Key
	dw A_ITEM                        ; Lock (40)
	dw A_ITEM                        ; Door
	dw A_UPDATE                      ; Opening/closing door
	dw 2 dup (A_ENTRANCE+A_UNDER)    ; Stairs, Cave
	dw 2 dup (A_UPDATE)              ; Cw, Ccw
	dw A_ITEM                        ; Gate
	dw A_UPDATE+A_UNDER              ; OpenGate
	dw A_SPEC_PUSH+A_ITEM+A_UPDATE   ; Transport
	dw A_ITEM+A_PUSHABLE             ; Coin
	dw 4 dup (A_UPDATE)              ; MovingWall -NSEW-
	dw A_ITEM+A_PUSHABLE+A_BLOW_UP   ; Pouch
	dw A_UPDATE                      ; Pusher
	dw A_PUSHNS                      ; SliderNS
	dw A_PUSHEW                      ; SliderEW
	dw A_UPDATE+A_HURTS              ; Lazer
	dw A_UPDATE                      ; LazerWallShooter (60)
	dw A_UPDATE+A_BLOW_UP+A_SHOOTABLE+A_ITEM+A_ENEMY ; Bullet
	dw A_UPDATE+A_HURTS+A_EXPLODE    ; Missile
	dw A_UPDATE+A_UNDER+A_AFFECT_IF_STOOD ; Fire
	dw 0                             ; 64='?'
	dw A_ITEM                        ; Forest
	dw A_PUSHABLE+A_ITEM+A_UPDATE    ; Life
	dw 4 dup (A_UPDATE+A_UNDER+A_ENTRANCE) ; Whirlpool -1234-
	dw A_ITEM                        ; Invisible
	dw A_SPEC_SHOT                   ; RicochetPanel
	dw A_SPEC_SHOT                   ; Ricochet
	dw A_ITEM+A_SPEC_SHOT+A_SPEC_BOMB+A_UPDATE ; Mine
	dw 2 dup (A_HURTS)               ; Spike, CustomHurt
	dw 0                             ; Text
	dw A_UPDATE+A_ENEMY+A_BLOW_UP    ; ShootingFire
	dw A_UPDATE+A_ENEMY+A_BLOW_UP+A_PUSHABLE ; Seeker
	dw A_UPDATE+A_PUSHABLE+A_ENEMY+A_SHOOTABLE+A_BLOW_UP ; Snake (80)
	dw A_UPDATE+A_EXPLODE+A_ITEM+A_SPEC_SHOT+A_PUSHABLE ; Eye
	dw A_UPDATE+A_BLOW_UP+A_PUSHABLE+A_ITEM+A_SHOOTABLE ; Thief
	dw A_UPDATE+A_ITEM+A_SPEC_BOMB+A_SPEC_SHOT ; SlimeBlob
	dw A_UPDATE+A_ENEMY+A_BLOW_UP+A_PUSHABLE+A_SPEC_SHOT ; Runner
	dw A_UPDATE+A_ITEM+A_PUSHABLE+A_SPEC_BOMB+A_SPEC_SHOT ; Ghost
	dw A_UPDATE+A_SPEC_SHOT+A_SPEC_BOMB+A_ITEM ; Dragon
	dw A_UPDATE+A_ITEM+A_BLOW_UP+A_SPEC_SHOT+A_PUSHABLE ; Fish
	dw A_UPDATE+A_BLOW_UP+A_SHOOTABLE+A_ENEMY ; Shark
	dw A_UPDATE+A_ENEMY+A_BLOW_UP+A_SPEC_SHOT+A_PUSHABLE ; Spider
	dw A_UPDATE+A_ENEMY+A_SHOOTABLE+A_PUSHABLE+A_BLOW_UP ; Goblin (90)
	dw A_UPDATE+A_PUSHABLE+A_SHOOTABLE+A_ENEMY+A_BLOW_UP ; Tiger
	dw 2 dup (A_UPDATE)              ; BulletGun, SpinningGun
	dw A_UPDATE+A_ENEMY+A_SPEC_SHOT+A_PUSHABLE+A_BLOW_UP ; Bear
	dw A_UPDATE+A_BLOW_UP+A_ENEMY+A_SHOOTABLE+A_PUSHABLE ; BearCub
	dw 0                             ; 96='?'
	dw A_UPDATE                      ; MissileGun
	dw 24 dup (0)                    ; 98-121='?'
	dw A_SPEC_STOOD+A_SPEC_PUSH      ; Sensor
	dw A_ITEM+A_UPDATE+A_SPEC_SHOT+A_SPEC_BOMB+A_SPEC_PUSH ; Robot (pushable)
	dw A_ITEM+A_UPDATE+A_SPEC_SHOT+A_SPEC_BOMB ; Robot
	dw A_ITEM                        ; Sign
	dw A_ITEM+A_PUSHABLE             ; Scroll
	dw A_SPEC_SHOT+A_SPEC_PUSH+A_SPEC_BOMB ; Player (127) (SPEC_PUSH in case
														; there is a sensor beneath)

board_list        dd 0 ; Far pointer to array of board names
board_offsets     dd 0 ; Far pointer to array of mem/file offsets
board_sizes       dd 0 ; Far pointer to array of sizes
board_filenames   dd 0 ; Far pointer to array of filenames

level_id          dd 0 ; Far pointer to array of 100x100
level_color       dd 0
level_param       dd 0
level_under_id    dd 0
level_under_color dd 0
level_under_param dd 0
overlay           dd 0
overlay_color     dd 0
update_done 		dd 0 ; Whether it's been updated


ends

end
