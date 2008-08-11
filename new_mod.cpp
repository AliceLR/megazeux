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

//TO DO:
// Sample playing (including cache'ing and freeing up mem if needed)
// Internal .MOD conversion

#include "ems.h"
#include "meminter.h"
#include "mod.h"
#include "bwsb.h"
#include "data.h"
#include "timer.h"
#include <fcntl.h>
#include <io.h>
#include <dos.h>
#include "string.h"
#include "error.h"
#include "boardmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys\stat.h>
extern char no_ems;
extern int rob_global_ems;

//Error leniency, 0 is least lenient, 2 is most lenient.
//
//Errors in most lenient: (normal game)
//   File "MZXBLANK.FIL" not found; Music code compromised
//   Low on conventional memory; Music code compromised
//
//Additional errors in middle leniency: (game w/debug menu)
//   Error opening module file
//   Error loading module
//   Error loading SAM
//   Out of memory for SAM
//
//Additional errors in least lenient: (editor)
//   Music is off- Module is set for board but not loaded
//   Music is off- SAM cannot play
unsigned char error_mode=2;

GDMHeader ModHeader;            /* Module header */
int ErrorFlag;
int file, MusChans, t1;
unsigned int Addr=0xFFFF,IRQ=0xFF,DMA=0xFF;

char sfx_chan[4]={ -1,-1,-1,-1 };//Actual IDs of SFX channels (left)
char sfx_chan2[4]={ -1,-1,-1,-1 };//Actual IDs of SFX channels (right)
char actual_num_sfx=0;//Actual number of different SFX channels
char current_sfx=0;//Current sfx channel

char mod_active=0;//1=blank mod 2=real mod

//Data for data integrity error checks
int SCDoing=0;
char far *SCLoadingName;

#define FT_GDM		1
#define FT_4MOD	2
#define FT_8MOD	3
#define FT_XMOD	4
//(unkown)
#define FT_NONE	0

int _file_type(int fp,char *fn) {
	char id[5];
	int tmp;
	//Tells us what kind of Music file it is! (assumes at start of file)
	read(fp,&id,4);
	id[4]=0;
	if(!str_cmp(id,"GDMþ")) return FT_GDM;
	lseek(fp,1080,SEEK_SET);
	read(fp,&id,4);
	id[4]=0;
	if(!str_cmp(id,"M.K.")) {
		//WOW??
		tmp=str_len(fn);
		if(!str_cmp(&fn[tmp-3],"WOW")) return FT_8MOD;
		else return FT_4MOD;
		}
	if((!str_cmp(id,"M!K!"))||(!str_cmp(id,"FLT4"))||
		(!str_cmp(id,"4CHN"))) return FT_4MOD;
	if((!str_cmp(id,"8CHN"))||(!str_cmp(id,"FLT8"))||
		(!str_cmp(id,"OCTA"))) return FT_8MOD;
	if((id[1]=='C')&&(id[2]=='H')&&(id[3]=='N')) return FT_XMOD;
	if((id[2]=='C')&&(id[3]=='H')) return FT_XMOD;
	return FT_NONE;
}

int ConvertMOD(int file,int type);

