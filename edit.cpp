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

//Password completely removed -Koj
//Took out SMZX -Koji
//Editing the world!

#include "helpsys.h"
#include "runrobot.h"
#include "scrdisp.h"
#include "scrdump.h"
#include "sfx.h"
#include "sfx_edit.h"
#include "counter.h"
#include "game.h"
#include "fill.h"
#include "meter.h"
//#include "password.h"
#include "egacode.h"
#include "block.h"
#include "pal_ed.h"
#include "char_ed.h"
#include "edit_di.h"
#include "beep.h"
#include "boardmem.h"
#include "intake.h"
#include "mod.h"
#include "param.h"
#include "error.h"
#include "idarray.h"
#include "ems.h"
#include "meminter.h"
#include "cursor.h"
#include "retrace.h"
#include "string.h"
#include "edit.h"
#include "ezboard.h"
#include "data.h"
#include "const.h"
#include "graphics.h"
#include "window.h"
#include "getkey.h"
#include "palette.h"
#include "idput.h"
#include "hexchar.h"
#include <dos.h>
#include "roballoc.h"
#include "saveload.h"
#include <stdio.h>
#include <stdlib.h>
#include "blink.h"
#include "cursor.h"
#include "counter.h"
// For saving/loading MZX image files - Exo
#include "mzm.h"

/* Edit menu- (w/box ends on sides) Current menu name is highlighted. The
	bottom section zooms to show a list of options for the current menu,
	although all keys are available at all times. PGUP/PGDN changes menu.
	The menus are shown in listed order. [draw] can be [text]. (in which case
	there is the words "Type to place text" after the ->) [draw] can also be
	[place] or [block]. The options DRAW, TEXT, DEBUG, BLOCK, MOD, and DEF.
	COLORS are highlighted white (instead of green) when active. Debug mode
	pops up a small window in the lower left corner, which rushes to the right
	side and back when the cursor reaches 3/4 of the way towards it,
	horizontally. The menu itself is 15 lines wide and 7 lines high. The
	board display, w/o the debug menu, is 19 lines high and 80 lines wide.
	Note: SAVE is highlighted white if the world has changed since last load
	or save.

	The commands with ! instead of : have not been implemented yet.

	The menu line turns to "Overlay editing-" and the lower portion shows
	overlay commands on overlay mode.
ÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
X/Y:    99/99³
Board:    000³
Mem:    0000k³
EMS:    0000k³
Robot memory-³
64.0/64k 100%³
             ³ <- added for modules - Exo
ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
 WORLD  BOARD  THING  CURSOR  SHOW  MISC  Drawing:_!_ (_#_ PushableRobot p00)
ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
 L:Load     S:Save  G:Global Info    Alt+R:Restart  Alt+T:Test  *:Protection
 Alt+S:Status Info  Alt+C:Char Edit  Alt+E:Palette  Alt+F:Sound Effects

 Alt+Z:Clear   X:Exits       Alt+P:Size/Pos  I:Info  A:Add  D:Delete  V:View
 Alt+I:Import  Alt+X:Export  B:Select Board  Alt+O:Edit Overlay

 F3:Terrain  F4:Item      F5:Creature  F6:Puzzle  F7:Transport  F8:Element
 F9:Misc     F10:Objects  P:Parameter  C:Color

 :Move  Space:Place  Enter:Modify+Grab  Alt+M:Modify  Ins:Grab  Del:Delete
 F:Fill   Tab:Draw     F2:Text            Alt+B:Block   Alt+:Move 10

 Shift+F1:Show InvisWalls  Shift+F2:Show Robots  Shift+F3:Show Fakes
 Shift+F4:Show Spaces

 F1!Help    R:Redraw Screen  Alt+A:Select Char Set  Alt+D:Default Colors
 ESC:Exit	Alt+L:Test SAM   Alt+Y:Debug Mode       Alt+N:Music
ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄPgup/Pgdn:MenuÄ

ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
 OVERLAY EDITING- (Alt+O to end)          Current:_!_ (_#_ Character p00)
ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
 :Move  Space:Place  Ins:Grab  Enter:Character  Del:Delete        F:Fill
 C:Color  Alt+B:Block  Tab:Draw  Alt+:Move 10   Alt+S:Show level  F2:Text
ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

Undocumented keys:

End:			LR corner
Home:			UL corner
ESC:			Cancel mode/overlay mode if active (otherwise exit)
Backspace:	Delete (move left one in text)
Alt+W:                  redraw screen
*/

/* Editing data */

int draw_mode=0;//1 for on, 2 for text, 3 for block, 4 for block place.
					 //+128 for overlay.
int curr_color=7,curr_thing=0,curr_param=0;//Current. Param 0 for robot/etc
														 //is temp. storage to be deleted
														 //or realigned upon first
														 //placement. Other params mean
														 //to copy on placement.
char curr_menu=0;//Current menu, 0 thru 5.
char def_color_mode=1;//If default colors are on.
char debug_mode=0,debug_x=0;//If debug mode is on, and X position of box.
int x_pos=0,y_pos=0;//Current screen cursor position.
int x_top_pos=0,y_top_pos=0;//Current x/y position in array of screen
#define ARRAY_X (x_pos+x_top_pos)
#define ARRAY_Y (y_pos+y_top_pos)
#define ARRAY_OFFS (ARRAY_X+ARRAY_Y*max_bxsiz)
char changed=0;//Whether world has changed since a load or save.
//Whether to update menu and/or view, whether to fade in
char update_menu=1,fade_in=1,update_view=1;
//Remembers one corner of a block cmd or the starting position for a text cmd
int save_x,save_y;
int save_x2,save_y2;
char blk_cmd;
//Whether to show non-overlay when editing overlay
char show_level=1;

/* Menu data */
#define NUM_MENUS 6
char far menu_names[NUM_MENUS][9]={
	" WORLD "," BOARD "," THING "," CURSOR "," SHOW "," MISC " };
char far draw_names[7][10]={ " Current:"," Drawing:","    Text:","   Block:",
	"   Block:"," Import:", " Import:" };
char far text_help[19]="Type to place text";
char far block_help[28]="Press ENTER on other corner";
char far block_help2[27]="Press ENTER to place block";
char far block_help3[26]="Press ENTER to place ANSi";
char far block_help4[25]="Press ENTER to place MZM";
char far menu_help[15]="Pgup/Pgdn:Menu";
char far *menu_lines[NUM_MENUS][2]={ {
" L:LoadS:Save  G:Global Info    Alt+R:Restart  Alt+T:Test",
" Alt+S:Status Info  Alt+C:Char Edit  Alt+E:Palette  Alt+F:Sound Effects"
} , {
" Alt+Z:Clear   X:Exits  Alt+P:Size/Pos  I:Info  A:Add  D:Delete  V:View",
" Alt+I:Import  Alt+X:Export  B:Select Board  Alt+O:Edit Overlay"
} , {
" F3:Terrain  F4:Item F5:Creature  F6:Puzzle  F7:Transport  F8:Element",
" F9:MiscF10:Objects  P:Parameter  C:Color"
} , {
" :Move  Space:Place  Enter:Modify+Grab  Alt+M:Modify  Ins:Grab  Del:Delete",
" F:Fill   Tab:DrawF2:Text  Alt+B:Block   Alt+:Move 10"
} , {
" Shift+F1:Show InvisWalls  Shift+F2:Show Robots  Shift+F3:Show Fakes",
" Shift+F4:Show Spaces"
} , {
" F1:Help    R:Redraw Screen  Alt+A:Select Char Set  Alt+D:Default Colors",
" ESC:Exit   Alt+L:Test SAM   Alt+Y:Debug Mode  Alt+N:Music  Alt+8:Mod *"
} };

int far menu_keys[NUM_MENUS+1][2][28]={ {
{	6,'L',5,0,6,'S',2,0,13,'G',4,0,13,-19,2,0,10,-20,2,0,12,'*',2,0 },
{	17,-31,2,0,15,-46,2,0,13,-18,2,0,19,-33,7,0 }
},{
{	11,-44,3,0,7,'X',7,0,14,-25,2,0,6,'I',2,0,5,'A',2,0,8,'D',2,0,6,'V',2,0 },
{	12,-23,2,0,12,-45,2,0,14,'B',2,0,18,-24,2,0 }
},{
{  10,-61,2,0,7,-62,6,0,11,-63,2,0,9,-64,2,0,12,-65,2,0,10,-66,4,0 },
{  7,-67,5,0,11,-68,2,0,11,'P',2,0,7,'C',32,0 }
},{
{	9,0,11,' ',2,0,17,13,2,0,12,-50,2,0,8,-82,2,0,10,-83,2,0 },
{	6,'F',3,0,8,9,5,0,7,-60,12,0,11,-48,22,0 }
},{
{	24,-84,2,0,20,-85,2,0,19,-86,10,0 },
{	20,-87,57,0 }
},{
{	7,-59,4,0,15,'R',2,0,21,-30,2,0,20,-32,6,0 },
{	8,27,3,0,14,-38,3,0,16,-21,7,0,11,-49,15,0 }
},{
{	9,0,11,' ',2,0,8,-82,2,0,15,13,2,0,10,-83,8,0,6,'F',4,0 },
{	7,'C',2,0,11,-48,2,0,8,9,19,0,16,-31,2,0,7,-60,3,0 } } };

char far *overlay_menu_lines[4]={
" OVERLAY EDITING- (Alt+O to end)",
" :Move  Space:Place  Ins:Grab  Enter:Character  Del:Delete   F:Fill",
" C:Color  Alt+B:Block  Tab:Draw  Alt+:Move 10   Alt+S:Show level  F2:Text",
"Character" };

void add_ext(char far *str,char far *ext) {
	int t1,t2=str_len(str);
	//check for existing ext.
	for(t1=0;t1<t2;t1++)
		if(str[t1]=='.') break;
	if(t1<t2) return;
	str[8]=0;//Limit main filename section to 8 chars
	str_cat(str,ext);
}

