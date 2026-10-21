/**********************************************************************\
 *
 *	GETOPT.H
 *
 * Portable getopt(3). Prefixed names so that it does not conflict
 * with the system getopt.
 *
\**********************************************************************/

extern int	d_optind;	/* index of which argument is next */
extern char*	d_optarg;	/* pointer to argument of current option */
extern int	d_opterr;	/* print error messages if nonzero */

extern int	d_getopt(int argc, char* argv[], const char* optstring);
