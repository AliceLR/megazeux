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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"
#include "error.h"
#include "game_ops.h"
#include "graphics.h"
#include "idput.h"
#include "sprite.h"
#include "world.h"
#include "world_struct.h"

static inline int is_blank(uint16_t c)
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
  /**
   * Prior to 2.80, only one of these had to be set, and it was extremely
   * likely at least one WOULD be set because DOS versions would not clear
   * anything but the flags between game loads. Instead of emulating the
   * quirks of pre-port MZX for this, just unconditionally allow placement.
   */
  if((cur_sprite->width && cur_sprite->height) ||
   (mzx_world->version < VERSION_PORT))
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

// Sort by zorder, then by sprite number
static int compare_spr_normal(const void *dest, const void *src)
{
  struct sprite *spr_dest = *((struct sprite **)dest);
  struct sprite *spr_src = *((struct sprite **)src);

  int diff = spr_dest->z - spr_src->z;

  return diff ? diff : (spr_dest->qsort_order - spr_src->qsort_order);
}

// Sort by zorder, then by yorder, then by sprite number
static int compare_spr_yorder(const void *dest, const void *src)
{
  struct sprite *spr_dest = *((struct sprite **)dest);
  struct sprite *spr_src = *((struct sprite **)src);

  int diff = spr_dest->z - spr_src->z;
  int dest_y, src_y;

  if (diff != 0) return diff;

  dest_y = spr_dest->y * (spr_dest->flags & SPRITE_UNBOUND ? 1 : CHAR_H);
  src_y = spr_src->y * (spr_src->flags & SPRITE_UNBOUND ? 1 : CHAR_H);

  diff = ((dest_y + spr_dest->col_y) - (src_y + spr_src->col_y));

  return diff ? diff : (spr_dest->qsort_order - spr_src->qsort_order);
}

static inline void sort_sprites(const struct sprite **sorted_list,
 struct sprite **sprite_list, int spr_yorder)
{
  // Fill the sorted list with the active sprites in the beginning
  // and the inactive sprites in the end.
  struct sprite *cur_sprite;
  int i;
  int i_active;
  int i_inactive;
  int (*spr_compare)(const void *, const void *);

  for(i = 0, i_active = 0, i_inactive = MAX_SPRITES - 1; i < MAX_SPRITES; i++)
  {
    cur_sprite = sprite_list[i];

    // This is used to stabilize the sort
    cur_sprite->qsort_order = i;

    if(cur_sprite->flags & SPRITE_INITIALIZED)
    {
      sorted_list[i_active] = cur_sprite;
      i_active++;
    }
    else
    {
      sorted_list[i_inactive] = cur_sprite;
      i_inactive--;
    }
  }

  if(spr_yorder)
    spr_compare = compare_spr_yorder;

  else
    spr_compare = compare_spr_normal;

  qsort(sorted_list, i_active, sizeof(struct sprite *), spr_compare);
}

