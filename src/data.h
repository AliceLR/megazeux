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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __DATA_H
#define __DATA_H

#include "compat.h"

__M_BEGIN_DECLS

#include "const.h"
#include <stdio.h>
#include <stdlib.h>

/* This first section is for idput.cpp */
extern unsigned char id_chars[455];

#define thin_line            128
#define thick_line           144
#define ice_anim             160
#define lava_anim            164
#define low_ammo             167
#define hi_ammo              168
#define lit_bomb             169
#define energizer_glow       176
#define explosion_colors     184
#define horiz_door           188
#define vert_door            189
#define cw_anim              190
#define ccw_anim             194
#define open_door            198
#define transport_anims      230
#define trans_north          230
#define trans_south          234
#define trans_east           238
#define trans_west           242
#define trans_all            246
#define thick_arrow          250
#define thin_arrow           254
#define horiz_lazer          258
#define vert_lazer           262
#define fire_anim            266
#define fire_colors          272
#define life_anim            278
#define life_colors          282
#define ricochet_panels      286
#define mine_anim            288
#define shooting_fire_anim   290
#define shooting_fire_colors 292
#define seeker_anim          294
#define seeker_colors        298
#define whirlpool_glow       302
#define bullet_char          306
#define player_char          318
#define player_color         322

/* This second section is also for idput.cpp */
extern unsigned char id_dmg[128];
extern unsigned char def_id_chars[455];
//extern unsigned char *player_color;
//extern unsigned char *player_char;
extern unsigned char bullet_color[3];
extern unsigned char missile_color;
//extern unsigned char *bullet_char;
extern char curr_file[MAX_PATH];
extern char curr_sav[MAX_PATH];
extern char help_file[MAX_PATH];
extern char megazeux_dir[MAX_PATH];
extern char current_dir[MAX_PATH];
extern char config_dir[MAX_PATH];

extern unsigned char scroll_color;        // Current scroll color
extern unsigned char cheats_active;       // (additive flag)
extern unsigned char current_help_sec0;   // Use for context-sens.help
extern int was_zapped;

extern char saved_pl_color;

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
   (id != BULLET_GUN) && (id != SPINNING_GUN))            \

#define is_storageless(id)                                \
  (id < SENSOR)                                           \

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

