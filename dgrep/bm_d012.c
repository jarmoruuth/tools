/**********************************************************************
 *
 * Boyer-Moore algorithm separated from original egrep.c-file in
 * fastgrep. Here we won't do alternation, as in original version.
 *
 *	J.Ruuth 07-Feb-1988
 *
 * Delta2-table implemented as in original paper to guarantee O(m+n)-time 
 * worst case performance.
 *	J.Ruuth 10-Dec-88
\**********************************************************************/

/* Original comment from fastgrep:

     Hybrid Boyer/Moore/Gosper-assisted 'grep/egrep/fgrep' search, with delta0
     table as in original paper (CACM, October, 1977).  No delta1 or delta2.
     According to experiment (Horspool, Soft. Prac. Exp., 1982), delta2 is of
     minimal practical value.  However, to improve for worst case input,
     integrating the improved Galil strategies (Apostolico/Giancarlo, SIAM. J.
     Comput., Feb. 1986) deserves consideration.

     Algorithm amalgam summary:

		bmg			(boyer/moore/gosper)
		"superimposed" bmg   	(jaw)
		fgrep			(aho/corrasick)

		sorry, but the knuth/morris/pratt machine, horspool's
		"frequentist" code, and the rabin/karp matcher, however cute,
		just don't cut it for this production.

     James A. Woods				Copyright (c) 1986
     NASA Ames Research Center
*/

#include "system.h"
#include "dfa.h"
#include "bm.h"

struct bm_st {
	uchar*		pat;		/* pattern */
	int		patlen;		/* pattern length */
	/* large should be larger than or equal to strlen + patlen */
	unsigned	large;
	/* Boyer-Moore algorithm core */
	unsigned	delta0[NCHARS];
	unsigned	delta1[NCHARS];
	unsigned*	delta2;
	/* Character conversion table, in case sensitive search, gives
	   uppercase version of given character, otherwise just the same
	   character. */
	uchar		ccase[NCHARS];
};

#ifdef TEST
extern int	debug;
/* statistics, not thread safe */
static long inner_cnt, outer_cnt, cmp_cnt;

void show_boyer_moore_report(const bm_t* bm)
{
	if (debug > 1 && bm != NULL) {
		int i;
		for (i=0; i < bm->patlen; i++) {
			printf("delta2[%d] = %d\n", i, bm->delta2[i]);
		}
	}
	printf( "Boyer-Moore inner loop iterated "
		"%ld, outer %ld, comp.loop %ld times\n",
		inner_cnt, outer_cnt, cmp_cnt);
}
#endif

/**********************************************************************
 *	make_ccase
 */
static void make_ccase (bm_t* bm, int iflag)
{
	REG1 int	i = NCHARS;

	while (i--) {
		if (iflag && IS_LOWER(i))
			bm->ccase[i] = TO_UPPER(i);
		else
			bm->ccase[i] = i;
	}
}

/**********************************************************************
 *	build_delta2
 *
 * Builds delta2, the good suffix jump of Boyer and Moore, as the jump
 * of the text position at the mismatch. This used D.Knuth's linear
 * algorithm for dd', which is incorrect for some periodic patterns
 * (W.Rytter 1980); for example 40 a's were not found in a line of 45
 * a's. Now uses the linear suffixes algorithm from C.Charras and
 * T.Lecroq: Handbook of Exact String Matching Algorithms.
 */
static void build_delta2 (bm_t* bm, REG4 char *pat, int patlen)
{
	REG1 int	i;
	REG2 int	j;
	REG3 int	*suff;
	unsigned*	delta2;
	int		m = patlen;
	int		f = 0;
	int		g;

	delta2 = bm->delta2 = malloc((unsigned)m * sizeof(unsigned));
	if (delta2 == NULL)
		error("Out of memory", 3);
	suff = malloc(m * sizeof(int));
	if (suff == NULL)
		error("Out of memory", 3);
	/* suff[i] is the length of the longest suffix of pat[0..i]
	   that is also a suffix of pat */
	suff[m-1] = m;
	g = m - 1;
	for (i = m - 2; i >= 0; --i) {
		if (i > g && suff[i + m - 1 - f] < i - g)
			suff[i] = suff[i + m - 1 - f];
		else {
			if (i < g)
				g = i;
			f = i;
			while (g >= 0 && pat[g] == pat[g + m - 1 - f])
				--g;
			suff[i] = f - g;
		}
	}
	/* pattern shift when mismatch is at position j */
	for (j = 0; j < m; ++j)
		delta2[j] = m;
	j = 0;
	for (i = m - 1; i >= 0; --i)
		if (suff[i] == i + 1)
			for (; j < m - 1 - i; ++j)
				if (delta2[j] == (unsigned)m)
					delta2[j] = m - 1 - i;
	for (i = 0; i <= m - 2; ++i)
		delta2[m - 1 - suff[i]] = m - 1 - i;
	/* jump of the text position at the mismatch */
	for (j = 0; j < m; ++j)
		delta2[j] += m - 1 - j;
	free(suff);
}