void load_mod(char far *filename) {
	int t1,t2;
SCDoing|=512;
SCLoadingName=filename;
	if(!music_device) music_on=0;

	free_sam_cache(1);//Clear entire sfx cache
	save_map_state_EMS(rob_global_ems);

	if(mod_active>0) {
		StopMusic();
		StopOutput();
		UnloadModule();
		}
	mod_active=0;

	mod_playing[0]=0;
	if(!music_on) {
		//Save mod playing...
		if(filename!=NULL) {
			str_cpy(mod_playing,filename);
			if(!error_mode) error("Music is off- Module is set for board but not loaded",
				0,24,current_pg_seg,0x3201);
			}
		restore_map_state_EMS(rob_global_ems);
SCDoing&=~512;
		return;
		}
	if(filename==NULL) goto load_empty_mod;

retry_1:
	ErrorFlag=EmsExist()&&1;/* Enable EMS if available */
	//Turn off EMS if requested in CMD line-
	if(no_ems) ErrorFlag=0;
	file=open(filename,O_RDONLY|O_BINARY);
	if(file>-1) {
		//Check file type
		t1=_file_type(file,filename);
		if(t1==FT_NONE) {
			ErrorFlag=1;
			close(file);
			goto tsukino;
			}
		if(t1==FT_GDM) {
			LoadGDM(file,0,&ErrorFlag,&ModHeader);
			close(file);
			}
		else {
			//Mod file
			file=ConvertMOD(file,t1);
			LoadGDM(file,0,&ErrorFlag,&ModHeader);
			close(file);
			}
		}
	else {
		if(error_mode<2) error("Error opening module file",1,24,current_pg_seg,0x3701);
		goto load_empty_mod;
		}
	//Errors-
	if(ErrorFlag) {
		UnloadModule();
		//Try to free up memory
		restore_map_state_EMS(rob_global_ems);
		free_up_board_memory();
		save_map_state_EMS(rob_global_ems);
		file=open(filename,O_RDONLY|O_BINARY);
		if(file>-1) {
			//Check file type
			t1=_file_type(file,filename);
			if(t1==FT_NONE) {
				ErrorFlag=1;
				close(file);
				goto tsukino;
				}
			if(t1==FT_GDM) {
				LoadGDM(file,0,&ErrorFlag,&ModHeader);
				close(file);
				}
			else {
				//Mod file
				file=ConvertMOD(file,t1);
				LoadGDM(file,0,&ErrorFlag,&ModHeader);
				close(file);
				}
			}
		else {
		tsukino:
			if(error_mode<2) error("Error opening module file",1,24,current_pg_seg,0x3701);
			goto load_empty_mod;
			}
		if(ErrorFlag) {
			UnloadModule();
			if(error_mode<2)
				if(error("Error loading module",1,26,current_pg_seg,0x3300+ErrorFlag)==2)
					goto retry_1;
			goto load_empty_mod;
			}
		}
	//Count number of channels
	MusChans=current_sfx=actual_num_sfx=0;
	sfx_chan[0]=sfx_chan[1]=sfx_chan[2]=sfx_chan[3]=-1;
	sfx_chan2[0]=sfx_chan2[1]=sfx_chan2[2]=sfx_chan2[3]=-1;
	t2=0;//Start on left
	for(t1=0;t1<32;t1++) {
		if(ModHeader.PanMap[t1]!=0xFF) MusChans++;
		else if(actual_num_sfx<4) {
			ModHeader.PanMap[t1]=t2;
			if(t2==0) {//Left channel
				sfx_chan[actual_num_sfx]=t1;
				t2=0xF;
				}
			else {//Right channel
				sfx_chan2[actual_num_sfx++]=t1;
				t2=0;
				}
			}
		}
	if(actual_num_sfx>sfx_channels) actual_num_sfx=sfx_channels;
	MusChans+=actual_num_sfx<<1;
	if((actual_num_sfx==0)&&(sfx_channels>0)) {
		actual_num_sfx=1;
		sfx_chan[0]=30;
		sfx_chan2[0]=31;
		}
	//Activate up to 4 (8) SFX channels
	StartOutput(MusChans,0);
	StartMusic();
	str_cpy(mod_playing,filename);
	mod_active=2;
	restore_map_state_EMS(rob_global_ems);
SCDoing&=~512;
	return;

load_empty_mod:
retry_2:
	//Load "blank MOD"
	ErrorFlag=EmsExist()&&1;/* Enable EMS if available */
	//Turn off EMS if requested in CMD line-
	if(no_ems) ErrorFlag=0;
	file=open(mzx_blank_mod_file,O_RDONLY|O_BINARY);
	if(file>-1) {
		LoadGDM(file,0,&ErrorFlag,&ModHeader);
		close(file);
		}
	else {
		error("File \"MZXBLANK.FIL\" not found; Music code compromised",1,24,
			current_pg_seg,0x3601);
		goto load_no_mod;
		}
	//Errors-
	if(ErrorFlag) {
		UnloadModule();
		//Try to free up memory
		restore_map_state_EMS(rob_global_ems);
		free_up_board_memory();
		save_map_state_EMS(rob_global_ems);
		file=open(filename,O_RDONLY|O_BINARY);
		if(file>-1) {
			LoadGDM(file,0,&ErrorFlag,&ModHeader);
			close(file);
			}
		else {
			error("File \"MZXBLANK.FIL\" not found; Music code compromised",1,24,
				current_pg_seg,0x3601);
			goto load_no_mod;
			}
		if(ErrorFlag) {
			UnloadModule();
			if(error("Low on memory; Music code compromised",1,26,current_pg_seg,0x0600+ErrorFlag)==2)
				goto retry_2;
			goto load_no_mod;
			}
		}
	//Count number of channels
	MusChans=sfx_channels<<1;
	current_sfx=0;
	actual_num_sfx=sfx_channels;
	sfx_chan[0]=sfx_chan[1]=sfx_chan[2]=sfx_chan[3]=-1;
	sfx_chan2[0]=sfx_chan2[1]=sfx_chan2[2]=sfx_chan2[3]=-1;
	for(t1=0;t1<sfx_channels;t1++) {
		ModHeader.PanMap[(t1<<1)]=0;
		ModHeader.PanMap[(t1<<1)+1]=0xF;
		sfx_chan[t1]=(t1<<1);
		sfx_chan2[t1]=(t1<<1)+1;
		}
	//Activate up to 4 SFX channels
	StartOutput(MusChans,0);
	StartMusic();
	if(filename!=NULL) str_cpy(mod_playing,filename);
	mod_active=1;
	restore_map_state_EMS(rob_global_ems);
SCDoing&=~512;
	return;

load_no_mod:
	mod_active=actual_num_sfx=0;
	if(filename!=NULL) str_cpy(mod_playing,filename);
	restore_map_state_EMS(rob_global_ems);
SCDoing&=~512;
	return;
}

