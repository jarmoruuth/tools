/************************************************************************\
 *
 *	CLIB.C
 *
 * Input buffer allocating and some common routines.
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
{
	maxbuf = MAXBUF;
	buffer = malloc(MAXBUF);
	if (buffer == NULL)
		error("Out of memory when allocating input buffer", 3);
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
