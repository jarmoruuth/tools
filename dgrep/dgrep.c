/**********************************************************************\
 *
 *	DGREP.C
 *
 * Main module for dgrep. Input, output, search strategy selection
 * etc. are done here.
 *
 * Author: Jarmo Ruuth 6-Feb-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include "system.h"
#include "dfa.h"
#include "dfaregex.h"
#include "bm.h"
#include "getopt.h"
#include "walk.h"
#include <threads.h>
#ifdef USE_SSE2
#include <emmintrin.h>
#endif

#define VERSION		"1.75"

typedef enum {
	STOP,
	OK
} status_t;

typedef enum {
	NONE,		/* no file name displayed */
	BEFORE_LINE,	/* name displayed before each matching line (in same line) */
	BLOCK		/* name displayed only once before matching lines */
} show_fname_t;

/* Options. They are set in main and only read after that. */

#ifdef TEST
int			debug		= FALSE;
#endif
static char		nonmatch	= FALSE;
static char		count		= FALSE;
static char		names		= FALSE;
static char		number		= FALSE;
static char		ignorecase	= FALSE;
static char		regmust_only	= FALSE;
static char		exact		= FALSE;
static char		silent		= FALSE;
static char		no_boymoore	= FALSE;
static char		touch		= FALSE;
static char		block_mode	= FALSE;
static char		path_mode	= FALSE;
static char		recursive	= FALSE;

/* Binary files (files that contain a NUL byte) */
typedef enum {
	BIN_MATCHES,	/* print "Binary file x matches" (default) */
	BIN_TEXT,	/* -a, grep as text */
	BIN_SKIP	/* -I, skip binary files */
} binary_files_t;

static binary_files_t	binary_files	= BIN_MATCHES;
static char		zap_nul		= FALSE; /* NULs to EOLs in binary files */
static char		verbose		= FALSE;
static int		leading_context	= 0;
static int		trailing_context= 0;

/*
	Output of one file. It is collected in memory and written in the
	order of the files, see out_flush.
*/
typedef struct {
	char*		buf;
	size_t		len;
	size_t		size;
	size_t		flush_at;	/* try to write when len reaches this */
	char		sep_before;	/* separator before output if earlier
					   files printed context groups */
	char		groups;		/* context groups printed */
	char		finished;	/* file is grepped, output complete */
} out_t;

/*
	State of grepping one file. All functions that grep a file get
	a grep_t* gp parameter, and the OFFSET and BUFPTR macros use it.
	There will be one grep_t for each thread.
*/
typedef struct {
	regbuf_t*	rb;		/* compiled expression */
	uchar*		buffer;		/* input buffer, bufsize+1 bytes */
	unsigned	bufsize;	/* buffer size, at most maxbuf */
	char*		path;		/* current path and filename */
	char		fullpath[2048];	/* path for -p */
	ulong		matchcount;
	ulong		linecount;
	char		fname_shown;	/* block mode file name printed */
	char		binary;		/* file contains a NUL byte */
	/* Context printing state. Offsets are file offsets, so they stay
	   valid when the buffer contents are moved. */
	long long	buf_offset;	/* file offset of buffer[0] */
	long long	printed_offset;	/* offset after last printed line, -1 if none */
	int		trailing_left;	/* trailing context lines to print */
	ulong		trailing_lineno;/* line number of next trailing line */
	int		fileidx;	/* index of the file in files, -1 for stdin */
	out_t		out;		/* output of the file */
	/* results */
	char		was_match;
	char		file_not_found;
	char		touch_failed;
} grep_t;

#define OFFSET(p)	(gp->buf_offset + ((uchar*)(p) - gp->buffer))
#define BUFPTR(off)	((char*)gp->buffer + ((off) - gp->buf_offset))

/* output is written when there is at least OUTFLUSH bytes */
#define OUTFLUSH	(64*1024)

/* Searching the regmust string first and verifying the line with the
   dfa is slower than plain dfa if the regmust string is common. After
   MUSTCHECK hits in a buffer, regmust is not used for the rest of the
   buffer if there was a hit on average more often than every must_gap
   bytes. One character is searched with memchr, which is much faster
   than Boyer-Moore with a short pattern, so it can have more hits. */
#define MUSTCHECK	64
#define MUSTGAP_BM	128
#define MUSTGAP_MEMCHR	64
#define MUST_TOO_COMMON(p, buf)	((p) - (buf) < MUSTCHECK * must_gap)

/* Search setup. Set in main and only read after that. */
static int		must_gap;
static int		type_bits	= RE_SEARCH;
static show_fname_t	show_fname	= NONE;
static char*		expr		= NULL;
static bm_t*		bm;		/* Boyer-Moore tables for regmust */
static char*		regmust	= NULL;
static int		regmustlen;
static char		use_normal;
static struct utimbuf	touch_time;	/* time for -t */

/*
	Grepping files in parallel. Each thread has its own grep_t and
	takes the next file from files. Output of a file is written only
	when all earlier files are written (out_turn); finished output that
	must wait is kept in done. The lock protects next_file, out_turn,
	done, groups_printed and writing to stdout.
*/
#define MAXTHREADS	64	/* most threads with -j */
#define DEFTHREADS	16	/* most threads by default */

