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

/* flags and local variables */

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
static char     path_mode   = FALSE;
static char		verbose		= FALSE;
static int		leading_context	= 0;
static int		trailing_context= 0;

static ulong		matchcount;
static ulong		linecount;
static ulong*		linecount_ptr	= NULL;

/* Context printing state. Offsets are file offsets, so they stay
   valid when the buffer contents are moved. */
static long long	buf_offset;	/* file offset of buffer[0] */
static long long	printed_offset;	/* offset after last printed line, -1 if none */
static int		trailing_left;	/* trailing context lines to print */
static ulong		trailing_lineno;/* line number of next trailing line */

#define OFFSET(p)	(buf_offset + ((uchar*)(p) - buffer))
#define BUFPTR(off)	((char*)buffer + ((off) - buf_offset))

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

static int		must_gap;
static int		type_bits	= RE_SEARCH;
static char		file_not_found	= FALSE;
static char		touch_failed	= FALSE;
static char		was_match	= FALSE;
static show_fname_t	show_fname	= NONE;
static char		fname_shown	= FALSE;
static char		first_match	= TRUE;
static char*		path		= "";	/* current path and filename */
static char*		expr		= NULL;
static regbuf_t*	rb;		/* compiled expression */
static char*		regmust	= NULL;
static char     fullpath[2048];
static int		regmustlen;
static char		use_normal;

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
      "\t -b   filename is displayed only once before matches\n"
      "\t -c   only a count of matching lines is printed\n"
      "\t -d   only dfa is used for searching\n"
      "\t -i   case insensitive match\n"
      "\t -l   only names of files with matching lines are printed\n"
      "\t -n   each line is preceded by its relative line number in file\n"
	  "\t -p   filename is printed with full path\n"
      "\t -s   silent mode, nothing is printed except error messages\n"
};

