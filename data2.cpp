/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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

/* Further data definition- declaration of struct storage */

#include <_null.h>
#include "data.h"
#include "const.h"
#include "struct.h"

extern FILE *output_file = NULL;
extern FILE *input_file = NULL;
Robot far *robots=NULL;//NUM_ROBOTS in size
Scroll far *scrolls=NULL;//NUM_SCROLLS in size
Counter far *counters=NULL;//NUM_COUNTERS in size
Sensor far *sensors=NULL;//NUM_SENSORS in size

//Music devices
char far *music_devices[NUM_DEVICES+1]={
	"None",
	"Soundblaster 1.xx",
	"Soundblaster 2.xx",
	"Soundblaster PRO",
	"Soundblaster 16",
	"Pro Audio Spectrum 16",
	"Gravis Ultra-Sound (cannot play",
	"digital sound effects)" };
char *music_MSEs[NUM_DEVICES]={
	"",
	"SB1X.MSE",
	"SB2X.MSE",
	"SBPRO.MSE",
	"SB16.MSE",
	"PAS.MSE",
	"GUS.MSE" };
//Mixing rates, Low Med High quality per device
unsigned int mixing_rates[NUM_DEVICES][3]={
	{ 0,0,0 },
	{ 15,30,45 },
	{ 15,30,45 },
	{ 15,30,45 },
	{ 15,30,45 },
	{ 15,30,45 },
	{ 45,45,45 } };
//Sound quality strings
char far *music_quality[3]={
	"Low",
	"Medium",
	"High" };

//Names for all things
char far *thing_names[128]={
	"Space",
	"Normal",
	"Solid",
	"Tree",
	"Line",
	"CustomBlock",
	"Breakaway",
	"CustomBreak",
	"Boulder",
	"Crate",
	"CustomPush",
	"Box",
	"CustomBox",
	"Fake",
	"Carpet",
	"Floor",
	"Tiles",
	"CustomFloor",
	"Web",
	"ThickWeb",
	"StillWater",
	"NWater",
	"SWater",
	"EWater",
	"WWater",
	"Ice",
	"Lava",
	"Chest",
	"Gem",
	"MagicGem",
	"Health",
	"Ring",
	"Potion",
	"Energizer",
	"Goop",
	"Ammo",
	"Bomb",
	"LitBomb",
	"Explosion",
	"Key",
	"Lock",
	"Door",
	"OpenDoor",
	"Stairs",
	"Cave",
	"CWRotate",
	"CCWRotate",
	"Gate",
	"OpenGate",
	"Transport",
	"Coin",
	"NMovingWall",
	"SMovingWall",
	"EMovingWall",
	"WMovingWall",
	"Pouch",
	"Pusher",
	"SliderNS",
	"SliderEW",
	"Lazer",
	"LazerGun",
	"Bullet",
	"Missile",
	"Fire",
	"[unknown]",
	"Forest",
	"Life",
	"Whirlpool",
	"Whirlpool2",
	"Whirlpool3",
	"Whirlpool4",
	"InvisWall",
	"RicochetPanel",
	"Ricochet",
	"Mine",
	"Spike",
	"CustomHurt",
	"Text",
	"ShootingFire",
	"Seeker",
	"Snake",
	"Eye",
	"Thief",
	"Slimeblob",
	"Runner",
	"Ghost",
	"Dragon",
	"Fish",
	"Shark",
	"Spider",
	"Goblin",
	"SpittingTiger",
	"BulletGun",
	"SpinningGun",
	"Bear",
	"BearCub",
	"[unknown]",
	"MissileGun",
	"Sprite","Sprite_colliding","Image_file","[unknown]", //98, 99
	"[unknown]","[unknown]","[unknown]","[unknown]",
	"[unknown]","[unknown]","[unknown]","[unknown]",
	"[unknown]","[unknown]","[unknown]","[unknown]",
	"[unknown]","[unknown]","[unknown]","[unknown]",
	"[unknown]","[unknown]","[unknown]","[unknown]",
	"Sensor",
	"PushableRobot",
	"Robot",
	"Sign",
	"Scroll",
	"Player" };
