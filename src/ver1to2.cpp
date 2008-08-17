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

/* Converts a version 1.0? .MZX or .MZB file to a version 2.00 file. */

#include <stdio.h>
#include <dir.h>
#include <string.h>
#include <alloc.h>

void convert(char far *fn);
void rle_to_rle2(unsigned char huge *&source,unsigned char huge *&dest);
long convert_board(unsigned char huge *loc);//Returns size
void convert_robot_line(unsigned char far *source,unsigned char far *dest);
extern char far *cmd_formats[256];

//Destination for board conversion
unsigned char huge *newboard;//Must allocate 210000 bytes

void main(int argc,char *argv[]) {
  int t1;
  printf("\n\n");
  newboard=(unsigned char huge *)farmalloc(210000L);
  if(newboard==NULL) {
    printf("ERROR: Out of memory\n\n\a");
    return;
    }
  if(argc<2) {
    printf("VER1TO2.EXE converts MegaZeux worlds (.MZX) and boards (.MZB) from version\n");
    printf("1.00g, 1.01, 1.02, or 1.03 of MegaZeux to version 2.00 of MegaZeux. It is\n");
    printf("run from the command line as follows:\n\n");
    printf("VER1TO2 file.ext [file.ext ...]\n\n");
    printf("VER1TO2 can accept multiple files and they may include wildcards. The files\n");
    printf("do not all have to be the same type. Corrupted files, non-board/non-world\n");
    printf("files, and version 2.00 files will all be skipped over with an error. Note\n");
    printf("that VER1TO2 verifies the extension of the file, so the file must have the\n");
    printf("proper extension of .MZX or .MZB.\n\n\a");
    return;
    }
  //Do files
  for(t1=1;t1<argc;t1++)
    convert(argv[t1]);
  //Done
  printf("\nAll files converted!\n\n");
  farfree(newboard);
}

unsigned char far magic_code[16]="\xe6\x52\xeb\xf2\x6d\x4d\x4a\xb7\x87\xb2\x92\x88\xde\x91\x24";
unsigned char far id_dmg[128]={
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,100,0,0,0,0,0,
  0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,10,0,10,5,5,
  0,0,0,0,0,0,0,0,0,0,0,10,10,0,10,10,
  10,0,0,10,10,10,10,10,10,10,10,10,0,0,10,10,
  0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0 };
unsigned char far palette[48]={
  0,0,0,0,0,42,0,42,0,0,42,42,
  42,0,0,42,0,42,42,21,0,42,42,42,
  21,21,21,21,21,63,21,63,21,21,63,63,
  63,21,21,63,21,63,63,63,21,63,63,63 };
unsigned char far global_robot[43]={
  2,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,1,0,0,1,
  0,0,0,1,0,0,0,0,0,0,0,0,
  0,0,1,0,0,255,0 };

long orig_board_size[150];
long orig_board_location[150];
long new_board_stat_ptr[150];

