/**********************************************************************\
 *
 *		LJ.C
 *
 * Unix-like ls, originally for MS-DOS. Works in Windows and POSIX
 * systems. For more info see usage().
 *
 * Return values: 0, if files are found
 *		  1, if no files are found
 *		  2, if errors
 *
 * Version: 1.00 1988		MS-DOS version using filelist library
 *	    2.00 22-Oct-2026	Portable to Windows and POSIX, 64-bit sizes,
 *				no limits on file name length or file count
 *
\**********************************************************************/

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include "pdir.h"

#define FALSE	0
#define TRUE	1

static struct {
	char	reverse;
	char	timesort;
	char	long_ls;
	char	recurse;
	char	sizes;
	char	all;
	char	one_column;
	char	dirs;
	char	files;
	char	sizesort;
} mode;

typedef struct {
	char		*path;		/* directory, "" for file arguments */
	pd_entry_t	*items;
	size_t		len;
	size_t		max;
} group_t;

static int	exit_code = 1;		/* no files found */
static int	ngroups;		/* printed groups */
static int	headers;		/* print directory headers */
static int	termwidth;

/**********************************************************************
 *
 *	usage
 */
static void usage(void)
{
	printf(
"Usage: lj [-adflrRsSt1] [files and directories]\n"
"Options: -a  list all files, also hidden and dot files\n"
"         -d  list only directories\n"
"         -f  list only files\n"
"         -l  long listing\n"
"         -r  reverse sort order\n"
"         -R  recurse subdirectories\n"
"         -s  print size of each file\n"
"         -S  sort by size\n"
"         -t  sort by time\n"
"         -1  one column format\n"
"If no arguments are given, current directory is listed.\n"
"Wildcards * and ? are allowed in the last path component. With -R the\n"
"pattern is matched in all subdirectories; quote it in Unix shells.\n");
	exit(2);
}

/**********************************************************************
 *
 *	error
 */
static void error(const char *name, const char *msg)
{
	fflush(stdout);
	fprintf(stderr, "lj: %s: %s\n", name, msg);
	exit_code = 2;
}

/**********************************************************************
 *
 *	name_cmp
 */
static int name_cmp(const void *v1, const void *v2)
{
	const pd_entry_t	*p1 = v1;
	const pd_entry_t	*p2 = v2;
	int			retval;

	retval = pd_namecmp(p1->name, p2->name);
	return mode.reverse ? -retval : retval;
}

/**********************************************************************
 *
 *	size_cmp
 */
static int size_cmp(const void *v1, const void *v2)
{
	const pd_entry_t	*p1 = v1;
	const pd_entry_t	*p2 = v2;
	int			retval;

	retval = (p1->size > p2->size) - (p1->size < p2->size);
	return retval ? (mode.reverse ? retval : -retval)
		      : name_cmp(v1, v2);
}

/**********************************************************************
 *
 *	time_cmp
 */
static int time_cmp(const void *v1, const void *v2)
{
	const pd_entry_t	*p1 = v1;
	const pd_entry_t	*p2 = v2;
	int			retval;

	retval = (p1->mtime > p2->mtime) - (p1->mtime < p2->mtime);
	return retval ? (mode.reverse ? retval : -retval)
		      : name_cmp(v1, v2);
}

/**********************************************************************
 *
 *	ultos
 *
 * Formats a number with thousand separators.
 */
static char *ultos(uint64_t l, char *buf)
{
	char	tmp[32];
	int	len, i;
	char	*bp = buf;

	len = sprintf(tmp, "%" PRIu64, l);
	for (i = 0; i < len; i++) {
		if (i > 0 && (len - i) % 3 == 0)
			*bp++ = ',';
		*bp++ = tmp[i];
	}
	*bp = '\0';
	return buf;
}

/**********************************************************************
 *
 *	print_padded
 */
static void print_padded(const char *s, size_t width)
{
	size_t	w = pd_strwidth(s);

	fputs(s, stdout);
	for (; w < width; w++)
		putchar(' ');
}

