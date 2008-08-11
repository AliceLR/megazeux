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

// Code for setting and retrieving counters and keys.

#include <time.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

#include "counter.h"

#include "getkey.h"
#include "arrowkey.h"
#include "bwsb.h"
#include "egacode.h"
#include "blink.h"
#include "const.h"
#include "cursor.h"
#include "data.h"
#include "game.h"
#include "game2.h"
#include "graphics.h"
#include "idarray.h"
#include "idput.h"
#include "mouse.h"
#include "palette.h"
#include "runrobot.h"
#include "roballoc.h"
#include "sfx.h"
#include "string.h"
#include "struct.h"
#include "conio.h"
// We can mess with sprites now. - Exo
#include "sprite.h"
#include "mstring.h"
// So you can save/load with counters - Exo
#include "saveload.h"
#include "ezboard.h"
#include "mod.h"
#include "palette.h"
#include "vlayer.h"

long off;
char fileio;
char file_in[12];
char file_out[12];
int multiplier;
int divider;
int c_divisions;

// I took away the mzxakversion stuff. So you no longer
// have to set "mzxakversion" to 1. -Koji

//Temp vars for new funcitons
int dir,dirx,diry,loc;

// Date and Time data
struct time t;
struct date d;

void set_built_in_messages(int param);

int player_restart_x=0,player_restart_y=0,gridxsize=1,gridysize=1;
int myscrolledx=0,myscrolledy=0;
//int MZXAKWENDEVERSION = 0;
//Some added variables to speed up pixel editing -Koji
unsigned char pixel_x = 0, pixel_y = 0;
//if (input_file != NULL)
//	fclose(input_file);
//if (output_file != NULL)
//	fclose(output_file);

char was_zapped=0;
extern char mid_prefix;
void prefix_xy(int&fx,int&fy,int&mx,int&my,int&lx,int&ly,int robotx,
               int roboty);

