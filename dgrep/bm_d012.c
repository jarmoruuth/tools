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

/* large should be larger than or equal to strlen + patlen */
static unsigned 	large;

/* Boyer-Moore algorithm core */
static unsigned	delta0[NCHARS];
static unsigned	delta1[NCHARS];
static unsigned	*delta2;

/* Character conversion table, in case sensitive search, gives uppercase */
/* version of given character, otherwise just the same character. */
static uchar ccase[NCHARS];

#ifdef TEST
extern int	debug;
static long inner_cnt, outer_cnt, cmp_cnt;
static int plen;

void show_boyer_moore_report()
{
	if (debug > 1) {
		int i;
		for (i=0; i < plen; i++) {
			printf("delta2[%d] = %d\n", i, delta2[i]);
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
static void make_ccase (int iflag)
{
	REG1 int	i = NCHARS;
	
	while (i--) {
		if (iflag && islower(i))
			ccase[i] = toupper(i);
		else
			ccase[i] = i;
	}
}

/**********************************************************************
 *	build_delta2
 *
 * D.Knuth's linear algorithm to build dd' (delta2).
 */
static void build_delta2 (REG4 char *pat, int patlen)
{
	REG1 int	j;
	REG2 int	t;
	REG3 int	*f;
	
	d_free(&delta2);
	delta2 = malloc((unsigned)patlen * sizeof(unsigned));
	if (delta2 == NULL)
		error("Out of memory", 3);
	f = malloc(patlen * sizeof(int));
	if (f == NULL)
		error("Out of memory", 3);
	t = 2 * patlen - 1;
	for (j = patlen-1; j >= 0; j--)
		delta2[j] = t - j;
	t = patlen;
	j = t - 1;
	for (; j >= 0; j--, t--) {
		f[j] = t;
		while (t < patlen && pat[j] != pat[t]) {
			delta2[t] = min(delta2[t], patlen-j-1);
			t = f[t];
		}
	}
	for (j = 0; j <= t; j++)
		delta2[j] = min(delta2[j], patlen+t-j);
	free(f);
}

/***********************************************************************
 *	gosper
 *
 * Computes "Boyer-Moore" delta table -- puts skip distance in delta0[],
 * delta1[] and delta2[].
 */
void gosper(uchar *pattern, int len, int iflag)
{						/* ... HAKMEM lives ... */
	REG1 int j;
	REG2 int c;

#ifdef	TEST
	inner_cnt = outer_cnt = cmp_cnt = 0L;
	plen = len;
#endif
	make_ccase(iflag);
	build_delta2(pattern, len);
	large =  MAXBUF + MAXREGMUST;
	/* For chars that aren't in string, skip by string length. */
	for (j = NCHARS; j--; )
		delta1[j] = len;
	/* For chars in a string, skip distance from char to end of string. */
	/* (If char appears more than once, skip minimum distance.) */
	for (j = 0, --len; j <= len; ++j) {
		delta1[c = pattern[j]] = len - j;
		if (iflag)		/* same skip with both cases */
			delta1[CHCASE(c)] = delta1[c];
	}
	for (c = NCHARS; c--; )
		delta0[c] = delta1[c];
	/* For last char, fall out of search loop. */
	delta0[c = pattern[j-1]] = large;
	if (iflag)
		delta0[CHCASE(c)] = delta0[c];
}


/**********************************************************************
 *	boyer_moore
 *
 * "reach out and boyer-moore search someone"
 *  soon-to-be-popular bumper sticker
 */
char *boyer_moore(uchar *str, uchar *se, REG6 uchar *pat, int patlen)
{
	REG1 uchar	*k;
	REG2 uchar	*strend = se;
	REG3 unsigned	delta1_jmp;
	REG4 unsigned	delta2_jmp;
	REG5 uchar	*s;

	/* The variables below exist merely for optimization purposes
	** to avoid calculations inside the outermost for loop.
	** They don't really affect the overall performance significantly,
	** though.
	*/
	uchar	*str_plus_large = str + large;
	uchar	*pat_plus_patlen_minus_1 = pat + patlen - 1;
	
	if (patlen == 1)
		return memchr(str, *pat, se-str+1);
	for (k = str + patlen - 1; ; ) {
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
					return s - patlen + 1;
			} while (ccase[*k] == *strend);
		} else
#endif
		do {
			--k;
			--strend;
			if (strend < pat)
				return s - patlen + 1;
		} while (ccase[*k] == *strend);
		/* Any optimizing compiler can usually do the below max()
		   better without temporary variables. */
		delta1_jmp = delta1[*k];
		delta2_jmp = delta2[strend-pat];
		k += max(delta1_jmp, delta2_jmp);
		strend = se;
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
