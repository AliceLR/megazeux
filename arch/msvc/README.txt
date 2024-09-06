BUILDING MEGAZEUX WITH MICROSOFT VISUAL STUDIO

This document covers the rationale and caveats regarding MegaZeux
compilation with Microsoft's Visual C++ 2017 compiler (henceforth MSVC).

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


OPENING THE PROJECT

The pre-built project files can be found in this directory as "MegaZeux.sln".
The solution may not always work, as it is not tested often. At the time of
writing, Visual Studio 2017 was the minimum working version required to open
it. It may work using Visual Studio 2013 and/or 2015, but those versions have
not been tested. You must also build MZX's dependencies into a "Deps" folder
alongside the solution, or use the prebuilt Deps found in the releases here:

https://github.com/AliceLR/megazeux-dependencies

There may be instability with the MSVC binary that is not present in the GCC
builds. If you find such instability, please report it (fixes are also
welcome).

--ajs (20130711) / spectere (20171123)