void convert(char far *fn) {
  int t1,t2;
  char type=1;//1=board, 0=world
  char temp_name[13];
  FILE *source;
  FILE *dest;
  unsigned char huge *cboard=NULL;

  //Verify extension
  t2=1;//Problem
  for(t1=0;t1<strlen(fn);t1++)
    if(fn[t1]=='.') break;
  if(t1<strlen(fn)) {
    if(!stricmp(&fn[++t1],"MZX")) type=t2=0;
    else if(!stricmp(&fn[t1],"MZB")) {
      type=1;
      t2=0;
      }
    }
  if(t2) {
    printf("ERROR: File is not a .MZB file or a .MZX file (%s)\n",fn);
    return;
    }
  //Make sure it exists
  source=fopen(fn,"rb+");
  if(source==NULL) {
    printf("ERROR: Non-existant file- %s\n",fn);
    return;
    }
  //Verify file type
  if(type) {
    //Verify board file
    fseek(source,0,SEEK_SET);
    t1=fgetc(source);
    if(t1==255) {
      fclose(source);
      printf("ERROR: File is already a version 2.00 or greater .MZB file (%s)\n",fn);
      return;
      }
    //Our only way of file type verification for older .MZB files-
    //   Make sure x/y sizes are 1-100
    //   Make sure next byte is NOT a 0
    if((t1>100)||(t1==0)) {
      fclose(source);
      printf("ERROR: File is not a .MZB file or a .MZX file (%s)\n",fn);
      return;
      }
    t1=fgetc(source);
    if((t1>100)||(t1==0)) {
      fclose(source);
      printf("ERROR: File is not a .MZB file or a .MZX file (%s)\n",fn);
      return;
      }
    if(fgetc(source)==0) {
      fclose(source);
      printf("ERROR: File is not a .MZB file or a .MZX file (%s)\n",fn);
      return;
      }
    }
  else {
    fseek(source,25,SEEK_SET);
    t1=fgetc(source);
    if(t1) fseek(source,16,SEEK_CUR);//Skip over pw
    if(fgetc(source)!='M') type=1;
    if(fgetc(source)!='Z') type=1;
    t1=fgetc(source);
    if(((t1>='2')&&(t1<='9'))) {
      printf("ERROR: File is already a version 2.00 or greater .MZX file (%s)\n",fn);
      fclose(source);
      return;
      }
    if(t1!='X') type=1;
    if(type) {
      fclose(source);
      printf("ERROR: File is not a .MZB file or a .MZX file (%s)\n",fn);
      return;
      }
    }
  //We now know whether it is a .MZB or .MZX file, and that it is the
  //right version.
  fclose(source);
  //Rename to a temp file
  strcpy(temp_name,"~V1TO2XXXXXX");
  mktemp(temp_name);
  rename(fn,temp_name);
  //Open both files
  dest=fopen(fn,"wb+");
  if(dest==NULL) {
    printf("ERROR: Error opening file for output (%s)\n",fn);
    rename(temp_name,fn);
    return;
    }
  source=fopen(temp_name,"rb");
  if(source==NULL) {
    fclose(dest);
    printf("ERROR: Error opening file for input (%s, originally %s)\n",
      temp_name,fn);
    rename(temp_name,fn);
    return;
    }
  //Everything is ready!
  printf("Converting %s file %s... ",type?"board":"world",fn);
  switch(type) {
    case 0:
      //World file conversion
int _cpt;
#define copy_bytes(num) { for(_cpt=0;_cpt<num;_cpt++) fputc(fgetc(source),dest); }

      int pro_method,pw_size;
      unsigned char password[16];

      copy_bytes(25);//Title
      fputc(pro_method=fgetc(source),dest);//Protection method
      if(pro_method) {//Password
        pw_size=fgetc(source);
        fread(password,1,15,source);
        //First, normalize password...
        for(t1=0;t1<15;t1++) {
          password[t1]^=magic_code[t1];
          password[t1]-=0x12+pro_method;
          password[t1]^=0x8D;
          }
        password[pw_size]=0;
        for(t1=strlen(password);t1<16;t1++)
          password[t1]=0;
        //Write the new password
        for(t1=0;t1<15;t1++) {
          t2=password[t1]^0x8D;
          t2+=pro_method+0x12;
          t2^=magic_code[t1];
          fputc(t2,dest);
          }
        }
      fseek(source,3,SEEK_CUR);
      fwrite("MZ2",1,3,dest);//Code
      copy_bytes(3584);//Character set
      copy_bytes(34);//Idchars, before goop
      fputc(176,dest);
      fgetc(source);//Idchars, goop
      copy_bytes(271);//Idchars, after goop

      //We WILL be writing the rest of the Idchars.
      //First, we need to read the entire remainder into variables.

      unsigned char status_counters[60];
      unsigned char bullet_chars[4];
      unsigned char player_char;
      unsigned char edge_color,player_color,bullet_color,missile_color;
      unsigned char first_board,eg_board,die_board;
      int eg_x,eg_y,die_x,die_y;
      int lives,lives_limit,health,health_limit;
      int nme_hurt_nme,gosfx;

      fread(status_counters,1,60,source);
      fread(bullet_chars,1,4,source);
      player_char=fgetc(source);
      fseek(source,3,SEEK_CUR);//Skip saved player position
      edge_color=fgetc(source);
      player_color=fgetc(source);
      bullet_color=fgetc(source);
      missile_color=fgetc(source);
      first_board=fgetc(source);
      eg_board=fgetc(source);
      die_board=fgetc(source);
      eg_x=fgetc(source);
      eg_y=fgetc(source);
      die_x=fgetc(source);
      die_y=fgetc(source);
      fread(&lives,2,1,source);
      fread(&lives_limit,2,1,source);
      fread(&health,2,1,source);
      fread(&health_limit,2,1,source);
      nme_hurt_nme=fgetc(source);
      gosfx=fgetc(source);
      fgetc(source);//Blank spot

      //Source is at # of boards. Now we complete the Idchars, inserting
      //proper info. Then spew remainder of info as well.

      for(t1=0;t1<3;t1++)
        fwrite(bullet_chars,1,4,dest);//Bullet char arrays x3
      for(t1=0;t1<4;t1++)
        fputc(player_char,dest);//Player char x4
      fputc(player_color,dest);
      fputc(missile_color,dest);
      for(t1=0;t1<3;t1++)
        fputc(bullet_color,dest);//Bullet color x3
      fwrite(id_dmg,1,128,dest);//Id dmg portion

      //Stuff after idchars...

      fwrite(status_counters,1,60,dest);
      for(t1=0;t1<30;t1++)
        fputc(0,dest);//Blank status counters x2
      fputc(edge_color,dest);
      fputc(first_board,dest);
      if(eg_board==128) eg_board=255;
      fputc(eg_board,dest);
      if(die_board>=128) die_board=383-die_board;//128 -> 255, 129 -> 254
      fputc(die_board,dest);
      fwrite(&eg_x,2,1,dest);
      fwrite(&eg_y,2,1,dest);
      fputc(gosfx,dest);
      fwrite(&die_x,2,1,dest);
      fwrite(&die_y,2,1,dest);
      fwrite(&lives,2,1,dest);
      fwrite(&lives_limit,2,1,dest);
      fwrite(&health,2,1,dest);
      fwrite(&health_limit,2,1,dest);
      fputc(nme_hurt_nme,dest);
      fputc(0,dest);//Clear projectiles (no)
      fputc(0,dest);//Only from swap world (no)
      fwrite(palette,1,48,dest);

      //Now we need to store the position of the global robot.
      //We'll reserve space, save the location, and wait...

      long glrob_pos_ptr=ftell(dest);
      fwrite("\0\0\0\0",1,4,dest);

      //Now get and copy number of boards...

      int num_boards=fgetc(source);
      fputc(num_boards,dest);

      //...copy names of boards...

      copy_bytes(num_boards*25);

      //...and get old board sizes and positions. Also reserve dest
      //space for the new stats at this time.

      for(t1=0;t1<num_boards;t1++) {
        fread(&orig_board_size[t1],4,1,source);
        fread(&orig_board_location[t1],4,1,source);
        new_board_stat_ptr[t1]=ftell(dest);
        fwrite("\0\0\0\0\0\0\0\0",1,8,dest);
        }

      //Global robot...

      long glrob_pos=ftell(dest);
      fseek(dest,glrob_pos_ptr,SEEK_SET);
      fwrite(&glrob_pos,4,1,dest);
      fseek(dest,glrob_pos,SEEK_SET);
      fwrite(global_robot,1,43,dest);

      //Do boards.

      for(t1=0;t1<num_boards;t1++) {
        printf("\n   Board #%d... ",t1);
        //Empty board?
        if(!orig_board_size[t1]) {
          printf("(blank)");
          continue;
          }
        //Do board t1- Reserve space...
        cboard=(unsigned char huge *)farmalloc(orig_board_size[t1]);
        if(cboard==NULL) {
          printf("ERROR: Out of memory (board %d, file %s) Conversion aborted\n",t1,fn);
          goto abort;
          }
        //Seek to board
        fseek(source,orig_board_location[t1],SEEK_SET);
        //Read in board
        long done=0;
        unsigned char huge *tptr=cboard;
        do {
          if((orig_board_size[t1]-done)>65535U) {
            fread(tptr,65535U,1,source);
            tptr+=65535U;
            done+=65535U;
            }
          else {
            fread(tptr,orig_board_size[t1]-done,1,source);
            break;
            }
        } while(done<orig_board_size[t1]);
        //Convert board
        long new_size=convert_board(cboard);
        if(!new_size) {
          //Board was aborted- No need to change stats.
          farfree(cboard);
          continue;
          }
        //Remember location for new board
        long new_loc=ftell(dest);
        //Write new board
        done=0;
        tptr=newboard;
        do {
          if((new_size-done)>65535U) {
            fwrite(tptr,65535U,1,dest);
            tptr+=65535U;
            done+=65535U;
            }
          else {
            fwrite(tptr,new_size-done,1,dest);
            break;
            }
        } while(done<new_size);
        //Save stats of new board
        fseek(dest,new_board_stat_ptr[t1],SEEK_SET);
        fwrite(&new_size,4,1,dest);
        fwrite(&new_loc,4,1,dest);
        fseek(dest,0,SEEK_END);
        //Next board!
        farfree(cboard);
        }

      //Password convert file
      if(pro_method) {
        printf("\n   Password encoding...");
        //Calculate xor code for file
        int xor=85;
        for(t1=0;t1<15;t1++) {
          //For each byte, roll once to the left and xor in pw byte if it
          //is an odd character, or add in pw byte if it is an even character.
          xor<<=1;
          if(xor>255) xor^=257;//Wraparound from roll
          if(t1&1) {
            xor+=password[t1];//Add (even byte)
            if(xor>255) xor^=257;//Wraparound from add
            }
          else xor^=password[t1];//XOR (odd byte);
          }
        //To factor in protection method, add it in and roll one last time
        xor+=pro_method;
        if(xor>255) xor^=257;
        xor<<=1;
        if(xor>255) xor^=257;
        //Can't be 0-
        if(xor==0) xor=86;//(01010110)
        //NOW convert entire dest file, after MZ2.
        long end=ftell(dest),cur;
        unsigned char bfr[512];
        int bfrsiz;
        fseek(dest,44,SEEK_SET);
        do {
          fseek(dest,0,SEEK_CUR);
          cur=ftell(dest);
          if((end-cur)<512) {
            bfrsiz=end-cur;
            fread(bfr,1,bfrsiz,dest);
            }
          else {
            fread(bfr,1,512,dest);
            bfrsiz=512;
            }
          for(t1=0;t1<512;t1++)
            bfr[t1]^=xor;
          fseek(dest,-bfrsiz,SEEK_CUR);
          fwrite(bfr,1,bfrsiz,dest);
        } while(end!=ftell(dest));
        }
      //Done!
      break;
    case 1:
      //Board file conversion
      //Header for new file

      fwrite("\xFFMB2",1,4,dest);

      //Get size of old board

      fseek(source,0,SEEK_END);
      long size=ftell(source)-25;
      fseek(source,0,SEEK_SET);

      //Allocate memory

      cboard=(unsigned char huge *)farmalloc(size);
      if(cboard==NULL) {
        printf("ERROR: Out of memory, conversion aborted (%s)\n",fn);
        goto abort;
        }

      //Read in board

      long done=0;
      unsigned char huge *tptr=cboard;
      do {
        if((size-done)>65535U) {
          fread(tptr,65535U,1,source);
          tptr+=65535U;
          done+=65535U;
          }
        else {
          fread(tptr,size-done,1,source);
          break;
          }
      } while(done<size);

      //Convert board

      long new_size=convert_board(cboard);

      if(!new_size) goto abort;//Board was aborted

      //Write new board

      done=0;
      tptr=newboard;
      do {
        if((new_size-done)>65535U) {
          fwrite(tptr,65535U,1,dest);
          tptr+=65535U;
          done+=65535U;
          }
        else {
          fwrite(tptr,new_size-done,1,dest);
          break;
          }
      } while(done<new_size);

      //Copy board title

      fseek(source,-25,SEEK_END);
      copy_bytes(25);
      farfree(cboard);
      break;
    }
  printf("\n   Done!\n");
  fclose(source);
  fclose(dest);
  unlink(temp_name);
  return;
abort:
  fclose(source);
  fclose(dest);
  unlink(fn);
  rename(temp_name,fn);
}