void end_mod(void) {
	load_mod(NULL);
}

//Sample allocation info-
//Number of samples minimum cacheable (must be min. 4)
#define NUM_SAM_CACHE	16
//For sample allocation into MOD memory
SamHeader SamHead[NUM_SAM_CACHE];
//Allocated pointers (NULL does NOT mean NOT LOADED. see SamPlayed)
char far *SamStorage[NUM_SAM_CACHE];
//Filenames
char SamNames[NUM_SAM_CACHE][13];
//Number of times played (0=NOT LOADED)
int SamPlayed[NUM_SAM_CACHE];
//SFX Channel currently playing on plus one (0=none) 1-4
char SamChannel[NUM_SAM_CACHE];
//Sam numbers stored in, in MOD memory- 250 minus cache index

//Internal func-initialize arrays
void _init_sam_cache(void) {
	int t1,t2;
	for(t1=0;t1<NUM_SAM_CACHE;t1++) {
		SamStorage[t1]=NULL;
		SamNames[t1][0]=0;
		SamPlayed[t1]=0;
		SamChannel[t1]=0;
		for(t2=0;t2<32;t2++) SamHead[t1].SamName[t2]=0;
		for(t2=0;t2<12;t2++) SamHead[t1].FileName[t2]=0;
		SamHead[t1].EmsHandle=0;
		SamHead[t1].LoopBegin=0;
		SamHead[t1].LoopEnd=0;
		SamHead[t1].Flags=0;
		SamHead[t1].C4Hertz=8363;
		SamHead[t1].Volume=64;
		SamHead[t1].Pan=0xFF;
		SamHead[t1].Length=0;
		SamHead[t1].Segment=0;
		}
}

//Internal- Deallocates one sample by cache index
void _free_sam(char sam_num) {
	if(SamPlayed[sam_num]==0) return;
SCDoing|=32;
	save_map_state_EMS(rob_global_ems);
	if(SamChannel[sam_num]) {
		//Stop playing
		ChannelPos(sfx_chan[SamChannel[sam_num]-1]+1,65534);
		ChannelPos(sfx_chan2[SamChannel[sam_num]-1]+1,65534);
		ChannelVol(sfx_chan[SamChannel[sam_num]-1]+1,0);
		ChannelVol(sfx_chan2[SamChannel[sam_num]-1]+1,0);
		}
	//Free actual sample from music code
	if(FreeSample(250-sam_num)&255) error("BAD !",2,4,current_pg_seg,1);
	//Free memory (auto freed by FreeSample)
	if(SamStorage[sam_num]!=NULL)	SamStorage[sam_num]=NULL;
	//Set other stuff
	SamNames[sam_num][0]=0;
	SamPlayed[sam_num]=0;
	SamChannel[sam_num]=0;
	SamHead[sam_num].Length=0;
	SamHead[sam_num].Segment=0;
	SamHead[sam_num].EmsHandle=0;
	restore_map_state_EMS(rob_global_ems);
SCDoing&=~32;
}

