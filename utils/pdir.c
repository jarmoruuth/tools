/**********************************************************************\
 *
 *	PDIR.C
 *
 * Portable directory, file and console utilities for Windows and
 * POSIX systems. See pdir.h.
 *
\**********************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#else
#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdir.h"

/**********************************************************************
 *
 *	pd_malloc
 */
void *pd_malloc(size_t size)
{
	void	*ptr;

	if ((ptr = malloc(size)) == NULL) {
		fprintf(stderr, "Error: Out of memory\n");
		exit(3);
	}
	return ptr;
}

/**********************************************************************
 *
 *	pd_realloc
 */
void *pd_realloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL) {
		fprintf(stderr, "Error: Out of memory\n");
		exit(3);
	}
	return ptr;
}

/**********************************************************************
 *
 *	pd_strdup
 */
char *pd_strdup(const char *s)
{
	size_t	len = strlen(s) + 1;

	return memcpy(pd_malloc(len), s, len);
}

/**********************************************************************
 *
 *	pd_join
 *
 * Returns a new string dir/name. Empty dir means current directory.
 */
char *pd_join(const char *dir, const char *name)
{
	size_t	dlen = strlen(dir);
	size_t	nlen = strlen(name);
	char	*path = pd_malloc(dlen + nlen + 2);
	char	*p = path;

	memcpy(p, dir, dlen);
	p += dlen;
	if (dlen > 0 && !PD_ISSLASH(dir[dlen-1])
#ifdef _WIN32
	    && !(dlen == 2 && dir[1] == ':')
#endif
	    )
		*p++ = PD_SLASH;
	memcpy(p, name, nlen + 1);
	return path;
}

/**********************************************************************
 *
 *	pd_basename
 *
 * Returns pointer to the file name part of path.
 */
char *pd_basename(const char *path)
{
	const char	*base = path;

	for (; *path != '\0'; path++)
		if (PD_ISSLASH(*path)
#ifdef _WIN32
		    || *path == ':'
#endif
		    )
			base = path + 1;
	return (char *)base;
}

/**********************************************************************
 *
 *	pd_haswild
 */
int pd_haswild(const char *s)
{
	return strpbrk(s, "*?") != NULL;
}

/**********************************************************************
 *
 *	fold
 *
 * Case folding for file name comparison. Only ASCII letters are
 * folded, UTF-8 bytes are compared as they are.
 */
static int fold(int c)
{
	return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : (unsigned char)c;
}

#ifdef _WIN32
#define CHAREQ(a, b)	(fold(a) == fold(b))
#else
#define CHAREQ(a, b)	((a) == (b))
#endif

/**********************************************************************
 *
 *	u8next
 *
 * Skips one UTF-8 character.
 */
static const char *u8next(const char *s)
{
	s++;
	while ((*s & 0xC0) == 0x80)
		s++;
	return s;
}

/**********************************************************************
 *
 *	pd_match
 *
 * Matches name against a pattern with * and ? wildcards. Pattern *.*
 * matches all names like in MS-DOS. Matching is case insensitive in
 * Windows.
 */
int pd_match(const char *p, const char *s)
{
	const char	*pstar = NULL;
	const char	*sstar = NULL;

	if (strcmp(p, "*.*") == 0)
		p = "*";
	while (*s != '\0') {
		if (*p == '*') {
			pstar = ++p;
			sstar = s;
		} else if (*p == '?') {
			p++;
			s = u8next(s);
		} else if (*p != '\0' && CHAREQ(*p, *s)) {
			p++;
			s++;
		} else if (pstar != NULL) {
			p = pstar;
			s = sstar = u8next(sstar);
		} else
			return 0;
	}
	while (*p == '*')
		p++;
	return *p == '\0';
}

/**********************************************************************
 *
 *	pd_namecmp
 *
 * Compares file names for sorting: case insensitive first, then
 * case sensitive to get a stable order.
 */
int pd_namecmp(const char *s1, const char *s2)
{
	const unsigned char	*p1 = (const unsigned char *)s1;
	const unsigned char	*p2 = (const unsigned char *)s2;

	while (*p1 != '\0' && fold(*p1) == fold(*p2)) {
		p1++;
		p2++;
	}
	if (fold(*p1) != fold(*p2))
		return fold(*p1) - fold(*p2);
	return strcmp(s1, s2);
}

