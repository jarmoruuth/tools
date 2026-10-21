/**********************************************************************\
 *
 *	GETOPT.C
 *
 * Portable getopt(3), System V style. Options end at the first
 * argument that does not start with '-', at a lone "-" or after "--".
 * Unknown options and missing option arguments return '?'.
 *
\**********************************************************************/

#include <stdio.h>
#include <string.h>
#include "getopt.h"

int	d_optind = 1;
char*	d_optarg = NULL;
int	d_opterr = 1;

static char*	nextchar = NULL;	/* next option character in argv */

/**********************************************************************
 *
 *	d_getopt
 */
int d_getopt(int argc, char* argv[], const char* optstring)
{
	const char*	p;
	int		opt;

	d_optarg = NULL;
	if (nextchar == NULL || *nextchar == '\0') {
		if (d_optind >= argc || argv[d_optind] == NULL
		    || argv[d_optind][0] != '-' || argv[d_optind][1] == '\0')
			return EOF;
		if (strcmp(argv[d_optind], "--") == 0) {
			d_optind++;
			return EOF;
		}
		nextchar = argv[d_optind] + 1;
	}
	opt = (unsigned char)*nextchar++;
	p = opt == ':' ? NULL : strchr(optstring, opt);
	if (p == NULL) {
		if (*nextchar == '\0')
			d_optind++;
		if (d_opterr)
			fprintf(stderr, "%s: illegal option -- %c\n", argv[0], opt);
		return '?';
	}
	if (p[1] == ':') {
		if (*nextchar != '\0') {
			d_optarg = nextchar;
		} else if (d_optind + 1 < argc) {
			d_optarg = argv[++d_optind];
		} else {
			nextchar = NULL;
			d_optind++;
			if (d_opterr)
				fprintf(stderr, "%s: option requires an argument -- %c\n",
					argv[0], opt);
			return '?';
		}
		nextchar = NULL;
		d_optind++;
	} else if (*nextchar == '\0') {
		d_optind++;
	}
	return opt;
}