//This function will remove samples from the cache one at a time,
//starting with least-played and ending with currently-playing.
//Call it removes ONE sample unless CLEAR_ALL is set. Returns 1
//if nothing was found to deallocate (IE no further fixes possible)
char free_sam_cache(char clear_all) {
	//LeastPlayedNotPlaying, LeastPlayedCurrentlyPlaying
	int lpnp,lpcp;
	//Value of the above, IE the number of TIMES played
	int lpnpv,lpcpv;
	int t1;
SCDoing|=clear_all?64:128;
next:
	lpnp=lpcp=-1;
	lpnpv=lpcpv=32767;
	for(t1=0;t1<NUM_SAM_CACHE;t1++) {
		if(SamPlayed[t1]<1) continue;//Skip those not loaded
		if(SamChannel[t1]) {
			if(SamPlayed[t1]<lpcpv) {
				lpcpv=SamPlayed[t1];
				lpcp=t1;
				}
			}
		else {
			if(SamPlayed[t1]<lpnpv) {
				lpnpv=SamPlayed[t1];
				lpnp=t1;
				}
			}
		}
	//Any NOT playing?
	if(lpnp>=0) {
		//Yep, deallocate
		_free_sam(lpnp);
		if(clear_all) goto next;
SCDoing&=~(64|128);
		return 0;
		}
	//No? Any PLAYING!?
	if(lpcp>=0) {
		//Yep, deallocate
		_free_sam(lpcp);
		if(clear_all) goto next;
SCDoing&=~(64|128);
		return 0;
		}
	//Nope.
SCDoing&=~(64|128);
	return 1;
}

//This one will return the cache index for a given sample filename.
//If needed it will allocate it, freeing memory if necessary.
//Returns -1 if out of memory, -2 on error loading sample
//Increases SamPlayed.
char _load_sam(char far *file) {
	//First, see if already loaded
	int t1,t2;
	char oldc;
	FILE *fp;
	long siz;

SCLoadingName=file;
SCDoing|=256;
	for(t1=0;t1<NUM_SAM_CACHE;t1++) {
		if(!str_cmp(file,SamNames[t1]))
			if(SamPlayed[t1]) break;//Found
		}
	if(t1<NUM_SAM_CACHE) {
		if(SamPlayed[t1]<32700) SamPlayed[t1]++;//No overflow!
SCDoing&=~256;
		return t1;
		}
	//Oops, must load a new one
	//Find an empty slot
find_empty:
	for(t1=0;t1<NUM_SAM_CACHE;t1++)
		if(SamPlayed[t1]==0) break;
	if(t1>=NUM_SAM_CACHE) {
		//Create a new slot
		free_sam_cache(0);
		goto find_empty;
		}
	//Now open file and get filesize
	fp=fopen(file,"rb");
	if(fp==NULL) return -2;
	fseek(fp,0,SEEK_END);
	siz=ftell(fp);
	siz&=~1;//Make it even through truncation
	if(siz>65500) siz=65500;//Truncate filesize
	if(siz<4) {
		//Too short
		fclose(fp);
SCDoing&=~256;
		return -2;
		}
	//Now we work to allocate memory
	if(NULL==(SamStorage[t1]=(char far *)farmalloc(siz))) {
		//Try to free up memory
		free_up_board_memory();
		if(NULL==(SamStorage[t1]=(char far *)farmalloc(siz))) {
			//Free up other samples
			while(!free_sam_cache(0)) {
				SamStorage[t1]=(char far *)farmalloc(siz);
				if(SamStorage[t1]!=NULL) break;
				}
			//Did we not get it?
			if(SamStorage[t1]==NULL) {
				fclose(fp);
SCDoing&=~256;
				return -1;//Out of memory
				}
			}
		}
	//Allocated. Load sample in.
	fseek(fp,0,SEEK_SET);
	fread(SamStorage[t1],siz,1,fp);
	fclose(fp);
	mem_xor(SamStorage[t1],siz,128);
	//Save name and set played to 1
	//Before saving name, truncate filename if over 12 chars
	if(str_len(file)>12) {
		error("BAD !",2,4,current_pg_seg,3);
		oldc=file[12];
		file[12]=0;
		str_cpy(SamNames[t1],file);
		file[12]=oldc;
		}
	else str_cpy(SamNames[t1],file);
	SamPlayed[t1]=1;
	//Now allocate into actual MOD memory
	save_map_state_EMS(rob_global_ems);
	SamHead[t1].Length=siz;
	SamHead[t1].EmsHandle=0;
	SamHead[t1].Segment=FP_SEG(SamStorage[t1]);
	t2=AllocSample(250-t1,&SamHead[t1]);
	restore_map_state_EMS(rob_global_ems);
	//Free memory?
	if(t2) {
		farfree(SamStorage[t1]);
		SamStorage[t1]=NULL;
		}
	//Error?
	if(t2<2) {
SCDoing&=~256;
		return t1;
		}
	error("BAD !",2,4,current_pg_seg,2);
	SamPlayed[t1]=0;
	SamNames[t1][0]=0;
	SamHead[t1].Length=0;
	SamHead[t1].Segment=0;
	SamHead[t1].EmsHandle=0;
SCDoing&=~256;
	return -2;
}

