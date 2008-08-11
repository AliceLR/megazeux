
; Include file- CURSOR.INC
; Declarations for CURSOR.ASM

global C cursor_underline:far
global C cursor_solid:far
global C cursor_off:far
global C move_cursor:far

global C cursor_mode:byte

CURSOR_UNDERL EQU 0
CURSOR_BLOCK EQU 1
CURSOR_INVIS EQU 2