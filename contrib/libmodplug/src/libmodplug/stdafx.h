/*
 * This source code is public domain.
 *
 * Authors: Rani Assaf <rani@magic.metawire.com>,
 *          Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/

#ifndef _STDAFX_H_
#define _STDAFX_H_

#ifdef _MSC_VER

#pragma warning (disable:4201)
#pragma warning (disable:4514)
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <stdio.h>

#include "msvc.h"

static inline void ProcessPlugins(int n) {}

#define sleep(x) Sleep(x * 1000)
#define strnicmp _strnicmp

#else

#if defined(HAVE_CONFIG_H) && !defined(CONFIG_H_INCLUDED)
# include "config.h"
# define CONFIG_H_INCLUDED 1
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __WIN32__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define sleep(x) Sleep(x * 1000)

#else // !__WIN32__

#include <unistd.h> // for sleep()

#ifdef __amigaos__

/* On AmigaOS platforms we work around the fact that the toolchain headers
 * pollute the namespace with many MSVC-like type declarations. Using their
 * versions directly is not possible, as some are just plain wrong. WORD, for
 * example, is implicitly signed (not unsigned) as with the BYTE type. All
 * other toolchains do this differently, which ModPlug depends on internally.
 */
#define ULONG _ULONG
#define LONG _LONG
#define WORD _WORD
#define BYTE _BYTE
#define BOOL _BOOL

#else // !__amigaos__

/* On non-AmigaOS platforms we also define VOID; on AmigaOS we can
 * trust the VOID declaration and the same _VOID trick doesn't work.
 */
typedef void VOID;

#endif // __amigaos__

typedef int8_t CHAR;
typedef uint8_t UCHAR;
typedef uint8_t* PUCHAR;
typedef uint16_t USHORT;
typedef uint32_t ULONG;
typedef uint32_t UINT;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int64_t LONGLONG;
typedef int32_t* LPLONG;
typedef uint32_t* LPDWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef uint8_t* LPBYTE;
typedef bool BOOL;
typedef char* LPSTR;
typedef void* LPVOID;
typedef uint16_t* LPWORD;
typedef const char* LPCSTR;
typedef void* PVOID;

#define lstrcpynA	strncpy
#define lstrcpyA	strcpy
#define lstrcmpA	strcmp
#define wsprintfA	sprintf

#ifndef strnicmp
#define strnicmp(a,b,c)	strncasecmp(a,b,c)
#endif

#define GHND		0

#endif // !__WIN32__

inline LONG MulDiv (long a, long b, long c)
{
  // if (!c) return 0;
  return ((uint64_t) a * (uint64_t) b ) / c;
}

#define MODPLUG_NO_FILESAVE
#define NO_AGC
#define LPCTSTR LPCSTR
#define WAVE_FORMAT_PCM 1
//#define ENABLE_EQ

inline int8_t * GlobalAllocPtr(unsigned int, size_t size)
{
  int8_t * p = (int8_t *) malloc(size);

  if (p != NULL) memset(p, 0, size);
  return p;
}

inline void ProcessPlugins(int n) {}

#define GlobalFreePtr(p) free((void *)(p))

#ifndef FALSE
#define FALSE	false
#endif

#ifndef TRUE
#define TRUE	true
#endif

#endif // MSC_VER

#endif