//Quick Set SamChannel array to clear all instances
//of something being played on given channel (doesn't STOP playback)
void _clear_cache_chan(char channel) {
	int t1;
	for(t1=0;t1<NUM_SAM_CACHE;t1++)
		if(SamChannel[t1]==channel+1) SamChannel[t1]=0;

}

//Old frequencies - 214   = C-3 (c-5 now), INCREASE octave = half
//New frequencies - 16770 = C-3 (c-5 now), DECREASE octave = half

//Equation NEW=3588780/OLD
void play_sample(int freq,char far *file) {
	long conv;
	int sample;
	if(!mod_active) return;
	if(!music_device) return;
	if(music_device==6) return;
	if(!actual_num_sfx) return;
	if(!freq) return;
SCDoing|=1;
	conv=(3588780L/(long)freq);
	if(conv>65535U) conv=65535U;
	sample=_load_sam(file);
	if(sample<0) {
		//Error
		if(error_mode<2) {
			switch(sample) {
				case -1://Out of memory for SAM 0x2A01
					error("Out of memory for SAM",1,24,current_pg_seg,0x2A01);
					break;
				case -2://Error loading SAM 0x3501
					error("Error loading SAM",1,24,current_pg_seg,0x3501);
					break;
				}
			}
SCDoing&=~1;
		return;
		}
	_clear_cache_chan(current_sfx);
	SamChannel[sample]=current_sfx+1;
	save_map_state_EMS(rob_global_ems);
	PlaySample(sfx_chan[current_sfx]+1,250-sample,conv,sound_gvol<<3,0xFF);
	PlaySample(sfx_chan2[current_sfx++]+1,250-sample,conv,sound_gvol<<3,0xFF);
	restore_map_state_EMS(rob_global_ems);
	if(current_sfx>=actual_num_sfx) current_sfx=0;
SCDoing&=~1;
}

void end_sample(void) {
	int t1;
	if(!mod_active) return;
	if(!music_device) return;
	if(music_device==6) return;
	if(!actual_num_sfx) return;
SCDoing|=2;
	save_map_state_EMS(rob_global_ems);
	for(t1=0;t1<actual_num_sfx;t1++) {
		ChannelVol(sfx_chan[t1]+1,0);
		ChannelVol(sfx_chan2[t1]+1,0);
		}
	for(t1=0;t1<NUM_SAM_CACHE;t1++)
		SamChannel[t1]=0;
	restore_map_state_EMS(rob_global_ems);
SCDoing&=~2;
}

void jump_mod(int order) {
	if(!music_device) return;
	if(!mod_active) return;
SCDoing|=4;
	save_map_state_EMS(rob_global_ems);
	MusicOrder(order);
	restore_map_state_EMS(rob_global_ems);
SCDoing&=~4;
}

void volume_mod(int vol) {
	if(!music_device) return;
	if(!mod_active) return;
SCDoing|=8;
	save_map_state_EMS(rob_global_ems);
	MusicVolume((vol*music_gvol)>>5);
	restore_map_state_EMS(rob_global_ems);
SCDoing&=~8;
}

void mod_exit(void) {
	if(!music_device) return;
	free_sam_cache(1);
	if(mod_active>0) {
		StopMusic();
		StopOutput();
		UnloadModule();
		}
	mod_active=0;
	FreeMSE();
	music_device=0;
}

void music_off(void) {
	if(!music_device) return;
	free_sam_cache(1);
	save_map_state_EMS(rob_global_ems);
	if(mod_active>0) {
		StopMusic();
		StopOutput();
		UnloadModule();
		}
	mod_active=0;
	restore_map_state_EMS(rob_global_ems);
}

