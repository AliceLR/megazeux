/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

// sprite.cpp, by Exophase

// Global declarations:

#include <stdlib.h>

#include "data.h"
#include "sprite.h"
#include "graphics.h"
#include "idput.h"
#include "error.h"
#include "game.h"
#include "world.h"

// For the qsort.
static int compare_spr(const void *dest, const void *src)
{
  struct sprite *spr_dest = *((struct sprite **)dest);
  struct sprite *spr_src = *((struct sprite **)src);

  return ((spr_dest->y + spr_dest->col_y) -
   (spr_src->y + spr_src->col_y));
}

static int is_blank(char c)
{
  int cp[4];
  int blank = 0;

  cp[3] = 0;
  ec_read_char(c, (char *)cp);

  blank |= cp[0];
  blank |= cp[1];
  blank |= cp[2];
  blank |= cp[3];

  return !blank;
}

void plot_sprite(struct world *mzx_world, struct sprite *cur_sprite, int color,
 int x, int y)
{
  if(((cur_sprite->width) && (cur_sprite->height)))
  {
    cur_sprite->x = x;
    cur_sprite->y = y;

    if(color == 288)
    {
      cur_sprite->flags |= SPRITE_SRC_COLORS;
    }
    else
    {
      cur_sprite->flags &= ~SPRITE_SRC_COLORS;
      cur_sprite->color = color;
    }

    if(!(cur_sprite->flags & SPRITE_INITIALIZED))
    {
      cur_sprite->flags |= SPRITE_INITIALIZED;
      mzx_world->active_sprites++;
    }
  }
}