void draw_sprites(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int start_x, start_y, offset_x, offset_y;
  int i, x, y;
  int src_offset;
  int overlay_offset;
  int src_skip;
  int overlay_skip;
  int draw_width, draw_height, ref_x, ref_y, screen_x, screen_y;
  int src_width;
  int src_height;
  boolean use_vlayer;
  struct sprite **sprite_list = mzx_world->sprite_list;
  const struct sprite *sorted_list[MAX_SPRITES];
  const struct sprite *cur_sprite;
  uint16_t ch;
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
  uint32_t layer;
  int draw_layer_order;
  boolean unbound;
  int transparent_color;

  calculate_xytop(mzx_world, &screen_x, &screen_y);

  sort_sprites(sorted_list, sprite_list, mzx_world->sprite_y_order);

  // draw this on top of the SCREEN window.
  for(i = 0; i < MAX_SPRITES; i++)
  {
    cur_sprite = sorted_list[i];

    if(!(cur_sprite->flags & SPRITE_INITIALIZED))
      continue;

    if(cur_sprite->flags & SPRITE_UNBOUND)
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
    if(!unbound)
    {
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
      src_width = mzx_world->vlayer_width;
      src_height = mzx_world->vlayer_height;
      src_chars = mzx_world->vlayer_chars;
      src_colors = mzx_world->vlayer_colors;
    }
    else
    {
      use_vlayer = false;
      src_width = board_width;
      src_height = src_board->board_height;
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

    if((ref_x + draw_width) > src_width)
      draw_width = src_width - ref_x;

    if((ref_y + draw_height) > src_height)
      draw_height = src_height - ref_y;

    if(draw_width <= 0 || draw_height <= 0)
      continue;

    if(unbound)
    {
      int clip_x = (viewport_width + viewport_x + 1) * CHAR_W;
      int clip_y = (viewport_height + viewport_y + 1) * CHAR_H;
      int end_x;
      int end_y;

      // Are portions of this sprite entirely outside the viewport?
      // If so, clip them off
      start_x = cur_sprite->x + viewport_x * CHAR_W;
      start_y = cur_sprite->y + viewport_y * CHAR_H;

      if(!(cur_sprite->flags & SPRITE_STATIC))
      {
        start_x -= screen_x * CHAR_W;
        start_y -= screen_y * CHAR_H;
      }

      end_x = start_x + (draw_width * CHAR_W);
      end_y = start_y + (draw_height * CHAR_H);

      if(start_x < -CHAR_W + 1)
      {
        offset_x += start_x / -CHAR_W;
        draw_width -= start_x / -CHAR_W;
        start_x += (start_x / -CHAR_W) * CHAR_W;
      }
      if(start_y < -CHAR_H + 1)
      {
        offset_y += start_y / -CHAR_H;
        draw_height -= start_y / -CHAR_H;
        start_y += (start_y / -CHAR_H) * CHAR_H;
      }
      if(end_x > clip_x)
      {
        draw_width -= (end_x - clip_x) / CHAR_W;
        end_x = clip_x;
      }
      if(end_y > clip_y)
      {
        draw_height -= (end_y - clip_y) / CHAR_H;
        end_y = clip_y;
      }

      // If there is nothing left of the sprite, don't draw it
      if(draw_width < 1 || draw_height < 1)
        continue;

      // Zero out start_x and start_y for now
      // We will then modify their values directly
      start_x = 0;
      start_y = 0;
    }

    if(cur_sprite->flags & SPRITE_OVER_OVERLAY)
      draw_layer_order = LAYER_DRAWORDER_OVERLAY + 1 + i;
    else
      draw_layer_order = LAYER_DRAWORDER_BOARD + 1 + i;

    // Only unbound sprites can have a transparent colour
    if(unbound)
      transparent_color = cur_sprite->transparent_color;
    else
      transparent_color = -1;

    layer = create_layer(start_x * CHAR_W, start_y * CHAR_H, draw_width,
     draw_height, draw_layer_order, transparent_color, cur_sprite->offset,
     unbound);
    select_layer(layer);

    // Offset and skip value for sprite reference
    src_offset = (ref_y + offset_y) * src_width + ref_x + offset_x;
    src_skip = src_width - draw_width;

    // Offset and skip value for overlay (as it may cover part of a sprite)
    overlay_skip = board_width - draw_width;
    if(overlay_mode == 2)
    {
      // Static overlay
      overlay_offset = ((start_y - viewport_y) * board_width) +
       start_x - viewport_x;
    }
    else
    {
      // Normal overlay
      overlay_offset = ((cur_sprite->y + offset_y) * board_width) +
       cur_sprite->x + offset_x;
    }

    for(y = 0; y < draw_height; y++)
    {
      for(x = 0; x < draw_width; x++)
      {
        if(use_vlayer)
        {
          color = src_colors[src_offset];
          ch = src_chars[src_offset];
        }
        else
        {
          color = get_id_color(src_board, src_offset);
          ch = get_id_char(src_board, src_offset);
        }

        if(!(cur_sprite->flags & SPRITE_SRC_COLORS))
          color = cur_sprite->color;

        if(unbound || (cur_sprite->flags & SPRITE_OVER_OVERLAY) ||
         !(overlay_mode && overlay_mode != 3 && overlay[overlay_offset] != 32))
        {
          if(unbound || ch != 32)
          {
            if((cur_sprite->flags &
             (SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2)) != SPRITE_CHAR_CHECK2 ||
             !is_blank((ch + cur_sprite->offset) % PROTECTED_CHARSET_POSITION))
            {
              if(!unbound)
              {
                // This implements the legacy sprite "transparency" effect.
                draw_char_bleedthru_ext(ch, color, x + start_x, y + start_y, 0, 0);
              }
              else
                draw_char_to_layer(ch, color, x, y, 0, 0);
            }
          }
        }
        src_offset++;
        overlay_offset++;
      }
      src_offset += src_skip;
      overlay_offset += overlay_skip;
    }

    if(unbound)
    {
      // Fix the x and y positions of this sprite
      start_x = cur_sprite->x + viewport_x * CHAR_W + offset_x * CHAR_W;
      start_y = cur_sprite->y + viewport_y * CHAR_H + offset_y * CHAR_H;

      if(!(cur_sprite->flags & SPRITE_STATIC))
      {
        start_x -= screen_x * CHAR_W;
        start_y -= screen_y * CHAR_H;
      }
      move_layer(layer, start_x, start_y);
    }
  }
}

/**
 * This is legacy sprite-checking code.
 * Use it in worlds <=2.84 for maximum compat. (The real reason it was left
 * alone is because it's a dumpster fire and no one wants to touch it.)
 */
static int sprite_colliding_xy_old(struct world *mzx_world,
 struct sprite *check_sprite, int x, int y)
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
  boolean use_vlayer, use_vlayer2;
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

  if((ref_y + col_height) >= bheight)
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

      if(!(check_sprite->flags & SPRITE_CHAR_CHECK) || (c != 32))
      {
        // if ccheck2 is on and the char is blank, don't trigger.
        if(!(check_sprite->flags & SPRITE_CHAR_CHECK2) || !is_blank(c))
        {
          if(level_id[i3] == CUSTOM_BLOCK)
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

static inline boolean rectangle_overlap(struct rect a,
 struct rect b)
{
  //debug("rectangle_overlap(%d, %d, %d, %d, %d, %d, %d, %d)\n",
  // a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h);
  if(a.x < b.x + b.w &&
     a.x + a.w > b.x &&
     a.y < b.y + b.h &&
     a.y + a.h > b.y)
    return true;
  return false;
}

static inline boolean constrain_rectangle(struct rect a,
 struct rect *b)
{
  // Constrain rectangle b to only cover the portion that exists within
  // rectangle a

  // Returns true if there is any of rectangle b left afterwards
  if(b->w <= 0 || b->h <= 0)
    return false;

  if(b->x < a.x)
  {
    b->w -= (a.x - b->x);
    b->x += (a.x - b->x);
    if(b->w <= 0)
      return false;
  }
  if(b->y < a.y)
  {
    b->h -= (a.y - b->y);
    b->y += (a.y - b->y);
    if(b->h <= 0)
      return false;
  }
  if(b->x + b->w > a.x + a.w)
  {
    b->w -= (b->x + b->w) - (a.x + a.w);
    if(b->w <= 0)
      return false;
  }
  if(b->y + b->h > a.y + a.h)
  {
    b->h -= (b->y + b->h) - (a.y + a.h);
    if(b->h <= 0)
      return false;
  }
  return true;
}

static inline struct rect sprite_rectangle(const struct sprite *spr)
{
  struct rect sprite_rect = rectangle(spr->x, spr->y, spr->width, spr->height);

  // Multiply out the coords of unbound sprites
  if(!(spr->flags & SPRITE_UNBOUND))
  {
    sprite_rect.x *= CHAR_W;
    sprite_rect.y *= CHAR_H;
  }
  sprite_rect.w *= CHAR_W;
  sprite_rect.h *= CHAR_H;

  return sprite_rect;
}

static inline struct rect collision_rectangle(const struct sprite *spr)
{
  struct rect collision_rect = rectangle(
    spr->col_x,
    spr->col_y,
    spr->col_width,
    spr->col_height
  );

  if(!(spr->flags & SPRITE_UNBOUND))
  {
    collision_rect.x *= CHAR_W;
    collision_rect.y *= CHAR_H;
    collision_rect.w *= CHAR_W;
    collision_rect.h *= CHAR_H;
  }
  return collision_rect;
}

static inline struct rect board_rectangle(struct rect r)
{
  // Get the containing rectangle in board chars of an input rectangle in pixels
  int x1 = r.x;
  int y1 = r.y;
  int x2 = r.x + r.w - 1;
  int y2 = r.y + r.h - 1;

  return rectangle(
    x1 / CHAR_W,
    y1 / CHAR_H,
    x2 / CHAR_W - x1 / CHAR_W + 1,
    y2 / CHAR_H - y1 / CHAR_H + 1
  );
}

boolean sprite_at_xy(struct sprite *spr, int x, int y)
{
  struct rect sprite_rect;
  struct rect pos_rect;

  // If the sprite isn't on, this instantly fails
  if(!(spr->flags & SPRITE_INITIALIZED))
    return false;

  sprite_rect = sprite_rectangle(spr);
  pos_rect = rectangle(x * CHAR_W, y * CHAR_H, CHAR_W, CHAR_H);

  if(rectangle_overlap(sprite_rect, pos_rect))
    return true;
  return false;
}

static inline void get_sprite_tile(struct world *mzx_world,
 const struct sprite *spr, int x, int y, int *ch, int *col)
{
  struct board *cur_board;
  int offset;

  if(spr->flags & SPRITE_VLAYER)
  {
    if(x >= 0 && x < mzx_world->vlayer_width &&
     y >= 0 && y < mzx_world->vlayer_height)
    {
      offset = x + y * mzx_world->vlayer_width;

      *ch = mzx_world->vlayer_chars[offset];
      if(col) *col = mzx_world->vlayer_colors[offset];
    }
    else
    {
      *ch = -1;
      if(col) *col = -1;
    }
  }
  else
  {
    cur_board = mzx_world->current_board;
    if(x >= 0 && x < cur_board->board_width &&
     y >= 0 && y < cur_board->board_height)
    {
      offset = x + y * cur_board->board_width;

      *ch = get_id_char(cur_board, offset);
      if(col) *col = get_id_color(cur_board, offset);
    }
    else
    {
      *ch = -1;
      if(col) *col = -1;
    }
  }

  if(col && !(spr->flags & SPRITE_SRC_COLORS))
    *col = spr->color;
}

static inline boolean collision_char(struct world *mzx_world,
 const struct sprite *spr, char flags, int x, int y)
{
  int ch;
  get_sprite_tile(mzx_world, spr, x, y, &ch, NULL);

  if(ch == -1) return false;

  ch = (ch + spr->offset) % PROTECTED_CHARSET_POSITION;

  if((flags & SPRITE_CHAR_CHECK) && ch != 32) return true;
  if((flags & SPRITE_CHAR_CHECK2) && !is_blank(ch)) return true;
  return false;
}

static inline boolean collision_in(struct world *mzx_world,
 const struct sprite *spr, char flags, struct rect c)
{
  int cx, cy, c2x, c2y;
  //debug("spr collision_in(%d,%d,%d,%d)\n", c.x, c.y, c.w, c.h);

  if(!(flags & (SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2))) return true;
  if((flags & SPRITE_PIXCHECK) == SPRITE_PIXCHECK) return true;

  cx = c.x / CHAR_W + spr->ref_x;
  cy = c.y / CHAR_H + spr->ref_y;
  if(collision_char(mzx_world, spr, flags, cx, cy)) return true;

  c2x = (c.x + c.w - 1) / CHAR_W + spr->ref_x;
  if(c2x != cx && collision_char(mzx_world, spr, flags, c2x, cy)) return true;

  c2y = (c.y + c.h - 1) / CHAR_H + spr->ref_y;
  if(c2y != cy && collision_char(mzx_world, spr, flags, cx, c2y)) return true;

  if((c2x != cx || c2y != cy) &&
   collision_char(mzx_world, spr, flags, c2x, c2y)) return true;

  return false;
}

struct mask
{
  struct rect dim;
  uint8_t *mapping;
  uint8_t *data;
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

static inline int mask_get_pixel(struct world *mzx_world,
 const struct sprite *spr, struct mask m, unsigned int pixel_x, unsigned int pixel_y)
{
  unsigned int ch = (pixel_y / CHAR_H * spr->width + pixel_x / CHAR_W);

  if(!m.mapping[ch])
  {
    uint8_t *output = &m.data[ch * CHAR_SIZE];
    int tcol = spr->transparent_color;
    int x = pixel_x / CHAR_W;
    int y = pixel_y / CHAR_H;
    int chr;
    int col;

    m.mapping[ch] = 1;
    get_sprite_tile(mzx_world, spr, x + spr->ref_x, y + spr->ref_y, &chr, &col);

    if(chr != -1)
    {
      if(get_char_visible_bitmask((chr + spr->offset) % PRO_CH, col, tcol, output))
        m.mapping[ch] = 2;
    }
  }

  if(m.mapping[ch] < 2)
    return 0;

  return m.data[ch * CHAR_SIZE + (pixel_y % CHAR_H)] & (0x80 >> (pixel_x % CHAR_W));
}

static inline boolean collision_pix_in(struct world *mzx_world,
 const struct sprite *spr, struct mask m, struct rect c)
{
  unsigned int px, py;
  int x, y;

  if((spr->flags & SPRITE_PIXCHECK) != SPRITE_PIXCHECK)
    return true;

  // mask_get_pixel relies on these relations being true to speed up its math
  // with unsigned int params. Furthermore, negative ints would break its math.
  assert(c.x >= m.dim.x);
  assert(c.y >= m.dim.y);

  // Pixel by pixel collision check
  for(y = c.y; y < c.y + c.h; y++)
  {
    py = y - m.dim.y;
    for(x = c.x; x < c.x + c.w; x++)
    {
      px = x - m.dim.x;
      if(mask_get_pixel(mzx_world, spr, m, px, py))
        return true;
    }
  }
  return false;
}

static inline boolean collision_pix_between(struct world *mzx_world,
 const struct sprite *spr, struct mask spr_m,
 const struct sprite *targ, struct mask targ_m, struct rect c)
{
  // Determine if there is a collision between two sprites with
  // overlapping chars. Unbound sprites in CCHECK mode 3 require pixel checking.

  if((spr->flags & SPRITE_PIXCHECK) != SPRITE_PIXCHECK)
  {
    // Automatic success, since we know chars are overlapping
    if((targ->flags & SPRITE_PIXCHECK) != SPRITE_PIXCHECK)
      return true;

    // Only target sprite does a pixel check
    return collision_pix_in(mzx_world, targ, targ_m, c);
  }
  else

  if((targ->flags & SPRITE_PIXCHECK) != SPRITE_PIXCHECK)
  {
    // Only the checked sprite does a pixel check
    return collision_pix_in(mzx_world, spr, spr_m, c);
  }

  else
  {
    // Both sprites need a pixel check
    int x, y, sx, sy, tx, ty;

    // Pixel by pixel collision check
    for(y = c.y; y < c.y + c.h; y++)
    {
      sy = y - spr_m.dim.y;
      ty = y - targ_m.dim.y;

      for(x = c.x; x < c.x + c.w; x++)
      {
        sx = x - spr_m.dim.x;
        tx = x - targ_m.dim.x;
        if(mask_get_pixel(mzx_world, spr, spr_m, sx, sy) &&
         mask_get_pixel(mzx_world, targ, targ_m, tx, ty))
          return true;
      }
    }
  }
  return false;
}

int sprite_colliding_xy(struct world *mzx_world, struct sprite *spr,
 int x, int y)
{
  struct sprite collision_sprite = *spr;
  struct sprite *target_spr;
  struct rect sprite_rect;
  struct rect col_rect;
  struct rect col_board_rect;
  struct rect board_rect;
  struct rect board_full_rect;
  struct rect target_spr_rect;
  struct rect target_col_rect;
  struct rect check_rect;
  struct rect check_rect_tr;
  int *collision_list = mzx_world->collision_list;
  int *collisions = &mzx_world->collision_count;
  struct board *cur_board = mzx_world->current_board;
  char *level_id = cur_board->level_id;
  int board_width = cur_board->board_width;
  int bx, by;
  int sprite_idx;
  int cx, cy;
  boolean sprite_collided;
  char target_flags;
  struct mask spr_mask = null_mask();
  struct mask target_mask = null_mask();
  boolean spr_mask_allocated = false;
  boolean target_mask_allocated;

  if(mzx_world->version < V290)
    return sprite_colliding_xy_old(mzx_world, spr, x, y);

  board_rect = rectangle(
    0,
    0,
    cur_board->board_width,
    cur_board->board_height
  );

  board_full_rect = rectangle(
    0,
    0,
    cur_board->board_width * CHAR_W,
    cur_board->board_height * CHAR_H
  );

  collision_sprite.x = x;
  collision_sprite.y = y;

  //debug("sprite_colliding_xy(%d,%d,%d,%d)\n",
  // spr->x, spr->y, spr->width, spr->height);

  if(!(spr->flags & SPRITE_INITIALIZED))
    return -1;

  // Reset the collision list. To match old behaviour, this only happens
  // if the initialisation check passes
  *collisions = 0;

  sprite_rect = sprite_rectangle(&collision_sprite);
  col_rect = collision_rectangle(&collision_sprite);
  col_rect.x += sprite_rect.x;
  col_rect.y += sprite_rect.y;

  if(!(spr->flags & SPRITE_UNBOUND))
    if(!constrain_rectangle(board_full_rect, &col_rect))
      return -1;

  if((spr->flags & SPRITE_PIXCHECK) == SPRITE_PIXCHECK)
  {
    if(!constrain_rectangle(sprite_rect, &col_rect))
      return -1;

    spr_mask = allocate_mask(&collision_sprite);
    spr_mask_allocated = true;
  }

  // Check the contents of the board
  col_board_rect = board_rectangle(col_rect);
  if(constrain_rectangle(board_rect, &col_board_rect))
  {
    for(by = col_board_rect.y; by < col_board_rect.y + col_board_rect.h; by++)
    {
      for(bx = col_board_rect.x; bx < col_board_rect.x + col_board_rect.w; bx++)
      {
        if(level_id[by * board_width + bx] != CUSTOM_BLOCK)
          continue;

        check_rect = rectangle(bx * CHAR_W, by * CHAR_H, CHAR_W, CHAR_H);

        if(!constrain_rectangle(col_rect, &check_rect))
          continue;

        check_rect_tr = check_rect;
        check_rect_tr.x -= sprite_rect.x;
        check_rect_tr.y -= sprite_rect.y;

        if(!collision_in(mzx_world, spr, spr->flags, check_rect_tr))
          continue;

        if(!collision_pix_in(mzx_world, spr, spr_mask, check_rect))
          continue;

        collision_list[(*collisions)++] = -1;
        break;
      }

      if(*collisions)
        break;
    }
  }

  for(sprite_idx = 0; sprite_idx < MAX_SPRITES; sprite_idx++)
  {
    target_spr = mzx_world->sprite_list[sprite_idx];

    if(!(target_spr->flags & SPRITE_INITIALIZED))
      continue;

    if(target_spr == spr)
      continue;

    target_spr_rect = sprite_rectangle(target_spr);
    target_col_rect = collision_rectangle(target_spr);
    //debug("sprite #%d a: %d %d %d %d\n",
    // target_rect.x, target_rect.y, target_rect.w, target_rect.h);
    target_col_rect.x += target_spr_rect.x;
    target_col_rect.y += target_spr_rect.y;

    if(!(target_spr->flags & SPRITE_UNBOUND))
      if(!constrain_rectangle(board_full_rect, &target_col_rect))
        continue;

    // ccheck between bound sprites works by looking at the flags of the
    // sprite doing the collision checking only. This is to maintain backwards
    // compatibility with code that expects this.
    // The reason this is not tied into a version check is to allow old code
    // to continue working even in a new world.
    // If either sprite is unbound we switch to sensible behaviour (using the
    // ccheck of each sprite to determine its respective collision mask).

    if((spr->flags | target_spr->flags) & SPRITE_UNBOUND)
      target_flags = target_spr->flags;
    else
      target_flags = spr->flags;

    // Get the overlapping area of the two sprite collision rectangles.
    // If the collision rectangles aren't overlapping at all, skip this sprite.
    if(!constrain_rectangle(col_rect, &target_col_rect))
      continue;

    // Look closer to see if these sprites are actually colliding.
    target_mask_allocated = false;
    if((target_spr->flags & SPRITE_PIXCHECK) == SPRITE_PIXCHECK)
    {
      // In unbound sprite CCHECK mode 3, we're checking for a collision against
      // the sprite's visible pixels. Make sure the collision is in the sprite's
      // visible area.
      if(!constrain_rectangle(target_spr_rect, &target_col_rect))
        continue;

      target_mask = allocate_mask(target_spr);
      target_mask_allocated = true;
    }

    sprite_collided = false;

    for(cy = col_rect.y; cy < col_rect.y + col_rect.h; cy += CHAR_H)
    {
      for(cx = col_rect.x; cx < col_rect.x + col_rect.w; cx += CHAR_W)
      {
        check_rect = rectangle(cx, cy, CHAR_W, CHAR_H);

        if(!constrain_rectangle(target_col_rect, &check_rect))
          continue;

        check_rect_tr = check_rect;
        check_rect_tr.x -= sprite_rect.x;
        check_rect_tr.y -= sprite_rect.y;

        if(!collision_in(mzx_world, spr, spr->flags, check_rect_tr))
          continue;

        check_rect_tr = check_rect;
        check_rect_tr.x -= target_spr_rect.x;
        check_rect_tr.y -= target_spr_rect.y;

        if(!collision_in(mzx_world, target_spr, target_flags, check_rect_tr))
          continue;

        if(!collision_pix_between(mzx_world, spr, spr_mask,
         target_spr, target_mask, check_rect))
          continue;

        collision_list[(*collisions)++] = sprite_idx;
        sprite_collided = true;
        break;
      }

      // We've already seen a collision with this sprite
      if(sprite_collided)
        break;
    }

    if(target_mask_allocated)
      destroy_mask(target_mask);
  }

  if(spr_mask_allocated)
    destroy_mask(spr_mask);

  return *collisions;
}
