/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

/* Declarations for DATA.ASM (most global data) */

#ifndef __DATA_H
#define __DATA_H

#include "const.h"
#include <stdio.h>
#include <stdlib.h>


/* This first section is for idput.cpp */
extern unsigned char id_chars[455];
extern unsigned char id_dmg[128];
extern unsigned char def_id_chars[455];
extern unsigned char *player_color;
extern unsigned char *player_char;
extern unsigned char bullet_color[3];
extern unsigned char missile_color;
extern unsigned char *bullet_char;
extern char curr_file[512];
extern char curr_sav[512];
extern char help_file[MAX_PATH];
extern char megazeux_dir[MAX_PATH];
extern char current_dir[MAX_PATH];

extern unsigned char scroll_color;        // Current scroll color
extern unsigned char cheats_active;       // (additive flag)
extern unsigned char current_help_sec0;   // Use for context-sens.help
extern int was_zapped;

extern char saved_pl_color;

extern char *update_done;
extern int update_done_size;

extern char          *thing_names[128];
extern unsigned int  flags[128];

typedef enum
{
  SPACE           = 0,
  NORMAL          = 1,
  SOLID           = 2,
  TREE            = 3,
  LINE            = 4,
  CUSTOM_BLOCK    = 5,
  BREAKAWAY       = 6,
  CUSTOM_BREAK    = 7,
  BOULDER         = 8,
  CRATE           = 9,
  CUSTOM_PUSH     = 10,
  BOX             = 11,
  CUSTOM_BOX      = 12,
  FAKE            = 13,
  CARPET          = 14,
  FLOOR           = 15,
  TILES           = 16,
  CUSTOM_FLOOR    = 17,
  WEB             = 18,
  THICK_WEB       = 19,
  STILL_WATER     = 20,
  N_WATER         = 21,
  S_WATER         = 22,
  E_WATER         = 23,
  W_WATER         = 24,
  ICE             = 25,
  LAVA            = 26,
  CHEST           = 27,
  GEM             = 28,
  MAGIC_GEM       = 29,
  HEALTH          = 30,
  RING            = 31,
  POTION          = 32,
  ENERGIZER       = 33,
  GOOP            = 34,
  AMMO            = 35,
  BOMB            = 36,
  LIT_BOMB        = 37,
  EXPLOSION       = 38,
  KEY             = 39,
  LOCK            = 40,
  DOOR            = 41,
  OPEN_DOOR       = 42,
  STAIRS          = 43,
  CAVE            = 44,
  CW_ROTATE       = 45,
  CCW_ROTATE      = 46,
  GATE            = 47,
  OPEN_GATE       = 48,
  TRANSPORT       = 49,
  COIN            = 50,
  N_MOVING_WALL   = 51,
  S_MOVING_WALL   = 52,
  E_MOVING_WALL   = 53,
  W_MOVING_WALL   = 54,
  POUCH           = 55,
  PUSHER          = 56,
  SLIDER_NS       = 57,
  SLIDER_EW       = 58,
  LAZER           = 59,
  LAZER_GUN       = 60,
  BULLET          = 61,
  MISSILE         = 62,
  FIRE            = 63,
  FOREST          = 65,
  LIFE            = 66,
  WHIRLPOOL_1     = 67,
  WHIRLPOOL_2     = 68,
  WHIRLPOOL_3     = 69,
  WHIRLPOOL_4     = 70,
  INVIS_WALL      = 71,
  RICOCHET_PANEL  = 72,
  RICOCHET        = 73,
  MINE            = 74,
  SPIKE           = 75,
  CUSTOM_HURT     = 76,
  TEXT            = 77,
  SHOOTING_FIRE   = 78,
  SEEKER          = 79,
  SNAKE           = 80,
  EYE             = 81,
  THIEF           = 82,
  SLIMEBLOB       = 83,
  RUNNER          = 84,
  GHOST           = 85,
  DRAGON          = 86,
  FISH            = 87,
  SHARK           = 88,
  SPIDER          = 89,
  GOBLIN          = 90,
  SPITTING_TIGER  = 91,
  BULLET_GUN      = 92,
  SPINNING_GUN    = 93,
  BEAR            = 94,
  BEAR_CUB        = 95,
  MISSILE_GUN     = 97,
  SPRITE          = 98,
  SPR_COLLISION   = 99,
  IMAGE_FILE      = 100,
  SENSOR          = 122,
  ROBOT_PUSHABLE  = 123,
  ROBOT           = 124,
  SIGN            = 125,
  SCROLL          = 126,
  PLAYER          = 127,
  NO_ID           = 255
} mzx_thing;