static int		nthreads	= 0;	/* -j, 0 = default */
static char**		files;		/* files to grep */
static int		nfiles;
static int		next_file	= 0;	/* next file to grep */
static int		out_turn	= 0;	/* file whose output is written */
static out_t*		done;		/* finished output waiting for turn */
static mtx_t		lock;
static char		groups_printed	= FALSE; /* context groups written */

/* By default, use time() as a random number generator */
#define drandom()	((unsigned)time(NULL))

/* Funtext quotations are found from the pep[1-4]grep.doc texts written
   by James A. Woods. Texts came with the fastgrep archive. (Woods is also
   responsible for the original idea to combine bm-search with egrep.) */
static char* funtext[] = {
	
   "The chief defect of Henry King\n"
   "Was chewing little bits of string.\n"
   "\t-- Hilaire Belloc, Cautionary Tales [1907]\n",

   "Attempt the end, and never stand to doubt\n"
   "Nothing's so hard but search will find it out.\n"
   "\t-- Robert Herrick, Hesperides [1648]\n",

   "Gratiano speaks an infinite deal of nothing, more than any man in all\n"
   "of Venice.  His reasons are as two grains of wheat hid in two bushels of\n"
   "chaff:  you shall seek all day ere you find them, they are not worth\n"
   "the search.\n"
   "\t-- Shakespeare, Merchant of Venice\n",

   "Reach out and Boyer-Moore Egrep Someone\n"
   "\t-- James A. Woods\n",

   "I need very little,\n"
   "  and of that little,\n"
   "     I need very little.\n"
   "\t-- St. Francis of Assisi\n",

   "How long a time lies in one little word!\n"
   "\t-- Shakespeare, Richard II, I, iii\n",

   "Fine words butter no parsnips.\n"
   "\t-- Southern proverb\n"
};

#define NFUNTEXT	(sizeof(funtext) / sizeof(funtext[0]))

static char* usage_line = {
"Usage: dgrep [options] {-f expression file | [-e] expression} [file...]\n"
};

static char* version_line = {
"Version " VERSION ", dgrep -h for full help\n"
};

static char* options1 = {
"Options: -An  n lines after the matching line are printed\n"
      "\t -Bn  n lines before the matching line are printed\n"
      "\t -a   binary files are grepped as text\n"
      "\t -b   filename is displayed only once before matches\n"
      "\t -c   only a count of matching lines is printed\n"
      "\t -d   only dfa is used for searching\n"
      "\t -i   case insensitive match\n"
      "\t -I   binary files are skipped\n"
      "\t -l   only names of files with matching lines are printed\n"
      "\t -n   each line is preceded by its relative line number in file\n"
	  "\t -p   filename is printed with full path\n"
	  "\t -r   directories are searched recursively\n"
      "\t -s   silent mode, nothing is printed except error messages\n"
};

/* split options for MSC 5.X */
static char* options2 = {
      "\t -t   all files that contain matches are touched\n"
      "\t -v   all lines but those matching are printed\n"
      "\t -x   exact, all characters in expression are taken literally\n"
      "\t -z   verbose\n"
      "\t -1-9 1-9 lines before and after the matching line are printed\n"
      "\t -jn  n threads grep files in parallel (default CPUs, at most 16)\n"
      "\t -e expression, useful when expression begins with -\n"
      "\t -f file that contains expression\n"
};

static char* regexps = {
"Regular expressions:\t\t\t"		"."	"\tany single character\n"
"*"	"\tzero or more repeats\t\t"	"(...)"	"\tgrouping\n"
"+"	"\tone or more repeats\t\t"	"^"	"\tbeginning of line\n"
"?"	"\tzero or one repeat\t\t"	"$"	"\tend of line\n"
"[...]"	"\tany character in set\t\t"	"\\c"	"\tquote special character c\n"
"[^...]""\tany character not in set\t"	"|"	"\talternative (\"or\")\n"
};

/**********************************************************************
 *
 *	short_usage
 *
 * Exits with options help message and funtext.
 */
static void short_usage(void)
{
	printf("%s%s%s%s%s",
		usage_line,
		version_line,
		options1,
		options2,
		funtext[drandom() % NFUNTEXT]);
	exit(2);
}

/**********************************************************************
 *
 *	long_usage
 *
 * Exits with long help message.
 */
static void long_usage(void)
{
	printf("%s%s%s%s",
		usage_line,
		options1,
		options2,
		regexps);
	exit(2);
}

/**********************************************************************
 *
 *	quote_expr
 */
static char* quote_expr(REG1 char* expr, unsigned len)
{
	REG2 char*	p;
	REG3 char*	ptr;
	
	p = malloc(2 * len + 1);
	if (p == NULL)
		error("Out of memory", 3);
	ptr = p;
	while (*expr) {
		*p++ = '\\';
		*p++ = *expr++;
	}
	*p = '\0';
	return ptr;
}

/**********************************************************************
 *
 *	str_toupper
 */
static void str_toupper(REG1 uchar* s)
{
	REG2 unsigned ch;
	
	for (; (ch = *s) != 0; s++) {
		if (IS_LOWER(ch))
			*s = TO_UPPER(ch);
	}
}

/**********************************************************************
 *
 *	str_equal
 *
 * Compares strings, ignoring the case of ASCII letters if ignorecase.
 */
static bool str_equal(REG1 char* s1, REG2 char* s2, int ignorecase)
{
	if (!ignorecase)
		return strcmp(s1, s2) == 0;
	for (; *s1 && TO_UPPER(*s1) == TO_UPPER(*s2); s1++, s2++)
		;
	return *s1 == *s2;
}

