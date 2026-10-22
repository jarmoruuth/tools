/**********************************************************************\
 *
 *	DFAREGEX.H
 *
 * Definitions for deterministic finite automata regular expression
 * matcher.
 *
 * A compiled expression is kept in a regbuf_t, allocated with
 * reg_alloc. Compiling is not reentrant (it uses static variables), so
 * compile in one thread at a time. A compiled regbuf_t can be used by
 * one thread at a time; give each thread its own.
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#ifndef REGBUF_T_DEFINED
#define REGBUF_T_DEFINED
typedef struct regbuf_st regbuf_t;
#endif

/* Type bit definitions */

#define RE_SEARCH	0x1	/* builds substring searcher */
#define RE_IGNORECASE	0x2	/* case insensitive match */
#define RE_FULL		0x4	/* tries to get longest possible match */

regbuf_t* reg_alloc(void);
void reg_free(regbuf_t* rb);
char* reg_comp(regbuf_t* rb, char* regexp, int type_bits);
char* reg_comp_eol(regbuf_t* rb, char* regexp, char* eol, int neol,
		   int type_bits);
char* reg_exec(regbuf_t* rb, char* strbeg, char* strend, unsigned long* linecnt);
