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
  int dest_y = spr_dest->y * (spr_dest->flags & SPRITE_UNBOUND ? 1 : CHAR_H);
  int src_y = spr_src->y * (spr_src->flags & SPRITE_UNBOUND ? 1 : CHAR_H);

  int diff = ((dest_y + spr_dest->col_y) - (src_y + spr_src->col_y));

  return diff ? diff : (spr_dest->qsort_order - spr_src->qsort_order);
}

static inline int is_blank(Uint16 c)
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
  Uint16 ch;
  char color;
  int viewport_x = src_board->viewport_x;
  int viewport_y = src_board->viewport_y;
  int viewport_width = src_board->viewport_width;
  int viewport_height = src_board->viewport_height;
  int board_width = src_board->board_width;
  int overlay_mode = src_board->overlay_mode;
  char *overlay = src_board->overlay;
  char *src_chars;
  char *src_colors;
  Uint32 layer;
  int draw_layer_order;
  bool unbound;
  int transparent_color;
  int ub_offx, ub_offy;

  calculate_xytop(mzx_world, &screen_x, &screen_y);
  // see if y sort should be done
  if(mzx_world->sprite_y_order)
  {
    // Switched to stdlib qsort. Hooray!
    // Fill it with the active sprites in the beginning
    // and the inactive sprites in the end.
    for(i = 0, i2 = 0, i3 = MAX_SPRITES - 1; i < MAX_SPRITES; i++)
    {
      // Stabilize the sort
      sprite_list[i]->qsort_order = i;
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
      if (cur_sprite->flags & SPRITE_UNBOUND)
        unbound = true;
      else
        unbound = false;
      ref_x = cur_sprite->ref_x;
      ref_y = cur_sprite->ref_y;
      offset_x = 0;
      offset_y = 0;
      start_x = viewport_x;
      start_y = viewport_y;

      start_x += cur_sprite->x;
      start_y += cur_sprite->y;

      if(!(cur_sprite->flags & SPRITE_STATIC))
      {
        start_x -= screen_x;
        start_y -= screen_y;
      }

      draw_width = cur_sprite->width;
      draw_height = cur_sprite->height;

      // clip draw position against viewport
      if (!unbound) {
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
      }

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
      
      if (draw_width <= 0 || draw_height <= 0)
        continue;
      ub_offx = 0;
      ub_offy = 0;
      if (unbound)
      {
        // Are portions of this sprite entirely outside the viewport?
        // If so, clip them off
        start_x = cur_sprite->x + viewport_x * CHAR_W;
        start_y = cur_sprite->y + viewport_y * CHAR_H;
        if (!(cur_sprite->flags & SPRITE_STATIC)) {
          start_x -= screen_x * CHAR_W;
          start_y -= screen_y * CHAR_H;
        }

        if (start_x < -CHAR_W + 1) {
          ub_offx += start_x / -CHAR_W;
          draw_width -= start_x / -CHAR_W;
          start_x += (start_x / -CHAR_W) * CHAR_W;
        }
        if (start_y < -CHAR_H + 1) {
          ub_offy += start_y / -CHAR_H;
          draw_height -= start_y / -CHAR_H;
          start_y += (start_y / -CHAR_H) * CHAR_H;
        }
        if (start_x + draw_width * CHAR_W >= (viewport_width + viewport_x + 1) * CHAR_W) {
          draw_width += (((viewport_width + viewport_x + 1) * CHAR_W) - (start_x + draw_width * CHAR_W)) / CHAR_W;
        }
        if (start_y + draw_height * CHAR_H >= (viewport_height + viewport_y + 1) * CHAR_H) {
          draw_height += (((viewport_height + viewport_y + 1) * CHAR_H) - (start_y + draw_height * CHAR_H)) / CHAR_H;
        }

        // If there is nothing left of the sprite, don't draw it
        if (draw_width < 1 || draw_height < 1) continue;

        // Zero out start_x and start_y for now
        // We will then modify their values directly
        start_x = 0;
        start_y = 0;
      }

      i4 = ((ref_y + offset_y + ub_offy) * bwidth) + ref_x + offset_x + ub_offx;
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
      if (cur_sprite->flags & SPRITE_OVER_OVERLAY)
        draw_layer_order = LAYER_DRAWORDER_OVERLAY + 1 + i;
      else
        draw_layer_order = LAYER_DRAWORDER_BOARD + 1 + i;

      // Only unbound sprites can have a transparent colour
      if (unbound)
        transparent_color = cur_sprite->transparent_color;
      else
        transparent_color = -1;
      
      layer = create_layer(start_x * CHAR_W, start_y * CHAR_H, draw_width,
       draw_height, draw_layer_order, transparent_color, cur_sprite->offset,
       unbound);
      select_layer(layer);
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
          if(!(cur_sprite->flags & SPRITE_SRC_COLORS))
            color = cur_sprite->color;
          if (!unbound)
            if(!(color & 0xF0))
              color = (color & 0x0F) | (get_color_linear(i5) & 0xF0);

          if (unbound || !(overlay_mode && overlay_mode != 3 && overlay[i6] != 32 && !(cur_sprite->flags & SPRITE_OVER_OVERLAY)))
          {
            if (unbound || ch != 32)
            {
              if((cur_sprite->flags & (SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2)) != SPRITE_CHAR_CHECK2 || !is_blank((ch + cur_sprite->offset) % PROTECTED_CHARSET_POSITION))
              {
                if (!unbound)
                  draw_char_linear_ext(color, ch, i5, 0, 0);
                else
                  draw_char_to_layer(color, ch, i3, i2, 0, 0);
              }
            }
          }
          i4++;
          i5++;
          i6++;
        }
        i4 += skip;
        i5 += skip2;
        i6 += skip3;
      }
      
      if (unbound)
      {
        // Fix the x and y positions of this sprite
        start_x = cur_sprite->x + viewport_x * CHAR_W + ub_offx * CHAR_W;
        start_y = cur_sprite->y + viewport_y * CHAR_H + ub_offy * CHAR_H;
        if (!(cur_sprite->flags & SPRITE_STATIC)) {
          start_x -= screen_x * CHAR_W;
          start_y -= screen_y * CHAR_H;
        }
        move_layer(layer, start_x, start_y);
      }
    }
  }
}

