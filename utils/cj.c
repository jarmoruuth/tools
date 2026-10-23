/**********************************************************************\
 *
 *	CJ.C
 *
 * Unix like file copy.
 * No wild cards are allowed in destination.
 *
 * Return values:
 *	0 - ok
 *	1 - no files copied
 *	2 - errors
 *
 *	J.Ruuth 15-09-1987
 *
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <conio.h>
#include <string.h>
#include <process.h>
#include <errno.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <sys\utime.h>

#include <c.h>
#include <getopt.h>
#include <listmenu.h>
#include <diruti.h>

#define MAXPATHLEN	U_MAXPATH

#define CTRLC		3

#define DEV		0x01
#define DIR		0x02
#define ROOT		0x04

typedef enum {
	COPIED,
	FAIL_SRC,
	FAIL_DEST,
	FULL
} copy_status_t;

static int	exit_code = 0;		/* exit() code */
static unsigned	bufsize;		/* copy buffer size */
static char	*buffer;		    /* copy buffer */
static int	overwrite = 0;		/* overwrite flag */
static int	interactive = 0;	/* interactive flag */
static int	time_copy = 0;		/* copy only over older or new files */
static int	silent = 0;		    /* silent flag */
static int	changed_copy = 0;	/* copy only changed or new files */
static int	keep_attr = 1;		/* copy src's attributes to dest */
static int	force = 0;		    /* force flag */
static int  src_fmode = O_BINARY;
static int  dest_fmode = O_BINARY;

#ifdef __TURBOC__

#include <dos.h>

#define read		_read
#define write		_write

#endif

#define MAXMEM	0x7fff
#define MAXBUF	((MAXMEM / 2048U) * 2048U)

/**********************************************************************
 *
 *	interact
 *
 * Ask user permission to move file.
 */
static bool interact(void)
{
	register int	ans;
	
	fprintf(stderr, " ? ");
	do {
		ans = getch();
		if (ans == CTRLC) {
			fprintf(stderr, "^C\n");
			exit(2);
		}
	} while (!(ans=='y' || ans=='Y' || ans=='n' || ans=='N'));
	if (ans == 'n' || ans == 'N')
		fprintf(stderr, "No\n");
	else
		fprintf(stderr, "Yes\n");
	return ans=='y' || ans=='Y';
}

/**********************************************************************
 *
 *	overwrite_file
 *
 * Ask user permission to overwrite existing file.
 */
static bool overwrite_file(char *dest)
{
	fprintf(stderr, "File %s exists, overwrite", dest);
	return interact();
}

/**********************************************************************
 *
 *	usage
 *
 * Exit with help message
 */
static void usage(int error_code)
{
	printf(
	"Usage: cj [-acfimostDU] source [source...] destination\n"
	"If more than one source, destination must be directory\n"
	"Options: -a do not copy attributes from source to destination\n"
	"         -c copy only changed or new files\n"
	"         -f force copy\n"
	"         -i interactive\n"
#ifndef NT
	"         -m use menu to select files\n"
#endif
	"         -o overwrite existing files\n"
	"         -s silent mode\n"
	"         -t copy only over older or new files\n"
	"         -D unix to Dos copy, translate lf to cr+lf\n"
	"         -U dos to Unix copy, translate cr+lf to lf\n");
	exit(error_code);
}

/**********************************************************************
 *
 *	cp_error
 *
 * Display error message.
 */
static int cp_error(char *name)
{
	register char	*error_str;
	
	exit_code = 2;
	switch (errno) {
		case EACCES:
			error_str = "Permission denied";
			break;
		case EBADF:
			error_str = "Bad file number";
			break;
		case ENOENT:
			error_str = "Path or file name not found";
			break;
		case EMFILE:
			error_str = "Too many open files";
			break;
#ifdef __TURBOC__
		case EINVACC:
			error_str = "Invalid access code";
			break;
#endif
		default:
			error_str = "Unknown error";
			break;
	}
	printf("%s:%s\n", name, error_str);
	return FALSE;
}

