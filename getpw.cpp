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

#include <stdio.h>
#include <dir.h>
#include <string.h>
#include "meminter.h"

unsigned char far magic_code[16]="æRëòmMJ·‡²’ˆÞ‘$";
int pro_method,pw_size;
unsigned char password[16],xor;

unsigned char get_pw_xor_code(void) {
	int work=85;//Start with 85... (01010101)
	int t1;
	if(pro_method==0) return 0;
	//Clear pw after first null
	for(t1=strlen(password);t1<16;t1++)
		password[t1]=0;
	for(t1=0;t1<15;t1++) {
		//For each byte, roll once to the left and xor in pw byte if it
		//is an odd character, or add in pw byte if it is an even character.
		work<<=1;
		if(work>255) work^=257;//Wraparound from roll
		if(t1&1) {
			work+=password[t1];//Add (even byte)
			if(work>255) work^=257;//Wraparound from add
			}
		else work^=password[t1];//XOR (odd byte);
		}
	//To factor in protection method, add it in and roll one last time
	work+=pro_method;
	if(work>255) work^=257;
	work<<=1;
	if(work>255) work^=257;
	//Can't be 0-
	if(work==0) work=86;//(01010110)
	//Done!
	return (unsigned char)work;
}

void main(int argc,char *argv[]) {
	FILE *source;
	FILE *dest;
	int t1,t2;
	long len;
	printf("\n\n");
	if(argc<2) {
		printf("Usage:\n\nGETPW file.mzx [-d]\n\n-d will de-protect the file.\n\n\a");
		return;
		}
	source=fopen(argv[1],"rb");
	fseek(source,25,SEEK_CUR);
	pro_method=fgetc(source);//Protection method
	if(pro_method) {//Password
		fread(password,1,15,source);
		//First, normalize password...
		for(t1=0;t1<15;t1++) {
			password[t1]^=magic_code[t1];
			password[t1]-=0x12+pro_method;
			password[t1]^=0x8D;
			}
		//Show!
		printf("Password: %s\n",password);
		//Deprotect?
		if((argc>2)&&((argv[2][1]=='d')||(argv[2][1]=='D'))) {
			printf("Deprotecting... (to file NO_PW.MZX)\n");
			//Xor code
			xor=get_pw_xor_code();
			//De-xor entire file
			dest=fopen("NO_PW.MZX","wb");
			fseek(source,0,SEEK_END);
			len=ftell(source);
			fseek(source,0,SEEK_SET);
			for(t1=0;t1<25;t1++)
				fputc(fgetc(source),dest);
			fputc(0,dest);
			fseek(source,16,SEEK_CUR);
			for(t1=0;t1<3;t1++)
				fputc(t2=fgetc(source),dest);
			if(t2=='X') xor=0;//No xor for later files
			len-=44;
			for(;len>0;len--)
				fputc(fgetc(source)^xor,dest);
			fclose(dest);
			}
		}
	else printf("No password!\n\a");
	fclose(source);
}