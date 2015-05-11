/**********************************************************************\
 *
 *		FILELIST.C
 *
 * Utility functions to read and convert array of file patterns to
 * array of files. File patterns can contain wildcards. Module contains 
 * routines to read only file names or also some other info about given 
 * files. Return value is struct, that contains number of entries found 
 * and array of pointers to requested info. Array is terminated by NULL 
 * entry. In case of an error routines return NULL. All found file names 
 * are converted to lower case.
 *
 *	J.Ruuth 03-Jul-1988
\**********************************************************************/

#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>
#include <ectype.h>
#ifdef __TURBOC__
#include <alloc.h>
#else
#include <malloc.h>
#endif

#include "diruti.h"
#include "filelist.h"

#define	TRUE		1
#define	FALSE		0

#define	N_NEW		25	/* How many list items to allocate at once */

typedef struct readinfo {
	int	attr;
	char	sort;
	char	dir_only;
	char	recurse;
	char	save_info;
	int	last;
	int	max;
	void	**list;
} READINFO;

/**********************************************************************
 *
 *		ri_malloc
 */
static void * ri_malloc(int size)
{
	register void	*ptr;
	
	ptr = malloc(size);
	if (ptr == NULL) {
		cputs("\n\rERROR in file filelist.c, function ri_malloc: "
		      "Out of heap space\r\n");
		exit(3);
	}
	return ptr;
}

/**********************************************************************
 *
 *		ri_realloc
 */
static void * ri_realloc(register void *ptr, int newsize)
{
	ptr = realloc(ptr, newsize);
	if (ptr == NULL) {
		cputs("\n\rERROR in file filelist.c, function ri_realloc: "
		      "Out of heap space\r\n");
		exit(3);
	}
	return ptr;
}

/**********************************************************************
 *
 *		str_lower
 */
static void str_lower(register char *s)
{
	register int	ch;
	
	while ((ch = *s) != '\0')
		*s++ = tolower(ch);
}

/**********************************************************************
 *
 *		name_list_cmp
 */
static int name_list_cmp(char **s1, char **s2)
{
	return strcmp(*s1, *s2);
}

/**********************************************************************
 *
 *		info_list_cmp
 */
static int info_list_cmp(INFOITEM **p1, INFOITEM **p2)
{
	register int	retval;

	retval = strcmp((*p1)->path, (*p2)->path);
	if (retval == 0)
		retval = strcmp((*p1)->name, (*p2)->name);
	return retval;
}

/**********************************************************************
 *
 *		sort_info_list
 */
void sort_info_list(INFOITEM **infolist, int len)
{
	qsort(infolist, len, sizeof(void **), info_list_cmp);
}

/**********************************************************************
 *
 *		sort_name_list
 */
void sort_name_list(char **namelist, int len)
{
	qsort(namelist, len, sizeof(void **), name_list_cmp);
}

/**********************************************************************
 *
 *		add_info_list
 */
static void add_info_list(register READINFO *ri, char *path, 
				 u_dirinfo_t *dirinfo)
{
	FTIME			*ftime;
	FDATE			*fdate;
	register INFOITEM	*il;
	
	if (ri->last == ri->max) {
		ri->max += N_NEW;
		ri->list = ri_realloc(ri->list, (ri->max+1)*sizeof(void *));
	}
	ri->last++;
	ri->list[ri->last] = il = ri_malloc(sizeof(INFOITEM));
	il->path = ri_malloc(strlen(path) + 1);
	strcpy(il->path, path);
	il->name = ri_malloc(strlen(dirinfo->di_name) + 1 + 1);
	strcpy(il->name, dirinfo->di_name);
	il->attr = dirinfo->di_attrib;
	ftime = (FTIME *)&(dirinfo->di_modtime);
	fdate = (FDATE *)&(dirinfo->di_moddate);
	il->sec = ftime->twosecs;
	il->min = ftime->minutes;
	il->hour = ftime->hours;
	il->day = fdate->day;
	il->month = fdate->month;
	il->year = fdate->year + 1980;
	il->size = dirinfo->di_size;
}