/***********************************************************************
 *	gosper
 *
 * Computes "Boyer-Moore" delta table -- puts skip distance in delta0[],
 * delta1[] and delta2[]. The pattern is not copied, it must exist as
 * long as the returned bm_t is used.
 */
bm_t* gosper(char *pat, int len, int iflag)
{						/* ... HAKMEM lives ... */
	REG1 int j;
	REG2 int c;
	REG3 uchar* pattern = (uchar*)pat;
	REG4 bm_t* bm;

	if ((bm = calloc(1, sizeof(bm_t))) == NULL)
		error("Out of memory", 3);
	bm->pat = pattern;
	bm->patlen = len;
#ifdef	TEST
	inner_cnt = outer_cnt = cmp_cnt = 0L;
#endif
	make_ccase(bm, iflag);
	build_delta2(bm, pat, len);
	bm->large =  MAXBUF + MAXREGMUST;
	/* For chars that aren't in string, skip by string length. */
	for (j = NCHARS; j--; )
		bm->delta1[j] = len;
	/* For chars in a string, skip distance from char to end of string. */
	/* (If char appears more than once, skip minimum distance.) */
	for (j = 0, --len; j <= len; ++j) {
		bm->delta1[c = pattern[j]] = len - j;
		if (iflag)		/* same skip with both cases */
			bm->delta1[CHCASE(c)] = bm->delta1[c];
	}
	for (c = NCHARS; c--; )
		bm->delta0[c] = bm->delta1[c];
	/* For last char, fall out of search loop. */
	bm->delta0[c = pattern[j-1]] = bm->large;
	if (iflag)
		bm->delta0[CHCASE(c)] = bm->delta0[c];
	return bm;
}

/**********************************************************************
 *	bm_free
 */
void bm_free(bm_t* bm)
{
	if (bm != NULL) {
		d_free(&bm->delta2);
		free(bm);
	}
}

/**********************************************************************
 *	boyer_moore
 *
 * "reach out and boyer-moore search someone"
 *  soon-to-be-popular bumper sticker
 */
char *boyer_moore(const bm_t* bm, char *str, char *se)
{
	REG1 uchar	*k;
	REG2 uchar	*strend = (uchar*)se;
	REG3 unsigned	delta1_jmp;
	REG4 unsigned	delta2_jmp;
	REG5 uchar	*s;
	/* table pointers in local variables keep the loops tight */
	REG6 const unsigned* delta0 = bm->delta0;
	const uchar*	ccase = bm->ccase;
	uchar*		pat = bm->pat;
	int		patlen = bm->patlen;
	unsigned	large = bm->large;

	/* The variables below exist merely for optimization purposes
	** to avoid calculations inside the outermost for loop.
	** They don't really affect the overall performance significantly,
	** though.
	*/
	uchar	*str_plus_large = (uchar*)str + large;
	uchar	*pat_plus_patlen_minus_1 = pat + patlen - 1;

	/* memchr can't ignore case */
	if (patlen == 1 && (!IS_ALPHA(*pat) || ccase['a'] != ccase['A']))
		return memchr(str, *pat, se-str+1);
	for (k = (uchar*)str + patlen - 1; ; ) {
		/* for a large class of patterns, upwards of 80% of
		 * match time is spent on the next loop.  We beat
		 * existing microcode (vax 'matchc') this way. */
#ifdef	TEST
		if (debug)
			for (; k <= strend; k += delta0[*k])
				++inner_cnt;
		else
#endif
		for (; k <= strend; k += delta0[*k])
			;
		if (k < str_plus_large)
			break;
		k -= large;

		/* compare to pattern */
		s = k;
		strend = pat_plus_patlen_minus_1;
#ifdef	TEST
		if (debug) {
			++outer_cnt;
			do {
				++cmp_cnt;
				--k;
				--strend;
				if (strend < pat)
					return (char*)s - patlen + 1;
			} while (ccase[*k] == *strend);
		} else
#endif
		do {
			--k;
			--strend;
			if (strend < pat)
				return (char*)s - patlen + 1;
		} while (ccase[*k] == *strend);
		/* Any optimizing compiler can usually do the below max()
		   better without temporary variables. */
		delta1_jmp = bm->delta1[*k];
		delta2_jmp = bm->delta2[strend-pat];
		k += max(delta1_jmp, delta2_jmp);
		strend = (uchar*)se;
#ifdef TEST
		if (debug > 2)
			printf("Boyer-moore: max = %d from %s\n",
				max(delta1_jmp, delta2_jmp),
				delta1_jmp > delta2_jmp
				? "delta1" : "delta2");
#endif
	}
	return NULL;
}
