/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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

/* Block functions and dialogs */

#include "helpsys.h"
#include "idput.h"
#include <stdio.h>
#include "error.h"
#include "block.h"
#include "window.h"
#include "data.h"
#include <_null.h>
#include <dos.h>
#include "idarray.h"
#include "roballoc.h"

//--------------------------
//
// ( ) Copy block
// ( ) Move block
// ( ) Clear block
// ( ) Flip block
// ( ) Mirror block
// ( ) Paint block
// ( ) Copy to/from overlay
// ( ) Save as ANSi
//
//    _OK_      _Cancel_
//
//--------------------------

char bdi_types[3]={ DE_RADIO,DE_BUTTON,DE_BUTTON };
char bdi_xs[3]={ 2,5,15 };
char bdi_ys[3]={ 2,11,11 };
char far *bdi_strs[3]={ "Copy block\nMove block\nClear block\nFlip block\n\
Mirror block\nPaint block\nCopy to/from overlay\nSave as ANSi",
"OK","Cancel" };
int bdi_p1s[3]={ 8,0,-1 };
int bdi_p2s[1]={ 20 };
int block_op=0;
void far *bdi_storage[1]={ &block_op };

dialog bdi={
	26,4,53,17,"Choose block command",3,
	bdi_types,
	bdi_xs,
	bdi_ys,
	bdi_strs,
	bdi_p1s,
	bdi_p2s,
	bdi_storage,0 };

int block_cmd(void) {
	int t1;
	set_context(73);
	t1=run_dialog(&bdi,current_pg_seg);
	pop_context();
	if(t1) return -1;
	return block_op;
}

char adi_types[3]={ DE_RADIO,DE_BUTTON,DE_BUTTON };
char adi_xs[3]={ 6,5,15 };
char adi_ys[3]={ 4,11,11 };
char far *adi_strs[3]={ "Custom Block\nCustom Floor\nText",
"OK","Cancel" };
int adi_p1s[3]={ 3,0,-1 };
int adi_p2s[1]={ 12 };
int obj_type=0;
void far *adi_storage[1]={ &obj_type };

dialog adi={
	26,4,53,17,"Object type",3,
	adi_types,
	adi_xs,
	adi_ys,
	adi_strs,
	adi_p1s,
	adi_p2s,
	adi_storage,0 };

int rtoo_obj_type(void) {
	set_context(74);
	if(run_dialog(&adi,current_pg_seg)) {
		pop_context();
		return -1;
		}
	pop_context();
	return obj_type;
}

char csdi_types[3]={ DE_RADIO,DE_BUTTON,DE_BUTTON };
char csdi_xs[3]={ 6,5,15 };
char csdi_ys[3]={ 4,11,11 };
char far *csdi_strs[3]={ "MegaZeux default\nASCII set\nBlank set",
"OK","Cancel" };
int csdi_p1s[3]={ 3,0,-1 };
int csdi_p2s[1]={ 16 };
int cs_type=0;
void far *csdi_storage[1]={ &cs_type };

dialog csdi={
	26,4,53,17,"Character set",3,
	csdi_types,
	csdi_xs,
	csdi_ys,
	csdi_strs,
	csdi_p1s,
	csdi_p2s,
	csdi_storage,0 };

int choose_char_set(void) {
	set_context(75);
	if(run_dialog(&csdi,current_pg_seg)) {
		pop_context();
		return -1;
		}
	pop_context();
	return cs_type;
}

char ed_types[3]={ DE_INPUT,DE_BUTTON,DE_BUTTON };
char ed_xs[3]={ 5,15,37 };
char ed_ys[3]={ 2,4,4 };
char far *ed_strs[3]={ NULL,"OK","Cancel" };
int ed_p1s[3]={ FILENAME_SIZE-1,0,1 };
int ed_p2s=193;
void far *fe_ptr=NULL;
dialog e_di={ 10,8,69,14,NULL,3,ed_types,ed_xs,ed_ys,ed_strs,ed_p1s,
	&ed_p2s,&fe_ptr,0 };

char save_file_dialog(char far *title,char far *prompt,char far *dest) {
	fe_ptr=(void far *)dest;
	ed_strs[0]=prompt;
	e_di.title=title;
	char t1=run_dialog(&e_di,current_pg_seg);
	return t1;
}