/**********************************************************************
 *
 *	short_ls
 *
 * Lists names in columns sorted down the columns. With -s also
 * the size of each file is shown.
 */
static void short_ls(pd_entry_t *ii, size_t len)
{
	size_t	maxnamelen = 0;
	size_t	maxsizelen = 0;
	size_t	i, w, rows, cols, r, c;
	char	buf[32];

	if (mode.one_column) {
		for (i = 0; i < len; i++)
			printf("%s\n", ii[i].name);
		return;
	}
	for (i = 0; i < len; i++) {
		if ((w = pd_strwidth(ii[i].name)) > maxnamelen)
			maxnamelen = w;
		if (mode.sizes
		    && (w = strlen(ultos(ii[i].size, buf))) > maxsizelen)
			maxsizelen = w;
	}
	w = maxnamelen + 1 + (mode.sizes ? maxsizelen + 2 : 0);
	cols = (termwidth - 1) / w;
	if (cols < 1)
		cols = 1;
	rows = (len + cols - 1) / cols;
	cols = (len + rows - 1) / rows;
	for (r = 0; r < rows; r++) {
		for (c = 0; c < cols && (i = c * rows + r) < len; c++) {
			if (mode.sizes)
				printf(" %*s ", (int)maxsizelen,
				       ultos(ii[i].size, buf));
			if (c + 1 < cols && (c + 1) * rows + r < len)
				print_padded(ii[i].name, maxnamelen + 1);
			else
				fputs(ii[i].name, stdout);
		}
		putchar('\n');
	}
}

/**********************************************************************
 *
 *	attr_string
 */
static char *attr_string(pd_entry_t *ip, char *buf)
{
#ifdef _WIN32
	sprintf(buf, "%c%c%c%c%c%c",
		ip->is_link ? 'l' : ip->is_dir ? 'd' : '-',
		ip->archive  ? 'a' : '-',
		ip->system   ? 's' : '-',
		ip->hidden && !ip->system ? 'h' : '-',
		'r',
		ip->readonly ? '-' : 'w');
#else
	static const char rwx[] = "rwxrwxrwx";
	unsigned	m = ip->mode;
	int		i;

	buf[0] = ip->is_link ? 'l' : ip->is_dir ? 'd' : '-';
	for (i = 0; i < 9; i++)
		buf[i+1] = (m & (0400 >> i)) ? rwx[i] : '-';
	buf[10] = '\0';
#endif
	return buf;
}

/**********************************************************************
 *
 *	long_ls
 */
static void long_ls(pd_entry_t *ii, size_t len)
{
	size_t		i, w, maxsizelen = 0;
	char		attr[16], buf[32];
	time_t		t;
	struct tm	*tm;
	struct tm	zero;

	for (i = 0; i < len; i++)
		if ((w = strlen(ultos(ii[i].size, buf))) > maxsizelen)
			maxsizelen = w;
	memset(&zero, 0, sizeof(zero));
	for (i = 0; i < len; i++) {
		t = (time_t)ii[i].mtime;
		if ((tm = localtime(&t)) == NULL)
			tm = &zero;
		printf("%s  %*s  %2d.%02d.%04d  %2d:%02d  %s\n",
			attr_string(&ii[i], attr),
			(int)maxsizelen,
			ultos(ii[i].size, buf),
			tm->tm_mday,
			tm->tm_mon + 1,
			tm->tm_year + 1900,
			tm->tm_hour,
			tm->tm_min,
			ii[i].name);
	}
}

/**********************************************************************
 *
 *	add_item
 */
static void add_item(group_t *g, pd_entry_t *e, const char *name)
{
	pd_entry_t	*ip;
	size_t		len = strlen(name);

	if (g->len == g->max) {
		g->max = g->max ? 2 * g->max : 64;
		g->items = pd_realloc(g->items, g->max * sizeof(pd_entry_t));
	}
	ip = &g->items[g->len++];
	*ip = *e;
	/* room for the directory slash */
	ip->name = memcpy(pd_malloc(len + 2), name, len + 1);
	if (ip->is_dir)
		strcat(ip->name, PD_SLASHSTR);
}

