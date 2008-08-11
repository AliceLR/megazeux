/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2002 B.D.A - Koji_takeo@worldmailer.com
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

// sprite.cpp, by Exophase

// Global declarations:

#include "data.h"
#include "sprite.h"
#include "graphics.h"
#include "idput.h"
#include "error.h"
#include "game.h"
#include <dos.h>
                                                                                                          
char sprite_num;
char total_sprites;                                   
char sprite_y_order;
Sprite sprites[64];
Collision_list collision_list;

void plot_sprite(int color, int sprite, int x, int y)
{
  if(((x + sprites[sprite].width > board_xsiz) ||
   (y + sprites[sprite].height > board_ysiz)) ||
    !((sprites[sprite].width) || (sprites[sprite].height)))
  {
    return;
  }

  sprites[sprite].x = x;
  sprites[sprite].y = y;
  if(color == 288)
  {
    sprites[sprite].flags |= SPRITE_SRC_COLORS;
  }
  else
  {
    sprites[sprite].flags &= ~SPRITE_SRC_COLORS;
    sprites[sprite].color = (char)color;
  }
   
  sprites[sprite].flags |= SPRITE_INITIALIZED;
  total_sprites++;
}

void draw_sprites()
{
  int i, i2, i3, i4, i5, i6, st_x, st_y, of_x, of_y, d;
  int drawn = 0;
  int skip, skip2;
  int dr_w, dr_h, ref_x, ref_y, scr_x, scr_y;
  char dr_order[64], temp, cur_y;
  char ch, color, dcolor;

  // see if y sort should be done
  if(sprite_y_order)
  {
    // This is a bad merge sort, but the set is small..

    // sort order...
    int pos;

    // first fill it with the actual orders.
    for(i = 0; i < 64; i++)
    {
      dr_order[i] = i;
    }
    
    // Now start sort.

    for(pos = 0, cur_y = sprites[pos].y; pos < 64; pos++)
    {
      if(!(sprites[dr_order[pos]].flags & SPRITE_INITIALIZED))
      {
        continue;
      }
      cur_y = sprites[dr_order[pos]].y + sprites[dr_order[pos]].col_y;
      for(i = pos; i < 64; i++)
      {
        // Did we find a smaller one?
        if((sprites[dr_order[i]].y + sprites[dr_order[i]].col_y) < cur_y)
        {
          // Then swap.
          temp = dr_order[pos];
          dr_order[pos] = dr_order[i];
          dr_order[i] = temp;
        }
      }
    }
  }  

  // draw this on top of the SCREEN window.
  for(i = 0; i < 64; i++)
  {
    //if(drawn == total_sprites) break;
    if(sprite_y_order)
    {
      d = dr_order[i];
    }
    else
    {
      d = i;
    }

    if(!(sprites[d].flags & SPRITE_INITIALIZED)) continue;

    drawn++;

    calculate_xytop(scr_x, scr_y);
    ref_x = sprites[d].ref_x;
    ref_y = sprites[d].ref_y;
    char far *screen = (char far *)MK_FP(current_pg_seg, 0);
    of_x = 0;
    of_y = 0;
    st_x = (sprites[d].x + viewport_x);
    st_y = (sprites[d].y + viewport_y);
    if(!(sprites[d].flags & SPRITE_STATIC))
    {
      st_x -= scr_x;
      st_y -= scr_y;
    }

    dr_w = sprites[d].width;
    dr_h = sprites[d].height;
      
    // clip draw position against viewport

    if(st_x < viewport_x)
    {
      of_x = viewport_x - st_x;
      dr_w -= of_x;
      if(dr_w < 1) continue;

      st_x = viewport_x;
    }
    if(st_y < viewport_y)
    {
      of_y = viewport_y - st_y;
      dr_h -= of_y;
      if(dr_h < 1) continue;

      st_y = viewport_y;
    }
    if((st_x > (viewport_x + viewport_xsiz)) || 
     (st_y > (viewport_y + viewport_ysiz))) continue;
    if((st_x + dr_w) > (viewport_x + viewport_xsiz))
    {
      dr_w = viewport_x + viewport_xsiz - st_x;
    }
    if((st_y + dr_h) > (viewport_y + viewport_ysiz))
    {
      dr_h = viewport_y + viewport_ysiz - st_y;
    }
      
    i4 = ((ref_y + of_y) * max_bxsiz) + ref_x + of_x;
    i5 = (st_y * 80) + st_x;
    i6 = (sprites[d].y * max_bxsiz) + sprites[d].x;

    skip = max_bxsiz - dr_w;
    skip2 = 80 - dr_w;

    switch((sprites[d].flags & 0x0C) >> 2)
    {
      // natural colors, over overlay
      case 3: // 11
      {
        for(i2 = 0; i2 < dr_h; i2++)
        {
          for(i3 = 0; i3 < dr_w; i3++)
          {
            color = level_color[i4];
            ch = get_id_char(i4);
        
            if(!(color & 0xF0))
            {
              color = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if(ch != 32)
            {
              draw_char_linear(color, ch, i5, current_pg_seg);
            }
            i4++;
            i5++;
            i6++;
          }
          i4 += skip;
          i5 += skip2;
          i6 += skip;
        }
        break;
      }

      // natural colors, under overlay
      case 2: // 10
      {
        for(i2 = 0; i2 < dr_h; i2++)
        {
          for(i3 = 0; i3 < dr_w; i3++)
          {
            color = level_color[i4];
            ch = get_id_char(i4);

            if(!(color & 0xF0))
            {
              color = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if((!overlay_mode || (overlay[i6] == 32)) && ch != 32)
            {
              draw_char_linear(color, ch, i5, current_pg_seg);
            }
            i4++;
            i5++;
            i6++;
          }
          i4 += skip;
          i5 += skip2;
          i6 += skip;
        }
        break;
      }

      // given colors, over overlay
      case 1: // 01
      {
        color = sprites[d].color;

        for(i2 = 0; i2 < dr_h; i2++)
        {
          for(i3 = 0; i3 < dr_w; i3++)
          {
            ch = get_id_char(i4);
        
            dcolor = color;
            if(!(color & 0xF0))
            {
              dcolor = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if(ch != 32)
            {
              draw_char_linear(dcolor, ch, i5, current_pg_seg);
            }
            i4++;
            i5++;
            i6++;
          }
          i4 += skip;
          i5 += skip2;
          i6 += skip;
        }
        break;
      }

      // given colors, under overlay
      case 0: // 00
      {
        color = sprites[d].color;

        for(i2 = 0; i2 < dr_h; i2++)
        {
          for(i3 = 0; i3 < dr_w; i3++)
          {
            ch = get_id_char(i4);

            dcolor = color;
            if(!(color & 0xF0))
            {
              //dcolor = (color & 0x0F) | (level_color[i6] & 0xF0);
              dcolor = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if((!overlay_mode || (overlay[i6] == 32)) && ch != 32)
            {
              draw_char_linear(dcolor, ch, i5, current_pg_seg);
            }
            i4++;
            i5++;
            i6++;
          }
          i4 += skip;
          i5 += skip2;
          i6 += skip;
        }
        break;
      }
    }
  }    
}


int sprite_at_xy(unsigned int sprite, int x, int y)
{
  if((x >= sprites[sprite].x) && (x < sprites[sprite].x + sprites[sprite].width) &&
   (y >= sprites[sprite].y) && (y < sprites[sprite].y + sprites[sprite].height)
   && (sprites[sprite].flags & SPRITE_INITIALIZED))
  {
    return(1);
  }
  else
  {
    return(0);
  }
}
                                                            
int sprite_colliding_xy(unsigned int sprite, int x, int y)
{
  int colliding = 0;
  int skip, i, i2, i3, i4, i5;
  int x1, x2, y1, y2;
  int mw, mh;
  unsigned int x_lmask, x_gmask, y_lmask, y_gmask, xl, xg, yl, yg, wl, hl, wg, hg;

  if(!(sprites[sprite].flags & SPRITE_INITIALIZED)) return(-1);
  
  // Check against the background, will only collide against customblock for now
  //  (id 5)

  i3 = ((y + sprites[sprite].col_y) * max_bxsiz) + x + sprites[sprite].col_x;
  i4 = ((sprites[sprite].ref_y + sprites[sprite].col_y) * max_bxsiz) +
   sprites[sprite].ref_x + sprites[sprite].col_x;
  skip = max_bxsiz - sprites[sprite].col_width;

  // Scan board area
  for(i = 0; i < sprites[sprite].col_height; i++)
  {
    for(i2 = 0; i2 < sprites[sprite].col_width; i2++)
    {
      if(level_id[i3] == 5)
      {
        // found a collision...
        // Do char based check?
        if(sprites[sprite].flags & SPRITE_CHAR_CHECK)
        {
          // Don't mark collision if the sprite's char there is 32.
          if(get_id_char(i4) != 32)
          {
            colliding++;
            collision_list.collisions[0] = -1;
            goto next;
          }
        }
        else
        {
          colliding++;
          collision_list.collisions[0] = -1;
          goto next;
        }
      }
      i3++;
      i4++;
    }
    i3 += skip;
    i4 += skip;
  }

  // Yeah, I feel dirty.

  next:

  for(i = 0; i < 64; i++)
  {
    if((i == sprite) || !(sprites[i].flags & SPRITE_INITIALIZED)) continue;
    if(!(sprites[i].col_width) || !(sprites[i].col_height)) continue;
    x1 = sprites[i].x + sprites[i].col_x;
    x2 = x + sprites[sprite].col_x;
    y1 = sprites[i].y + sprites[i].col_y;
    y2 = y + sprites[sprite].col_y;
    x_lmask = (signed int)(x1 - x2) >> 15;
    x_gmask = ~x_lmask;
    y_lmask = (signed int)(y1 - y2) >> 15;
    y_gmask = ~y_lmask;
    xl = (x1 & x_lmask) | (x2 & x_gmask);
    xg = (x1 & x_gmask) | (x2 & x_lmask);
    yl = (y1 & y_lmask) | (y2 & y_gmask);
    yg = (y1 & y_gmask) | (y2 & y_lmask);
    wl = (sprites[i].col_width & x_lmask) | 
     (sprites[sprite].col_width & x_gmask);
    hl = (sprites[i].col_height & y_lmask) | 
     (sprites[sprite].col_height & y_gmask);
    if((((xg - xl) - wl) & ((yg - yl) - hl)) >> 15)
    {
      // Does it require char/char verification?
      if(sprites[sprite].flags & SPRITE_CHAR_CHECK)
      {
        // The sub rectangle is going to be somewhere within both sprites;
        // It's going to start at the beginning of the component further
        // along (xg/yg) which is an absolute board position; find the 
        // offset both sprites are from this... and you will iterate in a
        // rectangle there that is (xl + wl) - xg... the difference between
        // where the first one ends and the second one begins
        mw = (xl + wl) - xg;
        mh = (yl + hl) - yg;
        // Reuse these.. offsets. For both you must look at PHYSICAL data, that
        // is, where the chars of the sprite itself is.
        x1 = sprites[i].ref_x + (xg - x1);
        y1 = sprites[i].ref_y + (yg - y1);
        x2 = sprites[sprite].ref_x + (xg - x2);
        y2 = sprites[sprite].ref_y + (yg - y2);
        // Check to make sure draw area doesn't go over.
        wg = (sprites[i].col_width & x_gmask) | 
         (sprites[sprite].col_width & x_lmask);
        hg = (sprites[i].col_height & y_gmask) | 
         (sprites[sprite].col_height & y_lmask);
        if(mw > wg)
        {
          mw = wg;
        }
        if(mh > hg)
        {
          mh = hg;
        }

        // Now iterate through the rect; if both are NOT 32 then a collision is
        // flagged.
        i4 = (y1 * max_bxsiz) + x1;
        i5 = (y2 * max_bxsiz) + x2;
        skip = max_bxsiz - mw;
        for(i2 = 0; i2 < mh; i2++)
        {
          for(i3 = 0; i3 < mw; i3++)
          {
            if((get_id_char(i4) != 32) && (get_id_char(i5) != 32))
            {
              // Collision!
              collision_list.collisions[colliding] = i;
              colliding++;             

              goto next_2;
            }
            i4++;
            i5++;
          }
          i4 += skip;
          i5 += skip;
        }
      }
      else
      {
        collision_list.collisions[colliding] = i;
        colliding++;
      }

      next_2:
    }
  }

  collision_list.num = colliding;

  return(colliding);
}
    

     
