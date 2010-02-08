BUILDING MEGAZEUX WITH MICROSOFT VISUAL STUDIO

This document covers the rationale and caveats regarding MegaZeux
compilation with Microsoft's Visual C++ 2005 compiler (henceforth MSVC).


RATIONALE

Since MegaZeux's initial port to SDL/GCC by Exophase at version 2.80, much
effort has gone into improving MegaZeux's features. Unfortunately, MegaZeux's
portability was originally quite limited, only officially supporting Intel x86
Windows and Linux platforms.

Portability breakthroughs occurred with endian safety fixes to MegaZeux,
libmodplug and libgdm2s3m to improve functionality on big-endian platforms.
This work extended to PowerPC Macintosh platforms. Later, effort was put in to
make code 64bit safe, so that portability to AMD64 Linux also became possible.

However, one area that MegaZeux has suffered is its lack of portability to
different C compilers. A major obstacle to this support was the hash of C/C++
that the port originally used. Recently, MegaZeux was backported to pure C,
which reduced the minimum compiler requirements. Also, sticking closer to C
standards with GCC's "-W -Wall" warning flags allowed warts from the original
DOS code to be eliminated.

Even GCC, historically a very standards compliant compiler, has issues with
backwards compatibility. Care has been given to ensure MegaZeux builds
consistently with GCC 4.2, 3.4, and 3.3.5-propolice (OpenBSD). Also, support
for variant C ABIs like MinGW has been maintained.

As MinGW (a free GCC based compiler for Windows 32bit platforms) has not yet
completed a port to Microsoft's new "x64" 64bit platforms, it is currently not
possible to use GCC to build MegaZeux for 64bit Windows. An obvious interim
solution would be to use MSVC to build for this platform.

Another potential advantage of compiler portability is the idea that program
correctness may be enriched, by leveraging the different advantages of
different compilers. This has certainly been true of the ports to 64bit
platforms, and will hopefully be true of compiler diversity.

(EDIT: This premise was soon realized after finding several _bugs_ in
 libmodplug that GCC had erroneously not detected.)


VISUAL STUDIO LIMITATIONS AND WORKAROUNDS

Microsoft's Visual C++ compiler has improved significantly in recent years,
undoubtedly due to its rivalry with the free GCC compiler. Compatibility with
UNIX and POSIX APIs has matured to the point where much software depending on
these APIs can be compiled without issue. MegaZeux is one such application.

However, MSVC has the following constraints:

  - No UNIX "dirent" API support
      Fixed using Toni Ronkko's dirent.h implementation for MSVC
      This header is BSD licensed and compatible with MegaZeux's GPL license
      See "dirent.h" in this directory

  - No snprintf, strcasecmp, strncasecmp functions
      Fixed with compatible replacements
      Uses _snprintf, stricmp, strnicmp respectively

  - No S_ISDIR() macro for the POSIX stat(2) API
      #define S_ISDIR(mode) (mode & _S_IFDIR)

  - No unistd.h
      MSVC internally defines most of this anyway, so we simply do not
      #include it in MSVC builds.

  - No extensions support for mixing code and variable declarations
      MegaZeux did not depend on this anyway, and it is fairly sloppy
      to rely on this in C89 programs. C++ and C99 permit such mixes,
      but I simply recoded the failure points to not mix code and
      declarations.

  - No C99 support
     MegaZeux also uses C99 to declare variable length arrays on the
     stack. Such support could be expressed with alloca(), but I simply
     rewrote it to rely on heap functions.


CURRENT LIMITATIONS

The PNG library was not tested for compatibility, and PNG screendumps are not
enabled. This code should work, however. Edit config.h if you are interested.
The appropriate headers and libraries should be added to msvc/Deps.


OPENING THE PROJECT

The pre-built project files can be found in "msvc.zip" in the top level of
the sources. This package may not always work, as it is not tested often. At
the time of writing, Visual Studio 2005 was the minimum working version
required to open it. The ZIP should create a directory, "msvc", in the top
level, before this will work.

You may need to alter the include paths and possibly re-add the project
libraries to get it to compile on your machine. Any suggestions for improving
this are of course welcome.

At the moment, the resulting binary does not run and I haven't had time to
track down the root cause. Any experienced MSVC programmer should be able to
fix this (please send me any fixes).

--ajs