void rle_to_rle2(unsigned char huge *&source,unsigned char huge *&dest) {
  //Converts an rle compressed layer from source to an rle2 compressed
  //layer on dest.
  int xsiz,ysiz;
  int runs,runb;
  int size,done=0;
  //Check for lonely run of 0 from last rle
  if(*source==0) source=source+2;
  //Sizes
  xsiz=*source; source=source+1;
  ysiz=*source; source=source+1;
  *dest=xsiz; dest=dest+1;
  *dest=0; dest=dest+1;
  *dest=ysiz; dest=dest+1;
  *dest=0; dest=dest+1;
  size=xsiz*ysiz;
  //Convert. Source format- RUNSIZE, RUNBYTE, ...
  //Dest format- ( BYTE ) | ( RUNSIZE+128, RUNBYTE ), ...

  //Conversion method- Read RUNSIZE and RUNBYTE. If RUNSIZE is one,
  // write RUNBYTE unless it is >127, then instead write 129,RUNBYTE.
  // If RUNSIZE is from two to 127, write RUNSIZE+128,RUNBYTE. If
  // RUNSIZE is from 127 to 254, write 255,RUNBYTE,RUNSIZE+1,RUNBYTE.
  // If RUNSIZE is 255, write 255,RUNBYTE,255,RUNBYTE then write
  // RUNBYTE as if RUNSIZE were 1 (see above)

  do {
    runs=*source; source=source+1;
    runb=*source; source=source+1;
    if(!runs) continue;
    else if(runs==1) {
    single_byte:
      if(runb<128) {
        *dest=runb; dest=dest+1;
        }
      else {
        *dest=129; dest=dest+1;
        *dest=runb; dest=dest+1;
        }
      goto end_byte;
      }
    else if(runs<128) {
      *dest=runs|128; dest=dest+1;
      *dest=runb; dest=dest+1;
      }
    else if(runs<255) {
      *dest=255; dest=dest+1;
      *dest=runb; dest=dest+1;
      *dest=runs+1; dest=dest+1;
      *dest=runb; dest=dest+1;
      }
    else {
      *dest=255; dest=dest+1;
      *dest=runb; dest=dest+1;
      *dest=255; dest=dest+1;
      *dest=runb; dest=dest+1;
      goto single_byte;
      }
  end_byte:
    done+=runs;
  } while(done<size);
  //Done!
}

