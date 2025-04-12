/**********************************************************************\
 *
 *	LC.C
 *
 * Counts lines from specified files.
 * If no files are given, standard input is assumed.
 *
 * Return values:
 *	0 - ok
 *	1 - no files found
 *	2 - errors
 *
 *	J.Ruuth 12-09-87
 *
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>

#include <c.h>
#include <filelist.h>
#include <getopt.h>

#define	MAXBUF	0x7fffU
#define ATTR	0		/* only ordinary files */

static int		exit_code = 0;
static unsigned long	totalcount = 0;
static unsigned int	filecount  = 0;
static int total_only = 0;

static char		buffer[MAXBUF];

#ifdef __TURBOC__

/* Use faster direct dos system calls */
#define	read	_read

unsigned _stklen = 16 * 1024;

#endif

/**********************************************************************
 *
 *	usage
 *
 * Exit with help message.
 */
static void usage(int error_code)
{
	printf(
	"Usage: lc [-rt] [file...]\n"
	"If no files are given, standard input is assumed\n"
	"Options: -r recurse subdirectories\n"
	"         -t only total line and file count\n");
	exit(error_code);
}

/**********************************************************************
 *
 *	countln
 *
 * Count line feeds in buffer of size nbytes.
 */
static unsigned countln(register char *buffer, int nbytes)
{
	char		*newbuffer;
	unsigned	count = 0;
	
	while ((newbuffer = memchr(buffer, '\n', nbytes)) != NULL) {
		count++;
		nbytes -= (newbuffer - buffer + 1);
		buffer = newbuffer + 1;
	}
	return(count);
}


/**********************************************************************
 *
 *	lc
 *
 * Count lines from open handle.
 */
static void lc(int handle)
{
	register unsigned	nbytes;
	unsigned long		linecount = 0;
	
	++filecount;
	while ((nbytes = read(handle, buffer, MAXBUF)) && (int)nbytes != ERROR)
		linecount += countln(buffer, nbytes);
	if ((int)nbytes == ERROR)
		exit_code = 2;
        if (!total_only) {
	    printf("%lu", linecount);
        }
	totalcount += linecount;
}

/**********************************************************************
 *
 *	count
 *
 * Open handle and count lines.
 */
static void count(char *p)
{
	register int	handle;
	
        if (!total_only) {
	    printf("%s:", p);
        }
	if ((handle = open(p, O_RDONLY|O_BINARY)) != ERROR) {
		lc(handle);
		close(handle);
	} else {
            if (!total_only) {
		    printf("unable to open file");
            }
		exit_code = 2;
	}
        if (!total_only) {
	    printf("\n");
        }
}

/**********************************************************************
 *
 *	count_files
 */
static void count_files(char *pat, int recurse)
{
	NAMELIST	*namelist;
	char		**nl;
		
	namelist = read_names(1, &pat, ATTR, recurse);
	nl = namelist->name;
	for (; *nl != NULL; nl++)
		count(strncmp(*nl, ".\\", 2) == 0 ? (*nl)+2 : *nl);
	free_name_list(namelist);
}

/**********************************************************************
 *
 *	main
 */
int main(int argc, char *argv[])
{
	int	recurse = 0;
	int	opt;
	
	opterr = FALSE;	/* handle errors ourselves */
	while ((opt = getopt(argc, argv, "rt")) != EOF) {
		switch (opt) {
			case 'r':
				recurse++;
				break;
			case 't':
				total_only = 1;
				break;
			default:
				usage(2);
				break;
		}
	}
	if (argc <= optind)
		lc(fileno(stdin));
	else
		while (optind < argc)
			count_files(argv[optind++], recurse);
	if (filecount == 0)
		exit_code = max(exit_code, 1);
	printf("%u files, %lu lines\n", filecount, totalcount);
	return(exit_code);
}
