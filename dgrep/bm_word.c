/**********************************************************************
 *
 * Boyer-Moore algorithm that checks one word at a time instead of just 
 * a single character. In theory sligtly faster, because stays longer 
 * in the inner loop. In practise this is slower, because inner loop is 
 * more complicated and jumps are equally long in both versions. If we 
 * had enough memory, we could make the inner loop as fast as the 
 * character version (64k table for delta0, if MAXCHAR is 255).
 *
 *	J.Ruuth 11-Dec-1988
 **********************************************************************/

/* Same code as in boymoore.c is removed */

static unsigned	d0[NCHARS*3];  /* Boyer-Moore algorithm core */

/***********************************************************************
 *
 *	gosper
 *
 * Computes "Boyer-Moore" delta table -- puts skip distance in d0[c]
 */
void gosper(uchar *pattern, int len, int iflag)
{						/* ... HAKMEM lives ... */
	REG1 int 	j;
	REG2 int 	c;
	REG3 char	*p;
	REG4 int	default_skip;

#ifdef	TEST
	inner_cnt = outer_cnt = cmp_cnt = 0L;
#endif
	if (len <= 1)
		return;
	make_ccase(iflag);
	large =  MAXBUF + MAXREGMUST;
	make_delta1(pattern, len, iflag);
	p = pattern;
	/* For chars that aren't in string, skip by string length. */
	for (j = NCHARS; j < 3*(NCHARS-1); j++)
		d0[j] = len;
	/* Default skip is from the first skip array. */
	for (j = 0; j < NCHARS; j++)
		d0[j] = NCHARS;
	/* For chars in a string, skip distance from char to end of string. */
	/* (If char appears more than once, skip minimum distance.) */
	for (j = 0, --len; j < len-1; ++j) {
		c = p[j];
		d0[c] = 2 * NCHARS;
		if (iflag)		/* same index with both cases */
			d0[CHCASE(c)] = d0[c];
		d0[c + 2 * NCHARS] = len - 1;
		if (iflag)		/* same skip with both cases */
			d0[CHCASE(c) + 2 * NCHARS] = d0[c + 2 * NCHARS];
		c = p[j+1];
		d0[c + 2 * NCHARS] = len - j - 1;
		if (iflag)		/* same skip with both cases */
			d0[CHCASE(c) + 2 * NCHARS] = d0[c + 2 * NCHARS];
	}
	/* For last char, fall out of search loop. */
	c = p[j];
	d0[(char)c] = 2 * NCHARS;
	if (iflag)		/* same index with both cases */
		d0[CHCASE(c)] = d0[c];
	c = p[j+1];
	d0[c + 2 * NCHARS] = large;
	if (iflag)		/* same skip with both cases */
		d0[CHCASE(c) + 2 * NCHARS] = d0[c + 2 * NCHARS];
}


/**********************************************************************
 *
 *	boyer_moore
 *
 * "reach out and boyer-moore search someone"
 *  soon-to-be-popular bumper sticker
 */
char *boyer_moore(uchar *str, uchar *se,
		  uchar *pat, int patlen)
{
	REG1 uchar	*k;
	REG2 uchar	*strend = se;
	uchar		*s;
	unsigned	wordlen;

	/* The variables below exist merely for optimization purposes
	** to avoid calculations inside the outermost for loop.
	** They don't really affect the overall performance significantly,
	** though.
	*/
	uchar	*str_plus_large = str + large;
	uchar	*pat_plus_patlen_minus_1 = pat + patlen - 1;
	
	if (patlen <= 0)
		return NULL;
	if (patlen == 1)
		return memchr(str, *pat, se-str+1);
	for (k = str + patlen - 1; ; ) {
		/* adjust k to point to the last word that we could match */
		k--;
		/* for a large class of patterns, upwards of 80% of 
		 * match time is spent on the next loop.  We beat 
		 * existing microcode (vax 'matchc') this way. */
#ifdef	TEST
		if (debug)
			for (; k <= strend; k += d0[d0[*k] + (*(unsigned *)k >> 8)])
				++inner_cnt;
		else
#endif
		for (; k <= strend; k += d0[d0[*k] + (*(unsigned *)k >> 8)])
			;
		/* adjust k to the last matched char */
		k++;
		if (k < str_plus_large)
			break;
		k -= large;
 		strend = pat_plus_patlen_minus_1;
		s = k;
 		/* We know, that last two chars match, so we can as well
 		   skip them. Below we skip the first one and the second
 		   char in the do loop.
 		*/
		k--;
		strend--;
		/* compare to pattern */
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
		k = s + (delta1-1)[pat_plus_patlen_minus_1-strend][c_index[*k]];
		strend = se;
	}
	return NULL;
}
