/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 B.D.A
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// Translation of data.asm to C

#include "data.h"
#include "const.h"

// Some globals that still lurk.. should be moved to other places.

// Current MZX file information
char curr_file[MAX_PATH] = "CAVERNS.MZX";
char curr_sav[MAX_PATH] = "SAVED.SAV";
char help_file[MAX_PATH];                       // Drive + Path + Filename
char megazeux_dir[MAX_PATH];                    // Directory started in
char current_dir[MAX_PATH];                     // Current directory
char config_dir[MAX_PATH];                      // Config file path
char quicksave_file[512];                       // Current quicksave filename

// Some global world values that haven't been removed yet.

unsigned char scroll_color = 15;                // Current scroll color
unsigned char current_help_sec = 0;             // Use for context-sens.help

// Array of flags for things
unsigned int flags[] =
{
  A_UNDER,                                                    // 0x00 Space
  0,                                                          // 0x01 Normal
  0,                                                          // 0x02 Solid
  0,                                                          // 0x03 Tree
  0,                                                          // 0x04 Line
  0,                                                          // 0x05 Customblock
  A_SHOOTABLE | A_BLOW_UP,                                    // 0x06 Breakaway
  A_SHOOTABLE | A_BLOW_UP,                                    // 0x07 Customblock
  A_PUSHABLE | A_BLOW_UP,                                     // 0x08 Boulder
  A_PUSHABLE | A_BLOW_UP,                                     // 0x09 Crate
  A_PUSHABLE | A_BLOW_UP,                                     // 0x0A Custompush
  A_PUSHABLE,                                                 // 0x0B Box
  A_PUSHABLE,                                                 // 0x0C Customsolidpush
  A_UNDER,                                                    // 0x0D Fake
  A_UNDER,                                                    // 0x0E Carpet
  A_UNDER,                                                    // 0x0F Floor
  A_UNDER,                                                    // 0x10 Tiles
  A_UNDER,                                                    // 0x11 Customfloor
  A_UNDER | A_BLOW_UP,                                        // 0x12 Web
  A_UNDER | A_BLOW_UP,                                        // 0x13 Thickweb
  A_UNDER,                                                    // 0x14 Stillwater
  A_UNDER | A_AFFECT_IF_STOOD,                                // 0x15 NWater
  A_UNDER | A_AFFECT_IF_STOOD,                                // 0x16 SWater
  A_UNDER | A_AFFECT_IF_STOOD,                                // 0x17 EWater
  A_UNDER | A_AFFECT_IF_STOOD,                                // 0x18 WWater
  A_UNDER | A_AFFECT_IF_STOOD | A_UPDATE,                     // 0x19 Ice
  A_UNDER | A_AFFECT_IF_STOOD | A_UPDATE,                     // 0x1A Lava
  A_ITEM | A_BLOW_UP,                                         // 0x1B Chest
  A_ITEM | A_PUSHABLE | A_BLOW_UP | A_SHOOTABLE,              // 0x1C Gem
  A_ITEM | A_PUSHABLE | A_BLOW_UP | A_SHOOTABLE,              // 0x1D Magicgem
  A_ITEM | A_PUSHABLE,                                        // 0x1E Health
  A_ITEM | A_PUSHABLE,                                        // 0x1F Ring
  A_ITEM | A_PUSHABLE,                                        // 0x20 Potion
  A_ITEM | A_PUSHABLE | A_UPDATE,                             // 0x21 Energizer
  A_UNDER | A_ITEM,                                           // 0x22 Goop
  A_ITEM | A_PUSHABLE,                                        // 0x23 Ammo
  A_ITEM | A_PUSHABLE | A_EXPLODE,                            // 0x24 Bomb
  A_PUSHABLE | A_EXPLODE | A_UPDATE,                          // 0x25 LitBomb
  A_HURTS | A_UPDATE,                                         // 0x26 Explosion
  A_ITEM | A_PUSHABLE,                                        // 0x27 Key
  A_ITEM,                                                     // 0x28 Lock
  A_ITEM,                                                     // 0x29 Door
  A_UPDATE,                                                   // 0x2A Opening/closing door
  A_ENTRANCE | A_UNDER,                                       // 0x2B Stairs
  A_ENTRANCE | A_UNDER,                                       // 0x2C Cave
  A_UPDATE,                                                   // 0x2D CW
  A_UPDATE,                                                   // 0x2E CCW
  A_ITEM,                                                     // 0x2F Gate
  A_UPDATE | A_UNDER,                                         // 0x30 OpenGate
  A_SPEC_PUSH | A_ITEM | A_UPDATE,                            // 0x31 Transport
  A_ITEM | A_PUSHABLE,                                        // 0x32 Coin
  A_UPDATE,                                                   // 0x33 MovingWall N
  A_UPDATE,                                                   // 0x34 MovingWall S
  A_UPDATE,                                                   // 0x35 MovingWall E
  A_UPDATE,                                                   // 0x36 MovingWall W
  A_ITEM | A_PUSHABLE | A_BLOW_UP,                            // 0x37 Pouch
  A_UPDATE,                                                   // 0x38 Pusher
  A_PUSHNS,                                                   // 0x39 SliderNS
  A_PUSHEW,                                                   // 0x3A SliderEW
  A_UPDATE | A_HURTS,                                         // 0x3B Lazer
  A_UPDATE,                                                   // 0x3C LazerWallShooter
  A_UPDATE | A_BLOW_UP | A_SHOOTABLE | A_ITEM |  A_ENEMY,     // 0x3D Bullet
  A_UPDATE | A_HURTS | A_EXPLODE,                             // 0x3E Missile
  A_UPDATE | A_UNDER | A_AFFECT_IF_STOOD,                     // 0x3F Fire
  0,                                                          // 0x40
  A_ITEM,                                                     // 0x41 Forest
  A_PUSHABLE | A_ITEM | A_UPDATE,                             // 0x42 Life
  A_UPDATE | A_UNDER | A_ENTRANCE,                            // 0x43 Whirlpool 1
  A_UPDATE | A_UNDER | A_ENTRANCE,                            // 0x44 Whirlpool 2
  A_UPDATE | A_UNDER | A_ENTRANCE,                            // 0x45 Whirlpool 3
  A_UPDATE | A_UNDER | A_ENTRANCE,                            // 0x46 Whirlpool 4
  A_ITEM,                                                     // 0x47 Invisible
  A_SPEC_SHOT,                                                // 0x48 RicochetPanel
  A_SPEC_SHOT,                                                // 0x49 Ricochet
  A_ITEM | A_SPEC_SHOT | A_SPEC_BOMB | A_UPDATE,              // 0x4A Mine
  A_HURTS,                                                    // 0x4B Spike
  A_HURTS,                                                    // 0x4C Customhurt
  0,                                                          // 0x4D Text
  A_UPDATE | A_ENEMY | A_BLOW_UP,                             // 0x4E ShootingFire
  A_UPDATE | A_ENEMY | A_BLOW_UP | A_PUSHABLE,                // 0x4F Seeker
  A_UPDATE | A_PUSHABLE | A_ENEMY | A_SHOOTABLE | A_BLOW_UP,  // 0x50 Snake
  A_UPDATE | A_EXPLODE | A_ITEM | A_SPEC_SHOT | A_PUSHABLE,   // 0x51 Eye
  A_UPDATE | A_BLOW_UP | A_PUSHABLE | A_ITEM | A_SHOOTABLE,   // 0x52 Thief
  A_UPDATE | A_ITEM | A_SPEC_BOMB | A_SPEC_SHOT,              // 0x53 SlimeBlob
  A_UPDATE | A_ENEMY | A_BLOW_UP | A_PUSHABLE | A_SPEC_SHOT,  // 0x54 Runner
  A_UPDATE | A_ITEM | A_PUSHABLE | A_SPEC_BOMB | A_SPEC_SHOT, // 0x55 Ghost
  A_UPDATE | A_SPEC_SHOT | A_SPEC_BOMB | A_ITEM,              // 0x56 Dragon
  A_UPDATE | A_ITEM | A_BLOW_UP | A_SPEC_SHOT | A_PUSHABLE,   // 0x57 Fish
  A_UPDATE | A_BLOW_UP | A_SHOOTABLE | A_ENEMY,               // 0x58 Shark
  A_UPDATE | A_ENEMY | A_BLOW_UP | A_SPEC_SHOT | A_PUSHABLE,  // 0x59 Spider
  A_UPDATE | A_ENEMY | A_SHOOTABLE | A_PUSHABLE | A_BLOW_UP,  // 0x5A Goblin (90)
  A_UPDATE | A_PUSHABLE | A_SHOOTABLE | A_ENEMY | A_BLOW_UP,  // 0x5B Tiger
  A_UPDATE,                                                   // 0x5C BulletGun
  A_UPDATE,                                                   // 0x5D SpinningGun
  A_UPDATE | A_ENEMY | A_SPEC_SHOT | A_PUSHABLE | A_BLOW_UP,  // 0x5E Bear
  A_UPDATE | A_BLOW_UP | A_ENEMY | A_SHOOTABLE | A_PUSHABLE,  // 0x5F BearCub
  0,                                                          // 0x60 '?'
  A_UPDATE,                                                   // 0x61 MissileGun
  0,                                                          // 0x62 98
  0,                                                          // 0x63 99
  0,                                                          // 0x64 100
  0,                                                          // 0x65 101
  0,                                                          // 0x66 102
  0,                                                          // 0x67 103
  0,                                                          // 0x68 104
  0,                                                          // 0x69 105
  0,                                                          // 0x6A 106
  0,                                                          // 0x6B 107
  0,                                                          // 0x6C 108
  0,                                                          // 0x6D 109
  0,                                                          // 0x6E 110
  0,                                                          // 0x6F 111
  0,                                                          // 0x70 112
  0,                                                          // 0x71 113
  0,                                                          // 0x72 114
  0,                                                          // 0x73 115
  0,                                                          // 0x74 116
  0,                                                          // 0x75 117
  0,                                                          // 0x76 118
  0,                                                          // 0x77 119
  0,                                                          // 0x78 120
  0,                                                          // 0x79 x0C 121
  A_SPEC_STOOD | A_SPEC_PUSH,                                 // 0x7A x0D Sensor
  A_ITEM | A_UPDATE | A_SPEC_SHOT | A_SPEC_BOMB | A_SPEC_PUSH,// 0x7B x0E Robot (pushable)
  A_ITEM | A_UPDATE | A_SPEC_SHOT | A_SPEC_BOMB,              // 0x7C x0F Robot
  A_ITEM,                                                     // 0x7D x00 Sign
  A_ITEM | A_PUSHABLE,                                        // 0x7E x01 Scroll
  A_SPEC_SHOT | A_SPEC_PUSH | A_SPEC_BOMB                     // 0x7F x02 Player
};

//Names for all things
char *thing_names[128] =
{
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
  "Sprite",
  "Sprite_colliding",
  "Image_file",
  "[unknown]",
  "[unknown]", "[unknown]", "[unknown]", "[unknown]",
  "[unknown]", "[unknown]", "[unknown]", "[unknown]",
  "[unknown]", "[unknown]", "[unknown]", "[unknown]",
  "[unknown]", "[unknown]", "[unknown]", "[unknown]",
  "[unknown]", "[unknown]", "[unknown]", "[unknown]",
  "Sensor",
  "PushableRobot",
  "Robot",
  "Sign",
  "Scroll",
  "Player"
};


