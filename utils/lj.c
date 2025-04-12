/**********************************************************************\
 *
 *		LJ.C
 *
 * Unix-like ls for MS-DOS. For more info see usage().
 *
 * Return values: 0, if files are found
 *		  1, if no files are found
 *		  2, if errors
 * 
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <process.h>
#include <ectype.h>
#include <sys\types.h>
#include <sys\stat.h>

#include <c.h>
#include <filelist.h>
#include <diruti.h>
#include <getopt.h>

#define SIZE_COLS	3

struct {
	char	reverse;
	char	timesort;
	char	long_ls;
	char	recurse;
	char	sizes;
	char	all;
	char	one_column;
	char	dirs;
	char	sizesort;
} mode = {
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE
};

static char lbuf[20], dbuf[20];

#ifdef __TURBOC__

unsigned _stklen = 16 * 1024;

#endif

/**********************************************************************
 *
 *	usage
 */
static void usage(void)
{
	printf(
"Usage: lj [-adflrRsSt1] [files and directories]\n"
"Options: -a  list all files\n"
"         -d  list only directories\n"
"         -f  list only files\n"
"         -l  long listing\n"
"         -r  reverse sort order\n"
"         -R  recurse subdirectories\n"
"         -s  print size of each file\n"
"         -S  sort by size\n"
"         -t  sort by time\n"
"         -1  one column format\n"
"If no arguments are given, *.* is assumed\n");
	exit(2);
}

/**********************************************************************
 *
 *	ls_malloc
 */
static void *ls_malloc(int size)
{
	register void *ptr;
	
	if ((ptr = malloc(size)) == NULL) {
		printf("Error: Out of heap space\n");
		exit(2);
	}
	return ptr;
}

/**********************************************************************
 *
 *	name_cmp 
 */
static int name_cmp(INFOITEM **p1, INFOITEM **p2)
{
	register int	retval;

	retval = strcmp((*p1)->name, (*p2)->name);
	return mode.reverse ? -retval : retval;
}

/**********************************************************************
 *
 *	sort_by_name
 */
static void sort_by_name(INFOITEM **ii, int len)
{
	qsort(ii, len, sizeof(INFOITEM **), name_cmp);
}

/**********************************************************************
 *
 *	size_cmp 
 */
static int size_cmp(INFOITEM **p1, INFOITEM **p2)
{
	register int	retval;
	long		diff;

	diff = (*p1)->size - (*p2)->size;
	if (diff > 0)
		retval = 1;
	else if (diff < 0)
		retval = -1;
	else
		retval = 0;
	return retval ? (mode.reverse ? retval : -retval)
		      : name_cmp(p1, p2);
}

/**********************************************************************
 *
 *	sort_by_size
 */
static void sort_by_size(INFOITEM **ii, int len)
{
	qsort(ii, len, sizeof(INFOITEM **), size_cmp);
}

/**********************************************************************
 *
 *	time_cmp 
 */
static int time_cmp(INFOITEM **pp1, INFOITEM **pp2)
{
	register INFOITEM	*p1 = *pp1;
	INFOITEM		*p2 = *pp2;
	register int		retval;

	if (!(retval = p1->year - p2->year)
	    && !(retval = p1->month - p2->month)
	    && !(retval = p1->day - p2->day)
	    && !(retval = p1->hour - p2->hour)
	    && !(retval = p1->min - p2->min))
		retval = p1->sec - p2->sec;
	return retval ? (mode.reverse ? retval : -retval)
		      : name_cmp(pp1, pp2);
}

/**********************************************************************
 *
 *	sort_by_time 
 */
static void sort_by_time(INFOITEM **ii, int len)
{
	qsort(ii, len, sizeof(INFOITEM **), time_cmp);
}

/**********************************************************************
 *
 *	ultos
 */
static char *ultos(ulong l, char *buf)
{
	ulong		tmp;
	int 		output = 0;
	char		*bp = buf;

	if (tmp = l / 1000000000UL) {
		bp += sprintf(bp, "%lu,", tmp);
		output = 1;
	}
	l -= tmp * 1000000000UL;
	tmp = l / 1000000UL;
	if (output || tmp) {
		bp += sprintf(bp, output ? "%03lu," : "%lu,", tmp);
		output = 1;
	}
	l -= tmp * 1000000UL;
	tmp = l / 1000UL;
	if (output || tmp) {
		bp += sprintf(bp, output ? "%03lu," : "%lu,", tmp);
		output = 1;
	}
	l -= tmp * 1000UL;
	sprintf(bp, output ? "%03lu" : "%lu", l);
	return buf;
}

