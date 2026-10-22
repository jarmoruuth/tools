/************************************************************************\
**			RJ.C - rm for MS-DOS, Windows and POSIX
**			---------------------------------------
**	Almost similar to UNIX command rm.
**	The most remarkable difference is that you do not have to hit <CR>
**	after typing 'y' or 'n' in interactive mode.
**	Also, one extension is added -- the possibility to remove
**	directories when using option -d
**	With option -r the file pattern is searched in all subdirectories,
**	so "rj -r *.obj" removes all .obj files in the directory tree.
**
**	syntax:
**		rj [-dfirs] filename ...
**
**	options:
**		-d	- also delete directories
**		-f	- force delete for read-only and hidden files
**		-i	- interactive
**		-r	- recurse subdirectories
**		-s	- silent mode
**
**	wildcards are allowed in filenames
**
**	WARNING :
**		command "rj -dfr /"
**		will erase ALL disc contents.
**
**	author :	Petri Soini
**	date :		V1.0 January 17, 1988
**
**	Copyright :	(C) 1987,88 by Petri Soini,
**	May be freely copied for any non-commercial usage.
**
**	Disclaimer:	The author is NOT responsible for any damage
**			caused by correct or incorrect use of this
**			program.
**
**	Modified:	- J.Ruuth, to display always which file to rm.
**			           I like it more.
**			- J.Ruuth, added menu selection
**			- J.Ruuth 22-Oct-2026, portable to Windows and
**			  POSIX, no limits on path length, links and
**			  junctions are removed but never followed,
**			  menu selection removed
\************************************************************************/

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"pdir.h"

#define FALSE	0
#define TRUE	1

static struct {
	char	interactive,
		directory,
		force,
		recursive,
		silent;
} mode;

typedef struct {
	char	**name;
	size_t	len;
	size_t	max;
} list_t;

static int	rval;
static int	nerrors;

static list_t	dir_list,
		file_list;

/************************************************************************
**		usage()
**		-------
**	exit with help message
*/
static void 	usage(void)
{
	printf(
	"Usage:\trj [-dfirs] filename ...\n"
	"Options:\n\t"
		"-d  delete directories\n\t"
		"-f  force delete for read-only and hidden files\n\t"
		"-i  interactive\n\t"
		"-r  recurse subdirectories, search the pattern in all of them\n\t"
		"-s  silent mode\n"
	"Specify a directory by adding a slash after the name:\n"
	"\"rj -rd dir/\" removes the whole directory tree.\n"
	"Wildcards * and ? are allowed in the last path component.\n"
	"Quote them in Unix shells so that rj expands them: rj -r '*.o'\n"
	"Links and junctions are removed, but never followed.\n");
	exit(1);
}


/************************************************************************
**		error()
**		-------
*/
static void 	error(const char *name, const char *msg)
{
	fflush(stdout);
	fprintf(stderr, "rj: %s: %s\n", name, msg);
	rval = 2;
	nerrors++;
}


/************************************************************************
**		mark()
**		------
**	mark file or directory to be deleted by linking its name
**	to list. Name is not copied.
*/
static void 	mark (char *name, list_t *list)
{
	if (list->len == list->max) {
		list->max = list->max ? 2 * list->max : 64;
		list->name = pd_realloc(list->name, list->max * sizeof(char *));
	}
	list->name[list->len++] = name;
}


/************************************************************************
**		remove_one()
**		------------
**	removes a file or a directory, with -f also read-only ones.
*/
static void 	remove_one (char *name, int dir)
{
	int	failed;

	if (!mode.interactive && !mode.silent) {
		printf(dir ? "rmdir %s\n" : "rm %s\n", name);
		fflush(stdout);
	}
	failed = dir ? pd_rmdir(name) : pd_remove(name);
	if (failed && mode.force && pd_make_writable(name) == 0)
		failed = dir ? pd_rmdir(name) : pd_remove(name);
	if (failed)
		error(name, pd_strerror());
}


/************************************************************************
**		expunge()
**		---------
**	does the real deletion for files and possibly directories.
**	Directories are in the list after their contents.
*/
static void 	expunge (void)
{
	size_t	i;

	for (i = 0; i < file_list.len; i++)
		remove_one(file_list.name[i], FALSE);
	for (i = 0; i < dir_list.len; i++)
		remove_one(dir_list.name[i], TRUE);
}


/************************************************************************
**		delete()
**		--------
**	delete file (after asking the user for permission in
**	interactive mode). Name must be allocated with malloc.
*/
static void 	delete(char *fname, int dir)
{
	int	ans;

	if (mode.interactive) {
		fflush(stdout);
		fprintf(stderr, "remove %s%s? ", dir ? "directory " : "", fname);
		do {
			ans = pd_getkey();
			if (ans == 3 || ans == -1) {
				fprintf(stderr, "^C\n");
				exit(1);
			}
		} while (!(ans=='y' || ans=='Y' || ans=='n' || ans=='N'));
		if (ans == 'n' || ans == 'N') {
			fprintf(stderr, "No\n");
			free(fname);
			return;
		}
		fprintf(stderr, "Yes\n");
	}
	mark(fname, dir ? &dir_list : &file_list);
}


/************************************************************************
**		name_cmp()
**		----------
*/
static int	name_cmp (const void *p1, const void *p2)
{
	return pd_namecmp(*(char *const *)p1, *(char *const *)p2);
}