/**********************************************************************
 *
 *	count_eol
 *
 * Returns the number of EOL2 characters in p .. end-1. With SSE2,
 * 16 bytes are compared at a time and the matches are added to byte
 * counters, which are summed every 255 blocks.
 */
static ulong count_eol(char* p, char* end)
{
	REG1 uchar*	s = (uchar*)p;
	REG2 uchar*	e = (uchar*)end;
	ulong		n = 0;
#ifdef USE_SSE2
	const __m128i	eol = _mm_set1_epi8(EOL2);
	const __m128i	zero = _mm_setzero_si128();
	__m128i		acc;
	int		i;

	while (e - s >= 16) {
		acc = zero;
		for (i = 0; i < 255 && e - s >= 16; i++, s += 16)
			acc = _mm_sub_epi8(acc, _mm_cmpeq_epi8(
				_mm_loadu_si128((const __m128i*)s), eol));
		acc = _mm_sad_epu8(acc, zero);
		n += (ulong)_mm_cvtsi128_si32(acc) + _mm_extract_epi16(acc, 4);
	}
#endif
	for (; s < e; s++)
		n += *s == EOL2;
	return n;
}

/**********************************************************************
 *
 *	d_memrchr
 */
static char* d_memrchr(REG1 char* b, REG3 char c, REG2 unsigned n)
{
	for (; n--; b--) {
		if (*b == c)
			return b;
	}
	return NULL;
}

/**********************************************************************
 *
 *	out_mem
 *
 * Adds len bytes to the output of gp.
 */
static void out_mem(grep_t* gp, const char* s, size_t len)
{
	REG1 out_t*	o = &gp->out;

	if (o->len + len > o->size) {
		size_t	size = max(2 * o->size, o->len + len);

		if ((o->buf = realloc(o->buf, size)) == NULL)
			error("Out of memory", 3);
		o->size = size;
	}
	memcpy(o->buf + o->len, s, len);
	o->len += len;
}

#define out_str(gp, s)	out_mem((gp), (s), strlen(s))

/**********************************************************************
 *
 *	out_begin
 *
 * Starts the output of a new file.
 */
static void out_begin(grep_t* gp, int fileidx)
{
	gp->fileidx = fileidx;
	gp->out.len = 0;
	gp->out.flush_at = OUTFLUSH;
	gp->out.sep_before = FALSE;
	gp->out.groups = FALSE;
	gp->out.finished = FALSE;
}

/**********************************************************************
 *
 *	out_write
 *
 * Writes output o to stdout. If the output starts with a context
 * group and the earlier output had groups, a separator is written
 * first. With several files, lock must be held and it must be the
 * turn of the file of o.
 */
static void out_write(REG1 out_t* o)
{
	if (o->sep_before) {
		if (groups_printed)
			fputs("----------\n", stdout);
		o->sep_before = FALSE;
	}
	if (o->groups)
		groups_printed = TRUE;
	fwrite(o->buf, 1, o->len, stdout);
	o->len = 0;
	o->flush_at = OUTFLUSH;
}

/**********************************************************************
 *
 *	out_flush
 *
 * Writes the output collected so far, if it is the turn of the file.
 * Otherwise the output is kept, and we try again after OUTFLUSH more
 * bytes.
 */
static void out_flush(grep_t* gp)
{
	if (gp->fileidx < 0) {
		out_write(&gp->out);
		return;
	}
	mtx_lock(&lock);
	if (gp->fileidx == out_turn)
		out_write(&gp->out);
	else
		gp->out.flush_at = gp->out.len + OUTFLUSH;
	mtx_unlock(&lock);
}

/**********************************************************************
 *
 *	out_file_done
 *
 * Called when the file of gp is grepped. If it is the turn of the file,
 * writes its output and the waiting output of the following files that
 * are finished. Otherwise moves the output to done to wait.
 */
static void out_file_done(grep_t* gp)
{
	mtx_lock(&lock);
	if (gp->fileidx == out_turn) {
		out_write(&gp->out);
		for (out_turn++; out_turn < nfiles && done[out_turn].finished;
		     out_turn++) {
			out_write(&done[out_turn]);
			d_free(&done[out_turn].buf);
		}
	} else {
		done[gp->fileidx] = gp->out;
		done[gp->fileidx].finished = TRUE;
		memset(&gp->out, 0, sizeof(gp->out));
	}
	mtx_unlock(&lock);
}

/**********************************************************************
 *
 *	show_number
 */
static void show_number(grep_t* gp, ulong number)
{
	char	s[24];

	sprintf(s, "%s%lu", show_fname == BEFORE_LINE ? ":" : "", number);
	out_str(gp, s);
}

/**********************************************************************
 *
 *	touch_file
 */
static void touch_file(grep_t* gp)
{
	if (utime(gp->path, &touch_time) != 0) {
		fprintf(stderr, "Warning: Can't touch file %s\n", gp->path);
		gp->touch_failed = TRUE;
	}
}

/**********************************************************************
 *
 *	print_line
 */
