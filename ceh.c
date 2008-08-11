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
	ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
	³                   CRITICAL ERROR HANDLER                   ³
	³                                                            ³
	³ The Critical Error Handler or CEH as it is also called is  ³
	³ a software routine that attempts to recover from a variety ³
	³ of internal and external errors that would prevent program ³
	³ continuance.                                               ³
	³                                                            ³
	³ Consider this example.  A program wants to open a file and ³
	³ issues the DOS services interrupt (interrupt 21h) with the ³
	³ "open file" function code in AH (3Dh).  This file is found ³
	³ on a floppy disk in the A: drive.  However, the drive door ³
	³ handle has been left open.  What happens?                  ³
	³                                                            ³
	³ Once interrupt 21h is issued, the DOS kernel takes over.   ³
	³ The kernel calls a device driver.  The device driver calls ³
	³ the BIOS.  The BIOS communicates with the disk controller  ³
	³ via the IN and OUT machine language instructions.  Because ³
	³ the controller cannot read the disk, it sends error infor- ³
	³ mation to the BIOS.  The BIOS sends error information to   ³
	³ the device driver.  The device driver sends error infor-   ³
	³ mation to the kernel.  At this point, the DOS designers    ³
	³ could have done one of two things.  The kernel could have  ³
	³ always sent the error information to the program causing   ³
	³ it to deal with the error (the FAIL option) or the kernel  ³
	³ could have made an attempt to solve the problem with user  ³
	³ help (the ABORT, RETRY, IGNORE, FAIL prompt).  This latter ³
	³ approach was chosen and is more flexible than the previous ³
	³ approach.  The user has the opportunity of closing the     ³
	³ door handle and selecting RETRY.  This time, the operation ³
	³ should succeed.                                            ³
	³                                                            ³
	³ The ABORT option causes DOS to return to the COMMAND.COM   ³
	³ drive prompt.  This option could be disasterous if a file  ³
	³ has been opened by the program and information written to  ³
	³ the file but still in a DOS holding buffer.  When ABORT is ³
	³ chosen, the information in the buffer is not written to    ³
	³ this file and the file is not properly closed (with its    ³
	³ directory entry updated).                                  ³
	³                                                            ³
	³ RETRY allows the DOS kernel to attempt the operation again ³
	³ but this option is generally only useful if the problem is ³
	³ due to leaving a drive handle open or the printer offline. ³
	³ Simply closing the handle or placing the printer online is ³
	³ all that is needed to get the program running again.       ³
	³                                                            ³
	³ The IGNORE option is a form of gambling.  The DOS kernel   ³
	³ is told to ignore the error and let the program continue.  ³
	³ If the kernel does not feel that the error is that serious ³
	³ then it will allow the program to continue.                ³
	³                                                            ³
	³ DOS 3.3 added the FAIL option (better than IGNORE).  FAIL  ³
	³ tells the kernel to allow the program to continue but pass ³
	³ back an error code via interrupt 21h to the program allow- ³
	³ ing it to make the decision as to continuing or not.       ³
	³                                                            ³
	³ Upon entry to interrupt 24h (the critical error handler),  ³
	³ certain registers contain values indicating the nature of  ³
	³ the error and what course of action should be taken.  Reg- ³
	³ isters AH, AL, BP:SI, and DI contain this information.     ³
	³                                                            ³
	³ If bit 7 of AH is 0 then the error was caused by a disk    ³
	³ operation and is known as a hard error.  The failing drive ³
	³ number is placed in AL (0 = A, 1 = B, 2 = C, etc).  If bit ³
	³ 5 is 1 then the IGNORE option is allowed.  If bit 4 is 1   ³
	³ then the RETRY option is allowed.  If bit 3 is 1 then the  ³
	³ FAIL option is allowed.  FAIL is not allowed on versions   ³
	³ prior to 3.3 and IGNORE may not be allowed on versions 3.3 ³
	³ and higher because FAIL is present.  RETRY is allowed in   ³
	³ versions 3.3 and higher.  Bits 2 and 1 contain a code that ³
	³ indicates the affected disk area.  A "0 0" indicates DOS,  ³
	³ a "0 1" indicates the file allocation table (FAT), a "1 0" ³
	³ indicates the directory, and a "1 1" indicates the data    ³
	³ area.  If bit 0 is 1 then the operation that was taking    ³
	³ place when the error occured was a write otherwise it was  ³
	³ a read.                                                    ³
	³                                                            ³
	³ If bit 7 of AH is 1 then the error was caused by either a  ³
	³ character device such as the printer or a bad memory image ³
	³ of the file allocation table (FAT).  BP:SI points to the   ³
	³ device header of the failing device.  If the high bit of   ³
	³ the byte at BP:[SI+4] is 1 then the error is due to a bad  ³
	³ FAT image otherwise it is due to a character device.  AL   ³
	³ contains no useful information if bit 7 of AH is 1.        ³
	³                                                            ³
	³ Regardless of what caused the error (disk or device or bad ³
	³ FAT image), the lower-half of DI contains an error code.   ³
	³                                                            ³
	³ 00h - write-protect violation                              ³
	³ 01h - unknown drive number                                 ³
	³ 02h - drive not ready                                      ³
	³ 03h - unknown command (to controller)                      ³
	³ 04h - CRC data error                                       ³
	³ 05h - bad request structure length                         ³
	³ 06h - seek error                                           ³
	³ 07h - unknown media (disk) type                            ³
	³ 08h - sector not found                                     ³
	³ 09h - printer out of paper                                 ³
	³ 0ah - error while writing                                  ³
	³ 0bh - error while reading                                  ³
	³ 0ch - general failure                                      ³
	³                                                            ³
	³ Upon exit, the CEH returns a code in AL to let the kernel  ³
	³ know what action to take.                                  ³
	³                                                            ³
	³ 0 - IGNORE                                                 ³
	³ 1 - RETRY                                                  ³
	³ 2 - ABORT                                                  ³
	³ 3 - FAIL                                                   ³
	³                                                            ³
	³ Since the CEH is called by the DOS kernel and DOS is not   ³
	³ reentrant, only the following interrupt 21h services can   ³
	³ be used from inside the handler.  The services are 01h to  ³
	³ 0ch inclusive (the console I/O functions) and 59h (get the ³
	³ extended error information).  Any other service will cause ³
	³ the computer to lock up.                                   ³
	³                                                            ³
	³ The default critical error handler is actually located in  ³
	³ the resident portion of the COMMAND.COM TSR.  There are a  ³
	³ few problems with this handler.  First of all, if ABORT is ³
	³ chosen then the program is aborted as control returns to   ³
	³ the DOS prompt (and any data waiting to be written to disk ³
	³ never gets there).  Secondly, when the handler writes its  ³
	³ messages to the screen, these messages can overwrite what- ³
	³ ever is presently on the screen without first saving these ³
	³ contents.  The screen is not restored when the handler is  ³
	³ finished.  These problems are solved by CEH.C.  CEH.C con- ³
	³ tains source code for a handler that is more user-friendly ³
	³ that the default.  Two options are allowed: RETRY or ABORT ³
	³ (ABORT means FAIL in this case allowing a program to con-  ³
	³ tinue and decide if it should exit).  DOS 3.3 is required  ³
	³ to use this handler because of the FAIL option.            ³
	³                                                            ³
	³ This handler was written in BORLAND C++ 2.0 using the C    ³
	³ compiler.  It has been tested successfully in all memory   ³
	³ models.  It will undoubtedly work on all BORLAND C compil- ³
	³ ers starting with Turbo C 1.5.  Some modifications will be ³
	³ necessary to port this code to other C compilers, notably  ³
	³ the console video functions.  Here is a quick rundown on   ³
	³ these functions.                                           ³
	³                                                            ³
	³ window (int left, int top, int right, int bottom);         ³
	³                                                            ³
	³ The console video functions work within a simple windowing ³
	³ environment.  The window () function defines the rectangle ³
	³ on the screen where console I/O is performed.  The screen  ³
	³ coordinates are relative to the upper-left corner of this  ³
	³ window and have origin (1, 1).  The default window is the  ³
	³ size of the screen (1, 1, 40, 25) in 40-column text modes  ³
	³ and (1, 1, 80, 25) in 80-column text modes.  The CEH pro-  ³
	³ gram assumes an 80-column mode but could be converted to   ³
	³ support either 80 or 40.                                   ³
	³                                                            ³
	³ gotoxy (int x, int y);                                     ³
	³                                                            ³
	³ This function positions the cursor within the active win-  ³
	³ dow to column x and row y.  It fails if the coordinates    ³
	³ are outside of the window range.                           ³
	³                                                            ³
	³ wherex (void);                                             ³
	³                                                            ³
	³ This function returns the x-coordinate of the cursor.      ³
	³                                                            ³
	³ wherey (void);                                             ³
	³                                                            ³
	³ This function returns the y-coordinate of the cursor.      ³
	³                                                            ³
	³ cputs (const char *string);                                ³
	³                                                            ³
	³ This function writes a string to the window beginning at   ³
	³ the current cursor location within the window.  Scrolling  ³
	³ (vertical) occurs if the string wraps past the lower-right ³
	³ boundary.  The current attribute defined by textattr () is ³
	³ used.                                                      ³
	³                                                            ³
	³ putch (int character);                                     ³
	³                                                            ³
	³ This function writes a character to the window at the cur- ³
	³ rent cursor location within the window.  The cursor loca-  ³
	³ tion is updated and the current attribute defined by text- ³
	³ attr () is used.                                           ³
	³                                                            ³
	³ gettext (int left, int top, int right, int bottom, char *  ³
	³          buffer);                                          ³
	³                                                            ³
	³ This function uses screen coordinates, not window.  It is  ³
	³ not restricted to a window.  The screen contents bounded   ³
	³ by (left, top) and (right, bottom) are read into an array  ³
	³ pointed to by buffer.  Two bytes are read for each screen  ³
	³ position: one for the character and one for the attribute. ³
	³ The puttext () function is similar except that it writes   ³
	³ the buffer contents to the screen.                         ³
	³                                                            ³
	³ textattr (int attribute);                                  ³
	³                                                            ³
	³ Set current drawing attribute used by cputs ()/putch () to ³
	³ attribute.                                                 ³
	³							     ³
	³ clrscr (void);                                             ³
	³                                                            ³
	³ Clear the contents of the current window.  Blanks are used ³
	³ for clearing along with the attribute most recently speci- ³
	³ fied by textattr ().                                       ³
	³                                                            ³
	³ gettextinfo (struct text_info *t);                         ³
	³                                                            ³
	³ The current windowing state including window boundaries,   ³
	³ cursor location, and drawing attribute is read into the t  ³
	³ structure.  This allows the windowing state to be restored ³
	³ later.                                                     ³
	³                                                            ³
	³ There are a few additional modifications to be made.       ³
	³                                                            ³
	³ The peekb() macro takes two arguments: segment and offset. ³
	³ It returns the byte found at this location.                ³
	³                                                            ³
	³ The setvect () function takes two arguments: an integer    ³
	³ identifying the interrupt to be taken over and a pointer   ³
	³ to the new interrupt function.                             ³
	³                                                            ³
	³ The geninterrupt() macro and pseudo-registers (_AH, _ES,   ³
	³ etc.) can be replaced via an int86 () function.            ³
	³                                                            ³
	³ The _osmajor and _osminor global variables contain the     ³
	³ major and minor portions of the DOS version number respec- ³
	³ tively.  A call to interrupt 21h with function 30h placed  ³
	³ in AH will return the major portion in AL and the minor in ³
	³ AH.                                                        ³
	³                                                            ³
	³ Written by: Geoff Friesen, November 1991                   ³
	ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
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