/**********************************************************************
 *
 *	pd_strwidth
 *
 * Returns display width of an UTF-8 string, one column per character.
 */
size_t pd_strwidth(const char *s)
{
	size_t	width = 0;

	for (; *s != '\0'; s++)
		if ((*s & 0xC0) != 0x80)
			width++;
	return width;
}

#ifdef _WIN32

/**********************************************************************
 *
 *		Windows implementation
 *
 **********************************************************************/

struct pd_dir {
	HANDLE			handle;
	int			first;
	WIN32_FIND_DATAW	fd;
	char			*name;
	size_t			namesize;
};

static DWORD	last_error;
static UINT	old_output_cp;

/**********************************************************************
 *
 *	to_utf8
 */
static char *to_utf8(const wchar_t *w, char *buf, size_t *bufsize)
{
	int	n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);

	if (buf == NULL || *bufsize < (size_t)n) {
		*bufsize = n + 64;
		buf = pd_realloc(buf, *bufsize);
	}
	WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, n, NULL, NULL);
	return buf;
}

/**********************************************************************
 *
 *	wpath
 *
 * Converts an UTF-8 path to a wide character path. Long paths are
 * made absolute and prefixed with \\?\ to pass the MAX_PATH limit.
 */
static wchar_t *wpath(const char *path)
{
	int	n;
	DWORD	len;
	wchar_t	*w, *full, *p;

	if (*path == '\0')
		path = ".";
	n = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
	w = pd_malloc(n * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, path, -1, w, n);
	if (wcsncmp(w, L"\\\\?\\", 4) == 0)
		return w;
	/* the limit is for the full path, also when path is relative */
	len = GetFullPathNameW(w, 0, NULL, NULL);
	if (len == 0 || len < MAX_PATH - 12)
		return w;
	full = pd_malloc((len + 8) * sizeof(wchar_t));
	p = full + 8;
	len = GetFullPathNameW(w, len, p, NULL);
	free(w);
	if (p[0] == '\\' && p[1] == '\\') {
		/* \\server\share -> \\?\UNC\server\share */
		p -= 6;
		memcpy(p, L"\\\\?\\UNC", 7 * sizeof(wchar_t));
	} else {
		p -= 4;
		memcpy(p, L"\\\\?\\", 4 * sizeof(wchar_t));
	}
	memmove(full, p, (wcslen(p) + 1) * sizeof(wchar_t));
	return full;
}

/**********************************************************************
 *
 *	restore_cp
 */
static void restore_cp(void)
{
	if (old_output_cp != 0)
		SetConsoleOutputCP(old_output_cp);
}

/**********************************************************************
 *
 *	ctrl_handler
 */
static BOOL WINAPI ctrl_handler(DWORD type)
{
	(void)type;
	restore_cp();
	return FALSE;
}

/**********************************************************************
 *
 *	pd_init
 *
 * Gets command line arguments as UTF-8, sets console output
 * to UTF-8 and stdout and stderr to binary mode.
 */
void pd_init(int *argc, char ***argv)
{
	int	i, n;
	wchar_t	**wargv;
	char	**av;
	size_t	size;

	wargv = CommandLineToArgvW(GetCommandLineW(), &n);
	if (wargv != NULL) {
		av = pd_malloc((n + 1) * sizeof(char *));
		for (i = 0; i < n; i++)
			av[i] = to_utf8(wargv[i], NULL, &size);
		av[n] = NULL;
		LocalFree(wargv);
		*argc = n;
		*argv = av;
	}
	old_output_cp = GetConsoleOutputCP();
	if (old_output_cp != 0 && old_output_cp != CP_UTF8) {
		SetConsoleOutputCP(CP_UTF8);
		atexit(restore_cp);
		SetConsoleCtrlHandler(ctrl_handler, TRUE);
	} else
		old_output_cp = 0;
	/* LF line ends like in Unix, also in Git Bash pipes */
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);
}

/**********************************************************************
 *
 *	filetime_to_unix
 */
static int64_t filetime_to_unix(FILETIME ft)
{
	int64_t	t = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

	return t / 10000000 - 11644473600LL;
}

