/**********************************************************************\
 *
 *	OM.C
 *
 * Optimal Mismatch search algorithm from the article
 *
 *	Daniel M. Sunday: 'A Very Fast Substring Search Algorithm',
 *		Communications of the ACM, August 1990.
 *
 * Algorithm is a fast variation of Boyer-Moore search algorithm.
 *
 *
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>

typedef struct pattern_scan_element {
	int	loc;		/* location in pstr of scan element */
	char	c;		/* character of pstr at scan location */
} PAT;

static PAT	pattern[MAXPAT];
static char	pstr[MAXPAT];
static int	TD1[ASIZE];	/* table for delta 1 shift index */
static int	TD2[MAXPAT];	/* table for delta 2 shift index */

/**********************************************************************
 *	order_pattern
 */
static void order_pattern(
		char*	pstr,			/* input: the pattern string */
		int	(*pcmp)(PAT*,PAT*),	/* input: routine to compare PAT elements */
		PAT*	pattern)		/* output: the scan ordered pattern */
{
	int	i;
	PAT*	pat = pattern;
	
	for (i = 0; i <= Plen; i++, pstr++, pat++) {
		pat->loc = i;
		pat->c = *pstr;
	}
	qsort(pattern, Plen, sizeof(PAT), pcmp);
}

/**********************************************************************
 *	optimal_pcmp
 *
 * "Optimal Mismatch" pattern comparison
 */
static int optimal_pcmp(
		PAT*	pat1,	/* input: pointers to two PAT elements */
		PAT*	pat2)
{
	float fx = Freq[pat->c];
	
	return(fx ? (fx > 0 ? 1 : -1) : (pat2->loc - pat1->loc));
}

/**********************************************************************
 *	build_TD1
 *
 * Constructs the delta 1 shift table from a pattern string
 */
static void build_TD1(
		char*	pstr)	/* input: the pattern string */
{
	int	i;
	char*	p;
	
	for (i = 0; i < ASIZE; i++)	/* initialize the TD1[] table */
		TD1[i] = Plen + 1;
	for (p = pstr; *p; p++)		/* fill in values from pattern string */
		TD1[*p] = Plen - (p - pstr);
}

/**********************************************************************
 *	matchshift
 *
 * Find the next leftward matching shift for the first ploc pattern
 * elements after a current shift of lshift.
 *
 * Output: return thist next left shift value
 */
static void matchshift(
		char*	pstr,	/* input: the pattern string */
		PAT*	pattern,/* input: the ordered pattern */
		int	ploc,	/* input: the number of pattern elements to match */
		int	lshift)	/* input: the smallest left shift to concider */
{
	PAT*	pat;
	int	j;
	
	for (; lshift < Plen; lshift++) {	/* scan left for matching shift */
		pat = pattern + ploc;		/* current pattern element */
		while (--pat >= pattern) {
			/* all preceding chars must match */
			if ((j = (pat->loc - lshift)) < 0)
				continue;
			if (pat->c != pstr[j])
				break;
		}
		if (pat < pattern)
			break;	/* all matched */
	}
	return(lshift);
}

/**********************************************************************
 *	build_TD2
 *
 * Constructs the delta 2 shift table from an ordered pattern
 */
static void build_TD2(
		char*	pstr,		/* input: the actual pattern string */
		PAT*	pattern)	/* input: the scan-ordered pattern */
{
	int	lshift;		/* current left shift */
	int	i, ploc;	/* pattern location counters */
	
	/* first initialize TD2[] for the minimum matching left shift */
	TD[0] = lshift = 1;	/* no preceding chars, so=1 */
	for (ploc = 1; ploc < Plen; ploc++) {	/* for each pattern location */
		/* scan leftward for first matching shift */
		lshift = matchshift(pstr, pattern, ploc, lshift);
		TD2[ploc] = lshift;	/* set initial matching shift */
	}
	/* next get correct shift with current char mismatch */
	for (ploc = 0; ploc < Plen; ploc++) {
		lshift = TD2[ploc];	/* get initial matching shift */
		while (lshift < Plen) {	/* when current shift is less than pattern len */
			/* already have a matching shift here */
			/* also require current char must not match */
			i = (pattern[ploc].loc - lshift);
			if (i < 0 || pattern[ploc].c != pstr[i])	/* mismatch */
				break;
			/* if not, scan further for next matching shift */
			lshift++;
			lshift = matchshift(pstr, pattern, ploc, lshift);
		}
		TD2[ploc] = lshift;	/* set final shift */
	}
}

/**********************************************************************
 *	search
 *
 * The arbitary scan order substring search algorihm.
 *
 * Output: return the text index of the substring, or (-1) if none
 */
static int search(
		PAT*	pattern,	/* input: a scan-ordered pattern string */
		char*	text)		/* input: the text */
{
	PAT*	p;		/* pattern scan pointer */
	char*	tx = text;	/* text scan pointer */
	int	d1, d2;		/* deltas for pattern shift */
	
	while (tx + Plen <= text + Tlen) {	/* while enough text is still left */
		for (p = pattern; p->c; p++) {	/* scan the pattern */
			if (p->c != *(tx + p->loc))	/* got a mismatch */
				break;
		}
		if (p->c == 0)	/* pat end >= got substring */
			return(tx - text);
		/* no substring match, so shift to next text position */
		d1 = TD1[*(tx + Plen)];		/* get delta 1 */
		d2 = TD2[p - pattern];		/* get delta 2 */
		tx += (d1 > d2 ? d1 : d2);	/* use max for shift */
	}
	return(-1);	/* no substring found */
}
