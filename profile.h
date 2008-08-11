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

#ifndef __PROFILE_H
#define __PROFILE_H

#ifdef PROFILE

#ifdef __cplusplus
extern "C" {
#endif

void PROFILING_ON(void);
void PROFILING_OFF(void);
char REGISTER_FUNC(char far *name);
void SELECT_FUNC(char which);
void POP_FUNC(void);
void PROFILING_SUMMARY(void);

#ifdef __cplusplus
}
#endif

#define profiling_on() PROFILING_ON()
#define profiling_off() PROFILING_OFF()
#define enter_func(a) \
	static int _xqz=0;\
	if(!_xqz) _xqz=REGISTER_FUNC(a);\
	SELECT_FUNC(_xqz)
#define enter_funcn(a,n) \
	static int _xqz##n=0;\
	if(!_xqz##n) _xqz##n=REGISTER_FUNC(a);\
	SELECT_FUNC(_xqz##n)
#define exit_func() POP_FUNC()
#define profiling_summary() PROFILING_SUMMARY()

#define MAX_REG 50
#define STACK_SIZE 50
extern char reg_funcs[MAX_REG][30];
extern char next_reg;
extern long func_clicks[MAX_REG];
extern long total_clicks;
extern char func_stack[STACK_SIZE];
extern char curr_stack_pos;

#else

#define profiling_on();
#define profiling_off();
#define enter_func(a);
#define enter_funcn(a,n);
#define exit_func();
#define profiling_summary();

#endif

#endif