static void print_line(REG2 grep_t* gp, char* beg, REG1 char* end, ulong lineno)
{
	dassert(*end == EOL2);

	if (end > beg && end[-1] == EOL1)
		end--;
	if (show_fname == BEFORE_LINE)
		out_str(gp, gp->path);
	else if (show_fname == BLOCK && !gp->fname_shown) {
		out_str(gp, "*** File ");
		out_str(gp, gp->path);
		out_str(gp, ":\n");
		gp->fname_shown = TRUE;
	}
	if (number)
		show_number(gp, lineno);
	if (show_fname == BEFORE_LINE || number)
		out_mem(gp, ":", 1);
	out_mem(gp, beg, end - beg);
	out_mem(gp, "\n", 1);
	if (gp->out.len >= gp->out.flush_at)
		out_flush(gp);
}

/**********************************************************************
 *
 *	get_leading_bytes
 */
static int get_leading_bytes(char* bufbeg, char* bufend, int* nlines)
{
	int	nfound = 0;
	char*	ptr = bufend;
	
	for (; ; ptr--) {
		if (ptr <= bufbeg) {
			*nlines = nfound;
			return bufend - bufbeg + 1;
		}
		if (*ptr == EOL2) {
			nfound++;
			if (nfound > *nlines) {
                    ptr++;
				break;
                }
		}
	}
	*nlines = nfound - 1;
	return bufend - ptr + 1;
}

/**********************************************************************
 *
 *	print_context
 *
 * Prints at most nlines lines starting from beg, but not past bufend
 * (the last EOL character that may be printed). Returns the number of
 * printed lines and sets *next to point after the last printed line.
 */
static int print_context(grep_t* gp, char* beg, char* bufend, int nlines,
			 ulong lineno, char** next)
{
	char*	ptr;
	int	nprinted = 0;

	for (ptr = beg; nprinted < nlines && ptr <= bufend; ptr++)
		if (*ptr == EOL2) {
			print_line(gp, beg, ptr, lineno++);
			nprinted++;
			beg = ptr + 1;
		}
	*next = beg;
	return nprinted;
}

/**********************************************************************
 *
 *	print_trailing_context
 *
 * Prints the trailing context lines of the previous match that are
 * still waiting, but only lines before limit.
 */
static void print_trailing_context(REG1 grep_t* gp, char* limit)
{
	char*	beg;
	int	n;

	if (gp->trailing_left == 0)
		return;
	beg = BUFPTR(gp->printed_offset);
	if (beg < (char*)gp->buffer)
		beg = (char*)gp->buffer;
	n = print_context(gp, beg, limit - 1, gp->trailing_left,
		gp->trailing_lineno, &beg);
	gp->trailing_left -= n;
	gp->trailing_lineno += n;
	gp->printed_offset = OFFSET(beg);
}

/**********************************************************************
 *
 *	leading_context_beg
 *
 * Returns the beginning of the leading context of the line starting
 * at beg and sets *nlines to the number of the context lines. The
 * context does not include lines that are already printed.
 */
static char* leading_context_beg(grep_t* gp, char* beg, int* nlines)
{
	char*	limit = (char*)gp->buffer;	/* first line in buffer */

	if (gp->printed_offset > OFFSET(limit))
		limit = BUFPTR(gp->printed_offset);
	for (*nlines = 0; *nlines < leading_context && beg > limit; ++*nlines) {
		beg--;	/* EOL of the previous line */
		while (beg > limit && beg[-1] != EOL2)
			beg--;
	}
	return beg;
}

/**********************************************************************
 *
 *	print_match
 *
 * Prints a matching line with its context. Prints a separator line
 * between groups of lines that are not adjacent. For the first group
 * in a file, out_flush decides if a separator is needed, because it
 * depends on the output of the earlier files (except in block mode,
 * where the file name separates the groups).
 */
static void print_match(REG1 grep_t* gp, char* beg, char* end)
{
	char*	ctxbeg;
	int	nlines;

	if (!leading_context && !trailing_context) {
		print_line(gp, beg, end, gp->linecount);
		return;
	}
	print_trailing_context(gp, beg);
	ctxbeg = leading_context_beg(gp, beg, &nlines);
	if (gp->printed_offset >= 0) {
		if (OFFSET(ctxbeg) != gp->printed_offset)
			out_str(gp, "----------\n");
	} else if (show_fname != BLOCK)
		gp->out.sep_before = TRUE;
	gp->out.groups = TRUE;
	print_context(gp, ctxbeg, beg - 1, nlines, gp->linecount - nlines,
		&ctxbeg);
	print_line(gp, beg, end, gp->linecount);
	gp->printed_offset = OFFSET(end + 1);
	gp->trailing_left = trailing_context;
	gp->trailing_lineno = gp->linecount + 1;
}

/**********************************************************************
 *
 *	check_flags
 */
static status_t check_flags(REG1 grep_t* gp, char* beg, char* end)
{
	gp->was_match = TRUE;
	if (touch)
		touch_file(gp);
	if (names) {
		if (!silent) {
			out_str(gp, gp->path);
			out_mem(gp, "\n", 1);
		}
		return STOP;
	}
	if (count) {
		gp->matchcount++;
		return OK;
	}
	if (gp->binary && binary_files == BIN_MATCHES) {
		if (!silent) {
			out_str(gp, "Binary file ");
			out_str(gp, gp->path[0] ? gp->path : "(standard input)");
			out_str(gp, " matches\n");
		}
		return STOP;
	}
	if (silent)
		return OK;
	print_match(gp, beg, end);
	if (number)
		gp->linecount++;
	return OK;
}

