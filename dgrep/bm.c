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
 *
 * SSE2 search: two rare bytes of the pattern are compared to 16
 * positions at a time, and the positions where both match are verified.
 * Boyer-Moore is used for the end of the buffer and if there are too
 * many false candidates.
 **********************************************************************/

#include "system.h"
#include "dfa.h"
#include "bm.h"

/* SSE2 is in every x86-64 CPU; -DNOSIMD disables it */
#if !defined(NOSIMD) && (defined(__SSE2__) || defined(_M_X64) \
			 || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define BM_SSE2
#include <emmintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#endif

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
	/* SSE2 search: positions of two rare bytes in the pattern
	   (rare1 <= rare2) and the bytes in both cases */
	int		rare1, rare2;
	uchar		byte1, byte1alt;
	uchar		byte2, byte2alt;
	char		iflag;
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
 *	byte_frequency
 *
 * Rough frequency of byte c in text, used to pick rare bytes for the
 * SSE2 search. Lower case letters by English letter frequency, upper
 * case letters are rarer, space is the most common.
 */
static int byte_frequency(int c)
{
	/* per mille, a..z */
	static const unsigned char letters[26] = {
		82, 15, 28, 43, 127, 22, 20, 61, 70, 2, 8, 40, 24,
		67, 75, 19, 1, 60, 63, 91, 28, 10, 24, 2, 20, 1
	};

	if (c == ' ')
		return 200;
	if (IS_LOWER(c))
		return letters[c - 'a'];
	if (IS_UPPER(c))
		return letters[c - 'A'] / 4;
	if (c >= '0' && c <= '9')
		return 12;
	if (c == '\t' || c == '.' || c == ',' || c == '_' || c == '-'
	    || c == '(' || c == ')' || c == ';' || c == '=' || c == '/'
	    || c == '"' || c == '\'' || c == ':' || c == '*')
		return 10;
	return 3;
}

/************************************************************************
 *	init_rare
 *
 * Selects two rare bytes of the pattern for the SSE2 search.
 */
static void init_rare(bm_t* bm, uchar* pat, int patlen, int iflag)
{
	int	i, f, r1, r2, f1, f2;

	r1 = r2 = 0;
	f1 = f2 = 1000;
	for (i = 0; i < patlen; i++) {
		f = byte_frequency(pat[i]);
		if (iflag && IS_ALPHA(pat[i]))
			f = byte_frequency(CHCASE(pat[i])) + f;
		if (f < f1) {
			r2 = r1;
			f2 = f1;
			r1 = i;
			f1 = f;
		} else if (f < f2) {
			r2 = i;
			f2 = f;
		}
	}
	if (patlen == 1)
		r2 = r1;
	bm->rare1 = min(r1, r2);
	bm->rare2 = max(r1, r2);
	bm->iflag = iflag != 0;
	bm->byte1 = pat[bm->rare1];
	bm->byte2 = pat[bm->rare2];
	bm->byte1alt = iflag ? CHCASE(bm->byte1) : bm->byte1;
	bm->byte2alt = iflag ? CHCASE(bm->byte2) : bm->byte2;
}

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
	init_rare(bm, (uchar*)pat, patlen, iflag);
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
 *	bm_search
 *
 * "reach out and boyer-moore search someone"
 *  soon-to-be-popular bumper sticker
 */
static char* bm_search(const bm_t* bm, char* str, char* strend)
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

#ifdef BM_SSE2

/* after SSE_MINFALSE false candidates, Boyer-Moore is used if there
   is more than one false candidate in SSE_FALSEGAP bytes */
#define SSE_MINFALSE	64
#define SSE_FALSEGAP	32

/**********************************************************************
 *
 *	lowest_bit
 */
static int lowest_bit(unsigned m)
{
#ifdef _MSC_VER
	unsigned long	i;

	_BitScanForward(&i, m);
	return (int)i;
#else
	return __builtin_ctz(m);
#endif
}

/**********************************************************************
 *
 *	sse2_search
 *
 * Compares the two rare bytes of the pattern to 16 positions at a
 * time and verifies the positions where both match. Reads only bytes
 * str .. strend.
 */
static char* sse2_search(const bm_t* bm, char* str, char* strend)
{
	uchar*		p = (uchar*)str;
	uchar*		end = (uchar*)strend + 1;	/* after last byte */
	uchar*		pat = bm->pat;
	int		patlen = bm->patlen;
	int		rare1 = bm->rare1;
	int		rare2 = bm->rare2;
	const uchar*	ccase = bm->ccase;
	const __m128i	b1 = _mm_set1_epi8((char)bm->byte1);
	const __m128i	b1alt = _mm_set1_epi8((char)bm->byte1alt);
	const __m128i	b2 = _mm_set1_epi8((char)bm->byte2);
	const __m128i	b2alt = _mm_set1_epi8((char)bm->byte2alt);
	unsigned	nfalse = 0;
	unsigned	mask;
	uchar*		cand;
	int		i;

	for (; end - p >= rare2 + 16; p += 16) {
		__m128i	t1 = _mm_loadu_si128((const __m128i*)(p + rare1));
		__m128i	t2 = _mm_loadu_si128((const __m128i*)(p + rare2));
		__m128i	e1 = _mm_cmpeq_epi8(t1, b1);
		__m128i	e2 = _mm_cmpeq_epi8(t2, b2);

		if (bm->iflag) {
			e1 = _mm_or_si128(e1, _mm_cmpeq_epi8(t1, b1alt));
			e2 = _mm_or_si128(e2, _mm_cmpeq_epi8(t2, b2alt));
		}
		mask = (unsigned)_mm_movemask_epi8(_mm_and_si128(e1, e2));
		for (; mask != 0; mask &= mask - 1) {
			cand = p + lowest_bit(mask);
			if (end - cand < patlen)
				return NULL;	/* no room for later matches */
			if (bm->iflag) {
				for (i = 0; i < patlen && ccase[cand[i]] == pat[i]; i++)
					;
			} else
				i = memcmp(cand, pat, patlen) == 0 ? patlen : 0;
			if (i == patlen)
				return (char*)cand;
			if (++nfalse > SSE_MINFALSE
			    && nfalse > (unsigned)(cand - (uchar*)str) / SSE_FALSEGAP)
				return bm_search(bm, (char*)p, strend);
		}
	}
	/* end of buffer, less than 16 positions left */
	return bm_search(bm, (char*)p, strend);
}

#endif /* BM_SSE2 */

/**********************************************************************
 *
 *	boyer_moore
 *
 * Returns the first match of the pattern in str .. strend (inclusive),
 * or NULL.
 */
char* boyer_moore(const bm_t* bm, char* str, char* strend)
{
#ifdef BM_SSE2
	/* bm_search uses memchr, which is faster, for one character
	   except with -i */
	if (bm->patlen > 1 || (bm->iflag && IS_ALPHA(*bm->pat)))
		return sse2_search(bm, str, strend);
#endif
	return bm_search(bm, str, strend);
}
