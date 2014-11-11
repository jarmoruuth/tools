/**********************************************************************\
 *
 *	SET.C
 *
 * Set routines.
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include "system.h"
#include "set.h"

/**********************************************************************
 *
 *	set_empty
 */
bool set_empty(REG1 set_t* set, int setsize)
{
	REG2 int	i = setsize/sizeof(set_t) - 1;
	
	for (set += i; i >= 0; i--, set--)
		if (*set)
			return FALSE;
	return TRUE;
}

/**********************************************************************
 *
 *	set_union
 *
 * Union of two sets.
 */
void set_union(REG1 set_t* result, REG3 set_t* set1, REG4 set_t* set2,
	       int setsize)
{
	REG2 int	i = setsize/sizeof(set_t) - 1;
	
	result += i;
	set1 += i;
	set2 += i;
	for (; i >= 0; i--, result--, set1--, set2--)
		*result = *set1 | *set2;
}