//The ansi prefix code
char ansi_pre[3]="[";
//EGA colors to ANSi colors
char col2ansi[8]={ 0,4,2,6,1,5,3,7 };
//Mini func to output meta codes to transform color from one to another.
//Curr color is the current color and dest color is the color we want.
char issue_meta(unsigned char curr,unsigned char dest,FILE *fp) {
	int size=2;
	char reset=0;
	if(curr==dest) return 0;
	fwrite(ansi_pre,1,2,fp);
	if((curr&128)&&(!(dest&128))) reset=1;
	if((curr&8)&&(!(dest&8))) reset=1;
	if(reset) {
		fputc('0',fp);
		size++;
		curr=7;
		}
	if(dest&128) {
		if(size>2) {
			fputc(';',fp);
			size++;
			}
		fputc('5',fp);
		size++;
		}
	if(dest&8) {
		if(size>2) {
			fputc(';',fp);
			size++;
			}
		fputc('1',fp);
		size++;
		}
	//Bold/Blink set. Set fg
	if((curr&7)!=(dest&7)) {
		if(size>2) {
			fputc(';',fp);
			size++;
			}
		fputc('3',fp);
		fputc('0'+col2ansi[dest&7],fp);
		size+=2;
		}
	//Set bk
	if((curr&112)!=(dest&112)) {
		if(size>2) {
			fputc(';',fp);
			size++;
			}
		fputc('4',fp);
		fputc('0'+col2ansi[(dest&112)>>4],fp);
		size+=2;
		}
	//Send 'm'
	fputc('m',fp);
	return(size+1);
}

//Export ansi according to current view.
//Only exports the actual chars if text_only is set.
void export_ansi(char far *file,int x1,int y1,int x2,int y2,char text_only) {
	FILE *fp;
	int x,y,line_size;
	int curr_color;
	int col,chr;
	unsigned char far *vid=(unsigned char far *)MK_FP(current_pg_seg,0);
	fp=fopen(file,"wb");
	if(fp==NULL) {
		if(text_only) error("Error exporting text",1,24,current_pg_seg,0x1401);
		else error("Error exporting ANSi",1,24,current_pg_seg,0x0F01);
		return;
		}
	//Fix xsiz/ysiz
	if((x2-x1)>78) x2=x1+78;
	if((y2-y1)>23) y2=y1+23;
	//File open- Init x/y pos and color
	x=x1; y=y1;
	curr_color=7;
	line_size=8;
	if(!text_only) {
		fwrite(ansi_pre,1,2,fp);
		fwrite("0m",1,2,fp);
		fwrite(ansi_pre,1,2,fp);
		fwrite("2J",1,2,fp);
		}
	//Ready to fly! Longest meta code issuable is [5;1;30;40m, or
	//12 characters.
	do {
		id_put(0,0,x,y,0,0,current_pg_seg);
		chr=vid[0]; col=vid[1];
		//Issue meta code
		if(!text_only) line_size+=issue_meta(curr_color,col,fp);
		curr_color=col;
		//Plot character- NOT if an ESC, 10, 13, 9, 8, 0, or 7. (Then use
		//alternates)
		if(chr==27) chr='-';//Alternate (ESC)
		else if(chr==10) chr=219;//Alternate (LF)
		else if(chr==13) chr=14;//Alternate (CR)
		else if(chr==9) chr='o';//Alternate (TAB)
		else if(chr==8) chr=219;//Alternate (BKSP)
		else if(chr==7) chr=249;//Alternate (BELL)
		else if(chr==0) chr=32;//Alternate (NUL)
		fputc(chr,fp); line_size++;
		//Increase x/y
		if((++x)>x2) {
			x=x1;
			//Issue CR/LF
			fputc(13,fp);
			fputc(10,fp);
			line_size=0;
			if((++y)>y2) break;
			}
		else {
			//Line too long? (Can't happen in text mode)
			if(line_size>230) {
				//Issue save/restore
				fwrite(ansi_pre,1,2,fp);
				fputc('s',fp);
				fputc(13,fp);
				fputc(10,fp);
				fwrite(ansi_pre,1,2,fp);
				fputc('u',fp);
				line_size=3;
				}
			}
		//Loop!
	} while(1);
	//Done- close
	issue_meta(curr_color,7,fp);
	fclose(fp);
}

char exdi_types[3]={ DE_RADIO,DE_BUTTON,DE_BUTTON };
char exdi_xs[3]={ 2,5,15 };
char exdi_ys[3]={ 3,11,11 };
char far *exdi_strs[3]={ "Board file (MZB)\nCharacter set (CHR)\n\
Ansi display (ANS)\nText file (TXT)\nPalette (PAL)\nSound effects (SFX)",
"OK","Cancel" };
int exdi_p1s[3]={ 6,0,-1 };
int exdi_p2s[1]={ 19 };
void far *exdi_storage[1]={ NULL };

dialog exdi={
	26,4,53,17,"Export as:",3,
	exdi_types,
	exdi_xs,
	exdi_ys,
	exdi_strs,
	exdi_p1s,
	exdi_p2s,
	exdi_storage,0 };

