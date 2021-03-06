/**********************************************************************\
 *
 *	LRAND.C
 *
\**********************************************************************/

#include <c.h>

static ulong lseed;

/**********************************************************************
 *	lsrand
 */
void lsrand(ulong seed)
{
        lseed = seed;
}

/**********************************************************************
 *	lrand
 */
ulong lrand(void)
{
        lseed = (lseed * 31415821L + 1L) % 4000000000L;
        return(lseed);
}