/**********************************************************************
 *
 *	cp
 *
 * Copy file from open handles src to dest.
 * Return status of copy.
 */
static copy_status_t cp(int src, int dest)
{
	register unsigned	rcount,
				wcount;

	/* copy until zero bytes read or error */
	while ((rcount = read(src, buffer, bufsize)) != 0 && (int)rcount != ERROR) {
		if ((wcount = write(dest, buffer, rcount)) == (unsigned)ERROR)
			return FULL;
		/* check if disk full */
		if (wcount < rcount)
			return FULL;
	}
	if ((int)rcount == ERROR)
		return FAIL_SRC;
	return COPIED;
}

/**********************************************************************
 *
 *	show_cp
 */
static void show_cp(char *src, char *dest)
{
	FILE* f;
	
	if (interactive)
		f = stderr;
	else
		f = stdout;
	fprintf(f, "cp %s %s", src, dest);
}

/**********************************************************************
 *
 *	check_flags
 */
static bool check_flags(char *src, char *dest, struct stat *st_src)
{
	int		is_dest;
	struct stat	st_dest;
	
	is_dest = (stat(dest, &st_dest) == 0);
	if (stat(src, st_src) != 0) {
		printf("File %s not found\n", src);
		exit_code = 2;
		return FALSE;
	}
	if (time_copy && is_dest && st_src->st_mtime <= st_dest.st_mtime)
		return FALSE;
	if (changed_copy && is_dest && st_src->st_mtime == st_dest.st_mtime
				    && st_src->st_size == st_dest.st_size)
		return FALSE;
	if (interactive || !silent)
		show_cp(src, dest);
	if (stricmp(src, dest) == 0) {
		if (silent)
			show_cp(src, dest);
		printf(": file cannot be copied onto itself\n");
		exit_code = 2;
		return FALSE;
	}
	if (interactive) {
		if (!interact())
			return FALSE;
	} else if (!silent)
		 printf("\n");
	if (is_dest && !interactive && !overwrite && !overwrite_file(dest))
		return FALSE;
	return TRUE;
}

/**********************************************************************
 *
 *	copy_file
 *
 * Copy src to dest. If disk full, prompt user and wait to continue.
 * Return TRUE, if file was copied, otherwise false.
 */
static bool copy_file(char *src, char *dest)
{
	register int	src_h, dest_h;	/* file handles */
	int		attr;
	copy_status_t	status;
	struct stat	st_src;
	struct utimbuf	ut;

	if (!check_flags(src, dest, &st_src))
		return FALSE;
	do {
		if (keep_attr)
			attr = st_src.st_mode;
		else
			/* MikaH Changed &->| New file should be writable after copying */
			attr = st_src.st_mode | (S_IREAD|S_IWRITE);
		if ((src_h = open(src, O_RDONLY|src_fmode)) == ERROR)
			return cp_error(src);
		/* create new file to copy to */
		dest_h = open(dest, O_CREAT|O_TRUNC|O_WRONLY|dest_fmode, attr);
		if (dest_h == ERROR && force) {
			chmod(dest, S_IREAD|S_IWRITE);
			dest_h = open(dest, O_CREAT|O_TRUNC|O_WRONLY|dest_fmode, attr);
		}
		if (dest_h == ERROR) {
			close(src_h);
			return cp_error(dest);
		}
		/* copy file */
		status = cp(src_h, dest_h);
		close(src_h);
		close(dest_h);
		/* update destination file time */
		ut.actime = st_src.st_atime;
		ut.modtime = st_src.st_mtime;
#ifdef OS2
            /* Ari changed this on Jul 31, 1997.
             * 'cj -oa' did not work on read-only files (solid sources that
             *  were in version control etc)
             * The read-only attribute was copied and then the utime() failed
             * in setting the destination file time.
             */
		if (utime(dest, &ut) != 0) {
                /* fprintf(stderr, "Could not set destination file time.\n"); */
                if (!keep_attr) {
			    chmod(dest, S_IREAD|S_IWRITE);
		        if (utime(dest, &ut) != 0) {
                        /* fprintf(stderr, "Could not set destination file time.\n"); */
                    } else {
                        /* fprintf(stderr, "Dest file time now set.\n"); */
                    }
                }
            }
#else
		utime(dest, &ut);
#endif
		/* check if disk full */
		if (status == FULL) {
			remove(dest);
			fprintf(stderr, "Disk full, press ^C to abort, 's' "
				"for OS shell or any other key to retry\n");
			switch(getch()) {
				case CTRLC:
					fprintf(stderr, "^C\n");
					exit(2);
				case 's':
				case 'S':
					system("");
					break;
			}
		}
	} while (status == FULL);
	if (status == FAIL_SRC)
		cp_error(src);
	else if (status == FAIL_DEST)
		cp_error(dest);
	return status == COPIED;
}

