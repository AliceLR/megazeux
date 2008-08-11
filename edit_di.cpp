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

//Dialogs for the world editor.

#include "helpsys.h"
#include "intake.h"
#include "graphics.h"
#include "mouse.h"
#include "edit.h"
#include "robo_ed.h"
#include "boardmem.h"
#include "window.h"
#include "edit_di.h"
#include "data.h"
#include "const.h"
#include "struct.h"
#include <_null.h>
#include "string.h"

//Communial dialog
#define MAX_ELEMS 15
char sdi_types[MAX_ELEMS]={ DE_BUTTON,DE_BUTTON,0,0,0,0,0,0,0,0,0,0,0,0 };
char sdi_xs[MAX_ELEMS]={ 15,37,0,0,0,0,0,0,0,0,0,0,0,0 };
char sdi_ys[MAX_ELEMS]={ 15,15,0,0,0,0,0,0,0,0,0,0,0,0 };
char far *sdi_strs[MAX_ELEMS]={ "OK","Cancel",NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL };
int sdi_p1s[MAX_ELEMS]={ 0,1,0,0,0,0,0,0,0,0,0,0,0,0 };
int sdi_p2s[MAX_ELEMS]={ 0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
void far *sdi_storage[MAX_ELEMS]={ NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL };
dialog sdi={
	10,4,69,21,"Edit settings",3,
	sdi_types,sdi_xs,sdi_ys,
	sdi_strs,sdi_p1s,sdi_p2s,sdi_storage,2 };
//Internal- Reset dialog to above settings
void reset_sdi(void) {
	for(int t1=2;t1<MAX_ELEMS;t1++) {
		sdi_types[t1]=sdi_xs[t1]=sdi_ys[t1]=sdi_p1s[t1]=sdi_p2s[t1]=0;
		sdi_strs[t1]=NULL;
		sdi_storage[t1]=NULL;
		}
	sdi.curr_element=2;
	sdi.num_elements=3;
}
#define do_dialog() run_dialog(&sdi,current_pg_seg,0,2)

char far *sci_strs[NUM_STATUS_CNTRS]={
	"Status counter 1: ",
	"2: ",
	"3: ",
	"4: ",
	"5: ",
	"6: " };
//Status counter info
void status_counter_info(void) {
	int t1;
	set_context(82);
	reset_sdi();
	for(t1=0;t1<NUM_STATUS_CNTRS;t1++) {
		sdi_types[t1+2]=DE_INPUT;
		sdi_xs[t1+2]=t1?27:12;
		sdi_ys[t1+2]=5+t1;
		sdi_strs[t1+2]=sci_strs[t1];
		sdi_p1s[t1+2]=COUNTER_NAME_SIZE-1;
		sdi_storage[t1+2]=(void far *)
			&status_shown_counters[t1*COUNTER_NAME_SIZE];
		}
	sdi.num_elements=2+NUM_STATUS_CNTRS;
	do_dialog();
	pop_context();
}

char far *be_strs[4]={
	"Board to north:",
	"Board to south:",
	"Board to east:",
	"Board to west:" };
//Board exits
void board_exits(void) {
	int t1;
	int strg[4];
	set_context(83);
	reset_sdi();
	for(t1=0;t1<4;t1++) {
		sdi_types[t1+2]=DE_BOARD;
		sdi_xs[t1+2]=12;
		sdi_ys[t1+2]=4+(t1<<1);
		sdi_strs[t1+2]=be_strs[t1];
		sdi_p1s[t1+2]=1;
		sdi_storage[t1+2]=(void far *)&(strg[t1]);
		if(board_dir[t1]==NO_BOARD) strg[t1]=0;
		else strg[t1]=board_dir[t1];
		}
	sdi.num_elements=6;
	if(do_dialog()) {
		pop_context();
		return;
		}
	pop_context();
	for(t1=0;t1<4;t1++) {
		if(strg[t1]==0) board_dir[t1]=NO_BOARD;
		else board_dir[t1]=strg[t1];
		}
}

char far *sp_strs[7]={
	"Viewport X pos: ",
	"Viewport Y pos: ",
	"Viewport X size:",
	"Viewport Y size:",
	"Board X size:   ",
	"Board Y size:   ",
	"Max size 60x166\nMax size 80x125\nMax size 100x100\nMax size 200x50\nMax size 400x25" };
//Size/pos of board/viewport
void size_pos(void) {
	int t1,t2=2;
	int strg[7];
	set_context(84);
	reset_sdi();
	for(t1=0;t1<7;t1++) {
		sdi_types[t1+2]=DE_NUMBER;
		if(t1==6) sdi_types[t1+2]=DE_RADIO;
		sdi_xs[t1+2]=15;
		sdi_ys[t1+2]=t2++;
		if(t1==5) t2++;
		sdi_strs[t1+2]=sp_strs[t1];
		sdi_storage[t1+2]=(void far *)&(strg[t1]);
		if(t1<2) sdi_p1s[t1+2]=0;
		else sdi_p1s[t1+2]=1;
		}
	sdi_p1s[8]=5;
	sdi_p2s[2]=79;
	sdi_p2s[3]=24;
	sdi_p2s[4]=80;
	sdi_p2s[5]=25;
	sdi_p2s[6]=400;
	sdi_p2s[7]=166;
	sdi_p2s[8]=16;
	strg[0]=viewport_x;
	strg[1]=viewport_y;
	strg[2]=viewport_xsiz;
	strg[3]=viewport_ysiz;
	strg[4]=board_xsiz;
	strg[5]=board_ysiz;
	strg[6]=max_bsiz_mode;
	sdi_strs[1]="Center";
	sdi.num_elements=9;
	do {
	redo_it:
		t1=do_dialog();
		//Fix sizes
		if(!t1) {
			max_bsiz_mode=strg[6];
			convert_max_bsiz_mode();
			}
		if(strg[4]>max_bxsiz) strg[4]=max_bxsiz;
		if(strg[5]>max_bysiz) strg[5]=max_bysiz;
		if(strg[2]>strg[4]) strg[2]=strg[4];
		if(strg[3]>strg[5]) strg[3]=strg[5];
		if((strg[2]+strg[0])>80) strg[2]=80-strg[0];
		if((strg[3]+strg[1])>25) strg[3]=25-strg[1];
		if(t1==1) {
			//Center
			strg[0]=40-(strg[2]>>1);
			strg[1]=12-(strg[3]>>1);
			}
		else break;
	} while(1);
	//Check for size reduction
	if((!t1)&&((board_xsiz>strg[4])||(board_ysiz>strg[5])))
		if(confirm("Reduce board size- Are you sure?"))
			goto redo_it;
	sdi_strs[1]="Cancel";
	pop_context();
	if(t1) return;
	viewport_x=strg[0];
	viewport_y=strg[1];
	viewport_xsiz=strg[2];
	viewport_ysiz=strg[3];
	board_xsiz=strg[4];
	board_ysiz=strg[5];
	return;
}

//Dialog- (board info)
//----------------------------------------------------------
//        Board name- __________________________
//    [ ] Can shoot               ( ) Explosions to space
//    [ ] Can bomb                ( ) Explosions to ash
//    [ ] Fire burns space        ( ) Explosions to fire
//    [ ] Fire burns fakes
//    [ ] Fire burns trees        ( ) Can save
//    [ ] Fire burns brown        ( ) Can't save
//    [ ] Forest to floor         ( ) Can save on sensors
//    [ ] Collect bombs
//    [ ] Fire burns forever      ( ) No overlay
//    [ ] Restart if hurt         ( ) Normal overlay
//                                ( ) Static overlay
//    Time limit- _00000__!__!_   ( ) Transparent overlay
//
//              _OK_                  _Cancel_
//
//----------------------------------------------------------

char far *bi_cstr="Can shoot\nCan bomb\nFire burns space\nFire burns fakes\n\
Fire burns trees\nFire burns brown\nForest to floor\nCollect bombs\n\
Fire burns forever\nRestart if hurt";
char far *bi_rstr1="Explosions to space\nExplosions to ash\n\
Explosions to fire\n";
char far *bi_rstr2="Can save\nCan't save\nCan save on sensors";
char far *bi_rstr3="No overlay\nNormal overlay\nStatic overlay\n\
Transparent overlay";

//Board info
void board_info(void) {
	char chk[10]={ can_shoot,can_bomb,fire_burn_space,fire_burn_fakes,
		fire_burn_trees,fire_burn_brown,forest_becomes,collect_bombs,
		fire_burns,restart_if_zapped };
	int rad1=explosions_leave,rad2=save_mode,rad3=overlay_mode,
		time=time_limit;
	char tstr[BOARD_NAME_SIZE];
	set_context(85);
	reset_sdi();
	sdi_types[2]=DE_INPUT;
	sdi_types[3]=DE_CHECK;
	sdi_types[4]=DE_NUMBER;
	sdi_types[5]=DE_RADIO;
	sdi_types[6]=DE_RADIO;
	sdi_types[7]=DE_RADIO;

	sdi_xs[2]=9;
	sdi_xs[3]=5;
	sdi_xs[4]=5;
	sdi_xs[5]=33;
	sdi_xs[6]=33;
	sdi_xs[7]=33;

	sdi_ys[2]=1;
	sdi_ys[3]=2;
	sdi_ys[4]=13;
	sdi_ys[5]=2;
	sdi_ys[6]=6;
	sdi_ys[7]=10;

	sdi_strs[2]="Board name- ";
	sdi_strs[3]=bi_cstr;
	sdi_strs[4]="Time limit- ";
	sdi_strs[5]=bi_rstr1;
	sdi_strs[6]=bi_rstr2;
	sdi_strs[7]=bi_rstr3;

	sdi_p1s[2]=BOARD_NAME_SIZE-1;
	sdi_p1s[3]=10;
	sdi_p1s[5]=3;
	sdi_p1s[6]=3;
	sdi_p1s[7]=4;

	sdi_p2s[3]=18;
	sdi_p2s[4]=32767;
	sdi_p2s[5]=19;
	sdi_p2s[6]=19;
	sdi_p2s[7]=19;

	sdi_storage[2]=tstr;
	sdi_storage[3]=chk;
	sdi_storage[4]=&time;
	sdi_storage[5]=&rad1;
	sdi_storage[6]=&rad2;
	sdi_storage[7]=&rad3;

	str_cpy(tstr,&board_list[curr_board*BOARD_NAME_SIZE]);

	sdi.num_elements=8;

	if(do_dialog()) {
		pop_context();
		return;
		}

	str_cpy(&board_list[curr_board*BOARD_NAME_SIZE],tstr);
	can_shoot=chk[0];
	can_bomb=chk[1];
	fire_burn_space=chk[2];
	fire_burn_fakes=chk[3];
	fire_burn_trees=chk[4];
	fire_burn_brown=chk[5];
	forest_becomes=chk[6];
	collect_bombs=chk[7];
	fire_burns=chk[8];
	restart_if_zapped=chk[9];
	explosions_leave=rad1;
	save_mode=rad2;
	overlay_mode=rad3;
	time_limit=time;
	pop_context();
}

//Dialog- (global info)
//----------------------------------------------------------
//
//Death board-                 Endgame board-
//__________________________!!!__________________________!!!
//
//Death X- _00000__!__!_       Endgame X- _00000__!__!_
//Death Y- _00000__!__!_       Endgame Y- _00000__!__!_
//
//( ) Death- Same position     ( ) Endgame- Game over
//( ) Death- Restart board     ( ) Endgame- Teleport
//( ) Death- Teleport
//                             [ ] Play game over sfx
//
//    _More_ _Edit Chars_ _Edit Dmg_ _Edit Global Robot_
//
//              _OK_                  _Cancel_
//
//----------------------------------------------------------
void global_info(void) {
	int brd1=death_board,brd2=endgame_board;
	int dx=death_x,dy=death_y,ex=endgame_x,ey=endgame_y;
	int rad1=2,rad2=1;
	char chk[1]={ game_over_sfx };
	int t1;
	set_context(86);
redo:
	if(brd1==DEATH_SAME_POS) {
		brd1=0; rad1=0;
		}
	if(brd1==NO_DEATH_BOARD) {
		brd1=0; rad1=1;
		}
	if(brd2==NO_ENDGAME_BOARD) {
		brd2=0; rad2=0;
		}
	reset_sdi();
	sdi_types[2]=DE_BOARD;
	sdi_types[3]=DE_NUMBER;
	sdi_types[4]=DE_NUMBER;
	sdi_types[5]=DE_RADIO;
	sdi_types[6]=DE_BOARD;
	sdi_types[7]=DE_NUMBER;
	sdi_types[8]=DE_NUMBER;
	sdi_types[9]=DE_RADIO;
	sdi_types[10]=DE_CHECK;
	sdi_types[11]=DE_BUTTON;
	sdi_types[12]=DE_BUTTON;
	sdi_types[13]=DE_BUTTON;
	sdi_types[14]=DE_BUTTON;

	sdi_xs[2]=1;
	sdi_xs[3]=1;
	sdi_xs[4]=1;
	sdi_xs[5]=1;
	sdi_xs[6]=30;
	sdi_xs[7]=30;
	sdi_xs[8]=30;
	sdi_xs[9]=30;
	sdi_xs[10]=30;
	sdi_xs[11]=5;
	sdi_xs[12]=12;
	sdi_xs[13]=25;
	sdi_xs[14]=36;

	sdi_ys[2]=2;
	sdi_ys[3]=5;
	sdi_ys[4]=6;
	sdi_ys[5]=8;
	sdi_ys[6]=2;
	sdi_ys[7]=5;
	sdi_ys[8]=6;
	sdi_ys[9]=8;
	sdi_ys[10]=11;
	sdi_ys[11]=13;
	sdi_ys[12]=13;
	sdi_ys[13]=13;
	sdi_ys[14]=13;

	sdi_strs[2]="Death board-";
	sdi_strs[3]="Death X- ";
	sdi_strs[4]="Death Y- ";
	sdi_strs[5]="Death- Same position\nDeath- Restart board\nDeath- Teleport";
	sdi_strs[6]="Endgame board-";
	sdi_strs[7]="Endgame X- ";
	sdi_strs[8]="Endgame Y- ";
	sdi_strs[9]="Endgame- Game over\nEndgame- Teleport";
	sdi_strs[10]="Play game over sfx";
	sdi_strs[11]="More";
	sdi_strs[12]="Edit Chars";
	sdi_strs[13]="Edit Dmg";
	sdi_strs[14]="Edit Global Robot";

	sdi_p1s[5]=3;
	sdi_p1s[9]=2;
	sdi_p1s[10]=1;
	sdi_p1s[11]=2;
	sdi_p1s[12]=3;
	sdi_p1s[13]=4;
	sdi_p1s[14]=5;

	sdi_p2s[3]=99;
	sdi_p2s[4]=99;
	sdi_p2s[5]=20;
	sdi_p2s[7]=99;
	sdi_p2s[8]=99;
	sdi_p2s[9]=18;
	sdi_p2s[10]=18;

	sdi_storage[2]=&brd1;
	sdi_storage[3]=&dx;
	sdi_storage[4]=&dy;
	sdi_storage[5]=&rad1;
	sdi_storage[6]=&brd2;
	sdi_storage[7]=&ex;
	sdi_storage[8]=&ey;
	sdi_storage[9]=&rad2;
	sdi_storage[10]=chk;

	sdi.num_elements=15;
	t1=do_dialog();
	if((t1==-1)||(t1==1)) {
		pop_context();
		return;
		}

	if(rad1==2) death_board=brd1;
	else if(rad1==1) death_board=NO_DEATH_BOARD;
	else death_board=DEATH_SAME_POS;

	if(rad2==1) endgame_board=brd2;
	else endgame_board=NO_ENDGAME_BOARD;

	death_x=dx; endgame_x=ex;
	death_y=dy; endgame_y=ey;
	game_over_sfx=chk[0];

	if(t1==2) {
		if(global_next()) goto redo;//More
		}
	else if(t1==3) global_chars();//Chars
	else if(t1==4) global_dmg();//Dmg
	else if(t1==5) {
		m_hide();
		save_screen(current_pg_seg);
		draw_window_box(16,12,50,14,current_pg_seg,EC_DEBUG_BOX,
			EC_DEBUG_BOX_DARK,EC_DEBUG_BOX_CORNER);
		write_string("Name for robot:",18,13,EC_DEBUG_LABEL,
			current_pg_seg);
		m_show();
		if(intake(robots[NUM_ROBOTS].robot_name,14,34,13,current_pg_seg,15,1,
			0)==27) {
				restore_screen(current_pg_seg);
				pop_context();
				return;
				}
		restore_screen(current_pg_seg);
		set_context(87);
		robot_editor(NUM_ROBOTS);//Global robot
		pop_context();
		}
	pop_context();
}
//Next-
//----------------------------------------------------------
//
//     First board-
//     __________________________!!!   Edging color- _*_
//
//     Starting lives-  _00000__!__!_
//     Maximum lives-   _00000__!__!_
//
//     Starting health- _00000__!__!_
//     Maximum health-  _00000__!__!_  _Previous_
//
//     [ ] Enemies' bullets hurt other enemies
//     [ ] Clear messages and projectiles on exit
//     [ ] Can only play world from a 'SWAP WORLD'
//
//              _OK_                  _Cancel_
//
//----------------------------------------------------------
char global_next(void) {//Returns 1 for previous
	int brd=first_board;
	int col=edge_color;
	int lv=starting_lives,mlv=lives_limit;
	int hl=starting_health,mhl=health_limit;
	char chk[3]={ enemy_hurt_enemy,clear_on_exit,only_from_swap };
	int t1;

	set_context(88);
	reset_sdi();
	sdi_types[2]=DE_BOARD;
	sdi_types[3]=DE_COLOR;
	sdi_types[4]=DE_NUMBER;
	sdi_types[5]=DE_NUMBER;
	sdi_types[6]=DE_NUMBER;
	sdi_types[7]=DE_NUMBER;
	sdi_types[8]=DE_BUTTON;
	sdi_types[9]=DE_CHECK;

	sdi_xs[2]=6;
	sdi_xs[3]=38;
	sdi_xs[4]=6;
	sdi_xs[5]=6;
	sdi_xs[6]=6;
	sdi_xs[7]=6;
	sdi_xs[8]=38;
	sdi_xs[9]=6;

	sdi_ys[2]=2;
	sdi_ys[3]=3;
	sdi_ys[4]=5;
	sdi_ys[5]=6;
	sdi_ys[6]=8;
	sdi_ys[7]=9;
	sdi_ys[8]=9;
	sdi_ys[9]=11;

	sdi_strs[2]="First board-";
	sdi_strs[3]="Edging color- ";
	sdi_strs[4]="Starting lives-  ";
	sdi_strs[5]="Maximum lives-   ";
	sdi_strs[6]="Starting health- ";
	sdi_strs[7]="Maximum health-  ";
	sdi_strs[8]="Previous";
	sdi_strs[9]="Enemies' bullets hurt other enemies\n\
Clear messages and projectiles on exit\n\
Can only play world from a 'SWAP WORLD'";

	sdi_p1s[4]=1;
	sdi_p1s[5]=1;
	sdi_p1s[6]=1;
	sdi_p1s[7]=1;
	sdi_p1s[8]=2;
	sdi_p1s[9]=3;

	sdi_p2s[4]=32767;
	sdi_p2s[5]=32767;
	sdi_p2s[6]=32767;
	sdi_p2s[7]=32767;
	sdi_p2s[9]=39;

	sdi_storage[2]=&brd;
	sdi_storage[3]=&col;
	sdi_storage[4]=&lv;
	sdi_storage[5]=&mlv;
	sdi_storage[6]=&hl;
	sdi_storage[7]=&mhl;
	sdi_storage[9]=chk;

	sdi.num_elements=10;
	t1=do_dialog();
	pop_context();
	if(t1==2) return 1;

	first_board=brd;
	edge_color=col;
	starting_lives=lv;
	lives_limit=mlv;
	starting_health=hl;
	health_limit=mhl;
	enemy_hurt_enemy=chk[0];
	clear_on_exit=chk[1];
	only_from_swap=chk[2];

	if(lv>mlv) starting_lives=mlv;
	if(hl>mhl) starting_health=mhl;
	return 0;
}
//----------------------------------------------------------
// (Note- The numbers in parenthesis are the codes to use
//        with the CHANGE CHAR ID robot command)
//
//Energizer color #1 (200)-_*_  Energizer color #1 (200)-_*_
// .                             .
// .                             .
// .                             .
// .                             .
// .                             .
// .                             .
// .                             .
// .                             .
// .                             .
//
//             _Next_    _Previous_    _Done_
//
//----------------------------------------------------------
char cdi_types[27]={ DE_BUTTON,DE_BUTTON,DE_BUTTON,
	DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,
	DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,
	DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,
	DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR,DE_CHAR };
char cdi_xs[27]={ 15,25,39,2,2,2,2,2,2,2,2,2,2,2,2,32,32,32,32,32,32,32,32,
	32,32,32,32 };
char cdi_ys[27]={ 15,15,15,2,3,4,5,6,7,8,9,10,11,12,13,2,3,4,5,6,7,8,9,10,
	11,12,13 };
char far *cdi_strs[27]={ "Next","Previous","Done",NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL };
int cdi_p1s[27]={ 1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
void far *cdi_storage[27]={ NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL };
dialog cdi={
	9,4,70,21,"Edit characters",27,
	cdi_types,cdi_xs,cdi_ys,cdi_strs,cdi_p1s,NULL,cdi_storage,3 };
//Internal- Reset dialog to above settings
void reset_cdi(void) {
	for(int t1=3;t1<27;t1++) {
		cdi_types[t1]=DE_CHAR;
		cdi_p1s[t1]=0;
		cdi_strs[t1]=NULL;
		cdi_storage[t1]=NULL;
		cdi_xs[t1]=2;
		if(t1>14) cdi_xs[t1]=32;
		}
	cdi.curr_element=3;
	cdi.num_elements=27;
}
#define do_cdialog() run_dialog(&cdi,current_pg_seg,0,3)
//Info for char funcs, 192 selections (24 per func, 8 funcs)
char far *char_strs[8][24]={ {
	"Space (000)-",
	"Normal (001)-",
	"Solid (002)-",
	"Tree (003)-",
	"Breakaway (006)-",
	"Boulder (008)-",
	"Crate (009)-",
	"Box (011)-",
	"Fake (013)-",
	"Carpet (014)-",
	"Floor (015)-",
	"Tiles (016)-",
	"Still Water (020)-",
	"N Water (021)-",
	"S Water (022)-",
	"E Water (023)-",
	"W Water (024)-",
	"Chest (027)-",
	"Gem (028)-",
	"Magic Gem (029)-",
	"Health (030)-",
	"Ring (031)-",
	"Potion (032)-",
	"Energizer (033)-" },{
	"Goop (034)-",
	"Bomb (036)-",
	"Explosion (038)-",
	"Key (039)-",
	"Lock (040)-",
	"Stairs (043)-",
	"Cave (044)-",
	"Gate (047)-",
	"Open Gate (048)-",
	"Coin (050)-",
	"Pouch (055)-",
	"Slider NS (057)-",
	"Slider EW (058)-",
	"Lazer Gun (060)-",
	"Forest (065)-",
	"Whirlpool 1 (067)-",
	"Whirlpool 2 (068)-",
	"Whirlpool 3 (069)-",
	"Whirlpool 4 (070)-",
	"Invis. Wall (071)-",
	"Ricochet (073)-",
	"Snake (080)-",
	"Eye (081)-",
	"Thief (082)-"  },{
	"Slime Blob (083)-",
	"Runner (084)-",
	"Ghost (085)-",
	"Dragon (086)-",
	"Fish (087)-",
	"Shark (088)-",
	"Spider (089)-",
	"Goblin (090)-",
	"Spitting Tiger (091)-",
	"Bear (094)-",
	"Bear Cub (095)-",
	"Sign (125)-",
	"Scroll (126)-",
	"Blank Ice (160)-",
	"Ice Anim 1 (161)-",
	"Ice Anim 2 (162)-",
	"Ice Anim 3 (163)-",
	"Lava Anim 1 (164)-",
	"Lava Anim 2 (165)-",
	"Lava Anim 3 (166)-",
	"Small Ammo (167)-",
	"Large Ammo (168)-",
	"Lit Bomb Anim 1 (169)-",//Special
	"Energizer Color 1 (176)-" },{
	"Energizer Color 2 (177)-",
	"Energizer Color 3 (178)-",
	"Energizer Color 4 (179)-",
	"Energizer Color 5 (180)-",
	"Energizer Color 6 (181)-",
	"Energizer Color 7 (182)-",
	"Energizer Color 8 (183)-",
	"Explosion Stage 1 (184)-",
	"Explosion Stage 2 (185)-",
	"Explosion Stage 3 (186)-",
	"Explosion Stage 4 (187)-",
	"Horizontal Door (???)-",//Special
	"Vertical Door (???)-",//Special
	"Diagonal Door / (???)-",//Special
	"Diagonal Door \\ (???)-",//Special
	"CW Anim 1 (190)-",
	"CW Anim 2 (191)-",
	"CW Anim 3 (192)-",
	"CW Anim 4 (193)-",
	"CCW Anim 1 (194)-",
	"CCW Anim 2 (195)-",
	"CCW Anim 3 (196)-",
	"CCW Anim 4 (197)-",
	"N Transport Anim 1 (230)-" },{
	"N Transport Anim 2 (231)-",
	"N Transport Anim 3 (232)-",
	"N Transport Anim 4 (233)-",
	"S Transport Anim 1 (234)-",
	"S Transport Anim 2 (235)-",
	"S Transport Anim 3 (236)-",
	"S Transport Anim 4 (237)-",
	"E Transport Anim 1 (238)-",
	"E Transport Anim 2 (239)-",
	"E Transport Anim 3 (240)-",
	"E Transport Anim 4 (241)-",
	"W Transport Anim 1 (242)-",
	"W Transport Anim 2 (243)-",
	"W Transport Anim 3 (244)-",
	"W Transport Anim 4 (245)-",
	"A.Transport Anim 1 (246)-",
	"A.Transport Anim 2 (247)-",
	"A.Transport Anim 3 (248)-",
	"A.Transport Anim 4 (249)-",
	"N Thick Arrow (250)-",
	"S Thick Arrow (251)-",
	"E Thick Arrow (252)-",
	"W Thick Arrow (253)-",
	"N Thin Arrow (254)-" },{
	"S Thin Arrow (255)-",
	"E Thin Arrow (256)-",
	"W Thin Arrow (257)-",
	"Horiz Lazer Anim 1 (258)-",
	"Horiz Lazer Anim 2 (259)-",
	"Horiz Lazer Anim 3 (260)-",
	"Horiz Lazer Anim 4 (261)-",
	"Vert Lazer Anim 1 (262)-",
	"Vert Lazer Anim 2 (263)-",
	"Vert Lazer Anim 3 (264)-",
	"Vert Lazer Anim 4 (265)-",
	"Fire Anim 1 (266)-",
	"Fire Anim 2 (267)-",
	"Fire Anim 3 (268)-",
	"Fire Anim 4 (269)-",
	"Fire Anim 5 (270)-",
	"Fire Anim 6 (271)-",
	"Fire Color 1 (272)-",
	"Fire Color 2 (273)-",
	"Fire Color 3 (274)-",
	"Fire Color 4 (275)-",
	"Fire Color 5 (276)-",
	"Fire Color 6 (277)-",
	"Life Anim 1 (278)-" },{
	"Life Anim 2 (279)-",
	"Life Anim 3 (280)-",
	"Life Anim 4 (281)-",
	"Life Color 1 (282)-",
	"Life Color 2 (283)-",
	"Life Color 3 (284)-",
	"Life Color 4 (285)-",
	"Ricochet Panel \\ (286)-",
	"Ricochet Panel / (287)-",
	"Mine Anim 1 (288)-",
	"Mine Anim 2 (289)-",
	"Spit Fire Anim 1 (290)-",
	"Spit Fire Anim 2 (291)-",
	"Spit Fire Color 1 (292)-",
	"Spit Fire Color 2 (293)-",
	"Seeker Anim 1 (294)-",
	"Seeker Anim 2 (295)-",
	"Seeker Anim 3 (296)-",
	"Seeker Anim 4 (297)-",
	"Seeker Color 1 (298)-",
	"Seeker Color 2 (299)-",
	"Seeker Color 3 (300)-",
	"Seeker Color 4 (301)-",
	"Whirlpool Color 1 (302)-" },{
	"Whirlpool Color 2 (303)-",
	"Whirlpool Color 3 (304)-",
	"Whirlpool Color 4 (305)-",
	"N Player Bullet (306)-",
	"S Player Bullet (307)-",
	"E Player Bullet (308)-",
	"W Player Bullet (309)-",
	"N Neutral Bullet (310)-",
	"S Neutral Bullet (311)-",
	"E Neutral Bullet (312)-",
	"W Neutral Bullet (313)-",
	"N Enemy Bullet (314)-",
	"S Enemy Bullet (315)-",
	"E Enemy Bullet (316)-",
	"W Enemy Bullet (317)-",
	"N Player Char (318)-",
	"S Player Char (319)-",
	"E Player Char (320)-",
	"W Player Char (321)-",
	"Player Color (322)-",
	"Missile Color (323)-",
	"Player Bullet Color (324)-",
	"Neutral Bullet Color (325)-",
	"Enemy Bullet Color (326)-" } };
//Have bit 512 set for a color
int char_values[8][24]={ {
	0,
	1,
	2,
	3,
	6,
	8,
	9,
	11,
	13,
	14,
	15,
	16,
	20,
	21,
	22,
	23,
	24,
	27,
	28,
	29,
	30,
	31,
	32,
	33 },{
	34,
	36,
	38,
	39,
	40,
	43,
	44,
	47,
	48,
	50,
	55,
	57,
	58,
	60,
	65,
	67,
	68,
	69,
	70,
	71,
	73,
	80,
	81,
	82 },{
	83,
	84,
	85,
	86,
	87,
	88,
	89,
	90,
	91,
	94,
	95,
	125,
	126,
	160,
	161,
	162,
	163,
	164,
	165,
	166,
	167,
	168,
	169,//Special
	176+512 },{
	177+512,
	178+512,
	179+512,
	180+512,
	181+512,
	182+512,
	183+512,
	184+512,
	185+512,
	186+512,
	187+512,
	188,//Special
	189,//Special
	198,//Special
	200,//Special
	190,
	191,
	192,
	193,
	194,
	195,
	196,
	197,
	230 },{
	231,
	232,
	233,
	234,
	235,
	236,
	237,
	238,
	239,
	240,
	241,
	242,
	243,
	244,
	245,
	246,
	247,
	248,
	249,
	250,
	251,
	252,
	253,
	254 },{
	255,
	256,
	257,
	258,
	259,
	260,
	261,
	262,
	263,
	264,
	265,
	266,
	267,
	268,
	269,
	270,
	271,
	272+512,
	273+512,
	274+512,
	275+512,
	276+512,
	277+512,
	278 },{
	279,
	280,
	281,
	282+512,
	283+512,
	284+512,
	285+512,
	286,
	287,
	288,
	289,
	290,
	291,
	292+512,
	293+512,
	294,
	295,
	296,
	297,
	298+512,
	299+512,
	300+512,
	301+512,
	302+512 },{
	303+512,
	304+512,
	305+512,
	306,
	307,
	308,
	309,
	310,
	311,
	312,
	313,
	314,
	315,
	316,
	317,
	318,
	319,
	320,
	321,
	322+512,
	323+512,
	324+512,
	325+512,
	326+512 } };

//Chars #1-8
void global_chars(void) {
	int t1,t2;
	int curr_scr=0;
	int temp[24];
	set_context(89);
	do {
		reset_cdi();
		for(t1=0;t1<24;t1++) {
			//Set strs, storage, param 1, types, and X position
			if(char_values[curr_scr][t1]&512) cdi_types[t1+3]=DE_COLOR;
			else if(char_values[curr_scr][t1]>127) cdi_p1s[t1+3]=1;
			cdi_strs[t1+3]=char_strs[curr_scr][t1];
			cdi_storage[t1+3]=&temp[t1];
			temp[t1]=id_chars[char_values[curr_scr][t1]&511];
			cdi_xs[t1+3]+=(25-str_len(char_strs[curr_scr][t1]));
			}
		//Run
		t1=do_cdialog();
		if(t1==-1) {
			pop_context();
			return;
			}
		//Get from storage
		for(t2=0;t2<24;t2++)
			id_chars[char_values[curr_scr][t2]&511]=temp[t2];
		//Setup lit bomb sequence or doors
		if(curr_scr==2) {
			//Lit bomb
			for(t2=170;t2<176;t2++)
				id_chars[t2]=id_chars[t2-1]-1;
			}
		else if(curr_scr==3) {
			//Doors
			id_chars[199]=id_chars[198];// '/'
			id_chars[201]=id_chars[200];// '\'
			id_chars[202]=id_chars[200];// '\'
			id_chars[203]=id_chars[200];// '\'
			id_chars[204]=id_chars[198];// '/'
			id_chars[205]=id_chars[198];// '/'
			id_chars[206]=id_chars[189];// '|'
			id_chars[207]=id_chars[188];// '-'
			id_chars[208]=id_chars[189];// '|'
			id_chars[209]=id_chars[188];// '-'
			id_chars[210]=id_chars[189];// '|'
			id_chars[211]=id_chars[188];// '-'
			id_chars[212]=id_chars[189];// '|'
			id_chars[213]=id_chars[188];// '-'
			id_chars[214]=id_chars[189];// '|'
			id_chars[215]=id_chars[188];// '-'
			id_chars[216]=id_chars[189];// '|'
			id_chars[217]=id_chars[188];// '-'
			id_chars[218]=id_chars[189];// '|'
			id_chars[219]=id_chars[188];// '-'
			id_chars[220]=id_chars[189];// '|'
			id_chars[221]=id_chars[188];// '-'
			id_chars[222]=id_chars[198];// '/'
			id_chars[223]=id_chars[198];// '/'
			id_chars[224]=id_chars[200];// '\'
			id_chars[225]=id_chars[200];// '\'
			id_chars[226]=id_chars[200];// '\'
			id_chars[227]=id_chars[200];// '\'
			id_chars[228]=id_chars[198];// '/'
			id_chars[229]=id_chars[198];// '/'
			}
		//Next, prev, or done
		if(t1==1) {
			if((++curr_scr)>7) curr_scr=0;
			}
		if(t1==2) {
			if((--curr_scr)<0) curr_scr=7;
			}
	} while((t1==1)||(t1==2));
	pop_context();
}
//Info for damage editing
char dmg_ids[22]={
	26,
	38,
	59,
	61,
	62,
	63,
	75,
	76,
	78,
	79,
	80,
	83,
	84,
	85,
	86,
	87,
	88,
	89,
	90,
	91,
	94,
	95 };
char far *dmg_strs[24]={
	"OK","Cancel",
	"Lava-",
	"Explosion-",
	"Lazer-",
	"Bullet-",
	"Missile-",
	"Fire-",
	"Spike-",
	"Custom Hurt-",
	"Shooting Fire-",
	"Seeker-",
	"Snake-",
	"Slime Blob-",
	"Runner-",
	"Ghost-",
	"Dragon-",
	"Fish-",
	"Shark-",
	"Spider-",
	"Goblin-",
	"Spitting Tiger-",
	"Bear-",
	"Bear Cub-" };
char ddi_types[24]={ DE_BUTTON,DE_BUTTON,
	DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,
	DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,
	DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER,
	DE_NUMBER,DE_NUMBER,DE_NUMBER,DE_NUMBER };
char ddi_xs[24]={ 15,37,2,2,2,2,2,2,2,2,2,2,2,31,31,31,31,31,31,31,31,
	31,31,31 };
char ddi_ys[24]={ 15,15,2,3,4,5,6,7,8,9,10,11,12,2,3,4,5,6,7,8,9,10,
	11,12 };
int ddi_p1s[24]={ 0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
int ddi_p2s[24]={ 0,0,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255 };
void far *ddi_storage[24]={ NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL };
dialog ddi={
	10,4,69,21,"Edit damage",24,
	ddi_types,ddi_xs,ddi_ys,dmg_strs,ddi_p1s,ddi_p2s,ddi_storage,2 };
#define do_ddialog() run_dialog(&ddi,current_pg_seg,0,2)
void global_dmg(void) {
	int t1;
	int tmp[22];
	set_context(90);
	for(t1=0;t1<22;t1++) {
		ddi_storage[t1+2]=&tmp[t1];
		tmp[t1]=id_dmg[dmg_ids[t1]];
		ddi_xs[t1+2]=2+(14-str_len(dmg_strs[t1+2]));
		if(t1>10) ddi_xs[t1+2]+=29;
		}
	ddi.curr_element=2;
	if(do_ddialog()) {
		pop_context();
		return;
		}
	for(t1=0;t1<22;t1++)
		id_dmg[dmg_ids[t1]]=tmp[t1];
		pop_context();
}