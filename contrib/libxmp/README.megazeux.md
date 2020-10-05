# MegaZeux libxmp fork.

This is a fork of libxmp generated specifically for MegaZeux
from this branch using the file `mzx-gen.sh`: https://github.com/AliceLR/libxmp/tree/mzx-gen-libxmp

The script that generated this forked libxmp from commit
[92313f6f](https://github.com/AliceLR/libxmp/commit/92313f6f525a8510a2492df4266abcf8f0b45834)
and applied the following branches as patches:

* [mingw-snprintf](https://github.com/AliceLR/libxmp/tree/mingw-snprintf):
  disables the usage of libxmp_snprintf and libxmp_vsnprintf in Windows for MSVC 2015+
  and for MinGW when using `__USE_MINGW_ANSI_STDIO`, which are both standard for MZX
  Windows builds.
  Pending merge upstream: https://github.com/cmatsuoka/libxmp/pull/175
* [add-libxmp-no-depackers](https://github.com/AliceLR/libxmp/tree/add-libxmp-no-depackers):
  adds a define that allows the depackers to be disabled entirely. MegaZeux uses
  this since depackers have never been supported by MegaZeux, aren't really
  needed for MegaZeux games, and removing them reduces the executable size.
  Pending merge upstream: https://github.com/cmatsuoka/libxmp/pull/172
* [mzx-integration-hacks](https://github.com/AliceLR/libxmp/tree/mzx-integration-hacks):
  adds some defines to the start of xmp.h to hack various things to act
  the way MZX would prefer, since this copy of libxmp needs to build static
  against MZX in the MZX build system.
* [mzx-remove-extra-formats](https://github.com/AliceLR/libxmp/tree/mzx-remove-extra-formats):
  removes some of the more obscure formats supported by libxmp that were not
  likely to have been used. This means effectively everything that wasn't
  supported by the MegaZeux fork of libmodplug except for things that could
  be mistaken for other formats (misc. formats for MOD, MED derivatives for MED).
  This keeps the MZX supported formats list roughly the same between libmodplug,
  mikmod, and libxmp and helps reduce executable size for embedded platforms.
* [fix-xmp-load-module-from-file](https://github.com/AliceLR/libxmp/tree/fix-xmp-load-module-from-file):
  removes erroneous usage of fdopen that was causing portability issues.
  Pending merge upstream: https://github.com/cmatsuoka/libxmp/pull/173
* [fix-xmp-set-position-jump](https://github.com/AliceLR/libxmp/tree/fix-xmp-set-position-jump):
  makes sure some variables are reset after xmp_set_position() is called.
  This was causing bugs with some MegaZeux games.
  Pending merge upstream: https://github.com/cmatsuoka/libxmp/pull/169
* [fix-it-sample-volume-vibrato](https://github.com/AliceLR/libxmp/tree/fix-it-sample-volume-vibrato):
  tweaks the way sample volume and vibrato are handled for IT files.
  This has not been contributed upstream because I'm not convinced this is the
  "correct" fix yet, despite it seeming to fix these issues on the surface level.
  Relevant upstream issue(s):
  https://github.com/cmatsuoka/libxmp/issues/102
  https://github.com/cmatsuoka/libxmp/issues/103
* [fix-s3m-adpcm](https://github.com/AliceLR/libxmp/tree/fix-s3m-adpcm):
  adds Mod Plugin ADPCM4 support for S3M files.
  Pending merge upstream: https://github.com/cmatsuoka/libxmp/pull/163
* [fix-gdm-fine-effects](https://github.com/AliceLR/libxmp/tree/fix-gdm-fine-effects):
  fixes continue for the fine volume slide and portamento effects for GDM files.
  Pending merge upstream: https://github.com/cmatsuoka/libxmp/pull/165

This patch automatically copies only the files required by MegaZeux, so manual file
removal should no longer be necessary.

See README and docs/COPYING.LIB for more libxmp information and the libxmp license.
See the upstream repository for the original libxmp, which you should probably use
instead of this hacked up copy for most purposes: https://github.com/cmatsuoka/libxmp/

## Pending MZX-specific hacks

* asie contributed a patch for tracker detection removal in the S3M and IT loaders.
  This is not currently used by MZX and could save about 2-3kb RAM. This might be
  worth looking into someday. https://github.com/AliceLR/megazeux/pull/202
* There are also IceTracker/ST2.6 and Archimedes cleanup patches attached to that
  which saved far less RAM. If something ends up desperate for 1kb of RAM this could
  be looked at too.
