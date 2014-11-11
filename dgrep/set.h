/**********************************************************************\
 *
 *	SET.H
 *
 * Interfaces to set routines and macro definitions.
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

typedef unsigned	set_t;

#define SETBITS		(sizeof(set_t) * CHARBITS)

/**********************************************************************
 *
 *	set_bytesize
 *
 * Returns number of bytes needed for given number of bits.
 */
#define set_bytesize(bitsize) \
	(((unsigned)(bitsize) / SETBITS + 1) * sizeof(set_t))

/**********************************************************************
 *
 *	in_set
 */
#define in_set(set, pos) \
	(((set)[(unsigned)(pos)/SETBITS])&(1<<((unsigned)(pos)%SETBITS)))

/**********************************************************************
 *
 *	add_set
 */
#define add_set(set, pos) \
	(((set)[(unsigned)(pos)/SETBITS])|=(1<<((unsigned)(pos)%SETBITS)))

/**********************************************************************
 *
 *	set_equal
 */
#define set_equal(set1, set2, setsize) (memcmp(set1, set2, setsize) == 0)

/**********************************************************************
 *
 *	set_copy
 *
 * Copies set to another.
 */
#define set_copy(dest, src, setsize) memcpy(dest, src, setsize)

/**********************************************************************
 *
 *	set_clear
 *
 * Clears set.
 */
#ifdef BSD
#define set_clear(set, setsize) bzero(set, setsize)
#else
#define set_clear(set, setsize) memset(set, 0, setsize)
#endif

extern bool set_empty(set_t* set, int setsize);
extern void set_union(set_t* result, set_t* set1, set_t* set2, int setsize);
