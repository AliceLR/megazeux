#
# Makefile compiler compatibility checks.
# Define flags for optional compiler flags or settings to keep most
# of the compiler version check jank out of the main Makefile.
#

IS_GCC   := ${shell ${CC} --version | grep -qi "gcc"; echo $$?}
IS_CLANG := ${shell ${CC} --version | grep -qi "clang version"; echo $$?}

GCC_VER := ${shell ${CC} -dumpfullversion -dumpversion |\
 awk -F. '{print $$3+100*($$2+100*$$1)}'}

GCC_VER_GE_4     := ${shell test ${GCC_VER} -ge 40000; echo $$?}
GCC_VER_GT_4_2_1 := ${shell test ${GCC_VER} -gt 40201; echo $$?}
GCC_VER_GE_4_8   := ${shell test ${GCC_VER} -ge 40800; echo $$?}
GCC_VER_GE_7     := ${shell test ${GCC_VER} -ge 70000; echo $$?}

ifeq (${IS_CLANG},0)
CLANG_VER := ${shell ${CC} --version |\
 grep -oi "^clang version.* [0-9]\+\.[0-9]\+\.[0-9]\+" |\
 grep -o "[0-9]\+.*" |\
 awk -F. '{print $$3+100*($$2+100*$$1)}'}

CLANG_VER_GE_3_3 := ${shell test "${CLANG_VER}" -ge 30300; echo $$?}
endif

#
# Features requiring GCC 4 minimum.
# NOTE: most of these haven't been verified; this is how they were originally
# version checked in the Makefile.
#
ifeq (${GCC_VER_GE_4},0)
HAS_PEDANTIC = 1
HAS_F_PERMISSIVE = 1
HAS_F_STACK_PROTECTOR = 1
HAS_F_VISIBILITY = 1
HAS_W_NO_VARIADIC_MACROS = 1
endif

#
# Features requiring higher versions of GCC.
#
ifneq (${IS_CLANG},0)

# Not actually sure if this is the correct version; the != 4.2.1 check that
# existed prior in the Makefile may have been intended to block clang.
ifeq (${GCC_VER_GT_4_2_1},0)
HAS_F_BOUNDS_CHECK = 1
endif

ifeq ($(GCC_VER_GE_7),0)
HAS_W_NO_FORMAT_TRUNCATION = 1
endif

endif

#
# C++11 support.
# C++11 is optional or unused for all core MegaZeux features, but may be
# required in the future for optional features (e.g. the network layer).
#
ifeq ($(IS_GCC),0)
ifeq ($(GCC_VER_GE_4_8),0)
HAS_CXX_11 = 1
endif
endif
ifeq ($(IS_CLANG),0)
ifeq ($(CLANG_VER_GE_3_3),0)
HAS_CXX_11 = 1
endif
endif
