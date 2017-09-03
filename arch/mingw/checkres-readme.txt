checkres overview:
------------------

checkres is a utility that scans MZX worlds and finds all of the resources they
rely on.  checkres will return a list of all palettes, character sets, sound
files, and other files referenced in Robotic code and in board settings.  This
includes file commands like LOAD PALETTE, alternate functions of other commands
like PUT "@file" Image_file p00 [#] [#], and function counters like FREAD_OPEN.

checkres will attempt to match these references to real files in the directory
of the MZX world--or to Robotic commands that may produce those files in gameplay.


checkres usage:
---------------

checkres can be used two ways: by dragging and dropping files onto checkres.bat
in Windows Explorer or by using the checkres executable directly on the command line.

  C:\MZX>utils\checkres.exe mzm2ans.mzx

  Required by   Resource path     Status     Found in
  -----------   -------------     ------     --------
  mzm2ans.mzx   ai/barrier.txt    NOT FOUND
  mzm2ans.mzx   ai/ws-item.txt    NOT FOUND

  Finished processing 'mzm2ans.mzx'.


Both methods will allow multiple inputs, e.g. dragging multiple files or

  C:\MZX>checkres.exe world1.mzx world2.mzx world3.mzx


checkres also supports MZX board files and ZIP archives.  checkres will scan
every .mzx and .mzb file found in a .zip archive.

checkres supports multiple options available from the command line only:

  -h              Display usage info.
  -a              Display all dependencies (found, not found, maybe created).
  -A              Display all dependencies that do not exist (not found, maybe created).
  -q              Prints resource paths only, with no extra formatting.

  -extra [file]   Extra dependency path for the last provided base file.  This may be a
                  folder or a zip archive.  For a .mzx or .mzb base file, the contents of
                  this path will be treated as existing in the same folder as the base
                  file.  For a zip archive, the contents of this path will be treated as
                  existing at the root of the zip archive.

  -in [path]      Provides a relative path for the extra dependency preceding it.


checkres examples:
------------------

  C:\MZX>checkres.exe -a Labyrinth-Platinum.zip

  Required by              Resource path               Status     Found in
  -----------              -------------               ------     --------
  Labyrinth/joymap.mzx     Labyrinth/joymap.cnf        FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/temp/disc         CREATED
  Labyrinth/labyrinth.mzx  Labyrinth/ZEUX/ascii.chr    FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/ZEUX.CHR          FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/ZEUX.PAL          FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/__editor          CREATED
  Labyrinth/labyrinth.mzx  Labyrinth/sfx/yeah.wav      FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/sfx/bombdrop.ogg  FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/sfx/bonus2.ogg    FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/sfx/unlock.ogg    FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/sfx/life.ogg      FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/ZEUX.ZCFG         NOT FOUND
  Labyrinth/labyrinth.mzx  Labyrinth/joymap.mzx        FOUND      Labyrinth-Platinum.zip
  Labyrinth/labyrinth.mzx  Labyrinth/__menu            CREATED

  Finished processing 'Labyrinth-Platinum.zip'.


  C:\MZX>checkres.exe -A -q DE_Game.zip -extra DE_Sound.zip

  room1.mzm
  room2.mzm
  lightng2.sam


  C:\MZX>checkres.exe -A -q DE_Game.zip -extra DE_Sound.zip Taoyarin_v1.02.zip -extra ty_music.zip

  room1.mzm
  room2.mzm
  lightng2.sam
  ctree.raw
  dither.raw
  t2.dat


copyright:
----------

checkres is a product of ajs and Revvy, with a massive rewrite by Lachesis.
The source code for checkres is available here:

http://github.com/AliceLR/megazeux/raw/master/src/utils/checkres.c

Note that checkres is dependent on the MegaZeux source code and will not build
separately from it.

Copyright (c) 2007 Josh Matthews (mrlachatte@gmail.com)
Copyright (C) 2017 Alice Rowan (petrifiedrowan@gmail.com)
