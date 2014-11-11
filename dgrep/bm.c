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

/* large should be larger than or equal to strlen + patlen */
static unsigned large;

#if (MAXREGMUST > MAXCHAR)
typedef	unsigned short	delta1_jmp_t;
#else
typedef	unsigned char	delta1_jmp_t;
#endif

static uchar		c_index[NCHARS];
static unsigned		n_index;
static delta1_jmp_t*    delta1[MAXREGMUST-1] /*= {0}*/;
static unsigned		delta0[NCHARS];  /* Boyer-Moore algorithm core */

/* Character conversion table, in case sensitive search, gives uppercase */
/* version of given character, otherwise just the same character. */
static uchar ccase[NCHARS];

#ifdef TEST
extern int	debug;
static long inner_cnt, outer_cnt, cmp_cnt;
static int plen;
void show_boyer_moore_report(void)
{
	if (debug > 1) {
		int i,j;
		printf("       ");
		for (i = 0; i < NCHARS; i++)
			if (c_index[i])
				printf(" %2c", i);
		printf("\n");
		for (i = 1; i < plen; i++) {
			printf("%2d: ", i);
			printf(" %2d", (delta1-1)[i][0]);
			for (j = 0; j < NCHARS; j++)
				if (c_index[j] != 0)
					printf(" %2d", (delta1-1)[i][c_index[j]]);
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
static void init_c_index(uchar* pat, int patlen, int iflag)
{
	register int	i, j;
	
	for (i = NCHARS; i > 0; )
		c_index[--i] = 0;
	for (n_index = 1, i = patlen - 2; i > 0; ) {
		if (!c_index[j = pat[--i]]) {
			c_index[j] = n_index++;
			if (iflag)
				c_index[CHCASE(j)] = c_index[j];
		}
	}
}

/**********************************************************************
 *
 *	make_ccase
 */
static void	make_ccase(int iflag)
{
	REG1 int	i = NCHARS;
	
	if (iflag) {
		while (i--)
			ccase[i] = islower(i) ? CHCASE(i) : i;
	} else {
		while (i--)
			ccase[i] = i;
	}
}

/************************************************************************
 *	gosper
 *
 * slides pattern over itself and tries to find rightmost and longest
 * reoccurence of its right side. Then it calculates jump value
 * for that character position and the character that caused the first
 * mismatch. ie. this routine calculates jump values for all
 * entries of delta1 and delta0.
 */
void	gosper(char* pat, REG5 int patlen, int iflag)
{
	REG3 uchar*	patend = (uchar*)pat + patlen - 1;
	REG1 int	i;
	REG2 int	j;
	REG4 int	k;
		
#ifdef	TEST
	inner_cnt = outer_cnt = cmp_cnt = 0L;
	plen = patlen;
#endif
	make_ccase(iflag);
	large =  maxbuf + MAXREGMUST;
	for (j = NCHARS; j; )
		delta0[--j] = patlen;
	/* first we initialize the space-saving c_index array */
	init_c_index(pat, patlen, iflag);
	/* next we allocate jump array */
	d_free(&delta1[0]);
	if (patlen > 1
	&&  (delta1[0] = malloc(sizeof(delta1_jmp_t) * n_index *
				(unsigned)(patlen - 1))) == NULL)
		error("Out of memory", 3);
	for (i = 1, j = patlen - 1; i < j; ++i)
		delta1[i] = (delta1-1)[i] + n_index;
	for (i = patlen - 1; i > 0; --i)
		for (j = n_index; j > 0; )
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
					for (k = n_index; k > 0; )
						(delta1-1)[i][--k] = j;
				break;
			}
			if (patend[-i] != (k = patend[-i - j])) { /* mismatch */
				(delta1-1)[i][c_index[k]] = j;
				break;
			}
		}
	}
	/* the 'large' trick to fall out of search loop when the
	** rightmost char matches. This trick saves one comparison
	** from the inner loop of the Boyer-Moore search.
	*/
	delta0[k = *patend] = large;	
	if (iflag)			
		delta0[CHCASE(k)] = large;
}

/**********************************************************************
 *
 *	boyer_moore
 *
 * "reach out and boyer-moore search someone"
 *  soon-to-be-popular bumper sticker
 */
char* boyer_moore(char* str, char* strend, char* pat, int patlen)
{
	REG1 uchar*	k;
	REG2 uchar*	p;
	REG3 uchar*	s;

	/* The variables below exist merely for optimization purposes
	** to avoid calculations inside the outermost for loop.
	** They don't really affect the overall performance significantly,
	** though.
	*/
	uchar*	str_plus_large = (uchar*)str + large;
	uchar*	patend = (uchar*)pat + patlen - 1;
	
	if (patlen == 1 && (!isalpha(*pat) || ccase['a'] != ccase['A']))
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
				if (p < (uchar*)pat)
					return s - patlen + 1;
			} while (ccase[*k] == *p);
		} else
#endif
		do {
			--k;
			--p;
			if (p < (uchar*)pat)
				return s - patlen + 1;
		} while (ccase[*k] == *p);
		k = s + (delta1-1)[patend - p][c_index[*k]];
	}
}
