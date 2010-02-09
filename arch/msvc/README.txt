BUILDING MEGAZEUX WITH MICROSOFT VISUAL STUDIO

This document covers the rationale and caveats regarding MegaZeux
compilation with Microsoft's Visual C++ 2008 compiler (henceforth MSVC).

RATIONALE

As MinGW (a free GCC based compiler for Windows 32bit platforms) has not yet
completed a port to Microsoft's new "x64" 64bit platforms, it is currently not
possible to use GCC to build MegaZeux for 64bit Windows. An obvious interim
solution would be to use MSVC to build for this platform.

Another potential advantage of compiler portability is the idea that program
correctness may be enriched, by leveraging the different advantages of
different compilers. This has certainly been true of the ports to 64bit
platforms, and will hopefully be true of compiler diversity.

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


OPENING THE PROJECT

The pre-built project files can be found in "msvc.zip" in the top level of
the sources. This package may not always work, as it is not tested often. At
the time of writing, Visual Studio 2008 was the minimum working version
required to open it. The ZIP should create a directory, "msvc", in the top
level, before this will work.

You may need to alter the include paths and possibly re-add the project
libraries to get it to compile on your machine. Any suggestions for improving
this are of course welcome.

There may be instability with the MSVC binary that is not present in the GCC
builds. If you find such instability, please report it (fixes are also
welcome).

--ajs
