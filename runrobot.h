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

// Prototypes for RUNROBOT.CPP

#ifndef __RUNROBOT_H
#define __RUNROBOT_H

//Can use "ALL" or sensornames
void send_robot(char far *robot,char far *mesg,char ignore_lock=0);

//Returns non-0 if not possible
char send_robot_id(int id,char far *mesg,char ignore_lock=0);

//Prefixes a set of x/y pairs
void prefix_xy(int&fx,int&fy,int&mx,int&my,int&lx,int&ly,int robotx,
	int roboty);

//Move an x/y pair in a given direction. Returns non-0 if edge reached.
char move_dir(int&x,int&y,char dir);

//Returns the numeric value pointed to OR the numeric value represented
//by the counter string pointed to. (the ptr is at the param within the
//command)
int parse_param(unsigned char far *robot,int id);

extern unsigned char ibuff[161];//Use for most recent tr_msg
extern unsigned char ibuff2[161];//Alternate buffer
unsigned char far *tr_msg(unsigned char far *orig_mesg,int id,
	unsigned char far *buffer=ibuff);

//Returns pos of next parameter, where pos is pos of current parameter
unsigned int next_param(unsigned char far *ptr,unsigned int pos);

char restore_label(unsigned char far *robot,unsigned char far *label);
char zap_label(unsigned char far *robot,unsigned char far *label);

//Turns a color (including those w/??) to a real color (0-255)
unsigned char fix_color(int color,unsigned char def);

extern char first_prefix;//1-3 normal 5-7 is 1-3 but from a REL FIRST cmd
extern char mid_prefix;
extern char last_prefix;//See first_prefix
extern char far *item_to_counter[9];

//Robot box message stuff
void display_robot_line(unsigned char far *robot,int y,int id);
void robot_frame(unsigned char far *robot,int id);
void robot_box_display(unsigned char far *robot,int id,char far *label_storage);

#ifdef __cplusplus
extern "C" {
#endif

void send_robot_def(int robot_id,char mesg_id);

//x/y to adjust struct's x/y incase the robot was moved by an outside force
//such as pushing or a MOVE ALL command.
void run_robot(int id,int x,int y);

//Sends robot in struct of sensor #id to SENSORPUSHED
void push_sensor(int id);

//Sends robot in struct of sensor #id to SENSORON
void step_sensor(int id);

#ifdef __cplusplus
}
#endif

#endif