/************************************************************************
**		rm ()
**		-----
**	delete files matching pattern in directory dir, recursively if
**	required. Directory contents are marked before the directory.
**	Hidden files match a wildcard pattern only with -f.
**	Returns the number of matching files.
*/
static size_t	rm (const char *dir, const char *pattern)
{
	pd_dir_t	*d;
	pd_entry_t	e;
	list_t		subdirs, dirs;
	size_t		i, found = 0;
	int		wild = pd_haswild(pattern);
	int		plain_dir;

	if ((d = pd_opendir(dir)) == NULL) {
		error(*dir ? dir : ".", pd_strerror());
		return 0;
	}
	memset(&subdirs, 0, sizeof(subdirs));
	memset(&dirs, 0, sizeof(dirs));
	while (pd_readdir(d, &e)) {
		/* links are removed as files and never followed */
		plain_dir = e.is_dir && !e.is_link;
		if (mode.recursive && plain_dir && (mode.force || !e.hidden))
			mark(pd_join(dir, e.name), &subdirs);
		if (!pd_match(pattern, e.name)
		    || (wild && e.hidden && !mode.force))
			continue;
		found++;
		if (!plain_dir)
			delete(pd_join(dir, e.name), FALSE);
		else if (mode.directory)
			mark(pd_join(dir, e.name), &dirs);
	}
	pd_closedir(d);
	qsort(subdirs.name, subdirs.len, sizeof(char *), name_cmp);
	for (i = 0; i < subdirs.len; i++) {
		found += rm(subdirs.name[i], pattern);
		free(subdirs.name[i]);
	}
	free(subdirs.name);
	for (i = 0; i < dirs.len; i++)
		delete(dirs.name[i], TRUE);
	free(dirs.name);
	return found;
}

/**********************************************************************
**	strip_slashes
**
** Removes trailing slashes but keeps root directory as it is.
*/
static void	strip_slashes(char *path)
{
	size_t	len = strlen(path);
	size_t	min = 1;

#ifdef _WIN32
	if (len >= 2 && path[1] == ':')
		min = 3;
#endif
	while (len > min && PD_ISSLASH(path[len-1]))
		path[--len] = '\0';
}

/**********************************************************************
**	is_root
*/
static int	is_root(const char *path)
{
#ifdef _WIN32
	if (path[0] != '\0' && path[1] == ':')
		path += 2;
	return *path == '\0' || (PD_ISSLASH(path[0]) && path[1] == '\0');
#else
	return strcmp(path, "/") == 0;
#endif
}

/**********************************************************************
**	isdir
**
** File is expected to mean a directory if it ends with \ or /.
*/
static int 	isdir(const char *fname)
{
	size_t	len = strlen(fname);

	return len > 0 && PD_ISSLASH(fname[len-1]);
}

/**********************************************************************
**	rm_dir
**
** Remove all files in the directory and the directory itself if
** mode.directory is TRUE.
*/
static void 	rm_dir(char *dirname)
{
	pd_entry_t	e;
	const char	*base;

	strip_slashes(dirname);
	if (pd_stat(dirname, &e) != 0) {
		error(dirname, pd_strerror());
		return;
	}
	if (!e.is_dir || e.is_link) {
		error(dirname, "not a directory");
		return;
	}
	rm(dirname, "*");
	base = pd_basename(dirname);
	/* if not root or current directory, delete it */
	if (mode.directory && !is_root(dirname)
	    && strcmp(base, ".") != 0 && strcmp(base, "..") != 0)
		delete(pd_strdup(dirname), TRUE);
}

/**********************************************************************
**	rm_arg
**
** Remove files given by one command line argument.
*/
static void 	rm_arg(char *arg)
{
	pd_entry_t	e;
	char		*base = pd_basename(arg);
	char		*dir;
	size_t		dirlen = base - arg;
	int		errors;

	dir = memcpy(pd_malloc(dirlen + 1), arg, dirlen);
	dir[dirlen] = '\0';
	strip_slashes(dir);
	if (pd_haswild(dir)) {
		error(arg, "wildcards are allowed only in the last path component");
	} else if (mode.recursive || pd_haswild(base)) {
		errors = nerrors;
		if (rm(dir, base) == 0 && nerrors == errors)
			error(arg, "not found");
	} else if (pd_stat(arg, &e) != 0) {
		error(arg, pd_strerror());
	} else if (e.is_dir && !e.is_link) {
		if (mode.directory)
			delete(pd_strdup(arg), TRUE);
		else
			error(arg, "is a directory, use -d or add a slash after the name");
	} else
		delete(pd_strdup(arg), FALSE);
	free(dir);
}

/************************************************************************
**		main()
**		------
*/
int	main(int argc, char **argv)
{
	int	i;
	char	*p;

	pd_init(&argc, &argv);
	for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}
		for (p = argv[i] + 1; *p != '\0'; p++) {
			switch (*p) {
				case 'd' :
					mode.directory = TRUE;
					break;
				case 'f' :
					mode.force = TRUE;
					break;
				case 'i' :
					mode.interactive = TRUE;
					break;
				case 'r' :
					mode.recursive = TRUE;
					break;
				case 's':
					mode.silent = TRUE;
					break;
				default  :
					printf("Illegal option argument\n");
					usage();
			}
		}
	}
	if  (argc <= i)
		usage();
	for (; i < argc; i++) {
		if (isdir(argv[i]))
			rm_dir (argv[i]);
		else
			rm_arg (argv[i]);
	}
	expunge();
	return (rval);
}
