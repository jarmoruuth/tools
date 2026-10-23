/**********************************************************************\
 *
 *	CJ.C
 *
 * Unix like file copy, originally for MS-DOS. Works in Windows and
 * POSIX systems. Options -U and -D translate line ends when copying
 * text files between Windows and Unix.
 * No wild cards are allowed in destination.
 *
 * Return values:
 *	0 - ok, also if no files needed copying
 *	1 - usage
 *	2 - errors
 *
 *	J.Ruuth 15-09-1987
 *	J.Ruuth 23-10-2026, portable to Windows and POSIX, no limits
 *	on path length, 64-bit file sizes, -U and -D work in all
 *	systems, wild cards are expanded here, menu selection and
 *	disk full retry removed
 *
\**********************************************************************/

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdir.h"

#define FALSE	0
#define TRUE	1

#define CTRLC	3

#define BUFSIZE	(256 * 1024)

static struct {
	char	noattr,		/* do not copy attributes */
		changed,	/* copy only changed or new files */
		force,		/* force copy */
		interactive,
		overwrite,	/* overwrite existing files */
		silent,
		time,		/* copy only over older or new files */
		to_dos,		/* translate lf to cr+lf */
		to_unix;	/* translate cr+lf to lf */
} mode;

typedef struct {
	char	**name;
	size_t	len;
	size_t	max;
} list_t;

static int	exit_code = 0;		/* exit() code */
static char	*buf[2];		/* copy buffers */

/**********************************************************************
 *
 *	usage
 *
 * Exit with help message
 */
static void usage(int error_code)
{
	printf(
	"Usage: cj [-acfiostDU] source [source...] destination\n"
	"If more than one source, destination must be directory\n"
	"Options: -a do not copy attributes from source to destination\n"
	"         -c copy only changed or new files\n"
	"         -f force copy, also over read-only files and\n"
	"            hidden files with wildcards\n"
	"         -i interactive\n"
	"         -o overwrite existing files\n"
	"         -s silent mode\n"
	"         -t copy only over older or new files\n"
	"         -D unix to Dos copy, translate lf to cr+lf\n"
	"         -U dos to Unix copy, translate cr+lf to lf\n"
	"Wildcards * and ? are allowed in the last path component of\n"
	"sources.\n");
	exit(error_code);
}

/**********************************************************************
 *
 *	error
 */
static void error(const char *name, const char *msg)
{
	fflush(stdout);
	fprintf(stderr, "cj: %s: %s\n", name, msg);
	exit_code = 2;
}

/**********************************************************************
 *
 *	interact
 *
 * Ask user permission with a single key press.
 */