/**********************************************************************
 *
 *	add_name_list
 */
static void add_name_list(register READINFO *ri, char *path, char *name)
{
	register char	*item;

	if (ri->last == ri->max) {
		ri->max += N_NEW;
		ri->list = ri_realloc(ri->list, (ri->max+1)*sizeof(void *));
	}
	ri->last++;
	item = ri_malloc(strlen(path) + strlen(name) + 1 + 1);
	ri->list[ri->last] = item;
	strcat(strcpy(item, path), name);
}

/************************************************************************
 *
 *		get_one_pat
 *
 * Read files matching fname, recursively if required.
 */
static void get_one_pat(char *fname, READINFO *ri)
{
	register char	*path;
	char		*fpath,
			*file,
			*full_path;
	register int	done;
	char		*fileptr;
	u_dirinfo_t	dirinfo;
	int		fname_len, file_len, path_len;

	fname_len = strlen(fname);
	fpath = ri_malloc(fname_len < 2 ? 3 : fname_len + 1);
	strcpy(fpath, fname);
	fileptr = u_splitpath(fpath);
	file_len = strlen(fileptr);
	if (file_len < 12)
		file_len = 12;
	file = ri_malloc(file_len + 1);
	strcpy(file, fileptr);
	*fileptr = '\0';
	path_len = strlen(fpath);
	if (path_len == 0)
		path_len = 2;
	path = ri_malloc(path_len + 12 + 1);
	strcpy(path, fpath);
	full_path = ri_malloc(path_len + 12 + 1 + file_len + 1);
	if (ri->recurse) {
		strcat(path, "*.*");
		done = u_findfirst(path, &dirinfo, DI_DIREC);
		for (; !done; done = u_findnext(&dirinfo)) {
			if ((dirinfo.di_attrib & DI_DIREC)
			    &&  strcmp(dirinfo.di_name, ".")
			    &&  strcmp(dirinfo.di_name, ".."))
			{
			  	str_lower(dirinfo.di_name);
				get_one_pat(
				  strcat(
				    strcat(
				      strcat(
				        strcpy(full_path, fpath),
				        dirinfo.di_name),
				      "\\"),
				    file), ri);
			}
		}
		strcpy(path, fpath);
	}
	if (!*fpath)
		strcpy(fpath, ".\\");
	strcat(path, file);
	done = u_findfirst(path, &dirinfo, ri->attr);
	for (; !done; done = u_findnext(&dirinfo)) {
		str_lower(dirinfo.di_name);
		if (ri->dir_only && !(dirinfo.di_attrib & DI_DIREC))
			continue;
		if (ri->save_info)
			add_info_list(ri, fpath, &dirinfo);
		else
			add_name_list(ri, fpath, dirinfo.di_name);
	}
	free(fpath);
	free(file);
	free(path);
	free(full_path);
}

/**********************************************************************
 *
 *		filelist
 *
 * Create list of filenames from given list of file patterns.
 */
static void * filelist(register int count, register char *filepat[], 
			      READINFO *ri)
{
	ri->list = ri_malloc(sizeof(void *) * N_NEW);
	ri->max = N_NEW - 1;
	ri->last = -1;
	while (count--) {
		str_lower(*filepat);
		get_one_pat(*filepat++, ri);
	}
	ri->last++;	/* now ri->last is list len */
	ri->list = ri_realloc(ri->list, sizeof(void *) * (ri->last+2));
	ri->list[ri->last] = NULL;
	if (ri->sort) {
		if (ri->save_info)
			sort_info_list(ri->list, ri->last);
		else
			sort_name_list(ri->list, ri->last);
	}
	return ri->list;
}

/**********************************************************************
 *
 *		do_read_names
 *
 * Do real file name reading.
 */
