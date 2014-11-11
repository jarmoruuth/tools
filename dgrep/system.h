/**********************************************************************\
 *
 *	SYSTEM.H
 *
 * System dependencies and configurations for dgrep.
 *
 * Conditional compilation:
 *	ECTYPE  - Includes extended ctype instead of original.
 *	UNIX    - Uses '\n' as end of line character, otherwise uses
 *		  '\r\n'-pair.
 *	BSD     - Defines from SysV library to Bsd, defines also UNIX.
 *      QNX     - Definitions for QNX operating system.
 *	TEST    - Includes routines to display some internal structures
 *		  and defines debug flag 'debug' which controls debug
 *		  displaying. Debug can be set to TRUE with -D option.
 *	MANYREG	- Uses more than two register variables. Defined
 *		  automatically with UNIX and BSD.
 *	TOUCH	- Touches all files that contain matches. Only for Turbo C
 *		  and systems, that contain time(3) and utime(3). Utime(3)
 *		  is supposed to be in every UNIX-system and in QNX.
 *	FAST	- Makes faster dfa but uses more memory. This doesn't really
 *		  affect to the state machine but to the way how the
 *		  transitions are stored internally.
 *
 * Author: Jarmo Ruuth 17-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <time.h>

#ifdef ECTYPE

#include "ectype.h"
#define	CHCASE(c)	_diffcase[(unsigned char)(c)]

#else

#include <ctype.h>
#define	CHCASE(c)	(isalpha(c) ? ((c) ^ ('a' ^ 'A')) : (c))

#endif	/* ECTYPE */

#ifdef BSD

#define UNIX 
#define memcpy(dest,src,len)	bcopy(src,dest,len)
#define memcmp(s1,s2,len)	bcmp(s1,s2,len)
extern void* memset(void* b, char ch, unsigned n);
extern void* memchr(void* b, char ch, unsigned n);
#include <strings.h>

#else

#include <string.h>
#include <process.h>
#include <io.h>
#include <stdlib.h>

#endif	/* BSD */

#ifdef __TURBOC__

#define TOUCH
#include <alloc.h>

#endif	/* TURBOC */

#if defined(M_I86) && !defined(__TSC__)	/* MSC and not TopSpeed C */

#define TOUCH
#include <malloc.h>
#include <sys\types.h>
#include <sys\utime.h>
typedef struct utimbuf	utime_t;

#endif	/* MSC */

#if defined(UNIX)

#define O_BINARY        0
#define EOL1		'\n'
#define EOL2		'\n'
#define NEOL(s)		1
#define IS_EOL(c)	((c) == EOL1)
#define ALIGN		4096
#define MANYREG
#define TOUCH

#include <sys/types.h>
#include <time.h>

#elif defined(QNX)

#include <sys/types.h>
#include <time.h>
#include <sys/utime.h>

typedef struct utimbuf utime_t;
#define EOL1		'\n'	/* Actually '\n' is ASCII RS (not LF) */
#define EOL2		'\n'
#define NEOL(s)		1
#define IS_EOL(c)	((c) == EOL1)
#define ALIGN		512	/* QNX fsys uses 512 byte clusters */
#define TOUCH

#else

#define EOL1		'\r'
#define EOL2		'\n'
#define NEOL(s)		(((s)[0] == EOL1 && (s)[1] == EOL2) ? 2 : 1)
#define IS_EOL(c)	((c) == EOL1 || (c) == EOL2)
#define ALIGN		2048

#endif	/* UNIX || QNX || other */


#if defined(TOUCH) && (defined(UNIX) || defined(__TURBOC__))

/* define utime() parameter in MSC style */
typedef struct {
	time_t actime;      /* access time */
	time_t modtime;     /* modification time */
} utime_t;

#ifdef __TURBOC__
int utime(char* path, utime_t* stamp);
#endif

#endif	/* TOUCH and UNIX or TURBOC */

#ifndef max
#define max(x,y)	((x) > (y) ? (x) : (y))
#endif

#ifndef min
#define min(x,y)	((x) < (y) ? (x) : (y))
#endif

#define REG1	register
#define REG2	register

#ifdef MANYREG

#define REG3	register
#define REG4	register
#define REG5	register
#define REG6	register

#else

#define REG3
#define REG4
#define REG5
#define REG6

#endif

#define CHARBITS	8		/* hardware character bits */
#define NCHARS		(1 << CHARBITS)
#define MAXCHAR		(NCHARS-1)

#define MAXBUF		(1024*1024*24)
#define MAXREGMUST	MAXCHAR

#define ERROR		-1

/* common typedefs */
typedef unsigned char	uchar;
typedef unsigned int	uint;
typedef unsigned long	ulong;

typedef enum {
	FALSE = 0,
	TRUE = 1
} bool;

#ifndef MSC
extern void* malloc(unsigned nbytes);
extern void* calloc(unsigned nelem, unsigned elsize);
extern void* realloc(void* p, unsigned newsize);
extern void  free(void* p);
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