/* split options for MSC 5.X */
static char* options2 = {
      "\t -t   all files that contain matches are touched\n"
      "\t -v   all lines but those matching are printed\n"
      "\t -x   exact, all characters in expression are taken literally\n"
      "\t -z   verbose\n"
      "\t -1-9 1-9 lines before and after the matching line are printed\n"
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
 *	show_number
 */
static void show_number(ulong number)
{
	printf("%s%lu", show_fname == BEFORE_LINE ? ":" : "", number);
}

/**********************************************************************
 *
 *	touch_file
 */
static void touch_file(char* fname)
{
	static struct utimbuf	touch_time;
	static int	first_time = TRUE;
	
	if (first_time) {
		first_time = FALSE;
		touch_time.actime = touch_time.modtime = time(NULL);
	}
	if (utime(fname, &touch_time) != 0) {
		fprintf(stderr, "Warning: Can't touch file %s\n", fname);
		touch_failed = TRUE;
	}
}

/**********************************************************************
 *
 *	print_line
 */
static void print_line(char* beg, REG1 char* end, ulong lineno)
{
	REG3 int	c;

	dassert(*end == EOL2);
	
	if (end > beg && end[-1] == EOL1)
		end--;
	c = *end;
	*end = '\0';
	if (show_fname == BEFORE_LINE)
		fputs(path, stdout);
	else if (show_fname == BLOCK && !fname_shown) {
		printf("*** File %s:\n", path);
		fname_shown = TRUE;
	}
	if (number)
		show_number(lineno);
	if (show_fname == BEFORE_LINE || number)
		printf(":");
	printf("%s\n", beg);
	*end = c;
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
static int print_context(char* beg, char* bufend, int nlines, ulong lineno,
			 char** next)
{
	char*	ptr;
	int	nprinted = 0;

	for (ptr = beg; nprinted < nlines && ptr <= bufend; ptr++)
		if (*ptr == EOL2) {
			print_line(beg, ptr, lineno++);
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
static void print_trailing_context(char* limit)
{
	char*	beg;
	int	n;

	if (trailing_left == 0)
		return;
	beg = BUFPTR(printed_offset);
	if (beg < (char*)buffer)
		beg = (char*)buffer;
	n = print_context(beg, limit - 1, trailing_left, trailing_lineno, &beg);
	trailing_left -= n;
	trailing_lineno += n;
	printed_offset = OFFSET(beg);
}

/**********************************************************************
 *
 *	leading_context_beg
 *
 * Returns the beginning of the leading context of the line starting
 * at beg and sets *nlines to the number of the context lines. The
 * context does not include lines that are already printed.
 */
static char* leading_context_beg(char* beg, int* nlines)
{
	char*	limit = (char*)buffer;	/* first line in buffer */

	if (printed_offset > OFFSET(limit))
		limit = BUFPTR(printed_offset);
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
 * between groups of lines that are not adjacent.
 */
static void print_match(char* beg, char* end)
{
	char*	ctxbeg;
	int	nlines;

	if (!leading_context && !trailing_context) {
		print_line(beg, end, linecount);
		return;
	}
	print_trailing_context(beg);
	ctxbeg = leading_context_beg(beg, &nlines);
	if (!first_match && OFFSET(ctxbeg) != printed_offset)
		printf("----------\n");
	first_match = FALSE;
	print_context(ctxbeg, beg - 1, nlines, linecount - nlines, &ctxbeg);
	print_line(beg, end, linecount);
	printed_offset = OFFSET(end + 1);
	trailing_left = trailing_context;
	trailing_lineno = linecount + 1;
}

/**********************************************************************
 *
 *	check_flags
 */
static status_t check_flags(char* beg, char* end)
{
	was_match = TRUE;
	if (touch)
		touch_file(path);
	if (names) {
		if (!silent)
			printf("%s\n", path);
		return STOP;
	}
	if (count) {
		matchcount++;
		return OK;
	}
	if (silent)
		return OK;
	print_match(beg, end);
	if (number)
		linecount++;
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
static int normal_dgrep(char* buf, char* bufend, REG4 int bufsize)
{
	REG3 char*	line = buf;	/* current line */
	REG2 char*	le;		/* line end */
	REG1 char*	match;
	bool		use_must = regmust != NULL;
	int		nhits = 0;

	while ((le = memchr(line, EOL2, bufsize)) != NULL) {
		if (use_must) {
			match = boyer_moore(line,le,regmust,regmustlen);
			if (match != NULL && !regmust_only) {
				match = reg_exec(rb, line, le, NULL);
				if (++nhits == MUSTCHECK && MUST_TOO_COMMON(line, buf))
					use_must = FALSE;
			}
		} else
			match = reg_exec(rb, line, le, NULL);
		if ((match && !nonmatch) || (!match && nonmatch)) {
			if (check_flags(line, le) == STOP)
				return -1;
		} else
			linecount++;
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
 * makes a line from the match position.
 */
static int fast_dgrep(char* buf, REG3 char* bufend, int bufsize)
{
	REG1 char*	matchptr = buf;
	REG2 char*	linebeg;	/* beginning of current line */
	bool		use_must = regmust != NULL;
	bool		verify;		/* must verify match with dfa */
	int		nhits = 0;

	for (;;) {
		if (use_must) {
			matchptr = boyer_moore(matchptr,bufend,regmust,regmustlen);
			verify = !regmust_only;
			if (verify && ++nhits == MUSTCHECK && matchptr != NULL
			    && MUST_TOO_COMMON(matchptr, buf))
				use_must = FALSE;
		} else {
			matchptr = reg_exec(rb, matchptr, bufend, linecount_ptr);
			verify = FALSE;
		}
		if (matchptr == NULL)
			break;
		linebeg = get_linebeg(matchptr, buf);
		/* below matchptr points end of line */
		matchptr = memchr(matchptr, EOL2, bufend-matchptr);
		if (matchptr == NULL)
			matchptr = bufend;
		if (!verify || reg_exec(rb, linebeg, matchptr, NULL) != NULL)
			if (check_flags(linebeg, matchptr) == STOP)
				return -1;
		matchptr++;
		if (matchptr > bufend)
			break;
	}
	return (buf+bufsize)-(bufend+NEOL(bufend));
}

/**********************************************************************
 *
 *	dgrep_buffer
 */
static int dgrep_buffer(char* buf, int bufsize)
{
	char*	bufend;
	int	nleftover;

	/* bufend is last EOL character in buffer */
	if ((bufend=d_memrchr(buf+bufsize-1, EOL2, bufsize)) == NULL) {
		return bufsize;
	}
	nleftover = use_normal ? normal_dgrep(buf, bufend, bufsize)
			       : fast_dgrep(buf, bufend, bufsize);
	if (nleftover >= 0)
		print_trailing_context(bufend + 1);
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
 *	dgrep
 *
 * Greps previously opened handle h.
 */
static void dgrep(int h)
{
	REG1 int	bufsize;
	REG2 int	nleftover = 0;
	REG3 int	leading_bytes = 0;
	int		nlines;
	int		nread;

	linecount = 1L;		/* first line number is 1 */
	matchcount = 0L;
	buf_offset = 0;
	printed_offset = -1;
	trailing_left = 0;
	if (verbose && show_fname != NONE) {
		printf("*** File %s:\n", path);
		fname_shown = TRUE;
	} else
		fname_shown = FALSE;
	if (show_fname == BLOCK)	/* reset context match flag */
		first_match = TRUE;
	nread = align(maxbuf);
	while ((bufsize = read(h, buffer+leading_bytes+nleftover, nread)) > 0)
	{
		bufsize += nleftover;
		nleftover = dgrep_buffer(buffer+leading_bytes, bufsize);
		if (nleftover < 0) {
			break;
		} else if (nleftover == bufsize) {
			/* no EOL in buffer, make room for more */
			if (align(maxbuf-leading_bytes-nleftover) == 0
			    && leading_bytes > 0) {
				memmove(buffer, buffer+leading_bytes, nleftover);
				buf_offset += leading_bytes;
				leading_bytes = 0;
			}
			if (align(maxbuf-leading_bytes-nleftover) == 0) {
				fprintf(stderr, "Warning: No line separator in buffer");
				if (show_fname != NONE)
					fprintf(stderr, " in file %s", path);
				fprintf(stderr, "\n");
				buf_offset += nleftover;
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
				if (leading_bytes + nleftover > maxbuf / 2)
					leading_bytes = 0;
			}
			buf_offset += bufsize-leading_bytes-nleftover;
			memmove(buffer, buffer+bufsize-leading_bytes-nleftover,
				leading_bytes+nleftover);
		}
		nread = align(maxbuf-leading_bytes-nleftover);
	}
	if (nleftover > 0) {
		/* last line without EOL, buffer has room for one more byte */
		buffer[leading_bytes+nleftover] = EOL2;
		dgrep_buffer(buffer+leading_bytes, nleftover+1);
	}
	if (!silent && count && !names) {
		if (show_fname != NONE)
			fputs(path, stdout);
		show_number(matchcount);
		printf("\n");
	}
}

/**********************************************************************
 *
 *	dgrep_file
 *
 * Greps file defined in src.
 */
static void dgrep_file(REG1 char* src)
{
	REG2 int h;	/* file handle */

	path = src;	/* for filename display */
	if (path_mode && !IS_ABSPATH(src)
	    && getcwd(fullpath, sizeof(fullpath)) != NULL) {
		size_t len = strlen(fullpath);

		if (len + strlen(src) + 2 <= sizeof(fullpath)) {
			if (len > 0 && fullpath[len-1] != PATHSEP)
				fullpath[len++] = PATHSEP;
			strcpy(fullpath + len, src);
			path = fullpath;
		}
	}
	if ((h=open(src,O_RDONLY|O_BINARY)) != ERROR) {
		dgrep(h);
		close(h);
	} else {
		/* fprintf(stderr, "Warning: Can't open file %s\n", src); */
		file_not_found = TRUE;
	}
}

/**********************************************************************
 *
 *	read_exp
 *
 * Reads an expression from a file.
 */
static uchar* read_exp(REG1 char* str)
{
	REG2 int	bufsize;
	REG3 int	h, len;

	if ((h = open(str, O_RDONLY)) == ERROR)
		error("Can't open expression file",2);
	if ((bufsize = read(h, buffer, maxbuf)) == 0)
		error("Empty expression file",2);
	if (bufsize == ERROR)
		error("Error when reading expression file",3);
	buffer[bufsize] = '\0';
	if ((str = memchr(buffer,'\n',bufsize)) != NULL)
		*str = '\0';
	len = strlen(buffer);
	if (len == 0)
		error("Empty expression in expression file",2);
	if ((str = malloc(len+1)) == NULL)
		error("Out of memory",3);
	return strcpy(str, buffer);
}

/**********************************************************************
 *
 *	get_args
 */
static int get_args(REG1 int argc, char* argv[])
{
	REG2 int	opt;

	d_opterr = FALSE;	/* handle errors ourselves */
	while ((opt = d_getopt(argc, argv, "bdchilnoprstvxz123456789A:B:e:f:D")) != EOF) {
		switch (opt) {
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
				linecount_ptr = &linecount;
				break;
			case 'p':
				path_mode = TRUE;
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
	if (count || names || silent || touch)
		leading_context = trailing_context = 0;
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

	if (argc == 1)
		short_usage();
	alloc_buffer();	/* Allocate I/O buffer */
	rb = reg_alloc();
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
	if (!exact) {
		char eolbuf[2];
		eolbuf[0] = EOL1;
		eolbuf[1] = EOL2;
		s = reg_comp_eol(rb, expr, eolbuf, NEOL(eolbuf), type_bits);
		if (s != NULL)
			error(s, 2);
	}
	if (!no_boymoore)
		regmust = exact ? expr : rb->regmust;
	if (regmust) {
		if (ignorecase) {
			str_toupper(regmust);
			str_toupper(expr);
		}
		regmust_only = (strcmp(regmust,expr) == 0);
		regmustlen = strlen(regmust);
		gosper(regmust, regmustlen, ignorecase);
		/* boyer_moore uses memchr for one character */
		must_gap = regmustlen == 1
			   && !(ignorecase && IS_ALPHA(*regmust))
			   ? MUSTGAP_MEMCHR : MUSTGAP_BM;
	}
	use_normal = nonmatch || (regmust && number);
	nfile = argc - optind;
	if (nfile > 0) {
		if (block_mode)
			show_fname = BLOCK;
		else
			show_fname = BEFORE_LINE;
		while (optind < argc)
			dgrep_file(argv[optind++]);
	} else
		dgrep(fileno(stdin));
#ifdef TEST
	if (debug) {
		printf("maxbuf = %d\n", maxbuf);
		show_dfa_report(rb);
		show_boyer_moore_report();
		fflush(stdout);
	}
#endif
	return (file_not_found || touch_failed) ? 2 : was_match ? 0 : 1;
}