void mod_init(void) {
	if(!music_device) return;
	//Concatenate path of base MZX directory-
	str_cat(mzx_blank_mod_file,"MZXBLANK.FIL");
	str_cat(mzx_convert_mod_file,"MZX_CMOD.FIL");
	str_cat(MSE_file,music_MSEs[music_device]);
	ErrorFlag=LoadMSE(MSE_file,0,mixing_rate,4096,&Addr,&IRQ,&DMA);
	switch(ErrorFlag) {
		case 1:
			music_device=0;
			error("Sound card I/O address detection failure",2,20,
				current_pg_seg,0x3801);
		case 2:
			music_device=0;
			error("Sound card IRQ level detection failure",2,20,
				current_pg_seg,0x3901);
		case 3:
		case 4:
			music_device=0;
			error("Sound card DMA channel detection failure",2,20,
				current_pg_seg,0x3A01+ErrorFlag-3);
		case 10:
		case 11:
			music_device=0;
			error("Error loading MSE music driver",2,20,
				current_pg_seg,0x3C01+ErrorFlag-10);
		case 12:
			music_device=0;
			error("MVSOUND.SYS must be loaded for PAS support",2,20,
				current_pg_seg,0x3D01);
		default:
			music_device=0;
			error("Error initializing sound card/music code",2,20,
				current_pg_seg,0x3B01+ErrorFlag-6);
		case 0:
			break;
		}
	end_mod();
	_init_sam_cache();
}

//Old frequencies - 214   = C-3 (c-5 now), INCREASE octave = half
//New frequencies - 16770 = C-3 (c-5 now), DECREASE octave = half

//Equation NEW=3588780/OLD
void spot_sample(int freq,int sample) {
	long conv;
	if(!freq) return;
	if(!music_device) return;
	if(!actual_num_sfx) return;
	if(!mod_active) return;
SCDoing|=16;
	conv=(3588780L/(long)freq);
	if(conv>65535U) conv=65535U;
	_clear_cache_chan(current_sfx);
	save_map_state_EMS(rob_global_ems);
	PlaySample(sfx_chan[current_sfx]+1,sample,conv,sound_gvol<<3,0xFF);
	PlaySample(sfx_chan2[current_sfx++]+1,sample,conv,sound_gvol<<3,0xFF);
	restore_map_state_EMS(rob_global_ems);
	if(current_sfx>=actual_num_sfx) current_sfx=0;
SCDoing&=~16;
}

typedef unsigned char byte;
typedef unsigned int word;
typedef unsigned long dword;

//Converts a mod
int MODSamC4Hertz[16]={
	8363, 8424, 8485, 8547, 8608, 8671, 8734, 8797, 7894,
	7951, 8009, 8067, 8125, 8184, 8244, 8303 };
GDMHeader GDMHead={//Pre-init as much data as possible
	{ 'G','D','M','þ' },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	{ 13,10,26 },
	{ 'G','M','F','S' },
	1,
	0,
	115,
	2,
	5,
	{ 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255 },
	64,
	6,
	125,
	1,
	0,0,
	0,0,
	0,0,0,
	0,0,
	0,0,
	0,0 };
SamHeader2 SamHead2[31]={//Pre-init as much data as possible :)
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 },
	{ { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
	  { 0,0,0,0,0,0,0,0,0,0,0,0 },0,0,0,0,0,0,0,255 } };
byte far patdata[2624];//Allocate at runtime, later. (dest info for GDM)
byte far Music[2048];//Same. (orig pattern info)
//Periods for standard mod notes
int PT[60]={
	1712, 1616, 1525, 1440, 1357, 1281, 1209, 1141, 1077, 1017, 961, 907,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508, 480, 453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254, 240, 226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127, 120, 113,
	 107,  101,   95,   90,   85,   80,   76,   71,   67,   64,  60,  57 };
//GDM note value for each of these 60 "notes"
byte Note2GDM[60]={
	33,34,35,36,37,38,39,40,41,42,43,44,
	49,50,51,52,53,54,55,56,57,58,59,60,
	65,66,67,68,69,70,71,72,73,74,75,76,
	81,82,83,84,85,86,87,88,89,90,91,92,
	97,98,99,100,101,102,103,104,105,106,107,108 };

//Generic order for channel mapping
byte DefPan[32]={ 15,0,0,15,15,0,0,15,15,0,0,15,
	15,0,0,15,15,0,0,15,15,0,0,15,15,0,0,15,15,0,0,15 };

