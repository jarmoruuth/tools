/**********************************************************************\
 *
 *	SYSTEM.H
 *
 * System dependencies and configurations for dgrep.
 *
 * The platform is detected automatically: _WIN32 selects Windows,
 * everything else is treated as POSIX (Linux, BSD, macOS, ...).
 *
 * Conditional compilation:
 *	TEST    - Includes routines to display some internal structures
 *		  and defines debug flag 'debug' which controls debug
 *		  displaying. Debug can be set to TRUE with -D option.
 *	NOFAST	- Uses state indexes instead of state pointers in the dfa
 *		  transition tables. Uses less memory, but is slower.
 *	MAXBUF	- Size of the input buffer, default 24 MB. A tiny buffer
 *		  is useful for testing buffer boundary handling.
 *
 * Author: Jarmo Ruuth 17-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32

#include <io.h>
#include <direct.h>
#include <sys/utime.h>

#define EOL1		'\r'
#define EOL2		'\n'
#define NEOL(s)		(((s)[0] == EOL1 && (s)[1] == EOL2) ? 2 : 1)
#define IS_EOL(c)	((c) == EOL1 || (c) == EOL2)
#define PATHSEP		'\\'
#define IS_ABSPATH(p)	((p)[0] == '\\' || (p)[0] == '/' || \
			 ((p)[0] != '\0' && (p)[1] == ':'))

#else /* POSIX */

#define UNIX
#include <unistd.h>
#include <utime.h>

#define EOL1		'\n'
#define EOL2		'\n'
#define NEOL(s)		1
#define IS_EOL(c)	((c) == EOL1)
#define PATHSEP		'/'
#define IS_ABSPATH(p)	((p)[0] == '/')

#endif /* _WIN32 */

#ifndef O_BINARY
#define O_BINARY	0
#endif

#define ALIGN		4096

#ifndef NOFAST
#define FAST
#endif

/*
	Character classification and case conversion. Only ASCII letters
	have case, so that bytes of multibyte (UTF-8) characters are
	never changed. The macros accept any int, also the BOL, EOL and
	REM pseudo characters that are outside of the char range.
*/
#define IS_UPPER(c)	((unsigned)(c) - 'A' < 26)
#define IS_LOWER(c)	((unsigned)(c) - 'a' < 26)
#define IS_ALPHA(c)	(IS_UPPER(c) || IS_LOWER(c))
#define TO_UPPER(c)	(IS_LOWER(c) ? (c) - 'a' + 'A' : (c))
#define CHCASE(c)	(IS_ALPHA(c) ? ((c) ^ ('a' ^ 'A')) : (c))

#ifndef max
#define max(x,y)	((x) > (y) ? (x) : (y))
#endif

#ifndef min
#define min(x,y)	((x) < (y) ? (x) : (y))
#endif

#define REG1	register
#define REG2	register
#define REG3	register
#define REG4	register
#define REG5	register
#define REG6	register

#define CHARBITS	8		/* hardware character bits */
#define NCHARS		(1 << CHARBITS)
#define MAXCHAR		(NCHARS-1)

#ifndef MAXBUF
#define MAXBUF		(1024*1024*24)
#endif
#define MAXREGMUST	MAXCHAR

#define ERROR		-1

/* common typedefs */
typedef unsigned char	uchar;
typedef unsigned int	uint;
typedef unsigned long	ulong;

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

extern void  d_free(void* ptr);
extern void  error(char* errmsg, int exit_value);
extern void  alloc_buffer(void);

extern uchar*	buffer;
extern unsigned	maxbuf;

#ifdef TEST
#define dassert	assert
#else
#define dassert(exp)
#endif