void draw_sprites(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int i, i2, i3, i4, i5, i6, start_x, start_y, offset_x, offset_y;
  int skip, skip2, skip3;
  int draw_width, draw_height, ref_x, ref_y, screen_x, screen_y;
  int bwidth, bheight;
  bool use_vlayer;
  struct sprite **sprite_list = mzx_world->sprite_list;
  struct sprite *draw_order[MAX_SPRITES];
  struct sprite *cur_sprite;
  char ch, color, dcolor;
  int viewport_x = src_board->viewport_x;
  int viewport_y = src_board->viewport_y;
  int viewport_width = src_board->viewport_width;
  int viewport_height = src_board->viewport_height;
  int board_width = src_board->board_width;
  int overlay_mode = src_board->overlay_mode;
  char *overlay = src_board->overlay;
  char *src_chars;
  char *src_colors;

  // see if y sort should be done
  if(mzx_world->sprite_y_order)
  {
    // Switched to stdlib qsort. Hooray!
    // Fill it with the active sprites in the beginning
    // and the inactive sprites in the end.
    for(i = 0, i2 = 0, i3 = MAX_SPRITES - 1; i < MAX_SPRITES; i++)
    {
      if((sprite_list[i])->flags & SPRITE_INITIALIZED)
      {
        draw_order[i2] = sprite_list[i];
        i2++;
      }
      else
      {
        draw_order[i3] = sprite_list[i];
        i3--;
      }
    }

    // Now sort it, using qsort.

    qsort(draw_order, i2, sizeof(struct sprite *), compare_spr);
  }

  // draw this on top of the SCREEN window.
  for(i = 0; i < MAX_SPRITES; i++)
  {
    if(mzx_world->sprite_y_order)
      cur_sprite = draw_order[i];
    else
      cur_sprite = sprite_list[i];

    if(cur_sprite->flags & SPRITE_INITIALIZED)
    {
      calculate_xytop(mzx_world, &screen_x, &screen_y);
      ref_x = cur_sprite->ref_x;
      ref_y = cur_sprite->ref_y;
      offset_x = 0;
      offset_y = 0;
      start_x = (cur_sprite->x + viewport_x);
      start_y = (cur_sprite->y + viewport_y);

      if(!(cur_sprite->flags & SPRITE_STATIC))
      {
        start_x -= screen_x;
        start_y -= screen_y;
      }

      draw_width = cur_sprite->width;
      draw_height = cur_sprite->height;

      // clip draw position against viewport

      if(start_x < viewport_x)
      {
        offset_x = viewport_x - start_x;
        draw_width -= offset_x;
        if(draw_width < 1)
          continue;

        start_x = viewport_x;
      }

      if(start_y < viewport_y)
      {
        offset_y = viewport_y - start_y;
        draw_height -= offset_y;
        if(draw_height < 1)
          continue;

        start_y = viewport_y;
      }

      if((start_x > (viewport_x + viewport_width)) ||
       (start_y > (viewport_y + viewport_height)))
      {
        continue;
      }

      if((start_x + draw_width) > (viewport_x + viewport_width))
        draw_width = viewport_x + viewport_width - start_x;

      if((start_y + draw_height) > (viewport_y + viewport_height))
        draw_height = viewport_y + viewport_height - start_y;

      if(cur_sprite->flags & SPRITE_VLAYER)
      {
        use_vlayer = true;
        bwidth = mzx_world->vlayer_width;
        bheight = mzx_world->vlayer_height;
        src_chars = mzx_world->vlayer_chars;
        src_colors = mzx_world->vlayer_colors;
      }
      else
      {
        use_vlayer = false;
        bwidth = board_width;
        bheight = src_board->board_height;
        src_chars = src_board->level_param;
        src_colors = src_board->level_color;
      }

      if(ref_x < 0)
      {
        draw_width += ref_x;
        ref_x = 0;
      }

      if(ref_y < 0)
      {
        draw_height += ref_y;
        ref_y = 0;
      }

      if((ref_x + draw_width) > bwidth)
        draw_width = bwidth - ref_x;

      if((ref_y + draw_height) > bheight)
        draw_height = bheight - ref_y;

      i4 = ((ref_y + offset_y) * bwidth) + ref_x + offset_x;
      i5 = (start_y * 80) + start_x;

      if(overlay_mode == 2)
      {
        i6 = ((start_y - viewport_y) * board_width) + start_x - viewport_x;
      }
      else
      {
        i6 = ((cur_sprite->y + offset_y) * board_width) +
         cur_sprite->x + offset_x;
      }

      skip = bwidth - draw_width;
      skip2 = 80 - draw_width;
      skip3 = board_width - draw_width;

      switch((cur_sprite->flags & 0x0C) >> 2)
      {
        // natural colors, over overlay
        case 3: // 11
        {
          for(i2 = 0; i2 < draw_height; i2++)
          {
            for(i3 = 0; i3 < draw_width; i3++)
            {
              if(use_vlayer)
              {
                color = src_colors[i4];
                ch = src_chars[i4];
              }
              else
              {
                color = get_id_color(src_board, i4);
                ch = get_id_char(src_board, i4);
              }

              if(!(color & 0xF0))
                color = (color & 0x0F) | (get_color_linear(i5) & 0xF0);

              if(ch != 32)
              {
                if(!(cur_sprite->flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
                  draw_char_linear_ext(color, ch, i5, 0, 0);
              }
              i4++;
              i5++;
            }
            i4 += skip;
            i5 += skip2;
          }
          break;
        }

        // natural colors, under overlay
        case 2: // 10
        {
          for(i2 = 0; i2 < draw_height; i2++)
          {
            for(i3 = 0; i3 < draw_width; i3++)
            {
              if(use_vlayer)
              {
                color = src_colors[i4];
                ch = src_chars[i4];
              }
              else
              {
                color = get_id_color(src_board, i4);
                ch = get_id_char(src_board, i4);
              }

              if(!(color & 0xF0))
                color = (color & 0x0F) | (get_color_linear(i5) & 0xF0);

              if((!overlay_mode || overlay_mode == 3 ||
               (overlay[i6] == 32)) && ch != 32)
              {
                if(!(cur_sprite->flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
                  draw_char_linear_ext(color, ch, i5, 0, 0);
              }
              i4++;
              i5++;
              i6++;
            }
            i4 += skip;
            i5 += skip2;
            i6 += skip3;
          }
          break;
        }

        // given colors, over overlay
        case 1: // 01
        {
          color = cur_sprite->color;

          for(i2 = 0; i2 < draw_height; i2++)
          {
            for(i3 = 0; i3 < draw_width; i3++)
            {
              if(use_vlayer)
                ch = src_chars[i4];
              else
                ch = get_id_char(src_board, i4);

              dcolor = color;
              if(!(color & 0xF0))
                dcolor = (color & 0x0F) | (get_color_linear(i5) & 0xF0);

              if(ch != 32)
              {
                if(!(cur_sprite->flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
                  draw_char_linear_ext(dcolor, ch, i5, 0, 0);
              }
              i4++;
              i5++;
            }
            i4 += skip;
            i5 += skip2;
          }
          break;
        }

        // given colors, under overlay
        case 0: // 00
        {
          color = cur_sprite->color;

          for(i2 = 0; i2 < draw_height; i2++)
          {
            for(i3 = 0; i3 < draw_width; i3++)
            {
              if(use_vlayer)
                ch = src_chars[i4];
              else
                ch = get_id_char(src_board, i4);

              dcolor = color;
              if(!(color & 0xF0))
                dcolor = (color & 0x0F) | (get_color_linear(i5) & 0xF0);

              if((!overlay_mode || overlay_mode == 3 ||
               (overlay[i6] == 32)) && ch != 32)
              {
                if(!(cur_sprite->flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
                  draw_char_linear_ext(dcolor, ch, i5, 0, 0);
              }
              i4++;
              i5++;
              i6++;
            }
            i4 += skip;
            i5 += skip2;
            i6 += skip3;
          }
          break;
        }
      }
    }
  }
}


int sprite_at_xy(struct sprite *cur_sprite, int x, int y)
{
  if((x >= cur_sprite->x) && (x < cur_sprite->x + cur_sprite->width) &&
   (y >= cur_sprite->y) && (y < cur_sprite->y + cur_sprite->height)
   && (cur_sprite->flags & SPRITE_INITIALIZED))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int sprite_colliding_xy(struct world *mzx_world, struct sprite *check_sprite,
 int x, int y)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int ref_x = check_sprite->ref_x + check_sprite->col_x;
  int ref_y = check_sprite->ref_y + check_sprite->col_y;
  int col_width = check_sprite->col_width;
  int col_height = check_sprite->col_height;
  int check_x = x + check_sprite->col_x;
  int check_y = y + check_sprite->col_y;
  struct sprite **sprite_list = mzx_world->sprite_list;
  struct sprite *cur_sprite;
  int colliding = 0;
  int skip, skip2, i, i2, i3, i4, i5;
  int bwidth, bheight;
  int x1, x2, y1, y2;
  int mw, mh;
  bool use_vlayer, use_vlayer2;
  unsigned int x_lmask, x_gmask, y_lmask, y_gmask;
  unsigned int xl, xg, yl, yg, wl, hl, wg, hg;
  char *vlayer_chars = mzx_world->vlayer_chars;
  char *level_id = src_board->level_id;
  int *collision_list = mzx_world->collision_list;
  int vlayer_width = mzx_world->vlayer_width;
  int board_collide = 0;

  if(!(check_sprite->flags & SPRITE_INITIALIZED))
    return -1;

  // Check against the background, will only collide against
  // customblock for now (id 5)

  if(check_sprite->flags & SPRITE_VLAYER)
  {
    use_vlayer = true;
    bwidth = vlayer_width;
    bheight = mzx_world->vlayer_height;
  }
  else
  {
    use_vlayer = false;
    bwidth = board_width;
    bheight = src_board->board_height;
  }

  if(ref_x < 0)
  {
    col_width += ref_x;
    ref_x = 0;
  }

  if(ref_y < 0)
  {
    col_height += ref_y;
    ref_y = 0;
  }

  if(check_x < 0)
  {
    col_width += check_x;
    check_x = 0;
  }

  if(check_y < 0)
  {
    col_height += check_y;
    check_y = 0;
  }

  if((ref_x + col_width) >= bwidth)
    col_width = bwidth - ref_x;

  if((ref_y + col_width) >= bheight)
    col_height = bheight - ref_y;

  if((check_x + col_width) >= board_width)
    col_width = board_width - check_x;

  if((check_y + col_height) >= board_height)
    col_height = board_height - check_y;

  // Check for <= 0 width or height (prevent crashing)
  if((col_width <= 0) || (col_height <= 0))
  {
    mzx_world->collision_count = 0;
    return 0;
  }

  // Scan board area

  skip = board_width - col_width;
  skip2 = bwidth - col_width;

  i3 = (check_y * board_width) + check_x;
  i4 = (ref_y * bwidth) + ref_x;

  for(i = 0; i < col_height; i++)
  {
    for(i2 = 0; i2 < col_width; i2++)
    {
      // First, if ccheck is on, it won't care if the source is 32
      int c;

      if(use_vlayer)
      {
        c = vlayer_chars[i4];
      }
      else
      {
        c = get_id_char(src_board, i4);
      }

      if(!(check_sprite->flags & SPRITE_CHAR_CHECK) ||
       (c != 32))
      {
        // if ccheck2 is on and the char is blank, don't trigger.
        if(!(check_sprite->flags & SPRITE_CHAR_CHECK2) ||
         (!is_blank(c)))
        {
          if(level_id[i3] == 5)
          {
            // Colliding against background
            if(board_collide) break;
            collision_list[colliding] = -1;
            colliding++;
            board_collide = 1;
          }
        }
      }

      i3++;
      i4++;
    }
    i3 += skip;
    i4 += skip2;
  }

  for(i = 0; i < MAX_SPRITES; i++)
  {
    cur_sprite = sprite_list[i];
    if((cur_sprite == check_sprite) ||
     !(cur_sprite->flags & SPRITE_INITIALIZED))
      continue;

    if(!(cur_sprite->col_width) || !(cur_sprite->col_height))
     continue;

    x1 = cur_sprite->x + cur_sprite->col_x;
    x2 = x + check_sprite->col_x;
    y1 = cur_sprite->y + cur_sprite->col_y;
    y2 = y + check_sprite->col_y;
    x_lmask = (signed int)(x1 - x2) >> 31;
    x_gmask = ~x_lmask;
    y_lmask = (signed int)(y1 - y2) >> 31;
    y_gmask = ~y_lmask;
    xl = (x1 & x_lmask) | (x2 & x_gmask);
    xg = (x1 & x_gmask) | (x2 & x_lmask);
    yl = (y1 & y_lmask) | (y2 & y_gmask);
    yg = (y1 & y_gmask) | (y2 & y_lmask);
    wl = (cur_sprite->col_width & x_lmask) |
     (check_sprite->col_width & x_gmask);
    hl = (cur_sprite->col_height & y_lmask) |
     (check_sprite->col_height & y_gmask);
    if((((xg - xl) - wl) & ((yg - yl) - hl)) >> 31)
    {
      // Does it require char/char verification?
      if(check_sprite->flags & (SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2))
      {
        // The sub rectangle is going to be somewhere within both sprites;
        // It's going to start at the beginning of the component further
        // along (xg/yg) which is an absolute board position; find the
        // i5 both sprites are from this... and you will iterate in a
        // rectangle there that is (xl + wl) - xg... the difference between
        // where the first one ends and the second one begins
        mw = (xl + wl) - xg;
        mh = (yl + hl) - yg;
        // Reuse these.. i5s. For both you must look at PHYSICAL data, that
        // is, where the chars of the sprite itself is.
        x1 = cur_sprite->ref_x + (xg - x1) + cur_sprite->col_x;
        y1 = cur_sprite->ref_y + (yg - y1) + cur_sprite->col_y;
        x2 = check_sprite->ref_x + (xg - x2) + check_sprite->col_x;
        y2 = check_sprite->ref_y + (yg - y2) + check_sprite->col_y;
        // Check to make sure draw area doesn't go over.
        wg = (cur_sprite->col_width & x_gmask) |
         (check_sprite->col_width & x_lmask);
        hg = (cur_sprite->col_height & y_gmask) |
         (check_sprite->col_height & y_lmask);

        if((unsigned int)mw > wg)
        {
          mw = wg;
        }
        if((unsigned int)mh > hg)
        {
          mh = hg;
        }

        // Now iterate through the rect; if both are NOT 32 then a
        // collision is flagged.

        if(cur_sprite->flags & SPRITE_VLAYER)
        {
          use_vlayer2 = true;
          i4 = (y1 * vlayer_width) + x1;
          skip = vlayer_width - mw;
        }
        else
        {
          use_vlayer2 = false;
          i4 = (y1 * board_width) + x1;
          skip = board_width - mw;
        }

        i5 = (y2 * bwidth) + x2;
        skip2 = bwidth - mw;

        // If ccheck mode 2 is on it should do a further strength check.
        if(check_sprite->flags & SPRITE_CHAR_CHECK2)
        {
          for(i2 = 0; i2 < mh; i2++)
          {
            for(i3 = 0; i3 < mw; i3++)
            {
              char c1, c2;
              if(!use_vlayer2)
              {
                c1 = get_id_char(src_board, i4);
              }
              else
              {
                c1 = vlayer_chars[i4];
              }
              if(!use_vlayer)
              {
                c2 = get_id_char(src_board, i5);
              }
              else
              {
                c2 = vlayer_chars[i5];
              }
              if((c1 != 32) && (c2 != 32))
              {
                // Collision.. maybe.
                // Neither char may be blank
                if(!(is_blank(c1) || is_blank(c2)))
                {
                  collision_list[colliding] = i;
                  colliding++;

                  goto next;
                }
              }
              i4++;
              i5++;
            }
            i4 += skip;
            i5 += skip2;
          }
        }
        else
        {
          for(i2 = 0; i2 < mh; i2++)
          {
            for(i3 = 0; i3 < mw; i3++)
            {
              char c1, c2;
              if(!use_vlayer2)
              {
                c1 = get_id_char(src_board, i4);
              }
              else
              {
                c1 = vlayer_chars[i4];
              }
              if(!use_vlayer)
              {
                c2 = get_id_char(src_board, i5);
              }
              else
              {
                c2 = vlayer_chars[i5];
              }
              if((c1 != 32) && (c2 != 32))
              {
                // Collision!
                collision_list[colliding] = i;
                colliding++;

                goto next;
              }
              i4++;
              i5++;
            }
            i4 += skip;
            i5 += skip;
          }
        }
      }
      else
      {
        collision_list[colliding] = i;
        colliding++;
      }

      next:
      continue;
    }
  }

  mzx_world->collision_count = colliding;
  return colliding;
}
