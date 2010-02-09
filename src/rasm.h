/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __RASM_H
#define __RASM_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdio.h>

#define MAX_OBJ_SIZE       65536

#define IMM_U16            (1 << 0)
#define IMM_S16            (1 << 0)
#define CHARACTER          (1 << 2)
#define COLOR              (1 << 3)
#define DIRECTION          (1 << 4)
#define THING              (1 << 5)
#define PARAM              (1 << 6)
#define STRING             (1 << 7)
#define EQUALITY           (1 << 8)
#define CONDITION          (1 << 9)
#define ITEM               (1 << 10)
#define EXTRA              (1 << 11)
#define COMMAND            (1 << 12)
#define UNDEFINED          (1 << 13)

#define S_IMM_U16          0
#define S_IMM_S16          0
#define S_CHARACTER        2
#define S_COLOR            3
#define S_DIR              4
#define S_THING            5
#define S_PARAM            6
#define S_STRING           7
#define S_EQUALITY         8
#define S_CONDITION        9
#define S_ITEM             10
#define S_EXTRA            11
#define S_CMD              12
#define S_UNDEFINED        13

#define ERR_BADSTRING      1
#define ERR_BADCHARACTER   2
#define ERR_INVALID        3

#define CMD                (1 << 31)
#define CMD_NOT            CMD | 0
#define CMD_ANY            CMD | 1
#define CMD_PLAYER         CMD | 2
#define CMD_NS             CMD | 3
#define CMD_EW             CMD | 4
#define CMD_ATTACK         CMD | 5
#define CMD_ITEM           CMD | 6
#define CMD_SELF           CMD | 7
#define CMD_RANDOM         CMD | 8
#define CMD_STRING         CMD | 9
#define CMD_CHAR           CMD | 10
#define CMD_ALL            CMD | 11
#define CMD_EDIT           CMD | 12
#define CMD_PUSHABLE       CMD | 13
#define CMD_NONPUSHABLE    CMD | 14
#define CMD_LAVAWALKER     CMD | 15
#define CMD_NONLAVAWALKER  CMD | 16
#define CMD_ROW            CMD | 17
#define CMD_COUNTERS       CMD | 18
#define CMD_ID             CMD | 19
#define CMD_MOD            CMD | 20
#define CMD_ORDER          CMD | 21
#define CMD_THICK          CMD | 22
#define CMD_ARROW          CMD | 23
#define CMD_THIN           CMD | 24
#define CMD_MAXHEALTH      CMD | 25
#define CMD_POSITION       CMD | 26
#define CMD_MESG           CMD | 27
#define CMD_COLUMN         CMD | 28
#define CMD_COLOR          CMD | 29
#define CMD_SIZE           CMD | 30
#define CMD_BULLETN        CMD | 31
#define CMD_BULLETS        CMD | 32
#define CMD_BULLETE        CMD | 33
#define CMD_BULLETW        CMD | 34
#define CMD_BULLETCOLOR    CMD | 35
#define CMD_FIRST          CMD | 36
#define CMD_LAST           CMD | 37
#define CMD_FADE           CMD | 38
#define CMD_OUT            CMD | 39
#define CMD_IN             CMD | 40
#define CMD_BLOCK          CMD | 41
#define CMD_SFX            CMD | 42
#define CMD_INTENSITY      CMD | 43
#define CMD_SET            CMD | 44
#define CMD_PALETTE        CMD | 45
#define CMD_WORLD          CMD | 46
#define CMD_ALIGNEDROBOT   CMD | 47
#define CMD_GO             CMD | 48
#define CMD_SAVING         CMD | 49
#define CMD_SENSORONLY     CMD | 50
#define CMD_ON             CMD | 51
#define CMD_STATIC         CMD | 52
#define CMD_TRANSPARENT    CMD | 53
#define CMD_OVERLAY        CMD | 54
#define CMD_START          CMD | 55
#define CMD_LOOP           CMD | 56
#define CMD_EDGE           CMD | 57
#define CMD_SAM            CMD | 58
#define CMD_PLAY           CMD | 59
#define CMD_PERCENT        CMD | 60
#define CMD_HIGH           CMD | 61
#define CMD_MATCHES        CMD | 62
#define CMD_NONE           CMD | 63
#define CMD_INPUT          CMD | 64
#define CMD_DIR            CMD | 65
#define CMD_COUNTER        CMD | 66
#define CMD_DUPLICATE      CMD | 67
#define CMD_NO             CMD | 68

#define IGNORE_TYPE             (1 << 30)
#define IGNORE_TYPE_COMMA       IGNORE_TYPE | 0
#define IGNORE_TYPE_SEMICOLON   IGNORE_TYPE | 1
#define IGNORE_TYPE_A           IGNORE_TYPE | 2
#define IGNORE_TYPE_AN          IGNORE_TYPE | 3
#define IGNORE_TYPE_AND         IGNORE_TYPE | 4
#define IGNORE_TYPE_AS          IGNORE_TYPE | 5
#define IGNORE_TYPE_AT          IGNORE_TYPE | 6
#define IGNORE_TYPE_BY          IGNORE_TYPE | 7
#define IGNORE_TYPE_ELSE        IGNORE_TYPE | 8
#define IGNORE_TYPE_FOR         IGNORE_TYPE | 9
#define IGNORE_TYPE_FROM        IGNORE_TYPE | 10
#define IGNORE_TYPE_INTO        IGNORE_TYPE | 11
#define IGNORE_TYPE_IS          IGNORE_TYPE | 12
#define IGNORE_TYPE_OF          IGNORE_TYPE | 13
#define IGNORE_TYPE_THE         IGNORE_TYPE | 14
#define IGNORE_TYPE_THEN        IGNORE_TYPE | 15
#define IGNORE_TYPE_THERE       IGNORE_TYPE | 16
#define IGNORE_TYPE_THROUGH     IGNORE_TYPE | 17
#define IGNORE_TYPE_THRU        IGNORE_TYPE | 18
#define IGNORE_TYPE_TO          IGNORE_TYPE | 19
#define IGNORE_TYPE_WITH        IGNORE_TYPE | 20

typedef struct
{
  char *name;
  int parameters;
  int *param_types;
} mzx_command;

typedef struct
{
  const char *name;
  int count;
  int offsets[19];
} search_entry;

typedef struct
{
  const char *name;
  int offset;
  int type;
} search_entry_short;

int is_dir(char *cmd_line, char **next);
int is_condition(char *cmd_line, char **next);
int is_equality(char *cmd_line, char **next);
int is_item(char *cmd_line, char **next);
int is_command_fragment(char *cmd_line, char **next);
int is_extra(char *cmd_line, char **next);

int assemble_text(char *input_name, char *output_name);
void print_command(mzx_command *cmd);
char *assemble_file(char *name, int *size);
void disassemble_file(char *name, char *program, int program_length,
 int allow_ignores, int base);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC int assemble_line(char *cpos, char *output_buffer, char *error_buffer,
 char *param_listing, int *arg_count_ext);
CORE_LIBSPEC int disassemble_line(char *cpos, char **next, char *output_buffer,
 char *error_buffer, int *total_bytes, int print_ignores, char *arg_types,
 int *arg_count, int base);
CORE_LIBSPEC const search_entry_short *find_argument(char *name);
CORE_LIBSPEC void print_color(int color, char *color_buffer);
CORE_LIBSPEC int unescape_char(char *dest, char c);
CORE_LIBSPEC int get_color(char *cmd_line);
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __RASM_H
