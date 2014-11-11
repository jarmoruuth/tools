/************************************************************************\
 *
 *	CLIB.C
 *
 * This file defines some ANSI C and SYSTEM V compatible library
 * routines for BSD UNIX, input buffer allocating and some system
 * dependent routines.
 * (WARNING! This file is a real mess filled with #if's, so don't look 
 * here if you don't have to.)
\************************************************************************/

#include "system.h"

unsigned	maxbuf;
uchar*		buffer;

/**********************************************************************
 *
 *	d_free
 */
void d_free(void* ptr)
{
	REG1 void** p = (void**)ptr;

	if (*p != NULL) {
		free(*p);
		*p = NULL;
	}
}

/**********************************************************************
 *
 *	alloc_buffer
 *
 * Allocate I/O-buffer.
 */
void alloc_buffer(void)
#if (defined(__TURBOC__) || defined(M_I86)) && !defined(__TSC__) && !defined(OS2)
{
	buffer = sbrk(0);
	if ((int)buffer == -1)
		error("Input buffer allocation failed", 3);

	/* Calculate largest input buffer that doesn't cause pointer
	   wraparound in boyer-moore. */
	maxbuf = ((unsigned)(-((int)buffer)) >> 1) - MAXREGMUST;

	/* Starting from maxbuf, try to allocate input buffer. If allocation
	   fails, cut the buffer size to half until we reach minimum
	   buffer size. */
	while (maxbuf >= 2*MAXREGMUST) {
		if (buffer == sbrk(maxbuf + 1))
			return;
		maxbuf /= 2;
	}
	error("Out of memory when allocating input buffer", 3);

}
#else
{
	maxbuf = MAXBUF;
	buffer = malloc(MAXBUF);
	if (buffer == NULL)
		error("Out of memory when allocating input buffer", 3);
}
#endif	/* defined(__TURBOC__) || defined(M_I86) */

/**********************************************************************
 *
 *	error
 */
void error(char* errmsg, int exit_value)
{
	fprintf(stderr, "Error: %s\n", errmsg);
	exit(exit_value);
}

#if defined(TOUCH) && defined(__TURBOC__)

#include <fcntl.h>

/**********************************************************************
 *
 *	utime
 *
 * Turbo C (at least up to versions 2.0) lacks utime.
 */
int utime(char* path, utime_t* stamp)
{
	struct ftime	ft;
	utime_t		localstamp;
	struct tm*	stamp_tm;
	int		handle;
	int		retcode;

	if (stamp == NULL) {
		localstamp.actime = localstamp.modtime = time(NULL);
		stamp = &localstamp;
	}
	stamp_tm = localtime(&stamp->modtime);
	
	ft.ft_tsec = stamp_tm->tm_sec;
	ft.ft_min = stamp_tm->tm_min;
	ft.ft_hour = stamp_tm->tm_hour;
	ft.ft_day = stamp_tm->tm_mday;
	ft.ft_month = stamp_tm->tm_mon + 1;
	ft.ft_year = stamp_tm->tm_year - 80;
	
	handle = open(path, O_RDWR);
	
	if (handle == -1)
		return -1;

	retcode = setftime(handle, &ft);
	
	close(handle);
	
	return retcode;
}
#endif

#ifdef BSD	/* BSD UNIX lacks mem(set|chr) */
/**********************************************************************
 *
 *	memset
 */
void* memset(void* b, REG3 char ch, REG2 unsigned n)
{
	REG1 uchar* s = (uchar*)b;
	
	while (n--)
		*s++ = ch;
	return b;
}

/************************************************************************
 *
 *	memchr
 */
void* memchr(void* b, REG3 char ch, REG2 unsigned n)
{
	REG1 uchar* s = (uchar*)b;
	
	while (n--)
		if (*s++ == ch)
			return s - 1;
	return NULL;
}
#endif /* BSD */