//Get a counter. Include robot id if a robot is running.
int get_counter(char far *name,unsigned char id) 
{
  int t1;

  // They go up here since they're the most important. - Exo

  if(!str_cmp(name,"LOOPCOUNT"))
    return robots[id].loop_count;

  if(!strn_cmp(name, "LOCAL", 5))
  {
    if(*(name + 5) == 0)
    {
      return robots[id].blank;
    }
    if((*(name + 6) == 0) && (version_loaded >= 0x246))
    {
      switch(*(name + 5))
      {
        case '2':
        {
          return(((int)robots[id].walk_dir << 8) | ((unsigned int)robots[id].is_locked));
        }
        case '3':
        {
          return(((int)robots[id].last_touch_dir << 8) | 
           ((unsigned int)robots[id].last_shot_dir));
        }        
      }
    }
  }

  //a few counters convert to var's for speed.
  if (!strn_cmp(name,"CHAR_", 5))
  {
    if (!str_cmp(name + 5, "X"))
    {
      return pixel_x;
    }

    if (!str_cmp(name + 5, "Y"))
    {
      return pixel_y;
    }
    //Reads a single byte of a char -Koji
    if (!str_cmp(name + 5,"BYTE"))
      return *(curr_set + (get_counter("CHAR") * 14) + (get_counter("BYTE") % 14));
  }

  if (!str_cmp(name,"PLAYERDIST")) return(abs(robots[id].xpos-player_x)
                                        + abs(robots[id].ypos-player_y));

  // Char in a string, or string as int representation
  int str_type = string_type(name);
  if(str_type)
  {
    if(str_type == 2)
    {
      return get_str_char(name);
    }
    else
    {
      return get_str_int(name);
    }
  }

  // These mess with some things with math stuff.
  if(!str_cmp(name, "DIVIDER"))
  {
    return divider;
  }
  
  if(!str_cmp(name, "MULTIPLIER"))
  {
    return multiplier;
  }

  if(!str_cmp(name, "C_DIVISIONS"))
  {
    return c_divisions;
  }

  // Trig. - Exo
  if(!strn_cmp(name, "SIN", 3))
  {
    if(c_divisions)
    {
      int sin_return;
      int theta = (int)(strtol(name + 3, NULL, 10)) % c_divisions;
      // Do sin
      asm {
        shl theta, 1
        fild word ptr theta
        fldpi
        fmulp st(1), st(0)
        fidiv word ptr c_divisions

        // If compiling with Borland C/TC++ 3.0 and it doesn't get
        // past here it's because the compiler can't normally cope with
        // 386/387 instructions like this. To get around it, you should
        // turn on "compile through assembler" in the compiler options.
        // This will, however, make you unable to compile other larger
        // modules, so you need to then turn it off when those are compiling.
        fsin
        fimul word ptr multiplier
        fistp word ptr sin_return
      }      
      return(sin_return);
    }
  }

  if(!strn_cmp(name, "COS", 3))
  {
    if(c_divisions)
    {
      int cos_return;
      int theta = (int)(strtol(name + 3, NULL, 10)) % c_divisions;
      // Do cos
      asm {
        shl theta, 1
        fild word ptr theta
        fldpi
        fmulp st(1), st(0)
        fidiv word ptr c_divisions
        fcos
        fimul word ptr multiplier
        fistp word ptr cos_return
      }      
      return(cos_return);
    }
  }

  if(!strn_cmp(name, "TAN", 3))
  {
    if(c_divisions)
    {
      int tan_return;
      int theta = (int)(strtol(name + 3, NULL, 10)) % c_divisions;
      if((theta + (c_divisions >> 2)) % (c_divisions >> 1))
      {
        //return((int)(tan((theta * M_PI * 2)/c_divisions) * multiplier));
        // Do tan
        asm {
          shl theta, 1
          fild word ptr theta
          fldpi
          fmulp st(1), st(0)
          fidiv word ptr c_divisions
          fptan
          fstp st(0)
          fimul word ptr multiplier
          fistp word ptr tan_return
        }      
        return(tan_return);
      }
    }
  }

  if(!strn_cmp(name, "ATAN", 4))
  {
    if(divider)
    {
      int atan_return;
      int val = strtol(name + 4, NULL, 10);
      double const_2pi = 6.2831853071796;
      if((val <= divider) && (val >= -divider))
      {
        asm {
          fild word ptr val
          fidiv word ptr divider
          fld1
          fpatan
          fimul word ptr c_divisions
          fld const_2pi
          fdivp
          fistp word ptr atan_return
        }
        return(atan_return);
      }
    }
  }

  if(!strn_cmp(name, "ASIN", 4))
  {
    if(divider)
    {
      int asin_return;
      int val = strtol(name + 4, NULL, 10);
      double const_2pi = 6.2831853071796;
      if((val <= divider) && (val >= -divider))
      {
        // asin(a) = atan(a/sqrt(1 - a^2))
        asm {
          fild word ptr val         
          fidiv word ptr divider    
          fld st(0)                 // put a in st(1)
          fmul st(0), st(0)         // square a
          fld1                      // load 1
          fsubrp                    // 1 - a^2
          fsqrt                     // take sqrt
          fpatan
          fimul word ptr c_divisions
          fld const_2pi
          fdivp
          fistp word ptr asin_return
        }
        return(asin_return);
      }
    }
  }

  if(!strn_cmp(name, "ACOS", 4))
  {
    if(divider)
    {
      int acos_return;
      int val = strtol(name + 4, NULL, 10);
      double const_2pi = 6.2831853071796;
      if((val <= divider) && (val >= -divider))
      {
        // asin(a) = atan(sqrt(1 - a^2)/a)
        asm {
          fild word ptr val         
          fidiv word ptr divider    
          fld st(0)                 // put a in st(1)
          fmul st(0), st(0)         // square a
          fld1                      // load 1
          fsubrp                    // 1 - a^2
          fsqrt                     // take sqrt
          fxch                      // swap st(0) and st(1)
          fpatan
          fimul word ptr c_divisions
          fld const_2pi
          fdivp
          fistp word ptr acos_return
        }
        return(acos_return);
      }
    }
  }

  // Other math stuff. - Exo

  if(!strn_cmp(name, "SQRT", 4))
  {
    int val = (int)(strtol(name + 4, NULL, 10));
    if(val >= 0)
    {
      int sqrt_return;
      asm {
        fild word ptr val
        fsqrt
        fistp word ptr sqrt_return
      }
      return(sqrt_return);
    }
  }

  if(!strn_cmp(name, "ABS", 3))
  {
    int val = (int)(strtol(name + 3, NULL, 10));
    if(val < 0) val = -val;
    return(val);
  }

  //REAL distance -Koji
  // Can only handle real distances up to 129.
  // Can't seem to help that. -Koji
  // Well, when your function is returning a short int.. - Exo
  // Does anyone want this? No... - Exo

/*  if(!str_cmp(name,"R_PLAYERDIST"))
  {
    long d;
    d = long((robots[id].xpos-player_x)*(robots[id].xpos-player_x));
    d += long((robots[id].ypos-player_y)*(robots[id].ypos-player_y));
    d = sqrt(d) << 8;
    d += 128;
    d >>= 8;
    return d;
  } */

  //Reads a single pixel - Koji
  // I was really tired the night I was trying to make this.
  // and Exophase practically gave me the source
  // I would've made one myself the next day, but it's already done.
  if (!str_cmp(name,"PIXEL"))
  {
    int pixel_mask;
    char sub_x, sub_y, current_byte;
    unsigned char current_char;
    sub_x = pixel_x % 8;
    sub_y = pixel_y % 14;
    current_char = ((pixel_y / 14) * 32) + (pixel_x / 8);
    current_byte = *(curr_set+(current_char*14)+sub_y);
    pixel_mask = (128 >> sub_x);
    return(current_byte & pixel_mask) >> (7 - sub_x);
  }
  //Read binary positions of "INT" ("bit_place" = 0-15) - Koji
  if (!str_cmp(name,"INT2BIN"))
  {
    int place = (get_counter("BIT_PLACE") & 15);
    return((get_counter("INT") >> place) & 1);
  }

  // Prepare for SMZX palette
  if(!str_cmp(name, "SMZX_PALETTE"))
  {
    fileio = 6;
  }

  //Open input_file! -Koji
  if(!strn_cmp(name, "FREAD", 5))
  {
    if(!str_cmp(name + 5, "_OPEN"))
    {
      fileio = 1;
      return 0;
    }

    if(input_file == NULL)
    {
      return(-1);
    }

    //Reads a counter from input_file - Exo
    if(!str_cmp(name + 5, "_COUNTER"))
    {
      return(fgetc(input_file) | fgetc(input_file) << 8);
    }

    if(!str_cmp(name + 5, "_POS"))
    {
      return(ftell(input_file) & 32767);
    }

    if(!str_cmp(name + 5, "_PAGE"))
    {
      return(ftell(input_file) >> 15);
    }

    //Reads from input_file. -Koji
    if(name[5] == 0)
    {
      return fgetc(input_file);
    }
  }

  if(!strn_cmp(name, "FWRITE", 6))
  {
    //opens output_file as normal. -Koji
    if(!str_cmp(name + 6,"_OPEN"))
    {
      fileio = 2;
      return 0;
    }

    // Opens output_file to modify - Exo
    if(!str_cmp(name + 6, "_MODIFY"))
    {
      fileio = 8;
      return 0;
    }

    //Opens output_file as APPEND. -Koji
    if(!str_cmp(name + 6,"_APPEND"))
    {
      fileio = 3;
      return 0;
    }

    if(output_file == NULL) return -1;

    if(!str_cmp(name + 6,"_POS"))
    {
      return(ftell(output_file) & 32767);
    }

    if(!str_cmp(name + 6,"_PAGE"))
    {
      return(ftell(output_file) >> 15);
    }
  }

  // Save game (.sav file)
  if(!str_cmp(name, "SAVE_GAME"))
  {
    fileio = 4;
    return 0;
  }
  // Save world (.mzx file)
  if(!str_cmp(name, "SAVE_WORLD"))
  {
    fileio = 7;
    return 0;
  }
  if(!str_cmp(name, "LOAD_GAME"))
  {
    fileio = 5;
    return 0;
  }

  // Don't think anyone was using these and they're crippled
  // anyway. - Exo

  /*	if(!str_cmp(name,"FREAD_LENGTH"))
    {
      if(input_file == NULL) return -1;
      return filelength(fileno(input_file));
    }

    if(!str_cmp(name,"FWRITE_LENGTH"))
    {
      if(output_file == NULL) return -1;
      return filelength(fileno(output_file));
    } */

    if(!strn_cmp(name, "SPR", 3))
    {
      unsigned char spr_num;
      char far *next = name + 3;
        
      if(!str_cmp(next, "_NUM"))
      {
        return(sprite_num);
      }

      if(!str_cmp(next, "_COLLISIONS"))
      {
        return(collision_list.num);
      }

      if(!strn_cmp(next, "_CLIST", 6))
      {
        spr_num = (int)strtol(next + 6, NULL, 10);
        return(collision_list.collisions[spr_num]);
      }

      spr_num = (unsigned char)strtol(next, &next, 10);

      if(!str_cmp(next, "_X"))
      {
        return(sprites[spr_num].x);
      }

      if(!str_cmp(next, "_Y"))
      {
        return(sprites[spr_num].y);
      }

      if(!str_cmp(next, "_REFX"))
      {
        return(sprites[spr_num].ref_x);
      }

      if(!str_cmp(next, "_REFY"))
      {
        return(sprites[spr_num].ref_y);
      }
    
      if(!str_cmp(next, "_WIDTH"))
      {
        return(sprites[spr_num].width);
      }

      if(!str_cmp(next, "_HEIGHT"))
      {
        return(sprites[spr_num].height);
      }

      if(!strn_cmp(next, "_C", 2))
      {
        next += 2;
        if(!str_cmp(next, "X"))
        {
          return(sprites[spr_num].col_x);
        }
        if(!str_cmp(next, "Y"))
        {
          return(sprites[spr_num].col_y);
        }
        if(!str_cmp(next, "WIDTH"))
        {
          return(sprites[spr_num].col_width);
        }
        if(!str_cmp(next, "HEIGHT"))
        {
          return(sprites[spr_num].col_height);
        }
      }
    }

  //	}if(MZXAKWENDEVERSION == 1)
  if(!strn_cmp(name,"SMZX_", 5))
  {
    if(!str_cmp(name + 5, "MODE"))
    {
  	  return smzx_mode;
    }
    if((name[5] == 'R') || (name[5] == 'r'))
    {
      int c;
      c = strtol(name + 6, NULL, 10) & 255;
      return smzx_mode2_palette[c * 3];
    }
    if((name[5] == 'G') || (name[5] == 'g'))
    {
      int c;
      c = strtol(name + 6, NULL, 10) & 255;
      return smzx_mode2_palette[(c * 3) + 1];
    }
    if((name[5] == 'B') || (name[5] == 'b'))
    {
      int c;
      c = strtol(name + 6, NULL, 10) & 255;
      return smzx_mode2_palette[(c * 3) + 2];
    }
  }

  if(!strn_cmp(name, "DATE_", 5))
  {
    if(!str_cmp(name + 5,"DAY"))
    {
      getdate(&d);
      return d.da_day;
    }
      
    if(!str_cmp(name + 5,"YEAR"))
    {
      getdate(&d);
      return d.da_year;
    }

    if(!str_cmp(name + 5,"MONTH"))
    {
      getdate(&d);
      return d.da_mon;
    }
  }

  if(!strn_cmp(name, "TIME_", 5))
  {
    if(!str_cmp(name + 5,"SECONDS"))
    {
      gettime(&t);
      return t.ti_sec;
    }

    if(!str_cmp(name + 5,"MINUTES"))
    {
      gettime(&t);
      return t.ti_min;
    }

    if(!str_cmp(name + 5,"HOURS"))
    {
      gettime(&t);
      return t.ti_hour;
    }
  }

//	}

  if(!str_cmp(name,"MZX_SPEED"))
    return overall_speed;

  //First, check for robot specific counters
  if(!str_cmp(name,"BULLETTYPE"))
    return robots[id].bullet_type;

  if(!str_cmp(name,"HORIZPLD"))
    return abs(robots[id].xpos-player_x);

  if(!str_cmp(name,"VERTPLD"))
    return abs(robots[id].ypos-player_y);

  if(!strn_cmp(name, "THIS", 4))
  {
    if(!str_cmp(name + 4,"X"))
    {
      if (mid_prefix<2) return robots[id].xpos;
      else if (mid_prefix==2) return robots[id].xpos-player_x;
      else return robots[id].xpos-get_counter("XPOS");
    }

    if(!str_cmp(name + 4,"Y"))
    {
      if (mid_prefix<2) return robots[id].ypos;
      else if (mid_prefix==2) return robots[id].ypos-player_y;
      else return robots[id].ypos-get_counter("YPOS");
    }

    if(!str_cmp(name + 4,"_CHAR")) return robots[id].robot_char;
    
    if(!str_cmp(name + 4,"_COLOR"))
      return level_color[robots[id].ypos * max_bxsiz + robots[id].xpos];
  }

  //Next, check for global, non-standard counters
  if(!str_cmp(name,"INPUT"))      return num_input;
  if(!str_cmp(name,"INPUTSIZE"))    return input_size;
  if(!strn_cmp(name,"KEY",3))
  {
    if(!str_cmp(name + 3, "_PRESSED"))
    {
      return key_get;
    }
    if(!str_cmp(name + 3, "_RELEASE"))
    {
      int r = (unsigned char)key_code - 128;
      if(r < 0)
      {
        return 0;
      }
      else
      {
        return r;
      }
    }
    if(!str_cmp(name + 3, "_CODE"))
    {
      if(key_code < 0)
      {
        return 0;
      }
      else
      {
        return key_code;
      }
    }
    if(name[3] == 0)
    {
      return last_key;
    }
    else
    {
      int n = strtol(name + 3, NULL, 10) & 0x7F;
      return state_table[n];
    }
  }

  if(!str_cmp(name,"SCORE"))      return(unsigned int)score;
  if(!str_cmp(name,"TIMERESET"))    return time_limit;
  if(!str_cmp(name,"PLAYERFACEDIR")) return player_last_dir>>4;
  if(!str_cmp(name,"PLAYERLASTDIR")) return player_last_dir&15;
  // Mouse info Spid
  if(!strn_cmp(name, "MOUSE", 5))
  {
    if(!strn_cmp(name + 5, "_M", 2))
    {
      if(!str_cmp(name + 7, "X"))
      {
        return mmx;
      }
      if(!str_cmp(name + 7, "Y"))
      {
        return mmy;
      }
    }
    if(!str_cmp(name + 5,"X")) return saved_mouse_x;

    if(!str_cmp(name + 5,"Y")) return saved_mouse_y;
  }

  if(!str_cmp(name,"BUTTONS")) return saved_mouse_buttons;
  // scroll_x and scroll_y never work, always return 0. Spid
  if(!strn_cmp(name, "SCROLLED", 8))
  {
    if(!str_cmp(name + 8,"X"))
    {
      calculate_xytop(myscrolledx,myscrolledy);
      return myscrolledx;
    }

    if(!str_cmp(name + 8,"Y"))
    {
      calculate_xytop(myscrolledx,myscrolledy);
      return myscrolledy;
    }
  }

  // Player position Spid
  if(!strn_cmp(name, "PLAYER", 6))
  {
    if(!str_cmp(name + 6,"X")) return player_x;    
    if(!str_cmp(name + 6,"Y")) return player_y;
  }

  if(!strn_cmp(name, "MBOARD", 6))
  {
    if(!str_cmp(name + 6,"X"))
    {
      calculate_xytop(myscrolledx,myscrolledy);
      return(mousex + myscrolledx - viewport_x);
    } 
    if(!str_cmp(name + 6,"Y"))
    {
      calculate_xytop(myscrolledx,myscrolledy);
      return(mousey + myscrolledy - viewport_y);
    }
  }

  // Functions added :by Akwende
  // Adds Time Counters, and access to internal Robot ID

  //	if(MZXAKWENDEVERSION == 1)
  //	{

  if(!strn_cmp(name,"ROBOT_ID", 8))
  {
    if((name[8]) == 0)
    {
      return id;
    }

    if(name[8] == '_')
    {
      int i;
      for(i = 0; i < NUM_ROBOTS; i++)
      {
        if(!str_cmp(robots[i].robot_name, name + 9))
        {
          return(i);
        }
      }
    }
  }

  if(!strn_cmp(name, "RID", 3))
  {
    int i;
    for(i = 0; i < NUM_ROBOTS; i++)
    {
      if(!str_cmp(robots[i].robot_name, name + 3))
      {
        return(i);
      }
    }
  }

  if((name[0] == 'R') || (name[0] == 'r'))
  {
    char far *next;
    unsigned int n = strtol(name + 1, &next, 10);
    if(*next == '.')
    {
      return(get_counter(next + 1, n));
    }
  }

  if(!strn_cmp(name, "OVERLAY_", 8))
  {   
    if(!str_cmp(name + 8,"CHAR"))
    {
      if(overlay_mode == 0) return 0;
      loc = get_counter("OVERLAY_X",id);
      if(loc > board_xsiz)
        return 0;
      loc += (get_counter("OVERLAY_Y",id) * max_bxsiz);
      if(loc > (board_xsiz * board_ysiz))
        return 0;

      return overlay[loc];
    }

    if(!str_cmp(name + 8,"COLOR"))
    {
      if(overlay_mode == 0) return 0;
      loc = get_counter("OVERLAY_X",id);
      if(loc > board_xsiz)
        return 0;

      loc += (get_counter("OVERLAY_Y",id) * max_bxsiz);
      if(loc > (board_xsiz * board_ysiz))
        return 0;
      return overlay_color[loc];
    }

    if(!str_cmp(name + 8,"MODE"))
      return overlay_mode;
  }

  if(!strn_cmp(name, "VLAYER_", 7))
  {
    if(!str_cmp(name + 7, "WIDTH"))
    {
      return vlayer_width;      
    }
    if(!str_cmp(name + 7, "HEIGHT"))
    {
      return vlayer_height;
    }
  }

  if(!strn_cmp(name, "VC", 2))
  {
    int x, y;
    if(translate_coordinates(name + 3, x, y) != -1)
    {
      unsigned char v;
      if(!strn_cmp(name + 2, "H", 1))
      {
        map_vlayer();
        v = *(vlayer_chars + (y * vlayer_width) + x);
        unmap_vlayer();
        return v;
      }
      if(!strn_cmp(name + 2, "O", 1))
      {
        map_vlayer();
        v = *(vlayer_colors + (y * vlayer_width) + x);
        unmap_vlayer();
        return v;
      }
    }
  }

  if(!strn_cmp(name, "BOARD_", 6))
  {
    if(!str_cmp(name + 6,"CHAR"))
    {
      loc = get_counter("BOARD_X",id);
      if(loc > board_xsiz)
        return 0;

      loc += (get_counter("BOARD_Y",id) * max_bxsiz);

      if(loc > (board_xsiz * board_ysiz))
        return 0;

      return get_id_char(loc);
    } 
    
    if(!str_cmp(name + 6,"COLOR"))
    {
      loc = get_counter("BOARD_X",id);

      if(loc > board_xsiz)
        return 0;
      loc += (get_counter("BOARD_Y",id) * max_bxsiz);

      if(loc > (board_xsiz * board_ysiz))
        return 0;
      return level_color[loc];
    }

    if(!str_cmp(name + 6, "W"))
    {
      return(board_xsiz);
    }

    if(!str_cmp(name + 6, "H"))
    {
      return(board_ysiz);
    }
  }

  if(!str_cmp(name,"RED_VALUE"))
  {
    return get_Color_Aspect(get_counter("CURRENT_COLOR",id),0);
  }
  if(!str_cmp(name,"GREEN_VALUE"))
  {
    return get_Color_Aspect(get_counter("CURRENT_COLOR",id),1);
  }
  if(!str_cmp(name,"BLUE_VALUE"))
  {
    return get_Color_Aspect(get_counter("CURRENT_COLOR",id),2);
  }
  //My_target messed with the can_lavawalk local
  //I'll just make it into a new local with that name -Koji
  if(!str_cmp(name,"LAVA_WALK"))
  {
    return robots[id].can_lavawalk;
  }

  // Returns current order of the mod playing - Exo
  if(!str_cmp(name, "MOD_ORDER"))
  {
    if(music_on)
    {
      return(MusicOrder(0xFF));
    }
    else
    {
      return(-1);
    }
  }

  /*
    if(!str_cmp(name,"GET_TARGET_ID"))
    {
      loc = get_counter("BOARD_X",id);

      if(mid_prefix==1)
      {
        loc+= robots[id].xpos;
        loc += ((get_counter("BOARD_Y",id)+ robots[id].ypos) * max_bxsiz);
      }
      else
      {
        if(mid_prefix==2)
        {
          loc+= player_x;
          loc += ((get_counter("BOARD_Y",id)+ player_y) * board_xsiz);
        }
        else
        {
          int temp = get_counter("YPOS");
          loc+= temp;
          loc+= ((get_counter("BOARD_Y",id)+ temp) * board_xsiz);
        }
      }

      if ((level_id[loc] == 123) || (level_id[loc] ==124))
        return level_param[loc];
      else
      return 0;
    }
    if(!str_cmp(name,"TARGET_X"))
    {
      return robots[robots[id].can_lavawalk].xpos;
    }

    if(!str_cmp(name,"TARGET_Y"))
      return robots[robots[id].can_lavawalk].ypos;

    if(!str_cmp(name,"TARGET_XDIR"))
    {
      dir =	robots[robots[id].can_lavawalk].xpos - robots[id].xpos;
      if (dir == 0) return 0;
      return dir/abs(dir);
    }

    if(!str_cmp(name,"TARGET_YDIR"))
    {
      dir =	robots[robots[id].can_lavawalk].ypos - robots[id].ypos;
      if (dir == 0) return 0;
      return dir/abs(dir);
    }

    if(!str_cmp(name,"TARGET_DIST"))
    {
      diry =	robots[robots[id].can_lavawalk].ypos;
      diry -= robots[id].ypos;

      dirx =	robots[robots[id].can_lavawalk].xpos;
      dirx = robots[id].xpos;

      return abs(dirx) + abs(diry);
    }*/
//	}
//Now search counter structs

  for(t1=0;t1<NUM_COUNTERS;t1++)
  if(!str_cmp(name,counters[t1].counter_name)) break;
  if(t1<NUM_COUNTERS) return counters[t1].counter_value;
  return 0;//Not found
}


