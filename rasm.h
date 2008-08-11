#ifndef RASM_H
#define RASM_H

#include <stdio.h>

#define MAX_OBJ_SIZE       32768

#define IMM_U16            (1 << 0)
#define IMM_S16            (1 << 0)
#define CHARACTER          (1 << 2)
#define COLOR              (1 << 3)
#define DIR                (1 << 4)
#define THING              (1 << 5)
#define PARAM              (1 << 6)
#define STRING             (1 << 7)
#define EQUALITY           (1 << 8)
#define CONDITION          (1 << 9)
#define ITEM               (1 << 10)
#define EXTRA              (1 << 11)

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

typedef struct
{
  char *name;
  int parameters;
  int *param_types;
} mzx_command;

int is_color(char *cmd_line);
int is_param(char *cmd_line);
int is_dir(char *cmd_line, char **next);
int is_condition(char *cmd_line, char **next);
int is_equality(char *cmd_line, char **next);
int is_thing(char *cmd_line, char **next);
int is_item(char *cmd_line, char **next);
int is_command_fragment(char *cmd_line, char **next);
int is_extra(char *cmd_line, char **next);

int get_color(char *cmd_line);
int get_param(char *cmd_line);

int parse_argument(char *cmd_line, char **next, int *arg_translated, int *error);
void get_word(char *str, char *source, char t);
int match_command(mzx_command *cmd);
int assemble_text(char *input_name, char *output_name);
int assemble_command(int command_number, mzx_command *cmd, void *params[32], 
 char *obj_pos, char **next_obj_pos);
void print_command(mzx_command *cmd);
void skip_whitespace(char *cpos, char **next);
int get_line(char *buffer, FILE *fp);

#endif