int export_type(void) {
	int t1=0;
	set_context(77);
	exdi_storage[0]=&t1;
	if(run_dialog(&exdi,current_pg_seg)) {
		pop_context();
		return -1;
		}
	pop_context();
	return t1;
}


char imdi_types[3]={ DE_RADIO,DE_BUTTON,DE_BUTTON };
char imdi_xs[3]={ 2,5,15 };
char imdi_ys[3]={ 3,11,11 };
char far *imdi_strs[3]={ "Board file (MZB)\nCharacter set (CHR)\n\
Ansi display (ANS)\nWorld file (MZX)\nPalette (PAL)\nSound effects (SFX)\n\
Ansi (choose pos.)",
"OK","Cancel" };
int imdi_p1s[3]={ 7,0,-1 };
int imdi_p2s[1]={ 19 };
void far *imdi_storage[1]={ NULL };

dialog imdi={
	26,4,53,17,"Import:",3,
	imdi_types,
	imdi_xs,
	imdi_ys,
	imdi_strs,
	imdi_p1s,
	imdi_p2s,
	imdi_storage,0 };

int import_type(void) {
	int t1=0;
	set_context(78);
	imdi_storage[0]=&t1;
	if(run_dialog(&imdi,current_pg_seg)) {
		pop_context();
		return -1;
		}
	pop_context();
	return t1;
}