long convert_board(unsigned char huge *loc) {//Return size
  //Converts a stripped board (no header) in loc[] to newboard[].
  //Both are already allocated.
  int t1,t2,t3;
  unsigned char huge *brd=newboard;//use brd[] not newboard[]
  unsigned char huge *robptr;//Pointer to robot size storage
  int num_robots,num_scrolls,num_sensors;
  unsigned int item_size,new_isize,done;//Program/scroll size, position within
  unsigned char robold_line[256];
  unsigned char robnew_line[256];

  *brd=2; brd=brd+1;//2 for a 100x100 board
  //Translate all 6 layers
  printf("Layers");
  for(t1=0;t1<6;t1++) {
    printf(".");
    rle_to_rle2(loc,brd);
    }
  printf(" Misc... ");
  //Next 39 characters (up thru input_size) are copied exactly
  for(t1=0;t1<39;t1++) {
    *brd=*loc;
    brd=brd+1;
    loc=loc+1;
    }
  //Rememer next info
  unsigned char mod_vol=*loc; loc=loc+1;
  unsigned char pl_lock_ns=*loc; loc=loc+1;
  unsigned char pl_lock_ew=*loc; loc=loc+1;
  unsigned char pl_lock_att=*loc; loc=loc+1;
  //Copy input string
  for(t1=0;t1<81;t1++) {
    *brd=*loc;
    brd=brd+1;
    loc=loc+1;
    }
  //Skip 9 bytes (blanks and effect durations which are now global)
  loc=loc+9;
  //Copy last dir moved, adding in a faced dir of south (+16)
  *brd=(*loc)|16;
  brd=brd+1;
  loc=loc+1;
  //Copy bottom message
  for(t1=0;t1<81;t1++) {
    *brd=*loc;
    brd=brd+1;
    loc=loc+1;
    }
  //Copy bmesg cycle, lazer cycle, and mesg row
  *brd=*loc; brd=brd+1; loc=loc+1;
  *brd=*loc; brd=brd+1; loc=loc+1;
  *brd=*loc; brd=brd+1; loc=loc+1;
  //Copy bmesg column, changing 0 to 255
  unsigned char bmesg_col=*loc; loc=loc+1;
  if((bmesg_col==0)||(bmesg_col>79)) bmesg_col=255;
  *brd=bmesg_col; brd=brd+1;
  //Copy x and y scroll bytes, converting to words
  unsigned int scroll_x=(unsigned int)(signed char)*loc; loc=loc+1;
  unsigned int scroll_y=(unsigned int)(signed char)*loc; loc=loc+1;
  *brd=scroll_x&255; brd=brd+1;
  *brd=scroll_x>>8; brd=brd+1;
  *brd=scroll_y&255; brd=brd+1;
  *brd=scroll_y>>8; brd=brd+1;
  //Write misc. info and saved info
  for(t1=0;t1<4;t1++) {
    *brd=255;//Locked x/y info
    brd=brd+1;
    }
  *brd=pl_lock_ns; brd=brd+1;
  *brd=pl_lock_ew; brd=brd+1;
  *brd=pl_lock_att; brd=brd+1;
  *brd=mod_vol; brd=brd+1;
  *brd=0; brd=brd+1;//Volume increase (none)
  *brd=mod_vol; brd=brd+1;//Volume target (same as current vol)
  //Convert robots
  *brd=num_robots=*loc; loc=loc+1; brd=brd+1;
  printf("Robots... ");
  if(num_robots) {
    for(t1=0;t1<num_robots;t1++) {
      //Convert a robot
      //Copy and annotate structure, saving robot's size
      item_size=*loc; loc=loc+1;
      item_size+=(*loc)<<8; loc=loc+7;
      robptr=brd;//Pointer to robot size
      *brd=item_size&255; brd=brd+1;
      *brd=item_size>>8; brd=brd+3;
      if(item_size>31744) {
        printf("\n   ERROR: Robot over 31k in length, board aborted %d",item_size);
        return 0;
        }
      //Name, char, first word of curr.line
      for(t2=0;t2<18;t2++) {
        *brd=*loc; brd=brd+1; loc=loc+1;
        }
      //Skip second word of curr.line
      loc=loc+2;
      //Copy remaining 9 bytes
      for(t2=0;t2<9;t2++) {
        *brd=*loc; brd=brd+1; loc=loc+1;
        }
      //Fill in remainder of v2.00 struct (x, y, future x3, used, loop)
      for(t2=0;t2<7;t2++) {
        *brd=0; brd=brd+1;
        }
      *brd=(item_size!=0); brd=brd+1;
      *brd=0; brd=brd+1;
      *brd=0; brd=brd+1;
      //Now convert program.
      if(item_size) {
        new_isize=done=0;
        do {
          t2=*loc; loc=loc+1; done++;//Cmd size
          if((!t2)||(t2==255)) {
            //Same thing
            *brd=t2;
            brd=brd+1;
            new_isize++;
            continue;
            }
          //Read in actual line
          robold_line[0]=t2;
          for(t3=0;t3<t2;) {
            robold_line[++t3]=*loc; loc=loc+1;
            }
          robold_line[++t3]=*loc; loc=loc+1;
          done+=t3;
          //Convert line
          convert_robot_line(robold_line,robnew_line);
          //Write line
          t2=robnew_line[0];
          for(t3=0;t3<t2+2;t3++) {
            *brd=robnew_line[t3]; brd=brd+1;
            }
          new_isize+=t2+2;
          //Loop
        } while(done<item_size);
        //Write new size
        robptr[0]=new_isize&255;
        robptr[1]=new_isize>>8;
        }
      }
    }
  //Convert scrolls
  printf("Scrolls... ");
  *brd=num_scrolls=*loc; loc=loc+1; brd=brd+1;
  if(num_scrolls) {
    for(t1=0;t1<num_scrolls;t1++) {
      //Convert a scroll
      //Copy and annotate structure, saving scroll's size
      *brd=*loc; brd=brd+1; loc=loc+1;
      *brd=*loc; brd=brd+3; loc=loc+5;
      item_size=*loc; loc=loc+1;
      item_size+=(*loc)<<8; loc=loc+3;
      *brd=item_size&255; brd=brd+1;
      *brd=item_size>>8; brd=brd+1;
      *brd=(item_size!=0); brd=brd+1;
      if(item_size>31744) {
        printf("\n   ERROR: Scroll over 31k in length, board aborted");
        return 0;
        }
      //Now copy message, of length item_size
      if(item_size) {
        for(t2=0;t2<item_size;t2++) {
          *brd=*loc; brd=brd+1; loc=loc+1;
          if((loc[-1]=='\n')||(loc[-1]==1)) {
            //Check for '!' lines w/o \n or 0.
            if(*loc=='!') {
              if((loc[1]!='\n')&&(loc[1]!=0)) {
                *brd=' '; brd=brd+1; loc=loc+1;
                *brd=' '; brd=brd+1; loc=loc+1;
                t2+=2;
                }
              }
            }
          }
        }
      }
    }
  //Convert sensors
  printf("Sensors... ");
  *brd=num_sensors=*loc; loc=loc+1; brd=brd+1;
  if(num_sensors) {
    for(t1=0;t1<num_sensors;t1++) {
      //Convert a sensor
      //Copy and annotate structure
      for(t2=0;t2<31;t2++) {
        *brd=*loc; brd=brd+1; loc=loc+1;
        }
      *brd=1; brd=brd+1;
      }
    }
  //Return bytes used
  printf("Done!");
  return brd-newboard;
}

