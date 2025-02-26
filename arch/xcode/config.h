#include "version.h"
#define PLATFORM "xcode-" SUBPLATFORM
#define CONFDIR "../Resources"
#define CONFFILE "config.txt"
#define USERCONFFILE ".megazeux-config"
#define SHAREDIR "../Resources"
#define LICENSEDIR "../Resources"
#define CONFIG_SDL 2
#define CONFIG_EDITOR
#define CONFIG_HELPSYS
#define CONFIG_RENDER_SOFT
#define CONFIG_RENDER_SOFTSCALE
#define CONFIG_RENDER_GL_FIXED
#define CONFIG_RENDER_GL_PROGRAM
#define CONFIG_ENABLE_SCREENSHOTS
#define CONFIG_XMP
#define CONFIG_AUDIO
#define CONFIG_AUDIO_MOD_SYSTEM
#define CONFIG_REALITY
#define CONFIG_VORBIS
#define CONFIG_PNG
#define CONFIG_ICON
#define CONFIG_MODULAR
#define CONFIG_CHECK_ALLOC
#define CONFIG_COUNTER_HASH_TABLES
#define CONFIG_GAMECONTROLLERDB

/* TODO: do this from Xcode? */
#if defined(__x86_64h__)
#define SUBPLATFORM "x86_64h"
#elif defined(__x86_64__)
#define SUBPLATFORM "x86_64"
#elif defined(__x86__)
#define SUBPLATFORM "i386"
#elif defined(__arm64e__)
#define SUBPLATFORM "arm64e"
#elif defined(__arm64__)
#define SUBPLATFORM "arm64"
#else
#define SUBPLATFORM "unknown"
#endif