/**********************************************************************
 *	get_linebeg
 *
 * Returns pointer to the beginning of line ended at s.
 */
static char* get_linebeg(char* s, char* bufbeg)
{
	if (s <= bufbeg)
		return bufbeg;
	if (s[0] == EOL2)
		s--;
	s = d_memrchr(s, EOL2, s - bufbeg + 1);
	if (s == NULL)
		return bufbeg;
	return s+1;
}

/**********************************************************************
 *
 *	normal_dgrep
 *
 * When use_normal is TRUE, use this. This version separates
 * lines from buffer and then searches pattern from that line.
 */
static int normal_dgrep(REG5 grep_t* gp, char* buf, char* bufend,
			REG4 int bufsize)
{
	REG3 char*	line = buf;	/* current line */
	REG2 char*	le;		/* line end */
	REG1 char*	match;
	bool		use_must = regmust != NULL;
	int		nhits = 0;

	while ((le = memchr(line, EOL2, bufsize)) != NULL) {
		if (use_must) {
			match = boyer_moore(bm, line, le);
			if (match != NULL && !regmust_only) {
				match = reg_exec(gp->rb, line, le, NULL);
				if (++nhits == MUSTCHECK && MUST_TOO_COMMON(line, buf))
					use_must = FALSE;
			}
		} else
			match = reg_exec(gp->rb, line, le, NULL);
		if ((match && !nonmatch) || (!match && nonmatch)) {
			if (check_flags(gp, line, le) == STOP)
				return -1;
		} else
			gp->linecount++;
		le++;
		if ((bufsize -= (le-line)) <= 0)
			return 0;
		line = le;
	}
	return bufsize;
}

/**********************************************************************
 *
 *	fast_dgrep
 *
 * Searches pattern from the whole buffer, and when a match is found,
 * makes a line from the match position. With -n, the lines before
 * the matching line are counted only when a match is found.
 */
static int fast_dgrep(REG4 grep_t* gp, char* buf, REG3 char* bufend,
		      int bufsize)
{
	REG1 char*	matchptr = buf;
	REG2 char*	linebeg;	/* beginning of current line */
	bool		use_must = regmust != NULL;
	bool		verify;		/* must verify match with dfa */
	int		nhits = 0;
	char*		counted = buf;	/* lines before this are counted */

	for (;;) {
		if (use_must) {
			matchptr = boyer_moore(bm, matchptr, bufend);
			verify = !regmust_only;
			if (verify && ++nhits == MUSTCHECK && matchptr != NULL
			    && MUST_TOO_COMMON(matchptr, buf))
				use_must = FALSE;
		} else {
			matchptr = reg_exec(gp->rb, matchptr, bufend, NULL);
			verify = FALSE;
		}
		if (matchptr == NULL)
			break;
		linebeg = get_linebeg(matchptr, buf);
		/* below matchptr points end of line */
		matchptr = memchr(matchptr, EOL2, bufend-matchptr);
		if (matchptr == NULL)
			matchptr = bufend;
		if (!verify || reg_exec(gp->rb, linebeg, matchptr, NULL) != NULL) {
			if (number) {
				/* check_flags counts the matching line */
				gp->linecount += count_eol(counted, linebeg);
				counted = matchptr + 1;
			}
			if (check_flags(gp, linebeg, matchptr) == STOP)
				return -1;
		}
		matchptr++;
		if (matchptr > bufend)
			break;
	}
	if (number)
		gp->linecount += count_eol(counted, bufend + 1);
	return (buf+bufsize)-(bufend+NEOL(bufend));
}

/**********************************************************************
 *
 *	dgrep_buffer
 */
static int dgrep_buffer(grep_t* gp, char* buf, int bufsize)
{
	char*	bufend;
	int	nleftover;

	/* bufend is last EOL character in buffer */
	if ((bufend=d_memrchr(buf+bufsize-1, EOL2, bufsize)) == NULL) {
		return bufsize;
	}
	nleftover = use_normal ? normal_dgrep(gp, buf, bufend, bufsize)
			       : fast_dgrep(gp, buf, bufend, bufsize);
	if (nleftover >= 0)
		print_trailing_context(gp, bufend + 1);
	return nleftover;
}

#if 0 /* Pete removed because this is not used anymore ! */
/**********************************************************************
 *
 *	add_last_newline_if
 *
 * Checks that buffer ends with an EOL character. If it doesn't, adds one
 * at the end of buffer and returns 1. Otherwise returns 0. Routine
 * assemes that there is enough space for the added EOL character and
 * that bufsize is at least one.
 */
static int add_last_newline_if(char* buffer, int bufsize)
{
	dassert(bufsize > 0);

	if (buffer[bufsize-1] == EOL2) {
		return 0;
	} else {
		buffer[bufsize] = EOL2;
		return 1;
	}
}
#endif

/**********************************************************************
 *
 *	align
 *
 * If the buffer size is larger than ALIGN, align it. Using alignment we
 * can always read full I/O device blocks which potentially makes reading
 * faster. If ALIGN is 1 then no alignment is done.
 */
static int align(int bufsize)
{
	if (bufsize < ALIGN)
		return bufsize;
	else
		return (bufsize / ALIGN) * ALIGN;
}