/**********************************************************************
 *
 *	free_group
 */
static void free_group(group_t *g)
{
	size_t	i;

	for (i = 0; i < g->len; i++)
		free(g->items[i].name);
	free(g->items);
	g->items = NULL;
	g->len = g->max = 0;
}

/**********************************************************************
 *
 *	selected
 *
 * Checks if entry is selected by -a, -d and -f options.
 */
static int selected(pd_entry_t *e)
{
	if (!mode.all && (e->hidden || e->name[0] == '.'))
		return FALSE;
	if (mode.dirs && !e->is_dir)
		return FALSE;
	if (mode.files && e->is_dir)
		return FALSE;
	return TRUE;
}

/**********************************************************************
 *
 *	print_group
 */
static void print_group(group_t *g, const char *diskpath)
{
	size_t		i;
	uint64_t	size = 0;
	int64_t		diskfree;
	char		buf[32];

	if (g->len == 0)
		return;
	exit_code = exit_code == 2 ? 2 : 0;
	if (ngroups++ > 0)
		putchar('\n');
	if (headers && *g->path != '\0')
		printf("%s:\n", g->path);
	if (mode.timesort)
		qsort(g->items, g->len, sizeof(pd_entry_t), time_cmp);
	else if (mode.sizesort)
		qsort(g->items, g->len, sizeof(pd_entry_t), size_cmp);
	else
		qsort(g->items, g->len, sizeof(pd_entry_t), name_cmp);
	if (mode.long_ls)
		long_ls(g->items, g->len);
	else
		short_ls(g->items, g->len);
	if (!mode.one_column) {
		for (i = 0; i < g->len; i++)
			size += g->items[i].size;
		printf("%s bytes in %lu files", ultos(size, buf),
		       (unsigned long)g->len);
		if ((diskfree = pd_diskfree(diskpath)) >= 0)
			printf(", %s bytes free", ultos(diskfree, buf));
		putchar('\n');
	}
}

/**********************************************************************
 *
 *	subdir_cmp
 */
static int subdir_cmp(const void *v1, const void *v2)
{
	return pd_namecmp(*(char *const *)v1, *(char *const *)v2);
}

/**********************************************************************
 *
 *	ls_dir
 *
 * Lists files matching pattern in directory dir, recursively
 * if required. Empty dir means current directory.
 */
static void ls_dir(const char *dir, const char *pattern)
{
	pd_dir_t	*d;
	pd_entry_t	e;
	group_t		g;
	char		**subdirs = NULL;
	size_t		nsubdirs = 0, maxsubdirs = 0, i;

	if ((d = pd_opendir(dir)) == NULL) {
		error(*dir ? dir : ".", pd_strerror());
		return;
	}
	memset(&g, 0, sizeof(g));
	g.path = *dir ? (char *)dir : ".";
	while (pd_readdir(d, &e)) {
		if (pd_match(pattern, e.name) && selected(&e))
			add_item(&g, &e, e.name);
		if (mode.recurse && e.is_dir && !e.is_link
		    && (mode.all || !(e.hidden || e.name[0] == '.'))) {
			if (nsubdirs == maxsubdirs) {
				maxsubdirs = maxsubdirs ? 2 * maxsubdirs : 16;
				subdirs = pd_realloc(subdirs,
						maxsubdirs * sizeof(char *));
			}
			subdirs[nsubdirs++] = pd_join(dir, e.name);
		}
	}
	pd_closedir(d);
	print_group(&g, dir);
	free_group(&g);
	if (nsubdirs > 0) {
		qsort(subdirs, nsubdirs, sizeof(char *), subdir_cmp);
		if (mode.reverse)
			for (i = 0; i < nsubdirs / 2; i++) {
				char *tmp = subdirs[i];
				subdirs[i] = subdirs[nsubdirs-1-i];
				subdirs[nsubdirs-1-i] = tmp;
			}
		for (i = 0; i < nsubdirs; i++) {
			ls_dir(subdirs[i], pattern);
			free(subdirs[i]);
		}
	}
	free(subdirs);
}

