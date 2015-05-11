/**********************************************************************\
 *
 *		FILELIST.H
 *
 * Utility functions to read and convert array of file patterns to
 * array of files. File patterns can contain wildcards. Module contains 
 * routines to read only file names or also some other info about given 
 * files. Return value is a struct, that contains number of entries found 
 * and array of pointers to requested info. Array is terminated by NULL 
 * entry. In case of an error routines return NULL. All found file names 
 * are converted to lower case.
 *
 *	J.Ruuth 03-Jul-1988
\**********************************************************************/

typedef struct infoitem {
	char	*path;
	char	*name;
	char	attr;
	char	sec;
	char	min;
	char	hour;
	char	day;
	char	month;
	int	year;
	long	size;
} INFOITEM;

typedef struct infolist {
	INFOITEM	**info;
	int		len;
} INFOLIST;

typedef struct namelist {
	char	**name;
	int	len;
} NAMELIST;

/* Function declarations */

NAMELIST *read_names(int len, char *filepat[], int attr, int recurse);
NAMELIST *read_names_no_sort(int len, char *filepat[], int attr, int recurse);
NAMELIST *read_names_no_sort_dirs_only(int len, char *filepat[], int attr, int recurse);
INFOLIST *read_info(int len, char *filepat[], int attr, int recurse);
INFOLIST *read_info_no_sort(int len, char *filepat[], int attr, int recurse);
INFOLIST *read_info_no_sort_dirs_only(int len, char *filepat[], int attr, int recurse);
void sort_name_list(char **namelist, int len);
void sort_info_list(INFOITEM **infolist, int len);
void free_name_list(NAMELIST *nl);
void free_info_list(INFOLIST *il);
