/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 2002 Gilead Kutnick - exophase@adelphia.net
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
#include "egacode.h"
#include "vlayer.h"
#include <dos.h>
#include <stdlib.h>
                                                                                                          
unsigned char sprite_num;
unsigned char total_sprites;                                   
char sprite_y_order;
Sprite sprites[MAX_SPRITES];
Collision_list collision_list;

void plot_sprite(int color, unsigned char sprite, int x, int y)
{
  if(!((sprites[sprite].width) || (sprites[sprite].height)))
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

// For the qsort.

int compare_spr(const void *a, const void *b);

int compare_spr(const void *a, const void *b)
{
  char spr_a = *((char *)a);
  char spr_b = *((char *)b);
  return ((sprites[spr_a].y + sprites[spr_a].col_y) -
   (sprites[spr_b].y + sprites[spr_b].col_y));
}

void draw_sprites()
{
  int i, i2, i3, i4, i5, i6, st_x, st_y, of_x, of_y, d;
  int skip, skip2, skip3;
  int dr_w, dr_h, ref_x, ref_y, scr_x, scr_y;
  int bwidth, use_chars = 1;
  int uvlayer = 0;
  unsigned char dr_order[MAX_SPRITES];
  unsigned char ch, color, dcolor;
  char far *screen = (char far *)MK_FP(current_pg_seg, 0);
  char far *src_chars;
  char far *src_colors;

  // see if y sort should be done
  if(sprite_y_order)
  {
    // Switched to stdlib qsort. Hooray!

    // Fill it with the active sprites in the beginning
    // and the inactive sprites in the end.
    for(i = 0, i2 = 0, i3 = MAX_SPRITES - 1; i < MAX_SPRITES; i++)
    {
      if(sprites[i].flags & SPRITE_INITIALIZED)
      {
        dr_order[i2] = i;
        i2++;
      }
      else
      {
        dr_order[i3] = i;
        i3--;
      }
    }
    
    // Now sort it, using qsort.

    qsort(dr_order, i2, 1, compare_spr);
  }  

  // draw this on top of the SCREEN window.
  for(i = 0; i < MAX_SPRITES; i++)
  {
    if(sprite_y_order)
    {
      d = dr_order[i];
    }
    else
    {
      d = i;
    }

    if(!(sprites[d].flags & SPRITE_INITIALIZED)) continue;

    calculate_xytop(scr_x, scr_y);
    ref_x = sprites[d].ref_x;
    ref_y = sprites[d].ref_y;
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

    if(sprites[d].flags & SPRITE_VLAYER)
    {
      use_chars = 0;
      if(!uvlayer)
      {
        map_vlayer();
      }
      uvlayer = 1;
      use_chars = 1;
      bwidth = vlayer_width;
      src_chars = vlayer_chars;
      src_colors = vlayer_colors;
    }
    else
    {
      use_chars = 0;
      bwidth = max_bxsiz;
      src_chars = level_param;
      src_colors = level_color;
    }

    i4 = ((ref_y + of_y) * bwidth) + ref_x + of_x;
    i5 = (st_y * 80) + st_x;

    if(overlay_mode == 2)
    {
      i6 = ((sprites[d].y - scr_y) * max_bxsiz) + (sprites[d].x - scr_x);
    }
    else
    {
      i6 = (sprites[d].y * max_bxsiz) + sprites[d].x;
    }

    skip = bwidth - dr_w;
    skip2 = 80 - dr_w;
    skip3 = max_bxsiz - dr_w;

    switch((sprites[d].flags & 0x0C) >> 2)
    {
      // natural colors, over overlay
      case 3: // 11
      {
        for(i2 = 0; i2 < dr_h; i2++)
        {
          for(i3 = 0; i3 < dr_w; i3++)
          {
            color = src_colors[i4];
            if(use_chars)
            {
              ch = src_chars[i4];
            }
            else
            {
              ch = get_id_char(i4);
            }
                    
            if(!(color & 0xF0))
            {
              color = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if(ch != 32) 
            {
              if(!(sprites[d].flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
              {
                draw_char_linear(color, ch, i5, current_pg_seg);
              }
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
        for(i2 = 0; i2 < dr_h; i2++)
        {
          for(i3 = 0; i3 < dr_w; i3++)
          {
            color = src_colors[i4];
            if(use_chars)
            {
              ch = src_chars[i4];
            }
            else
            {
              ch = get_id_char(i4);
            }

            if(!(color & 0xF0))
            {
              color = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if((!overlay_mode || (overlay[i6] == 32)) && ch != 32)
            {
              if(!(sprites[d].flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
              {
                draw_char_linear(color, ch, i5, current_pg_seg);
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
            if(use_chars)
            {
              ch = src_chars[i4];
            }
            else
            {
              ch = get_id_char(i4);
            }
        
            dcolor = color;
            if(!(color & 0xF0))
            {
              dcolor = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if(ch != 32)
            {
              if(!(sprites[d].flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
              {
                draw_char_linear(dcolor, ch, i5, current_pg_seg);
              }
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
        color = sprites[d].color;

        for(i2 = 0; i2 < dr_h; i2++)
        {
          for(i3 = 0; i3 < dr_w; i3++)
          {
            if(use_chars)
            {
              ch = src_chars[i4];
            }
            else
            {
              ch = get_id_char(i4);
            }

            dcolor = color;
            if(!(color & 0xF0))
            {
              dcolor = (color & 0x0F) | (screen[(i5 << 1) + 1] & 0xF0);
            }
    
            if((!overlay_mode || (overlay[i6] == 32)) && ch != 32)
            {
              if(!(sprites[d].flags & SPRITE_CHAR_CHECK2) || !is_blank(ch))
              {
                draw_char_linear(dcolor, ch, i5, current_pg_seg);
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
        break;
      }
    }
  }    

  if(uvlayer)
  {
    unmap_vlayer();
  }
}


int sprite_at_xy(unsigned char sprite, int x, int y)
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
                                                            
int sprite_colliding_xy(unsigned char sprite, int x, int y)
{
  int colliding = 0;
  int skip, skip2, i, i2, i3, i4, i5;
  int bwidth;
  int x1, x2, y1, y2;
  int mw, mh;
  int use_chars = 1, use_chars2 = 1;
  unsigned int x_lmask, x_gmask, y_lmask, y_gmask, xl, xg, yl, yg, wl, hl, wg, hg;

  if(!(sprites[sprite].flags & SPRITE_INITIALIZED)) return(-1);
  
  // Check against the background, will only collide against customblock for now
  //  (id 5)

  if(sprites[sprite].flags & SPRITE_VLAYER)
  {
    map_vlayer();
    use_chars = 0;
    bwidth = vlayer_width;
  }
  else
  {
    bwidth = max_bxsiz;
  }

  i3 = ((y + sprites[sprite].col_y) * max_bxsiz) + x + sprites[sprite].col_x;
  i4 = ((sprites[sprite].ref_y + sprites[sprite].col_y) * bwidth);
	i4 += sprites[sprite].ref_x + sprites[sprite].col_x;
  skip = max_bxsiz - sprites[sprite].col_width;
  skip2 = bwidth - sprites[sprite].col_width;

  // Scan board area
  int board_collide = 0;

  for(i = 0; i < sprites[sprite].col_height; i++)
  {
    for(i2 = 0; i2 < sprites[sprite].col_width; i2++)
    {
      // First, if ccheck is on, it won't care if the source is 32
      int c;

      if(!use_chars)
      {
        c = vlayer_chars[i4];
      }
      else
      {
        c = get_id_char(i4);
      }
      if(!(sprites[sprite].flags & SPRITE_CHAR_CHECK) ||
       (c != 32))
      {
        // if ccheck2 is on and the char is blank, don't trigger.
        if(!(sprites[sprite].flags & SPRITE_CHAR_CHECK2) ||
         (!is_blank(c)))
        {
          if(level_id[i3] == 5)
          {
            // Colliding against background
            if(board_collide) break;
            collision_list.collisions[colliding] = -1;
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
        x1 = sprites[i].ref_x + (xg - x1) + sprites[i].col_x;
        y1 = sprites[i].ref_y + (yg - y1) + sprites[i].col_y;
        x2 = sprites[sprite].ref_x + (xg - x2) + sprites[sprite].col_x;
        y2 = sprites[sprite].ref_y + (yg - y2) + sprites[sprite].col_y;
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

        if(sprites[i].flags & SPRITE_VLAYER)
        {
          if(!use_chars) map_vlayer();
          use_chars2 = 0;
          i4 = (y1 * vlayer_width) + x1;
          skip = vlayer_width - mw;
        }
        else
        {
          i4 = (y1 * max_bxsiz) + x1;
          skip = max_bxsiz - mw;
        }

        i5 = (y2 * bwidth) + x2;
        skip2 = bwidth - mw;

        // If ccheck mode 2 is on it should do a further strength check.
        if(sprites[sprite].flags & SPRITE_CHAR_CHECK2)
        {
          for(i2 = 0; i2 < mh; i2++)
          {
            for(i3 = 0; i3 < mw; i3++)
            {
              unsigned char c1, c2; 
              if(use_chars2)
              {
                c1 = get_id_char(i4);
              }
              else
              {
                c1 = vlayer_chars[i4];
              }
              if(use_chars)
              {
                c2 = get_id_char(i5);
              }
              else
              {
                c2 = vlayer_chars[i5];
              }
              if((c1 != 32) && (c2 != 32))
              {
                // Collision.. maybe.
                // We still have to see if both of the chars aren't
                // blank.  
                if(!(is_blank(c1) || is_blank(c2)))
                {
                  collision_list.collisions[colliding] = i;
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
              unsigned char c1, c2;
              if(use_chars2)
              {
                c1 = get_id_char(i4);
              }
              else
              {
                c1 = vlayer_chars[i4];
              }
              if(use_chars)
              {
                c2 = get_id_char(i5);
              }
              else
              {
                c2 = vlayer_chars[i5];
              }
              if((c1 != 32) && (c2 != 32))
              {
                // Collision!
                collision_list.collisions[colliding] = i;
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
        collision_list.collisions[colliding] = i;
        colliding++;
      }

      next:
    }
  }

  if(!(use_chars && use_chars2))
  {
    unmap_vlayer();
  }

  collision_list.num = colliding;

  return(colliding);
}

int is_blank(unsigned char c)
{
  int far *cp = (int far *)(curr_set + (c * 14));  
  int blank = 0, i;

  for(i = 0; i < 7; i++)
  {
    blank |= *cp;
    cp++;
  }

  return !blank;
}
    

     