void convert_robot_line(unsigned char far *source,unsigned char far *dest) {
  int cmd=source[1];
  int sp=2,dp=2,cp=0;//Source/Dest/Cmd position ptr
  int t1,t2;
  dest[1]=cmd;
  //Format- LEN CMD PARAMS LEN
  //New param format- 0 then 2 byte number OR # then #-length string
  if(cmd_formats[cmd][0]=='O') {
    //Outdated
    if(cmd_formats[cmd][1]=='S') {
      //Special case- if player (not) [dir] [str]
      //Becomes-  if (not) touching [dir] [str]
      dest[1]=cmd-46;//cmd 18 or 19
      dest[dp++]=0;
      dest[dp++]=3;//Condition of touching
      dest[dp++]=source[sp++];//Direction
      //Now we need to convert a string
      cp=2;//So we exit the loop afterwards
      goto str_conv;
      }
    else {
      //Change dest cmd, then use remainder of format
      cp=1;
      switch(cmd) {
        case 13:
        case 14:
        case 15:
          dest[1]=cmd-3;
          break;
        case 17:
          dest[1]=16;
          break;
        case 158:
          dest[1]=40;
          break;
        case 166:
          dest[1]=153;
          break;
        case 167:
          dest[1]=139;
          break;
        }
      goto normal_conv;
      }
    }
  else {
  normal_conv:
    //Normal conversion
    while(cmd_formats[cmd][cp]) {
      //Get type of parameter
      t1=cmd_formats[cmd][cp++];
      //Convert
      switch(t1) {
        case 1:
        case 10:
          //Two byte number conversion (also works for conditions)
          dest[dp++]=0;
          dest[dp++]=source[sp++];
          dest[dp++]=source[sp++];
          break;
        case 2:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
          //One byte unsigned number conversion
          dest[dp++]=0;
          dest[dp++]=source[sp++];
          dest[dp++]=0;
          break;
        case 3:
        str_conv:
          //String conversion (old format- null terminated)
          t1=dp++;//Save location for size
          t2=0;//Size of string
          while(source[sp]) {
            if(source[sp]=='&') {//Replcae & with &&
              if((cmd==38)||(cmd==102)||(cmd==109)||(cmd==111)||
                (cmd==121)||(cmd==122)||(cmd==145)||(cmd==103)||
                (cmd==104)||(cmd==105)||(cmd==116)||(cmd==117)) {
                  dest[dp++]='&';
                  t2++;
                  }
              }
            dest[dp++]=source[sp++];
            t2++;
            };
          dest[dp++]=source[sp++];
          dest[t1]=t2+1;
          break;
        case 5:
          //Color (next source byte holds bit 256)
          dest[dp++]=0;
          dest[dp++]=source[sp++];
          dest[dp++]=source[sp]>>7;
          break;
        case 11:
          //Thing conversion
          dest[dp++]=0;
          dest[dp++]=source[sp++]&127;
          dest[dp++]=0;
          break;
        case 12:
          //Parameter conversion
          dest[dp++]=0;
          t1=source[sp++];
          if(t1) {
            dest[dp++]=t1;
            dest[dp++]=0;
            }
          else {
            dest[dp++]=0;
            dest[dp++]=1;
            }
          break;
        case 13:
          //Missing parameter conversion
          dest[dp++]=0;
          dest[dp++]=0;
          dest[dp++]=1;
          break;
        case 14:
          //Signed number (1-byte) conversion
          dest[dp++]=0;
          t1=(signed int)(signed char)source[sp++];
          dest[dp++]=((unsigned int)t1)&255;
          dest[dp++]=((unsigned int)t1)>>8;
          break;
        }
      //Loop until done
      };
    }
  //Done with this line- store new size
  dest[0]=dp-1;
  dest[dp]=dp-1;
}