#define is_fake(id)                                       \
  ((id == SPACE) || ((id >= FAKE) && (id <= THICK_WEB)))  \

#define is_robot(id)                                      \
  ((id == ROBOT) || (id == ROBOT_PUSHABLE))               \

#define is_signscroll(id)                                 \
  ((id == SIGN) || (id == SCROLL))                        \

#define is_water(id)                                      \
  ((id >= STILL_WATER) && (id <= W_WATER))                \

#define is_whirlpool(id)                                  \
  ((id >= WHIRLPOOL_1) && (id <= WHIRLPOOL_2))            \

#define is_enemy(id)                                      \
  ((id >= SNAKE) && (id <= BEAR_CUB) &&                   \
   (id != BULLET_GUN) && (id != SPINNING_GUN))             \

typedef enum
{
  IDLE    = 0,
  NORTH   = 1,
  SOUTH   = 2,
  EAST    = 3,
  WEST    = 4,
  RANDNS  = 5,
  RANDEW  = 6,
  RANDNE  = 7,
  RANDNB  = 8,
  SEEK    = 9,
  RANDANY = 10,
  BENEATH = 11,
  ANYDIR  = 12,
  FLOW    = 13,
  NODIR   = 14,
  RANDB   = 15,
  RANDP   = 16,
  CW      = 32,
  OPP     = 64,
  RANDNOT = 128
} mzx_dir;

#define is_cardinal_dir(d)                                \
  ((d >= NORTH) && (d <= WEST))                           \

#define dir_to_int(d)                                     \
  ((int)d - 1)                                            \

#define int_to_dir(d)                                     \
  ((mzx_dir)(d + 1))                                      \

const int CAN_PUSH      = 0x001;
const int CAN_TRANSPORT = 0x002;
const int CAN_LAVAWALK  = 0x004;
const int CAN_FIREWALK  = 0x008;
const int CAN_WATERWALK = 0x010;
const int MUST_WEB      = 0x020;
const int MUST_THICKWEB = 0x040;
const int REACT_PLAYER  = 0x080;
const int MUST_WATER    = 0x100;
const int MUST_LAVAGOOP = 0x200;
const int CAN_GOOPWALK  = 0x400;
const int SPITFIRE      = 0x800;

typedef enum
{
  NO_HIT      = 0,
  HIT         = 1,
  HIT_PLAYER  = 2,
  HIT_EDGE    = 3
} move_status;

typedef enum
{
  EQUAL                 = 0,
  LESS_THAN             = 1,
  GREATER_THAN          = 2,
  GREATER_THAN_OR_EQUAL = 3,
  LESS_THAN_OR_EQUAL    = 4,
  NOT_EQUAL             = 5,
} mzx_equality;

typedef enum
{
  WALKING               = 0,
  SWIMMING              = 1,
  FIRE_WALKING          = 2,
  TOUCHING              = 3,
  BLOCKED               = 4,
  ALIGNED               = 5,
  ALIGNED_NS            = 6,
  ALIGNED_EW            = 7,
  LASTSHOT              = 8,
  LASTTOUCH             = 9,
  RIGHTPRESSED          = 10,
  LEFTPRESSED           = 11,
  UPPRESSED             = 12,
  DOWNPRESSED           = 13,
  SPACEPRESSED          = 14,
  DELPRESSED            = 15,
  MUSICON               = 16,
  SOUNDON               = 17
} mzx_condition;

typedef enum
{
  ITEM_KEY              = 1,
  ITEM_COINS            = 2,
  ITEM_LIFE             = 3,
  ITEM_AMMO             = 4,
  ITEM_GEMS             = 5,
  ITEM_HEALTH           = 6,
  ITEM_POTION           = 7,
  ITEM_RING             = 8,
  ITEM_LOBOMBS          = 9,
  ITEM_HIBOMBS          = 10,
} mzx_chest_contents;

typedef enum
{
  POTION_DUD            = 0,
  POTION_INVINCO        = 1,
  POTION_BLAST          = 2,
  POTION_SMALL_HEALTH   = 3,
  POTION_BIG_HEALTH     = 4,
  POTION_POISON         = 5,
  POTION_BLIND          = 6,
  POTION_KILL           = 7,
  POTION_FIREWALK       = 8,
  POTION_DETONATE       = 9,
  POTION_BANISH         = 10,
  POTION_SUMMON         = 11,
  POTION_AVALANCHE      = 12,
  POTION_FREEZE         = 13,
  POTION_WIND           = 14,
  POTION_SLOW           = 15
} mzx_potion;

