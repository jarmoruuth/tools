/**********************************************************************\
 *
 *	PDIR.H
 *
 * Portable directory, file and console utilities for Windows and
 * POSIX systems. Used by lj, rj and calcj. All file names are UTF-8
 * strings. In Windows the wide character API is used internally, so
 * any file name and paths longer than MAX_PATH work.
 *
\**********************************************************************/

#ifndef PDIR_H
#define PDIR_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define PD_SLASH	'\\'
#define PD_SLASHSTR	"\\"
#define PD_ISSLASH(c)	((c) == '\\' || (c) == '/')
#else
#define PD_SLASH	'/'
#define PD_SLASHSTR	"/"
#define PD_ISSLASH(c)	((c) == '/')
#endif

typedef struct {
	char		*name;		/* file name without directory */
	int		is_dir;		/* directory (Windows: also a link to one) */
	int		is_link;	/* symbolic link or junction */
	int		hidden;		/* Windows: hidden or system, POSIX: dot name */
	int		system;		/* Windows system attribute */
	int		readonly;	/* Windows read-only attribute */
	int		archive;	/* Windows archive attribute */
	unsigned	mode;		/* POSIX st_mode, 0 in Windows */
	uint64_t	size;
	int64_t		mtime;		/* modification time, seconds since 1970 */
} pd_entry_t;

typedef struct pd_dir pd_dir_t;

void		pd_init(int *argc, char ***argv);
void		*pd_malloc(size_t size);
void		*pd_realloc(void *ptr, size_t size);
char		*pd_strdup(const char *s);
char		*pd_join(const char *dir, const char *name);
char		*pd_basename(const char *path);
int		pd_haswild(const char *s);
int		pd_match(const char *pattern, const char *name);
int		pd_namecmp(const char *s1, const char *s2);
size_t		pd_strwidth(const char *s);

pd_dir_t	*pd_opendir(const char *path);
int		pd_readdir(pd_dir_t *dir, pd_entry_t *entry);
void		pd_closedir(pd_dir_t *dir);
int		pd_stat(const char *path, pd_entry_t *entry);

int		pd_remove(const char *path);
int		pd_rmdir(const char *path);
int		pd_make_writable(const char *path);
int64_t		pd_diskfree(const char *path);
const char	*pd_strerror(void);

int		pd_getkey(void);
int		pd_termwidth(void);

#endif /* PDIR_H */