//New (and old, usually) command formats.
//1- number
//2- number (one-byte originally)
//3- string
//4- char
//5- color (see thing for bit 256 originally)
//6- color NOT with a thing
//7- direction
//8- item
//9- conditional
//A- condition
//B- thing
//C- param (convert 0 to 256)
//D- param (originally not there, use a 256)
//E- number (one-byte signed originally)

//Only commands avail. in ver 1.03 are listed.

char far *cmd_formats[256]={
  "",
  "",
  "\x02",
  "\x02",
  "\x07\x02",
  "\x07",
  "\x05\x0B\x0C",
  "\x04",
  "\x05",
  "\x0E\x0E",
  "\x03\x01",
  "\x03\x01",
  "\x03\x01",
  "O\x03\x03",//ODT
  "O\x03\x03",//ODT
  "O\x03\x03",//ODT
  "\x03\x09\x01\x03",
  "O\x03\x09\x03\x03",//ODT
  "\x0A\x03",
  "\x0A\x03",
  "\x05\x0B\x0D\x03",
  "\x05\x0B\x0D\x03",
  "\x05\x0B\x0D\x07\x03",
  "\x05\x0B\x0D\x07\x03",
  "\x05\x0B\x0D\x0E\x0E\x03",
  "\x0E\x0E\x03",
  "\x07\x05\x0B\x0D\x03",
  "\x03",
  "\x03",
  "\x03",
  "\x03\x03",
  "\x02",
  "\x05\x0B\x0C\x07",
  "\x01\x08",
  "\x01\x08",
  "\x01\x08\x03",
  "",
  "",
  "\x03",
  "\x01\x03",
  "\x02",
  "",
  "",
  "\x03",
  "",
  "\x03",
  "",
  "",//BLANK LINE
  "\x02",
  "\x03",
  "\x07",
  "",
  "",
  "\x07\x03",
  "\x03\x02",
  "\x03\x02",
  "",
  "",
  "",
  "",
  "",
  "\x07",
  "\x07\x03",
  "\x0E\x0E",
  "OS",//ODT SPECIAL
  "OS",//ODT SPECIAL
  "\x0E\x0E\x03",
  "\x07",
  "\x07\x03",
  "",
  "",
  "\x07\x07",
  "\x07",
  "\x07",
  "\x07",
  "\x07",
  "\x07",
  "\x07",
  "\x07\x02",
  "\x05\x0B\x0C\x0E\x0E",
  "",
  "\x0E\x0E\x03",
  "\x03",
  "\x0E\x0E",
  "\x07",
  "\x07",
  "\x0E\x0E",
  "\x04",
  "\x04",
  "\x04",
  "\x04",
  "\x05",
  "\x05\x03",
  "\x05",
  "\x05\x03",
  "\x03\x01\x01",
  "\x03\x01\x01",
  "\x03\x01\x01",
  "\x01\x08\x01\x08\x03",
  "\x07\x03",
  "\x05\x0B\x0C\x07",
  "\x03",
  "\x03",
  "\x03",
  "\x03\x03",
  "\x03\x03\x03",
  "\x03",
  "\x03",
  "\x03",
  "\x03\x0E\x0E",
  "\x07\x02",
  "\x03",
  "\x03\x03",
  "\x03\x03",
  "\x03\x03",
  "\x04",
  "\x03",
  "\x03",
  "\x05\x0B\x0D\x07",
  "\x0E\x0E\x0E\x0E",
  "\x05",
  "\x07\x03",
  "\x07",
  "\x04\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02\x02",
  "",
  "",
  "\x02",
  "\x02",
  "\x02",
  "\x02",
  "\x02",
  "",
  "\x07\x07",
  "",
  "",
  "\x05\x0B\x0D\x05\x0B\x0C",
  "\x05",
  "\x05",
  "\x05",
  "\x02",
  "",
  "",
  "",
  "\x01\x04",
  "\x02",
  "\x03",
  "",
  "\x07\x04",
  "\x07\x04",
  "\x01",
  "",
  "",
  "",
  "\x02",
  "",
  "",
  "",
  "\x01\x02",
  "O\x03",//ODT
  "\x05",
  "\x05",
  "\x05",
  "\x05",
  "\x05",
  "\x02\x02",
  "\x02\x02",
  "O\x3",//ODT
  "O\x3" };//ODT