/**********************************************************************
 *
 *	set_entry
 */
static void set_entry(pd_entry_t *e, DWORD attr, DWORD reparse_tag,
		      uint64_t size, FILETIME mtime)
{
	e->is_dir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	/* Other reparse points like OneDrive files are normal files */
	e->is_link = (attr & FILE_ATTRIBUTE_REPARSE_POINT)
		     && (reparse_tag == IO_REPARSE_TAG_SYMLINK
			 || reparse_tag == IO_REPARSE_TAG_MOUNT_POINT);
	e->hidden = (attr & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) != 0;
	e->system = (attr & FILE_ATTRIBUTE_SYSTEM) != 0;
	e->readonly = (attr & FILE_ATTRIBUTE_READONLY) != 0;
	e->archive = (attr & FILE_ATTRIBUTE_ARCHIVE) != 0;
	e->mode = 0;
	e->size = e->is_dir ? 0 : size;
	e->mtime = filetime_to_unix(mtime);
}

/**********************************************************************
 *
 *	pd_opendir
 */
pd_dir_t *pd_opendir(const char *path)
{
	pd_dir_t	*d;
	char		*pattern;
	wchar_t		*w;

	pattern = pd_join(*path ? path : ".", "*");
	w = wpath(pattern);
	free(pattern);
	d = pd_malloc(sizeof(pd_dir_t));
	d->handle = FindFirstFileExW(w, FindExInfoBasic, &d->fd,
				     FindExSearchNameMatch, NULL,
				     FIND_FIRST_EX_LARGE_FETCH);
	free(w);
	if (d->handle == INVALID_HANDLE_VALUE) {
		last_error = GetLastError();
		free(d);
		return NULL;
	}
	d->first = 1;
	d->name = NULL;
	d->namesize = 0;
	return d;
}

/**********************************************************************
 *
 *	pd_readdir
 *
 * Returns 1 and fills entry or returns 0 at the end of directory.
 * Entries . and .. are skipped. Entry name is valid until the next
 * call.
 */
int pd_readdir(pd_dir_t *d, pd_entry_t *e)
{
	WIN32_FIND_DATAW	*fd = &d->fd;

	for (;;) {
		if (!d->first && !FindNextFileW(d->handle, fd))
			return 0;
		d->first = 0;
		if (wcscmp(fd->cFileName, L".") != 0
		    && wcscmp(fd->cFileName, L"..") != 0)
			break;
	}
	d->name = to_utf8(fd->cFileName, d->name, &d->namesize);
	e->name = d->name;
	set_entry(e, fd->dwFileAttributes, fd->dwReserved0,
		  ((uint64_t)fd->nFileSizeHigh << 32) | fd->nFileSizeLow,
		  fd->ftLastWriteTime);
	return 1;
}

/**********************************************************************
 *
 *	pd_closedir
 */
void pd_closedir(pd_dir_t *d)
{
	FindClose(d->handle);
	free(d->name);
	free(d);
}

/**********************************************************************
 *
 *	pd_stat
 *
 * Gets info of path. Links are not followed. Entry name points
 * to the file name part of path.
 */
