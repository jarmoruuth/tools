/**********************************************************************\
 *
 *	WALK.H
 *
 * Collecting file names from directory trees for -r.
 *
\**********************************************************************/

typedef struct {
	char**	names;		/* file names */
	int	n;		/* number of names */
	int	size;		/* allocated size of names */
} filelist_t;

extern bool walk_path(filelist_t* fl, char* path);
