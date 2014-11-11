/**********************************************************************\
 *
 *	DFAREGEX.H
 *
 * Definitions for deterministic finite automata regular expression
 * matcher.
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

/* Type bit definitions */

#define RE_SEARCH	0x1	/* builds substring searcher */
#define RE_IGNORECASE	0x2	/* case insensitive match */
#define RE_FULL		0x4	/* tries to get longest possible match */

char* reg_comp(char* regexp, int type_bits);
char* reg_comp_eol(char* regexp, char* eol, int neol, int type_bits);
char* reg_exec(char* strbeg, char* strend, unsigned long* linecnt);
