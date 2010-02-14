checkres overview:
------------------

checkres is a utility that scans an mzx world and finds all of the resources it
relies on.  This means files such as palettes, character sets, modules and
sound effects, as referenced in robotic commands such as LOAD PAL,
LOAD CHAR SET, MOD, SAM, etc.  It also finds references to FILE_OPEN,
LOAD_ROBOT, and more.  What's more, checkres determines if any of those
resources are missing, and informs you of that fact.

checkres usage:
---------------

checkres can be used either via the batch script included or through the
command prompt, as demonstrated by these examples.

	c:\mzx>checkres.bat world.mzx
	Scanning world.mzx...
	missing.sam - NOT FOUND
	title.pal - NOT FOUND
	Press any key to continue...

This also means that you can drag a world file to the batch file icon, and
it'll work perfectly.  In fact, the batch file can accept as many worlds as
you like on the command line, like so:

	c:\mzx>checkres.bat world.mzx world2.mzx world3.mzx

checkres also supports reading zip files to ensure that all resources are
present in the archive.  This is extremely simple to do:

	c:\mzx>checkres.bat world.zip

Finally, checkres supports two options to enhance your resource checking
abilities, -q (quiet) and -a (all).  These can be used like so:

	c:\mzx>checkres.bat -q world.mzx
	Scanning world.mzx...
	missing.sam
	title.pal
	Press any key to continue...

	c:\mzx>checkres.bat -a world.mzx
	Scanning world.mzx...
	missing.sam - NOT FOUND
	music.mod - FOUND
	title.pal - NOT FOUND
	graphics.chr - FOUND
	Press any key to continue...

	c:\mzx>checkres.bat -a -q world.mzx
	Scanning world.mzx...
	missing.sam
	music.mod
	title.pal
	graphics.chr
	Press any key to continue...

This covers all the possibilities of checkres.

checkres conclusion:
--------------------

checkres is a product of ajs and Revvy, and the source for those not win32
inclined is available at:

http://github.com/ajs1984/megazeux/raw/master/src/utils/checkres.c

Copyright (c) 2007 Josh Matthews (mrlachatte@gmail.com)