int pd_stat(const char *path, pd_entry_t *e)
{
	WIN32_FILE_ATTRIBUTE_DATA	fa;
	WIN32_FIND_DATAW		fd;
	HANDLE				h;
	DWORD				tag = 0;
	wchar_t				*w = wpath(path);

	if (!GetFileAttributesExW(w, GetFileExInfoStandard, &fa)) {
		last_error = GetLastError();
		free(w);
		return -1;
	}
	if (fa.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		h = FindFirstFileW(w, &fd);
		if (h != INVALID_HANDLE_VALUE) {
			tag = fd.dwReserved0;
			FindClose(h);
		}
	}
	free(w);
	e->name = pd_basename(path);
	set_entry(e, fa.dwFileAttributes, tag,
		  ((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow,
		  fa.ftLastWriteTime);
	return 0;
}

/**********************************************************************
 *
 *	pd_remove
 *
 * Removes a file or a link. A directory link is removed with
 * RemoveDirectory, which removes the link and not the target.
 */
int pd_remove(const char *path)
{
	wchar_t	*w = wpath(path);
	DWORD	attr = GetFileAttributesW(w);
	BOOL	ok;

	if (attr != INVALID_FILE_ATTRIBUTES
	    && (attr & FILE_ATTRIBUTE_DIRECTORY))
		ok = RemoveDirectoryW(w);
	else
		ok = DeleteFileW(w);
	if (!ok)
		last_error = GetLastError();
	free(w);
	return ok ? 0 : -1;
}

/**********************************************************************
 *
 *	pd_rmdir
 */
int pd_rmdir(const char *path)
{
	wchar_t	*w = wpath(path);
	BOOL	ok = RemoveDirectoryW(w);

	if (!ok)
		last_error = GetLastError();
	free(w);
	return ok ? 0 : -1;
}

/**********************************************************************
 *
 *	pd_make_writable
 */
int pd_make_writable(const char *path)
{
	wchar_t	*w = wpath(path);
	DWORD	attr = GetFileAttributesW(w);
	BOOL	ok = attr != INVALID_FILE_ATTRIBUTES
		     && SetFileAttributesW(w, attr & ~(FILE_ATTRIBUTE_READONLY
						      |FILE_ATTRIBUTE_HIDDEN
						      |FILE_ATTRIBUTE_SYSTEM));
	if (!ok)
		last_error = GetLastError();
	free(w);
	return ok ? 0 : -1;
}

/**********************************************************************
 *
 *	pd_diskfree
 *
 * Returns free bytes in the disk containing directory path, or -1.
 */
int64_t pd_diskfree(const char *path)
{
	ULARGE_INTEGER	avail;
	wchar_t		*w = wpath(path);
	BOOL		ok = GetDiskFreeSpaceExW(w, &avail, NULL, NULL);

	free(w);
	return ok ? (int64_t)avail.QuadPart : -1;
}

/**********************************************************************
 *
 *	pd_strerror
 *
 * Returns error message of the last failed pd_ call.
 */
const char *pd_strerror(void)
{
	static char	*buf;
	static size_t	bufsize;
	wchar_t		msg[512];
	size_t		len;

	if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM
			    |FORMAT_MESSAGE_IGNORE_INSERTS,
			    NULL, last_error, 0, msg, 512, NULL))
		swprintf(msg, 512, L"Error %lu", (unsigned long)last_error);
	len = wcslen(msg);
	while (len > 0 && (msg[len-1] == '\r' || msg[len-1] == '\n'
			   || msg[len-1] == '.'))
		msg[--len] = '\0';
	buf = to_utf8(msg, buf, &bufsize);
	return buf;
}

/**********************************************************************
 *
 *	pd_getkey
 *
 * Reads one key from the console without waiting for Enter.
 */
int pd_getkey(void)
{
	int	c = _getch();

	if (c == 0 || c == 0xE0) {
		/* function or arrow key */
		_getch();
		return 0;
	}
	return c;
}

/**********************************************************************
 *
 *	pd_termwidth
 */
int pd_termwidth(void)
{
	CONSOLE_SCREEN_BUFFER_INFO	ci;
	char				*s;

	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci))
		return ci.srWindow.Right - ci.srWindow.Left + 1;
	if ((s = getenv("COLUMNS")) != NULL && atoi(s) > 0)
		return atoi(s);
	return 80;
}

#else /* _WIN32 */

/**********************************************************************
 *
 *		POSIX implementation
 *
 **********************************************************************/

struct pd_dir {
	DIR	*dir;
	char	*path;
	char	*name;
};

static int	last_errno;

/**********************************************************************
 *
 *	pd_init
 */
void pd_init(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;
}

/**********************************************************************
 *
 *	set_entry
 */
static void set_entry(pd_entry_t *e, const char *name, struct stat *st)
{
	e->name = (char *)name;
	e->is_dir = S_ISDIR(st->st_mode);
	e->is_link = S_ISLNK(st->st_mode);
	e->hidden = name[0] == '.';
	e->system = 0;
	e->readonly = 0;
	e->archive = 0;
	e->mode = st->st_mode;
	e->size = e->is_dir ? 0 : (uint64_t)st->st_size;
	e->mtime = st->st_mtime;
}

/**********************************************************************
 *
 *	pd_opendir
 */
