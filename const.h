/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Constants */
#ifndef __CONST_H
#define __CONST_H

#define OVERLAY_OFF			0
#define OVERLAY_ON			1
#define OVERLAY_STATIC		2
#define OVERLAY_TRANS		3

#define EXPL_LEAVE_SPACE	0
#define EXPL_LEAVE_ASH		1
#define EXPL_LEAVE_FIRE		2

#define CAN_SAVE				0
#define CANT_SAVE				1
#define CAN_SAVE_ON_SENSOR	2

#define FOREST_TO_EMPTY		0
#define FOREST_TO_FLOOR		1

#define FIRE_BURNS_LIMITED	0
#define FIRE_BURNS_FOREVER	1

#define NO_BOARD				255
#define NO_ENDGAME_BOARD	255
#define NO_DEATH_BOARD		255
#define DEATH_SAME_POS		254

#define PLAYER_BULLET		0
#define NEUTRAL_BULLET		1
#define ENEMY_BULLET			2

#define DIR_IDLE				0
#define DIR_NONE				0
#define DIR_N					1
#define DIR_S					2
#define DIR_E					3
#define DIR_W					4
#define DIR_RANDNS			5
#define DIR_RANDEW			6
#define DIR_RANDNE			7
#define DIR_RANDNB			8
#define DIR_SEEK				9
#define DIR_RANDANY			10
#define DIR_UNDER				11
#define DIR_ANYDIR			12
#define DIR_FLOW				13
#define DIR_NODIR				14
#define DIR_RANDB				15
//These are added to the above or checked using AND.
#define DIR_RANDP				16
#define DIR_CW					32
#define DIR_OPP				64
#define DIR_CCW				96
#define DIR_RANDNOT			128

#define HORIZONTAL			0
#define VERTICAL				1

#define NO_KEY					127

#define NO_PROTECTION		0
#define NO_SAVING				1
#define NO_EDITING			2
#define NO_PLAYING			3

//"SIZE" includes terminating \0
#define PATHNAME_SIZE		129
#define FILENAME_SIZE		13
#define NUM_BOARDS		   150 //Haven't increased this yet, over 255 causes mzx to crash on load
											 //Each board requires considerable amount of conventional mem Spid
#define BOARD_NAME_SIZE		25
#define COUNTER_NAME_SIZE	15  //Possibly decrease counter name size to gain conventional mem? Spid
#define NUM_KEYS				16

//Where something is in memory (CURRENT means it is in special variables for
//the current "whatever", MEMORY means stored in conventional memory,
//GROUPTEMPFILE means stored in a tempfile along with others of the same
//type, NOWHERE means it doesn't exist or is in it's original file)
//IMPORT means it is waiting for import.
#define W_NOWHERE				0
#define W_CURRENT 			1
#define W_MEMORY				2
#define W_TEMPFILE			3
#define W_GROUPTEMPFILE		4
#define W_EMS					5
#define W_IMPORT				6

#define ARRAY_DIR_N			-100
#define ARRAY_DIR_S			100
#define ARRAY_DIR_E			1
#define ARRAY_DIR_W			-1

//Attribute flags
#define A_PUSHNS				1
#define A_PUSHEW				2
#define A_PUSHABLE			3
#define A_ITEM 				4
#define A_UPDATE 				8
#define A_HURTS 				16
#define A_UNDER 				32
#define A_ENTRANCE 			64
#define A_EXPLODE 			128
#define A_BLOW_UP 			256
#define A_SHOOTABLE 			512
#define A_ENEMY 				1024
#define A_AFFECT_IF_STOOD 	2048
#define A_SPEC_SHOT 			4096
#define A_SPEC_PUSH 			8192
#define A_SPEC_BOMB 			16384
#define A_SPEC_STOOD 		32768

//VIDEO_SEG is the segment of the first video page. VIDEO_ADD is what to
//add per page.
#define VIDEO_SEG 0x0b800
#define VIDEO_ADD 0x0100

//VIDEO_NUM2SEG gives video segment of 0 or 1. Add a ! before your number
//for the segment of the opposite number.
#define VIDEO_NUM2SEG(x)	((x)?VIDEO_SEG+VIDEO_ADD:VIDEO_SEG)

//Actual number is one less due to reservation of ID #TEMP_STORAGE.
#define NUM_ROBOTS		200
#define NUM_SCROLLS		50
#define NUM_SENSORS		20
#define NUM_COUNTERS		1020 // This is a decent # for now. Spid
#define NUM_STATUS_CNTRS	6
//The first n counters are reserved for internal use (GEMS, etc)
#define RESERVED_COUNTERS	9
//ID number for storage of the only copy of the current robot/scroll/sensor
#define TEMP_STORAGE		0
//ID number for storage of the global robot
#define GLOBAL_ROBOT		NUM_ROBOTS

//Number of musical devices
#define NUM_DEVICES		7
//Device code for no music
#define NO_MUSIC			0

//Max/min sizes
#define MAX_ARRAY_X		max_bxsiz
#define MAX_ARRAY_Y		max_bysiz
#define MAX_VIEWPORT_X	80
#define MAX_VIEWPORT_Y	25
#define MIN_ARRAY_X		1
#define MIN_ARRAY_Y		1
#define MIN_VIEWPORT_X	1
#define MIN_VIEWPORT_Y	1

#endif