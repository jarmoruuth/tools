/************************************************************************\
 *
 *	CLIB.C
 *
 * Input buffer allocating and some common routines.
\************************************************************************/

#include "system.h"

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
 * maxbuf bytes, for an EOL or '\0' character.
 */
uchar* alloc_buffer(void)
{
	uchar*	buffer = malloc(maxbuf + 1);

	if (buffer == NULL)
		error("Out of memory when allocating input buffer", 3);
	return buffer;
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