#ifndef NT
/**********************************************************************
 *
 *	menu_select
 */
static bool menu_select(char** files)
{
	listmenu_t* l;
	char* s;

	l = default_listmenu_t();
	l->header = "Select files to copy:";
	s = listmenu(files, l);
	fprintf(stderr, "\n");
	return(s != NULL);
}
#endif

/**********************************************************************
 *
 *	get_path_type
 */
static int get_path_type(char *path)
{
	register int	path_end,
			path_len;
	struct stat	st;
	
	path_end = path[strlen(path)-1];
	if (path_end == ':')
		return DEV;
	path_len = strlen(path[1] == ':' ? path + 2 : path);
	if (path_len == 1 && (path_end == '\\' || path_end == '/'))
		return ROOT;
	if (stat(path, &st) != 0) 	/* not found, must be new file */
		return 0;
	return (st.st_mode & S_IFDIR) ? DIR : 0;
}

/**********************************************************************
 *
 *	main
 */
int main(int argc, register char *argv[])
{
	static char	destpath[MAXPATHLEN],
			fulldest[MAXPATHLEN];
	int		opt,
			copy_count = 0,
			path_type,
			use_menu = 0;
	char*		dest;
	
	opterr = FALSE;		/* handle errors ourselves */
	while ((opt = getopt(argc, argv, "acfimostDU")) != EOF) {
		switch (opt) {
			case 'a':
				keep_attr = 0;
				break;
			case 'c':
				changed_copy = 1;
				break;
			case 'f':
				force = overwrite = 1;
				break;
			case 'i':
				interactive = 1;
				break;
#ifndef NT
			case 'm':
				use_menu = 1;
				break;
#endif
			case 'o':
				overwrite = 1;
				break;
			case 's':
				silent = 1;
				break;
			case 't':
				time_copy = 1;
				break;
			case 'D':
				dest_fmode = O_TEXT;
				break;
			case 'U':
				src_fmode = O_TEXT;
				break;
			default:
				printf("Invalid command line option\n");
				usage(2);
		}
	}
	if (argc - optind < 2)
		usage(1);
	dest = argv[argc-1];
	argv[argc-1] = NULL;
	/* get as large buffer as possible */
	if ((buffer = malloc(bufsize = MAXBUF)) == NULL) {
		printf("Memory allocation error\n");
		return(2);
	}
#ifndef NT
	if (use_menu && !menu_select(argv+optind)) {
		printf("No files selected\n");
		return(1);
	}
#endif
	path_type = get_path_type(strcpy(destpath, dest));
	/* simple first: check if dest doesn't exist or it's a file */
	if (!path_type) {
		if (argc - optind == 2) {
			copy_file(argv[optind], destpath);
			return(exit_code);
		} else
			usage(2);
	}
	if (path_type == DIR)
		strcat(destpath, "\\");
	/* copy files to drive or directory */
	for (; argv[optind]; optind++) {
		strcat(strcpy(fulldest, destpath), u_splitpath(argv[optind]));
		if (copy_file(argv[optind], fulldest))
			++copy_count;
	}
	if (!copy_count) {
		if (!silent)
			printf("No files copied\n");
	}
	return(exit_code);
}
