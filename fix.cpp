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

#include <string.h>
#include <stdio.h>
#include <dir.h>

char magic_code[16]="æRëòmMJ·‡²’ˆÞ‘$";
long brd_pos[150];

void skip_layer(FILE *fp,unsigned char xor) {
	int xsiz,ysiz;
	int x=0,y=0;
	int runs;
	xsiz=fgetc(fp)^xor;
	xsiz+=(fgetc(fp)^xor)<<8;
	ysiz=fgetc(fp)^xor;
	ysiz+=(fgetc(fp)^xor)<<8;
	do {
		runs=fgetc(fp)^xor;
		if(runs<128) runs=1;
		else {
			runs^=128;
			fgetc(fp);
			}
		x+=runs;
	recap:
		if(x>=xsiz) {
			x-=xsiz;
			y++;
			goto recap;
			}
	} while(y<ysiz);
}

void main(void) {
	FILE *fp;
	ffblk ff;
	int t1;
	int t2,t3;
	char temp[5]="   \0";
	int pro_method;
	char password[15]="              ";
	int xor;
	unsigned int num_boards;

	printf("Fixing all version 2.00 .MZX and .MZB files...\n");

	//.MZX
	if(findfirst("*.MZX",&ff,0)) goto no_mzx;
	do {
		printf("\t%s... ",ff.ff_name);
		fp=fopen(ff.ff_name,"rb+");
		if(fp==NULL) {
			printf("Error opening file.\n");
			continue;
			}
		fseek(fp,25,SEEK_SET);
		pro_method=fgetc(fp);
		if(pro_method) fread(password,1,15,fp);
		fread(temp,1,3,fp);
		if(strcmp(temp,"MZ2")) {
			printf("Not a version 2.0? file.\n");
			fclose(fp);
			continue;
			}
		//xor code?
		if(pro_method) {
			for(t1=0;t1<15;t1++) {
				password[t1]^=magic_code[t1];
				password[t1]-=0x12+pro_method;
				password[t1]^=0x8D;
				}
			for(t1=strlen(password);t1<16;t1++)
				password[t1]=0;
			xor=85;
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
			}
		else xor=0;
		//Jump to # of boards
		fseek(fp,4234,SEEK_SET);
		if(pro_method) fseek(fp,15,SEEK_CUR);
		if(!(num_boards=xor^fgetc(fp))) {
			num_boards=fgetc(fp)^xor;
			num_boards+=(fgetc(fp)^xor)<<8;
			fseek(fp,num_boards,SEEK_CUR);
			num_boards=fgetc(fp)^xor;
			}
		//Skip board names
		fseek(fp,num_boards*25,SEEK_CUR);
		//Get board positions
		for(t1=0;t1<num_boards;t1++) {
			t3=1;
			for(t2=0;t2<4;t2++) {
				if(fgetc(fp)!=xor) t3=0;
				}
			brd_pos[t1]=0;
			if(t3) //Deleted board
				fseek(fp,4,SEEK_CUR);
			else {
				for(t2=0;t2<4;t2++)
					brd_pos[t1]+=((long)(fgetc(fp)^xor))<<(t2*8);
				}
			}
		//Do each board
		for(t1=0;t1<num_boards;t1++) {
			if(!brd_pos[t1]) continue;
			fseek(fp,brd_pos[t1],SEEK_SET);
			fgetc(fp);//Overall size
			//Overlay?
			if(!(fgetc(fp)^xor)) {
				//Yep.
				fgetc(fp);
				//Skip two layers
				skip_layer(fp,xor);
				skip_layer(fp,xor);
				}
			else fseek(fp,-1,SEEK_CUR);
			//Skip normal six layers
			skip_layer(fp,xor);
			skip_layer(fp,xor);
			skip_layer(fp,xor);
			skip_layer(fp,xor);
			skip_layer(fp,xor);
			skip_layer(fp,xor);
			//In regular info.
			fseek(fp,206,SEEK_CUR);
			//Write a 00 00 00 00 scroll, and a FF FF FF FF lock
			//then a 00 00 00 player lock
			for(t2=0;t2<4;t2++)
				fputc(xor,fp);
			for(t2=0;t2<4;t2++)
				fputc(255^xor,fp);
			for(t2=0;t2<3;t2++)
				fputc(xor,fp);
			//Next board!
			printf("*");
			}
		fclose(fp);
		//Next file!
		printf(" Done!\n");
	} while(!findnext(&ff));
no_mzx:
	//.MZB
	if(findfirst("*.MZB",&ff,0)) return;
	do {
		printf("\t%s... ",ff.ff_name);
		fp=fopen(ff.ff_name,"rb+");
		if(fp==NULL) {
			printf("Error opening file.\n");
			continue;
			}
		fread(temp,1,4,fp);
		if(strcmp(temp,"\xFFMB2")) {
			printf("Not a version 2.0? file.\n");
			fclose(fp);
			continue;
			}
		fgetc(fp);//Overall size
		//Overlay?
		if(!fgetc(fp)) {
			//Yep.
			fgetc(fp);
			//Skip two layers
			skip_layer(fp,0);
			skip_layer(fp,0);
			}
		else fseek(fp,-1,SEEK_CUR);
		//Skip normal six layers
		skip_layer(fp,0);
		skip_layer(fp,0);
		skip_layer(fp,0);
		skip_layer(fp,0);
		skip_layer(fp,0);
		skip_layer(fp,0);
		//In regular info.
		fseek(fp,206,SEEK_CUR);
		//Write a 00 00 00 00 scroll, and a FF FF FF FF lock
		//then a 00 00 00 player lock
		for(t2=0;t2<4;t2++)
			fputc(0,fp);
		for(t2=0;t2<4;t2++)
			fputc(255,fp);
		for(t2=0;t2<3;t2++)
			fputc(0,fp);
		fclose(fp);
		//Next file!
		printf("Done!\n");
	} while(!findnext(&ff));
	//Done!
	printf("\nDone with all files.\n\n");
}