//Sets the value of a counter. Include robot id if a robot is running.
void set_counter(char far *name,int value,unsigned char id) 
{
  int t1;
//File protocal -Koji
  if(fileio > 0)
  {       //This can be good thing or a bad thing...
    //I'd say it'd be wise to seclude a new game
    //inside a it's own folder to fight against
    //a posible viral threat. -Koji
    // I changed if's to a switch. - Exo

    switch(fileio)
    {
      case 1:
      {
        //read
        if(input_file != NULL)
        {
          fclose(input_file);
          file_in[0] = 0;
        }
        input_file = fopen(name, "rb");
        str_cpy(file_in, name);
        break;
      }

      case 2:
      {
        //write
        if(output_file != NULL)
        {
          fclose(output_file);
          file_out[0] = 0;
        }
        output_file = fopen(name, "wb");
        str_cpy(file_out, name);
        break;
      }

      case 3:
      {
        //Append
        if(output_file != NULL)
        {
          fclose(output_file);
          file_out[0] = 0;
        }
        output_file = fopen(name, "ab");
        str_cpy(file_out, name);
        break;
      }
      case 4:
      {
				//Store current
				store_current();
				//Save game
				save_world(name);
				//Reload current
				select_current(curr_board);
        break;
			}        
      case 5:
      {
        // Load game
        int last_board = curr_board;
  			store_current();
	  		clear_current();
        end_mod();
				if(!load_world(name, 2, 1, 0)) 
        {
          char temp[12];
					if(board_where[curr_board]!=W_NOWHERE)
					  select_current(curr_board);
					else select_current(0);
					send_robot_def(0,10);				  
          str_cpy(temp, mod_playing);
          if(temp[0])
          {
            load_mod(temp);
          }
					if(get_counter("CURSORSTATE", 0) == 0) 
          {
            m_hide();
          }
				}
        else
        {
          load_mod(mod_playing);
          select_current(last_board);
        }
        break;
      }
      // Load SMZX palette
      case 6:
      {    
        load_smzx_palette(name);
        break;
      }
      // Save world.. not .sav file.
      case 7:
      {
				int temp = robots[id].cur_prog_line;
				// First set the current line to 0
				robots[id].cur_prog_line = 0;
				//Store current
				store_current();
				//Save game
				save_world(name, 0, 0);
				//Reload current
				select_current(curr_board);
				robots[id].cur_prog_line = temp;
        break;
			}  
      case 8:
      {
        //Append
        if(output_file != NULL)
        {
          fclose(output_file);
          file_out[0] = 0;
        }
        output_file = fopen(name, "r+b");
        str_cpy(file_out, name);
        break;
      }
    }

    fileio = 0;
    return;
  }

  // Moved to the top because they're vastly more important. - Exo

  if(!str_cmp(name,"LOOPCOUNT"))
  {
    robots[id].loop_count=value;
    return;
  }

  if(!strn_cmp(name, "LOCAL", 5))
  {
    if(*(name + 5) == 0)
    {
      robots[id].blank=value;
      return;
    }
    if((*(name + 6) == 0) && (version_loaded >= 0x246))
    {
      switch(*(name + 5))
      {
        case '2':
        {
          robots[id].walk_dir = (unsigned char)(value >> 8);
          robots[id].is_locked = (unsigned char)(value & 0xFF);
          return;
        }
        case '3':
        {
          robots[id].last_touch_dir = (unsigned char)(value >> 8);
          robots[id].last_shot_dir = (unsigned char)(value & 0xFF);
          return;
        }

      }
    }
  }

  // Char in a string - Exo
  if(string_type(name) == 2)
  {
    set_str_char(name, value);
    return;
  }        

  //Ok, ok. I did steal the "pos" within "page" idea from Akwende.
  //It is a great idea to be able to read more than 32767 bytes.
  // The pages are limited to 256, so The max size of a file is
  // about 8MB
  //	-Koji
  // I changed all of these so they worked more accurately and now pages 
  // can go to 32767 too. So you may have (2^15)*(2^15) large files, or
  // 2^30 bytes, which is 1GB. Of course DOS will probably crap out before
  // then. - Exo

  if(!strn_cmp(name, "FREAD", 5))
  {
    if(input_file != NULL)
    {
      if(!str_cmp(name + 5,"_POS"))
      {
        if(value == -1)
        {
          fseek(input_file, 0, SEEK_END);
          return;
        }
        value &= 32767;
        fseek(input_file, ((long)get_counter("FREAD_PAGE") << 15) + value, SEEK_SET);
        return;
      }

      if(!str_cmp(name + 5,"_PAGE"))
      {
        value &= 32767;
        fseek(input_file, ((long)value << 15) + (get_counter("FREAD_POS")), SEEK_SET);
        return;
      }
    }
  }

  if(!strn_cmp(name, "FWRITE", 6))
  {
    if(output_file != NULL)
    {
      if(!str_cmp(name + 6,"_POS"))
      {
        if(value == -1)
        {
          fseek(output_file, 0, SEEK_END);
          return;
        }
        value &= 32767;
        fseek(output_file, ((long)get_counter("FWRITE_PAGE") << 15) + value, SEEK_SET);
        return;
      }

      if(!str_cmp(name + 6,"_PAGE"))
      { 
        value &= 32767;
        fseek(output_file, ((long)value << 15) + (get_counter("FWRITE_POS")), SEEK_SET);
        return;
      }

      // Writes a counter to the ouput_file. - Exo
      if(!str_cmp(name + 6, "_COUNTER"))
      {
        fputc(value & 255, output_file);
        fputc(value << 8, output_file);
        return;
      }

      if(name[6] == '\0')
      {
        fputc(value & 255, output_file);
        return;
      }
    }
  }

  if(!strn_cmp(name, "CHAR_", 5))
  {
    if(!str_cmp(name + 5,"X"))
    {
      pixel_x = value;
      return;
    }
    if(!str_cmp(name + 5,"Y"))
    {
      pixel_y = value;
      return;
    }

    //Writes to a single byte of a char -koji
    if(!str_cmp(name + 5,"BYTE"))
    {
      *(curr_set + (get_counter("CHAR") * 14) + (get_counter("BYTE") % 14)) = value;
      ec_need_update();
      return;
    }
  }

  //Writes to a pixel -Koji
  if(!str_cmp(name,"PIXEL"))
  {
    char sub_x, sub_y, current_byte;
    unsigned char current_char;
    pixel_x &= 255;
    pixel_y %= 112;
    sub_x = pixel_x & 7;
    sub_y = pixel_y % 14;
    current_char = ((pixel_y / 14) << 5) + (pixel_x >> 3);
    current_byte = ec_read_byte(current_char, sub_y);

    // Changed to a switch. - Exo

    switch(value)
    {
      case 0:
      {
        //clear
        current_byte &= ~(128 >> sub_x);
        *(curr_set + (current_char * 14) + (sub_y % 14)) = (current_byte & 255);
        break;
      }
      case 1:
      {
        //set
        current_byte |= (128 >> sub_x);
        *(curr_set + (current_char * 14) + (pixel_y % 14)) = (current_byte & 255);
        break;
      }
      case 2:
      {
        //toggle
        current_byte ^= (128 >> sub_x);
        *(curr_set + (current_char * 14) + (pixel_y % 14)) = (current_byte % 256);
        break;
      }
    }
    ec_need_update();
    return;
  }

  // These mess with some things with math stuff. - Exo
  if(!str_cmp(name, "DIVIDER"))
  {
    divider = value;
    return;
  }
  
  if(!str_cmp(name, "MULTIPLIER"))
  {
    multiplier = value;
    return;
  }

  if(!str_cmp(name, "C_DIVISIONS"))
  {
    c_divisions = value;
    return;
  }


  //Writes a single bit to a given bit place of "INT" -Koji
  // Inmate did crazy stuff here.. I changed it. - Exo
  //(bit place = 0-15)
  if(!str_cmp(name,"INT2BIN"))
  {
    unsigned int integer;
    int place;
    place = (get_counter("BIT_PLACE") & 15);
    integer = get_counter("INT");
    switch(value & 2)
    {
      case 0:
      {
        //clear
        integer &= ~(1 << place);
        break;
      } 
   
      case 1:
      {
        //set
        integer |= (1 << place);
        break;
      } 

      case 2:
      {
        //toggle
        integer ^= (1 << place);
        break;
      }
    }
    set_counter("INT", integer);
    return;
  }

  //First, check for robot specific counters
  /*	if(!str_cmp(name,"MZXAKVERSION"))
    {
    MZXAKWENDEVERSION = value % 2;
    return;
  }
*/
//Functions added by Akwende
//ABS Value Function
//	if(MZXAKWENDEVERSION == 1)
//	{

// Replaced by different methods - Exo
/*  if(!str_cmp(name,"ABS_VALUE"))
  {
    if (value < 0 ) value = value * -1;
    set_counter("VALUE",value,id);
    return;
  }
  if(!str_cmp(name,"SQRT_VALUE"))
  {
    set_counter("VALUE",sqrt(value),id);
    return;
  } */
//Akwende tried to make another overlay... Tried! -koji
/*	if(!str_cmp(name,"OVERLAY_MODE_4"))
  {
    overlay_mode = 4;
  }
*/

	if(!strn_cmp(name, "SMZX_", 5))
  {
    if(!str_cmp(name + 5, "MODE"))
    {
      smzx_mode = (char)value % 3;
    
      if(smzx_mode)
      {
        init_smzx_mode();
      }
      else
      {
      	default_EGA_hardware_pal[6] = 20;
        m_deinit();
        ega_14p_mode();
        cursor_off();
        blink_off();
        m_init();
        ec_update_set();
        update_palette(0);       
        reinit_palette();
        m_show();
      }    
      return;
    }
    if((name[5] == 'R') || (name[5] == 'r'))
    {
      int c = strtol(name + 6, NULL, 10);
      smzx_mode2_palette[c * 3] = char(value & 63);
      if(smzx_mode == 2)
        update_smzx_color((unsigned char)c);
      return;
    }
    if((name[5] == 'G') || (name[5] == 'g'))
    {
      int c = strtol(name + 6, NULL, 10);
      smzx_mode2_palette[(c * 3) + 1] = char(value & 63);
      if(smzx_mode == 2)
        update_smzx_color((unsigned char)c);
      return;
    }
    if((name[5] == 'B') || (name[5] == 'b'))
    {
      int c = strtol(name + 6, NULL, 10);
      smzx_mode2_palette[(c * 3) + 2] = char(value & 63);
      if(smzx_mode == 2)
        update_smzx_color((unsigned char)c);
      return;
    }
  }

  // Commands per cycle - Exo
  if(!str_cmp(name, "COMMANDS"))
  {
    commands = value;
    return;
  }

  if(!strn_cmp(name, "VLAYER_", 7))
  {
    if(!str_cmp(name + 7, "WIDTH"))
    {
      vlayer_width = value;
      vlayer_height = 32768 / vlayer_width;
      return;
    }
    if(!str_cmp(name + 7, "HEIGHT"))
    {
      vlayer_height = value;
      vlayer_width = 32768 / vlayer_height;
      return;
    }
  }

  if(!strn_cmp(name, "VC", 2))
  {
    int x, y;
    if(translate_coordinates(name + 3, x, y) != -1)
    {
      if(!strn_cmp(name + 2, "H", 1))
      {
        map_vlayer();
        *(vlayer_chars + (y * vlayer_width) + x) = value;
        unmap_vlayer();
        return;
      }
      if(!strn_cmp(name + 2, "O", 1))
      {
        map_vlayer();
        *(vlayer_colors + (y * vlayer_width) + x) = value;
        unmap_vlayer();
        return;
      }
    }
  }

  if(!strn_cmp(name, "BOARD_", 6))
  {
    if(!str_cmp(name + 6, "PARAM"))
    {
      level_param[(get_counter("BOARD_Y", id) * max_bxsiz) +
       get_counter("BOARD_X", id)] = value;
      return;
    }

    if(!str_cmp(name + 6, "ID"))
    {
      level_id[(get_counter("BOARD_Y", id) * max_bxsiz) +
       get_counter("BOARD_X", id)] = value;
      return;
    }
  }

  if(!str_cmp(name,"RED_VALUE"))
  {
    value = value % 64;
    set_Color_Aspect(get_counter("CURRENT_COLOR",id),0,value);
    return;
  }
  if(!str_cmp(name,"GREEN_VALUE"))
  {
    value = value % 64;
    set_Color_Aspect(get_counter("CURRENT_COLOR",id),1,value);
    return;
  }
  if(!str_cmp(name,"BLUE_VALUE"))
  {
    value = value % 64;
    set_Color_Aspect(get_counter("CURRENT_COLOR",id),2,value);
    return;
  }
  /*              //Silly Akwende we already have a variable that does
      //This. It's called "bullettype" -Koji
      if(!str_cmp(name,"BULLET_TYPE"))
      {
        robots[id].bullet_type=value;
        return;
      }
  */              //New Lava_walk local counter. -Koji
  if(!str_cmp(name,"LAVA_WALK"))
  {
    robots[id].can_lavawalk=value;
    return;
  }
//	}

  //I'm suprised Akwende didn't do this...
  //MZX_SPEED can now be changed by robots.
  //Meaning the game speed can now be set
  //by robots. -Koji
  if(!str_cmp(name,"MZX_SPEED"))
  {
    overall_speed=value;
    return;
  }

  // Sprite stuff - Exo
  if(!strn_cmp(name, "SPR", 3))
  {
    char far *next = name + 3;
    unsigned char spr_num;
        
    if(!str_cmp(next, "_YORDER"))
    {
      sprite_y_order = value;
      return;
    }
    if(!str_cmp(next, "_NUM"))
    {
      sprite_num = value;
      return;
    }

    spr_num = (unsigned char)strtol(next, &next, 10);   

    if(!str_cmp(next, "_CLIST"))
    {
      if(value)
      {
        sprite_colliding_xy(spr_num, sprites[spr_num].x, sprites[spr_num].y);
      }
      return;
    }

    if(!str_cmp(next, "_SETVIEW"))
    {
      int sx, sy;
      scroll_x=scroll_y=0;
      calculate_xytop(sx, sy);
      scroll_x = 
       (sprites[spr_num].x + (sprites[spr_num].width >> 1)) - 
       (viewport_xsiz >> 1) - sx;
      scroll_y = 
       (sprites[spr_num].y + (sprites[spr_num].height >> 1)) - 
       (viewport_ysiz >> 1) - sy;
      return;
    }
  
    if(!str_cmp(next, "_SWAP"))
    {
      Sprite spr_temp;
      mem_cpy((char *)&spr_temp, (char *)&(sprites[spr_num]), 
       sizeof(Sprite));
      mem_cpy((char *)&(sprites[spr_num]), 
       (char *)&(sprites[value]), sizeof(Sprite));
      mem_cpy((char *)&(sprites[value]), (char *)&spr_temp, 
       sizeof(Sprite));
    }

    if(!str_cmp(next, "_X"))
    {
      sprites[spr_num].x = value;
      return;
    }
  
    if(!str_cmp(next, "_Y"))
    {
      sprites[spr_num].y = value;
      return;
    }
  
    if(!str_cmp(next, "_REFX"))
    {
      sprites[spr_num].ref_x = value;
      return;
    }
  
    if(!str_cmp(next, "_REFY"))
    {
      sprites[spr_num].ref_y = value;
      return;
    }
      
    if(!str_cmp(next, "_WIDTH"))
    {
      sprites[spr_num].width = value;
      return;
    }
  
    if(!str_cmp(next, "_HEIGHT"))
    {
      sprites[spr_num].height = value;
      return;
    }
  
    if((!str_cmp(next, "_OVERLAID")) || (!str_cmp(next, "_OVERLAY")))
    {
      if(value)
      {
        sprites[spr_num].flags |= SPRITE_OVER_OVERLAY;
      }
      else
      {
        sprites[spr_num].flags &= ~SPRITE_OVER_OVERLAY;
      }
      return;
    }
      
    if(!str_cmp(next, "_OFF"))
    {
      if(value)
      {
        sprites[spr_num].flags &= ~SPRITE_INITIALIZED;
      }
      return;
    }
  
    if(!str_cmp(next, "_CCHECK"))
    {
      if(value)
      {
        if(value == 2)
        {
          sprites[spr_num].flags |= SPRITE_CHAR_CHECK2;
        }
        sprites[spr_num].flags |= SPRITE_CHAR_CHECK;
      }
      else
      {
        sprites[spr_num].flags &= ~SPRITE_CHAR_CHECK;
      }
      return;
    }
  
    if(!str_cmp(next, "_STATIC"))
    {
      if(value)
      {
        sprites[spr_num].flags |= SPRITE_STATIC;
      }
      else
      {
        sprites[spr_num].flags &= ~SPRITE_STATIC;
      }
      return;
    }
  
    if(!str_cmp(next, "_VLAYER"))
    {
      if(value)
      {
        sprites[spr_num].flags |= SPRITE_VLAYER;
      }
      else
      {
        sprites[spr_num].flags &= ~SPRITE_VLAYER;
      }
      return;
    }

    if(!strn_cmp(next, "_C", 2))
    {
      next += 2;
      if(!str_cmp(next, "X"))
      {
        sprites[spr_num].col_x = value;
        return;
      }
      if(!str_cmp(next, "Y"))
      {
        sprites[spr_num].col_y = value;
        return;
      }
      if(!str_cmp(next, "WIDTH"))
      {
        sprites[spr_num].col_width = value;
        return;
      }
      if(!str_cmp(next, "HEIGHT"))
      {
        sprites[spr_num].col_height = value;
        return;
      }
    }      
  }

  //Took out the Number constraints. -Koji

  if(!str_cmp(name,"BULLETTYPE"))
  {
    robots[id].bullet_type=value;
    return;
  }
  //Next, check for global, non-standard counters

  // SHOULD allow instant scrolling of screen, once scroll_x can be referenced
  // As of yet, doesn't work. Spid
  // And if it doesn't work then we don't want it, do we..? - Exo

/* if(!str_cmp(name,"SCROLLEDX")) {
   scroll_x= value;
   return;
   }
  if(!str_cmp(name,"SCROLLEDY")) {
   scroll_y= value;
   return;
   }
*/

/*	 // These Grids will be used for mouse buttons, once that is in. Spid
  if(!str_cmp(name,"GRIDXSIZE")) {
    gridxsize=value;
    return;
    }
  if(!str_cmp(name,"GRIDYSIZE")) {
    gridysize=value;
    return;
    }
*/      // Shows or hides the hardware cursor Spid
  if(!str_cmp(name,"CURSORSTATE"))
  {
    if(value==1)  m_show();
    if(value==0)  m_hide();
  }   //Don't return! We need to set the counter, as well.
  // CURSORSTATE is referenced in GAME.CPP
  // This should PROBABLY be a variable, instead of a counter,
  // but I DID have a reason for using a counter, if I ever
  // remember it. Spid
  if(!str_cmp(name,"BIMESG"))
  {
    set_built_in_messages(value);
    return;
  }
  //Don't return! Spid
  if(!strn_cmp(name, "MOUSE", 5))
  {
    if(!str_cmp(name + 5, "X"))
    {
      if (value>79) value=79;
      if (value<0) value=0;
      saved_mouse_y = value;
      m_move(value,mousey);
    }

    if(!str_cmp(name + 5,"Y"))
    {
      if (value>24) value=24;
      if (value<0) value=0;
      saved_mouse_x = value;
      m_move(mousex,value);
    }
  }

  if(!str_cmp(name,"INPUT"))
  {
    num_input=value;
    return;
  }

  if(!str_cmp(name,"INPUTSIZE"))
  {
    if (value>255) value=255;
    input_size=(unsigned char)value;
    return;
  }

  if(!str_cmp(name,"KEY"))
  {
    if (value>255) value=255;
    last_key=(unsigned char)value;
    return;
  }

  if(!str_cmp(name,"SCORE"))
  {
    score=value;
    return;
  }

  if(!str_cmp(name,"TIMERESET"))
  {
    time_limit=value;
    return;
  }

  if(!str_cmp(name,"PLAYERFACEDIR"))
  {
    if (value<0) value=0;
    if (value>3) value=3;
    player_last_dir=(player_last_dir&15)+(value<<4);
    return;
  }

  if(!str_cmp(name,"PLAYERLASTDIR"))
  {
    if (value<0) value=0;
    if (value>4) value=4;
    player_last_dir=(player_last_dir&240)+value;
    return;
  }

  //Check for overflow on HEALTH and LIVES
  if(!str_cmp(name,"HEALTH"))
  {
    if(value<0) value=0;
    if(value>health_limit)
      value=health_limit;
  }

  if(!str_cmp(name,"LIVES"))
  {
    if(value<0) value=0;
    if(value>lives_limit)
      value=lives_limit;
  }

  //Invinco
  if(!str_cmp(name,"INVINCO"))
  {
    if(!get_counter("INVINCO",0))
      saved_pl_color=*player_color;
    else if (value==0)
    {
      clear_sfx_queue();
      play_sfx(18);
      *player_color=saved_pl_color;
    }
  }

  //Now search counter structs
  if(!str_cmp(name,"CURSORSTATE"))
  {
    if (value==1)  m_show();
    if (value==0)  m_hide();
  }   //Don't return! We need to set the counter, as well.

  for(t1=0;t1<NUM_COUNTERS;t1++)
    if(!str_cmp(name,counters[t1].counter_name)) break;
  if(t1<NUM_COUNTERS)
  {
    if(t1<RESERVED_COUNTERS)//Reserved counters can't go below 0
    if(value<0) value=0;
    counters[t1].counter_value=value;
    return;
  }
  //Not found- search for an empty space AFTER the reserved spaces

  for(t1=RESERVED_COUNTERS;t1<NUM_COUNTERS;t1++)
    if(!counters[t1].counter_value) break;
  if(t1<NUM_COUNTERS)
  {//Space found
    str_cpy(counters[t1].counter_name,name);
    counters[t1].counter_value=value;
  }
  return;
}

  //Take a key. Returns non-0 if none of proper color exist.