/**********************************************************************
 *
 *	check_binary
 *
 * Checks if the newly read data p .. p+n-1 contains a NUL byte, which
 * makes the file binary. In a binary file NULs are changed to EOLs if
 * only "Binary file x matches" is printed: lines stay short and the
 * text between NULs is still searched. Returns STOP if the file is
 * skipped (-I).
 */
static status_t check_binary(grep_t* gp, char* p, int n)
{
	char*	end = p + n;

	if (!gp->binary) {
		if (memchr(p, '\0', n) == NULL)
			return OK;
		gp->binary = TRUE;
		if (binary_files == BIN_SKIP)
			return STOP;
	}
	if (zap_nul)
		while ((p = memchr(p, '\0', end - p)) != NULL)
			*p++ = EOL2;
	return OK;
}

/**********************************************************************
 *
 *	grow_buffer
 *
 * Doubles the input buffer size, up to maxbuf. Returns the new buffer.
 */
static char* grow_buffer(grep_t* gp)
{
	unsigned	size;
	uchar*		p;

	size = gp->bufsize > maxbuf / 2 ? maxbuf : 2 * gp->bufsize;
	if ((p = realloc(gp->buffer, size + 1)) == NULL)
		error("Out of memory when allocating input buffer", 3);
	gp->buffer = p;
	gp->bufsize = size;
	return (char*)p;
}

/**********************************************************************
 *
 *	dgrep
 *
 * Greps previously opened handle h.
 */
static void dgrep(REG4 grep_t* gp, int h)
{
	REG1 int	bufsize;
	REG2 int	nleftover = 0;
	REG3 int	leading_bytes = 0;
	char*		buffer = (char*)gp->buffer;
	int		nlines;
	int		nread;

	gp->linecount = 1L;	/* first line number is 1 */
	gp->matchcount = 0L;
	gp->buf_offset = 0;
	gp->printed_offset = -1;
	gp->trailing_left = 0;
	if (verbose && show_fname != NONE) {
		out_str(gp, "*** File ");
		out_str(gp, gp->path);
		out_str(gp, ":\n");
		gp->fname_shown = TRUE;
	} else
		gp->fname_shown = FALSE;
	gp->binary = FALSE;
	nread = align(gp->bufsize);
	while ((bufsize = read(h, buffer+leading_bytes+nleftover, nread)) > 0)
	{
		if (binary_files != BIN_TEXT)
			if (check_binary(gp, buffer+leading_bytes+nleftover, bufsize)
			    == STOP)
				break;
		bufsize += nleftover;
		nleftover = dgrep_buffer(gp, buffer+leading_bytes, bufsize);
		if (nleftover < 0) {
			break;
		} else if (nleftover == bufsize) {
			/* no EOL in buffer, make room for more */
			if (gp->bufsize-leading_bytes-nleftover < ALIGN
			    && gp->bufsize < maxbuf)
				buffer = grow_buffer(gp);
			if (align(gp->bufsize-leading_bytes-nleftover) == 0
			    && leading_bytes > 0) {
				memmove(buffer, buffer+leading_bytes, nleftover);
				gp->buf_offset += leading_bytes;
				leading_bytes = 0;
			}
			if (align(gp->bufsize-leading_bytes-nleftover) == 0) {
				fprintf(stderr, "Warning: No line separator in buffer");
				if (show_fname != NONE)
					fprintf(stderr, " in file %s", gp->path);
				fprintf(stderr, "\n");
				gp->buf_offset += nleftover;
				nleftover = 0;
			}
		} else {
			/* keep leading context lines and the partial last
			   line, move them to the beginning of buffer */
			bufsize += leading_bytes;
			leading_bytes = 0;
			if (leading_context) {
				nlines = leading_context;
				leading_bytes = get_leading_bytes(buffer,
					buffer+bufsize-nleftover-1, &nlines);
				/* leave at least half of the buffer for
				   reading, drop context only if needed */
				while (leading_bytes + nleftover > gp->bufsize / 2
				       && gp->bufsize < maxbuf)
					buffer = grow_buffer(gp);
				if (leading_bytes + nleftover > gp->bufsize / 2)
					leading_bytes = 0;
			}
			gp->buf_offset += bufsize-leading_bytes-nleftover;
			memmove(buffer, buffer+bufsize-leading_bytes-nleftover,
				leading_bytes+nleftover);
		}
		nread = align(gp->bufsize-leading_bytes-nleftover);
	}
	if (nleftover > 0) {
		/* last line without EOL, buffer has room for one more byte */
		buffer[leading_bytes+nleftover] = EOL2;
		dgrep_buffer(gp, buffer+leading_bytes, nleftover+1);
	}
	if (!silent && count && !names) {
		if (show_fname != NONE)
			out_str(gp, gp->path);
		show_number(gp, gp->matchcount);
		out_mem(gp, "\n", 1);
	}
}

/**********************************************************************
 *
 *	dgrep_file
 *
 * Greps file defined in src.
 */
static void dgrep_file(REG3 grep_t* gp, REG1 char* src)
{
	REG2 int h;	/* file handle */
	char*	fullpath = gp->fullpath;

	gp->path = src;	/* for filename display */
	if (path_mode && !IS_ABSPATH(src)
	    && getcwd(fullpath, sizeof(gp->fullpath)) != NULL) {
		size_t len = strlen(fullpath);

		if (len + strlen(src) + 2 <= sizeof(gp->fullpath)) {
			if (len > 0 && fullpath[len-1] != PATHSEP)
				fullpath[len++] = PATHSEP;
			strcpy(fullpath + len, src);
			gp->path = fullpath;
		}
	}
	if ((h=open(src,O_RDONLY|O_BINARY)) != ERROR) {
		dgrep(gp, h);
		close(h);
	} else {
		/* fprintf(stderr, "Warning: Can't open file %s\n", src); */
		gp->file_not_found = TRUE;
	}
}

