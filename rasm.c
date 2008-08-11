#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rasm.h"

int cm3[]   = { IGNORE_FOR, IMM_U16 | STRING };
int cm4[]   = { IMM_U16 | STRING };
int cm5[]   = { DIR, IGNORE_FOR, IMM_U16 | STRING };
int cm6[]   = { DIR };
int cm7[]   = { COLOR | STRING, THING, PARAM };
int cm8[]   = { CHARACTER | STRING | IMM_U16 };
int cm9[]   = { COLOR | STRING };
int cm10[]  = { IMM_S16 | STRING, IMM_S16 | STRING };
int cm11[]  = { STRING, IGNORE_TO, IMM_U16 | STRING };
int cm12[]  = { STRING, IGNORE_BY, IMM_U16 | STRING };
int cm13[]  = { STRING, IGNORE_BY, IMM_U16 | STRING };
int cm14[]  = { STRING, IGNORE_TO, STRING };
int cm15[]  = { STRING, IGNORE_BY, STRING };
int cm16[]  = { STRING, IGNORE_BY, STRING };
int cm17[]  = { STRING, EQUALITY, IMM_U16 | STRING, IGNORE_THEN, STRING };
int cm18[]  = { STRING, EQUALITY, STRING, IGNORE_THEN, STRING };
int cm19[]  = { CONDITION, IGNORE_THEN, STRING };
int cm20[]  = { CMD_NOT, CONDITION, IGNORE_THEN, STRING };
int cm21[]  = { CMD_ANY, COLOR | STRING, THING, PARAM, IGNORE_THEN, STRING };
int cm22[]  = { CMD_NOT, CMD_ANY, COLOR | STRING, THING, PARAM, IGNORE_THEN, STRING };
int cm23[]  = { COLOR | STRING, THING, PARAM, DIR, IGNORE_THEN, STRING };
int cm24[]  = { CMD_NOT, COLOR | STRING, THING, PARAM, DIR, IGNORE_THEN, STRING };
int cm25[]  = { COLOR | STRING, THING, PARAM, IGNORE_AT, IMM_S16 | STRING, 
 IMM_S16 | STRING, IGNORE_THEN, STRING };
