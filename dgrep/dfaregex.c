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

#ifdef TEST
extern int debug;
#endif

/**********************************************************************
 *
 *	free_rbufmem
 *
 * Releases memory held by regular expression.
 */
static void free_rbufmem(REG2 regbuf_t* rb)
{
	REG1 int	i;

	for (i = 0; i < NTRAN; i++) {
		if (rb->dstates[i] != NULL)
			d_free(&rb->dstates[i]->set);
		d_free(&rb->dstates[i]);
	}
	if (rb->follow_array != NULL) {
		for (i = 0; i <= rb->root; i++)
			d_free(&rb->follow_array[i]);
		d_free(&rb->follow_array);
	}
	if (rb->tree != NULL) {
		for (i = 0; i <= rb->root; i++) {
			if (rb->tree[i].type == CLASS_ID)
				d_free(&rb->tree[i].val.class);
		}
		d_free(&rb->tree);
	}
	d_free(&rb->regmust);
	d_free(&rb->language);
	d_free(&rb->tmpstate);
	d_free(&rb->eol);
	rb->mem_allocated = FALSE;
}

/**********************************************************************
 *
 *	reg_alloc
 *
 * Allocates an empty regular expression buffer.
 */
regbuf_t* reg_alloc(void)
{
	regbuf_t*	rb;

	if ((rb = calloc(1, sizeof(regbuf_t))) == NULL)
		error("Out of memory", 3);
	return rb;
}

/**********************************************************************
 *
 *	reg_free
 *
 * Releases regular expression buffer and its memory.
 */
void reg_free(regbuf_t* rb)
{
	if (rb != NULL) {
		free_rbufmem(rb);
		free(rb);
	}
}

/**********************************************************************
 *
 *	reg_comp_eol
 *
 * Compiles regular expression. It is possible to specify end of line
 * characters.
 */
char* reg_comp_eol(REG2 regbuf_t* rb, REG1 char* regexp, char* eol, int neol,
		   int type_bits)
{
	if (regexp == NULL)
		return "No regular expression";
	free_rbufmem(rb);	/* previous expression or failed compile */
	rb->reg_error = NULL;
	rb->ignorecase = type_bits & RE_IGNORECASE;
	rb->search = type_bits & RE_SEARCH;
	rb->full = type_bits & RE_FULL;
	if ((rb->eol = malloc(neol)) == NULL)
		return "Out of memory";
	rb->neol = neol;
	memcpy(rb->eol, eol, neol);
	if (create_tree(rb, (uchar*)regexp) == ERROR)
		return rb->reg_error;
	if (!find_regmust(rb)) {
		free_rbufmem(rb);
		return rb->reg_error = "Out of memory when allocating regmust";
	}
#ifdef TEST
	if (debug)
		printf("Regmust = '%s'\n", rb->regmust);
#endif
	if (!calcpos(rb)) {
		free_rbufmem(rb);
		return rb->reg_error = "Out of memory in calcpos";
	}
	if (!dfa(rb)) {
		free_rbufmem(rb);
		return rb->reg_error = "Out of memory when creating dfa";
	}
	rb->mem_allocated = TRUE;
#ifdef TEST
	if (debug > 1) {
		print_accepting_states(rb);
		print_dtran(rb);
	}
#endif
	return rb->reg_error;
}

/**********************************************************************
 *
 *	reg_comp
 *
 * Compiles regular expression with predefined end of line character
 * suitable for C's null-terminating string representation.
 */
char* reg_comp(regbuf_t* rb, char* regexp, int type_bits)
{
	char c = '\0';

	return reg_comp_eol(rb, regexp, &c, 1, type_bits);
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
char* reg_exec(regbuf_t* rb, char* strbeg, REG5 char* strend, REG6 ulong* linecnt)
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
	
	s = (uchar*)strbeg;
	if (s == NULL || strend == NULL)
		return NULL;
	if (rb->begline) {
check_bol:
		/* we need separate beginning-of-line check, because
		   there is no real character for that position */
		prev_state = FIRST_STATE;
		cur_state = rb->dstates[0]->tran[BOL];
		if (cur_state == UNKNOWN)
			cur_state = get_transition(rb, FIRST_STATE, BOL);
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
			cur_state = get_transition(rb, prev_state, *(s-1));
		if (cur_state == END_OF_LINE) {
			cur_state = state_of(prev_state)->tran[EOL];
			if (cur_state == ACCEPTING)
				break;	/* return match */
			if (linecnt)
				(*linecnt)++;
			/* find eol character position in array of eol
			   characters */
			for (i = 0; i < rb->neol; i++)
				if (s[-1] == rb->eol[i])
					break;
			/* skip all following eol characters that are in the
			   same order as in the array of eol characters */
			for (i++; i < rb->neol && *s == rb->eol[i]; i++, s++)
				;
			if (s > (uchar*)strend || cur_state == STOPSTATE)
				break;	/* return NULL */
			if (rb->begline)
				goto check_bol;
			cur_state = FIRST_STATE;
		}
	} while (cur_state < MAXTRAN);
	return cur_state == ACCEPTING ? (char*)s-1 : NULL;
}