/**********************************************************************
 *
 *	grep_new
 *
 * Allocates a new grep_t with its own input buffer and compiled
 * expression.
 */
static grep_t* grep_new(void)
{
	grep_t*	gp;
	char*	s;

	if ((gp = calloc(1, sizeof(grep_t))) == NULL)
		error("Out of memory", 3);
	gp->bufsize = min(INITBUF, maxbuf);
	gp->buffer = alloc_buffer(gp->bufsize);
	gp->path = "";
	if (!exact) {
		char eolbuf[2];

		eolbuf[0] = EOL1;
		eolbuf[1] = EOL2;
		gp->rb = reg_alloc();
		s = reg_comp_eol(gp->rb, expr, eolbuf, NEOL(eolbuf), type_bits);
		if (s != NULL)
			error(s, 2);
	}
	return gp;
}

/**********************************************************************
 *
 *	worker
 *
 * Greps the next file from files until there are no more files.
 */
static int worker(void* arg)
{
	grep_t*	gp = arg;
	int	i;

	for (;;) {
		mtx_lock(&lock);
		i = next_file++;
		mtx_unlock(&lock);
		if (i >= nfiles)
			break;
		out_begin(gp, i);
		dgrep_file(gp, files[i]);
		out_file_done(gp);
	}
	return 0;
}

/**********************************************************************
 *
 *	grep_files
 *
 * Greps n files in parallel threads. The calling thread is one of
 * them and uses gp. The results of all threads are combined to gp.
 */
static void grep_files(grep_t* gp, char** names, int n)
{
	grep_t*	gps[MAXTHREADS];
	thrd_t	threads[MAXTHREADS];
	int	nthr;
	int	i;

	files = names;
	nfiles = n;
	if ((done = calloc(n, sizeof(out_t))) == NULL)
		error("Out of memory", 3);
	if (mtx_init(&lock, mtx_plain) != thrd_success)
		error("Can't create mutex", 3);
	nthr = nthreads > 0 ? nthreads : min(cpu_count(), DEFTHREADS);
	nthr = min(nthr, n);
	/* compile the expressions before starting threads, because
	   compiling is not reentrant */
	gps[0] = gp;
	for (i = 1; i < nthr; i++)
		gps[i] = grep_new();
	for (i = 1; i < nthr; i++)
		if (thrd_create(&threads[i], worker, gps[i]) != thrd_success)
			error("Can't create thread", 3);
	worker(gp);
	for (i = 1; i < nthr; i++) {
		thrd_join(threads[i], NULL);
		gp->was_match |= gps[i]->was_match;
		gp->file_not_found |= gps[i]->file_not_found;
		gp->touch_failed |= gps[i]->touch_failed;
	}
}

/**********************************************************************
 *
 *	read_exp
 *
 * Reads an expression from a file.
 */
static char* read_exp(REG1 char* str)
{
	REG2 int	bufsize;
	REG3 int	h, len;
	char*		buf = (char*)alloc_buffer(maxbuf);

	if ((h = open(str, O_RDONLY)) == ERROR)
		error("Can't open expression file",2);
	if ((bufsize = read(h, buf, maxbuf)) == 0)
		error("Empty expression file",2);
	if (bufsize == ERROR)
		error("Error when reading expression file",3);
	close(h);
	buf[bufsize] = '\0';
	if ((str = memchr(buf,'\n',bufsize)) != NULL)
		*str = '\0';
	len = strlen(buf);
	if (len == 0)
		error("Empty expression in expression file",2);
	if ((str = malloc(len+1)) == NULL)
		error("Out of memory",3);
	strcpy(str, buf);
	free(buf);
	return str;
}

/**********************************************************************
 *
 *	get_args
 */
static int get_args(REG1 int argc, char* argv[])
{
	REG2 int	opt;

	d_opterr = FALSE;	/* handle errors ourselves */
	while ((opt = d_getopt(argc, argv, "abdchilnoprstvxzI123456789A:B:e:f:j:D")) != EOF) {
		switch (opt) {
			case 'j':
				nthreads = atoi(d_optarg);
				if (nthreads < 1 || nthreads > MAXTHREADS)
					error("Number of threads must be "
					      "1-64", 2);
				break;
			case 'A':
				trailing_context = (unsigned)atoi(d_optarg);
				break;
			case 'B':
				leading_context = (unsigned)atoi(d_optarg);
				break;
			case 'b':
				block_mode = TRUE;
				break;
			case 'd':
				no_boymoore = TRUE;
				break;
			case 'c':
				count = TRUE;
				break;
			case 'h':
				long_usage();	/* and exit */
			case 'i':
				ignorecase = TRUE;
				type_bits |= RE_IGNORECASE;
				break;
			case 'l':
				names = TRUE;
				break;
			case 'n':
				number = TRUE;
				break;
			case 'p':
				path_mode = TRUE;
				break;
			case 'r':
				recursive = TRUE;
				break;
			case 'a':
				binary_files = BIN_TEXT;
				break;
			case 'I':
				binary_files = BIN_SKIP;
				break;
			case 's':
				silent = TRUE;
				break;
			case 't':
				touch = names = TRUE;
				break;
			case 'v':
				nonmatch = TRUE;
				break;
			case 'x':
				exact = TRUE;
				break;
			case 'z':
				verbose = TRUE;
				block_mode = TRUE;
				break;
			case 'e':
				expr = d_optarg;
				break;
			case 'f':
				expr = read_exp(d_optarg);
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				leading_context = trailing_context = opt - '0';
				break;
#ifdef TEST
			case 'D':
				debug++;
				break;
#endif
			default:
				error("Invalid command line option, "
					"dgrep -h for help", 2);
		}
	}
	if (count || names || silent || touch) {
		/* no lines are printed */
		leading_context = trailing_context = 0;
		number = FALSE;
	}
	if (names || touch || silent) {
		block_mode = FALSE;
		verbose = FALSE;
	}
	return d_optind;
}