//Uses DOS file handle; returns handle to new file.
//Always closes old file. Returns -1 on error.
int ConvertMOD(int file,int type) {
	int tmp,tmp2,sam,NOP,patpos,pos,Note,LastFit;
	byte MaxChan,Patt,Row,Channel,FX1,FX1Data,NumChan,NumSam,
		Byte1,Byte2,Byte3,GDMNote,GDMIns,Chan,Ins,EvPos;
	byte Events[20];
	//For sample copy
	byte far *buff=NULL;
	word chunk=60000,chunk2;
	long len;
	char id[5];
	int destfile=open(mzx_convert_mod_file,O_CREAT|O_TRUNC|O_RDWR|O_BINARY,
		S_IREAD|S_IWRITE);
	if(destfile==-1) {
		close(file);
		return -1;
		}
	NumSam=30;
	switch(type) {
		case FT_4MOD:
			NumChan=3;
			break;
		case FT_8MOD:
			NumChan=7;
			break;
		default:
			//Determine number of channels
			lseek(file,1080,SEEK_SET);
			read(file,id,4);
			if(id[3]=='N') NumChan=id[0]-'1';//1 through 9 channels
			else NumChan=(id[1]-'1')+((id[0]-'0')*10);//10 and up channels
			break;
		}
	lseek(destfile,sizeof(GDMHeader),SEEK_SET);
	//Header conversion
	mem_cpy(GDMHead.PanMap,DefPan,32);
	GDMHead.NOS=NumSam;
	//Sample header conversion
	lseek(file,20,SEEK_SET);
	GDMHead.SamHeadOffset=tell(destfile);
	for(sam=0;sam<=NumSam;sam++) {
		//Convert sample header #sam
		tmp2=4;
		lseek(file,22,SEEK_CUR);
		read(file,Events,8);
		asm rol word ptr Events,8
		asm rol word ptr Events+4,8
		asm rol word ptr Events+6,8
		SamHead2[sam].Length=((unsigned int *)Events)[0]<<1;
		SamHead2[sam].C4Hertz=MODSamC4Hertz[Events[2]&0xF];
		if((SamHead2[sam].Volume=Events[3])>64) SamHead2[sam].Volume=64;
		SamHead2[sam].LoopBegin=((unsigned int *)Events)[2]<<1;
		SamHead2[sam].LoopEnd=SamHead2[sam].LoopBegin+
			(((unsigned int *)Events)[3]<<1)+1;
		if((SamHead2[sam].LoopEnd-SamHead2[sam].LoopBegin)>8) tmp2|=1;
		if(SamHead2[sam].LoopEnd>SamHead2[sam].Length)
			SamHead2[sam].LoopEnd=SamHead2[sam].Length+1;
		SamHead2[sam].Flags=tmp2;
		}
	for(tmp=NumSam;tmp>=0;tmp--)
		if((SamHead2[tmp].SamName[0])||(SamHead2[tmp].Length)) break;
	GDMHead.NOS=tmp;
	for(sam=0;sam<=GDMHead.NOS;sam++)
		write(destfile,&SamHead2[sam],sizeof(SamHeader2));
	//Order conversion
	read(file,patdata,130);
	GDMHead.NOO=patdata[0]-1;
	GDMHead.OrdOffset=tell(destfile);
	NOP=0;
	for(tmp=2;tmp<130;tmp++)
		if(patdata[tmp]>NOP) NOP=patdata[tmp];
	GDMHead.NOP=NOP;
	write(destfile,&patdata[2],GDMHead.NOO+1);
	//Pattern conversion
	lseek(file,154+(NumSam+1)*30,SEEK_SET);
	GDMHead.PatOffset=tell(destfile);
	MaxChan=0;
	for(Patt=0;Patt<=GDMHead.NOP;Patt++) {
		//Convert pattern #Patt
		patpos=2;
		pos=0;
		read(file,Music,(NumChan+1)<<8);
		for(Row=0;Row<64;Row++) {
			for(Channel=0;Channel<=NumChan;Channel++) {
				GDMNote=GDMIns=0;
				Byte1=Music[pos++];
				Byte2=Music[pos++];
				Byte3=Music[pos++];
				FX1Data=Music[pos++];
				Note=((Byte1&15)<<8)+Byte2;
				Ins=(Byte3>>4)+(Byte1&0xF0);
				if((Note)||(Ins)) {
					GDMIns=Ins;
					if(GDMIns>(GDMHead.NOS+1)) GDMIns=0;
					if(Note) {
						LastFit=32767;
						for(tmp=0;tmp<60;tmp++) {
							if(abs(PT[tmp]-Note)<LastFit) {
								GDMNote=Note2GDM[tmp];
								LastFit=abs(PT[tmp]-Note);
								}
							}
						}
					}
				FX1=Byte3&15;
				switch(FX1) {
					//Effects to leave as-is-
					//1 Portamento up
					//2 Portamento down
					//4 Vibrato
					//7 Tremolo
					//9 Sample offset
					//B Jump to order
					//D Pattern break
					case 0://Arpeggio or no-command
						if(FX1Data) FX1=0x10;
						break;
					case 3://Portamento to
						if(GDMNote) GDMNote=((GDMNote-1)|128)+1;
						break;
					case 5://Portamento to+Volume slide
						if(GDMNote) GDMNote=((GDMNote-1)|128)+1;
						if(FX1Data==0) FX1=3;
						break;
					case 6://Vibrato+Volume slide
						if(FX1Data==0) FX1=4;
						break;
					case 8://Pan
/*						if(FX1Data==0xA4) {
							FX1=0x1E;
							FX1Data=1;
							}
						else {
							if(FX1Data<0x80) {
								FX1Data>>=3;
								if(FX1Data>15) FX1Data=15;
								FX1=0x1E;
								FX1Data|=0x80;
								}
							else FX1=FX1Data=0;
							}*/
						FX1=FX1Data=0;
						break;
					case 0xA://Volume Slide
						if(FX1Data==0) FX1=0;
						break;
					case 0xC://Set Volume
						if(FX1Data>64) FX1Data=64;
						break;
					case 0xF://Set Tempo or BPM
						if(FX1Data>31) FX1=0x1F;
						else if(FX1Data==0) FX1=0;
						break;
					case 0xE://Extended effects
						switch(FX1Data>>4) {
							//Effects to leave as-is-
							//1 Fineslide Up
							//2 Fineslide Down
							//3 Glissando Control
							//4 Vibrato Waveform
							//5 Set C-4 finetune
							//6 Patttern Loop
							//7 Tremolo Waveform
							//A Fine Volume up
							//B Fine Volume down
							//C Note Cut
							//E Pattern Delay
							//F Invert Loop
							case 0://0 Set filter- remove
								FX1=0;
								FX1Data=0;
								break;
							case 8://Pan Position
								FX1=0x1E;
								break;
							case 9://Retrigger
								FX1=0x12;
								FX1Data&=0xF;
								if(FX1Data==0) FX1=0;
								break;
							case 0xD://Note Delay
								if(GDMNote) GDMNote=((GDMNote-1)|128)+1;
								else FX1=FX1Data=0;
								break;
							}
						break;
					}
				if((GDMNote)||(GDMIns)||(FX1)) {
					Chan=Channel;
					if(Channel>=MaxChan) MaxChan=Channel+1;
					EvPos=1;
					if((GDMNote)||(GDMIns)) {
						Chan|=32;
						Events[EvPos++]=GDMNote;
						Events[EvPos++]=GDMIns;
						}
					if(FX1) {
						Chan|=64;
						Events[EvPos++]=FX1;
						Events[EvPos++]=FX1Data;
						}
					Events[0]=Chan;
					mem_cpy(&patdata[patpos],Events,EvPos);
					patpos+=EvPos;
					}
				}//Next channel
			patdata[patpos++]=0;
			}//Next row
		patdata[0]=patpos&255;
		patdata[1]=patpos>>8;
		write(destfile,patdata,patpos);
		}//Next pattern
	for(tmp=MaxChan;tmp<32;tmp++)
		GDMHead.PanMap[tmp]=0xFF;
	//Sample conversion
	GDMHead.SamOffset=tell(destfile);
	//Allocate for sample copy
	buff=(byte far *)farmalloc(chunk);
	if(buff==NULL) {
		chunk=farcoreleft()-64;
		if(chunk>=2625) buff=(byte far *)farmalloc(chunk);
		if((buff==NULL)||(chunk<=2624)) {
			buff=patdata;
			chunk=2624;
			}
		}
	for(tmp=0;tmp<=GDMHead.NOS;tmp++) {
		//Convert sample #tmp
		if((len=SamHead2[tmp].Length)!=0) {
			//Copy buffer by buffer
			do {
				if(len<chunk) chunk2=len;
				else chunk2=chunk;
				read(file,buff,chunk2);
				mem_xor(buff,chunk2,128);
				write(destfile,buff,chunk2);
			} while((len-=chunk2)>0);
			}
		}//Next sample
	lseek(destfile,0,SEEK_SET);
	write(destfile,&GDMHead,sizeof(GDMHead));
	//Done!
	if(chunk>2624) farfree(buff);
	close(file);
	return destfile;
}

void fix_global_volumes(void) {
	//Call this when music_gvol and/or sound_gvol changes
	//This resets all the volumes properly
	MusicVolume((volume*music_gvol)>>5);
}