typedef enum
{
  TARGET_NONE           = 0,
  TARGET_POSITION       = 1,
  TARGET_ENTRANCE       = 2,
  TARGET_TELEPORT       = 3
} mzx_board_target;

/*

typedef enum
{
0    x  End = 0
1    x  Die = 1
2    x  Wait = 2
3       Cycle = 3
4   x g Go = 4
5    g  Walk = 5
6   x g Become = 6
7    g  Char = 7
8    g  Color = 8
9   x g Gotoxy = 9
10      Set = 10
11      Inc = 11
12      Dec = 12
13  ODT Set_s = 13
14  ODT Inc_s = 14
15  ODT Dec_s = 15
16      If = 16
17  ODT If_s = 17
18   g  If_cond = 18
19   g  If_not_cond = 19
20      If_any = 20
21      If_no = 21
22      If_thing_dir = 22
23      If_not_thing_dir = 23
24      If_thing_at = 24
25   g  If_at = 25
26      If_player_dir = 26
27      Double = 27
28      Half = 28
29      Goto = 29
30      Send = 30
31  x g Explode = 31
32   g  Put = 32
33      Give = 33
34      Take = 34
35      Take_else = 35
36   *  Endgame = 36
37   *  Endlife = 37
38      Mod = 38
39      Sam = 39
40      Volume = 40
41      End_mod = 41
42      End_sam = 42
43      Play = 43
44      End_play = 44
45      Wait_play = 45
46      Wait_play = 46
47      (blank line)
48      Sfx [##]
49      Play sfx [str]
50   g  Open [dir]
51   *  Lockself
52   *  Unlockself
53   g  Send [dir] [label]
54      Zap [str] [##]
55      Restore [str] [##]
56   *  Lockplayer
57   *  Unlockplayer
58   *  Lockplayer ns
59   *  Lockplayer ew
60   *  Lockplayer attack
61   x  Move player [dir]
62   x  Move player [dir] [str]
63      Put player [-#] [-#]
64  ODT If player [dir] [str] (Becomes If [cond=touching] [dir] [str])
65  ODT If player not [dir] [str] (Becomes If not [cond=touching] [dir] [str])
66      If player [-#] [-#] [str]
67   g  Put player [dir]
68  x g Try [dir] [str]
69  * g Rotatecw
70  * g Rotateccw
71   g  Switch [dir] [dir]
72   g  Shoot [dir]
73  * g Laybomb [dir]
74  * g Laybomb high [dir]
75  * g Shootmissile [dir]
76  * g Shootseeker [dir]
77  * g Spitfire [dir]
78  * g Lazerwall [dir] [##]
79      Put [color/thing/param] [-#] [-#]
80  x g Die item
81      Send [-#] [-#] [str]
82  x * Copyrobot [str]
83  x * Copyrobot [-#] [-#]
84  x*g Copyrobot [dir]
85   g  Duplicate self [dir]
86      Duplicate self [-#] [-#]
87      Bulletn [ch] (In MZX 2.0, changes ALL bullet n pics)
88      Bullets [ch] (In MZX 2.0, changes ALL bullet s pics)
89      Bullete [ch] (In MZX 2.0, changes ALL bullet e pics)
90      Bulletw [ch] (In MZX 2.0, changes ALL bullet w pics)
91   *  Givekey [col]
92   *  Givekey [col] [str]
93   *  Takekey [col]
94   *  Takekey [col] [str]
95      Inc [str] random [##] [##]
96      Dec [str] random [##] [##]
97      Set [str] random [##] [##]
98      Trade [##] [item] [##] [item] [str]
99      Send [dir] player [str]
100     Put [color/thing/param] [dir] player
101  x  /[str]
102     *[str]
103     [[str]
104     ?[str];[str]
105     ?[str];[str];[str]
106     :[str]
107     .[str]
108     |[str]
109  x  Teleport player [str] [-#] [-#]
110  *  Scrollview [dir] [##]
111     Input string [str]
112     If string [str] [str]
113     If string not [str] [str]
114     If string matches [str] [str]
115     Player char [ch] (In MZX 2.0, sets the pic for all four directions)
116     %[str]
117     &[str]
118  x  Move all [color/thing/param] [dir] (different)
119  x  Copy [-#] [-#] [-#] [-#]
120     Set edge color [col]
121     Board [dir] [str]
122     Board [dir] none
123     Char edit [ch] [##] [##] [##] [##] [##] [##] [##] [##] [##] [##] [##] [##] [##] [##]
124  g  Become pushable
125  g  Become nonpushable
126     Blind [##]
127     Firewalker [##]
128  *  Freezetime [##]
129  *  Slowtime [##]
130     Wind [##]
131     Avalance
132 x g Copy [dir] [dir]
133  g  Become lavawalker
134  g  Become nonlavawalker
135     Change [color/thing/param] [color/thing/param] (Diff. from ver 1.0?)
136  *  Playercolor [col]
137  *  Bulletcolor [col] (In MZX 2.0, changes all bullet colors)
138  *  Missilecolor [col]
139     Message row [##]
140 Pre Rel self (In MZX 2.0, unpredictable/useless for use w/global robot)
141 Pre Rel player
142 Pre Rel counters
143     Change char id [##] [ch] (Not param checked or doc'd until MZX 2.0)
144     Jump mod order [##]
145     Ask [str]
146  *  Fillhealth
147     Change thick arrow char [dir] [ch]
148     Change thin arrow char [dir] [ch]
149  *  Set maxhealth [##]
150     Save player position (In MZX 2.0, saves to position 1)
151     Restore player position (In MZX 2.0, uses position 1)
152     Exchange player position (In MZX 2.0, uses position 1)
153     Set mesg column [##]
154     Center mesg
155     Clear mesg
156  *  Resetview
157     Sam [##] [##]
158 ODT Volume [str]
159  *  Scrollbase color [col]
160  *  Scrollcorner color [col] (Diff. from MZX 1.03)
161  *  Scrolltitle color [col]
162  *  Scrollpointer color [col]
163  *  Scrollarrow color [col]
164     Viewport [##] [##] (Not param checked until MZX 2.0)
165     Viewport size [##] [##] (Not param checked until MZX 2.0)
166 ODT Set mesg column [str]
167 ODT Message row [str]
168     Save player position [##]
169     Restore player position [##]
170     Exchange player position [##]
171     Restore player position [##] duplicate self
172     Exchange player position [##] duplicate self
173     Player bulletn [ch]
174     Player bullets [ch]
175     Player bullete [ch]
176     Player bulletw [ch]
177     Neutral bulletn [ch]
178     Neutral bullets [ch]
179     Neutral bullete [ch]
180     Neutral bulletw [ch]
181     Enemy bulletn [ch]
182     Enemy bullets [ch]
183     Enemy bullete [ch]
184     Enemy bulletw [ch]
185  *  Player bulletcolor [col]
186  *  Neutral bulletcolor [col]
187  *  Enemy bulletcolor [col]
188
189
190
191
192
193 Pre Rel self first (Unpredictable/useless for use w/global robot)
194 Pre Rel self last (Unpredictable/useless for use w/global robot)
195 Pre Rel player first
196 Pre Rel player last
197 Pre Rel counters first
198 Pre Rel counters last
199     Mod fade out
200     Mod fade in [str]
201  x  Copy block [-#] [-#] [##] [##] [-#] [-#]
202     Clip input
203  g  Push [dir]
204     Scroll char [ch] [dir]
205     Flip char [ch] [dir]
206     Copy char [ch] [ch]
207
208
209
210     Change sfx [##] [str]
211     Color intensity [##] percent
212     Color intensity [##] [##] percent
213  x  Color fade out
214  x  Color fade in
215     Set color [##] [##] [##] [##]
216     Load char set [str]
217     Multiply [str] [##]
218     Divide [str] [##]
219     Modulo [str] [##]
220     Player char [dir] [ch]
221
222   Load palette [str]
223
224     Mod fade [##] [##] (target/speed)
225  *  Scrollview [-#] [-#]
226  x  Swap world [str]
227  *  If alignedrobot [str] [str]
228
229  *  Lockscroll
230  *  Unlockscroll
231     If first string [str] [str]
232     Persistent go [str] (waits until it can move then moves)
233     Wait mod fade
234
235   Enable saving
236   Disable saving
237  *  Enable sensoronly saving
238     Status counter [##] [str]
239     Overlay on
240     Overlay static
241     Overlay transparent
242     Put [col] [ch] overlay [-#] [-#]
243  x  Copy overlay block [-#] [-#] [##] [##] [-#] [-#]
244
245     Change overlay [col] [ch] [col] [ch]
246     Change overlay [col] [col]
247     Write overlay [col] [str] [-#] [-#]
248
249     INTERNAL USE (temporary use for box messages)
250
251   Loop start
252     Loop [##]
253     Abort loop
254     Disable mesg edge
255     Enable mesg edge

} mzx_robotic_commands;

*/

#endif
