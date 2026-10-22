/**********************************************************************\
 *
 *	WALK.C
 *
 * Collects file names from directory trees for -r. Directories are
 * walked recursively, the entries of each directory are sorted, so
 * the order does not depend on the file system. Symbolic links and
 * special files (FIFOs, devices, ...) inside the trees are skipped;
 * a path given on the command line is followed.
 *
\**********************************************************************/

#include "system.h"
#include "walk.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI		/* wingdi.h defines ERROR */
/* winnt.h defines MAXCHAR as 0x7f, keep ours */
#pragma push_macro("MAXCHAR")
#undef MAXCHAR
#include <windows.h>
#undef MAXCHAR
#pragma pop_macro("MAXCHAR")
#else
#include <dirent.h>
#endif

typedef enum {
	ENT_FILE,	/* regular file */
	ENT_DIR,	/* directory */
	ENT_SKIP	/* symbolic link or special file */
} ent_type_t;

typedef struct {
	char*		name;
	ent_type_t	type;
} entry_t;

/**********************************************************************
 *
 *	add_name
 *
 * Adds a file name to the list.
 */
static void add_name(filelist_t* fl, char* name)
{
	if (fl->n == fl->size) {
		fl->size = fl->size ? 2 * fl->size : 256;
		if ((fl->names = realloc(fl->names, fl->size * sizeof(char*))) == NULL)
			error("Out of memory", 3);
	}
	fl->names[fl->n++] = name;
}

/**********************************************************************
 *
 *	join_path
 *
 * Returns a new string dir + separator + name. Empty dir means the
 * current directory, then the result is just name.
 */
static char* join_path(char* dir, char* name)
{
	size_t	dlen = strlen(dir);
	char*	path;

	if ((path = malloc(dlen + strlen(name) + 2)) == NULL)
		error("Out of memory", 3);
	strcpy(path, dir);
	if (dlen > 0 && dir[dlen-1] != '/' && dir[dlen-1] != PATHSEP)
		path[dlen++] = PATHSEP;
	strcpy(path + dlen, name);
	return path;
}

/**********************************************************************
 *
 *	compare_entries
 */
static int compare_entries(const void* a, const void* b)
{
	return strcmp(((const entry_t*)a)->name, ((const entry_t*)b)->name);
}

/**********************************************************************
 *
 *	add_entry
 */
static void add_entry(entry_t** ents, int* n, int* size, const char* name,
		      ent_type_t type)
{
	if (*n == *size) {
		*size = *size ? 2 * *size : 64;
		if ((*ents = realloc(*ents, *size * sizeof(entry_t))) == NULL)
			error("Out of memory", 3);
	}
	if (((*ents)[*n].name = malloc(strlen(name) + 1)) == NULL)
		error("Out of memory", 3);
	strcpy((*ents)[*n].name, name);
	(*ents)[*n].type = type;
	++*n;
}

/**********************************************************************
 *
 *	read_dir
 *
 * Reads the entries of directory dir (empty means the current
 * directory). Returns FALSE if the directory can't be read.
 */
#ifdef _WIN32
static bool read_dir(char* dir, entry_t** ents, int* n, int* size)
{
	char*			pattern = join_path(dir, "*");
	WIN32_FIND_DATAA	fd;
	HANDLE			h;
	ent_type_t		type;

	h = FindFirstFileExA(pattern, FindExInfoBasic, &fd,
			     FindExSearchNameMatch, NULL,
			     FIND_FIRST_EX_LARGE_FETCH);
	free(pattern);
	if (h == INVALID_HANDLE_VALUE)
		return FALSE;
	do {
		if (strcmp(fd.cFileName, ".") == 0
		    || strcmp(fd.cFileName, "..") == 0)
			continue;
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			type = ENT_SKIP;	/* symbolic link or junction */
		else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			type = ENT_DIR;
		else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
			type = ENT_SKIP;
		else
			type = ENT_FILE;
		add_entry(ents, n, size, fd.cFileName, type);
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	return TRUE;
}
#else
static bool read_dir(char* dir, entry_t** ents, int* n, int* size)
{
	DIR*		d;
	struct dirent*	de;
	struct stat	st;
	ent_type_t	type;
	char*		path;

	if ((d = opendir(dir[0] ? dir : ".")) == NULL)
		return FALSE;
	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
#ifdef DT_DIR
		/* d_type saves a lstat call for each entry */
		if (de->d_type == DT_DIR)
			type = ENT_DIR;
		else if (de->d_type == DT_REG)
			type = ENT_FILE;
		else if (de->d_type != DT_UNKNOWN)
			type = ENT_SKIP;
		else
#endif
		{
			path = join_path(dir, de->d_name);
			if (lstat(path, &st) != 0)
				type = ENT_SKIP;
			else if (S_ISDIR(st.st_mode))
				type = ENT_DIR;
			else if (S_ISREG(st.st_mode))
				type = ENT_FILE;
			else
				type = ENT_SKIP;
			free(path);
		}
		add_entry(ents, n, size, de->d_name, type);
	}
	closedir(d);
	return TRUE;
}
#endif

/**********************************************************************
 *
 *	walk_dir
 *
 * Adds the files in directory dir and its subdirectories to the list.
 * Returns FALSE if some directory could not be read.
 */
static bool walk_dir(filelist_t* fl, char* dir)
{
	entry_t*	ents = NULL;
	int		n = 0;
	int		size = 0;
	int		i;
	bool		ok;
	char*		path;

	ok = read_dir(dir, &ents, &n, &size);
	qsort(ents, n, sizeof(entry_t), compare_entries);
	for (i = 0; i < n; i++) {
		if (ents[i].type != ENT_SKIP) {
			path = join_path(dir, ents[i].name);
			if (ents[i].type == ENT_DIR) {
				if (!walk_dir(fl, path))
					ok = FALSE;
				free(path);
			} else
				add_name(fl, path);
		}
		free(ents[i].name);
	}
	free(ents);
	return ok;
}

/**********************************************************************
 *
 *	is_dir
 *
 * Tests if path is a directory, following symbolic links.
 */
static bool is_dir(char* path)
{
#ifdef _WIN32
	DWORD	attr = GetFileAttributesA(path);

	return attr != INVALID_FILE_ATTRIBUTES
	       && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat	st;

	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

/**********************************************************************
 *
 *	walk_path
 *
 * Adds path to the list, or if it is a directory, the files under it.
 * Empty path means the current directory, and the names are then
 * relative to it. Returns FALSE if some directory could not be read.
 */
bool walk_path(filelist_t* fl, char* path)
{
	if (path[0] == '\0' || is_dir(path))
		return walk_dir(fl, path);
	add_name(fl, path);
	return TRUE;
}
