/*
	getopt.h - definition for getopt(3) and global variables
*/

extern int	optind;		/* index of which argument is next	*/
extern char*	optarg;		/* pointer to argument of current option */
extern int	opterr;		/* allow error message	*/

extern int	getopt(int argc, char *argv[], char *optionS);