static const int CAN_PUSH      = 0x001;
static const int CAN_TRANSPORT = 0x002;
static const int CAN_LAVAWALK  = 0x004;
static const int CAN_FIREWALK  = 0x008;
static const int CAN_WATERWALK = 0x010;
static const int MUST_WEB      = 0x020;
static const int MUST_THICKWEB = 0x040;
static const int REACT_PLAYER  = 0x080;
static const int MUST_WATER    = 0x100;
static const int MUST_LAVAGOOP = 0x200;
static const int CAN_GOOPWALK  = 0x400;
static const int SPITFIRE      = 0x800;

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
  End                       = 0
  Die = 1
  Wait = 2
  Cycle = 3
  Go = 4
  Walk = 5
  Become = 6
  Char = 7
  Color = 8
  Gotoxy = 9
  Set = 10
  Inc = 11
  Dec = 12
  Set_s = 13
  Inc_s = 14
  Dec_s = 15
  If = 16
  If_s = 17
  If_cond = 18
  If_not_cond = 19
  If_any = 20
  If_no = 21
  If_thing_dir = 22
  If_not_thing_dir = 23
  If_thing_at = 24
  If_at = 25
  If_player_dir = 26
  Double = 27
  Half = 28
  Goto = 29
  Send = 30
  Explode = 31
  Put = 32
  Give = 33
  Take = 34
  Take_else = 35
  Endgame = 36
  Endlife = 37
  Mod = 38
  Sam = 39
  Volume = 40
  End_mod = 41
  End_sam = 42
  Play = 43
  End_play = 44
  Wait_play = 45
  Wait_play = 46
  __blank_line = 47
  sfx = 48
  Play_sfx = 49
  Open = 50
  Lockself = 51
  Unlockself = 52
  Send = 53
  Zap = 54
  Restore = 55
  Lockplayer = 56
  Unlockplayer = 57
  Lockplayer_ns = 58
  Lockplayer_ew = 59
  Lockplayer_attack = 60
  Move_player = 61
  Move_player_else = 62
  Put_player = 63
  If_player = 64
  If_player_not = 65
  If_player_at = 66
  Put_player_dir = 67
  Try = 68
  Rotatecw = 69
  Rotateccw = 70
  Switch = 71
  Shoot = 72
  Laybomb = 73
  Laybomb_high = 74
  Shootmissile = 75
  Shootseeker = 76
  Spitfire = 77
  Lazerwall = 78
  Put_at = 79
  Die_item                    = 80
  Send_at                     = 81
  Copyrobot                   = 82
  Copyrobot_at                = 83
  Copyrobot_dir               = 84
  Duplicate_self              = 85
  Duplicate_self_at           = 86
  Bulletn = 87
  Bullets = 88
  Bullete = 89
  Bulletw = 90
  Givekey = 91
  Givekey_else = 92
  Takekey = 93
  Takekey_else = 94
  Inc_random = 95
  Dec_random = 96
  Set_random = 97
  Trade = 98
  Send_dir_player             = 99
  Put_dir_player              = 100
  _go_string                  = 101
  _row_message                = 102
  _box_message                = 103
  _box_message_label          = 104
  _box_message_label_counter  = 105
  _label                      = 106
  _comment                    = 107
  _zapped_label               = 108
  teleport_player             = 109
  Scrollview                  = 110
  Input_string                = 111
  If_string                   = 112
  If_string_not               = 113
  If_string_matches           = 114
  Player_char [ch]            = 115
  _color_message              = 116
  _center_message             = 117
  Move_all                    = 118
  Copy                        = 119
  Set_edge_color              = 120
  Board_exit                  = 121
  Board_exit_none             = 122
  Char_edit                   = 123
  Become_pushable             = 124
  Become_nonpushable          = 125
  Blind                       = 126
  Firewalker                  = 127
  Freezetime                  = 128
  Slowtime                    = 129
  Wind                        = 130
  Avalance                    = 131
  Copy_dir                    = 132
  Become_lavawalker           = 133
  Become_nonlavawalker        = 134
  Change                      = 135
  Playercolor                 = 136
  Bulletcolor                 = 137
  Missilecolor                = 138
  Message_row                 = 139
  Rel_self = 140
  Rel_player = 141
  Rel_counters = 142
  Change_char_id = 143
  Jump_mod_order = 144
  Ask = 145
  Fillhealth = 146
  Change_thick_arrow_char = 147
  Change_thin_arrow_char = 148
  Set_maxhealth = 149
  Save_player_position = 150
  Restore_player_position = 151
  Exchange_player_position = 152
  Set_mesg_column = 153
  Center_mesg = 154
  Clear_mesg = 155
  Resetview = 156
  Sam = 157
  Volume = 158
  Scrollbase_color = 159
  Scrollcorner_color = 160
  Scrolltitle_color = 161
  Scrollpointer_color = 162
  Scrollarrow_color = 163
  Viewport = 164
  Viewport_size = 165
  Set_mesg_column = 166
  Message_row = 167
  Save_player_position = 168
  Restore_player_position = 169
  Exchange_player_position = 170
  Restore_player_position = 171
  Exchange_player_position = 172
  Player_bulletn = 173
  Player_bullets = 174
  Player_bullete = 175
  Player_bulletw = 176
  Neutral_bulletn = 177
  Neutral_bullets = 178
  Neutral_bullete = 179
  Neutral_bulletw = 180
  Enemy_bulletn = 181
  Enemy_bullets = 182
  Enemy_bullete = 183
  Enemy_bulletw = 184
  Player_bulletcolor = 185
  Neutral_bulletcolor = 186
  Enemy_bulletcolor = 187
  Rel_self_first = 193
  Rel_self_last = 194
  Rel_player_first = 195
  Rel_player_last = 196
  Rel_counters_first = 197
  Rel_counters_last = 198
  Mod_fade_out = 199
  Mod_fade_in = 200
  Copy_block = 201
  Clip_input = 202
  Push = 203
  Scroll_char = 204
  Flip_char = 205
  Copy_char = 206
  Change_sfx = 210
  Color_intensity = 211
  Color_intensity_single = 212
  Color_fade_out = 213
  Color_fade_in = 214
  Set_color = 215
  Load_char_set = 216
  Multiply = 217
  Divide = 218
  Modulo = 219
  Player_char = 220
  Load_palette = 222
  Mod_fad = 224
  Scrollview = 225
  Swap_world = 226
  If_alignedrobo = 227
  Lockscroll = 229
  Unlockscroll = 230
  If_first_string = 231
  Persistent_go = 232
  Wait_mod_fade = 233
  Enable_saving = 235
  Disable_saving = 236
  Enable_sensoronly_saving = 237
  Status_counter = 238
  Overlay_on = 239
  Overlay_static = 240
  Overlay_transparent = 241
  Put_overlay = 242
  Copy_overlay_block = 243
  Change_overlay_char = 244
  Change_overlay = 245
  Write_overlay = 246
  __message_box_temporary = 249
  Loop_start = 251
  Loop = 252
  Abort_loop = 253
  Disable_mesg_edge = 254
  Enable_mesg_edge = 255
} mzx_robotic_commands;

*/

__M_END_DECLS

#endif // __DATA_H