static NAMELIST *do_read_names(int len, char *filepat[], int attr, 
			       int recurse, int sort, int dirs)
{
	READINFO	ri;
	NAMELIST	*nl = ri_malloc(sizeof(NAMELIST));
	
	ri.attr = attr;
	ri.sort = sort;
	ri.dir_only = dirs;
	ri.recurse = recurse;
	ri.save_info = FALSE;
	nl->name = filelist(len, filepat, &ri);
	nl->len = ri.last;	/* ri->last points to last null entry */
	return nl;
}

/**********************************************************************
 *
 *		read_names
 *
 * Read only file names.
 */
NAMELIST *read_names(int len, char *filepat[], int attr, int recurse)
{
	return do_read_names(len, filepat, attr, recurse, TRUE, FALSE);
}

/**********************************************************************
 *
 *		read_names_no_sort
 *
 * Read only file names.
 */
NAMELIST *read_names_no_sort(int len, char *filepat[], int attr, int recurse)
{
	return do_read_names(len, filepat, attr, recurse, FALSE, FALSE);
}

/**********************************************************************
 *
 *		read_names_no_sort_dirs_only
 *
 * Read only directory names.
 */
NAMELIST *read_names_no_sort_dirs_only(int len, char *filepat[], int attr, int recurse)
{
	return do_read_names(len, filepat, attr, recurse, FALSE, TRUE);
}

/**********************************************************************
 *
 *		do_read_info
 */
static INFOLIST *do_read_info(int len, char *filepat[], int attr, 
			      int recurse, int sort, int dirs)
{
	READINFO	ri;
	INFOLIST	*il = ri_malloc(sizeof(INFOLIST));
	
	ri.attr = attr;
	ri.sort = sort;
	ri.dir_only = dirs;
	ri.recurse = recurse;
	ri.save_info = TRUE;
	il->info = filelist(len, filepat, &ri);
	il->len = ri.last;	/* ri->last points to last null entry */
	return il;
}

/**********************************************************************
 *
 *		read_info
 */
INFOLIST *read_info(int len, char *filepat[], int attr, int recurse)
{
	return do_read_info(len, filepat, attr, recurse, TRUE, FALSE);
}

/**********************************************************************
 *
 *		read_info_no_sort
 */
INFOLIST *read_info_no_sort(int len, char *filepat[], int attr, int recurse)
{
	return do_read_info(len, filepat, attr, recurse, FALSE, FALSE);
}

/**********************************************************************
 *
 *		read_info_no_sort_dirs_only
 */
INFOLIST *read_info_no_sort_dirs_only(int len, char *filepat[], int attr, int recurse)
{
	return do_read_info(len, filepat, attr, recurse, FALSE, TRUE);
}

/**********************************************************************
 *
 *		free_name_list
 */
void free_name_list(NAMELIST *nl)
{
	register char	**namelist;
	
	namelist = nl->name;
	while (*namelist != NULL)
		free(*namelist++);
	free(nl->name);
	free(nl);
}

/**********************************************************************
 *
 *		free_info_list
 */
void free_info_list(INFOLIST *il)
{
	register INFOITEM	**infolist;
	
	infolist = il->info;
	while (*infolist != NULL) {
		free((*infolist)->path);
		free((*infolist)->name);
		free(*infolist++);
	}
	free(il->info);
	free(il);
}

#ifdef TEST

void main(int argc, char *argv[])
{
	NAMELIST	*nl;
	INFOLIST	*il;
	char 		**namelist;
	INFOITEM	**infolist;
	
	printf("coreleft = %lu\n", (long)coreleft());
	nl = read_names (argc-1, argv+1, 0, TRUE);
	namelist = nl->name;
	while (*namelist != NULL) {
		puts(*namelist++);
	}
	printf("coreleft = %lu\n", (long)coreleft());
	free_name_list(nl);
	printf("coreleft = %lu\n", (long)coreleft());
	il = read_info(argc-1, argv+1, 0, TRUE);
	infolist = il->info;
	while (*infolist != NULL) {
		printf("%s!%s\n", (*infolist)->path, (*infolist)->name);
		infolist++;
	}
	printf("coreleft = %lu\n", (long)coreleft());
	free_info_list(il);
	printf("coreleft = %lu\n", (long)coreleft());
}

#endif
