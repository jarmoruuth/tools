/**********************************************************************\
 *
 *	ASSERT.C
 *
 * Own assert exit routine to avoid linking with stream I/O and fprintf.
 *
\**********************************************************************/

#include <dos.h>
#include <stdlib.h>

#include "lassert.h"

/**********************************************************************
 *	putch
 */
#define putch(c) \
{ \
	_DL = (c); \
	_AH = 2; \
	geninterrupt(0x21); \
}

/**********************************************************************
 *	putstr
 */
static void putstr(register char* str)
{
	register int c;
	
	while ((c = *str++) != '\0')
		putch(c);
}

/**********************************************************************
 *	assertion_failure
 */
void assertion_failure(char* file, int line)
{
	char buf[10];

	putstr("Assertion failure: ");
	putstr(file);
	putstr(", ");
	putstr(itoa(line, buf, 10));
	putstr("\r\n");
	exit(3);
}
