BUILDING MEGAZEUX WITH MICROSOFT VISUAL STUDIO

This document covers the rationale and caveats regarding MegaZeux
compilation with Microsoft's Visual C++ 2013 Preview compiler
(henceforth MSVC).

RATIONALE

The primary advantage of compiler portability is the idea that program
correctness may be enriched, by leveraging the different advantages of
different compilers. This has certainly been true of the ports to 64bit
platforms, and will hopefully be true of compiler diversity.

Porting to MSVC increases MegaZeux's compiler portability.

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

The pre-built project files can be found in this directory as "MegaZeux.sln".
The solution may not always work, as it is not tested often. At the time of
writing, Visual Studio 2013 Preview was the minimum working version required
to open it. You must also build MZX's dependencies into a "Deps" folder
alongside the solution, or use these prebuilt Deps:

https://github.com/AliceLR/megazeux/raw/master/scripts/deps/msvc.zip

There may be instability with the MSVC binary that is not present in the GCC
builds. If you find such instability, please report it (fixes are also
welcome).

--ajs (20130711)
