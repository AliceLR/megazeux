Apply the patches in this directory to bring a stock libmodplug-0.8.9.0
distribution in line with the local copy used by MegaZeux. Additionally,
load_gdm.cpp needs to be copied from the local MegaZeux libmodplug.

In addition to the patches, the following files should be removed, as
they are no longer necessary:

- src/load_abc.cpp
- src/load_ams.cpp
- src/load_dbm.cpp
- src/load_dmf.cpp
- src/load_j2b.cpp
- src/load_mdl.cpp
- src/load_mid.cpp
- src/load_mt2.cpp
- src/load_pat.cpp
- src/load_pat.h
- src/load_psm.cpp
- src/load_ptm.cpp
- src/load_umx.cpp

--asie (20170524T19:22)
--ajs (20120128T15:42)