char take_key(char color)
{
  int t1;
  color&=15;
  for(t1=0;t1<NUM_KEYS;t1++)
  if(keys[t1]==color) break;
  if(t1<NUM_KEYS)
  {
    keys[t1]=NO_KEY;
    return 0;
  }
  return 1;
}

  //Give a key. Returns non-0 if no room.
char give_key(char color) 
{
  int t1;
  color&=15;
  for(t1=0;t1<NUM_KEYS;t1++)
    if(keys[t1]==NO_KEY) break;
  if(t1<NUM_KEYS)
  {
    keys[t1]=color;
    return 0;
  }
  return 1;
}

//Increase or decrease a counter.
void inc_counter(char far *name,int value,unsigned char id) 
{
  //long t;
  // Shouldn't need long. - Exo
  int t;

  // This is a special case because fread/write_pos is set offset
  // from the beginning of the file + the page offset.. well, if
  // it gets increased beyond 32767 a wraparound occurs and it
  // becomes negative, causing a NEGATIVE page.. not good.
  // These inc/dec from seek_cur instead (current position)
  // getting straight to the point. - Exo

  if(!str_cmp(name, "FREAD_POS"))
  {
    fseek(input_file, value, SEEK_CUR);
    return;
  }
  if(!str_cmp(name, "FWRITE_POS"))
  {
    fseek(output_file, value, SEEK_CUR);
    return;
  }
  
  // Rawrr... it gets wrapped around later, always - Exo
  /* if(!str_cmp(name,"CHAR_X"))
  {
    pixel_x = (pixel_x + value) % 256;
    return;
  }
  if(!str_cmp(name,"CHAR_Y"))
  {
    pixel_y = (pixel_y + value) % 256;
    return;
  } */
  if(!str_cmp(name,"SCORE"))
  {//Special score inc. code
    if ((score+value)<score) score=4294967295ul;
    else score+=value;
    return;
  }
  // What is this for, anyway? Who added this? - Exo
  /* if(!str_cmp(name,"WRAP"))
  {
    int current,max;
    current = get_counter("WRAP",id) + value;
    max = get_counter("WRAPVAL",id);
    if (current > max) current = 0;
    set_counter("WRAP",current,id);
  } */
  t=(get_counter(name,id))+value;
  // This was not only unnecessary, but incorrect - Exo
  //if(t>32767L) t=-32767L;
  //if(t<-32768L) t=32768L;
  set_counter(name,(int)t,id);
  }

