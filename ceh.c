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
/*
	������������������������������������������������������������Ŀ
	�                   CRITICAL ERROR HANDLER                   �
	�                                                            �
	� The Critical Error Handler or CEH as it is also called is  �
	� a software routine that attempts to recover from a variety �
	� of internal and external errors that would prevent program �
	� continuance.                                               �
	�                                                            �
	� Consider this example.  A program wants to open a file and �
	� issues the DOS services interrupt (interrupt 21h) with the �
	� "open file" function code in AH (3Dh).  This file is found �
	� on a floppy disk in the A: drive.  However, the drive door �
	� handle has been left open.  What happens?                  �
	�                                                            �
	� Once interrupt 21h is issued, the DOS kernel takes over.   �
	� The kernel calls a device driver.  The device driver calls �
	� the BIOS.  The BIOS communicates with the disk controller  �
	� via the IN and OUT machine language instructions.  Because �
	� the controller cannot read the disk, it sends error infor- �
	� mation to the BIOS.  The BIOS sends error information to   �
	� the device driver.  The device driver sends error infor-   �
	� mation to the kernel.  At this point, the DOS designers    �
	� could have done one of two things.  The kernel could have  �
	� always sent the error information to the program causing   �
	� it to deal with the error (the FAIL option) or the kernel  �
	� could have made an attempt to solve the problem with user  �
	� help (the ABORT, RETRY, IGNORE, FAIL prompt).  This latter �
	� approach was chosen and is more flexible than the previous �
	� approach.  The user has the opportunity of closing the     �
	� door handle and selecting RETRY.  This time, the operation �
	� should succeed.                                            �
	�                                                            �
	� The ABORT option causes DOS to return to the COMMAND.COM   �
	� drive prompt.  This option could be disasterous if a file  �
	� has been opened by the program and information written to  �
	� the file but still in a DOS holding buffer.  When ABORT is �
	� chosen, the information in the buffer is not written to    �
	� this file and the file is not properly closed (with its    �
	� directory entry updated).                                  �
	�                                                            �
	� RETRY allows the DOS kernel to attempt the operation again �
	� but this option is generally only useful if the problem is �
	� due to leaving a drive handle open or the printer offline. �
	� Simply closing the handle or placing the printer online is �
	� all that is needed to get the program running again.       �
	�                                                            �
	� The IGNORE option is a form of gambling.  The DOS kernel   �
	� is told to ignore the error and let the program continue.  �
	� If the kernel does not feel that the error is that serious �
	� then it will allow the program to continue.                �
	�                                                            �
	� DOS 3.3 added the FAIL option (better than IGNORE).  FAIL  �
	� tells the kernel to allow the program to continue but pass �
	� back an error code via interrupt 21h to the program allow- �
	� ing it to make the decision as to continuing or not.       �
	�                                                            �
	� Upon entry to interrupt 24h (the critical error handler),  �
	� certain registers contain values indicating the nature of  �
	� the error and what course of action should be taken.  Reg- �
	� isters AH, AL, BP:SI, and DI contain this information.     �
	�                                                            �
	� If bit 7 of AH is 0 then the error was caused by a disk    �
	� operation and is known as a hard error.  The failing drive �
	� number is placed in AL (0 = A, 1 = B, 2 = C, etc).  If bit �
	� 5 is 1 then the IGNORE option is allowed.  If bit 4 is 1   �
	� then the RETRY option is allowed.  If bit 3 is 1 then the  �
	� FAIL option is allowed.  FAIL is not allowed on versions   �
	� prior to 3.3 and IGNORE may not be allowed on versions 3.3 �
	� and higher because FAIL is present.  RETRY is allowed in   �
	� versions 3.3 and higher.  Bits 2 and 1 contain a code that �
	� indicates the affected disk area.  A "0 0" indicates DOS,  �
	� a "0 1" indicates the file allocation table (FAT), a "1 0" �
	� indicates the directory, and a "1 1" indicates the data    �
	� area.  If bit 0 is 1 then the operation that was taking    �
	� place when the error occured was a write otherwise it was  �
	� a read.                                                    �
	�                                                            �
	� If bit 7 of AH is 1 then the error was caused by either a  �
	� character device such as the printer or a bad memory image �
	� of the file allocation table (FAT).  BP:SI points to the   �
	� device header of the failing device.  If the high bit of   �
	� the byte at BP:[SI+4] is 1 then the error is due to a bad  �
	� FAT image otherwise it is due to a character device.  AL   �
	� contains no useful information if bit 7 of AH is 1.        �
	�                                                            �
	� Regardless of what caused the error (disk or device or bad �
	� FAT image), the lower-half of DI contains an error code.   �
	�                                                            �
	� 00h - write-protect violation                              �
	� 01h - unknown drive number                                 �
	� 02h - drive not ready                                      �
	� 03h - unknown command (to controller)                      �
	� 04h - CRC data error                                       �
	� 05h - bad request structure length                         �
	� 06h - seek error                                           �
	� 07h - unknown media (disk) type                            �
	� 08h - sector not found                                     �
	� 09h - printer out of paper                                 �
	� 0ah - error while writing                                  �
	� 0bh - error while reading                                  �
	� 0ch - general failure                                      �
	�                                                            �
	� Upon exit, the CEH returns a code in AL to let the kernel  �
	� know what action to take.                                  �
	�                                                            �
	� 0 - IGNORE                                                 �
	� 1 - RETRY                                                  �
	� 2 - ABORT                                                  �
	� 3 - FAIL                                                   �
	�                                                            �
	� Since the CEH is called by the DOS kernel and DOS is not   �
	� reentrant, only the following interrupt 21h services can   �
	� be used from inside the handler.  The services are 01h to  �
	� 0ch inclusive (the console I/O functions) and 59h (get the �
	� extended error information).  Any other service will cause �
	� the computer to lock up.                                   �
	�                                                            �
	� The default critical error handler is actually located in  �
	� the resident portion of the COMMAND.COM TSR.  There are a  �
	� few problems with this handler.  First of all, if ABORT is �
	� chosen then the program is aborted as control returns to   �
	� the DOS prompt (and any data waiting to be written to disk �
	� never gets there).  Secondly, when the handler writes its  �
	� messages to the screen, these messages can overwrite what- �
	� ever is presently on the screen without first saving these �
	� contents.  The screen is not restored when the handler is  �
	� finished.  These problems are solved by CEH.C.  CEH.C con- �
	� tains source code for a handler that is more user-friendly �
	� that the default.  Two options are allowed: RETRY or ABORT �
	� (ABORT means FAIL in this case allowing a program to con-  �
	� tinue and decide if it should exit).  DOS 3.3 is required  �
	� to use this handler because of the FAIL option.            �
	�                                                            �
	� This handler was written in BORLAND C++ 2.0 using the C    �
	� compiler.  It has been tested successfully in all memory   �
	� models.  It will undoubtedly work on all BORLAND C compil- �
	� ers starting with Turbo C 1.5.  Some modifications will be �
	� necessary to port this code to other C compilers, notably  �
	� the console video functions.  Here is a quick rundown on   �
	� these functions.                                           �
	�                                                            �
	� window (int left, int top, int right, int bottom);         �
	�                                                            �
	� The console video functions work within a simple windowing �
	� environment.  The window () function defines the rectangle �
	� on the screen where console I/O is performed.  The screen  �
	� coordinates are relative to the upper-left corner of this  �
	� window and have origin (1, 1).  The default window is the  �
	� size of the screen (1, 1, 40, 25) in 40-column text modes  �
	� and (1, 1, 80, 25) in 80-column text modes.  The CEH pro-  �
	� gram assumes an 80-column mode but could be converted to   �
	� support either 80 or 40.                                   �
	�                                                            �
	� gotoxy (int x, int y);                                     �
	�                                                            �
	� This function positions the cursor within the active win-  �
	� dow to column x and row y.  It fails if the coordinates    �
	� are outside of the window range.                           �
	�                                                            �
	� wherex (void);                                             �
	�                                                            �
	� This function returns the x-coordinate of the cursor.      �
	�                                                            �
	� wherey (void);                                             �
	�                                                            �
	� This function returns the y-coordinate of the cursor.      �
	�                                                            �
	� cputs (const char *string);                                �
	�                                                            �
	� This function writes a string to the window beginning at   �
	� the current cursor location within the window.  Scrolling  �
	� (vertical) occurs if the string wraps past the lower-right �
	� boundary.  The current attribute defined by textattr () is �
	� used.                                                      �
	�                                                            �
	� putch (int character);                                     �
	�                                                            �
	� This function writes a character to the window at the cur- �
	� rent cursor location within the window.  The cursor loca-  �
	� tion is updated and the current attribute defined by text- �
	� attr () is used.                                           �
	�                                                            �
	� gettext (int left, int top, int right, int bottom, char *  �
	�          buffer);                                          �
	�                                                            �
	� This function uses screen coordinates, not window.  It is  �
	� not restricted to a window.  The screen contents bounded   �
	� by (left, top) and (right, bottom) are read into an array  �
	� pointed to by buffer.  Two bytes are read for each screen  �
	� position: one for the character and one for the attribute. �
	� The puttext () function is similar except that it writes   �
	� the buffer contents to the screen.                         �
	�                                                            �
	� textattr (int attribute);                                  �
	�                                                            �
	� Set current drawing attribute used by cputs ()/putch () to �
	� attribute.                                                 �
	�							     �
	� clrscr (void);                                             �
	�                                                            �
	� Clear the contents of the current window.  Blanks are used �
	� for clearing along with the attribute most recently speci- �
	� fied by textattr ().                                       �
	�                                                            �
	� gettextinfo (struct text_info *t);                         �
	�                                                            �
	� The current windowing state including window boundaries,   �
	� cursor location, and drawing attribute is read into the t  �
	� structure.  This allows the windowing state to be restored �
	� later.                                                     �
	�                                                            �
	� There are a few additional modifications to be made.       �
	�                                                            �
	� The peekb() macro takes two arguments: segment and offset. �
	� It returns the byte found at this location.                �
	�                                                            �
	� The setvect () function takes two arguments: an integer    �
	� identifying the interrupt to be taken over and a pointer   �
	� to the new interrupt function.                             �
	�                                                            �
	� The geninterrupt() macro and pseudo-registers (_AH, _ES,   �
	� etc.) can be replaced via an int86 () function.            �
	�                                                            �
	� The _osmajor and _osminor global variables contain the     �
	� major and minor portions of the DOS version number respec- �
	� tively.  A call to interrupt 21h with function 30h placed  �
	� in AH will return the major portion in AL and the minor in �
	� AH.                                                        �
	�                                                            �
	� Written by: Geoff Friesen, November 1991                   �
	��������������������������������������������������������������
*/

#include "ceh.h"
#include <dos.h>
#include "error.h"
#include "data.h"

#define	RETRY	1
#define	FAIL	3

static char *errmsgs [] =
{
	"Disk is write-protected.",
	"Unknown drive number.",
	"Drive not ready.",
	"Disk controller does not recognize command.",
	"Data integrity error.",
	"Poorly organized data passed to disk controller.",
	"Seek error.",
	"Disk type is unknown.",
	"Sector not found.",
	"Printer out of paper.",
	"Could not write.",
	"Could not read.",
	"General failure - disk might need formatting.",
	"Internal error."
};

#pragma warn -par
static void interrupt ceh (unsigned bp, unsigned di, unsigned si,
				unsigned ds, unsigned es, unsigned dx,
				unsigned cx, unsigned bx, unsigned ax)
{
	unsigned i;

	i=(di > 12) ? 13 : di;
	i=error2(errmsgs[i],3,7,current_pg_seg,0);
	ax=(i==1)?FAIL:RETRY;
}
#pragma warn +par

void installceh (void)
{
	if (_osmajor < 3 || (_osmajor == 3 && _osminor < 3))
		 return;
	setvect (0x24, ceh);
	return;
}