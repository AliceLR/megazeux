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

//Code to edit (via edit dialog) and test custom sound effects

#include "helpsys.h"
#include "string.h"
#include "sfx_edit.h"
#include "sfx.h"
#include "window.h"
#include "data.h"

//8 char names per sfx

char sfx_names[NUM_SFX*10]={
	"Gem      \0"
	"MagicGem \0"
	"Health   \0"
	"Ammo     \0"
	"Coin     \0"
	"Life     \0"
	"Lo Bomb  \0"
	"Hi Bomb  \0"
	"Key      \0"
	"FullKeys \0"
	"Unlock   \0"
	"Need Key \0"
	"InvsWall \0"
	"Forest   \0"
	"GateLock \0"
	"OpenGate \0"
	"Invinco1 \0"
	"Invinco2 \0"
	"Invinco3 \0"
	"DoorLock \0"
	"OpenDoor \0"
	"Ouch     \0"
	"Lava     \0"
	"Death    \0"
	"GameOver \0"
	"GateClse \0"
	"Push     \0"
	"Trnsport \0"
	"Shoot    \0"
	"Break    \0"
	"NeedAmmo \0"
	"Ricochet \0"
	"NeedBomb \0"
	"Place Lo \0"
	"Place Hi \0"
	"BombType \0"
	"Explsion \0"
	"Entrance \0"
	"Pouch    \0"
	"Potion   \0"
	"EmtyChst \0"
	"Chest    \0"
	"Time Out \0"
	"FireOuch \0"
	"StolenGm \0"
	"EnemyHit \0"
	"DrgnFire \0"
	"Scroll   \0"
	"Goop     \0"
	"Unused   " };

//--------------------------
//
// ( ) Default internal SFX
// ( ) Custom SFX
//
//    _OK_      _Cancel_
//
//--------------------------

char sfxdi_types[3]={ DE_RADIO,DE_BUTTON,DE_BUTTON };
char sfxdi_xs[3]={ 2,5,15 };
char sfxdi_ys[3]={ 2,5,5 };
char far *sfxdi_strs[3]={ "Default internal SFX\nCustom SFX",
"OK","Cancel" };
int sfxdi_p1s[3]={ 2,0,-1 };
int sfxdi_p2s[1]={ 20 };
void far *sfxdi_storage[1]={ &custom_sfx_on };

dialog sfxdi={
	26,7,53,14,"Choose SFX mode",3,
	sfxdi_types,
	sfxdi_xs,
	sfxdi_ys,
	sfxdi_strs,
	sfxdi_p1s,
	sfxdi_p2s,
	sfxdi_storage,0 };

//(full screen)
//
//|--
//|
//|Explsion_________//_____|
//|MagicGem_________//_____|
//|etc.    _________//_____|
//|
//|
//|
//| Next Prev Done
//|
//|--

//17 on pg 1/2, 16 on pg 3

char sedi_types[21]={ DE_BUTTON,DE_BUTTON,DE_BUTTON,DE_TEXT,DE_INPUT,DE_INPUT,
	DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,
	DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT,DE_INPUT };
char sedi_xs[21]={ 16,36,60,23,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
char sedi_ys[21]={ 22,22,22,20,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18 };
int sedi_p1s[21]={ 1,2,0,0,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,
	68 };
int sedi_p2s[21]={ 0,0,0,0,224,224,224,224,224,224,224,224,224,224,224,224,
	224,224,224,224,224 };
char far *sedi_strs[21]={ "Next","Previous","Done","Press Alt+T to test a sound effect" };
void far *sedi_storage[21];

dialog sedi={
	0,0,79,24,"Edit custom SFX",21,
	sedi_types,
	sedi_xs,
	sedi_ys,
	sedi_strs,
	sedi_p1s,
	sedi_p2s,
	sedi_storage,4 };

void sfx_edit(void) {
	//First, ask whether sfx should be INTERNAL or CUSTOM. If change is made
	//from CUSTOM to INTERNAL, verify deletion of CUSTOM sfx. If changed to
	//CUSTOM, copy over INTERNAL to CUSTOM. If changed to or left at CUSTOM,
	//go to sfx editing.
	int t1;
	int old_sfx_mode=custom_sfx_on;
	char pg=0;
	set_context(97);
	if(run_dialog(&sfxdi,current_pg_seg,1)) {
		pop_context();
		return;
		}
	//Custom to internal
	if((old_sfx_mode)&&(!custom_sfx_on)) {
		if(!confirm("Delete current custom SFX?")) {
			pop_context();
			return;
			}
		else {
			custom_sfx_on=1;
			pop_context();
			return;
			}
		}
	//Internal to custom
	if((!old_sfx_mode)&&(custom_sfx_on)) {
		for(t1=0;t1<NUM_SFX;t1++)
			str_cpy(&custom_sfx[t1*69],sfx_strs[t1]);
		}
	//Internal
	if(!custom_sfx_on) {
		pop_context();
		return;
		}
	//Edit custom
	do {
		//Setup page
		for(t1=0;t1<17;t1++) {
			sedi_strs[t1+4]=&sfx_names[(t1+pg*17)*10];
			sedi_storage[t1+4]=&custom_sfx[(t1+pg*17)*69];
			}
		//Setup number and current
		sedi.num_elements=(pg<2)?21:20;
		sedi.curr_element=4;
		//Run dialog
		t1=run_dialog(&sedi,current_pg_seg,0,1);
		//Next/prev page?
		if(t1==1) pg++;
		else if(t1==2) pg--;
		if(pg<0) pg=2;
		if(pg>2) pg=0;
	} while(t1>0);
	//Done!
	pop_context();
}