static int interact(void)
{
	int	ans;

	fprintf(stderr, " ? ");
	do {
		ans = pd_getkey();
		if (ans == CTRLC || ans == -1) {
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
 *	add
 *
 * Add a name to list. Name is not copied.
 */
static void add(char *name, list_t *list)
{
	if (list->len == list->max) {
		list->max = list->max ? 2 * list->max : 64;
		list->name = pd_realloc(list->name, list->max * sizeof(char *));
	}
	list->name[list->len++] = name;
}

/**********************************************************************
 *
 *	name_cmp
 */
static int name_cmp(const void *p1, const void *p2)
{
	return pd_namecmp(*(char *const *)p1, *(char *const *)p2);
}

/**********************************************************************
 *
 *	add_sources
 *
 * Add files given by one command line argument to list. Wildcards
 * match only files, and hidden files only with -f.
 */
static void add_sources(char *arg, list_t *list)
{
	pd_dir_t	*d;
	pd_entry_t	e;
	char		*base = pd_basename(arg);
	char		*dir, *path;
	size_t		dirlen = base - arg;
	size_t		first = list->len;

	if (!pd_haswild(base)) {
		add(pd_strdup(arg), list);
		return;
	}
	dir = memcpy(pd_malloc(dirlen + 1), arg, dirlen);
	dir[dirlen] = '\0';
	if (pd_haswild(dir))
		error(arg, "wildcards are allowed only in the last path component");
	else if ((d = pd_opendir(dir)) == NULL)
		error(*dir ? dir : ".", pd_strerror());
	else {
		while (pd_readdir(d, &e)) {
			if (e.is_dir || !pd_match(base, e.name)
			    || (e.hidden && !mode.force))
				continue;
			path = pd_join(dir, e.name);
			/* skip links to directories */
			if (e.is_link && pd_stat_follow(path, &e) == 0 && e.is_dir)
				free(path);
			else
				add(path, list);
		}
		pd_closedir(d);
		if (list->len == first)
			error(arg, "not found");
		qsort(list->name + first, list->len - first, sizeof(char *),
		      name_cmp);
	}
	free(dir);
}

/**********************************************************************
 *
 *	to_unix
 *
 * Translates cr+lf to lf. A cr at the end of the buffer is left
 * pending in *cr, as only the next byte tells if it is followed
 * by lf. Returns the new length.
 */
static int to_unix(const char *in, int n, char *out, int *cr)
{
	char	*d = out;
	int	i;

	for (i = 0; i < n; i++) {
		if (*cr && in[i] != '\n')
			*d++ = '\r';
		*cr = in[i] == '\r';
		if (!*cr)
			*d++ = in[i];
	}
	return (int)(d - out);
}

/**********************************************************************
 *
 *	to_dos
 *
 * Translates lf to cr+lf. Lines ending already with cr+lf are not
 * changed. *cr tells if the previous byte was cr. Returns the new
 * length.
 */
static int to_dos(const char *in, int n, char *out, int *cr)
{
	char	*d = out;
	int	i;

	for (i = 0; i < n; i++) {
		if (in[i] == '\n' && !*cr)
			*d++ = '\r';
		*cr = in[i] == '\r';
		*d++ = in[i];
	}
	return (int)(d - out);
}

/**********************************************************************
 *
 *	cp
 *
 * Copy file from src to dest, translating line ends if requested.
 * Return TRUE, if ok.
 */
static int cp(pd_file_t *src, pd_file_t *dest,
	      const char *src_name, const char *dest_name)
{
	char	*p;
	int	n, unix_cr = FALSE, dos_cr = FALSE;

	while ((n = pd_read(src, buf[0], BUFSIZE)) > 0) {
		p = buf[0];
		if (mode.to_unix) {
			n = to_unix(p, n, buf[1], &unix_cr);
			p = buf[1];
		}
		if (mode.to_dos) {
			n = to_dos(p, n, p == buf[0] ? buf[1] : buf[0], &dos_cr);
			p = p == buf[0] ? buf[1] : buf[0];
		}
		if (pd_write(dest, p, n) != n) {
			error(dest_name, pd_strerror());
			return FALSE;
		}
	}
	if (n < 0) {
		error(src_name, pd_strerror());
		return FALSE;
	}
	if (unix_cr && pd_write(dest, "\r", 1) != 1) {
		error(dest_name, pd_strerror());
		return FALSE;
	}
	return TRUE;
}

/**********************************************************************
 *
 *	check_flags
 *
 * Checks if src should be copied to dest. Gets info of src.
 */
static int check_flags(const char *src, const char *dest, pd_entry_t *st_src)
{
	int		is_dest;
	pd_entry_t	st_dest;

	if (pd_stat_follow(src, st_src) != 0) {
		error(src, pd_strerror());
		return FALSE;
	}
	if (st_src->is_dir) {
		error(src, "is a directory, not copied");
		return FALSE;
	}
	is_dest = (pd_stat_follow(dest, &st_dest) == 0);
	if (is_dest && st_dest.is_dir) {
		error(dest, "is a directory");
		return FALSE;
	}
	if (is_dest && pd_samefile(src, dest)) {
		error(dest, "file cannot be copied onto itself");
		return FALSE;
	}
	if (mode.time && is_dest && st_src->mtime <= st_dest.mtime)
		return FALSE;
	/* with translation the sizes differ */
	if (mode.changed && is_dest && st_src->mtime == st_dest.mtime
	    && (st_src->size == st_dest.size || mode.to_dos || mode.to_unix))
		return FALSE;
	if (mode.interactive) {
		fflush(stdout);
		fprintf(stderr, "cp %s %s", src, dest);
		return interact();
	}
	if (!mode.silent) {
		printf("cp %s %s\n", src, dest);
		fflush(stdout);
	}
	if (is_dest && !mode.overwrite) {
		fprintf(stderr, "File %s exists, overwrite", dest);
		return interact();
	}
	return TRUE;
}

/**********************************************************************
 *
 *	copy_file
 *
 * Copy src to dest. Return TRUE, if file was copied, otherwise FALSE.
 */
static int copy_file(const char *src, const char *dest)
{
	pd_file_t	*src_f, *dest_f;
	pd_entry_t	st_src;
	int		ok;

	if (!check_flags(src, dest, &st_src))
		return FALSE;
	if ((src_f = pd_open(src)) == NULL) {
		error(src, pd_strerror());
		return FALSE;
	}
	/* create new file to copy to */
	dest_f = pd_create(dest);
	if (dest_f == NULL && mode.force && pd_make_writable(dest) == 0)
		dest_f = pd_create(dest);
	if (dest_f == NULL) {
		error(dest, pd_strerror());
		pd_close(src_f, NULL);
		return FALSE;
	}
	ok = cp(src_f, dest_f, src, dest);
	pd_close(src_f, NULL);
	if (mode.noattr) {
		/* only time is copied, new file stays writable */
		st_src.mode = 0;
		st_src.readonly = FALSE;
	}
	if (ok && pd_close(dest_f, &st_src) != 0) {
		error(dest, pd_strerror());
		ok = FALSE;
	} else if (!ok)
		pd_close(dest_f, NULL);
	if (!ok)
		pd_remove(dest);
	return ok;
}

/**********************************************************************
 *
 *	is_dir
 *
 * Checks if destination is a directory: an existing directory,
 * or a drive name in Windows.
 */
static int is_dir(const char *path)
{
	pd_entry_t	e;

#ifdef _WIN32
	if (path[0] != '\0' && path[1] == ':' && path[2] == '\0')
		return TRUE;
#endif
	return pd_stat_follow(path, &e) == 0 && e.is_dir;
}

/**********************************************************************
 *
 *	main
 */
int main(int argc, char **argv)
{
	int	i, copy_count = 0;
	size_t	j, len;
	char	*p, *dest, *fulldest;
	list_t	sources;

	pd_init(&argc, &argv);
	for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}
		for (p = argv[i] + 1; *p != '\0'; p++) {
			switch (*p) {
				case 'a':
					mode.noattr = TRUE;
					break;
				case 'c':
					mode.changed = TRUE;
					break;
				case 'f':
					mode.force = mode.overwrite = TRUE;
					break;
				case 'i':
					mode.interactive = TRUE;
					break;
				case 'o':
					mode.overwrite = TRUE;
					break;
				case 's':
					mode.silent = TRUE;
					break;
				case 't':
					mode.time = TRUE;
					break;
				case 'D':
					mode.to_dos = TRUE;
					break;
				case 'U':
					mode.to_unix = TRUE;
					break;
				default:
					printf("Invalid command line option\n");
					usage(2);
			}
		}
	}
	if (argc - i < 2)
		usage(1);
	dest = argv[argc-1];
	if (pd_haswild(dest)) {
		error(dest, "no wildcards are allowed in destination");
		return exit_code;
	}
	memset(&sources, 0, sizeof(sources));
	for (; i < argc - 1; i++)
		add_sources(argv[i], &sources);
	buf[0] = pd_malloc(2 * BUFSIZE + 1);
	buf[1] = pd_malloc(2 * BUFSIZE + 1);
	if (!is_dir(dest)) {
		len = strlen(dest);
		if (len > 0 && PD_ISSLASH(dest[len-1]))
			error(dest, "directory not found");
		else if (sources.len == 1)
			copy_file(sources.name[0], dest);
		else if (sources.len > 1)
			error(dest, "destination must be a directory");
		return exit_code;
	}
	/* copy files to drive or directory */
	for (j = 0; j < sources.len; j++) {
		fulldest = pd_join(dest, pd_basename(sources.name[j]));
		if (copy_file(sources.name[j], fulldest))
			++copy_count;
		free(fulldest);
	}
	if (!copy_count && !mode.silent)
		printf("No files copied\n");
	return exit_code;
}
