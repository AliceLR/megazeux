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

/* Declarations for IDPUT.ASM */

#ifndef __IDPUT_H
#define __IDPUT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "world_struct.h"
#include "board_struct.h"
#include "data.h"

void id_put(Board *src_board, unsigned char x_pos, unsigned char y_pos,
  int array_x, int array_y, int ovr_x, int ovr_y);
void draw_game_window(Board *src_board, int array_x, int array_y);

unsigned char get_id_char(Board *src_board, int id_offset);
unsigned char get_id_color(Board *src_board, int id_offset);

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

extern unsigned char id_chars[455];
extern unsigned char id_dmg[128];
extern unsigned char bullet_color[3];
extern unsigned char missile_color;

#ifdef CONFIG_EDITOR
void draw_edit_window(Board *src_board, int array_x, int array_y,
 int window_height);
extern unsigned char def_id_chars[455];
#endif

__M_END_DECLS

#endif // __IDPUT_H
