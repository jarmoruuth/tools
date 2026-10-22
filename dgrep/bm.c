/**********************************************************************
 *
 * Boyer-Moore algorithm separated from original egrep.c-file in
 * fastgrep. Here we won't do alternation, as in original version.
 *
 * Fastgrep was written by
 *   James A. Woods				Copyright (c) 1986
 *   NASA Ames Research Center
 *
 *	J.Ruuth 07-Feb-1988
 *
 * Modified Boyer-Moore for guaranteed O(m+n) time performance.
 *	P.Soini 07-Dec-1988
 **********************************************************************/

#include "system.h"
#include "dfa.h"
#include "bm.h"

#if (MAXREGMUST > MAXCHAR)
typedef	unsigned short	delta1_jmp_t;
#else
typedef	unsigned char	delta1_jmp_t;
#endif

struct bm_st {
	uchar*		pat;		/* pattern */
	int		patlen;		/* pattern length */
	/* large should be larger than or equal to strlen + patlen */
	unsigned	large;
	uchar		c_index[NCHARS];
	unsigned	n_index;
	delta1_jmp_t*	delta1[MAXREGMUST-1];
	unsigned	delta0[NCHARS];  /* Boyer-Moore algorithm core */
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
		int i,j;
		printf("       ");
		for (i = 0; i < NCHARS; i++)
			if (bm->c_index[i])
				printf(" %2c", i);
		printf("\n");
		for (i = 1; i < bm->patlen; i++) {
			printf("%2d: ", i);
			printf(" %2d", (bm->delta1-1)[i][0]);
			for (j = 0; j < NCHARS; j++)
				if (bm->c_index[j] != 0)
					printf(" %2d", (bm->delta1-1)[i][bm->c_index[j]]);
			printf("\n");
		}
	}
	printf( "Boyer-Moore inner loop iterated "
		"%ld, outer %ld, comp.loop %ld times\n",
		inner_cnt, outer_cnt, cmp_cnt);
}
#endif

/************************************************************************
 *	init_c_index
 *
 * Allocates index for every character. Every character in 'pat' will
 * get an index entry of its own. All characters that are not in
 * pat[0] .. pat[patlen-3] will be in one index: 0
 */
static void init_c_index(bm_t* bm, uchar* pat, int patlen, int iflag)
{
	register int	i, j;

	for (i = NCHARS; i > 0; )
		bm->c_index[--i] = 0;
	for (bm->n_index = 1, i = patlen - 2; i > 0; ) {
		if (!bm->c_index[j = pat[--i]]) {
			bm->c_index[j] = bm->n_index++;
			if (iflag)
				bm->c_index[CHCASE(j)] = bm->c_index[j];
		}
	}
}

/**********************************************************************
 *
 *	make_ccase
 */
static void	make_ccase(bm_t* bm, int iflag)
{
	REG1 int	i = NCHARS;

	if (iflag) {
		while (i--)
			bm->ccase[i] = IS_LOWER(i) ? CHCASE(i) : i;
	} else {
		while (i--)
			bm->ccase[i] = i;
	}
}

/************************************************************************
 *	gosper
 *
 * slides pattern over itself and tries to find rightmost and longest
 * reoccurence of its right side. Then it calculates jump value
 * for that character position and the character that caused the first
 * mismatch. ie. this routine calculates jump values for all
 * entries of delta1 and delta0. The pattern is not copied, it must
 * exist as long as the returned bm_t is used.
 */
