/**********************************************************************\
 *
 *	DFAREGEX.C
 *
 * Deterministic finite automata regular expression matcher.
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include "system.h"
#include "dfa.h"
#include "dfaregex.h"

/*
	Regular expression buffer. It is defined statically here, so
	it is not possible to compile and use several regexps at the
	same time. The reason for the static buffer is that it makes
	things slightly faster on the PC.
*/
regbuf_t 	rbuf;

#ifdef TEST
extern int debug;
#endif

/**********************************************************************
 *
 *	free_rbufmem
 *
 * Releases memory held by regular expression.
 */
static void free_rbufmem(void)
{
	REG1 int	i;
	
	for (i = 0; i < NTRAN; i++) {
		d_free(&rbuf.dstates[i]->set);
		d_free(&rbuf.dstates[i]);
	}
	if (rbuf.follow_array != NULL) {
		for (i = 0; i <= rbuf.root; i++)
			d_free(&rbuf.follow_array[i]);
		d_free(&rbuf.follow_array);
	}
	if (rbuf.tree != NULL) {
		for (i = 0; i <= rbuf.root; i++) {
			if (rbuf.tree[i].type == CLASS_ID)
				d_free(&rbuf.tree[i].val.class);
		}
		d_free(&rbuf.tree);
	}
	d_free(&rbuf.regmust);
	d_free(&rbuf.language);
	d_free(&rbuf.tmpstate);
	d_free(&rbuf.eol);
	rbuf.mem_allocated = FALSE;
}

/**********************************************************************
 *
 *	reg_comp_eol
 *
 * Compiles regular expression. It is possible to specify end of line
 * characters.
 */
char* reg_comp_eol(REG1 char* regexp, char* eol, int neol, int type_bits)
{
	if (regexp == NULL)
		return "No regular expression";
	rbuf.reg_error = NULL;
	rbuf.ignorecase = type_bits & RE_IGNORECASE;
	rbuf.search = type_bits & RE_SEARCH;
	rbuf.full = type_bits & RE_FULL;
	if (rbuf.mem_allocated)
		free_rbufmem();
	if ((rbuf.eol = malloc(neol)) == NULL)
		return "Out of memory";
	rbuf.neol = neol;
	memcpy(rbuf.eol, eol, neol);
	if (create_tree(regexp) == ERROR)
		return rbuf.reg_error;
	if (!find_regmust(rbuf.tree, rbuf.root)) {
		free_rbufmem();
		return rbuf.reg_error = "Out of memory when allocating regmust";
	}
#ifdef TEST
	if (debug)
		printf("Regmust = '%s'\n", rbuf.regmust);
#endif
	if (!calcpos()) {
		free_rbufmem();
		return rbuf.reg_error = "Out of memory in calcpos";
	}
	if (!dfa()) {
		free_rbufmem();
		return rbuf.reg_error = "Out of memory when creating dfa";
	}
	rbuf.mem_allocated = TRUE;
#ifdef TEST
	if (debug > 1) {
		print_accepting_states();
		print_dtran();
	}
#endif
	return rbuf.reg_error;
}

/**********************************************************************
 *
 *	reg_comp
 *
 * Compiles regular expression with predefined end of line character
 * suitable for C's null-terminating string representation.
 */
char* reg_comp(char* regexp, int type_bits)
{
	char c = '\0';
	
	return reg_comp_eol(regexp, &c, 1, type_bits);
}

/**********************************************************************
 *
 *	reg_exec
 *
 * Executes regular expression to the string 's'. Return value is NULL,
 * if no match was found and pointer to the last matching char, if match 
 * was found. Note that last matching char can be end of line character.
 *
 * Parameters:
 * 	Strbeg points to the beginning of the line.
 *	Strend points to the first character after the string. *strend should
 *		contain an end of line character.
 *	Linecnt points to the counter, that we increment every time we see
 *		an end of line character, if it does not belong to the matched
 *		line. If linecnt is NULL, it is not incremented.
 */
char* reg_exec(char* strbeg, REG5 char* strend, REG6 ulong* linecnt)
{
	REG1 uchar*     s;
#ifdef FAST
	REG2 tran_t	cur_state = FIRST_STATE;
	REG3 tran_t	prev_state;
#else
	/* use unsigned state variables, because some compilers can't
	   put char variables to registers (usually tran_t is char) */
	REG2 unsigned	cur_state = FIRST_STATE;
	REG3 unsigned	prev_state;
#endif
	REG4 int	i;
	
	s = strbeg;
	if (s == NULL || strend == NULL)
		return NULL;
	if (rbuf.begline) {
check_bol:
		/* we need separate beginning-of-line check, because
		   there is no real character for that position */
		prev_state = FIRST_STATE;
		cur_state = rbuf.dstates[0]->tran[BOL];
		if (cur_state == UNKNOWN)
			cur_state = get_transition(FIRST_STATE, BOL);
		if (cur_state == STOPSTATE)
			cur_state = FIRST_STATE;  /* we might match later */
		else if (cur_state == ACCEPTING)
			s++;	/* now we return correct pointer */
	}
	do {
		/* the inner loop */
		for (; cur_state < MAXTRAN; ) {
			prev_state = cur_state;
			cur_state = state_of(cur_state)->tran[*s++];
		}
		if (cur_state == UNKNOWN)
			cur_state = get_transition(prev_state, *(s-1));
		if (cur_state == END_OF_LINE) {
			cur_state = state_of(prev_state)->tran[EOL];
			if (cur_state == ACCEPTING)
				break;	/* return match */
			if (linecnt)
				(*linecnt)++;
			/* find eol character position in array of eol
			   characters */
			for (i = 0; i < rbuf.neol; i++)
				if (s[-1] == rbuf.eol[i])
					break;
			/* skip all following eol characters that are in the
			   same order as in the array of eol characters */
			for (i++; i < rbuf.neol && *s == rbuf.eol[i]; i++, s++)
				;
			if (s > strend || cur_state == STOPSTATE)
				break;	/* return NULL */
			if (rbuf.begline)
				goto check_bol;
			cur_state = FIRST_STATE;
		}
	} while (cur_state < MAXTRAN);
	return cur_state == ACCEPTING ? s-1 : NULL;
}
