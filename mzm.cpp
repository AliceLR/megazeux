/* $Id: idput.h,v 1.2 1999/01/17 20:35:41 mental Exp $
 * MegaZeux
 *
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

#include <stdlib.h>
#include "mzm.h"
#include "idput.h"
#include "data.h"
#include "vlayer.h"
#include "roballoc.h"

void save_mzm(char far *name, int x, int y, int w, int h, int mode, 
 int mode2)
{
  int i, i2;
  int bwidth, bheight;

  if((mode == 1) && !overlay_mode) return;

  if(mode == 2)
  {
    map_vlayer();
    bwidth = vlayer_width;
    bheight = vlayer_height;
  }
  else
  {
    bwidth = max_bxsiz;
    bheight = max_bysiz;
  }

  // No attempt is made to clip it.. if it runs off the board, it just
  // doesn't do it.
  if(!((x < 0) | (y < 0) | ((x + w) > bwidth) | ((y + h) > bheight)))
  {
    FILE *fp = fopen(name, "wb");
    fprintf(fp, "MZMX");
    fputc(w, fp);
    fputc(h, fp);
    fseek(fp, 16, SEEK_SET);
  
    // mode 0 = from board
    // mode 1 = from overlay
    // mode 2 = from vlayer
  
    if(mode == 0)
    {
      char far *src_id = level_id + (y * max_bxsiz) + x;
      int src_id2 = (y * max_bxsiz) + x;
      char far *src_param = level_param + (y * max_bxsiz) + x;
      char far *src_color = level_color + (y * max_bxsiz) + x;
      char far *src_uid = level_under_id + (y * max_bxsiz) + x;
      char far *src_uparam = level_under_param + (y * max_bxsiz) + x;
      char far *src_ucolor = level_under_color + (y * max_bxsiz) + x;
      int edge_skip = max_bxsiz - w;
      char id;
    
      for(i = 0; i < h; i++)
      {
        for(i2 = 0; i2 < w; i2++)
        {
          // Don't put a new player, robot, pushable robot, scroll, 
          // sign, or sensor
          id = *src_id;
          if(id < 122)
          {
            fputc(*src_id, fp);
            // if mode2 switch, save the char there as param
            if(mode2)
            {
              fputc(get_id_char(src_id2), fp);
            }
            else
            {
              fputc(*src_param, fp);
            }
          }
          else
          {
            // Instead, put a customblock that looks like the thing
            fputc(5, fp);
            fputc(get_id_char(src_id2), fp);
          }
          fputc(*src_color, fp);
          fputc(*src_uid, fp);
          fputc(*src_uparam, fp);
          fputc(*src_ucolor, fp);
          src_id++;
          src_param++;
          src_color++;
          src_uid++;
          src_uparam++;
          src_ucolor++;
          src_id2++;
        }
        src_id += edge_skip;
        src_id2 += edge_skip;
        src_param += edge_skip;
        src_color += edge_skip;
        src_uid += edge_skip;
        src_uparam += edge_skip;
        src_ucolor += edge_skip;
      }
      fclose(fp);
      return;
    }

    char far *src_char;
    char far *src_color;
    int edge_skip = bwidth - w;
  
    if((unsigned int)mode <= 2);
    {
      if(mode == 1)
      {
        src_char = overlay + (y * max_bxsiz) + x;
        src_color = overlay_color + (y * max_bxsiz) + x;
      }
      if(mode == 2)
      {
        src_char = vlayer_chars + (y * vlayer_width) + x;
        src_color = vlayer_colors + (y * vlayer_width) + x;
      }    
    
      for(i = 0; i < h; i++)
      {
        for(i2 = 0; i2 < w; i2++)
        {
          fputc(5, fp);
          fputc(*src_char, fp);
          fputc(*src_color, fp);
          fputc(0, fp);
          fputc(0, fp);
          fputc(0, fp);
          src_char++;
          src_color++;
        }
        src_char += edge_skip;
        src_color += edge_skip;
      }   
    }
    fclose(fp);
  }
  if(mode == 2) unmap_vlayer();
}

void load_mzm(char far *name, int x, int y, int mode)
{
  int i, i2;
  int w, h;

  if((mode == 1) && !overlay_mode) return;

  FILE *fp = fopen(name, "rb");
  if((fgetc(fp) == 'M') && (fgetc(fp) == 'Z') && (fgetc(fp) == 'M') &&
   (fgetc(fp) == 'X'))
  {
    // No attempt is made to clip it.. if it runs off the board, it just
    // doesn't do it.
    w = fgetc(fp);
    h = fgetc(fp);
    fseek(fp, 16, SEEK_SET);
    int bwidth, bheight;

    if(mode == 2)
    {
      map_vlayer();
      bwidth = vlayer_width;
      bheight = vlayer_height;
    }
    else
    {
      bwidth = max_bxsiz;
      bheight = max_bysiz;
    }
    // No attempt is made to clip it.. if it runs off the board, it just
    // doesn't do it.
    if(!((x < 0) | (y < 0) | ((x + w) > bwidth) | ((y + h) > bheight)))
    {
      // mode 0 = to board
      // mode 1 = to overlay
      // mode 2 = to vlayer

      if(mode == 0)
      {
        char far *dest_id = level_id + (y * max_bxsiz) + x;
        char far *dest_param = level_param + (y * max_bxsiz) + x;
        char far *dest_color = level_color + (y * max_bxsiz) + x;
        char far *dest_uid = level_under_id + (y * max_bxsiz) + x;
        char far *dest_uparam = level_under_param + (y * max_bxsiz) + x;
        char far *dest_ucolor = level_under_color + (y * max_bxsiz) + x;
        int edge_skip = max_bxsiz - w;
      
        for(i = 0; i < h; i++)
        {
          for(i2 = 0; i2 < w; i2++)
          {            
            // Don't overwrite the player!
            if(*dest_id != 127)
            {
              *dest_id = fgetc(fp);
            }
            else
            {
              fgetc(fp);
            }
            *dest_param = fgetc(fp);
            *dest_color = fgetc(fp);
            *dest_uid = fgetc(fp);
            *dest_uparam = fgetc(fp);
            *dest_ucolor = fgetc(fp);
            dest_id++;
            dest_param++;
            dest_color++;
            dest_uid++;
            dest_uparam++;
            dest_ucolor++;
          }
          dest_id += edge_skip;
          dest_param += edge_skip;
          dest_color += edge_skip;
          dest_uid += edge_skip;
          dest_uparam += edge_skip;
          dest_ucolor += edge_skip;
        }
        fclose(fp);
        return;
      }
  
      if((unsigned)mode <= 2)
      {     
        char far *dest_char;
        char far *dest_color;
        int edge_skip = bwidth - w;
  
        if(mode == 1)
        {
          dest_char = overlay + (y * max_bxsiz) + x;
          dest_color = overlay_color + (y * max_bxsiz) + x;
        }
        if(mode == 2)
        {
          dest_char = vlayer_chars + (y * vlayer_width) + x;
          dest_color = vlayer_colors + (y * vlayer_width) + x;
        }
      
        for(i = 0; i < h; i++)
        {
          for(i2 = 0; i2 < w; i2++)
          {
            fgetc(fp);
            *dest_char = fgetc(fp);
            *dest_color = fgetc(fp);
            fgetc(fp);
            fgetc(fp);
            fgetc(fp);
            dest_char++;
            dest_color++;
          }
          dest_char += edge_skip;
          dest_color += edge_skip;
        }
      }
    }
  }

  if(mode == 2) unmap_vlayer();

  fclose(fp);
}