bm_t*	gosper(char* pat, REG5 int patlen, int iflag)
{
	REG3 uchar*	patend = (uchar*)pat + patlen - 1;
	REG1 int	i;
	REG2 int	j;
	REG4 int	k;
	REG6 bm_t*	bm;
	delta1_jmp_t**	delta1;
	unsigned*	delta0;

	if ((bm = calloc(1, sizeof(bm_t))) == NULL)
		error("Out of memory", 3);
	bm->pat = (uchar*)pat;
	bm->patlen = patlen;
	delta1 = bm->delta1;
	delta0 = bm->delta0;
#ifdef	TEST
	inner_cnt = outer_cnt = cmp_cnt = 0L;
#endif
	make_ccase(bm, iflag);
	bm->large =  maxbuf + MAXREGMUST;
	for (j = NCHARS; j; )
		delta0[--j] = patlen;
	/* first we initialize the space-saving c_index array */
	init_c_index(bm, (uchar*)pat, patlen, iflag);
	/* next we allocate jump array */
	if (patlen > 1
	&&  (delta1[0] = malloc(sizeof(delta1_jmp_t) * bm->n_index *
				(unsigned)(patlen - 1))) == NULL)
		error("Out of memory", 3);
	for (i = 1, j = patlen - 1; i < j; ++i)
		delta1[i] = (delta1-1)[i] + bm->n_index;
	for (i = patlen - 1; i > 0; --i)
		for (j = bm->n_index; j > 0; )
			(delta1-1)[i][--j] = patlen;

	/* from now on 'j' represents the slide value and
	** 'i' represents the offset calculated backward from the
	** 'patend'
	*/
	for (j = patlen - 1; j > 0; --j) {
		if (patend[0] != (k = patend[-j])) {	/* mismatch at 'patend' */
			delta0[k] = j;
			if (iflag)
				delta0[CHCASE(k)] = j;
			continue; /* to next value of 'j' */
		}
		for (i = 1; ; ++i) {
			if (i + j >= patlen) {	/* was a match */
				for (; i < patlen; ++i)
					for (k = bm->n_index; k > 0; )
						(delta1-1)[i][--k] = j;
				break;
			}
			if (patend[-i] != (k = patend[-i - j])) { /* mismatch */
				(delta1-1)[i][bm->c_index[k]] = j;
				break;
			}
		}
	}
	/* the 'large' trick to fall out of search loop when the
	** rightmost char matches. This trick saves one comparison
	** from the inner loop of the Boyer-Moore search.
	*/
	delta0[k = *patend] = bm->large;
	if (iflag)
		delta0[CHCASE(k)] = bm->large;
	return bm;
}

/**********************************************************************
 *
 *	bm_free
 */
void bm_free(bm_t* bm)
{
	if (bm != NULL) {
		d_free(&bm->delta1[0]);
		free(bm);
	}
}

/**********************************************************************
 *
 *	boyer_moore
 *
 * "reach out and boyer-moore search someone"
 *  soon-to-be-popular bumper sticker
 */
char* boyer_moore(const bm_t* bm, char* str, char* strend)
{
	REG1 uchar*	k;
	REG2 uchar*	p;
	REG3 uchar*	s;
	/* table pointers in local variables keep the loops tight */
	REG4 const unsigned*	delta0 = bm->delta0;
	REG5 const uchar*	ccase = bm->ccase;
	uchar*		pat = bm->pat;
	int		patlen = bm->patlen;
	unsigned	large = bm->large;

	/* The variables below exist merely for optimization purposes
	** to avoid calculations inside the outermost for loop.
	** They don't really affect the overall performance significantly,
	** though.
	*/
	uchar*	str_plus_large = (uchar*)str + large;
	uchar*	patend = pat + patlen - 1;

	if (patlen == 1 && (!IS_ALPHA(*pat) || ccase['a'] != ccase['A']))
		return memchr(str, *pat, strend-str+1);
	for (k = (uchar*)str + patlen - 1; ; ) {
		p = (uchar*)strend;
		/* for a large class of patterns, upwards of 80% of
		 * match time is spent on the next loop.  We beat
		 * existing microcode (vax 'matchc') this way.
		 * In VAX with 'gcc' compiler the loop is only 4 instructions
		 * long, in 80([123]?86|1?88) with MSC 5.1 compiler it is 6
		 * instructions.
		 */
#ifdef	TEST
		if (debug)
			for (; k <= p; k += delta0[*k])
				++inner_cnt;
		else
#endif
		for (; k <= p; k += delta0[*k])
			;
		if (k < str_plus_large)
			return NULL;
		s = k -= large;
		p = patend;
#ifdef	TEST
		if (debug) {
			++outer_cnt;
			do {
				++cmp_cnt;
				--k;
				--p;
				if (p < pat)
					return (char*)s - patlen + 1;
			} while (ccase[*k] == *p);
		} else
#endif
		do {
			--k;
			--p;
			if (p < pat)
				return (char*)s - patlen + 1;
		} while (ccase[*k] == *p);
		k = s + (bm->delta1-1)[patend - p][bm->c_index[*k]];
	}
}