pd_dir_t *pd_opendir(const char *path)
{
	pd_dir_t	*d;
	DIR		*dir;

	if (*path == '\0')
		path = ".";
	if ((dir = opendir(path)) == NULL) {
		last_errno = errno;
		return NULL;
	}
	d = pd_malloc(sizeof(pd_dir_t));
	d->dir = dir;
	d->path = pd_strdup(path);
	d->name = NULL;
	return d;
}

/**********************************************************************
 *
 *	pd_readdir
 *
 * Returns 1 and fills entry or returns 0 at the end of directory.
 * Entries . and .. are skipped. Entry name is valid until the next
 * call. If the file has vanished, size and time are zero.
 */
int pd_readdir(pd_dir_t *d, pd_entry_t *e)
{
	struct dirent	*de;
	struct stat	st;

	do {
		if ((de = readdir(d->dir)) == NULL)
			return 0;
	} while (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0);
	free(d->name);
	d->name = pd_join(d->path, de->d_name);
	if (lstat(d->name, &st) != 0)
		memset(&st, 0, sizeof(st));
	set_entry(e, pd_basename(d->name), &st);
	return 1;
}

/**********************************************************************
 *
 *	pd_closedir
 */
void pd_closedir(pd_dir_t *d)
{
	closedir(d->dir);
	free(d->path);
	free(d->name);
	free(d);
}

/**********************************************************************
 *
 *	pd_stat
 *
 * Gets info of path. Links are not followed. Entry name points
 * to the file name part of path.
 */
int pd_stat(const char *path, pd_entry_t *e)
{
	struct stat	st;

	if (lstat(path, &st) != 0) {
		last_errno = errno;
		return -1;
	}
	set_entry(e, pd_basename(path), &st);
	return 0;
}

/**********************************************************************
 *
 *	pd_remove
 */
int pd_remove(const char *path)
{
	if (unlink(path) != 0) {
		last_errno = errno;
		return -1;
	}
	return 0;
}

/**********************************************************************
 *
 *	pd_rmdir
 */
int pd_rmdir(const char *path)
{
	if (rmdir(path) != 0) {
		last_errno = errno;
		return -1;
	}
	return 0;
}

/**********************************************************************
 *
 *	pd_make_writable
 */
int pd_make_writable(const char *path)
{
	struct stat	st;

	if (lstat(path, &st) != 0 || S_ISLNK(st.st_mode)
	    || chmod(path, st.st_mode | S_IWUSR) != 0) {
		last_errno = errno;
		return -1;
	}
	return 0;
}

/**********************************************************************
 *
 *	pd_diskfree
 *
 * Returns free bytes in the disk containing directory path, or -1.
 */
int64_t pd_diskfree(const char *path)
{
	struct statvfs	sv;

	if (statvfs(*path ? path : ".", &sv) != 0)
		return -1;
	return (int64_t)sv.f_bavail * (int64_t)sv.f_frsize;
}

/**********************************************************************
 *
 *	pd_strerror
 *
 * Returns error message of the last failed pd_ call.
 */
const char *pd_strerror(void)
{
	return strerror(last_errno);
}

/**********************************************************************
 *
 *	pd_getkey
 *
 * Reads one key from the terminal without waiting for Enter.
 * Returns -1 if there is no terminal.
 */
int pd_getkey(void)
{
	int		fd, c = -1;
	unsigned char	ch;
	struct termios	old, raw;

	if ((fd = open("/dev/tty", O_RDONLY)) < 0)
		return -1;
	if (tcgetattr(fd, &old) == 0) {
		raw = old;
		/* ISIG off: ^C is returned as 3 so the terminal is restored */
		raw.c_lflag &= ~(ICANON | ECHO | ISIG);
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		tcsetattr(fd, TCSANOW, &raw);
		if (read(fd, &ch, 1) == 1)
			c = ch;
		tcsetattr(fd, TCSANOW, &old);
	}
	close(fd);
	return c;
}

/**********************************************************************
 *
 *	pd_termwidth
 */
int pd_termwidth(void)
{
	struct winsize	ws;
	char		*s;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		return ws.ws_col;
	if ((s = getenv("COLUMNS")) != NULL && atoi(s) > 0)
		return atoi(s);
	return 80;
}

#endif /* _WIN32 */