//Imports an ANSi into board as fake/block or into overlay. Starts at
//UL corner. obj_type is the thing id for normal import (Text, etc.) OR
//0 for overlay import. (verify overlay is on before calling this function)
void import_ansi(char far *filename,char obj_type,int &curr_thing,
 int &curr_param,int start_x,int start_y) {
	int x=start_x,y=start_y,ux,color=7;
	int save_x=start_x,save_y=start_y,t1,t2,sym;
	FILE * fp;
	fp=fopen(filename,"rb");
	if(fp==NULL) {
		error("Error importing ANSi",1,24,current_pg_seg,0x1901);
		return;
		}
	do {
		sym=fgetc(fp);
		if(feof(fp)) break;
		//Tabs & BCKSPACE are interpreted as ANSi chars...
		if(sym==0x1B) {//ESC for ANSi code...
			sym=fgetc(fp);
			if(sym=='[') {//Yep.
				t2=-1;
				sym=fgetc(fp);
				//Save/Restore position
				if(sym=='s') {
					save_x=x;
					save_y=y;
					continue;
					}
				if(sym=='u') {
					x=save_x;
					y=save_y;
					continue;
					}
				//Clear line
				if(sym=='K') {
					continue;
					}
				//Move X chars in a direction
				if(sym=='A') {
					y--;
					if(y<start_y) y=start_y;
					continue;
					}
				if(sym=='B') {
					y++;
					if(y>=max_bysiz) y=max_bysiz-1;
					continue;
					}
				if(sym=='C') {
					x++;
					if(x>(start_x+79)) x=start_x+79;
					if(x>=max_bxsiz) x=max_bxsiz-1;
					continue;
					}
				if(sym=='D') {
					x--;
					if(x<start_x) x=start_x;
					continue;
					}
				if(sym=='H') {
					x=start_x;
					y=start_y;
					continue;
					}
				if((sym<'0')||(sym>'9')) goto print_it;
				//Others require a # then a code
			next_num:
				t1=0;
				do {
					t1=(t1*10)+sym-'0';
					sym=fgetc(fp);
				} while((sym>='0')&&(sym<='9'));
				//sym is H, m, ;, A, B, C, D, or J; t1 is #
				if(sym=='J') {
					if(t1!=2) break;
					x=start_x;
					y=start_y;
					continue;
					}
				if(sym=='A') {
					y-=t1;
					if(y<start_y) y=start_y;
					continue;
					}
				if(sym=='B') {
					y+=t1;
					if(y>=max_bysiz) y=max_bysiz-1;
					continue;
					}
				if(sym=='C') {
					x+=t1;
					if(x>(start_x+79)) x=start_x+79;
					if(x>=max_bxsiz) x=max_bxsiz-1;
					continue;
					}
				if(sym=='D') {
					x-=t1;
					if(x<start_x) x=start_x;
					continue;
					}
				if(sym=='H') {
					if(t2>-1) {
						y=start_y+t2-1;
						x=start_x+t1-1;
						}
					else {
						y=start_y+t1-1;
						x=start_x;
						}
					t2=-1;
					if(x>(start_x+79)) x=start_x+79;
					if(x>=max_bxsiz) x=max_bxsiz-1;
					if(y>=max_bysiz) y=max_bysiz-1;
					if(x<start_x) x=start_x;
					if(y<start_y) y=start_y;
					continue;
					}
				if(sym==';') {
					if(t2==-1) {
						t2=t1;
						sym=fgetc(fp);
						goto next_num;
						}
					}
				if((sym==';')||(sym=='m')) {
					if(t2==-1) t2=t1;
				next_meta:
					switch(t2) {
						case 0:
							color=7;
							break;
						case 1:
							color|=8;
							break;
						case 5:
							color|=128;
							break;
						case 30:
							color&=248;
							break;
						case 31:
							color=(color&248)+4;
							break;
						case 32:
							color=(color&248)+2;
							break;
						case 33:
							color=(color&248)+6;
							break;
						case 34:
							color=(color&248)+1;
							break;
						case 35:
							color=(color&248)+5;
							break;
						case 36:
							color=(color&248)+3;
							break;
						case 37:
							color=(color&248)+7;
							break;
						case 40:
							color&=143;
							break;
						case 41:
							color=(color&143)+64;
							break;
						case 42:
							color=(color&143)+32;
							break;
						case 43:
							color=(color&143)+96;
							break;
						case 44:
							color=(color&143)+16;
							break;
						case 45:
							color=(color&143)+80;
							break;
						case 46:
							color=(color&143)+48;
							break;
						case 47:
							color=(color&143)+112;
							break;
						}
					if((sym=='m')&&(t2==t1)) continue;
					else if(sym=='m') {
						t2=t1;
						goto next_meta;
						}
					t2=t1;
					sym=fgetc(fp);
					goto next_num;
					}
				}
			}
	print_it:
		if(sym=='\r') x=start_x;
		else if(sym=='\n') {
			if(++y>=max_bysiz) y=max_bysiz-1;
			}//Return
		else {
			ux=x;
			if(ux>=max_bxsiz) ux=max_bxsiz-1;
			if(obj_type>0) {
				//Can't overwrite player...
				if(level_id[ux+y*max_bxsiz]!=127) {
					//Check for deleting objects, possibly the current
					t1=level_id[ux+y*max_bxsiz];
					if(t1==122) {//(sensor)
						//Clear, copying to 0 first if param==curr
						if((level_param[ux+y*max_bxsiz]==curr_param)&&
							(curr_thing==122)) {
								copy_sensor(0,curr_param);
								curr_param=0;
								}
						clear_sensor(level_param[ux+y*max_bxsiz]);
						}
					else if((t1==123)||(t1==124)) {//(robot)
						//Clear, copying to 0 first if param==curr
						if((level_param[ux+y*max_bxsiz]==curr_param)&&
							((curr_thing==123)||(curr_thing==124))) {
								copy_robot(0,curr_param);
								curr_param=0;
								}
						clear_robot(level_param[ux+y*max_bxsiz]);
						}
					else if((t1==125)||(t1==126)) {//(scroll)
						//Clear, copying to 0 first if param==curr
						if((level_param[ux+y*max_bxsiz]==curr_param)&&
							((curr_thing==125)||(curr_thing==126))) {
								copy_scroll(0,curr_param);
								curr_param=0;
								}
						clear_scroll(level_param[ux+y*max_bxsiz]);
						}
					if(sym==32) id_place(ux,y,0,color,0);
					else id_place(ux,y,obj_type,color,sym);
					}
				}
			else {
				//Overlay
				overlay[ux+y*max_bxsiz]=sym;
				overlay_color[ux+y*max_bxsiz]=color;
				}
			if(++x>(start_x+79)) {
				x=start_x;
				if(++y>=max_bysiz) y=max_bysiz-1;
				}
			if(x>=max_bxsiz) x=max_bxsiz-1;
			}
		} while(1);
	fclose(fp);
	return;
}

char iadi_types[3]={ DE_RADIO,DE_BUTTON,DE_BUTTON };
char iadi_xs[3]={ 6,5,15 };
char iadi_ys[3]={ 4,11,11 };
char far *iadi_strs[3]={ "Custom Block\nCustom Floor\nText\nOverlay",
"OK","Cancel" };
int iadi_p1s[3]={ 4,0,-1 };
int iadi_p2s[1]={ 12 };
int iao_type=0;
void far *iadi_storage[1]={ &iao_type };

dialog iadi={
	26,4,53,17,"Import ANSi as-",3,
	iadi_types,
	iadi_xs,
	iadi_ys,
	iadi_strs,
	iadi_p1s,
	iadi_p2s,
	iadi_storage,0 };

int import_ansi_obj_type(void) {
	set_context(78);
	if(run_dialog(&iadi,current_pg_seg)) {
		pop_context();
		return -1;
		}
	pop_context();
	return iao_type;
}