/**********************************************************************
 *
 *	strip_slashes
 *
 * Removes trailing slashes but keeps root directory as it is.
 */
static char *strip_slashes(char *path)
{
	size_t	len = strlen(path);
	size_t	min = 1;

#ifdef _WIN32
	if (len >= 2 && path[1] == ':')
		min = 3;
#endif
	while (len > min && PD_ISSLASH(path[len-1]))
		path[--len] = '\0';
	return path;
}

/**********************************************************************
 *
 *	split_pattern
 *
 * Splits wildcard argument to directory and pattern.
 */
static void split_pattern(char *arg, char **dir, char **pattern)
{
	char	*base = pd_basename(arg);
	size_t	dirlen = base - arg;

	*pattern = base;
	*dir = memcpy(pd_malloc(dirlen + 1), arg, dirlen);
	(*dir)[dirlen] = '\0';
	strip_slashes(*dir);
}

/**********************************************************************
 *
 *	is_dir_arg
 *
 * Checks if a command line argument is a directory. A link to
 * a directory is listed as a directory.
 */
static int is_dir_arg(const char *path, pd_entry_t *e)
{
	pd_dir_t	*d;

	if (!e->is_link)
		return e->is_dir;
	if ((d = pd_opendir(path)) == NULL)
		return FALSE;
	pd_closedir(d);
	return TRUE;
}

/**********************************************************************
 *
 *	get_args
 */
static int get_args(int argc, char *argv[])
{
	int	i;
	char	*p;

	for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
		if (strcmp(argv[i], "--") == 0)
			return i + 1;
		for (p = argv[i] + 1; *p != '\0'; p++) {
			switch (*p) {
				case 'a': mode.all = TRUE; break;
				case 'd': mode.dirs = TRUE; break;
				case 'f': mode.files = TRUE; break;
				case 'l': mode.long_ls = TRUE; break;
				case 'r': mode.reverse = TRUE; break;
				case 'R': mode.recurse = TRUE; break;
				case 's': mode.sizes = TRUE; break;
				case 'S': mode.sizesort = TRUE; break;
				case 't': mode.timesort = TRUE; break;
				case '1': mode.one_column = TRUE; break;
				default:
					usage();
			}
		}
	}
	return i;
}

/**********************************************************************
 *
 *	main
 *
 * File arguments are listed first, then directories and patterns
 * in the order given.
 */
int main(int argc, char *argv[])
{
	int		i, first;
	group_t		files;
	pd_entry_t	e;
	char		*dir, *pattern;
	char		**args;
	static char	*dot[] = { "" };

	pd_init(&argc, &argv);
	setvbuf(stdout, NULL, _IOFBF, 64 * 1024);
	termwidth = pd_termwidth();
	first = get_args(argc, argv);
	args = argv + first;
	argc -= first;
	if (argc < 1) {
		args = dot;
		argc = 1;
	}
	headers = argc > 1 || mode.recurse;
	memset(&files, 0, sizeof(files));
	files.path = "";
	for (i = 0; i < argc; i++) {
		if (pd_haswild(args[i]) || *args[i] == '\0')
			continue;
		if (pd_stat(args[i], &e) != 0) {
			error(args[i], pd_strerror());
			args[i] = NULL;
		} else if (!is_dir_arg(args[i], &e)) {
			/* named files are listed even if hidden */
			if (!mode.dirs)
				add_item(&files, &e, args[i]);
			args[i] = NULL;
		}
	}
	print_group(&files, "");
	free_group(&files);
	for (i = 0; i < argc; i++) {
		if (args[i] == NULL)
			continue;
		if (pd_haswild(args[i])) {
			split_pattern(args[i], &dir, &pattern);
			if (pd_haswild(dir))
				error(args[i], "wildcards are allowed only "
				      "in the last path component");
			else
				ls_dir(dir, pattern);
			free(dir);
		} else
			ls_dir(strip_slashes(args[i]), "*");
	}
	fflush(stdout);
	return exit_code;
}