void dec_counter(char far *name,int value,unsigned char id) 
{
  //long t;
  int t;

  // Read the explanation in inc_counter() for these. Negated
  // Because it's decrementing. - Exo
  if(!str_cmp(name, "FREAD_POS"))
  {
    fseek(input_file, -value, SEEK_CUR);
    return;
  }
  if(!str_cmp(name, "FWRITE_POS"))
  {
    fseek(output_file, -value, SEEK_CUR);
    return;
  }

  // NO. - Exo
  /* if(!str_cmp(name,"CHAR_X"))
  {
    pixel_x = (pixel_x - value + 256) % 256;
    return;
  }
  if(!str_cmp(name,"CHAR_Y"))
  {
    pixel_y = (pixel_y - value + 256) % 256;
    return;
  } */
  if((!str_cmp(name,"HEALTH"))&&(value>0))
  {//Prevent hurts if invinco
    if (get_counter("INVINCO",0)>0) return;
    send_robot_def(0,13);
    if (restart_if_zapped)
    {
      //Restart since we were hurt
      if ((player_restart_x!=player_x)||(player_restart_y!=player_y))
      {
        id_remove_top(player_x,player_y);
        player_x=player_restart_x;
        player_y=player_restart_y;
        id_place(player_x,player_y,127,0,0);
        was_zapped=1;
      }
    }
  }
  if(!str_cmp(name,"SCORE"))
  {//Special score dec. code
    if ((score-value)>score) score=0;
    else score-=value;
    return;
  }

  // Mrrm.. - Exo
  /* if(!str_cmp(name,"WRAP"))
  {
    int current,max;
    current = get_counter("WRAP",id);
    current -= value;
    max = get_counter("WRAPVAL",id);
    if (current < 0) current = max;
    set_counter("WRAP",current,id);
  } */

  t=(get_counter(name,id))-value;
  //if (t>32767L) t=-32767L;
  //if (t<-32768L) t=32768L;
  set_counter(name,(int)t,id);
}

int translate_coordinates(char far *src, int &x, int &y)
{
  char far *next;

  if(*src == 0) return -1;  

  x = strtol(src, &next, 10);
  if(*next == ',')
  {
    y = strtol(next + 1, NULL, 10);
    return 0;
  }
  else
  {
    return -1;
  }
}