void edit_world(void) {
	int t1,t2,t3,t4,t5,t6,t7,t8,t9,t0;
	int key;
	char temp[20];
	char ansi[15];
  char mzm[15];
	long tlong1,tlong2;
	char r,g,b;
	FILE *fp;
	set_context(80);
	error_mode=0;
	//Clear world on entrance
	clear_world();
	clear_zero_objects();
	scroll_color=15;
	scroll_base_color=143;
	scroll_corner_color=135;
	scroll_pointer_color=128;
	scroll_title_color=143;
	scroll_arrow_color=142;
  if(smzx_mode == 2)
  {
    update_smzx_palette();
  }
	//Setup most variables
	draw_mode=curr_thing=curr_param=curr_menu=x_pos=y_pos=changed=
		x_top_pos=y_top_pos=0;
	curr_color=update_menu=fade_in=7;
	show_level=1;
	debug_x=65;
	//Set page
	current_page=0;
	current_pg_seg=VIDEO_SEG;
	//Clear screen
	clear_screen(1824,current_pg_seg);
	m_show();
	//Normally cursor is moved right after key interpretation.
	move_cursor(x_pos,y_pos);
	cursor_solid();
	do {
		//If NOTHING was changed, do NOT page flip
		set_counter("HELP_MENU",1);
		if(!(update_view|update_menu|fade_in)) {
			m_hide();
			goto no_pflip;
			}
		//Pre-page flip
		current_pg_seg=VIDEO_NUM2SEG(!current_page);
		//Draw view OR copy view from old page
		if(update_view) {
			//Fill area to blot out overflow
			fill_line(1520,0,0,EC_NA_FILL*256+177,current_pg_seg);
			if(draw_mode<128) {
				overlay_mode|=128;
				draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
				overlay_mode&=~128;
				}
			else {
				//ALWAYS shown as a normal overlay.
				t1=overlay_mode;
				overlay_mode=1;
				if(!show_level) overlay_mode|=64;
				draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
				overlay_mode=t1;
				}
			//Highlight block, if any
			if((draw_mode&127)==3) {
				//From t1/t2 to t3/t4
				t1=save_x;
				t2=save_y;
				t3=ARRAY_X;
				t4=ARRAY_Y;
				if(t3<t1) {
					t5=t1;
					t1=t3;
					t3=t5;
					}
				if(t4<t2) {
					t5=t2;
					t2=t4;
					t4=t5;
					}
				//Offset by xy_top_pos
				t1-=x_top_pos;
				t2-=y_top_pos;
				t3-=x_top_pos;
				t4-=y_top_pos;
				//Clip to screen
				if(t1<0) t1=0;
				if(t2<0) t2=0;
				if(t3>79) t3=79;
				if(t4>18) t4=18;
				//t3 becomes WIDTH
				t3=t3-t1+1;
				//Highlight block
				for(;t2<=t4;t2++)
					color_line(t3,t1,t2,159,current_pg_seg);
				}
			}
		else {
			//Copy 19 lines of 80 chars of 2 bytes each.
			m_hide();
			mem_cpy((char far *)MK_FP(current_pg_seg,0),
				(char far *)MK_FP(VIDEO_NUM2SEG(current_page),0),3040);
			m_show();
			}
		//Draw menu (draw debug if view updated)
		if(update_menu|update_view) {
			if(debug_mode) {
				draw_debug_box(11);
				update_view=0;
				}
			//Draw box
			if(update_menu) {
				draw_window_box(0,19,79,24,current_pg_seg,EC_MAIN_BOX,
					EC_MAIN_BOX_DARK,EC_MAIN_BOX_CORNER,0);
				draw_window_box(0,21,79,24,current_pg_seg,EC_MAIN_BOX,
					EC_MAIN_BOX_DARK,EC_MAIN_BOX_CORNER,0);
				draw_char(196,EC_MAIN_BOX_CORNER,78,21,current_pg_seg);
				draw_char(217,EC_MAIN_BOX_DARK,79,21,current_pg_seg);
				//Write menu names
				if(draw_mode<128) {
					t3=1;//X position
					for(t1=0;t1<NUM_MENUS;t1++) {
						t2=EC_MENU_NAME;//Pick the color
						if(t1==curr_menu) t2=EC_CURR_MENU_NAME;
						//Write it
						write_string(menu_names[t1],t3,20,t2,current_pg_seg);
						//Add to x
						t3+=str_len(menu_names[t1]);
						}
					}
				else {
					write_string(overlay_menu_lines[0],1,20,EC_MENU_NAME,
						current_pg_seg);
					t3=42;
					}
				//Write mode string
				write_string(draw_names[draw_mode&127],t3,20,EC_MODE_STR,current_pg_seg);
				t3+=str_len(draw_names[draw_mode&127]);
				//Write help or current thing
				if((draw_mode&127)==2) write_string(text_help,t3,20,EC_MODE_HELP,
					current_pg_seg);
				else if((draw_mode&127)==3) write_string(block_help,t3,20,EC_MODE_HELP,
					current_pg_seg);
				else if((draw_mode&127)==4) write_string(block_help2,t3,20,EC_MODE_HELP,
					current_pg_seg);
				else if((draw_mode&127)==5) write_string(block_help3,t3,20,EC_MODE_HELP,
					current_pg_seg);
				else if((draw_mode&127)==6) write_string(block_help4,t3,20,EC_MODE_HELP,
					current_pg_seg);
				else {
					//Current thing
					draw_char(' ',7,t3,20,current_pg_seg);
					draw_char(' ',7,t3+2,20,current_pg_seg);
					//Use id_put
					if(draw_mode<128) {
						t1=level_id[0];
						t2=level_color[0];
						t4=level_param[0];
						t5=level_under_color[0];
						level_id[0]=curr_thing;
						level_color[0]=curr_color;
						level_param[0]=curr_param;
						level_under_color[0]=0;
						overlay_mode|=128;
						id_put(t3+1,20,0,0,0,0,current_pg_seg);
						overlay_mode&=~128;
						level_id[0]=t1;
						level_color[0]=t2;
						level_param[0]=t4;
						level_under_color[0]=t5;
						}
					else draw_char(curr_param,curr_color,t3+1,20,current_pg_seg);
					//Space and '('
					t3+=4;
					draw_char('(',EC_CURR_THING,t3++,20,current_pg_seg);
					//Color symbol
					draw_color_box(curr_color,0,t3,20,current_pg_seg);
					//Name of thing
					if(draw_mode&128) {
						write_string(overlay_menu_lines[3],t3+4,20,EC_CURR_THING,
							current_pg_seg);
						t3+=str_len(overlay_menu_lines[3])+5;
						}
					else {
						write_string(thing_names[curr_thing],t3+4,20,EC_CURR_THING,
							current_pg_seg);
						t3+=str_len(thing_names[curr_thing])+5;
						}
					//Parameter (DON'T SHOW for robots/etc unless RPARAM def'd)
#ifndef RPARAM
					if(curr_thing<122) {
#endif
						draw_char('p',EC_CURR_PARAM,t3++,20,current_pg_seg);
						write_hex_byte(curr_param,EC_CURR_PARAM,t3,20,current_pg_seg);
						t3+=2;
#ifndef RPARAM
						}
					else t3--;
#endif
					//')'
					draw_char(')',EC_CURR_THING,t3,20,current_pg_seg);
					//Done
					}
				//Draw current menu
				if(draw_mode&128) {
					write_string(overlay_menu_lines[1],1,22,EC_OPTION,current_pg_seg);
					write_string(overlay_menu_lines[2],1,23,EC_OPTION,current_pg_seg);
					}
				else {
					write_string(menu_lines[curr_menu][0],1,22,EC_OPTION,current_pg_seg);
					write_string(menu_lines[curr_menu][1],1,23,EC_OPTION,current_pg_seg);
					}
				//Highlight any current options
				if(draw_mode&128) {
					if(show_level)
						color_line(16,51,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					if(draw_mode==129)
						color_line(8,24,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					else if(draw_mode==130)
						color_line(7,69,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					else if((draw_mode==131)||(draw_mode==132))
						color_line(11,11,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					}
				else if(curr_menu==0) {
					if(changed)
						color_line(6,13,22,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					}
				else if(curr_menu==3) {
					if(draw_mode==1)
						color_line(8,11,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					else if(draw_mode==2)
						color_line(7,24,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					else if((draw_mode==3)||(draw_mode==4))
						color_line(11,43,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					}
				else if(curr_menu==5) {
					if(def_color_mode)
						color_line(20,53,22,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					if(mod_playing[0]!=0)
						color_line(11,53,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					if(debug_mode)
						color_line(16,30,23,EC_HIGHLIGHTED_OPTION,current_pg_seg);
					}
				//Write menu help string
				if(draw_mode<128)
					write_string(menu_help,64,24,EC_CURR_PARAM,current_pg_seg);
				//Done!
				}
			else {//Otherwise copy menu
				m_hide();
				mem_cpy((char far *)MK_FP(current_pg_seg,3040),
					(char far *)MK_FP(VIDEO_NUM2SEG(current_page),3040),960);
				m_show();
				}
			//Menu is updated
			update_menu=0;
			}
		else {//Otherwise copy menu
			m_hide();
			mem_cpy((char far *)MK_FP(current_pg_seg,3040),
				(char far *)MK_FP(VIDEO_NUM2SEG(current_page),3040),960);
			m_show();
			}
		//Page flip
		current_page=1-current_page;
		//Page flip automatically doesn't occur until NEXT retrace.
		//But we wait until then so we don't start writing on the
		//visible page!
		m_hide();
		page_flip(current_page);
		move_cursor(x_pos,y_pos);
		cursor_solid();
		m_vidseg(current_pg_seg);
		wait_retrace();
		//Fade in (used upon entrance)
		if(fade_in) {
			vquick_fadein();
			fade_in=0;
			//Turn cursor on
			cursor_solid();
			}
	no_pflip:
		//Move cursor done at end of loop
		if(debug_mode) {
			write_number(ARRAY_X,EC_DEBUG_NUMBER,debug_x+7,12,current_pg_seg,3);
			write_number(ARRAY_Y,EC_DEBUG_NUMBER,debug_x+11,12,current_pg_seg,3);
			}
		//Highlight cursor - Save old color/char and fiddle with it
		t1=t2=*(unsigned char far *)MK_FP(current_pg_seg,((x_pos+y_pos*80)<<1)+1);
		//If same fg/bk, flip the bk brightness
		if((t1&15)==((t1&240)>>4)) t1^=128;
		*(unsigned char far *)MK_FP(current_pg_seg,((x_pos+y_pos*80)<<1)+1)=t1;
		//Check character- if it is a 219, change to a space.
		//If it is a ², make it a °.
		t1=t3=*(unsigned char far *)MK_FP(current_pg_seg,(x_pos+y_pos*80)<<1);
		if(t1==219) t1=32;
		if(t1==178) t1=176;
		*(unsigned char far *)MK_FP(current_pg_seg,(x_pos+y_pos*80)<<1)=t1;
		//Get key
		m_show();
		key=getkey();
		m_hide();
		//Fix cursor
		*(unsigned char far *)MK_FP(current_pg_seg,((x_pos+y_pos*80)<<1)+1)=t2;
		*(unsigned char far *)MK_FP(current_pg_seg,(x_pos+y_pos*80)<<1)=t3;
		m_show();
		//Act upon key
		cursor_off();
		//Check for text mode...
		if(((draw_mode&127)==2)&&((key>=32)&&(key<=255))) {
			//Place a character.
			changed=1;
			if(!(draw_mode&128)) {
				if(level_id[ARRAY_OFFS]==127) goto move_right;//No overwriting player
				//If top thing at position is a robot/etc, deallocate it. If it
				//is the CURRENT thing, copy to #0 first
				t1=level_id[ARRAY_OFFS];
				if(t1==122) {//(sensor)
					if((level_param[ARRAY_OFFS]==curr_param)&&(curr_thing==122))
						copy_sensor(0,curr_param);
					clear_sensor(level_param[ARRAY_OFFS]);
					curr_param=0;
					}
				else if((t1==123)||(t1==124)) {//(robot)
					if((level_param[ARRAY_OFFS]==curr_param)&&
						((curr_thing==123)||(curr_thing==124)))
							copy_robot(0,curr_param);//No mem. = too bad
					clear_robot(level_param[ARRAY_OFFS]);
					curr_param=0;
					}
				else if((t1==125)||(t1==126)) {//(scroll)
					if((level_param[ARRAY_OFFS]==curr_param)&&
						((curr_thing==125)||(curr_thing==126)))
							copy_scroll(0,curr_param);//No mem. = too bad
					clear_scroll(level_param[ARRAY_OFFS]);
					curr_param=0;
					}
				//Place.
				id_place(ARRAY_X,ARRAY_Y,77,curr_color,key);
				}
			else {
				//Overlay mode
				overlay[ARRAY_OFFS]=key;
				overlay_color[ARRAY_OFFS]=curr_color;
				}
			update_view=1;
			//Increase x position if possible
			goto move_right;
			}
		//Ensure uppercase
		if((key>='a')&&(key<='z')) key-=32;
		//Fix for block mode
		if(((draw_mode&127)==3)||((draw_mode&127)==4)||((draw_mode&127)==5)
     ||(draw_mode&127)==6)
			if((key==' ')||(key==13)) key=-48;
	re_evaul_key:
		switch(key) {
			case ']'://Screen .PCX dump
				dump_screen();
				break;
			case MOUSE_EVENT://Mouse click
				//Possibilities-
				//
				//1) Mouse is in upper menu area.
				// a) Mouse will change current menu/exit overlay mode (left)
				// b) Mouse will change current color (right)
				//2) Mouse is in lower menu area.
				// a) Mouse will choose menu command.
				// b) Mouse will change current menu. (Pgup/Pgdn)
				//3) Mouse is in level area- move cursor and place.
				//4) Mouse is anywhere else- do nothing.
				if(mouse_event.cy==20) {//#1
					if(mouse_event.cx<42) {//#1a
						if(draw_mode&128) goto overlay_off;
						//Determine menu to switch to.
						t1=1;
						for(t2=0;t2<NUM_MENUS;t2++) {
							t1+=str_len(menu_names[t2]);
							if(mouse_event.cx<t1) break;
							}
						if(t2>=NUM_MENUS) t2=NUM_MENUS-1;
						curr_menu=t2;
						update_menu=1;
						break;
						}
					else goto change_color;//#1b
					}
				else if(mouse_event.cy>21) {//#2
					if(mouse_event.cy<24) {//#2a
						if((mouse_event.cx<2)||(mouse_event.cx==79)) break;
						if(draw_mode&128) t1=NUM_MENUS;
						else t1=curr_menu;
						t2=mouse_event.cy-22;
						t3=mouse_event.cx-2;
						t4=0;
						do {
							t3-=menu_keys[t1][t2][t4];
							if(t3<0) break;
							t4+=2;
						} while(1);
						key=menu_keys[t1][t2][t4+1];
						goto re_evaul_key;
						}
					else {//#2b
						if(mouse_event.cx<68) goto prev_menu;
						else goto next_menu;
						}
					}
				//#3 or #4?
				if(mouse_event.cy>18) break;
				if(mouse_event.cx>=board_xsiz) break;
				if(mouse_event.cy>=board_ysiz) break;
				//#3- Move cursor and place.
				x_pos=mouse_event.cx;
				y_pos=mouse_event.cy;
				//Move debug window?
				if((debug_x==65)&&(x_pos>59)) {
					debug_x=0;
					update_view=1;
					}
				if((debug_x==0)&&(x_pos<20)) {
					debug_x=65;
					update_view=1;
					}
				goto place;
			case -20://Alt+T
				//Test
				//if(protection_method!=NO_PROTECTION) {
				//	error("Can't test password-protected world",0,24,
				//		current_pg_seg,0x2001);
				//	break;
				//	}
				if((curr_thing==122)&&(curr_param==0)) {
					clear_sensor(0);
					curr_thing=0;
					}
				else if(((curr_thing==123)||(curr_thing==124))&&(curr_param==0)) {
					clear_robot(0);
					curr_thing=0;
					}
				else if(((curr_thing==125)||(curr_thing==126))&&(curr_param==0)) {
					clear_scroll(0);
					curr_thing=0;
					}
				//Temporary save
				str_cpy(temp,curr_file);
				t1=curr_board;
				store_current();
				save_world("test_$$$.mzx");
				select_current(t1);
				//Turn cheats on and fade out
				cheats_active+=5;
				vquick_fadeout();
				//Play
				clear_game_params();
				set_counter("TIME",time_limit);
				send_robot_def(0,11);
				send_robot_def(0,10);
				m_hide();
				player_restart_x=player_x;
				player_restart_y=player_y;
				play_game(1);
				cheats_active-=5;
				//Clear screen
				clear_screen(1824,current_pg_seg);
				//Palette
				default_palette();
				for(t2=0;t2<16;t2++)
					set_color_intensity(t2,100);
				insta_fadein();
				m_show();
				//Reload
				clear_world();
				if(load_world("test_$$$.mzx",2)) {
					clear_world();
					select_current(0);
					update_view=update_menu=1;
					curr_file[0]=0;
          if(smzx_mode == 2)
          {
            update_smzx_palette();
          }
					break;
					}
				select_current(t1);
				unlink("test_$$$.mzx");
				insta_fadeout();
				fade_in=1;
				update_view=update_menu=1;
				str_cpy(curr_file,temp);
				break;
			case -33://AltF
				//Edit SFX
				sfx_edit();
				changed=1;
				break;
			case -17://AltW
				//Re-init screen
				vga_16p_mode();
				ega_14p_mode();
				cursor_off();
				blink_off();
				ec_update_set();
				update_palette(0);
				changed=1;
				update_menu=1;
				break;
			case 'F'://F
				//Fill
				//Just, well, fill! Fill function takes care of all
				//robot copying/deleting/etc, filling something over
				//itself, (same OR different param) and filling with/over
				//the player. (not allowed) Only precaution- copy current
				//robot to #0 first.
				if(draw_mode&128)
					fill_overlay(ARRAY_X,ARRAY_Y,curr_param,curr_color);
				else {
					if((curr_thing==122)&&(curr_param)) {
						copy_sensor(0,curr_param);
						sensors[0].used=1;
						curr_param=0;
						}
					else if(((curr_thing==123)||(curr_thing==124))&&(curr_param)) {
						copy_robot(0,curr_param);
						robots[0].used=1;
						curr_param=0;
						}
					else if(((curr_thing==125)||(curr_thing==126))&&(curr_param)) {
						copy_scroll(0,curr_param);
						scrolls[0].used=1;
						curr_param=0;
						}
					fill_area(ARRAY_X,ARRAY_Y,curr_thing,curr_color,curr_param);
					}
				//Was our current object overwritten?
				if((curr_thing==122)&&(!sensors[curr_param].used))
					curr_thing=0;
				else if(((curr_thing==123)||(curr_thing==124))&&
					(!robots[curr_param].used))
						curr_thing=0;
				else if(((curr_thing==125)||(curr_thing==126))&&
					(!scrolls[curr_param].used))
						curr_thing=0;
				changed=1;
				update_view=update_menu=1;
				break;
			case 'V'://V
				//View
				if(draw_mode&128) break;
				scroll_x=scroll_y=0;
				do {
					//Select OTHER page for seg
					current_pg_seg=VIDEO_NUM2SEG(!current_page);
					//Draw border
					draw_viewport();
					//Figure out x/y of top
					calculate_xytop(t1,t2);
					//Draw screen
					draw_game_window(t1,t2,current_pg_seg);
					//Page flip
					current_page=1-current_page;
					page_flip(current_page);
					m_vidseg(current_pg_seg);
					//Retrace
					wait_retrace();
					//Movement
					if(keywaiting()) {
						key=getkey();
						switch(key) {
							case -72://Up
								scroll_y--;
								break;
							case -80://Down
								scroll_y++;
								break;
							case -75://Left
								scroll_x--;
								break;
							case -77://Right
								scroll_x++;
								break;
							case -71://Home
							case -79://End
								scroll_x=scroll_y=0;
								break;
							}
						}
				} while(key!=27);
				scroll_x=scroll_y=0;
				update_view=update_menu=key=1;
				break;
			case -23://AltI
				//Import
				if(draw_mode&128) break;
				//Choose import mode
				t1=import_type();
				if(t1==-1) break;
				//Import as...
				switch(t1) {
					case 0:
						//...board file.
						if(choose_file("*.MZB",temp,"Choose board to import")) break;
						//Open file.
						fp=fopen(temp,"rb");
						if(fp==NULL) {
							error("Error importing board",1,24,current_pg_seg,0x1501);
							break;
							}
						//Verify header
						fread(temp,1,3,fp);
						temp[3]=0;
						if(str_cmp(temp,"\xFFMB")) {
							//Either a ver 1.0? file or corrupted
							error("Error importing board",1,24,current_pg_seg,0x1502);
							break;
							}
						//Get file ver.
						t1=fgetc(fp);
						if(t1!='2') {
							//Not a ver 2.00 file
							error("Board is from a more recent version of MegaZeux",
								0,24,current_pg_seg,0x1601);
							break;
							}
						//Read board- Store current...
						//If current object is a robot/etc of not #0, copy to #0.
						if((curr_thing==122)&&(curr_param!=0)) {
							copy_sensor(0,curr_param);
							curr_param=0;
							}
						if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
							//No room = forget it!
							if(!copy_robot(0,curr_param)) curr_param=0;
						if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
							//No room = forget it!
							if(!copy_scroll(0,curr_param)) curr_param=0;
						//Clear current
						clear_current();
						//Get size of board
						tlong1=ftell(fp);
						fseek(fp,-BOARD_NAME_SIZE,SEEK_END);
						tlong2=ftell(fp);
						fseek(fp,tlong1,SEEK_SET);
						tlong2-=tlong1;
						//tlong2=board size
						//no space allocated to current
						//allocate space and set vars
						if(allocate_board_space(tlong2,curr_board))//Exit only
							error("Out of memory and/or disk space",2,4,
								current_pg_seg,0x0205);
						//allocated, set vars
						board_sizes[curr_board]=tlong2;
						//Read board
						disk_board(curr_board,fp,1,0);
						//Load into current
						select_current(curr_board);
						//Load board name from file
						fread(&board_list[curr_board*BOARD_NAME_SIZE],1,
							BOARD_NAME_SIZE,fp);
						//Done.
						fclose(fp);
						break;
					case 1:
						//Character set
						if(choose_file("*.CHR",temp,"Choose character set")) break;
						//Open file.
						if(ec_load_set(temp))
							error("Error importing char. set",1,24,current_pg_seg,0x1801);
						break;
					case 2:
					case 6:
						//Ansi file
						if(choose_file("*.ANS",ansi,"Choose ANSi file")) break;
						//Get type of import
						t2=import_ansi_obj_type();
						if(t2==-1) break;
						//Check if overlay
						if(t2==3) {
							if(overlay_mode==0) {
								error("Overlay mode is not on (see Board Info)",0,24,
									current_pg_seg,0x1103);
								break;
								}
							}
						//Import
						if(t2==0) t2=5;
						else if(t2==1) t2=17;
						else if(t2==2) t2=77;
						else t2=0;
						if(t1==2) import_ansi(ansi,t2,curr_thing,curr_param);
						else {
							blk_cmd=t2;
							draw_mode=(draw_mode&128)+5;
							update_view=update_menu=1;
							}
						break;
          case 7:
          {
            // MZM file
            if(choose_file("*.MZM", mzm, "Choose MZM file")) break;
            // Get type of import
            t2 = import_mzm_obj_type();
						if(t2 == 1) 
            {
							if(overlay_mode == 0) 
              {
								error("Overlay mode is not on (see Board Info)",0,24,
									current_pg_seg,0x1103);
								break;
						  }
						}            

            blk_cmd = t2;
					  draw_mode = (draw_mode & 128) + 6;
            update_view = update_menu = 1;
            break;
          }            
					case 3:
						//Import world
						if(choose_file("*.MZX",temp,"Choose world to import")) break;
						//Open file
						fp=fopen(temp,"rb");
						if(fp==NULL) {
							error("Error importing world",1,24,current_pg_seg,0x1A01);
							break;
							}
						//Check for password protection and world type
						fseek(fp,25,SEEK_SET);
						if(fgetc(fp)!=0) {
							/*error("Cannot import password protected world",0,
								24,current_pg_seg,0x1B01);
							fclose(fp);
							break;*/
							}
						if(fgetc(fp)!='M') {
							error("Error importing world",1,24,current_pg_seg,0x1A02);
							fclose(fp);
							break;
							}
						t1=fgetc(fp);
						if(t1=='X') {
							error("World is from version 1- Use conversion program",
								0,24,current_pg_seg,0x1C01);
							fclose(fp);
							break;
							}
						//Version and pw okay
						fseek(fp,4234,SEEK_SET);
						t1=fgetc(fp);//Number of boards
						if(t1==0) {
							//SFX- Skip
							fread(&t1,1,2,fp);
							fseek(fp,t1,SEEK_CUR);
							t9=3+t1;
							t1=fgetc(fp);
							}
						else t9=0;//Size of SFX section
						//Count OUR empty boards
						t2=0;
						for(t3=0;t3<NUM_BOARDS;t3++)
							if(board_where[t3]==W_NOWHERE) t2++;
						//Compare
						if(t1>t2) {
							//Too many-
							if(t2==0) {
								error("No available boards",1,24,current_pg_seg,0x0B02);
								fclose(fp);
								break;
								}
							if(error("Too many boards- only partial import will be done",
								0,25,current_pg_seg,0x1D01)==1) {
									fclose(fp);
									break;//Fail
									}
							//They chose OK, so import as many as possible.
							}
						//Import t1 boards- Mark as importable by setting
						//type to W_IMPORT, offset to file area, and size
						//properly. Also sets title.
						t3=0;//Current board to change (out of NUM_BOARDS)
						t4=0;//Current board within MZX file (out of t1)
						do {
							//Find next empty board
							do {
								t3++;
								if(t3>=NUM_BOARDS) break;
							} while(board_where[t3]!=W_NOWHERE);
							if(t3>=NUM_BOARDS) break;//Done
							//Import to board t3- get title
							fread(&board_list[BOARD_NAME_SIZE*t3],1,
								BOARD_NAME_SIZE,fp);
							//Jump to board size/offset, saving curr. position
							tlong1=ftell(fp);
							//Tlong2=Position of size/offsets
							tlong2=4235+t9+(t1*BOARD_NAME_SIZE);
							//Increase to position of size/offset for current board
							tlong2+=t4*8;
							//Jump
							fseek(fp,tlong2,SEEK_SET);
							//Read size and offset
							fread(&board_sizes[t3],4,1,fp);
							fread(&board_offsets[t3].offset,4,1,fp);
							//Set where
							board_where[t3]=W_IMPORT;
							//Return to names
							fseek(fp,tlong1,SEEK_SET);
							//Next board...
						} while((++t4)<t1);
						//Now actually IMPORT each board- % meter used
						save_screen(current_pg_seg);
						t2=0;//Keeps track of number of boards imported
						//t4=Max number of boards we'll import
						meter("Importing world...",current_pg_seg,t2,t4);
						for(t1=0;t1<NUM_BOARDS;t1++) {
							//Import this board?
							if(board_where[t1]!=W_IMPORT) continue;
							//Save file offset
							tlong1=board_offsets[t1].offset;
							//Allocate space...
							board_where[t1]=W_NOWHERE;
							if(allocate_board_space(board_sizes[t1],t1)) {
								fclose(fp);
								restore_screen(current_pg_seg);
								error("Out of memory and/or disk space",2,4,
									current_pg_seg,0x0206);
								}
							//Read in board.
							fseek(fp,tlong1,SEEK_SET);
							disk_board(t1,fp,1,0);
							//Next!
							meter_interior(current_pg_seg,++t2,t4);
							}
						//Done!
						fclose(fp);
						restore_screen(current_pg_seg);
						break;
					case 4:
						//Palette file
						if(choose_file("*.PAL",temp,"Choose palette file")) break;
						//Open file.
						fp=fopen(temp,"rb");
						if(fp==NULL) {
							error("Error importing palette",1,24,current_pg_seg,0x2401);
							break;
							}
						for(t1=0;t1<16;t1++) {
							r=fgetc(fp);
							g=fgetc(fp);
							b=fgetc(fp);
							set_rgb(t1,r,g,b);
							}
						update_palette();
						//Done.
						fclose(fp);
						break;
					case 5:
						//SFX file
						if(choose_file("*.SFX",temp,"Choose SFX file")) break;
						//Open file.
						fp=fopen(temp,"rb");
						if(fp==NULL) {
							error("Error importing .SFX file",1,24,current_pg_seg,0x3001);
							break;
							}
						custom_sfx_on=1;
						for(t1=0;t1<NUM_SFX;t1++)
							fread(&custom_sfx[69*t1],1,69,fp);
						//Done.
						fclose(fp);
						break;
					}
				changed=1;
				update_view=update_menu=1;
				break;
			case -45://AltX
				//Export
				if(draw_mode&128) break;
				//Confirm pw
				//if(check_pw()) break;
				//Choose export mode
				t1=export_type();
				if(t1==-1) break;
				//Get file name
				temp[0]=0;

				if(save_file_dialog("Export","Save as: ",temp)) break;

				//Export as...
				switch(t1) {
					case 0:
						//...board file.
						add_ext(temp,".MZB");
						//Open file.
						fp=fopen(temp,"wb");
						if(fp==NULL) {
							error("Error exporting board",1,24,current_pg_seg,0x1201);
							break;
							}
						//Save header.
						fwrite("\xFFMB2",1,4,fp);
						//Write board- Store it...
						//If current object is a robot/etc of not #0, copy to #0.
						if((curr_thing==122)&&(curr_param!=0)) {
							copy_sensor(0,curr_param);
							curr_param=0;
							}
						if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
							//No room = forget it!
							if(!copy_robot(0,curr_param)) curr_param=0;
						if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
							//No room = forget it!
							if(!copy_scroll(0,curr_param)) curr_param=0;
						//Store current
						store_current();
						//Save board
						disk_board(curr_board,fp,0,0);
						//Reload current
						select_current(curr_board);
						update_view=update_menu=1;
						//Save board name to file
						fwrite(&board_list[curr_board*BOARD_NAME_SIZE],1,
							BOARD_NAME_SIZE,fp);
						//Done.
						fclose(fp);
						break;
					case 1:
						//Character set
            if(temp[0] == '+')
            {
              char tempc = temp[4];
              temp[4] = 0;
              char far *next;
              int offset = strtol(temp + 1, &next, 10);
              temp[4] = tempc;

              int size = 256;
 
              if(*next == '#')
              {
                tempc = next[4];
                next[4] = 0;
                char far *next2 = next;
                size = strtol(next + 1, &next, 10);
                next2[4] = tempc;
              }  

						  add_ext(next, ".CHR");
              if(ec_save_set(next, offset, size))
                error("Error exporting char. set",1,24,current_pg_seg,0x1301);
            }              
            else
            {  
              if(temp[0] == '#')
              {
                char tempc = temp[4];
                temp[4] = 0;
                char far *next;
                int size = strtol(temp + 1, &next, 10);
                temp[4] = tempc;
                
  						  add_ext(next, ".CHR");
                if(ec_save_set(next, 0, size))
                  error("Error exporting char. set",1,24,current_pg_seg,0x1301);
              }
              else
              {
  						  add_ext(temp,".CHR");
  						  //Open file.
  						  if(ec_save_set(temp, 0, 256))
  							  error("Error exporting char. set",1,24,current_pg_seg,0x1301);
              }
            }
            break;
					  case 2:
						  //Ansi file
						add_ext(temp,".ANS");
					case 3:
						//Text file
						if(t1==3) add_ext(temp,".TXT");
						//Overlays as they normally would be.
						//Save. (auto clips)
						export_ansi(temp,0,0,board_xsiz,board_ysiz,t1-2);
						//Done.
						break;
					case 4:
						//Palette file
						add_ext(temp,".PAL");
						fp=fopen(temp,"wb");
						if(fp==NULL) {
							error("Error exporting palette",1,24,current_pg_seg,0x2E01);
							break;
							}
						for(t1=0;t1<16;t1++) {
							get_rgb(t1,r,g,b);
							fputc(r,fp);
							fputc(g,fp);
							fputc(b,fp);
							}
						fclose(fp);
						break;
					case 5:
						//SFX file
						add_ext(temp,".SFX");
						fp=fopen(temp,"wb");
						if(fp==NULL) {
							error("Error exporting .SFX file",1,24,current_pg_seg,0x2F01);
							break;
							}
						for(t1=0;t1<NUM_SFX;t1++) {
							if(custom_sfx_on)
								fwrite(&custom_sfx[69*t1],1,69,fp);
							else fwrite(sfx_strs[t1],1,69,fp);
							}
						fclose(fp);
						break;
					}
				break;
		      /*case '*':// *
				//Password
				if(draw_mode&128) break;
				password_dialog();
				changed=1;
				break;*/
			case -30://AltA
				//Select char set
				if(draw_mode&128) break;
				t1=choose_char_set();
				if(t1==-1) break;
				switch(t1) {
					case 0:
						//Megazeux default
						ec_load_mzx();
						break;
					case 1:
						//Blank
						ec_load_mzx();
						//Characters to leave alone-
						//32 through 126 (text)
						//0 (already blank)
						//16-18, 24-27, 29-31 (arrows)
						//176-223 (lines/blocks)
						//254 (square)
						//249-250 (dots)
						//251 (check mark)
						//7 (circle)
						//
						//Blanken all others.
						//
						//Series to blanken-
						//1-6, 8-15, 19-23, 28, 127-175, 224-248, 252-253
						for(t1=1;t1<=6;t1++)
							for(t2=0;t2<14;t2++)
								ec_change_byte_nou(t1,t2,0);
						for(t1=8;t1<=15;t1++)
							for(t2=0;t2<14;t2++)
								ec_change_byte_nou(t1,t2,0);
						for(t1=19;t1<=23;t1++)
							for(t2=0;t2<14;t2++)
								ec_change_byte_nou(t1,t2,0);
						for(t1=127;t1<=175;t1++)
							for(t2=0;t2<14;t2++)
								ec_change_byte_nou(t1,t2,0);
						for(t1=224;t1<=248;t1++)
							for(t2=0;t2<14;t2++)
								ec_change_byte_nou(t1,t2,0);
						for(t2=0;t2<14;t2++) {
							ec_change_byte_nou(28,t2,0);
							ec_change_byte_nou(252,t2,0);
							ec_change_byte_nou(253,t2,0);
							ec_change_byte_nou(255,t2,0);
							}
						ec_update_set();
						break;
					}
				changed=1;
				break;
			case -48://AltB
				//Block mode
				if((draw_mode&127)==0) {
					//Turn ON.
					draw_mode=3+(draw_mode&128);
					save_x=ARRAY_X;
					save_y=ARRAY_Y;
					update_view=update_menu=1;
					break;
					}
				else if(((draw_mode&127)!=3)&&((draw_mode&127)!=4)) {
					if((draw_mode&127)==5)//Place ANSi
						import_ansi(ansi,blk_cmd,curr_thing,curr_param,ARRAY_X,ARRAY_Y);
          if((draw_mode & 127) == 6)//Place MZM
						load_mzm(mzm, ARRAY_X, ARRAY_Y, blk_cmd);
					//Turn other mode OFF.
					draw_mode&=128;
					update_view=update_menu=1;
					break;
					}
				else if((draw_mode&127)==4) {
					//Block destination chosen.
					t1=ARRAY_X;
					t2=ARRAY_Y;
					if(((save_x2-save_x)+t1)>=board_xsiz)
						save_x2=save_x+board_xsiz-t1-1;
					if(((save_y2-save_y)+t2)>=board_ysiz)
						save_y2=save_y+board_ysiz-t2-1;
					switch(blk_cmd) {
						case 0://Copy
							//Do ULeft->LRight or LRight->ULeft?
							if((t1==save_x)&&(t2==save_y)) break;//Done
							if((t1<save_x)||((t1==save_x)&&(t2<save_y))) {
								//Copy starting at UL, going by columns.
								for(t3=save_x;t3<=save_x2;t3++) {
									for(t4=save_y;t4<=save_y2;t4++) {
										t5=t3+t4*max_bxsiz;
										t6=((t3-save_x)+t1)+((t4-save_y)+t2)*max_bxsiz;
										//Copy from t5 to t6
										if(draw_mode&128) {
											//Overlay-
											overlay[t6]=overlay[t5];
											overlay_color[t6]=overlay_color[t5];
											continue;
											}
										//Clear anything there...
										t7=level_id[t6];
										if(t7==127) continue;//No copy over player
										if(t7==122) {//(sensor)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												(curr_thing==122)) {
													copy_sensor(0,curr_param);
													curr_param=0;
													}
											clear_sensor(level_param[t6]);
											}
										else if((t7==123)||(t7==124)) {//(robot)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==123)||(curr_thing==124))) {
													copy_robot(0,curr_param);
													curr_param=0;
													}
											clear_robot(level_param[t6]);
											}
										else if((t7==125)||(t7==126)) {//(scroll)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==125)||(curr_thing==126))) {
													copy_scroll(0,curr_param);
													curr_param=0;
													}
											clear_scroll(level_param[t6]);
											}
										//Copy any robot/scroll at t5
										t7=level_id[t5];
										t8=level_param[t5];
										if(t7==122) {//(sensor)
											//...make a copy...
											if(!(t9=find_sensor())) {
												error("No available sensors",1,24,current_pg_seg,0x0801);
												break;
												}
											copy_sensor(t9,t8);
											t8=t9;
											}
										if((t7==123)||(t7==124)) {//(robot)
											//...make a copy...
											if(!(t9=find_robot())) {
												error("No available robots",1,24,current_pg_seg,0x0901);
												break;
												}
											if(copy_robot(t9,t8)) {
												robots[t9].used=0;
												error("Out of robot memory",1,21,current_pg_seg,0x0502);
												break;
												}
											//...and deallocate original if it was number 0.
											t8=t9;
											}
										if((t7==125)||(t7==126)) {//(scroll)
											//...make a copy...
											if(!(t9=find_scroll())) {
												error("No available scrolls",1,24,current_pg_seg,0x0A01);
												break;
												}
											if(copy_scroll(t9,t8)) {
												robots[t9].used=0;
												error("Out of robot memory",1,21,current_pg_seg,0x0503);
												break;
												}
											t8=t9;
											}
										//Player?
										if(t7==127) {
											level_id[t6]=level_param[t6]=
												level_under_id[t6]=level_under_param[t6]=0;
											level_color[t6]=level_under_color[t6]=7;
											}
										else {
											//Place
											level_id[t6]=t7;
											level_param[t6]=t8;
											level_color[t6]=level_color[t5];
											level_under_id[t6]=level_under_id[t5];
											level_under_color[t6]=level_under_color[t5];
											level_under_param[t6]=level_under_param[t5];
											}
										//Loop. (whew!)
										}
									}
								//Done (UL -> LR)
								}
							else {
								//Copy starting at LR, going left by columns.
								for(t3=save_x2;t3>=save_x;t3--) {
									for(t4=save_y2;t4>=save_y;t4--) {
										t5=t3+t4*max_bxsiz;
										t6=((t3-save_x)+t1)+((t4-save_y)+t2)*max_bxsiz;
										//Copy from t5 to t6
										if(draw_mode&128) {
											//Overlay-
											overlay[t6]=overlay[t5];
											overlay_color[t6]=overlay_color[t5];
											continue;
											}
										//Clear anything there...
										t7=level_id[t6];
										if(t7==127) continue;//No copy over player
										if(t7==122) {//(sensor)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												(curr_thing==122)) {
													copy_sensor(0,curr_param);
													curr_param=0;
													}
											clear_sensor(level_param[t6]);
											}
										else if((t7==123)||(t7==124)) {//(robot)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==123)||(curr_thing==124))) {
													copy_robot(0,curr_param);
													curr_param=0;
													}
											clear_robot(level_param[t6]);
											}
										else if((t7==125)||(t7==126)) {//(scroll)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==125)||(curr_thing==126))) {
													copy_scroll(0,curr_param);
													curr_param=0;
													}
											clear_scroll(level_param[t6]);
											}
										//Copy any robot/scroll at t5
										t7=level_id[t5];
										t8=level_param[t5];
										if(t7==122) {//(sensor)
											//...make a copy...
											if(!(t9=find_sensor())) {
												error("No available sensors",1,24,current_pg_seg,0x0801);
												break;
												}
											copy_sensor(t9,t8);
											t8=t9;
											}
										if((t7==123)||(t7==124)) {//(robot)
											//...make a copy...
											if(!(t9=find_robot())) {
												error("No available robots",1,24,current_pg_seg,0x0901);
												break;
												}
											if(copy_robot(t9,t8)) {
												robots[t9].used=0;
												error("Out of robot memory",1,21,current_pg_seg,0x0502);
												break;
												}
											//...and deallocate original if it was number 0.
											t8=t9;
											}
										if((t7==125)||(t7==126)) {//(scroll)
											//...make a copy...
											if(!(t9=find_scroll())) {
												error("No available scrolls",1,24,current_pg_seg,0x0A01);
												break;
												}
											if(copy_scroll(t9,t8)) {
												robots[t9].used=0;
												error("Out of robot memory",1,21,current_pg_seg,0x0503);
												break;
												}
											t8=t9;
											}
										//Player?
										if(t7==127) {
											level_id[t6]=level_param[t6]=
												level_under_id[t6]=level_under_param[t6]=0;
											level_color[t6]=level_under_color[t6]=7;
											}
										else {
											//Place
											level_id[t6]=t7;
											level_param[t6]=t8;
											level_color[t6]=level_color[t5];
											level_under_id[t6]=level_under_id[t5];
											level_under_color[t6]=level_under_color[t5];
											level_under_param[t6]=level_under_param[t5];
											}
										//Loop. (whew!)
										}
									}
								//Done (UL -> LR)
								}
							break;
						case 6://Copy (to/from overlay)
							//If from overlay to main, ask if we want it to become
							//CustomFloor, CustomBlock, or Text.
							if(draw_mode<128) {
								t9=rtoo_obj_type();
								if(t9==-1) break;
								if(t9==0) t9=5;
								else if(t9==1) t9=17;
								else t9=77;
								}
							//Copy starting at UL, going by columns.
							for(t3=save_x;t3<=save_x2;t3++) {
								for(t4=save_y;t4<=save_y2;t4++) {
									t5=t3+t4*max_bxsiz;
									t6=((t3-save_x)+t1)+((t4-save_y)+t2)*max_bxsiz;
									//Copy from t5 to t6
									if(draw_mode&128) {
										//TO Overlay- Use magic id draw function
										overlay_mode|=128;//DON'T do overlay
										id_put(0,0,t3,t4,0,0,current_pg_seg);
										overlay_mode&=~128;
										//Use color/char at 0/0
										overlay[t6]=*(char far *)MK_FP(current_pg_seg,0);
										overlay_color[t6]=*(char far *)MK_FP(current_pg_seg,1);
										continue;
										}
									//FROM overlay...
									//Clear anything there...
									t7=level_id[t6];
									if(t7==127) continue;//No copy over player
									if(t7==122) {//(sensor)
										//Clear, copying to 0 first if param==curr
										if((level_param[t6]==curr_param)&&
											(curr_thing==122)) {
												copy_sensor(0,curr_param);
												curr_param=0;
												}
										clear_sensor(level_param[t6]);
										}
									else if((t7==123)||(t7==124)) {//(robot)
										//Clear, copying to 0 first if param==curr
										if((level_param[t6]==curr_param)&&
											((curr_thing==123)||(curr_thing==124))) {
												copy_robot(0,curr_param);
												curr_param=0;
												}
										clear_robot(level_param[t6]);
										}
									else if((t7==125)||(t7==126)) {//(scroll)
										//Clear, copying to 0 first if param==curr
										if((level_param[t6]==curr_param)&&
											((curr_thing==125)||(curr_thing==126))) {
												copy_scroll(0,curr_param);
												curr_param=0;
												}
										clear_scroll(level_param[t6]);
										}
									//Copy as ID type t9 UNLESS it's a space
									if(overlay[t5]==32) {
										level_id[t6]=level_param[t6]=0;
										level_color[t6]=7;
										}
									else {
										level_id[t6]=t9;
										level_param[t6]=overlay[t5];
										level_color[t6]=overlay_color[t5];
										}
									level_under_id[t6]=level_under_param[t6]=0;
									level_under_color[t6]=7;
									//Loop.
									}
								}
							//Done
							break;
						case 1://Move
							//Do ULeft->LRight or LRight->ULeft?
							if((t1==save_x)&&(t2==save_y)) break;//Done
							if((t1<save_x)||((t1==save_x)&&(t2<save_y))) {
								//Move starting at UL, going by columns.
								for(t3=save_x;t3<=save_x2;t3++) {
									for(t4=save_y;t4<=save_y2;t4++) {
										t5=t3+t4*max_bxsiz;
										t6=((t3-save_x)+t1)+((t4-save_y)+t2)*max_bxsiz;
										//Move from t5 to t6
										if(draw_mode&128) {
											//Overlay-
											overlay[t6]=overlay[t5];
											overlay_color[t6]=overlay_color[t5];
											overlay[t5]=32;
											overlay_color[t5]=7;
											continue;
											}
										//Clear anything there...
										t7=level_id[t6];
										if(t7==127) continue;//No copy over player
										if(t7==122) {//(sensor)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												(curr_thing==122)) {
													copy_sensor(0,curr_param);
													curr_param=0;
													}
											clear_sensor(level_param[t6]);
											}
										else if((t7==123)||(t7==124)) {//(robot)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==123)||(curr_thing==124))) {
													copy_robot(0,curr_param);
													curr_param=0;
													}
											clear_robot(level_param[t6]);
											}
										else if((t7==125)||(t7==126)) {//(scroll)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==125)||(curr_thing==126))) {
													copy_scroll(0,curr_param);
													curr_param=0;
													}
											clear_scroll(level_param[t6]);
											}
										//move
										level_id[t6]=level_id[t5];
										level_param[t6]=level_param[t5];
										level_color[t6]=level_color[t5];
										level_under_id[t6]=level_under_id[t5];
										level_under_color[t6]=level_under_color[t5];
										level_under_param[t6]=level_under_param[t5];
										id_clear(t3,t4);
										//Loop.
										}
									}
								//Done (UL -> LR)
								}
							else {
								//Move starting at LR, going left by columns.
								for(t3=save_x2;t3>=save_x;t3--) {
									for(t4=save_y2;t4>=save_y;t4--) {
										t5=t3+t4*max_bxsiz;
										t6=((t3-save_x)+t1)+((t4-save_y)+t2)*max_bxsiz;
										//Move from t5 to t6
										if(draw_mode&128) {
											//Overlay-
											overlay[t6]=overlay[t5];
											overlay_color[t6]=overlay_color[t5];
											overlay[t5]=32;
											overlay_color[t5]=7;
											continue;
											}
										//Clear anything there...
										t7=level_id[t6];
										if(t7==127) continue;//No copy over player
										if(t7==122) {//(sensor)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												(curr_thing==122)) {
													copy_sensor(0,curr_param);
													curr_param=0;
													}
											clear_sensor(level_param[t6]);
											}
										else if((t7==123)||(t7==124)) {//(robot)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==123)||(curr_thing==124))) {
													copy_robot(0,curr_param);
													curr_param=0;
													}
											clear_robot(level_param[t6]);
											}
										else if((t7==125)||(t7==126)) {//(scroll)
											//Clear, copying to 0 first if param==curr
											if((level_param[t6]==curr_param)&&
												((curr_thing==125)||(curr_thing==126))) {
													copy_scroll(0,curr_param);
													curr_param=0;
													}
											clear_scroll(level_param[t6]);
											}
										//Move
										level_id[t6]=level_id[t5];
										level_param[t6]=level_param[t5];
										level_color[t6]=level_color[t5];
										level_under_id[t6]=level_under_id[t5];
										level_under_color[t6]=level_under_color[t5];
										level_under_param[t6]=level_under_param[t5];
										id_clear(t3,t4);
										//Loop.
										}
									}
								//Done (UL -> LR)
								}
							break;
						}
					draw_mode&=128;
					changed=1;
					update_view=update_menu=1;
					break;
					}
				//Finish block
				draw_mode&=128;
				save_x2=ARRAY_X;
				save_y2=ARRAY_Y;
				if(save_x>save_x2) {
					t1=save_x;
					save_x=save_x2;
					save_x2=t1;
					}
				if(save_y>save_y2) {
					t1=save_y;
					save_y=save_y2;
					save_y2=t1;
					}
				//Redraw
				update_view=update_menu=1;
				//Ask function
				t1=block_cmd();
				if(t1==-1) break;
				//Do function t1
				switch(t1) {
					case 0:
						//Copy
						blk_cmd=t1;
						draw_mode+=4;
						break;
					case 1:
						//Move
						blk_cmd=t1;
						draw_mode+=4;
						break;
					case 2:
						//Clear
						for(t1=save_x;t1<=save_x2;t1++) {
							for(t2=save_y;t2<=save_y2;t2++) {
								if(draw_mode<128) {
									t3=level_id[t1+t2*max_bxsiz];
									if(t3==127) continue;//Player
									if(t3==122) {//(sensor)
										//Clear, copying to 0 first if param==curr
										if((level_param[t1+t2*max_bxsiz]==curr_param)&&
											(curr_thing==122)) {
												copy_sensor(0,curr_param);
												curr_param=0;
												}
										clear_sensor(level_param[t1+t2*max_bxsiz]);
										}
									else if((t3==123)||(t3==124)) {//(robot)
										//Clear, copying to 0 first if param==curr
										if((level_param[t1+t2*max_bxsiz]==curr_param)&&
											((curr_thing==123)||(curr_thing==124))) {
												copy_robot(0,curr_param);
												curr_param=0;
												}
										clear_robot(level_param[t1+t2*max_bxsiz]);
										}
									else if((t3==125)||(t3==126)) {//(scroll)
										//Clear, copying to 0 first if param==curr
										if((level_param[t1+t2*max_bxsiz]==curr_param)&&
											((curr_thing==125)||(curr_thing==126))) {
												copy_scroll(0,curr_param);
												curr_param=0;
												}
										clear_scroll(level_param[t1+t2*max_bxsiz]);
										}
									//Clear
									id_clear(t1,t2);
									}
								else {
									overlay[t1+t2*max_bxsiz]=32;
									overlay_color[t1+t2*max_bxsiz]=7;
									}
								}
							}
						changed=1;
						break;
					case 3:
						//Flip
						if(draw_mode<128) {
							t4=((save_y2-save_y)>>1)+save_y;
							for(t1=save_x;t1<=save_x2;t1++) {
								for(t2=save_y,t3=save_y2;t2<=t4;t2++,t3--) {
									t5=level_id[t1+t2*max_bxsiz];
									level_id[t1+t2*max_bxsiz]=level_id[t1+t3*max_bxsiz];
									level_id[t1+t3*max_bxsiz]=t5;

									t5=level_color[t1+t2*max_bxsiz];
									level_color[t1+t2*max_bxsiz]=level_color[t1+t3*max_bxsiz];
									level_color[t1+t3*max_bxsiz]=t5;

									t5=level_param[t1+t2*max_bxsiz];
									level_param[t1+t2*max_bxsiz]=level_param[t1+t3*max_bxsiz];
									level_param[t1+t3*max_bxsiz]=t5;

									t5=level_under_id[t1+t2*max_bxsiz];
									level_under_id[t1+t2*max_bxsiz]=level_under_id[t1+t3*max_bxsiz];
									level_under_id[t1+t3*max_bxsiz]=t5;

									t5=level_under_color[t1+t2*max_bxsiz];
									level_under_color[t1+t2*max_bxsiz]=level_under_color[t1+t3*max_bxsiz];
									level_under_color[t1+t3*max_bxsiz]=t5;

									t5=level_under_param[t1+t2*max_bxsiz];
									level_under_param[t1+t2*max_bxsiz]=level_under_param[t1+t3*max_bxsiz];
									level_under_param[t1+t3*max_bxsiz]=t5;
									}
								}
							}
						else {
							t4=((save_y2-save_y)>>1)+save_y;
							for(t1=save_x;t1<=save_x2;t1++) {
								for(t2=save_y,t3=save_y2;t2<=t4;t2++,t3--) {
									t5=overlay[t1+t2*max_bxsiz];
									overlay[t1+t2*max_bxsiz]=overlay[t1+t3*max_bxsiz];
									overlay[t1+t3*max_bxsiz]=t5;

									t5=overlay_color[t1+t2*max_bxsiz];
									overlay_color[t1+t2*max_bxsiz]=overlay_color[t1+t3*max_bxsiz];
									overlay_color[t1+t3*max_bxsiz]=t5;
									}
								}
							}
						changed=1;
						break;
					case 4:
						//Mirror
						if(draw_mode<128) {
							t4=((save_x2-save_x)>>1)+save_x;
							for(t1=save_x,t3=save_x2;t1<=t4;t1++,t3--) {
								for(t2=save_y;t2<=save_y2;t2++) {
									t5=level_id[t1+t2*max_bxsiz];
									level_id[t1+t2*max_bxsiz]=level_id[t3+t2*max_bxsiz];
									level_id[t3+t2*max_bxsiz]=t5;

									t5=level_color[t1+t2*max_bxsiz];
									level_color[t1+t2*max_bxsiz]=level_color[t3+t2*max_bxsiz];
									level_color[t3+t2*max_bxsiz]=t5;

									t5=level_param[t1+t2*max_bxsiz];
									level_param[t1+t2*max_bxsiz]=level_param[t3+t2*max_bxsiz];
									level_param[t3+t2*max_bxsiz]=t5;

									t5=level_under_id[t1+t2*max_bxsiz];
									level_under_id[t1+t2*max_bxsiz]=level_under_id[t3+t2*max_bxsiz];
									level_under_id[t3+t2*max_bxsiz]=t5;

									t5=level_under_color[t1+t2*max_bxsiz];
									level_under_color[t1+t2*max_bxsiz]=level_under_color[t3+t2*max_bxsiz];
									level_under_color[t3+t2*max_bxsiz]=t5;

									t5=level_under_param[t1+t2*max_bxsiz];
									level_under_param[t1+t2*max_bxsiz]=level_under_param[t3+t2*max_bxsiz];
									level_under_param[t3+t2*max_bxsiz]=t5;
									}
								}
							}
						else {
							t4=((save_x2-save_x)>>1)+save_x;
							for(t1=save_x,t3=save_x2;t1<=t4;t1++,t3--) {
								for(t2=save_y;t2<=save_y2;t2++) {
									t5=overlay[t1+t2*max_bxsiz];
									overlay[t1+t2*max_bxsiz]=overlay[t3+t2*max_bxsiz];
									overlay[t3+t2*max_bxsiz]=t5;

									t5=overlay_color[t1+t2*max_bxsiz];
									overlay_color[t1+t2*max_bxsiz]=overlay_color[t3+t2*max_bxsiz];
									overlay_color[t3+t2*max_bxsiz]=t5;
									}
								}
							}
						changed=1;
						break;
					case 5:
						//Paint
						if(draw_mode<128) {
							for(t1=save_x;t1<=save_x2;t1++)
								for(t2=save_y;t2<=save_y2;t2++)
									level_color[t1+t2*max_bxsiz]=curr_color;
							}
						else {
							for(t1=save_x;t1<=save_x2;t1++)
								for(t2=save_y;t2<=save_y2;t2++)
									overlay_color[t1+t2*max_bxsiz]=curr_color;
							}
						changed=1;
						break;
					case 6:
						//Main <-> overlay
						if(overlay_mode==0) {
							error("Overlay mode is not on (see Board Info)",0,24,
								current_pg_seg,0x1102);
							break;
							}
						blk_cmd=t1;
						if(draw_mode<128) {
							//Clear current object...
							if((curr_thing==122)&&(curr_param==0))
								clear_sensor(0);
							else if(((curr_thing==123)||(curr_thing==124))&&(curr_param==0))
								clear_robot(0);
							else if(((curr_thing==125)||(curr_thing==126))&&(curr_param==0))
								clear_scroll(0);
							curr_thing=0;
							curr_color=7;
							curr_param=32;
							//Draw mode fix
							draw_mode=132;
							//Update
							update_menu=update_view=1;
							//Done
							break;
							}
						//Turn off-
						curr_thing=0;
						curr_color=7;
						curr_param=0;
						//Draw mode fix
						draw_mode=4;
						//Update
						update_menu=update_view=1;
						break;
					case 7:
						//Pw check
						//if(protection_method==NO_SAVING)
							//if(check_pw()) break;
						//Save as ANSi
						temp[0]=0;
						if(save_file_dialog("Block ANSi Save","Save block as: ",temp))
							break;
						add_ext(temp,".ANS");
						t1=overlay_mode;
						if(draw_mode&128) overlay_mode=1|(64*(show_level==0));
						else overlay_mode=128;
						export_ansi(temp,save_x,save_y,save_x2,save_y2,0);
						overlay_mode=t1;
						break;
          case 8:
            // Save as MZM
            temp[0] = 0;
            if(save_file_dialog("Block MZM Save", "Save block as: ", temp))
              break;
            add_ext(temp, ".MZM");
            if(draw_mode & 128)
            {
              save_mzm(temp, save_x, save_y, (save_x2 - save_x + 1),
               (save_y2 - save_y + 1), 1, 0);
            }
            else
            {
              save_mzm(temp, save_x, save_y, (save_x2 - save_x + 1),
               (save_y2 - save_y + 1), 0, 0);
            }
					}
				break;
			case -24://AltO
				//Overlay mode
				//Turn on-
				if(draw_mode<128) {
					//Is overlay mode ON !?
					if(overlay_mode==0) {
						error("Overlay mode is not on (see Board Info)",0,24,
							current_pg_seg,0x1101);
						break;
						}
					//Clear current object...
					if((curr_thing==122)&&(curr_param==0))
						clear_sensor(0);
					else if(((curr_thing==123)||(curr_thing==124))&&(curr_param==0))
						clear_robot(0);
					else if(((curr_thing==125)||(curr_thing==126))&&(curr_param==0))
						clear_scroll(0);
					curr_thing=0;
					curr_color=7;
					curr_param=32;
					//Draw mode fix
					draw_mode=128;
					//Update
					update_menu=update_view=1;
					//Done
					context=81;
					break;
					}
			overlay_off:
				context=80;
				//Turn off-
				curr_thing=0;
				curr_color=7;
				curr_param=0;
				//Draw mode fix
				draw_mode=0;
				//Update
				update_menu=update_view=1;
				break;
			case -46://AltC
				if(draw_mode&128) break;
				//Edit char set
				char_editor();
				changed=1;
				break;
			case -18://AltE
				if(draw_mode&128) break;
				//Edit palette
				palette_editor();
				changed=1;
				break;
			case 'G'://G
				if(draw_mode&128) break;
				//Global info
				global_info();
				changed=1;
				update_menu=update_view=1;
				break;
			case -84://ShF1
				if(draw_mode&128) break;
				//Show invisible walls
				//Loop, fiddling with global chars and showing.
				t1=id_chars[71];
				overlay_mode|=128;
				do {
					id_chars[71]=178;
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
					id_chars[71]=176;
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
				} while(!keywaiting());
				overlay_mode&=~128;
				getkey();
				id_chars[71]=t1;
				update_view=1;
				break;
			case -85://ShF2
				if(draw_mode&128) break;
				//Show robots
				//Loop, fiddling with global chars and showing.
				overlay_mode|=128;
				do {
					id_chars[123]='!';
					id_chars[124]='!';
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
					id_chars[123]=0;
					id_chars[124]=0;
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
				} while(!keywaiting());
				overlay_mode&=~128;
				getkey();
				update_menu=1;//Redraw debug menu if needed
				break;
			case -86://ShF3
				if(draw_mode&128) break;
				//Show fakes
				//Loop, fiddling with global chars and showing.
				t1=id_chars[13];
				t2=id_chars[14];
				t3=id_chars[15];
				t4=id_chars[16];
				overlay_mode|=128;
				do {
					for(t5=13;t5<20;t5++) id_chars[t5]='#';
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
					for(t5=13;t5<20;t5++) id_chars[t5]=177;
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
				} while(!keywaiting());
				overlay_mode&=~128;
				getkey();
				id_chars[13]=t1;
				id_chars[14]=t2;
				id_chars[15]=t3;
				id_chars[16]=t4;
				id_chars[17]=255;
				id_chars[18]=0;
				id_chars[19]=0;
				update_view=1;
				break;
			case -87://ShF4
				if(draw_mode&128) break;
				//Show spaces
				//Loop, fiddling with global chars and showing.
				t1=id_chars[0];
				overlay_mode|=128;
				do {
					id_chars[0]='O';
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
					id_chars[0]='*';
					draw_edit_window(x_top_pos,y_top_pos,current_pg_seg);
				} while(!keywaiting());
				overlay_mode&=~128;
				getkey();
				id_chars[0]=t1;
				update_view=1;
				break;
			case -90://ShF7
				set_counter("SMZX_MODE",smzx_mode + 1, 0);
				update_view=update_menu=1;
				break;
			case 'I'://I
				if(draw_mode&128) break;
				//Board info
				board_info();
				changed=1;
				break;
			case -25://AltP
				if(draw_mode&128) break;
				t1=board_xsiz;
				t2=board_ysiz;
				t3=max_bxsiz;
				t4=max_bysiz;
				//Size/pos
				size_pos();
				//Fix x/y
				if(ARRAY_X>=board_xsiz) x_pos=x_top_pos=0;
				if(ARRAY_Y>=board_ysiz) y_pos=y_top_pos=0;
				if((x_top_pos+80)>board_xsiz) x_top_pos=0;
				if((y_top_pos+19)>board_ysiz) x_top_pos=0;

				//Board size change?
				if((t1!=board_xsiz)||(t2!=board_ysiz)||(t3!=max_bxsiz)||
				 (t4!=max_bysiz)) {
					//Yep. save board to file, using old sizes.
					fp=fopen("~EDITRSZ.TMP","wb+");
					if(fp==NULL) {
						error("Error accessing temp file, ~EDITRSZ.TMP",1,24,
							current_pg_seg,0x3601);
						break;
						}
					for(t5=0;t5<t1;t5++) {
						for(t6=0;t6<t2;t6++) {
							fputc(level_id[t5+t6*t3],fp);
							fputc(level_color[t5+t6*t3],fp);
							fputc(level_param[t5+t6*t3],fp);
							fputc(level_under_id[t5+t6*t3],fp);
							fputc(level_under_color[t5+t6*t3],fp);
							fputc(level_under_param[t5+t6*t3],fp);
							fputc(overlay[t5+t6*t3],fp);
							fputc(overlay_color[t5+t6*t3],fp);
							}
						}
					fseek(fp,0,SEEK_SET);
					//Now clear board...
					for(t5=0;t5<10000;t5++) {
						level_id[t5]=level_under_id[t5]=0;
						level_param[t5]=level_under_param[t5]=0;
						level_color[t5]=level_under_color[t5]=overlay_color[t5]=7;
						overlay[t5]=32;
						}
					t0=0;//We didn't clear the player yet
					//And reload
					for(t5=0;t5<t1;t5++) {
						for(t6=0;t6<t2;t6++) {
							if((t5>=board_xsiz)||(t6>=board_ysiz)) {
								//Not being placed. If a robot/scroll, delete
								t7=fgetc(fp);
								t8=fgetc(fp);//color
								t9=fgetc(fp);//param
								if(t7==127) t0=1;//player
								else if(t7==122) {//(sensor)
									//Clear, copying to 0 first if param==curr
									if((t9==curr_param)&&
										(curr_thing==122)) {
											copy_sensor(0,curr_param);
											curr_param=0;
											}
									clear_sensor(t9);
									}
								else if((t7==123)||(t7==124)) {//(robot)
									//Clear, copying to 0 first if param==curr
									if((t9==curr_param)&&
										((curr_thing==123)||(curr_thing==124))) {
											copy_robot(0,curr_param);
											curr_param=0;
											}
									clear_robot(t9);
									}
								else if((t7==125)||(t7==126)) {//(scroll)
									//Clear, copying to 0 first if param==curr
									if((t9==curr_param)&&
										((curr_thing==125)||(curr_thing==126))) {
											copy_scroll(0,curr_param);
											curr_param=0;
											}
									clear_scroll(t9);
									}
								//Under...
								t7=fgetc(fp);
								t8=fgetc(fp);
								t9=fgetc(fp);
								if(t7==122) {//(sensor)
									//Clear, copying to 0 first if param==curr
									if((t9==curr_param)&&
										(curr_thing==122)) {
											copy_sensor(0,curr_param);
											curr_param=0;
											}
									clear_sensor(t9);
									}
								//Overlay...
								fgetc(fp);
								fgetc(fp);
								}
							else {
								//Place into new board
								t7=t5+t6*max_bxsiz;
								level_id[t7]=fgetc(fp);
								level_color[t7]=fgetc(fp);
								level_param[t7]=fgetc(fp);
								level_under_id[t7]=fgetc(fp);
								level_under_color[t7]=fgetc(fp);
								level_under_param[t7]=fgetc(fp);
								overlay[t7]=fgetc(fp);
								overlay_color[t7]=fgetc(fp);
								}
							}
						}
					if(t0) {
						//Place player at first empty space
						t4=0;
						for(t1=0;t1<board_xsiz;t1++) {
							for(t2=0;t2<board_ysiz;t2++) {
								t3=level_id[t1+t2*max_bxsiz];
								if(t3<120) t4=t1+t2*max_bxsiz;
								if(level_id[t1+t2*max_bxsiz]) continue;
								id_place(t1,t2,127,0,0);
								goto donneth;
								}
							}
						//Put at first non-robot/scroll/sign/sensor
						offs_place_id(t4,0,127,0,0);
						}
					fclose(fp);
					unlink("~EDITRSZ.TMP");
					}
			donneth:
				changed=1;
				update_menu=update_view=1;
				break;
			case 'X'://X
				if(draw_mode&128) break;
				//Exits
				board_exits();
				changed=1;
				break;
			case -31://AltS
				if(draw_mode&128) {
					//Whether to show level in overlay mode
					update_view=update_menu=1;
					show_level^=1;
					break;
					}
				//Status counter info
				status_counter_info();
				changed=1;
				break;
			case -19://AltR
				if(draw_mode&128) break;
				//Restart
				if(!confirm("Clear ALL- Are you sure?")) {
					//If current object is a robot/etc of not #0, copy to #0.
					if((curr_thing==122)&&(curr_param!=0)) {
						copy_sensor(0,curr_param);
						curr_param=0;
						}
					if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
						//No room = forget it!
						if(!copy_robot(0,curr_param)) curr_param=0;
					if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
						//No room = forget it!
						if(!copy_scroll(0,curr_param)) curr_param=0;
					//If pw-protection is NO_SAVING, clear curr_objects
					//if(protection_method==NO_SAVING) {
					//	clear_zero_objects();
					//	curr_thing=curr_param=0;
					//	curr_color=7;
					//	}
					clear_world();
					changed=0;
					update_menu=update_view=1;
					draw_mode=0;
					}
				break;
			case 'L'://L
				if(draw_mode&128) break;
				//Changed?
				if(changed)
					if(confirm("Load: World has not been saved, are you sure?")) break;
				//Load world
				if(!choose_file("*.MZX",temp,"Choose world to load",1)) {
					//If current object is a robot/etc of not #0, copy to #0.
					if((curr_thing==122)&&(curr_param!=0)) {
						copy_sensor(0,curr_param);
						curr_param=0;
						}
					if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
						//No room = forget it!
						if(!copy_robot(0,curr_param)) curr_param=0;
					if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
						//No room = forget it!
						if(!copy_scroll(0,curr_param)) curr_param=0;
					//If pw-protection is NO_SAVING, clear curr_objects
					//if(protection_method==NO_SAVING) {
					//	clear_zero_objects();
					//	curr_thing=curr_param=0;
					//	curr_color=7;
					//	}
					//Swap out current board...
					store_current();
					clear_current();
					//Load game
					if(load_world(temp,1)) {
						select_current(curr_board);
						break;
						}
					//Swap in starting board
					if(board_where[first_board]!=W_NOWHERE)
						select_current(first_board);
					else select_current(0);
					changed=0;
					update_menu=update_view=1;
					x_pos=x_top_pos=y_pos=y_top_pos=draw_mode=0;
					//Copy filename
					str_cpy(curr_file,temp);
					}
				break;
			case 'S'://S
				if(draw_mode&128) break;
				//Pw check
				//if(protection_method==NO_SAVING)
				//	if(check_pw()) break;
				//Save world
				if(!save_world_dialog()) {
					//Name in curr_file...
					if(curr_file[0]==0) break;
					add_ext(curr_file,".MZX");
					//Check for an overwrite
					if((fp=fopen(curr_file,"rb"))!=NULL) {
						fclose(fp);
						if(confirm("File exists- Overwrite?")) break;
						}
					//If current object is a robot/etc of not #0, copy to #0.
					if((curr_thing==122)&&(curr_param!=0)) {
						copy_sensor(0,curr_param);
						curr_param=0;
						}
					if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
						//No room = forget it!
						if(!copy_robot(0,curr_param)) curr_param=0;
					if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
						//No room = forget it!
						if(!copy_scroll(0,curr_param)) curr_param=0;
					//Store current
					store_current();
					//Save entire game
					save_world(curr_file);
					//Reload current
					select_current(curr_board);
					changed=0;
					update_view=update_menu=1;
					}
				break;
			case 'D'://D
				if(draw_mode&128) break;
				//Delete board
				//Choose which one...
				t1=choose_board(curr_board,"Delete board:",current_pg_seg);
				if(t1>0) {
					if(confirm("DELETE BOARD- Are you sure?")) break;
					//Deleting board t1. Current?
					if(curr_board==t1) {
						//Fix objects and swap to 0
						//If current object is a robot/etc of not #0, copy to #0.
						if((curr_thing==122)&&(curr_param!=0)) {
							copy_sensor(0,curr_param);
							curr_param=0;
							}
						if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
							//No room = forget it!
							if(!copy_robot(0,curr_param)) curr_param=0;
						if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
							//No room = forget it!
							if(!copy_scroll(0,curr_param)) curr_param=0;
						swap_with(0);
						//Update x/y
						if(ARRAY_X>=board_xsiz) x_pos=x_top_pos=0;
						if(ARRAY_Y>=board_ysiz) y_pos=y_top_pos=0;
						if((x_top_pos+80)>board_xsiz) x_top_pos=0;
						if((y_top_pos+19)>board_ysiz) x_top_pos=0;
						}
					//Delete it if it isn't already
					if(board_where[t1]!=W_NOWHERE) delete_board(t1);
					//Fix starting board if neccessary
					if(t1==first_board) first_board=0;
					changed=1;
					}
				update_view=update_menu=1;
				break;
			case 'B'://B
				if(draw_mode&128) break;
				//Select board
				//If current object is a robot/etc of not #0, copy to #0.
				if((curr_thing==122)&&(curr_param!=0)) {
					copy_sensor(0,curr_param);
					curr_param=0;
					}
				if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
					//No room = forget it!
					if(!copy_robot(0,curr_param)) curr_param=0;
				if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
					//No room = forget it!
					if(!copy_scroll(0,curr_param)) curr_param=0;
				//Choose...
				t1=choose_board(curr_board,"Select current board:",current_pg_seg);
				if(t1>=0) swap_with(t1);
				draw_mode=0;
				update_view=update_menu=1;
				//Update x/y
				if(ARRAY_X>=board_xsiz) x_pos=x_top_pos=0;
				if(ARRAY_Y>=board_ysiz) y_pos=y_top_pos=0;
				if((x_top_pos+80)>board_xsiz) x_top_pos=0;
				if((y_top_pos+19)>board_ysiz) y_top_pos=0;
				break;
			case 'A'://A
				if(draw_mode&128) break;
				//Add board
				//Search for empty...
				for(t1=0;t1<NUM_BOARDS;t1++)
					if(board_where[t1]==W_NOWHERE) break;
				if(t1>=NUM_BOARDS) {
					error("No available boards",1,24,current_pg_seg,0x0B01);
					break;
					}
				//Input a name
				m_hide();
				save_screen(current_pg_seg);
				draw_window_box(16,12,64,14,current_pg_seg,EC_DEBUG_BOX,
					EC_DEBUG_BOX_DARK,EC_DEBUG_BOX_CORNER);
				write_string("Name for new board:",18,13,EC_DEBUG_LABEL,
					current_pg_seg);
				update_menu=update_view=1;
				board_list[t1*BOARD_NAME_SIZE]=0;
				m_show();
				if(intake(&board_list[t1*BOARD_NAME_SIZE],BOARD_NAME_SIZE-1,
					38,13,current_pg_seg,15,1,0)==27) {
						restore_screen(current_pg_seg);
						break;
						}
				restore_screen(current_pg_seg);
				//If current object is a robot/etc of not #0, copy to #0.
				if((curr_thing==122)&&(curr_param!=0)) {
					copy_sensor(0,curr_param);
					curr_param=0;
					}
				if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
					//No room = forget it!
					if(!copy_robot(0,curr_param)) curr_param=0;
				if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
					//No room = forget it!
					if(!copy_scroll(0,curr_param)) curr_param=0;
				//Store current
				store_current();
				//Add at number t1
				clear_current_and_select(t1);
				board_where[curr_board=t1]=W_CURRENT;
				board_sizes[t1]=0;
				changed=1;
				//No need to fix x/y position since sizing info is taken
				//from current board.
				break;
			case 27://ESC
				//ESC cancels draw mode if any, instead of exiting
				if((draw_mode&127)>0) {
					draw_mode&=128;
					update_view=update_menu=1;
					key=0;
					}
				//If none, tries to cancel overlay mode
				else if(draw_mode&128) {
					key=0;
					goto overlay_off;
					}
				//Else we will exit-
				else if(changed)
					if(confirm("Exit: World has not been saved, are you sure?")) {
						key=0;
						break;
						}
				break;
			case -81://Pgdn
			next_menu:
				if(draw_mode&128) break;
				if(curr_menu<(NUM_MENUS-1)) curr_menu++;
				else curr_menu=0;
				update_menu=1;
				break;
			case -73://Pgup
			prev_menu:
				if(draw_mode&128) break;
				if(curr_menu>0) curr_menu--;
				else curr_menu=NUM_MENUS-1;
				update_menu=1;
				break;
			case -71://Home
				x_pos=y_pos=x_top_pos=y_top_pos=0;
				update_view=1;
				debug_x=65;
				break;
			case -79://End
				if(board_xsiz<80) {
					x_top_pos=0;
					x_pos=board_xsiz-1;
					}
				else {
					x_pos=79;
					x_top_pos=board_xsiz-x_pos-1;
					}
				if(board_ysiz<19) {
					y_top_pos=0;
					y_pos=board_ysiz-1;
					}
				else {
					y_pos=18;
					y_top_pos=board_ysiz-y_pos-1;
					}
				update_view=1;
				debug_x=0;
				break;
			case -77://Right
			case -157://AltRight
			move_right:
				if(key==-157) t1=10;
				else t1=1;
				for(;t1>0;t1--) {//By amount
				//At edge?
				if(ARRAY_X<(board_xsiz-1)) {
					//Nope- Do a little incrementing.
					//If x position < 74, move right one.
					//Else scroll right one, unless can't, then
					//move right one.
					if(x_pos<74) x_pos++;
					else {
						if((x_top_pos+80)<board_xsiz) {
							x_top_pos++;
							update_view=1;
							}
						else x_pos++;
						}
					//Move debug window?
					if((debug_x==65)&&(x_pos>59)) {
						debug_x=0;
						update_view=1;
						}
					//Draw? (only if not last movement)
					if((t1)&&((draw_mode&127)==1)) {
						if(draw_mode&128) {
							overlay[ARRAY_OFFS]=curr_param;
							overlay_color[ARRAY_OFFS]=curr_color;
							update_view=1;
							}
						else if(level_id[ARRAY_OFFS]!=127) {//No overwriting player
							//If a robot/scroll/sensor...
							if(curr_thing==122) {//(sensor)
								//...make a copy...
								if(!(t2=find_sensor())) {
									error("No available sensors",1,24,current_pg_seg,0x0801);
									break;
									}
								copy_sensor(t2,curr_param);
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_sensor(0);
								curr_param=t2;
								}
							if((curr_thing==123)||(curr_thing==124)) {//(robot)
								//...make a copy...
								if(!(t2=find_robot())) {
									error("No available robots",1,24,current_pg_seg,0x0901);
									break;
									}
								if(copy_robot(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0502);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_robot(0);
								curr_param=t2;
								}
							if((curr_thing==125)||(curr_thing==126)) {//(scroll)
								//...make a copy...
								if(!(t2=find_scroll())) {
									error("No available scrolls",1,24,current_pg_seg,0x0A01);
									break;
									}
								if(copy_scroll(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0503);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_scroll(0);
								curr_param=t2;
								}
							//Search and erase old players if placing a player.
							if(curr_thing==127)
								for(t2=0;t2<board_xsiz;t2++)
									for(t3=0;t3<board_ysiz;t3++)
										if(level_id[t2+t3*max_bxsiz]==127)
											id_remove_top(t2,t3);
							//If top thing at position is a robot/etc, deallocate it.
							t2=level_id[ARRAY_OFFS];
							if((t2==122)&&(curr_thing!=127)) //(sensor)
								clear_sensor(level_param[ARRAY_OFFS]);
							else if((t2==123)||(t2==124)) //(robot)
								clear_robot(level_param[ARRAY_OFFS]);
							else if((t2==125)||(t2==126)) //(scroll)
								clear_scroll(level_param[ARRAY_OFFS]);
							//Place.
							id_place(ARRAY_X,ARRAY_Y,curr_thing,curr_color,curr_param);
							}
						}
					}
					}
				if((draw_mode&127)==1) goto place;
				if((draw_mode&127)==3) update_view=1;
				break;
			case -75://Left
			case -155://AltLeft
				if(key==-155) t1=10;
				else t1=1;
				for(;t1>0;t1--) {//By amount
				//At edge?
				if(ARRAY_X>0) {
					//Nope- Do a little decrementing.
					//If x position > 5, move left one.
					//Else scroll left one, unless can't, then
					//move left one.
					if(x_pos>5) x_pos--;
					else {
						if(x_top_pos>0) {
							x_top_pos--;
							update_view=1;
							}
						else x_pos--;
						}
					//Move debug window?
					if((debug_x==0)&&(x_pos<20)) {
						debug_x=65;
						update_view=1;
						}
					//Draw? (only if not last movement)
					if((t1)&&((draw_mode&127)==1)) {
						if(draw_mode&128) {
							overlay[ARRAY_OFFS]=curr_param;
							overlay_color[ARRAY_OFFS]=curr_color;
							update_view=1;
							}
						else if(level_id[ARRAY_OFFS]!=127) {//No overwriting player
							//If a robot/scroll/sensor...
							if(curr_thing==122) {//(sensor)
								//...make a copy...
								if(!(t2=find_sensor())) {
									error("No available sensors",1,24,current_pg_seg,0x0801);
									break;
									}
								copy_sensor(t2,curr_param);
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_sensor(0);
								curr_param=t2;
								}
							if((curr_thing==123)||(curr_thing==124)) {//(robot)
								//...make a copy...
								if(!(t2=find_robot())) {
									error("No available robots",1,24,current_pg_seg,0x0901);
									break;
									}
								if(copy_robot(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0502);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_robot(0);
								curr_param=t2;
								}
							if((curr_thing==125)||(curr_thing==126)) {//(scroll)
								//...make a copy...
								if(!(t2=find_scroll())) {
									error("No available scrolls",1,24,current_pg_seg,0x0A01);
									break;
									}
								if(copy_scroll(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0503);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_scroll(0);
								curr_param=t2;
								}
							//Search and erase old players if placing a player.
							if(curr_thing==127)
								for(t2=0;t2<board_xsiz;t2++)
									for(t3=0;t3<board_ysiz;t3++)
										if(level_id[t2+t3*max_bxsiz]==127)
											id_remove_top(t2,t3);
							//If top thing at position is a robot/etc, deallocate it.
							t2=level_id[ARRAY_OFFS];
							if((t2==122)&&(curr_thing!=127)) //(sensor)
								clear_sensor(level_param[ARRAY_OFFS]);
							else if((t2==123)||(t2==124)) //(robot)
								clear_robot(level_param[ARRAY_OFFS]);
							else if((t2==125)||(t2==126)) //(scroll)
								clear_scroll(level_param[ARRAY_OFFS]);
							//Place.
							id_place(ARRAY_X,ARRAY_Y,curr_thing,curr_color,curr_param);
							}
						}
					}
					}
				if((draw_mode&127)==1) goto place;
				if((draw_mode&127)==3) update_view=1;
				break;
			case 8://Backspace
				//Delete to the left...first move left in text mode
				//At edge?
				if((ARRAY_X>0)&&((draw_mode&127)==2)) {
					//Nope- Do a little decrementing.
					//If x position > 5, move left one.
					//Else scroll left one, unless can't, then
					//move left one.
					if(x_pos>5) x_pos--;
					else {
						if(x_top_pos>0) x_top_pos--;
						else x_pos--;
						}
					//Move debug window?
					if((debug_x==0)&&(x_pos<20)) debug_x=65;
					}
				//Jump to delete
				goto erase;
			case -80://Down
			case -160://AltDown
			move_down:
				if(key==-160) t1=10;
				else t1=1;
				for(;t1>0;t1--) {//By amount
				//At edge?
				if(ARRAY_Y<(board_ysiz-1)) {
					//Nope- Do a little incrementing.
					//If y position < 14, move down one.
					//Else scroll down one, unless can't, then
					//move down one.
					if(y_pos<14) y_pos++;
					else {
						if((y_top_pos+19)<board_ysiz) {
							y_top_pos++;
							update_view=1;
							}
						else y_pos++;
						}
					//Draw? (only if not last movement)
					if((t1)&&((draw_mode&127)==1)) {
						if(draw_mode&128) {
							overlay[ARRAY_OFFS]=curr_param;
							overlay_color[ARRAY_OFFS]=curr_color;
							update_view=1;
							}
						else if(level_id[ARRAY_OFFS]!=127) {//No overwriting player
							//If a robot/scroll/sensor...
							if(curr_thing==122) {//(sensor)
								//...make a copy...
								if(!(t2=find_sensor())) {
									error("No available sensors",1,24,current_pg_seg,0x0801);
									break;
									}
								copy_sensor(t2,curr_param);
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_sensor(0);
								curr_param=t2;
								}
							if((curr_thing==123)||(curr_thing==124)) {//(robot)
								//...make a copy...
								if(!(t2=find_robot())) {
									error("No available robots",1,24,current_pg_seg,0x0901);
									break;
									}
								if(copy_robot(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0502);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_robot(0);
								curr_param=t2;
								}
							if((curr_thing==125)||(curr_thing==126)) {//(scroll)
								//...make a copy...
								if(!(t2=find_scroll())) {
									error("No available scrolls",1,24,current_pg_seg,0x0A01);
									break;
									}
								if(copy_scroll(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0503);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_scroll(0);
								curr_param=t2;
								}
							//Search and erase old players if placing a player.
							if(curr_thing==127)
								for(t2=0;t2<board_xsiz;t2++)
									for(t3=0;t3<board_ysiz;t3++)
										if(level_id[t2+t3*max_bxsiz]==127)
											id_remove_top(t2,t3);
							//If top thing at position is a robot/etc, deallocate it.
							t2=level_id[ARRAY_OFFS];
							if((t2==122)&&(curr_thing!=127)) //(sensor)
								clear_sensor(level_param[ARRAY_OFFS]);
							else if((t2==123)||(t2==124)) //(robot)
								clear_robot(level_param[ARRAY_OFFS]);
							else if((t2==125)||(t2==126)) //(scroll)
								clear_scroll(level_param[ARRAY_OFFS]);
							//Place.
							id_place(ARRAY_X,ARRAY_Y,curr_thing,curr_color,curr_param);
							}
						}
					}
					}
				if((draw_mode&127)==1) goto place;
				if((draw_mode&127)==3) update_view=1;
				break;
			case -72://Up
			case -152://AltUp
				if(key==-152) t1=10;
				else t1=1;
				for(;t1>0;t1--) {//By amount
				//At edge?
				if(ARRAY_Y>0) {
					//Nope- Do a little decrementing.
					//If y position > 4, move up one.
					//Else scroll up one, unless can't, then
					//move up one.
					if(y_pos>4) y_pos--;
					else {
						if(y_top_pos>0) {
							y_top_pos--;
							update_view=1;
							}
						else y_pos--;
						}
					//Draw? (only if not last movement)
					if((t1)&&((draw_mode&127)==1)) {
						if(draw_mode&128) {
							overlay[ARRAY_OFFS]=curr_param;
							overlay_color[ARRAY_OFFS]=curr_color;
							update_view=1;
							}
						else if(level_id[ARRAY_OFFS]!=127) {//No overwriting player
							//If a robot/scroll/sensor...
							if(curr_thing==122) {//(sensor)
								//...make a copy...
								if(!(t2=find_sensor())) {
									error("No available sensors",1,24,current_pg_seg,0x0801);
									break;
									}
								copy_sensor(t2,curr_param);
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_sensor(0);
								curr_param=t2;
								}
							if((curr_thing==123)||(curr_thing==124)) {//(robot)
								//...make a copy...
								if(!(t2=find_robot())) {
									error("No available robots",1,24,current_pg_seg,0x0901);
									break;
									}
								if(copy_robot(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0502);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_robot(0);
								curr_param=t2;
								}
							if((curr_thing==125)||(curr_thing==126)) {//(scroll)
								//...make a copy...
								if(!(t2=find_scroll())) {
									error("No available scrolls",1,24,current_pg_seg,0x0A01);
									break;
									}
								if(copy_scroll(t2,curr_param)) {
									robots[t2].used=0;
									error("Out of robot memory",1,21,current_pg_seg,0x0503);
									break;
									}
								//...and deallocate original if it was number 0.
								if(curr_param==0) clear_scroll(0);
								curr_param=t2;
								}
							//Search and erase old players if placing a player.
							if(curr_thing==127)
								for(t2=0;t2<board_xsiz;t2++)
									for(t3=0;t3<board_ysiz;t3++)
										if(level_id[t2+t3*max_bxsiz]==127)
											id_remove_top(t2,t3);
							//If top thing at position is a robot/etc, deallocate it.
							t2=level_id[ARRAY_OFFS];
							if((t2==122)&&(curr_thing!=127)) //(sensor)
								clear_sensor(level_param[ARRAY_OFFS]);
							else if((t2==123)||(t2==124)) //(robot)
								clear_robot(level_param[ARRAY_OFFS]);
							else if((t2==125)||(t2==126)) //(scroll)
								clear_scroll(level_param[ARRAY_OFFS]);
							//Place.
							id_place(ARRAY_X,ARRAY_Y,curr_thing,curr_color,curr_param);
							}
						}
					}
					}
				if((draw_mode&127)==1) goto place;
				if((draw_mode&127)==3) update_view=1;
				break;
			case -21://AltY
				//Toggle debug
				debug_mode=!debug_mode;
				update_view=update_menu=1;
				break;
			case -32://AltD
				if(draw_mode&128) break;
				//Toggle default colors
				def_color_mode=!def_color_mode;
				update_menu=1;
				break;
			case 32://Space
				//Place, making copies properly for robots/scrolls/sensors,
				//not deleting the player, and moving the player from it's
				//original position if placed.
				//If same id/color, erase
				if(draw_mode&128) {
					if((overlay[ARRAY_OFFS]==curr_param)&&
						(overlay_color[ARRAY_OFFS]==curr_color)) goto erase;
					}
				else {
					if((level_id[ARRAY_OFFS]==curr_thing)&&
						(level_color[ARRAY_OFFS]==curr_color)) goto erase;
					}
			place:
				changed=1;
				//(place: label here since drawing and new things should
				//ALWAYS work)
				if(draw_mode&128) {
					overlay[ARRAY_OFFS]=curr_param;
					overlay_color[ARRAY_OFFS]=curr_color;
					update_view=1;
					break;
					}
				if(level_id[ARRAY_OFFS]==127) break;//No overwriting player
				//If a robot/scroll/sensor...
				if(curr_thing==122) {//(sensor)
					//...make a copy...
					if(!(t1=find_sensor())) {
						error("No available sensors",1,24,current_pg_seg,0x0801);
						break;
						}
					copy_sensor(t1,curr_param);
					//...and deallocate original if it was number 0.
					if(curr_param==0) clear_sensor(0);
					curr_param=t1;
					}
				if((curr_thing==123)||(curr_thing==124)) {//(robot)
					//...make a copy...
					if(!(t1=find_robot())) {
						error("No available robots",1,24,current_pg_seg,0x0901);
						break;
						}
					if(copy_robot(t1,curr_param)) {
						robots[t1].used=0;
						error("Out of robot memory",1,21,current_pg_seg,0x0502);
						break;
						}
					//...and deallocate original if it was number 0.
					if(curr_param==0) clear_robot(0);
					curr_param=t1;
					}
				if((curr_thing==125)||(curr_thing==126)) {//(scroll)
					//...make a copy...
					if(!(t1=find_scroll())) {
						error("No available scrolls",1,24,current_pg_seg,0x0A01);
						break;
						}
					if(copy_scroll(t1,curr_param)) {
						robots[t1].used=0;
						error("Out of robot memory",1,21,current_pg_seg,0x0503);
						break;
						}
					//...and deallocate original if it was number 0.
					if(curr_param==0) clear_scroll(0);
					curr_param=t1;
					}
				//Search and erase old players if placing a player.
				if(curr_thing==127)
					for(t1=0;t1<board_xsiz;t1++)
						for(t2=0;t2<board_ysiz;t2++)
							if(level_id[t1+t2*max_bxsiz]==127)
								id_remove_top(t1,t2);
				//If top thing at position is a robot/etc, deallocate it.
				t1=level_id[ARRAY_OFFS];
				if((t1==122)&&(curr_thing!=127)) //(sensor)
					clear_sensor(level_param[ARRAY_OFFS]);
				else if((t1==123)||(t1==124)) //(robot)
					clear_robot(level_param[ARRAY_OFFS]);
				else if((t1==125)||(t1==126)) //(scroll)
					clear_scroll(level_param[ARRAY_OFFS]);
				//Place.
				id_place(ARRAY_X,ARRAY_Y,curr_thing,curr_color,curr_param);
				update_view=1;
				break;
			case -83://Del
			erase:
				changed=1;
				//Erase
				if(draw_mode&128) {
					overlay[ARRAY_OFFS]=32;
					overlay_color[ARRAY_OFFS]=7;
					update_view=1;
					break;
					}
				//Special for robots/scrolls/sensors
				t1=level_id[ARRAY_OFFS];
				if(t1==122) {//(sensor)
					//Clear, copying to 0 first if param==curr
					if((level_param[ARRAY_OFFS]==curr_param)&&
						(curr_thing==122)) {
							copy_sensor(0,curr_param);
							curr_param=0;
							}
					clear_sensor(level_param[ARRAY_OFFS]);
					}
				else if((t1==123)||(t1==124)) {//(robot)
					//Clear, copying to 0 first if param==curr
					if((level_param[ARRAY_OFFS]==curr_param)&&
						((curr_thing==123)||(curr_thing==124))) {
							copy_robot(0,curr_param);
							curr_param=0;
							}
					clear_robot(level_param[ARRAY_OFFS]);
					}
				else if((t1==125)||(t1==126)) {//(scroll)
					//Clear, copying to 0 first if param==curr
					if((level_param[ARRAY_OFFS]==curr_param)&&
						((curr_thing==125)||(curr_thing==126))) {
							copy_scroll(0,curr_param);
							curr_param=0;
							}
					clear_scroll(level_param[ARRAY_OFFS]);
					}
				else if(t1==127) break;//No kill player
				//Clear
				id_clear(ARRAY_X,ARRAY_Y);
				update_view=1;
				break;
			case 'C'://C
			change_color:
				//Change color
				t1=color_selection(curr_color,current_pg_seg);
				if(t1>-1) {
					curr_color=t1;
					update_menu=1;
					}
				break;
			case -61://F3
			case -62://F4
			case -63://F5
			case -64://F6
			case -65://F7
			case -66://F8
			case -67://F9
			case -68://F10
				if(draw_mode&128) break;
				//Over player?
				if(level_id[ARRAY_OFFS]==127) {//No overwriting player
					error("Cannot overwrite player- move him first",0,24,
						current_pg_seg,0x3E01);
					break;
					}
				//Unselecting #0?
				if((curr_thing==122)&&(curr_param==0)) {
					clear_sensor(0);
					curr_thing=0;
					}
				else if(((curr_thing==123)||(curr_thing==124))&&(curr_param==0)) {
					clear_robot(0);
					curr_thing=0;
					}
				else if(((curr_thing==125)||(curr_thing==126))&&(curr_param==0)) {
					clear_scroll(0);
					curr_thing=0;
					}
				//Thing menus
				t1=(-key)-61;//Menu # from 0 to 7
				t2=list_menu(thing_menus[t1],20,tmenu_titles[t1],0,
					tmenu_num_choices[t1],current_pg_seg);
				if(t2>-1) {
					t1=tmenu_thing_ids[t1][t2];
					//Set param
					t2=edit_param(t1,-1);
					if(t2<0) break;//ESC
					curr_thing=t1;//Thing
					curr_param=t2;//Param
					if(def_color_mode)//Set color
						if(def_colors[curr_thing])
							curr_color=def_colors[curr_thing];
					update_menu=1;
					goto place;//Now PLACE it!
					}
				break;
			case -82://Insert
				if(draw_mode&128) {
					curr_param=overlay[ARRAY_OFFS];
					curr_color=overlay_color[ARRAY_OFFS];
					update_menu=1;
					break;
					}
				//Grab. Only special case is if current is a sensor/etc. with
				//param 0, then must free mem.
				if((curr_thing==122)&&(curr_param==0))
					clear_sensor(0);
				else if(((curr_thing==123)||(curr_thing==124))&&(curr_param==0))
					clear_robot(0);
				else if(((curr_thing==125)||(curr_thing==126))&&(curr_param==0))
					clear_scroll(0);
				//Grab (gets objects properly, as a reference to copy from)
				curr_thing=level_id[ARRAY_OFFS];
				curr_param=level_param[ARRAY_OFFS];
				curr_color=level_color[ARRAY_OFFS];
				update_menu=1;
				break;
			case -50://AltM
				if(draw_mode&128) break;
				//Modify- Edit parameter of thing UNDER cursor. If it is our
				//current object, we first copy ourselves to #0.
				if((level_id[ARRAY_OFFS]==curr_thing)&&
					(level_param[ARRAY_OFFS]==curr_param)) {
						if(curr_thing==122) {
							copy_sensor(0,curr_param);
							curr_param=0;
							}
						else if((curr_thing==123)||(curr_thing==124)) {
							//If out of mem, just don't copy!
							if(!copy_robot(0,curr_param)) curr_param=0;
							}
						else if((curr_thing==125)||(curr_thing==126)) {
							//If out of mem, just don't copy!
							if(!copy_scroll(0,curr_param)) curr_param=0;
							}
					}
				t2=edit_param(level_id[ARRAY_OFFS],level_param[ARRAY_OFFS]);
				if(t2>-1) {
					changed=1;
					level_param[ARRAY_OFFS]=t2;
					}
				break;
			case 13://Enter
				update_view=1;
				//(special for text mode)
				if((draw_mode&127)==2) {
					x_top_pos=save_x-5;
					x_pos=5;
					if(save_x<5) {
						x_top_pos=0;
						x_pos=save_x;
						}
					else if((save_x+75)>board_xsiz) {
						x_top_pos=board_xsiz-80;
						x_pos=save_x-x_top_pos;
						if(x_top_pos<0) {
							x_top_pos=0;
							x_pos=save_x;
							}
						if(x_pos<0) x_pos=0;
						}
					goto move_down;
					}
				if(draw_mode&128) {
					t1=char_selection(curr_param,current_pg_seg);
					if(t1>=0) curr_param=t1;
					update_menu=1;
					break;
					}
				//Modify at cursor and grab. Only special case is if current
				//is an object param #0. Grabbing objects works because our
				//copy is supposed to be an exact duplicate, and we make copies
				//during placement.
				if((curr_thing==122)&&(curr_param==0))
					clear_sensor(0);
				else if(((curr_thing==123)||(curr_thing==124))&&(curr_param==0))
					clear_robot(0);
				else if(((curr_thing==125)||(curr_thing==126))&&(curr_param==0))
					clear_scroll(0);
				//Grab
				curr_thing=level_id[ARRAY_OFFS];
				curr_param=level_param[ARRAY_OFFS];
				curr_color=level_color[ARRAY_OFFS];
				//Modify
				t2=edit_param(curr_thing,curr_param);
				if(t2>-1) {
					curr_param=t2;
					changed=1;
					}
				//Save
				level_param[ARRAY_OFFS]=curr_param;
				update_menu=1;
				break;
			case 'P'://P
				if(draw_mode&128) break;
				//Edit parameter of current thing.
				t2=edit_param(curr_thing,curr_param);
				if(t2>-1) curr_param=t2;
				update_menu=1;
				break;
			case 'R'://R
				//Redraw screen
				update_view=update_menu=1;
				break;
			case 9://Tab
				//Toggle draw
				if((draw_mode&127)==0) draw_mode=1+(draw_mode&128);
				else draw_mode&=128;
				update_menu=update_view=1;
				if(draw_mode&127) goto place;
				break;
			case -44://AltZ
				if(draw_mode&128) break;
				//Clear board
				if(!confirm("Clear board- Are you sure?")) {
					//Save current objects
					if((curr_thing==122)&&(curr_param!=0)) {
						copy_sensor(0,curr_param);
						curr_param=0;
						}
					if(((curr_thing==123)||(curr_thing==124))&&(curr_param!=0))
						//No room- forget it!
						if(!copy_robot(0,curr_param)) curr_param=0;
					if(((curr_thing==125)||(curr_thing==126))&&(curr_param!=0))
						//No room- forget it!
						if(!copy_scroll(0,curr_param)) curr_param=0;
					//No need to fix x/y position since board size remains
					clear_current();
					changed=1;
					update_view=update_menu=1;
					draw_mode=0;
					}
				break;
			case -49://AltN
				if(draw_mode&128) break;
				//Mod
				update_menu=1;
				//Turn off module if one present...
				if(mod_playing[0]!=0) end_mod();
				//...else new module
				else {
					if(choose_file(NULL,temp,"Choose a module file")) break;
					load_mod(temp);
					}
				changed=1;
				break;
			case -127://Alt8
				if(draw_mode&128) break;
				//Mod
				update_menu=1;
				//Turn off module if one present...
				if(mod_playing[0]!=0) end_mod();
				//...else new module
				str_cpy(mod_playing,"*");
				changed=1;
				break;
			case -38://AltL
				if(draw_mode&128) break;
				//Sample
				update_menu=1;
				//Choose and play sample
				if(choose_file("*.SAM",temp,"Choose a .SAM file")) break;
				play_sample(428,temp);
				break;
			case -60://F2
				//Text
				update_menu=1;
				if((draw_mode&127)==0) {
					draw_mode=2+(draw_mode&128);//Text mode
					save_x=ARRAY_X;//Remember our position for ENTER.
					}
				else draw_mode&=128;
				break;
			}
		move_cursor(x_pos,y_pos);
		cursor_solid();
		//Loop if not exiting
	} while(key!=27);
	cursor_off();
	vquick_fadeout();
	clear_sfx_queue();
	//Exit with mouse on
	error_mode=2;
	pop_context();
}

//Arrays for 'thing' menus
char tmenu_num_choices[8]={ 17,14,18,8,6,11,12,10 };
char far *tmenu_titles[8]={ "Terrains","Items","Creatures","Puzzle Pieces",
"Transport","Elements","Miscellaneous","Objects" };
char far *thing_menus[8]={//Each 'item' is 20 char long, including '\0'.
//Terrain (F3)
"Space           ~1 \0\
Normal          ~E²\0\
Solid           ~DÛ\0\
Tree            ~A\0\
Line            ~BÍ\0\
Custom Block    ~F?\0\
Breakaway       ~C±\0\
Custom Break    ~F?\0\
Fake            ~9²\0\
Carpet          ~4±\0\
Floor           ~6°\0\
Tiles           ~0þ\0\
Custom Floor    ~F?\0\
Web             ~7Å\0\
Thick Web       ~7Î\0\
Forest          ~2²\0\
Invis. Wall     ~1 ",
//Item (F4)
"Gem             ~A\0\
Magic Gem       ~E\0\
Health          ~C\0\
Ring            ~E\x9\0\
Potion          ~B–\0\
Energizer       ~D\0\
Ammo            ~3¤\0\
Bomb            ~8\0\
Key             ~F\0\
Lock            ~F\xA\0\
Coin            ~E\0\
Life            ~B›\0\
Pouch           ~7Ÿ\0\
Chest           ~6 ",
//Creature (F5)
"Snake           ~2ë\0\
Eye             ~Fì\0\
Thief           ~C\0\
Slime Blob      ~A*\0\
Runner          ~4\0\
Ghost           ~7ê\0\
Dragon          ~4\0\
Fish            ~Eà\0\
Shark           ~7\0\
Spider          ~7•\0\
Goblin          ~D\0\
Spitting Tiger  ~Bã\0\
Bear            ~6¬\0\
Bear Cub        ~6­\0\
Lazer Gun       ~4Î\0\
Bullet Gun      ~F\0\
Spinning Gun    ~F\0\
Missile Gun     ~8",
//Puzzle (F6)
"Boulder         ~7é\0\
Crate           ~6þ\0\
Custom Push     ~F?\0\
Box             ~Eþ\0\
Custom Box      ~F?\0\
Pusher          ~D\0\
Slider NS       ~D\0\
Slider EW       ~D",
//Tranport (F7)
"Stairs          ~A¢\0\
Cave            ~6¡\0\
Transport       ~E<\0\
Whirlpool       ~B—\0\
CWRotate        ~9/\0\
CCWRotate       ~9\\",
//Element (F8)
"Still Water     ~9°\0\
N Water         ~9\x18\0\
S Water         ~9\x19\0\
E Water         ~9\x1A\0\
W Water         ~9\x1B\0\
Ice             ~Bý\0\
Lava            ~C²\0\
Fire            ~E±\0\
Goop            ~8°\0\
Lit Bomb        ~8«\0\
Explosion       ~E±",
//Misc (F9)
"Door            ~6Ä\0\
Gate            ~8\0\
Ricochet Panel  ~9/\0\
Ricochet        ~A*\0\
Mine            ~C\0\
Spike           ~8\0\
Custom Hurt     ~F?\0\
Text            ~F?\0\
N Moving Wall   ~F?\0\
S Moving Wall   ~F?\0\
E Moving Wall   ~F?\0\
W Moving Wall   ~F?",
//Objects (F10)
"Robot           ~F?\0\
Pushable Robot  ~F?\0\
Player          ~B\0\
Scroll          ~Fè\0\
Sign            ~6â\0\
Sensor          ~F?\0\
Bullet          ~Fù\0\
Missile         ~8\0\
Seeker          ~A/\0\
Shooting Fire   ~E" };
char tmenu_thing_ids[8][18]={
//Terrain (F3)
{ 0,1,2,3,4,5,6,7,13,14,15,16,17,18,19,65,71 },
//Item (F4)
{ 28,29,30,31,32,33,35,36,39,40,50,66,55,27 },
//Creature (F5)
{ 80,81,82,83,84,85,86,87,88,89,90,91,94,95,60,92,93,97 },
//Puzzle (F6)
{ 8,9,10,11,12,56,57,58 },
//Tranport (F7)
{ 43,44,49,67,45,46 },
//Element (F8)
{ 20,21,22,23,24,25,26,63,34,37,38 },
//Misc (F9)
{ 41,47,72,73,74,75,76,77,51,52,53,54 },
//Objects (F10)
{ 124,123,127,126,125,122,61,62,79,78 } };

//Default colors
unsigned char def_colors[128]={
	7,0,0,10,0,0,0,0,7,6,0,0,0,0,0,0,0,0,7,7,25,25,25,25,25,59,76,6,0,0,12,14,//0-31
	11,15,24,3,8,8,239,0,0,0,0,0,0,0,0,8,8,0,14,0,0,0,0,7,0,0,0,0,4,15,8,12,//32-63
	0,2,11,31,31,31,31,0,9,10,12,8,0,0,14,10,2,15,12,10,4,7,4,14,7,7,2,11,15,15,6,6,//64-95
	0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,15,27 };//96-127

void draw_debug_box(char ypos) {
	int t1;
	draw_window_box(debug_x,ypos,debug_x+14,24,current_pg_seg,
		EC_DEBUG_BOX,EC_DEBUG_BOX_DARK,EC_DEBUG_BOX_CORNER,0);
	write_string("X/Y:     /\nBoard:\nMem:        k\nEMS:        k\nRobot memory-\n  . /61k    %",
		debug_x+1,ypos+1,EC_DEBUG_LABEL,current_pg_seg);
	write_number(curr_board,EC_DEBUG_NUMBER,debug_x+13,ypos+2,
		current_pg_seg,0,1);
	write_number((int)(farcoreleft()>>10),EC_DEBUG_NUMBER,
		debug_x+12,ypos+3,current_pg_seg,0,1);
	write_number(free_mem_EMS()<<4,EC_DEBUG_NUMBER,debug_x+12,ypos+4,
		current_pg_seg,0,1);
	t1=robot_free_mem/102;
	write_number(t1/10,EC_DEBUG_NUMBER,debug_x+2,ypos+6,current_pg_seg,
		0,1);
	write_number(t1%10,EC_DEBUG_NUMBER,debug_x+4,ypos+6,current_pg_seg,1);
  if(*mod_playing != 0)
  {
	  write_string(mod_playing,debug_x+2,ypos+7,EC_DEBUG_NUMBER,current_pg_seg);
  }
  else
  {
    write_string("(no module)",debug_x+2,ypos+7, EC_DEBUG_NUMBER, current_pg_seg);
  }
	t1=(unsigned int)(robot_free_mem*100L/62464L);
	write_number(t1,EC_DEBUG_NUMBER,debug_x+12,ypos+6,current_pg_seg,0,1);
}
