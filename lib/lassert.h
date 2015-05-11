/**********************************************************************\
 *
 *	LASSERT.H
 *
 * Replacement for assert.h. Only for internal use in library routines.
 *
\**********************************************************************/

#ifndef LASSERT_H
#define LASSERT_H

#include <stdio.h>
#include <stdlib.h>

static char _lassertstring_[] = "\nAssertion failure: file %s, line %d\n";

#define lerror  { printf(_lassertstring_, __FILE__, __LINE__); \
                  fflush(stdout); \
                  exit(3); \
                } \

#define lassert(exp) { if (!(exp)) lerror }

#endif /* LASSERT_H */
