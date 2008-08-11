
; Include file- ARROWKEY.INC
; Declarations for ARROWKEY.ASM

global C install_i09:far
global C uninstall_i09:far
global C key_code:byte
global C up_pressed:byte
global C dn_pressed:byte
global C lf_pressed:byte
global C rt_pressed:byte
global C sp_pressed:byte
global C del_pressed:byte
global C kb_installed:byte
global C keyb_mode:byte			; Set to 1 for alternate code
global C switch_keyb_table:far
global C curr_table:byte
global C state_table:far