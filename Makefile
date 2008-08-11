#
# You need TASM 3.0 and Borland C++ 3.0 to use this Makefile.
#
# Please note that in these old versions of MegaZeux, the source
# for the robot editor was lost. Therefore this file just hacks
# in robo_ed.obj when required.
#

.AUTODEPEND

INCPATH=C:\BORLANDC\INCLUDE;.
LIBPATH=C:\BORLANDC\LIB;.

CC = bcc +megazeux.cfg -I$(INCPATH) -L$(LIBPATH)
TASM = tasm
TLIB = tlib
TLINK = tlink

.c.obj:
	@$(CC) -c {$< }

.cpp.obj:
	@$(CC) -c {$< }

.asm.obj:
	@$(TASM) /jWARN /MX /M5 /ZI /O /T $<,$@

obj  = admath.obj arrowkey.obj beep.obj blink.obj block.obj boardmem.obj \
ceh.obj charset.obj char_ed.obj comp_chk.obj counter.obj cursor.obj \
data.obj data2.obj detect.obj dt_data.obj edit.obj edit_di.obj egacode.obj \
ems.obj error.obj ezboard.obj fill.obj game.obj game2.obj getkey.obj \
graphics.obj helpsys.obj hexchar.obj idarray.obj idput.obj intake.obj \
main.obj meminter.obj meter.obj mouse.obj new_mod.obj palette.obj \
pal_ed.obj param.obj password.obj random.obj retrace.obj roballoc.obj \
runrobot.obj runrobo2.obj saveload.obj scrdisp.obj scrdump.obj sfx.obj \
sfx_edit.obj string.obj timer.obj window.obj

#
# I'd rather this wasn't necessary, but I can't think of a way either in
# Make or DOS to convert the space separated list above into a + separated
# list. So it's just copy/pasted. This tr command helps:
#   tr ' ' '+' << "EOF"
#
lobj = admath.obj+arrowkey.obj+beep.obj+blink.obj+block.obj+boardmem.obj+\
ceh.obj+charset.obj+char_ed.obj+comp_chk.obj+counter.obj+cursor.obj+\
data.obj+data2.obj+detect.obj+dt_data.obj+edit.obj+edit_di.obj+egacode.obj+\
ems.obj+error.obj+ezboard.obj+fill.obj+game.obj+game2.obj+getkey.obj+\
graphics.obj+helpsys.obj+hexchar.obj+idarray.obj+idput.obj+intake.obj+\
main.obj+meminter.obj+meter.obj+mouse.obj+new_mod.obj+palette.obj+\
pal_ed.obj+param.obj+password.obj+random.obj+retrace.obj+roballoc.obj+\
runrobot.obj+runrobo2.obj+saveload.obj+scrdisp.obj+scrdump.obj+sfx.obj+\
sfx_edit.obj+string.obj+timer.obj+window.obj

all: megazeux.exe fix.exe getpw.exe killgbl.exe txt2hlp.exe ver1to2.exe

megazeux.exe: $(obj)
	@$(TLINK) /m/c/d/P-/L$(LIBPATH) @&&|
c0l.obj+$(lobj)+robo_ed.obj,megazeux,megazeux,mse_cl.lib+cl.lib
|

#
# Other external binaries that might be useful
#

fix.exe: fix.obj
	@$(TLINK) /m/c/d/P-/L$(LIBPATH) @&&|
c0l.obj+fix.obj,fix,fix,cl.lib
|

getpw.exe: getpw.obj
	@$(TLINK) /m/c/d/P-/L$(LIBPATH) @&&|
c0l.obj+getpw.obj,getpw,getpw,cl.lib
|

killgbl.exe: killgbl.obj
	@$(TLINK) /m/c/d/P-/L$(LIBPATH) @&&|
c0l.obj+killgbl.obj,killgbl,killgbl,cl.lib
|

txt2hlp.exe: txt2hlp.obj
	@$(TLINK) /m/c/d/P-/L$(LIBPATH) @&&|
c0l.obj+txt2hlp.obj,txt2hlp,txt2hlp,cl.lib
|

ver1to2.exe: ver1to2.obj
	@$(TLINK) /m/c/d/P-/L$(LIBPATH) @&&|
c0l.obj+ver1to2.obj,ver1to2,ver1to2,cl.lib
|

clean:
	@ren robo_ed.obj robo_ed.bak
	@del *.obj
	@ren robo_ed.bak robo_ed.obj
	@del *.exe
	@del *.map
