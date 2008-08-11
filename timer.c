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

/* TIMER.C- C code to fix timer for 500 ticks per second and run a
				delay function. This is C and not C++ so the interrupt
				functions prototype correctly. I tried to convert it to C++
				but C++ wants interrupt functions to be (...) and not
				(bp,di,etc) so it was too akward to be worth it. */

#include "profile.h"
#include "string.h"
#include "sfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <time.h>
#include "timer.h"

#define TIMER_TICK	0x8

#define RegList1 unsigned bp, unsigned di, unsigned si, unsigned ds
#define RegList2 unsigned es, unsigned dx, unsigned cx, unsigned bx
#define RegList3 unsigned ax, unsigned ip, unsigned cs, unsigned flags
#define RegIntList RegList1,RegList2,RegList3

int in_timer;/* Prevent reentrancy */
char tick_count=0;/* Keeps track of clock ticks for INT 8 */
char tick_target=27;/* The target number for tick_count */
void interrupt (*oldint8)(void);/* Holds old INT 8 vector */
void interrupt timer_system(RegIntList);
char timer_installed=0;/* To remember if it is already installed */

volatile int tcycle=0;

/* Initializes the new vector. If called while already installed,
	simply realigns the timer speed. */
void install_timer(void) {
	if(!timer_installed) {
	  in_timer=0;
	  oldint8=getvect(TIMER_TICK); 	/* get the original vector */
	  }
	disable();                       /* Ints off to set timer   */
	outportb(0x043,0x034);           /* Counter 0, read lsb-msb */
												/* mode 2, rate generator, */
												/* divide by N (0x0952h)   */
	outportb(0x040,0x052);           /* Low  byte of 0x00952    */
	outportb(0x040,0x009);           /* High byte of 0x00952    */
												/* Clock tick is now 500   */
												/* ticks per second...     */
												/* (note- use 0x004A9 for  */
												/* 1000 ticks...)          */
	if(!timer_installed) {
		setvect(TIMER_TICK,timer_system);/* set up our ISR.       */
		timer_installed=1;
		}
	enable();                        /* ...and off we go...     */
}

/* Uninstall the timer routine. */
void uninstall_timer(void) {
	if(timer_installed) {
		timer_installed=0;
		disable();                      /* Interrupts off          */
		outportb(0x043,0x034);          /* Counter 0, read lsb-msb */
												  /* mode 2, rate generator, */
												  /* divide by N             */
		outportb(0x040,0x000);          /* Low  byte of 0x00000    */
		outportb(0x040,0x000);          /* High byte of 0x00000    */
												  /* Clock tick is now 18.2  */
												  /* ticks/sec (normal rate) */
		setvect(TIMER_TICK,oldint8);    /* Restore timer ISR       */
		enable();                       /* ...and off we go...     */
		}
}

#pragma warn -par  /* Turn off warnings that I know about here */
#pragma warn -aus
#pragma warn -use

/* Calls original ISR 27 times, then 28 times, etc. (fluctuates to get an
	average of 27.5 times which results in a 18.18 timer rate) */
void interrupt timer_system(RegIntList) {
  unsigned temp[3];             /* temporary storage area */
  outportb(0x020,0x020);   /* Send EOI to 8259 PIC  */
  if(!in_timer)	{		/* Prevent reentrancy */
		in_timer=1;		/* Ok, we're in, lock the door... */

		tcycle++;
		if(tcycle<0) tcycle=32767;//Prevent overflow to negative (lockups!)
#ifdef PROFILE
		if(curr_stack_pos>=0) {
			total_clicks++;
			func_clicks[func_stack[curr_stack_pos]]++;
			}
#endif
		/* Do mouse countdown, sound code, disk code, etc. here */
		sound_system();
		in_timer=0;
		}
	if((++tick_count)>=tick_target) {     /* Have we had 27 or 28 ticks? */
		tick_count=0;       /* Yes, reset count to 0 */
		tick_target^=7;     /* Target fluctuates between 27 and 28 */
								  /* chain to clock ISR */
		temp[0] = bp;      /* Save top 3 values */
		temp[1]=di;
		temp[2]=si;
		bp=ds;           /* Move all others up by 3 */
		di=es;
		si=dx;
		ds=cx;
		es=bx;
		dx=ax;
		cx=FP_OFF(oldint8);/* and put a new ip/cs/flags */
		bx=FP_SEG(oldint8);/* on the stack */
		ax=flags&~0x200;
		_BP-=6;           /* Modify BP by three words        */
		return;           /* NOTE: after changing _BP, don't */
		}						/* plan on using any local variables, */
								/* other than those here.           */
}

