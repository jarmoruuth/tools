/************************************************************************\
 *
 *	CLIB.C
 *
 * Input buffer allocating and some common routines.
\************************************************************************/

#include "system.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI		/* wingdi.h defines ERROR */
#include <windows.h>
#endif

unsigned	maxbuf = MAXBUF;	/* input buffer size */

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
 * Allocates an I/O-buffer. There is room for one extra byte after
 * size bytes, for an EOL or '\0' character.
 */
uchar* alloc_buffer(unsigned size)
{
	uchar*	buffer = malloc(size + 1);

	if (buffer == NULL)
		error("Out of memory when allocating input buffer", 3);
	return buffer;
}

/**********************************************************************
 *
 *	cpu_count
 *
 * Returns the number of CPUs.
 */
int cpu_count(void)
{
#ifdef _WIN32
	SYSTEM_INFO	si;

	GetSystemInfo(&si);
	return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
#else
	long	n = sysconf(_SC_NPROCESSORS_ONLN);

	return n > 0 ? (int)n : 1;
#endif
}

/**********************************************************************
 *
 *	error
 */
void error(char* errmsg, int exit_value)
{
	fprintf(stderr, "Error: %s\n", errmsg);
	exit(exit_value);
}