// This is legacy sprite-checking code.
// We'll use it in worlds <=2.84 for maximum compat

static int sprite_colliding_xy_old(struct world *mzx_world, struct sprite *check_sprite,
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

struct rect {
  int x, y, w, h;
};

static inline struct rect rectangle(int x, int y, int w, int h)
{
  struct rect r = {x, y, w, h};
  return r;
}

static inline bool rectangle_overlap(struct rect a,
 struct rect b)
{
  //debug("rectangle_overlap(%d, %d, %d, %d, %d, %d, %d, %d)\n", a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h);
  if (a.x < b.x + b.w &&
      a.x + a.w > b.x &&
      a.y < b.y + b.h &&
      a.y + a.h > b.y)
    return true;
  return false;
}

static inline bool constrain_rectangle(struct rect a,
 struct rect *b)
{
  // Constrain rectangle b to only cover the portion that exists within
  // rectangle a

  // Returns true if there is any of rectangle b left afterwards
  if (b->w <= 0 || b->h <= 0) return false;

  if (b->x < a.x) {
    b->w -= (a.x - b->x);
    b->x += (a.x - b->x);
    if (b->w <= 0)
      return false;
  }
  if (b->y < a.y) {
    b->h -= (a.y - b->y);
    b->y += (a.y - b->y);
    if (b->h <= 0)
      return false;
  }
  if (b->x + b->w > a.x + a.w) {
    b->w -= (b->x + b->w) - (a.x + a.w);
    if (b->w <= 0)
      return false;
  }
  if (b->y + b->h > a.y + a.h) {
    b->h -= (b->y + b->h) - (a.y + a.h);
    if (b->h <= 0)
      return false;
  }
  return true;
}

static inline struct rect sprite_rectangle(const struct sprite *spr)
{
  struct rect sprite_rect = rectangle(spr->x, spr->y, spr->width, spr->height);

  // Multiply out the coords of unbound sprites
  if (!(spr->flags & SPRITE_UNBOUND)) {
    sprite_rect.x *= CHAR_W;
    sprite_rect.y *= CHAR_H;
  }
  sprite_rect.w *= CHAR_W;
  sprite_rect.h *= CHAR_H;

  return sprite_rect;
}

static inline struct rect collision_rectangle(const struct sprite *spr)
{
  struct rect collision_rect = rectangle(spr->col_x, spr->col_y, spr->col_width, spr->col_height);
  if (!(spr->flags & SPRITE_UNBOUND)) {
    collision_rect.x *= CHAR_W;
    collision_rect.y *= CHAR_H;
    collision_rect.w *= CHAR_W;
    collision_rect.h *= CHAR_H;
  }
  return collision_rect;
}

static inline struct rect board_rectangle(struct rect r)
{
  int x1 = r.x, y1 = r.y, x2 = r.x + r.w - 1, y2 = r.y + r.h - 1;
  return rectangle(x1 / CHAR_W, y1 / CHAR_H, x2 / CHAR_W - x1 / CHAR_W + 1, y2 / CHAR_H - y1 / CHAR_H + 1);
}

/*

struct collision_mask
{
  int sx, sy, cw, ch;
  char *mask;
};

static inline struct collision_mask create_collision_mask(struct sprite *spr, char flags, struct rect collision_rect)
{
  struct rect sprite_rect = sprite_rectangle(spr);
  int x1 = (collision_rect.x - sprite_rect.x) / CHAR_W;
  int y1 = (collision_rect.y - sprite_rect.y) / CHAR_H;
  int x2 = (collision_rect.x + collision_rect.w - 1 - sprite_rect.x) / CHAR_W;
  int y2 = (collision_rect.y + collision_rect.h - 1 - sprite_rect.y) / CHAR_H;
  int cw = x2 - x1 + 1;
  int ch = y2 - y1 + 1;
  int sx = x1 * CHAR_W;
  int sy = y1 * CHAR_H;
  struct collision_mask m = {sx, sy, cw, ch, NULL};
  int x, y, cx, cy, c, rx, ry;
  char *maskPtr = mask;
  if (!(flags & (SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2))) {
    return m;
  }
  m.mask = cmalloc(cw * ch);
  memset(m.mask, 1, cw * ch);
  for (y = 0; y < ch; y++) {
    for (x = 0; x < cw; x++) {
      cx = x + x1;
      cy = y + y1;

      rx = cx + spr->ref_x;


      maskPtr++;
    }
  }
}

static inline void destroy_collision_mask(struct collision_mask m)
{
  if (m.mask)
    free(m.mask);
}
*/

static inline void get_sprite_tile(const struct sprite *spr, int x, int y, struct world *mzx_world, int *ch, int *col)
{
  struct board *src_board;
  if (spr->flags & SPRITE_VLAYER) {
    if (x >= 0 && y >= 0 && x < mzx_world->vlayer_width && y < mzx_world->vlayer_height) {
      *ch = mzx_world->vlayer_chars[y * mzx_world->vlayer_width + x];
      if (col) *col = mzx_world->vlayer_colors[y * mzx_world->vlayer_width + x];
    } else {
      *ch = -1;
      if (col) *col = -1;
    }
  } else {
    src_board = mzx_world->current_board;
    if (x >= 0 && y >= 0 && x < src_board->board_width && y < src_board->board_height) {
      *ch = get_id_char(src_board, y * src_board->board_width + x);
      if (col) {
        *col = get_id_color(src_board, y * src_board->board_width + x);
      }
    } else {
      *ch = -1;
      if (col) *col = -1;
    }
  }
  if (col && !(spr->flags & SPRITE_SRC_COLORS)) {
    *col = spr->color;
  }
}

static inline bool collision_char(const struct sprite *spr, char flags, int x, int y, struct world *mzx_world)
{
  int ch;
  get_sprite_tile(spr, x, y, mzx_world, &ch, NULL);
  if (ch == -1) return false;
  ch = (ch + spr->offset) % PROTECTED_CHARSET_POSITION;
  if ((flags & SPRITE_CHAR_CHECK) && ch != 32) return true;
  if ((flags & SPRITE_CHAR_CHECK2) && !is_blank(ch)) return true;
  return false;
}

static inline bool collision_in(const struct sprite *spr, char flags, struct rect c, struct world *mzx_world)
{
  char pixcheck = SPRITE_UNBOUND | SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2;
  int cx, cy, c2x, c2y;
  //debug("spr collision_in(%d,%d,%d,%d)\n", c.x, c.y, c.w, c.h);
  if (!(flags & (SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2))) return true;
  if ((flags & pixcheck) == pixcheck) return true;
  cx = c.x / CHAR_W + spr->ref_x;
  cy = c.y / CHAR_H + spr->ref_y;
  if (collision_char(spr, flags, cx, cy, mzx_world)) return true;
  c2x = (c.x + c.w - 1) / CHAR_W + spr->ref_x;
  if (c2x != cx && collision_char(spr, flags, c2x, cy, mzx_world)) return true;
  c2y = (c.y + c.h - 1) / CHAR_H + spr->ref_y;
  if (c2y != cy && collision_char(spr, flags, cx, c2y, mzx_world)) return true;
  if ((c2x != cx || c2y != cy) && collision_char(spr, flags, c2x, c2y, mzx_world)) return true;

  return false;
}

struct mask {
  struct rect dim;
  Uint8 *mapping;
  Uint8 *data;
};


static inline struct mask allocate_mask(const struct sprite *spr)
{
  struct mask m;
  m.dim = sprite_rectangle(spr);
  m.mapping = ccalloc(spr->width * spr->height, 1);
  m.data = cmalloc(spr->width * spr->height * CHAR_SIZE);
  return m;
}

static inline struct mask null_mask(void)
{
  struct mask m = {rectangle(0, 0, 0, 0), NULL, NULL};
  return m;
}
static inline void destroy_mask(struct mask m)
{
  free(m.mapping);
  free(m.data);
}

int sprite_at_xy(struct sprite *spr, int x, int y)
{
  struct rect sprite_rect;

  // If the sprite isn't on, this instantly fails
  if (!(spr->flags & SPRITE_INITIALIZED))
    return 0;

  sprite_rect = sprite_rectangle(spr);

  if (rectangle_overlap(sprite_rect, rectangle(x * CHAR_W, y * CHAR_H, CHAR_W, CHAR_H)))
    return 1;
  return 0;
}

static inline void mask_alloc_chr(const struct sprite *spr, struct mask m, int ch, struct world *mzx_world)
{
  int y = ch / spr->width, x = ch % spr->width;
  int chr, col;
  int px, py;
  int tcol = spr->transparent_color;
  Uint8 bitmap_buffer[CHAR_W * CHAR_H];
  Uint8 *output;
  output = &m.data[ch * CHAR_SIZE];;

  if (!m.mapping[ch]) {
    m.mapping[ch] = 1;
    get_sprite_tile(spr, x + spr->ref_x, y + spr->ref_y, mzx_world, &chr, &col);
    memset(output, 0, CHAR_SIZE);
    if (chr != -1) {
      dump_char((chr + spr->offset) % PROTECTED_CHARSET_POSITION, col, -1, bitmap_buffer);
      for (py = 0; py < CHAR_H; py++) {
        for (px = 0; px < CHAR_W; px++) {
          if (bitmap_buffer[py * CHAR_W + px] != tcol) {
            output[py] |= 0x80 >> px;
          } else {
          }
        }
      }
    }
  }
}

static inline bool collision_pix_in(const struct sprite *spr, struct mask m, struct rect c, struct world *mzx_world)
{
  char pixcheck = SPRITE_UNBOUND | SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2;
  int x, y, px, py, ch;
  if ((spr->flags & pixcheck) != pixcheck) return true;

  // Pixel by pixel collision check
  for (y = c.y; y < c.y + c.h; y++) {
    py = y - m.dim.y;
    for (x = c.x; x < c.x + c.w; x++) {
      px = x - m.dim.x;
      ch = (py / CHAR_H * spr->width + px / CHAR_W);
      mask_alloc_chr(spr, m, ch, mzx_world);

      if (m.data[ch * CHAR_SIZE + (py % CHAR_H)] & (0x80 >> (px % CHAR_W))) return true;
    }
  }
  return false;
}

static inline bool collision_pix_between(const struct sprite *spr, struct mask spr_m, const struct sprite *targ,
 struct mask targ_m, struct rect c, struct world *mzx_world)
{
  char pixcheck = SPRITE_UNBOUND | SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2;
  if ((spr->flags & pixcheck) != pixcheck) {
    if ((targ->flags & pixcheck) != pixcheck) {
      return true;
    }
    // Only target sprite does a pixel check
    return collision_pix_in(targ, targ_m, c, mzx_world);
  } else {
    if ((targ->flags & pixcheck) != pixcheck) {
      // Only the checked sprite does a pixel check
      return collision_pix_in(spr, spr_m, c, mzx_world);
    }
    {
      // Both sprites need a pixel check
      int x, y, sx, sy, tx, ty, sch, tch;
      // Pixel by pixel collision check
      for (y = c.y; y < c.y + c.h; y++) {
        sy = y - spr_m.dim.y;
        ty = y - targ_m.dim.y;
        for (x = c.x; x < c.x + c.w; x++) {
          sx = x - spr_m.dim.x;
          tx = x - targ_m.dim.x;
          sch = (sy / CHAR_H * spr->width + sx / CHAR_W);
          tch = (ty / CHAR_H * targ->width + tx / CHAR_W);
          
          mask_alloc_chr(spr, spr_m, sch, mzx_world);
          mask_alloc_chr(targ, targ_m, tch, mzx_world);

          if ((spr_m.data[sch * CHAR_SIZE + (sy % CHAR_H)] & (0x80 >> (sx % CHAR_W))) &&
          (targ_m.data[tch * CHAR_SIZE + (ty % CHAR_H)] & (0x80 >> (tx % CHAR_W)))) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

int sprite_colliding_xy(struct world *mzx_world, struct sprite *spr,
 int x, int y)
{
  struct sprite collision_sprite = *spr;
  struct rect sprite_rect, collision_rect;
  int *collision_list = mzx_world->collision_list;
  int *collisions = &mzx_world->collision_count;
  struct board *src_board = mzx_world->current_board;
  struct rect board_rect = rectangle(0, 0, src_board->board_width, src_board->board_height);
  struct rect board_full_rect = rectangle(0, 0, src_board->board_width * CHAR_W, src_board->board_height * CHAR_H);
  struct rect collision_board_rect;
  char *level_id = src_board->level_id;
  int board_w = src_board->board_width;
  int bx, by;
  int sprite_idx;
  struct sprite *target_spr;
  struct rect target_spr_rect, target_col_rect;
  struct rect check_rect, check_rect_transformed;
  int cx, cy;
  bool sprite_collided;
  char target_flags;
  struct mask spr_mask = null_mask(), target_mask = null_mask();
  bool spr_mask_allocated = false, target_mask_allocated;

  if(mzx_world->version < V290)
    return sprite_colliding_xy_old(mzx_world, spr, x, y);

  collision_sprite.x = x;
  collision_sprite.y = y;

  //debug("sprite_colliding_xy(%d,%d,%d,%d)\n", spr->x, spr->y, spr->width, spr->height);

  if(!(spr->flags & SPRITE_INITIALIZED))
    return -1;
  
  // Reset the collision list. To match old behaviour, this only happens
  // if the initialisation check passes
  *collisions = 0;
  
  sprite_rect = sprite_rectangle(&collision_sprite);
  collision_rect = collision_rectangle(&collision_sprite);
  collision_rect.x += sprite_rect.x;
  collision_rect.y += sprite_rect.y;
  if (!(spr->flags & SPRITE_UNBOUND)) {
    if (!constrain_rectangle(board_full_rect, &collision_rect))
      return -1;
  }
  
  if (spr->flags & SPRITE_UNBOUND &&
      spr->flags & SPRITE_CHAR_CHECK &&
      spr->flags & SPRITE_CHAR_CHECK2)
  {
    if (!constrain_rectangle(sprite_rect, &collision_rect))
      return -1;
    spr_mask = allocate_mask(&collision_sprite);
    spr_mask_allocated = true;
  }
  
  // Check the contents of the board
  collision_board_rect = board_rectangle(collision_rect);
  if (constrain_rectangle(board_rect, &collision_board_rect)) {
    for (by = collision_board_rect.y; by < collision_board_rect.y + collision_board_rect.h; by++) {
      for (bx = collision_board_rect.x; bx < collision_board_rect.x + collision_board_rect.w; bx++) {
        if (level_id[by * board_w + bx] == 5) {
          check_rect = rectangle(bx * CHAR_W, by * CHAR_H, CHAR_W, CHAR_H);
          if (constrain_rectangle(collision_rect, &check_rect)) {
            check_rect_transformed = check_rect;
            check_rect_transformed.x -= sprite_rect.x; check_rect_transformed.y -= sprite_rect.y;
            if (collision_in(spr, spr->flags, check_rect_transformed, mzx_world)) {
              if (collision_pix_in(spr, spr_mask, check_rect, mzx_world)) {
                collision_list[(*collisions)++] = -1;
                break;
              }
            }
          }
        }
      }
      if (*collisions) break;
    }
  }

  for (sprite_idx = 0; sprite_idx < MAX_SPRITES; sprite_idx++) {
    target_spr = mzx_world->sprite_list[sprite_idx];
    if (!(target_spr->flags & SPRITE_INITIALIZED))
      continue;
    if (target_spr == spr)
      continue;
    target_spr_rect = sprite_rectangle(target_spr);
    target_col_rect = collision_rectangle(target_spr);
    //debug("sprite #%d a: %d %d %d %d\n", target_rect.x, target_rect.y, target_rect.w, target_rect.h);
    target_col_rect.x += target_spr_rect.x;
    target_col_rect.y += target_spr_rect.y;
    if (!(target_spr->flags & SPRITE_UNBOUND)) {
      if (!constrain_rectangle(board_full_rect, &target_col_rect))
        continue;
    }
    sprite_collided = false;

    // ccheck between bound sprites works by looking at the flags of the sprite doing the collision
    // checking only. This is to maintain backwards compatibility with code that expects this
    // The reason this is not tied into a version check is to allow old code to continue working
    // even in a new world
    // If either sprite is unbound we switch to sensible behaviour (using the ccheck of each sprite
    // to determine its respective collision mask)

    if ((spr->flags | target_spr->flags) & SPRITE_UNBOUND) {
      target_flags = target_spr->flags;
    } else {
      target_flags = spr->flags;
    }

    if (constrain_rectangle(collision_rect, &target_col_rect)) {
      target_mask_allocated = false;
      if (target_spr->flags & SPRITE_UNBOUND &&
          target_spr->flags & SPRITE_CHAR_CHECK &&
          target_spr->flags & SPRITE_CHAR_CHECK2)
      {
        if (!constrain_rectangle(target_spr_rect, &target_col_rect))
          continue;
        target_mask = allocate_mask(target_spr);
        target_mask_allocated = true;
      }

      for (cy = collision_rect.y; cy < collision_rect.y + collision_rect.h; cy += CHAR_H) {
        for (cx = collision_rect.x; cx < collision_rect.x + collision_rect.w; cx += CHAR_W) {
          check_rect = rectangle(cx, cy, CHAR_W, CHAR_H);
          if (constrain_rectangle(target_col_rect, &check_rect)) {
            check_rect_transformed = check_rect;
            check_rect_transformed.x -= sprite_rect.x;
            check_rect_transformed.y -= sprite_rect.y;
            if (collision_in(spr, spr->flags, check_rect_transformed, mzx_world)) {
              check_rect_transformed = check_rect;
              check_rect_transformed.x -= target_spr_rect.x;
              check_rect_transformed.y -= target_spr_rect.y;
              if (collision_in(target_spr, target_flags, check_rect_transformed, mzx_world)) {
                if (collision_pix_between(spr, spr_mask, target_spr, target_mask, check_rect, mzx_world)) {
                  sprite_collided = true;
                  collision_list[(*collisions)++] = sprite_idx;
                  break; // This breaks out of the cx loop
                }
              }
            }
          }
        }
        if (sprite_collided) break; // We've already seen a collision with this sprite
      }
      
      if (target_mask_allocated) {
        destroy_mask(target_mask);
      }
    }
  }

  if (spr_mask_allocated) {
    destroy_mask(spr_mask);
  }
  return *collisions;
}

/*
int sprite_at_xy_old(struct sprite *cur_sprite, int x, int y)
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
*/