void far delay_time(int cycles) {
	tcycle=0;
	while(tcycle<cycles) ;
}

//Profiling code
#ifdef PROFILE

//Registered functions
#define MAX_REG 50
char reg_funcs[MAX_REG][30];
char next_reg=0;

//Time spent
long func_clicks[MAX_REG];
long total_clicks=0;

//Function stack
#define STACK_SIZE 50
char func_stack[STACK_SIZE];
char curr_stack_pos=-1;

void PROFILING_INIT(void) {
	int t1;
	next_reg=1;
	total_clicks=0;
	for(t1=0;t1<MAX_REG;t1++) {
		func_clicks[t1]=0;
		reg_funcs[t1][0]=0;
		}
	str_cpy(reg_funcs[0],"Other");
}
#pragma startup PROFILING_INIT

//Turn ON profiling
void PROFILING_ON(void) {
	int t1;
	for(t1=0;t1<STACK_SIZE;t1++)
		func_stack[t1]=0;
	curr_stack_pos=0;
}

//Turn OFF profiling
void PROFILING_OFF(void) {
	curr_stack_pos=-1;
}

//Register a function
char REGISTER_FUNC(char far *name) {
	if(next_reg>=MAX_REG) return 0;
	str_cpy(reg_funcs[next_reg],name);
	return next_reg++;
}

//Select a function
void SELECT_FUNC(char which) {
	if(curr_stack_pos>=STACK_SIZE) return;
	func_stack[++curr_stack_pos]=which;
}

//Stop a function
void POP_FUNC(void) {
	if(curr_stack_pos<=0) return;
	func_stack[curr_stack_pos--]=0;
}

//Summarize stuff
void PROFILING_SUMMARY(void) {
	int t1;
	curr_stack_pos=-1;
	for(t1=0;t1<18;t1++) {
		printf("Function %20s- %9.9ld ticks (%3.3d percent)\n",reg_funcs[t1],
			func_clicks[t1],func_clicks[t1]*100L/total_clicks);
		}
	//More?
	if(next_reg>18) {
		printf("[ Press any key for more... ]");
		if(!getch()) getch();
		printf("\n");
		for(t1=18;t1<36;t1++) {
			printf("Function %20s- %9.9ld ticks (%3.3d percent)\n",reg_funcs[t1],
				func_clicks[t1],func_clicks[t1]*100L/total_clicks);
			}
		//More?
		if(next_reg>36) {
			printf("[ Press any key for more... ]");
			if(!getch()) getch();
			printf("\n");
			for(t1=36;t1<MAX_REG;t1++) {
				printf("Function %20s- %9.9ld ticks (%3.3d percent)\n",reg_funcs[t1],
					func_clicks[t1],func_clicks[t1]*100L/total_clicks);
				}
			}
		}
	printf("\nTotal ticks- %9.9ld\n",total_clicks);
	printf("Save profiling report to PROFILE.TXT? ");
	t1=getch();
	if(!t1) t1=-getch();
	if((t1=='y')||(t1=='Y')) {
		FILE *fp=fopen("profile.txt","wt");
		for(t1=0;t1<MAX_REG;t1++) {
			fprintf(fp,"Function %20s- %9.9ld ticks (%3.3d percent)\n",reg_funcs[t1],
				func_clicks[t1],func_clicks[t1]*100L/total_clicks);
			}
		fprintf(fp,"\nTotal ticks- %9.9ld\n",total_clicks);
		fclose(fp);
		printf("\nReport written to PROFILE.TXT.");
		}
}

#endif