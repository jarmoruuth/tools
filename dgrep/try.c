/* This is modified version for my own routines.
 *	J.Ruuth 16-Mar-1988
 *
 * Simple test program for regexp(3) stuff.  Knows about debugging hooks.
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 * Usage: try re [string [output [-]]]
 * The re is compiled and dumped, re_execed against the string, the result
 * is applied to output using regsub().  The - triggers a running narrative
 * from re_exec().  Dumping and narrative don't happen unless DEBUG.
 *
 * If there are no arguments, stdin is assumed to be a stream of lines with
 * five fields:  a r.e., a string to match it against, a result code, a
 * source string for regsub, and the proper result.  Result codes are 'c'
 * for compile failure, 'y' for match success, 'n' for match failure.
 * Field separator is tab.
 */

#include <stdio.h>
#include "dfaregex.h"

char buf[BUFSIZ];

int errreport = 0;		/* Report errors via errseen? */
char *errseen = NULL;		/* Error message. */
int status = 0;			/* Exit status. */
int debug = 0;
int test = 0;
int type_bits = RE_SEARCH;

main(argc, argv)
int argc;
char *argv[];
{
	int i;
	char *r;
	int		opt;
	extern int	optind, opterr;
	extern char	*optarg;

	opterr = 0;	/* handle errors ourselves */
	while ((opt = getopt(argc, argv, "sifdt")) != EOF) {
		switch (opt) {
			case 's':
				type_bits &= ~RE_SEARCH;
				break;
			case 'i':
				type_bits |= RE_IGNORECASE;
				break;
			case 'f':
				type_bits |= RE_FULL;
				break;
			case 't':
				test++;
				break;
			case 'd':
				debug++;
				break;
			default:
				usage();
		}
	}
	
	if (argc-optind == 0) {
		multiple();
		fflush(stdout);
		exit(status);
	}
	if (argc-optind != 2)
		usage();
	r = reg_comp(argv[optind++], type_bits);
	if (r != NULL)
		complain("reg_comp failure '%s'", r);
	if (argc > 2) {
		r = reg_exec(argv[optind], argv[optind]+strlen(argv[optind]), NULL);
		printf("%s\n", r);
	}
	fflush(stdout);
	exit(status);
}

usage()
{
	printf("Usage: try [-siftd] {expr str | < infile}\n");
	printf( "-s not substring searcher\n"
		"-i ignorecase\n"
		"-f full match\n"
		"-t test, display expressions\n"
		"-d debug, if files compiled with -DTEST\n");
	exit(1);
}

int lineno;

multiple()
{
	char rbuf[BUFSIZ];
	char *field[5];
	char *scan;
	int i;
	extern char *strchr();
	char *r;

	errreport = 1;
	lineno = 0;
	while (fgets(rbuf, sizeof(rbuf), stdin) != NULL) {
		rbuf[strlen(rbuf)-1] = '\0';	/* Dispense with \n. */
		++lineno;
		scan = rbuf;
		for (i = 0; i < 3; i++) {
			field[i] = scan;
			if (field[i] == NULL)
				fatal("bad testfile format", "");
			scan = strchr(scan, '\t');
			if (scan != NULL)
				*scan++ = '\0';
		}
		try(field);
	}

	printf("And finish up with some internal testing...\n");
	lineno = 9990;
	if (test)
		printf("%d: reg_comp((char *)NULL)\n", lineno);
	if (reg_comp((char *)NULL, type_bits) == NULL)
		fatal("reg_comp(NULL) doesn't complain", "");
	lineno = 9991;
	if (test)
		printf("%d: reg_comp(\"foo\")\n", lineno);
	r = reg_comp("foo", type_bits);
	if (r != NULL) {
		fatal("reg_comp(\"foo\") fails", "");
		return;
	}
	lineno = 9992;
	if (test)
		printf("%d: reg_exec((char *)NULL)\n", lineno);
	if (reg_exec((char *)NULL, NULL, NULL) != NULL)
		fatal("reg_exec(..., NULL) doesn't complain", "");
}

try(fields)
char **fields;
{
	char *r;
	char dbuf[BUFSIZ];

	if (test)
		printf("%d: %s %s %s\n", lineno,fields[0], fields[1], fields[2]);
	r = reg_comp(fields[0], type_bits);
	if (r != NULL) {
		if (!test)
			printf("%d: %s %s %s\n", lineno,fields[0], fields[1], fields[2]);
		if (*fields[2] != 'c')
			fatal("unexpected reg_comp failure  `%s'", r);
		else
			complain("reg_comp failure  `%s'", r);
		return;
	}
	if (*fields[2] == 'c') {
		if (!test)
			printf("%d: %s %s %s\n", lineno,fields[0], fields[1], fields[2]);
		fatal("unexpected reg_comp success  `%s'", r);
	}
	if (!reg_exec(fields[1], fields[2]-1, NULL)) {
		if (*fields[2] != 'n') {
			if (!test)
				printf("%d: %s %s %s\n", lineno,fields[0], fields[1], fields[2]);
			fatal("reg_exec failure in `%s'", fields[1]);
		}
#ifdef TEST
		if (debug)
			show_dfa_report();
#endif
		return;
	}
	if (*fields[2] == 'n') {
		if (!test)
			printf("%d: %s %s %s\n", lineno,fields[0], fields[1], fields[2]);
		fatal("unexpected reg_exec success", "");
		return;
	}
#ifdef TEST
	if (debug)
		show_dfa_report();
#endif
}

complain(s1, s2)
char *s1;
char *s2;
{
	printf("try: %d: ", lineno);
	printf(s1, s2);
	printf("\n");
	/*status = 1;*/
}

fatal(s1, s2)
char *s1;
char *s2;
{
	complain(s1, s2);
#ifdef TEST
	if (debug > 1) {
		print_accepting_states();
		print_dtran();
	}
#endif
	fflush(stdout);
	fflush(stderr);
	fprintf(stderr, "Abnormal program termination");
	exit(2);
}