/**********************************************************************
 *
 *	add_env_args
 *
 * Inserts the white space separated options from env after argv[0].
 * Returns the new argv and updates argc.
 */
static char** add_env_args(char* env, int* argc, char* argv[])
{
	char**	newargv;
	char*	s;
	int	nenv = 0;
	int	i;

	if ((s = malloc(strlen(env) + 1)) == NULL)
		error("Out of memory", 3);
	env = strcpy(s, env);
	for (s = env; *s; ) {
		while (*s == ' ' || *s == '\t')
			s++;
		if (*s == '\0')
			break;
		nenv++;
		while (*s && *s != ' ' && *s != '\t')
			s++;
	}
	newargv = malloc(sizeof(argv[0]) * (*argc + nenv + 1));
	if (newargv == NULL)
		error("Out of memory", 3);
	newargv[0] = argv[0];
	for (i = 1, s = env; i <= nenv; i++) {
		while (*s == ' ' || *s == '\t')
			s++;
		newargv[i] = s;
		while (*s && *s != ' ' && *s != '\t')
			s++;
		if (*s)
			*s++ = '\0';
	}
	memcpy(&newargv[nenv + 1], &argv[1], sizeof(argv[0]) * *argc);
	*argc += nenv;
	return newargv;
}

/**********************************************************************
 *
 *	main
 */
int main(int argc, char* argv[])
{
	REG2 int	optind;
	REG3 char*	s;
	REG4 int	len;
	int		nfile;
	grep_t*		gp;

	if (argc == 1)
		short_usage();
	s = getenv("DGREP");
	if (s != NULL)
		argv = add_env_args(s, &argc, argv);
	optind = get_args(argc, argv);
	if (optind >= argc && !expr)
		short_usage();
	if (!expr)
		expr = argv[optind++];
	len = strlen(expr);
	if (exact && (no_boymoore || len > MAXREGMUST || len == 0)) {
		/* Bm can handle only MAXREGMUST long patterns with
		   N+M worst case. So we verify match with reg_exec,
		   which does it in N+M time. (We use bm with the
		   regmust pattern from reg_comp.) Bm can't search
		   an empty pattern. */
		expr = quote_expr(expr, len);
		exact = FALSE;
	}
	gp = grep_new();	/* compiles expr if not exact */
	if (!no_boymoore)
		regmust = exact ? expr : gp->rb->regmust;
	if (regmust) {
		/* expr is not changed, it is compiled again for each grep_t */
		if (ignorecase)
			str_toupper((uchar*)regmust);
		regmust_only = regmust == expr || str_equal(regmust, expr,
							     ignorecase);
		regmustlen = strlen(regmust);
		bm = gosper(regmust, regmustlen, ignorecase);
		/* boyer_moore uses memchr for one character */
		must_gap = regmustlen == 1
			   && !(ignorecase && IS_ALPHA(*regmust))
			   ? MUSTGAP_MEMCHR : MUSTGAP_BM;
	}
	use_normal = nonmatch;
	zap_nul = binary_files == BIN_MATCHES && !count && !nonmatch;
	if (touch)
		touch_time.actime = touch_time.modtime = time(NULL);
	nfile = argc - optind;
	if (nfile > 0 || recursive) {
		if (block_mode)
			show_fname = BLOCK;
		else
			show_fname = BEFORE_LINE;
		if (recursive) {
			filelist_t	fl = { NULL, 0, 0 };

			/* without files search the current directory */
			if (nfile == 0 && !walk_path(&fl, ""))
				gp->file_not_found = TRUE;
			for (; optind < argc; optind++)
				if (!walk_path(&fl, argv[optind]))
					gp->file_not_found = TRUE;
			if (fl.n > 0)
				grep_files(gp, fl.names, fl.n);
		} else
			grep_files(gp, &argv[optind], nfile);
	} else {
		out_begin(gp, -1);
		dgrep(gp, fileno(stdin));
		out_flush(gp);
	}
	fflush(stdout);
#ifdef TEST
	if (debug) {
		printf("maxbuf = %d\n", maxbuf);
		if (gp->rb != NULL)
			show_dfa_report(gp->rb);
		show_boyer_moore_report(bm);
		fflush(stdout);
	}
#endif
	return (gp->file_not_found || gp->touch_failed) ? 2
		: gp->was_match ? 0 : 1;
}
