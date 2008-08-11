
**SUPPORT/UPDATES

  Up-to-the-minute upgrades of the official MegaZeux distributation are   available via FTP at:

		http://www.scsn.net/users/dbwilli/

  Contact Matt Williams:

      Internet: dbwilli@scsn.net
	WWW: http://www.scsn.net/users/dbwilli/
	
**NEW MUSIC CODE

  MegaZeux 2.51 has all-new music code. Some important new features-

  * Up to 32 music/sound channels playing simultaneously
  * Up to 4 simultaneous sound effects
  * Support for stereo and 16-bit sound cards
  * Support for SoundBlaster (1.x/2.x/16/Pro) GUS and PAS-16
  * Support for almost all file formats through a conversion program

  Sorry, but Sound Source and PC Speaker output have been removed.
  Note that MID and XM are NOT supported. MOD files can be loaded as
  normal, but all other supported formats MUST be converted to a GDM
  file first using the included 2GDM utility. Supported formats
  include MOD/NST/WOW/OCT, S3M, 669, and MTM.

  See the 'MegaZeux's Sound System' section of Help for details.

**GETTING MEGAZEUX STARTED-

  Many people have had problems getting MegaZeux to run on their
  systems. MegaZeux requires EMS for most usage, and many computers
  will need to use a boot disk.

  MegaZeux makes extensive use of EMS and conventional (first
  640k) memory. The more you have available, the better it will
  run.

  To have EMS memory available, you must have an EMS driver
  installed. Most EMS drivers also require an XMS/HIMEM driver
  loaded first. You will need lines similar to the following in
  your CONFIG.SYS to load HIMEM and an EMS driver, respectively-

  DEVICE=C:\DOS\HIMEM.SYS
  DEVICEHIGH=C:\DOS\EMM386.EXE 3072 H=160

  The 3072 tells DOS to reserve up to 3072K of memory (3
  megabytes) for EMS. Feel free to lower or raise this value,
  but a minimum of 2048K (2 megabytes) is recommended.

  It is also recommended that you load SmartDrive to run MegaZeux.
  SmartDrive is a disk-cache utility that is usually loaded,
  because it greatly speeds up repeat disk accesses. You probably
  already have a line such as SMARTDRV.EXE /X in your AUTOEXEC.BAT;
  if not, you should consider adding it somewhere after the PATH
  statement.

  To help free up conventional memory, you may need to create a
  boot disk. This is a common procedure for games that use less
  conventional setups (such as MegaZeux), and if a certain boot disk
  works for one game, it may work just fine for MegaZeux. However,
  you must remember to include HIMEM and an EMS driver, and include
  a mouse driver if you want your mouse to be available.

  To create a boot disk, stick a blank disk into your A: drive and
  type the following:

  FORMAT A: /S <enter>

  Answer Yes to all questions. Give the disk a label if you wish.
  Answer No to "Format another?" and then type:

  COPY CON A:CONFIG.SYS <enter>
  DEVICE=C:\DOS\HIMEM.SYS <enter>
  DEVICEHIGH=C:\DOS\EMM386.EXE 4096 H=160 <enter>
  DOS=HIGH <enter>
  <ctrl+z>

  COPY CON A:AUTOEXEC.BAT <enter>
  PATH C:\DOS;C:\WINDOWS <enter>
  SMARTDRV /X <enter>
  C:\MOUSE\MOUSE <enter>
  C: <enter>
  CD \MEGAZEUX <enter>
  MEGAZEUX <enter>
  <ctrl+z>

  The line to load the mouse driver is optional, and may vary on
  different computers. The drive and directory you have MegaZeux
  in may differ as well. Your windows and dos paths may differ.

  Once you've followed these instructions, you can insert the
  disk and reboot the computer to run MegaZeux. It is best if you
  get a computer-wise friend to help you, if you can't get the
  above to work.

  One note- most newer sound cards require drivers or other
  programs to be run before they can be used. Check your existing
  configuration files and/or manuals to find out what these
  commands are if you intend to use MegaZeux with a boot disk.

**PREVIOUS VERSION USERS-

  If you have used versions 2.00 or 2.01 of MegaZeux at any time,
  you should run the program FIX.EXE in every directory containing
  .MZX and .MZB files. This fixes a small bug in version 2.00/2.01.
  .SAV files may still run strangely- if so, start the game anew.

**NEW USERS-

  Welcome to MegaZeux!

  As you may already know, MegaZeux is a game system which
  allows you to play almost limitless worlds in EGA graphics
  and with beautiful digitized music and sound. MegaZeux comes
  with worlds, and new worlds are being uploaded every day to
  major on-line services such as America Online. But the most
  fabulous feature of MegaZeux is the World Editor.

  Using the World Editor, ANYONE can create the world of their
  dreams. Make it as simple or complex, as easy or difficult,
  as long or short as you please. And we aren't just talking
  about worlds made up of petty, pre-programmed enemies and
  objects. MegaZeux has it's own, easy to use PROGRAMMING
  LANGUAGE called Robotic that allows you to create enemies,
  objects, and worlds that do almost anything you desire.

  For the new user, I recommend you play Caverns, the first
  included game, to get the feel of MegaZeux. You may wish
  to read the help or .DOC section entitled "Controls" to
  learn how to play MegaZeux.

  Once you have the feel for the game, however, feel free
  to dive into the World Editor and get messy! You should
  probably read the help or .DOC section entitled "The World
  Editor" first.

  Use F1 to read help, or read MEGAZEUX.DOC, for more detailed
  information.