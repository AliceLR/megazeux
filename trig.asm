;  $Id$
;  MegaZeux
;
;  Copyright (C) 1996 Greg Janson
;  Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
;  Copyright (C) 2004 Gilead kutnick
;
;  This program is free software; you can redistribute it and/or
;  modify it under the terms of the GNU General Public License as
;  published by the Free Software Foundation; either version 2 of
;  the License, or (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;  General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program; if not, write to the Free Software
;  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

;
; TRIG.ASM- Trig functions for trig counters.
;
 
Ideal

p386
JUMPS
include "trig.inc"
include "model.inc"

Dataseg

return_value dw ?
multiplier dw 10000
divider dw 10000
c_divisions dw 360

const_2pi dt 6.2831853071796

Codeseg
  
proc sin far
arg theta:word

  shl [theta], 1
  fild [theta]
  fldpi
  fmulp st(1), st(0)
  fidiv [c_divisions]

  fsin
  fimul [multiplier]
  fistp [return_value]
  mov ax, [return_value]
  ret

endp sin

proc cos far
arg theta:word

  shl [theta], 1
  fild [theta]
  fldpi
  fmulp st(1), st(0)
  fidiv [c_divisions]
  fcos
  fimul [multiplier]
  fistp [return_value]
  mov ax, [return_value]
  ret

endp cos

proc tan far
arg theta:word

  shl theta, 1
  fild [theta]
  fldpi
  fmulp st(1), st(0)
  fidiv [c_divisions]
  fptan
  fstp st(0)
  fimul [multiplier]
  fistp [return_value]
  mov ax, [return_value]
  ret
  
endp tan
  
proc atan far
arg val:word
          
 fild [val]
 fidiv [divider]
 fld1
 fpatan
 fimul [c_divisions]
 fld const_2pi
 fdivp
 fistp [return_value]
 mov ax, [return_value]
 ret
 
endp atan

proc asin far
arg val:word
          
  fild [val]
  fidiv [divider]    
  fld st(0)                 ; put a in st(1)
  fmul st(0), st(0)         ; square a
  fld1                      ; load 1
  fsubrp                    ; 1 - a^2
  fsqrt                     ; take sqrt
  fpatan
  fimul [c_divisions]
  fld const_2pi
  fdivp
  fistp [return_value]
  mov ax, [return_value]
  ret
   
endp asin

proc acos far
arg val:word
          
  fild [val]
  fidiv [divider]    
  fld st(0)                 ; put a in st(1)
  fmul st(0), st(0)         ; squar a
  fld1                      ; load 1
  fsubrp                    ; 1 - a^2
  fsqrt                     ; take sqrt
  fxch                      ; swap st(0) and st(1)
  fpatan
  fimul [c_divisions]
  fld [const_2pi]
  fdivp
  fistp [return_value]
  mov ax, [return_value]
  ret

endp acos

ends

end