int cm26[]  = { IMM_S16 | STRING, IGNORE_AT, IMM_S16 | STRING, IGNORE_THEN, STRING };
int cm27[]  = { DIR, CMD_PLAYER, COLOR | STRING, THING, PARAM, IGNORE_THEN, STRING };
int cm28[]  = { STRING };
int cm29[]  = { STRING };
int cm30[]  = { STRING };
int cm31[]  = { STRING, IGNORE_TO, STRING };
int cm32[]  = { IMM_U16 | STRING };
int cm33[]  = { COLOR | STRING, THING, PARAM, IGNORE_TO, DIR };
int cm34[]  = { IMM_U16 | STRING, ITEM };
int cm35[]  = { IMM_U16 | STRING, ITEM };
int cm36[]  = { IMM_U16 | STRING, ITEM, IGNORE_ELSE, STRING };
int cm39[]  = { STRING };
int cm40[]  = { IMM_U16 | STRING, STRING };
int cm41[]  = { IMM_U16 | STRING };
int cm42[]  = { CMD_MOD };
int cm43[]  = { CMD_SAM };
int cm44[]  = { STRING };
int cm45[]  = { CMD_PLAY };
int cm46[]  = { CMD_PLAY, STRING };
int cm47[]  = { CMD_PLAY };
int cm49[]  = { IMM_U16 | STRING };
int cm50[]  = { CMD_SFX, STRING };
int cm51[]  = { IGNORE_AT, DIR };
int cm54[]  = { IGNORE_AT, DIR, IGNORE_TO, STRING };
int cm55[]  = { STRING, IMM_U16 | STRING };
int cm56[]  = { STRING, IMM_U16 | STRING };
int cm59[]  = { CMD_NS };
int cm60[]  = { CMD_EW };
int cm61[]  = { CMD_ATTACK };
int cm62[]  = { CMD_PLAYER, IGNORE_TO, DIR };
int cm63[]  = { CMD_PLAYER, IGNORE_TO, DIR, IGNORE_ELSE, STRING };
int cm64[]  = { CMD_PLAYER, IGNORE_AT, IMM_S16 | STRING, IMM_S16 | STRING };
int cm65[]  = { CMD_PLAYER, DIR, STRING };
int cm66[]  = { CMD_PLAYER, CMD_NOT, DIR, STRING };
int cm67[]  = { CMD_PLAYER, IGNORE_AT, IMM_S16 | STRING, IMM_S16 | STRING, STRING };
int cm68[]  = { CMD_PLAYER, DIR };
int cm69[]  = { DIR, IGNORE_ELSE, STRING };
int cm72[]  = { STRING, IGNORE_WITH, STRING };
int cm73[]  = { IGNORE_TO, DIR };
int cm74[]  = { IGNORE_TO, DIR };
int cm75[]  = { CMD_HIGH, IGNORE_TO, DIR };
int cm76[]  = { IGNORE_TO, DIR };
int cm77[]  = { IGNORE_TO, DIR };
int cm78[]  = { IGNORE_TO, DIR };
int cm79[]  = { IGNORE_TO, DIR, IGNORE_FOR, IMM_U16 | STRING };
int cm80[]  = { COLOR | STRING, THING, PARAM, IGNORE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING };
int cm81[]  = { IGNORE_AS, IGNORE_AN, CMD_ITEM };
int cm82[]  = { IGNORE_AT, IMM_S16 | STRING, IMM_S16 | STRING, IGNORE_TO, STRING };
int cm83[]  = { STRING };
int cm84[]  = { IGNORE_AT, IMM_S16 | STRING, IMM_S16 | STRING };
int cm85[]  = { IGNORE_FROM, DIR };
int cm86[]  = { CMD_SELF, IGNORE_TO, DIR };
int cm87[]  = { CMD_SELF, IGNORE_AT, IMM_S16 | STRING, IMM_S16 | STRING };
int cm88[]  = { IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm89[]  = { IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm90[]  = { IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm91[]  = { IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm92[]  = { COLOR | STRING };
int cm93[]  = { COLOR | STRING, IGNORE_ELSE, STRING };
int cm94[]  = { COLOR | STRING };
int cm95[]  = { COLOR | STRING, IGNORE_ELSE, STRING };
int cm96[]  = { STRING, IGNORE_BY, CMD_RANDOM, IMM_U16 | STRING, IGNORE_TO,
 IMM_U16 | STRING };
int cm97[]  = { STRING, IGNORE_BY, CMD_RANDOM, IMM_U16 | STRING, IGNORE_TO,
 IMM_U16 | STRING };
int cm98[]  = { STRING, IGNORE_BY, CMD_RANDOM, IMM_U16 | STRING, IGNORE_TO,
 IMM_U16 | STRING };
int cm99[]  = { IMM_U16 | STRING, ITEM, IGNORE_FOR, IMM_U16 | STRING, ITEM,
 IGNORE_ELSE, STRING };
int cm100[]  = { DIR, CMD_PLAYER, STRING };
int cm101[] = { COLOR | STRING, THING, PARAM, DIR, CMD_PLAYER };
int cm102[] = { STRING };
int cm103[] = { STRING };
int cm104[] = { STRING };
int cm105[] = { STRING, STRING };
int cm106[] = { STRING, STRING, STRING };
int cm107[] = { STRING };
int cm108[] = { STRING };
int cm109[] = { STRING };
int cm110[] = { CMD_PLAYER, IGNORE_TO, STRING, IGNORE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING };
int cm111[] = { IGNORE_TO, DIR, IGNORE_FOR, IMM_U16 | STRING };
int cm112[] = { CMD_STRING, STRING };
int cm113[] = { CMD_STRING, IGNORE_IS, STRING, IGNORE_THEN, STRING };
int cm114[] = { CMD_STRING, IGNORE_IS, CMD_NOT, STRING, IGNORE_THEN, STRING };
int cm115[] = { CMD_STRING, CMD_MATCHES, STRING, IGNORE_THEN, STRING };
int cm116[] = { CMD_CHAR, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm117[] = { STRING };
int cm118[] = { STRING };
int cm119[] = { CMD_ALL, COLOR | STRING, THING, PARAM, IGNORE_TO, DIR };
int cm120[] = { IGNORE_AT, IMM_S16 | STRING, IMM_S16 | STRING, IGNORE_TO,
 IMM_S16 | STRING, IMM_S16 | STRING };
int cm121[] = { CMD_EDGE, CMD_COLOR, IGNORE_TO, COLOR };
int cm122[] = { IGNORE_TO, IGNORE_THE, DIR, IGNORE_IS, STRING };
int cm123[] = { IGNORE_TO, IGNORE_THE, DIR, CMD_NONE };
int cm124[] = { CMD_EDIT, CHARACTER | STRING | IMM_U16, IGNORE_TO, 
 IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING,
 IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING,
 IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING,
 IMM_U16 | STRING, IMM_U16 | STRING };
int cm125[] = { CMD_PUSHABLE };
int cm126[] = { CMD_NONPUSHABLE };
int cm127[] = { IMM_U16 | STRING };
int cm128[] = { IMM_U16 | STRING };
int cm129[] = { IMM_U16 | STRING };
int cm130[] = { IMM_U16 | STRING };
int cm131[] = { IMM_U16 | STRING };
int cm133[] = { IGNORE_FROM, DIR, IGNORE_TO, DIR };
int cm134[] = { IGNORE_A, CMD_LAVAWALKER };
int cm135[] = { IGNORE_A, CMD_NONLAVAWALKER };
int cm136[] = { IGNORE_FROM, COLOR | STRING, THING, PARAM, IGNORE_TO, 
 COLOR | STRING, THING, PARAM };
int cm137[] = { IGNORE_IS, COLOR };
int cm138[] = { IGNORE_IS, COLOR };
int cm139[] = { IGNORE_IS, COLOR };
int cm140[] = { CMD_ROW, IGNORE_IS, IMM_U16 | STRING };
int cm141[] = { IGNORE_TO, CMD_SELF };
int cm142[] = { IGNORE_TO, CMD_PLAYER };
int cm143[] = { IGNORE_TO, CMD_COUNTERS };
int cm144[] = { CMD_CHAR, CMD_ID, IMM_U16 | STRING,
 IGNORE_TO, CHARACTER | STRING | IMM_U16 };
int cm145[] = { IGNORE_TO, CMD_MOD, CMD_ORDER, IMM_U16 | STRING };
int cm146[] = { STRING };
int cm148[] = { CMD_THICK, CMD_ARROW, CMD_CHAR, DIR,
 IGNORE_TO, CHARACTER | STRING | IMM_U16 };
int cm149[] = { CMD_THIN, CMD_ARROW, CMD_CHAR, DIR,
 IGNORE_TO, CHARACTER | STRING | IMM_U16 };
int cm150[] = { CMD_MAXHEALTH, IMM_U16 | STRING };
int cm151[] = { CMD_PLAYER, CMD_POSITION };
int cm152[] = { CMD_PLAYER, CMD_POSITION };
int cm153[] = { CMD_PLAYER, CMD_POSITION };
int cm154[] = { CMD_MESG, CMD_COLUMN, IGNORE_TO, IMM_U16 | STRING };
int cm155[] = { CMD_MESG };
int cm156[] = { CMD_MESG };
int cm158[] = { IMM_U16 | STRING, IMM_U16 | STRING };
int cm159[] = { STRING };
int cm160[] = { CMD_COLOR, IGNORE_IS, COLOR | STRING };
int cm161[] = { CMD_COLOR, IGNORE_IS, COLOR | STRING };
int cm162[] = { CMD_COLOR, IGNORE_IS, COLOR | STRING };
int cm163[] = { CMD_COLOR, IGNORE_IS, COLOR | STRING };
int cm164[] = { CMD_COLOR, IGNORE_IS, COLOR | STRING };
int cm165[] = { IGNORE_IS, IGNORE_AT, IMM_U16 | STRING, IMM_U16 | STRING };
int cm166[] = { CMD_SIZE, IGNORE_IS, IMM_U16 | STRING, IGNORE_BY, IMM_U16 | STRING };
int cm167[] = { CMD_MESG, CMD_COLUMN, IGNORE_TO, STRING };
int cm168[] = { CMD_ROW, IGNORE_IS, STRING };
int cm169[] = { CMD_PLAYER, CMD_POSITION, IGNORE_TO, IMM_U16 | STRING };
int cm170[] = { CMD_PLAYER, CMD_POSITION, IGNORE_FROM, IMM_U16 | STRING };
int cm171[] = { CMD_PLAYER, CMD_POSITION, IGNORE_WITH, IMM_U16 | STRING };
int cm172[] = { CMD_PLAYER, CMD_POSITION, IGNORE_FROM, IMM_U16 | STRING,
 IGNORE_AND, CMD_DUPLICATE, CMD_SELF };
int cm173[] = { CMD_PLAYER, CMD_POSITION, IGNORE_WITH, IMM_U16 | STRING,
 IGNORE_AND, CMD_DUPLICATE, CMD_SELF };
int cm174[] = { CMD_BULLETN, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm175[] = { CMD_BULLETS, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm176[] = { CMD_BULLETE, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm177[] = { CMD_BULLETW, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm178[] = { CMD_BULLETN, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm179[] = { CMD_BULLETS, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm180[] = { CMD_BULLETE, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm181[] = { CMD_BULLETW, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm182[] = { CMD_BULLETN, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm183[] = { CMD_BULLETS, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm184[] = { CMD_BULLETE, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm185[] = { CMD_BULLETW, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm186[] = { CMD_BULLETCOLOR, IGNORE_IS, COLOR | STRING };
int cm187[] = { CMD_BULLETCOLOR, IGNORE_IS, COLOR | STRING };
int cm188[] = { CMD_BULLETCOLOR, IGNORE_IS, COLOR | STRING };
int cm194[] = { CMD_SELF, CMD_FIRST };
int cm195[] = { CMD_SELF, CMD_LAST };
int cm196[] = { CMD_PLAYER, CMD_FIRST };
int cm197[] = { CMD_PLAYER, CMD_LAST };
int cm198[] = { CMD_COUNTERS, CMD_FIRST };
int cm199[] = { CMD_COUNTERS, CMD_LAST };
int cm200[] = { CMD_FADE, CMD_OUT };
int cm201[] = { CMD_FADE, CMD_IN, STRING };
int cm202[] = { CMD_BLOCK, IGNORE_AT, IMM_S16 | STRING, IMM_S16 | STRING,
 IGNORE_FOR, IMM_U16 | STRING, IGNORE_BY, IMM_U16 | STRING,
 IGNORE_TO, IMM_S16 | STRING, IMM_S16 | STRING };
int cm203[] = { CMD_INPUT };
int cm204[] = { IGNORE_TO, CMD_DIR };
int cm205[] = { CMD_CHAR, CHARACTER | STRING | IMM_U16, DIR };
int cm206[] = { CMD_CHAR, CHARACTER | STRING | IMM_U16, DIR };
int cm207[] = { CMD_CHAR, CHARACTER | STRING | IMM_U16,
 IGNORE_TO, CHARACTER | STRING | IMM_U16 };
int cm211[] = { CMD_SFX, IMM_U16 | STRING, IGNORE_TO, STRING };
int cm212[] = { CMD_INTENSITY, IMM_U16 | STRING, IGNORE_IS, IGNORE_AT,
 CMD_PERCENT };
int cm213[] = { CMD_INTENSITY, IMM_U16 | STRING, IGNORE_IS, IGNORE_AT, 
 IMM_U16 | STRING, CMD_PERCENT };
int cm214[] = { CMD_FADE, CMD_OUT };
int cm215[] = { CMD_FADE, CMD_IN };
int cm216[] = { CMD_COLOR, IGNORE_TO, IMM_U16 | STRING, IMM_U16 | STRING,
 IMM_U16 | STRING, IMM_U16 | STRING };
int cm217[] = { CMD_CHAR, CMD_SET, STRING };
int cm218[] = { STRING, IGNORE_BY, IMM_U16 | STRING };
int cm219[] = { STRING, IGNORE_BY, IMM_U16 | STRING };
int cm220[] = { STRING, IGNORE_BY, IMM_U16 | STRING };
int cm221[] = { CMD_CHAR, DIR, IGNORE_IS, CHARACTER | STRING | IMM_U16 };
int cm223[] = { CMD_PALETTE, STRING };
int cm225[] = { CMD_FADE, IGNORE_TO, IMM_U16 | STRING, IGNORE_BY,
 IMM_U16 | STRING };
int cm226[] = { CMD_POSITION, IMM_S16 | STRING, IMM_S16 | STRING };
int cm227[] = { CMD_WORLD, STRING };
int cm228[] = { CMD_ALIGNEDROBOT, IGNORE_WITH, STRING, IGNORE_THEN, STRING };
int cm232[] = { CMD_FIRST, CMD_STRING, IGNORE_IS, STRING, IGNORE_THEN, STRING };
int cm233[] = { CMD_GO, STRING };
int cm234[] = { IGNORE_FOR, CMD_MOD, CMD_FADE };
int cm236[] = { CMD_SAVING };
int cm237[] = { CMD_SAVING };
int cm238[] = { CMD_SENSORONLY, CMD_SAVING };
int cm239[] = { CMD_COUNTER, IGNORE_IS, IMM_U16 | STRING, STRING };
int cm240[] = { IGNORE_IS, CMD_ON };
int cm241[] = { IGNORE_IS, CMD_STATIC };
int cm242[] = { IGNORE_IS, CMD_TRANSPARENT };
int cm243[] = { COLOR | STRING, CHARACTER | STRING | IMM_U16, CMD_OVERLAY,
 IGNORE_TO, IMM_S16 | STRING, IMM_S16 | STRING };
int cm244[] = { CMD_OVERLAY, CMD_BLOCK, IGNORE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING, IGNORE_FOR, IMM_U16 | STRING, IGNORE_BY, IMM_U16 | STRING,
 IGNORE_TO, IMM_S16 | STRING, IMM_S16 | STRING };
int cm246[] = { CMD_OVERLAY, COLOR | STRING, CHARACTER | STRING | IMM_U16, 
 IGNROE_TO, COLOR | STRING, CHARACTER | STRING };
int cm247[] = { CMD_OVERLAY, COLOR | STRING, IGNORE_TO, COLOR | STRING };
int cm248[] = { CMD_OVERLAY, COLOR | STRING, STRING, IGNORE_AT, 
 IMM_S16 | STRING, IMM_S16 | STRING };
int cm252[] = { CMD_START };
int cm253[] = { IGNORE_FOR, IMM_U16 | STRING };
int cm254[] = { CMD_LOOP };
int cm255[] = { CMD_MESG, CMD_EDGE };
int cm256[] = { CMD_MESG, CMD_EDGE };

mzx_command command_list[] =
{
  { "end",            0, NULL },
  { "die",            0, NULL },
  { "wait",           1, cm3 },
  { "cycle",          1, cm4 },
  { "go",             2, cm5 },
  { "walk",           1, cm6 },
  { "become",         3, cm7 },
  { "char",           1, cm8 },
  { "color",          1, cm9 },
  { "gotoxy",         2, cm10 },
  { "set",            2, cm11 },
  { "inc",            2, cm12 },  
  { "dec",            2, cm13 },
  { "set",            2, cm14 },
  { "inc",            2, cm15 },  
  { "dec",            2, cm16 },
  { "if",             4, cm17 },
  { "if",             4, cm18 },
  { "if",             2, cm19 },
  { "if",             3, cm20 },
  { "if",             5, cm21 },
  { "if",             6, cm22 },
  { "if",             5, cm23 },
  { "if",             6, cm24 },
  { "if",             6, cm25 },
  { "if",             3, cm26 },
  { "if",             6, cm27 },
  { "double",         1, cm28 },
  { "half",           1, cm29 },
  { "goto",           1, cm30 },
  { "send",           2, cm31 },
  { "explode",        1, cm32 },
  { "put",            4, cm33 },
  { "give",           2, cm34 },
  { "take",           2, cm35 },
  { "take",           3, cm36 },  
  { "endgame",        0, NULL },
  { "endlife",        0, NULL },
  { "mod",            1, cm39 },
  { "sam",            2, cm40 },
  { "volume",         1, cm41 },
  { "end",            1, cm42 },
  { "end",            1, cm43 },
  { "play",           1, cm44 },
  { "end",            1, cm45 },
  { "wait",           2, cm46 }, 
  { "wait",           1, cm47 },
  { "_blank_line",    0, NULL },
  { "sfx",            1, cm49 },
  { "play",           2, cm50 },
  { "open",           1, cm51 },
  { "lockself",       0, NULL },
  { "unlockself",     0, NULL },
  { "send",           2, cm54 },
  { "zap",            2, cm55 },
  { "restore",        2, cm56 },
  { "lockplayer",     0, NULL },
  { "unlockplayer",   0, NULL },
  { "lockplayer",     1, cm59 },  
  { "lockplayer",     1, cm60 },
  { "lockplayer",     1, cm61 },
  { "move",           2, cm62 },
  { "move",           3, cm63 },
  { "put",            2, cm64 },
  { "if",             3, cm65 },
  { "if",             4, cm66 },
  { "if",             4, cm67 },
  { "put",            2, cm68 },
  { "try",            2, cm69 },
  { "rotatecw",       0, NULL },
  { "rotateccw",      0, NULL },
  { "switch",         2, cm72 },
  { "shoot",          1, cm73 },
  { "laybomb",        1, cm74 },
  { "laybomb",        1, cm75 },
  { "shootmissile",   1, cm76 },
  { "shootseeker",    1, cm77 },
  { "spitfire",       1, cm78 },
  { "lazerwall",      2, cm79 },
  { "put",            5, cm80 },
  { "die",            1, cm81 },
  { "send",           3, cm82 },
  { "copyrobot",      1, cm83 },
  { "copyrobot",      2, cm84 },
  { "copyrobot",      1, cm85 },
  { "duplicate",      2, cm86 },
  { "duplicate",      3, cm87 },
  { "bulletn",        1, cm88 },
  { "bullets",        1, cm89 },
  { "bullete",        1, cm90 },
  { "bulletw",        1, cm91 },
  { "givekey",        1, cm92 },
  { "givekey",        2, cm93 },
  { "takekey",        1, cm94 },
  { "takekey",        2, cm95 },
  { "inc",            4, cm96 },
  { "dec",            4, cm97 },
  { "set",            4, cm98 },
  { "trade",          5, cm99 },
  { "send",           3, cm100 },
  { "put",            5, cm101 },
  { "//",             1, cm102 },
  { "*",              1, cm103 },
  { "[",              1, cm104 },
  { "?",              2, cm105 },
  { "?",              3, cm106 },
  { ":",              1, cm107 },
  { ".",              1, cm108 },
  { "|",              1, cm109 },
  { "teleport",       4, cm110 },
  { "scrollview",     2, cm111 },
  { "input",          2, cm112 },
  { "if",             3, cm113 },
  { "if",             4, cm114 },
  { "if",             4, cm115 },
  { "player",         2, cm116 },
  { "%",              1, cm117 },
  { "&",              1, cm118 },
  { "move",           5, cm119 },
  { "copy",           4, cm120 },
  { "set",            3, cm121 },
  { "board",          2, cm122 },
  { "board",          2, cm123 },
  { "char",           16, cm124 },
  { "become",         1, cm125 },
  { "become",         1, cm126 },
  { "blind",          1, cm127 },
  { "firewalker",     1, cm128 },
  { "freezetime",     1, cm129 },
  { "slowtime",       1, cm130 },
  { "wind",           1, cm131 },
  { "avalanche",      0, NULL },
  { "copy",           2, cm133 },
  { "become",         1, cm134 },
  { "become",         1, cm135 },
  { "change",         6, cm136 },
  { "playercolor",    1, cm137 },
  { "bulletcolor",    1, cm138 },
  { "missilecolor",   1, cm139 },
  { "message",        2, cm140 },
  { "rel",            1, cm141 },
  { "rel",            1, cm142 },
  { "rel",            1, cm143 },
  { "change",         4, cm144 },
  { "jump",           2, cm145 },
  { "ask",            1, cm146 },
  { "fillhealth",     0, NULL },
  { "change",         5, cm148 },
  { "change",         5, cm149 },
  { "set",            2, cm150 },
  { "save",           2, cm151 },
  { "restore",        2, cm152 },
  { "exchange",       2, cm153 },
  { "set",            3, cm154 },
  { "center",         1, cm155 },
  { "clear",          1, cm156 },
  { "resetview",      0, NULL },
  { "sam",            2, cm158 },
  { "volume",         1, cm159 },
  { "scrollbase",     2, cm160 },
  { "scrollcorner",   2, cm161 },
  { "scrolltitle",    2, cm162 },
  { "scrollpointer",  2, cm163 },
  { "scrollarrow",    2, cm164 },
  { "viewport",       2, cm165 },
  { "viewport",       3, cm166 },
  { "set",            3, cm167 },
  { "message",        2, cm168 },
  { "save",           3, cm169 },
  { "restore",        3, cm170 },
  { "exchange",       3, cm171 },
  { "restore",        5, cm172 },
  { "exchange",       5, cm173 },
  { "player",         2, cm174 },
  { "player",         2, cm175 },
  { "player",         2, cm176 },
  { "player",         2, cm177 },
  { "neutral",        2, cm178 },
  { "neutral",        2, cm179 },
  { "neutral",        2, cm180 },
  { "neutral",        2, cm181 },
  { "enemy",          2, cm182 },
  { "enemy",          2, cm183 },
  { "enemy",          2, cm184 },
  { "enemy",          2, cm185 },
  { "player",         2, cm186 },
  { "neutral",        2, cm187 },
  { "enemy",          2, cm188 },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "rel",            2, cm194 },
  { "rel",            2, cm195 },
  { "rel",            2, cm196 },
  { "rel",            2, cm197 },
  { "rel",            2, cm198 },
  { "rel",            2, cm199 },
  { "mod",            2, cm200 },
  { "mod",            3, cm201 },
  { "copy",           7, cm202 },
  { "clip",           1, cm203 },
  { "push",           1, cm204 },
  { "scroll",         3, cm205 },
  { "flip",           3, cm206 },
  { "copy",           3, cm207 },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "change",         3, cm211 },
  { "color",          3, cm212 },
  { "color",          4, cm213 },
  { "color",          2, cm214 },
  { "color",          2, cm215 },
  { "set",            5, cm216 },
  { "load",           3, cm217 },
  { "multiply",       2, cm218 },
  { "divide",         2, cm219 },
  { "modulo",         2, cm220 },
  { "player",         3, cm221 },
  { "__unused",       0, NULL },
  { "load",           2, cm223 },
  { "__unused",       0, NULL },
  { "mod",            3, cm225 },
  { "scrollview",     3, cm226 },
  { "swap",           2, cm227 },
  { "if",             3, cm228 },
  { "__unused",       0, NULL },
  { "lockscroll",     0, NULL },
  { "unlockscroll",   0, NULL },
  { "if",             4, cm232 },
  { "persistent",     2, cm233 },
  { "wait",           2, cm234 },
  { "__unused",       0, NULL },
  { "enable",         1, cm236 },
  { "disable",        1, cm237 },
  { "enable",         2, cm238 },
  { "status",         3, cm239 },
  { "overlay",        1, cm240 },
  { "overlay",        1, cm241 },
  { "overlay",        1, cm242 },
  { "put",            5, cm243 },
  { "copy",           8, cm244 },
  { "__unused",       0, NULL },
  { "change",         5, cm246 },
  { "change",         3, cm247 },
  { "write",          5, cm248 },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "loop",           1, cm252 },
  { "loop",           1, cm253 },
  { "abort",          1, cm254 },
  { "disable",        2, cm255 },
  { "enable",         2, cm256 }
};

char *dir_types1[20] =
{
  "IDLE", "NORTH", "SOUTH", "EAST", "WEST", "RANDNS", "RANDEW", "RANDNE", "RANDNB",
  "SEEK", "RANDANY", "BENEATH", "ANYDIR", "FLOW", "NODIR", "RANDB", "RANDP", "CW",
  "OPP", "RANDNOT"
};

char *dir_types2[20] =
{
  "IDLE", "N", "S", "E", "W", "RANDNS", "RANDEW", "RANDNE", "RANDNB",
  "SEEK", "RANDANY", "UNDER", "ANYDIR", "FLOW", "NODIR", "RANDB", "RANDP", "CW",
  "OPP", "RANDNOT"
};

char *dir_types3[20] =
{
  "IDLE", "UP", "DOWN", "RIGHT", "LEFT", "RANDNS", "RANDEW", "RANDNE", "RANDNB",
  "SEEK", "RANDANY", "BENEATH", "ANYDIR", "FLOW", "NODIR", "RANDB", "RANDP", "CW",
  "OPP", "RANDNOT"
};

char *equality_types1[6] =
{
  "=", "!=", ">", "<", ">=", "<="
};

char *equality_types2[6] =
{
  "==", "<>", ">", "<", "=>", "=<"
};

char *equality_types3[6] =
{
  "=", "><", ">", "<", ">=", "<="
};

char *condition_types[18] =
{
  "Walking", "Swimming", "Firewalking", "Touching", "Blocked", "Aligned", "Alignedns",
  "Alignedew", "Lastshot", "Lasttouch", "Rightpressed", "Leftpressed", "Uppressed",
  "Downpressed", "Spacepressed", "Delpressed", "Musicon", "Pcsfxon"
};

char *item_types[9] =
{
  "GEMS", "AMMOS", "TIME", "SCORE", "HEALTHS", "LIVES", "LOBOMBS", "HIBOMBS", "COINS" 
};

char *thing_types[128] = 
{
	"Space",
	"Normal",
	"Solid",
	"Tree",
	"Line",
	"CustomBlock",
	"Breakaway",
	"CustomBreak",
	"Boulder",
	"Crate",
	"CustomPush",
	"Box",
	"CustomBox",
	"Fake",
	"Carpet",
	"Floor",
	"Tiles",
	"CustomFloor",
	"Web",
	"ThickWeb",
	"StillWater",
	"NWater",
	"SWater",
	"EWater",
	"WWater",
	"Ice",
	"Lava",
	"Chest",
	"Gem",
	"MagicGem",
	"Health",
	"Ring",
	"Potion",
	"Energizer",
	"Goop",
	"Ammo",
	"Bomb",
  "LitBomb",
	"Explosion",
	"Key",
	"Lock",
	"Door",
	"OpenDoor",
	"Stairs",
	"Cave",
	"CWRotate",
	"CCWRotate",
	"Gate",
	"OpenGate",
	"Transport",
	"Coin",
	"NMovingWall",
	"SMovingWall",
	"EMovingWall",
	"WMovingWall",
	"Pouch",
	"Pusher",
	"SliderNS",
	"SliderEW",
	"Lazer",
	"LazerGun",
	"Bullet",
	"Missile",
	"Fire",
	"[unknown]",
	"Forest",
	"Life",
	"Whirlpool",
	"Whirlpool2",
	"Whirlpool3",
	"Whirlpool4",
	"InvisWall",
	"RicochetPanel",
	"Ricochet",
	"Mine",
	"Spike",
	"CustomHurt",
	"Text",
	"ShootingFire",
	"Seeker",
	"Snake",
	"Eye",
	"Thief",
	"Slimeblob",
	"Runner",
	"Ghost",
	"Dragon",
	"Fish",
	"Shark",
	"Spider",
	"Goblin",
	"SpittingTiger",
	"BulletGun",
	"SpinningGun",
	"Bear",
	"BearCub",
	"[unknown]",
	"MissileGun",
	"Sprite",
  "Sprite_colliding",
  "Image_file",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
  "__unused",
	"__unused",
  "__unused",
  "__unused",
  "__unused",
	"__unused",
  "__unused",
  "__unused",
  "__unused",
	"Sensor",
	"PushableRobot",
	"Robot",
	"Sign",
	"Scroll",
	"Player" 
};

char *command_fragments[68] =
{
  "not",
  "any",
  "player",
  "ns",
  "ew",
  "attack",
  "item",
  "self",
  "random",
  "string",
  "char",
  "all",
  "edit",
  "pushable",
  "nonpushable",
  "lavawalker",
  "nonlavawalker",
  "row",
  "counters",
  "id",
  "mod",
  "order",
  "thick",
  "arrow",
  "thin",
  "maxhealth",
  "position",
  "mesg",
  "column",
  "color",
  "size",
  "bulletn",
  "bullets",
  "bullete",
  "bulletw",
  "bulletcolor",
  "first",
  "last",
  "fade",
  "out",
  "in",
  "block",
  "sfx",
  "intensity",
  "set",
  "palette",
  "world",
  "alignedrobot",
  "go",
  "saving",
  "sensoronly",
  "on",
  "static",
  "transparent",
  "overlay",
  "start",
  "loop",
  "edge",
  "sam",
  "play",
  "percent",
  "high",
  "matches",
  "none",
  "input",
  "dir",
  "counter",
  "duplicate"
};

char *ignore_list[21] =
{
  ",", ";", "a", "an", "and", "as", "at", "by", "else", "for", "from",
  "into", "is", "of", "the", "then", "there", "through", "thru", "to", "with"
};

int parse_argument(char *cmd_line, char **next, int *arg_translated, int *error)
{
  int i;
  char *tmp_next;
  char current = *cmd_line;

  *error = 0;

  // If the first character is a quote it'll be a string; 
	// make sure it can find the end though.
  if(current == '\"')
  {
    do
    {
      cmd_line++;
      current = *cmd_line;
    } while (current != '\"');

    *arg_translated = 0;
    *next = cmd_line + 1;
    return STRING;
  }

  // If the first character is a negative sign or a numerical constant, 
	// it's an immediate number. I can't figure out the difference between
	// IMM_U16 and IMM_S16 to Robotic so I'm giving IMM_U16 for now.
  
  if((current == '-') || isdigit(current))
  {
    *arg_translated = strtol(cmd_line, next, 10);
    return IMM_U16;
  }

  // If the first letter is a single quote then it's a character; 
	// make sure there's only one constant there and a close quote.
 
  if(current == '\'')
  {
    if(cmd_line[2] != '\'')
    {
      *error = ERR_BADCHARACTER;
      return -1;
    }
    *arg_translated = (int)(cmd_line[1]);
    *next = cmd_line + 3;
    return CHARACTER;
  }

  // Is it a color?
  if(is_color(cmd_line))
  {
    *arg_translated = (int)get_color(cmd_line);
    *next = cmd_line + 3;
    return COLOR;
  }

  // Is it a parameter?
  if(is_param(cmd_line))
  {
    *arg_translated = (int)get_param(cmd_line);
    *next = cmd_line + 3;
    return PARAM;
  }

  // Is it a condition?
  i = is_condition(cmd_line, &tmp_next);
  if(i != -1)
  {
    *arg_translated = i;
    *next = tmp_next;
    return CONDITION;
  }

  // Is it an item?
  i = is_item(cmd_line, &tmp_next);
  if(i != -1)
  {
    *arg_translated = i;
    *next = tmp_next;
    return ITEM;
  }

  // Is it a direction?
  i = is_dir(cmd_line, &tmp_next);
  if(i != -1)
  {
    *arg_translated = i;
    *next = tmp_next;
    return DIR;
  }

  // Is it a command fragment?
  i = is_command_fragment(cmd_line, &tmp_next);
  if(i != -1)
  {
    if(i == (CMD_PLAYER))
    {
      *arg_translated = 127;
    }
    else
    {
      *arg_translated = i;
    }
    *next = tmp_next;
    return CMD | i;
  }

  // Is it a thing?
  i = is_thing(cmd_line, &tmp_next);
  if(i != -1)
  {
    *arg_translated = i;
    *next = tmp_next;
    return THING;
  }

  // Is it an equality?
  i = is_equality(cmd_line, &tmp_next);
  if(i != -1)
  {
    *arg_translated = i;
    *next = tmp_next;
    return EQUALITY;
  }

  // It may be an extra; ignore it then
  i = is_extra(cmd_line, &tmp_next);
  if(i != -1)
  {
    *arg_translated = i;
    *next = tmp_next;
    return EXTRA;
  }

  // It's invalid.
  *error = ERR_INVALID;
  return -1;
}

int is_color(char *cmd_line)
{
  if( (cmd_line[0] == 'c') &&
  ( (isxdigit(cmd_line[1]) && (cmd_line[2] == '?'))  || 
    (isxdigit(cmd_line[2]) && (cmd_line[1] == '?'))  ||
    (isxdigit(cmd_line[1]) && isxdigit(cmd_line[2])) ||
    ((cmd_line[1] == '?') && (cmd_line[2] == '?')) ) )
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int get_color(char *cmd_line)
{  
  if(cmd_line[1] == '?')
  {
    if(cmd_line[2] == '?')
    {
      return 288;
    }
    else
    {
      return strtol(cmd_line + 2, NULL, 16) + 256;      
    }    
  }

  if(cmd_line[2] == '?')
  {
    char temp[2];
    temp[0] = cmd_line[1];
    temp[1] = 0;
    return (strtol(temp, NULL, 16) + 256 + 16);
  }

  return strtol(cmd_line + 1, NULL, 16);    
}

int is_param(char *cmd_line)
{
  if( (cmd_line[0] == 'p') &&
       ( (isxdigit(cmd_line[1]) && isxdigit(cmd_line[2])) ||
        ((cmd_line[1] == '?') && (cmd_line[2] == '?')) ) )
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int get_param(char *cmd_line)
{
  if((cmd_line[1] == '?') && (cmd_line[2] == '?'))
  {
    return 256;
  }
  
  return strtol(cmd_line + 1, NULL, 16);
}

int is_equality(char *cmd_line, char **next)
{
  char temp[256];
  int i = 0;  

  get_word(temp, cmd_line, ' ');

  for(i = 0; i < 6; i++)
  {
    if(!strcasecmp(temp, equality_types1[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }
  for(i = 0; i < 6; i++)
  {
    if(!strcasecmp(temp, equality_types2[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }
  for(i = 0; i < 6; i++)
  {
    if(!strcasecmp(temp, equality_types3[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }

  return -1;
}

int is_dir(char *cmd_line, char **next)
{
  char temp[256];
  int i = 0;  

  get_word(temp, cmd_line, ' ');

  for(i = 0; i < 20; i++)
  {
    if(!strcasecmp(temp, dir_types1[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }
  for(i = 0; i < 20; i++)
  {
    if(!strcasecmp(temp, dir_types2[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }
  for(i = 0; i < 20; i++)
  {
    if(!strcasecmp(temp, dir_types3[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }

  return -1;
}

int is_condition(char *cmd_line, char **next)
{
  char temp[256];
  int i = 0;  

  get_word(temp, cmd_line, ' ');

  for(i = 0; i < 18; i++)
  {
    if(!strcasecmp(temp, condition_types[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }

  return -1;
}

int is_item(char *cmd_line, char **next)
{
  char temp[256];
  int i = 0;  

  get_word(temp, cmd_line, ' ');

  for(i = 0; i < 9; i++)
  {
    if(!strcasecmp(temp, item_types[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }

  return -1;
}

int is_thing(char *cmd_line, char **next)
{
  char temp[256];
  int i = 0;  

  get_word(temp, cmd_line, ' ');

  // It shouldn't actually check thing #127, the player.

  for(i = 0; i < 127; i++)
  {
    if(!strcasecmp(temp, thing_types[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }

  return -1;
}

int is_command_fragment(char *cmd_line, char **next)
{
  char temp[256];
  int i;  

  get_word(temp, cmd_line, ' ');

  for(i = 0; i < 68; i++)
  {
    if(!strcasecmp(temp, command_fragments[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }

  return -1;
}

int is_extra(char *cmd_line, char **next)
{
  char temp[256];
  int i;  

  get_word(temp, cmd_line, ' ');

  for(i = 0; i < 21; i++)
  {
    if(!strcasecmp(temp, ignore_list[i]))
    {
      *next = cmd_line + strlen(temp);
      return i;
    }
  }

  return -1;
}


void get_word(char *str, char *source, char t)
{
  int i = 0;
  char current = *source;

  do
  {
    str[i] = current;
    source++;
    current = *source;
    i++;
  } while((current != t) && (current != 0) && (i < 256));

  str[i] = 0;
}

int match_command(mzx_command *cmd)
{
  int i, i2;
  for(i = 0; i < 256; i++)
  {
    if(strcasecmp(cmd->name, command_list[i].name))
    {
      continue;
    }
    if(cmd->parameters != command_list[i].parameters)
    {
      continue;
    }
    for(i2 = 0; i2 < cmd->parameters; i2++)
    {
			if(!(command_list[i].param_types[i2] & IGNORE))
			{
				if(command_list[i].param_types[i2] & CMD)
				{
					if(cmd->param_types[i2] != command_list[i].param_types[i2])
					{
						break;
					}
				}
				else
				{        
					if((cmd->param_types[i2] & command_list[i].param_types[i2]) !=
					 cmd->param_types[i2])
					{
						break;
					}
				}
			}
			else
			{
				// Don't count this one
				i2--;
			}
    }
    if(i2 == cmd->parameters) break;
  }

  return i;
}

int assemble_text(char *input_name, char *output_name)
{
  FILE *input_file, *output_file;
  char current_line[256];
  char *current_line_position;
  char *next_line_position;
  char object_file[32768];  
  char *object_file_position = object_file + 1;
  char *next_object_file_position;
  int line_number = 1;
  int bytes_assembled = 1;
  int translated_command = 0;

  mzx_command current_command;
  char name[256];
  char temp[256];
  void *param_list[32];
  int i;
  int current_arg_type;
  int current_arg_translation;
  int last_arg_type;
  int last_arg_translation;
  int arg_count;
  int error = 0;
  int bytes_used;
  int advance;

  object_file[0] = -1;

  input_file = fopen(input_name, "rb");
  if(input_file == NULL)
  {
    printf("Failed to open %s.\n", input_name);
    return -1;
  }

  printf("Opening source file %s\n", input_name);

  while(1)
  {
    current_command.name = (char *)malloc(32);
    current_command.param_types = (int *)malloc(32 * 4);    

    if(get_line(current_line, input_file) == -1)
    {
      *object_file_position = 0;      
      break;
    }

    arg_count = 0;

    if(current_line[0] == 0)
    {
      translated_command = 47;
      strcpy(current_command.name, command_list[47].name);
      current_command.parameters = 0;
      last_arg_type = current_arg_type;
      last_arg_translation = current_arg_translation;
      arg_count++;
    }
    else
    {
      current_line_position = current_line;
  
      get_word(name, current_line_position, ' ');
      strcpy(current_command.name, name);

      current_line_position += strlen(name);
  
      last_arg_type = 0;
  
      while(*current_line_position != 0)
      {
        skip_whitespace(current_line_position, &current_line_position);

        current_arg_type = parse_argument(current_line_position,
				 &next_line_position, &current_arg_translation, &error);
        current_command.param_types[arg_count] = current_arg_type;
          
        if(error == ERR_BADSTRING)
        {
          printf("%d: Unterminated string.\n", line_number);
          return -1;
        }
        if(error == ERR_BADCHARACTER)
        {
          printf("%d: Unterminated char constant.\n", line_number);
          return -1;
        }
        if(error == ERR_INVALID)
        {
          get_word(temp, current_line_position, ' ');
          printf("%d: Invalid argument '%s'\n", line_number, temp);
          return -1;
        }

        if(current_arg_type != EXTRA)
        {
          if(current_arg_type == STRING)
          {
            // Grab the string off the command list.
            int str_size;
  
            get_word(temp, current_line_position + 1, '\"');
            str_size = strlen(temp);
            param_list[arg_count] = (void *)malloc(str_size + 1);
            strcpy((char *)param_list[arg_count], temp);
          }
          else
          {
            // Store the translation into the command list.
            param_list[arg_count] = (void *)malloc(2);
            *((short *)param_list[arg_count]) = current_arg_translation;
          }

          advance = 1;
    
          if((arg_count > 0) && (current_arg_type == DIR))
          {
            if((last_arg_type == CONDITION) &&
            ((last_arg_translation == 0) || (last_arg_translation == 3) ||
            (last_arg_translation == 4)))
            {
              *((short *)param_list[arg_count - 1]) 
               |= (current_arg_translation << 8);
              advance = 0;
            }
            if((last_arg_type == DIR) && (last_arg_translation >= 16))
            {
              *((short *)param_list[arg_count - 1])
               += (1 << (last_arg_translation - 12));
              advance = 0;
            }              
          }

          if(advance)
          {
            last_arg_type = current_arg_type;
            last_arg_translation = current_arg_translation;
            arg_count++;
          }
      
          if(arg_count == 32)
          {
            printf("%d: Too many arguments to parse.\n", line_number);
            return -1;
          }
        }
        current_line_position = next_line_position;
      } 

      current_command.parameters = arg_count;
      translated_command = match_command(&current_command);
      if(translated_command == 256)
      {
        printf("%d: Line '%s':\n", line_number, current_line);
        printf("%d: Command does not exist: ", line_number);
        print_command(&current_command);
        return -1;
      }

      for(i = 0; i < arg_count; i++)
      {
        free(param_list[i]);
      }

      line_number++;
    }
    
    bytes_used = 
     assemble_command(translated_command, &current_command, param_list, 
     object_file_position, &next_object_file_position);
    printf("%d bytes: ", bytes_used);
    print_command(&current_command);
    bytes_assembled += bytes_used;
    if(bytes_assembled >= MAX_OBJ_SIZE)
    {
      printf("%d: Maximum robot size exceeded.\n", line_number);
      return -1;
    }
    object_file_position = next_object_file_position;
  }    

  fclose(input_file);

  *object_file_position = 0;
  bytes_assembled++;

  output_file = fopen(output_name, "wb");
  fwrite(object_file, 1, bytes_assembled, output_file);

  printf("Assembled object file: %s, %d bytes\n", output_name, bytes_assembled);

  fclose(output_file);
    
  return 0;
}

void print_command(mzx_command *cmd)
{
  int i;

  printf("%s ", cmd->name);
  for(i = 0; i < cmd->parameters; i++)
  {
    switch(cmd->param_types[i])
    {
      case IMM_U16:
      {
        printf("[##] ");
        break;
      }
      case STRING:
      {
        printf("[str] ");
        break;
      }
      case COLOR:
      {
        printf("[col] ");
        break;
      }
      case PARAM:
      {
        printf("[par] ");
        break;
      }
      case CHARACTER:
      {
        printf("[chr] ");
        break;
      }
      case CONDITION:
      {
        printf("[cond] ");
        break;
      }
      case EQUALITY:
      {
        printf("[!<>=] ");
        break;
      }
      case DIR:
      {
        printf("[dir] ");
        break;
      }
      case ITEM:
      {
        printf("[dir] ");
        break;
      }
      case THING:
      {
        printf("[thing] ");
        break;
      }
      default:
      {
        if(cmd->param_types[i] & CMD)
        {
          printf("%s ", command_fragments[cmd->param_types[i] & ~CMD]);
        }
        else
        {
          printf("[unknown] ");
          break;
        }
      }
    }      
  }
  printf("\n");
}  

int assemble_command(int command_number, mzx_command *cmd, void *params[32], 
 char *obj_pos, char **next_obj_pos)
{
  int i;
  int size;
  char *c_obj_pos = obj_pos + 2;

  obj_pos[1] = (char)command_number;

  for(i = 0; i < cmd->parameters; i++)
  {
    if(cmd->param_types[i] == STRING)
    {
      int str_size = strlen((char *)params[i]);
      *(c_obj_pos) = str_size + 1;
      strcpy(c_obj_pos + 1, (char *)params[i]);
      c_obj_pos += str_size + 2;
    }
    else
    {
      if(!(cmd->param_types[i] & CMD))
      {
        // It's not a command fragment
        *c_obj_pos = 0;
        *((short int *)(c_obj_pos + 1)) = *((short *)params[i]);
        c_obj_pos += 3;
      }
    }
  }    

  size = c_obj_pos - obj_pos - 1;
  *obj_pos = size;
  *c_obj_pos = size;

  *next_obj_pos = c_obj_pos + 1;

  return size + 2;
}

int get_line(char *buffer, FILE *fp)
{
  int current;
  int escape_1 = 0;
  int i;

  // Should ignore \0, \n, and \r while in ' or "

  for(i = 0; i < 256; i++)
  {
    current = fgetc(fp);
    if(current == -1)
    {
      return -1;
    }
    if(current == '\"')
    {
      if(escape_1 == 0)
      {
        escape_1 = 1;
      }
      else
      {
        escape_1 = 0;
      }
    }

    if(!escape_1)
    {
      if(current == '\r')
      {
        fgetc(fp);
        buffer[i] = 0;
        return 0;
      }
  
      if(current == '\n')
      {
        buffer[i] = 0;
        return 0;
      }
    }
    buffer[i] = current;
  }
  buffer[255] = 0;

  return -2;
}

void skip_whitespace(char *cpos, char **next)
{
  while(*cpos == ' ')
  {
    cpos++;
  }
  *next = cpos;
}

int main(int argc, char *argv[])
{
  if(argc != 3)
  {
    printf("Usage: <robotic code> <object file>\n");
    return -1;
  }

  if(assemble_text(argv[1], argv[2]) == -1)
  {
    printf("Fatal error; aborting.\n");
    return -1;
  }

  return 0;
}
