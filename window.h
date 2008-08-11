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

/* WINDOW.H- Declarations for WINDOW.CPP */

#ifndef __WINDOW_H
#define __WINDOW_H

#include "world.h"

// Error message for being out of window memory for list_menu, etc functions
#define OUT_OF_WIN_MEM -32766

// For name seeking in list_menu
#define TIME_SUSPEND 300

// All screen-affecting code preserves the mouse cursor
int save_screen();
int restore_screen();
int draw_window_box(int x1, int y1, int x2, int y2, int color,
 int dark_color, int corner_color, int shadow, int fill_center);
int list_menu(char *choices, int choice_size, char *title,
  int current, int num_choices, int xpos);
int char_selection(int current);
int color_selection(int current, int allow_wild);
void draw_color_box(int color, int q_bit, int x, int y);

// Shell for list_menu() (returns -1 for ESC)
int add_board(World *mzx_world, int current);
int choose_board(World *mzx_world, int current, char *title, int board0_none);

// Shell for run_dialog() (returns 0 for ok, 1 for cancel, -1 for ESC)
char confirm(World *mzx_world, char *str);
char ask_yes_no(World *mzx_world, char *str);

// Shell for list_menu() (copies file chosen to ret and returns -1 for ESC)
// dirs_okay of 1 means drive and directory changing is allowed. Returns
// zero on proper selection.
int choose_file(char **wildcards, char *ret, char *title, int dirs_okay);

// Dialog box structure definition

struct dialog
{
  // Location. Always has a shadow and colors are preset.
  int x1, y1, x2, y2;
  char *title; // Not alloc'd- points to memory. (most titles are static)
  char num_elements;   //Number of elements
  char *element_types; // (not alloc'd) Points to array of types for
                       //the elements. Element types #define'd below
  char *element_xs;    // (not alloc'd) Points to array of x locations
                       //(as offsets from top left corner)
  char *element_ys;    //(not alloc'd) Points to array of y locations
                       //(as offsets from top left corner)
  char **element_strs; //(not alloc'd) Points to array of str pointers
                       //for element names/text.
  int *element_param1s;   //(not alloc'd) Points to array of parameters
  int *element_param2s;   //(not alloc'd) Points to array of parameters
  void **element_storage; //(not alloc'd) Points to array of void
                          //pointers, each pointing to where in memory
                          //the answer of each element is.
  char curr_element;      //The currently selected element
};

typedef struct dialog dialog;

// Element typ #define's (DE=Dialog Element)

#define DE_TEXT 0
#define DE_BOX 1
#define DE_LINE 2
#define DE_INPUT 3
#define DE_CHECK 4
#define DE_RADIO 5
#define DE_COLOR 6
#define DE_CHAR 7
#define DE_BUTTON 8
#define DE_NUMBER 9
#define DE_LIST 10
#define DE_BOARD 11
#define DE_FIVE 12

//DE_TEXT- String is the text. Storage- None. Uses color_string.
//DE_BOX-  Param 1 is width, param 2 is height. Storage- None.
//DE_LINE- Param 1 is length, param 2 is 0 for horiz, 1 for vert. Storage-
//         None.
//DE_INPUT- String is the question, param 1 is the max length, param 2 is
//          the type of input. Storage- char* for a string. Input
//          type byte- See INTAKE.H or INTAKE.CPP.
//DE_CHECK- String is a string containing the multiple lines of check box
//          identifiers, seperated by returns. Param 1 is the number of
//          check boxes, param 2 is the length of the longest identifier.
//          Storage- char* for an array of yes/no bytes.
//DE_RADIO- String is a string containing the multiple lines of radio options,
//          seperated by returns. Param 1 is the number of options, param 2
//          is the length of the longest option. Storage- char* for a
//          numerical answer. int* is also okay, it will just use the
//          lowest byte.
//DE_COLOR- String is the question, param 1 is 0 normally, 1 if ?? colors are
//          allowed. Storage- int* for a numerical answer.
//DE_CHAR- String is the question. Storage- unsigned char* for a byte
//         answer. If param 1 is 1 then char 255 is allowed, otherwise it
//         is not. Char 0 is NEVER allowed. Both are set to 1 if selected.
//DE_BUTTON- String is the button label, param 1 is the return code sent back
//           for pressing this button. Storage- None.
//DE_NUMBER- String is the question, param 1 is the lower limit, param 2 is
//           the upper limit. If the lower limit is 1 and the upper
//           limit is 9 or less, the number is shown as a highlighted number
//           on a line of dark numbers- 1234567. (4 being highlighted)
//           Storage- int* for a numerical answer. (signed numbers are
//           allowed) Behavior undefined if param 1 >= param 2.
//DE_LIST- Param 1 is the number of list choices, param 2 is the length in
//         memory of each choice (IE 25 is 24 chars plus a null, with junk
//         padding for those < 24 chars) String points to the sequential
//         list. The first element is the question to place ABOVE the list.
//         Storage- int* for a numerical answer. The x/y location is
//         the location of the QUESTION. The list answer is one below.
//         The list/elements are drawn w/color_string.
//DE_BOARD- String is the question. Storage- int* for a numerical answer.
//          See DE_LIST for details. Board 0 is shown as "(none)" IF AND
//          ONLY IF param 1 is non-zero.
//DE_FIVE- String is the question, param 1 is the lower limit/5, param 2 is
//         the upper limit/5. This is like DE_NUMBER but the number shown is
//         multiplied by 5 before showing. Storage- int* for the
//         answer/5.

// Dialog box color #define's-
#define DI_MAIN             31
#define DI_DARK             16
#define DI_CORNER           25
#define DI_TITLE            31
#define DI_LINE             16
#define DI_TEXT             27
#define DI_NONACTIVE        25
#define DI_ACTIVE           31
#define DI_INPUT            159
#define DI_CHAR             159
#define DI_NUMERIC          159
#define DI_LIST             159
#define DI_BUTTON           176
#define DI_ACTIVEBUTTON     252
#define DI_ARROWBUTTON      249
#define DI_METER            159
#define DI_PCARROW          30
#define DI_PCFILLER         25
#define DI_PCDOT            144

// Code to run a dialog box. Returns button code pressed, or -1 for ESC.
// If reset_curr=1, the current element will be set to the top. Otherwise,
// the current element from within the di structure will be used.
// If sfx_alt_t is 1, ALT+T will PLAY_SFX any DE_INPUT.
// If sfx_alt_t is >1, END jumps to location 0, and HOME to location
// SFX_ALT_T.
int run_dialog(World *mzx_world, dialog *di, char reset_curr = 1,
 char sfx_alt_t = 0);

// Characters for dialog box elements
extern char check_on[4];
extern char check_off[4];
extern char radio_on[4];
extern char radio_off[4];
extern unsigned char color_blank;
extern unsigned char color_wild;
extern unsigned char color_dot;
extern char list_button[4];
extern char num_buttons[7];

// Foreground colors that look nice for each background color
extern char fg_per_bk[16];

#endif