/**********************************************************************
 *
 *	short_ls 
 */
static void short_ls(register INFOITEM **ii, int len)
{
        int maxnamelen = 0;
	int i, m;
	register unsigned* p;
        int name_cols;
        char format[20];

	if (mode.one_column) {
		for (; len--; ii++)
			printf("%s\n", (*ii)->name);
		return;
	}
        for (i = 0; i < len; i++) {
            if (strlen(ii[i]->name) > maxnamelen) {
                maxnamelen = strlen(ii[i]->name);
            }
        }
        name_cols = 79 / (maxnamelen + 1);
        if (name_cols < 1) {
            name_cols = 1;
        }
        p = ls_malloc(sizeof(unsigned) * (name_cols + 1));
	m = len / name_cols + 1;
	p[0] = 0;
	for (i=1; i < name_cols; i++) {
		p[i] = p[i-1] + m;
		if (p[i] >= len)
			p[i] = len;
	}
        p[name_cols] = len;
	i = m;
        sprintf(format, "%%-%ds ", maxnamelen);
	while (i-- > 0) {
            int j;
            for (j = 0; j < name_cols; j++) {
                printf(format, p[j] < p[j+1] ? ii[p[j]++]->name : "");
            }
            printf("\n");
        }
}

/**********************************************************************
 *
 *	size_ls 
 */
static void size_ls(register INFOITEM **ii, int len)
{
        int maxnamelen = 0;
        int maxsizelen;
        long maxsize = 0;
	int i, m;
	register unsigned* p;
        int name_cols;
        char format[20];
	char buf[20];

	if (mode.one_column) {
		for (; len--; ii++)
			printf("%s\n", (*ii)->name);
		return;
	}
        for (i = 0; i < len; i++) {
            if (strlen(ii[i]->name) > maxnamelen) {
                maxnamelen = strlen(ii[i]->name);
            }
            if (ii[i]->size > maxsize) {
                maxsize = ii[i]->size;
            }
        }
        ultos(maxsize, buf);
        maxsizelen = strlen(buf);
        name_cols = 79 / (maxnamelen + maxsizelen + 2);
        if (name_cols < 1) {
            name_cols = 1;
        }
        p = ls_malloc(sizeof(unsigned) * (name_cols + 1));
	m = len / name_cols + 1;
	p[0] = 0;
	for (i=1; i < name_cols; i++) {
		p[i] = p[i-1] + m;
		if (p[i] >= len)
			p[i] = len;
	}
        p[name_cols] = len;
	i = m;
        sprintf(format, " %%%ds %%-%ds", maxsizelen, maxnamelen);
	while (i-- > 0) {
            int j;
            for (j = 0; j < name_cols; j++) {
                if (p[j] < p[j+1]) {
                    printf(format,
                        ultos(ii[p[j]]->size, buf),
                        ii[p[j]]->name);
                    p[j]++;
                } else {
                    printf(format, "", "");
                }
            }
            printf("\n");
        }
}

/**********************************************************************
 *
 *	long_ls 
 */
static void long_ls(INFOITEM **ipp, int len)
{
	register int		attr;
	register INFOITEM	*ip;
	
	for (; len-- > 0; ipp++) {
		ip = *ipp;
		attr = ip->attr;
		printf("%c%c%c%c%c%c  %11s  %2u.%02u.%04u  %2u:%02u  %s\n",
			attr&DI_DIREC  ? 'd' : '-',
			attr&DI_ARCH   ? 'a' : '-',
			attr&DI_SYSTEM ? 's' : '-',
			attr&DI_HIDDEN ? 'h' : '-',
			'r',
			attr&DI_RDONLY ? '-' : 'w',
			ultos(ip->size, lbuf),
			ip->day,
			ip->month,
			ip->year,
			ip->hour,
			ip->min,
			ip->name);
	}
}

/**********************************************************************
 *
 *	select_ls 
 */
static void select_ls(register INFOITEM **ii, register int len)
{
	if (len < 1)
		return;
	/* Sort files */
	if (mode.timesort)
		sort_by_time(ii, len);
	else if (mode.sizesort)
		sort_by_size(ii, len);
	else
		sort_by_name(ii, len);
	/* Select correct display format */
	if (mode.long_ls)
		long_ls(ii, len);
	else if (mode.sizes)
		size_ls(ii, len);
	else
		short_ls(ii, len);
}	

