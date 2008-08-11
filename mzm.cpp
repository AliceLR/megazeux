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
#include "world.h"

void save_mzm(World *mzx_world, char *name, int x, int y, int w,
 int h, int mode, int mode2)
{
  Board *src_board = mzx_world->current_board;
  int i, i2;
  int bwidth, bheight;
  char *src_char;
  char *src_color;
  int edge_skip;

  if((mode == 1) && !src_board->overlay_mode) return;

  if(mode == 2)
  {
    bwidth = mzx_world->vlayer_width;
    bheight = mzx_world->vlayer_height;
  }
  else
  {
    bwidth = src_board->board_width;;
    bheight = src_board->board_height;
  }

  edge_skip = bwidth - w;

  // No attempt is made to clip it.. if it runs off the board, it just
  // doesn't do it.
  if(!((x < 0) | (y < 0) | ((x + w) > bwidth) | ((y + h) > bheight)))
  {
    FILE *fp = fopen((const char *)name, "wb");
    fprintf(fp, "MZMX");
    fputc(w, fp);
    fputc(h, fp);
    fseek(fp, 16, SEEK_SET);

    // mode 0 = from board
    // mode 1 = from overlay
    // mode 2 = from vlayer

    if(mode == 0)
    {
      char *src_id =
       src_board->level_id + (y * bwidth) + x;
      int src_id2 = (y * bwidth) + x;
      char *src_param =
       src_board->level_param + (y * bwidth) + x;
      char *src_color =
       src_board->level_color + (y * bwidth) + x;
      char *src_uid =
       src_board->level_under_id + (y * bwidth) + x;
      char *src_uparam =
       src_board->level_under_param + (y * bwidth) + x;
      char *src_ucolor =
       src_board->level_under_color + (y * bwidth) + x;
      int edge_skip = bwidth - w;
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
              fputc(get_id_char(src_board, src_id2), fp);
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
            fputc(get_id_char(src_board, src_id2), fp);
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

    if(mode <= 2)
    {
      if(mode == 1)
      {
        src_char = src_board->overlay + (y * bwidth) + x;
        src_color = src_board->overlay_color + (y * bwidth) + x;
      }
      else

      if(mode == 2)
      {
        src_char = mzx_world->vlayer_chars +
         (y * mzx_world->vlayer_width) + x;
        src_color = mzx_world->vlayer_colors +
         (y * mzx_world->vlayer_width) + x;
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
}

void load_mzm(World *mzx_world, char *name, int x, int y, int mode)
{
  Board *src_board = mzx_world->current_board;
  int i, i2;
  int w, h;

  if((mode == 1) && !src_board->overlay_mode) return;

  FILE *fp = fopen(name, "rb");

  if(fp && (fgetc(fp) == 'M') && (fgetc(fp) == 'Z') &&
   (fgetc(fp) == 'M') && (fgetc(fp) == 'X'))
  {
    int bwidth, bheight;

    // No attempt is made to clip it.. if it runs off the board, it just
    // doesn't do it.
    w = fgetc(fp);
    h = fgetc(fp);
    fseek(fp, 16, SEEK_SET);

    if(mode == 2)
    {
      bwidth = mzx_world->vlayer_width;
      bheight = mzx_world->vlayer_height;
    }
    else
    {
      bwidth = src_board->board_width;
      bheight = src_board->board_height;
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
        char *dest_id =
         src_board->level_id + (y * bwidth) + x;
        char *dest_param =
         src_board->level_param + (y * bwidth) + x;
        char *dest_color =
         src_board->level_color + (y * bwidth) + x;
        char *dest_uid =
         src_board->level_under_id + (y * bwidth) + x;
        char *dest_uparam =
         src_board->level_under_param + (y * bwidth) + x;
        char *dest_ucolor =
         src_board->level_under_color + (y * bwidth) + x;
        int edge_skip = bwidth - w;

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

      if(mode <= 2)
      {
        char *dest_char;
        char *dest_color;
        int edge_skip = bwidth - w;

        if(mode == 1)
        {
          dest_char = src_board->overlay + (y * bwidth) + x;
          dest_color = src_board->overlay_color + (y * bwidth) + x;
        }
        else

        if(mode == 2)
        {
          dest_char = mzx_world->vlayer_chars +
           (y * bwidth) + x;
          dest_color = mzx_world->vlayer_colors +
           (y * bwidth) + x;
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

    fclose(fp);
  }
}