/**********************************************************************
 *
 *	ls
 */
static int ls(INFOLIST *il)
{
	register int		i;
	int			len, nfiles, count = 0;
	INFOITEM		**iip;
	register INFOITEM	**iitmp;
	char			*path;
	ulong			size;
	int			disk;
	
	if (il == NULL || il->info == NULL)
		return 2;
	if ((len = il->len) < 1)
		return 1;
	iip = iitmp = il->info;
	for (i = 0; i < len; iip = iitmp) {
		if (!mode.all)
			for (; i < len && (*iip)->name[0] == '.'; i++, iip++)
				;
		if (i == len)
			break;
		for (nfiles = 0, size = 0, iitmp = iip, path = (*iitmp)->path;
		     i < len && strcmp(path, (*iitmp)->path) == 0; 
		     i++, nfiles++, size += (*iitmp)->size, iitmp++)
			if ((*iitmp)->attr & DI_DIREC)
				strcat((*iitmp)->name, "\\");
		if (path[1] == ':')
			disk = tolower(path[0]) - 'a' + 1;
		else
			disk = 0;
		if (count++ || i < len) {
			if (path[1] && !(path[1] == ':' && path[2] && !path[3]))
				path[strlen(path)-1] = '\0';
			printf("%s:\n", path);
		}
		select_ls (iip, nfiles);
		if (!mode.one_column)
			printf("%s bytes in %u files, %s bytes free\n", 
				ultos(size, lbuf), nfiles, 
				ultos(u_getdiskfree(disk), dbuf));
		if (i < len)
			printf("\n");
		
	}
	return 0;
}

/**********************************************************************
 *
 *	get_args
 *
 * Get argumets and return search attribute.
 */
static int get_args(int argc, char *argv[])
{
	register int	c;
	register int	search_attribute = DI_NORMAL | DI_DIREC;
	
	while ((c = getopt(argc, argv, "adflrRsSt1")) != EOF) {
		switch (c) {
			case 'a':
				mode.all = TRUE;
				search_attribute |=
					(DI_RDONLY | DI_HIDDEN | DI_SYSTEM);
				break;
			case 'f':
				search_attribute &= ~DI_DIREC;
				break;
			case 'd':
				mode.dirs = TRUE;
				break;
			case 'l':
				mode.long_ls = TRUE;
				break;
			case 'r':
				mode.reverse = TRUE;
				break;
			case 'R':
				mode.recurse = TRUE;
				break;
			case 's':
				mode.sizes = TRUE;
				break;
			case 'S':
				mode.sizesort = TRUE;
				break;
			case 't':
				mode.timesort = TRUE;
				break;
			case '1':
				mode.one_column = TRUE;
				break;
			default:
				usage();
		}
	}
	return search_attribute;
}

/**********************************************************************
 *
 *	SLASH
 */
#define SLASH(s)	((s) == '\\' || (s) == '/')

/**********************************************************************
 *
 *	APPEND
 */
#define APPEND(s1, s1len, s2, s2len) \
	strcat(strcpy(ls_malloc(s1len + s2len + 1), s1), s2)

/**********************************************************************
 *
 *	check_dir
 */
static char *check_dir(register char *s)
{
	struct stat	st;
	register int	len;
	
	if (strpbrk(s, "*?") != NULL)
		return s;
	len = strlen(s);
	if ((len == 1 && SLASH(s[0]))
	    || (s[1] == ':' && (len == 2
	    			|| (len == 3 && SLASH(s[2])))))
		return APPEND(s, len, "*.*", 3);
	else if (stat(s, &st) == 0 && st.st_mode & S_IFDIR)
		return APPEND(s, len, "\\*.*", 4);
	else
		return s;
}

/**********************************************************************
 *
 *	main 
 */
int main(register int argc, register char *argv[])
{
	int	count, firstfile;
	int	search_attribute;

	search_attribute = get_args(argc, argv);
	firstfile = optind;
	count = argc - optind;
	if  (count < 1) {
		argv[optind] = "*.*";
		count = 1;
	} else
		for (; optind < argc; optind++)
			argv[optind] = check_dir(argv[optind]);
	return mode.dirs
		? ls(read_info_no_sort_dirs_only(count, &argv[firstfile], 
			search_attribute, mode.recurse))
		: ls(read_info_no_sort(count, &argv[firstfile], 
			search_attribute, mode.recurse))
		;
}
