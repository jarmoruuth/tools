/************************************************************************\
**			RJ.C - rm for MS-DOS
**			--------------------
**	Almost similar to UNIX command rm.
**	The most remarkable difference is that you do not have to hit <CR>
**	after typing 'y' or 'n' in interactive mode.
**	Also, one extension is added -- the possibility to remove
**	directories when using option -d
**	
**	syntax:
**		rj [-dfirs] filename ...
**
**	options:
**		-d	- also delete directories
**		-f	- force delete for read-only and invisible files
**		-i	- interactive
**		-r	- recurse subdirectories
**		-s	- silent mode
**
**	wildcards are allowed in filenames
**	
**	WARNING :
**		command "rj -dfr \*.*"
**		will erase ALL disc contents.
**
**	Compiler :	Turbo C
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
\************************************************************************/

#include	<stdio.h>
#include	<dos.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<string.h>
#include	<conio.h>
#include	<io.h>
#include	<sys\types.h>
#include	<sys\stat.h>
#ifdef TC
#include <dir.h>
#endif
#ifdef MSC
#include <direct.h>
#endif

#include	<c.h>
#include	<getopt.h>
#include	<listmenu.h>
#include	<diruti.h>

#define NALLOC	10

static struct {
	char	interactive,
		directory,
		force,
		recursive,
		silent,
		menu;
} mode = {
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE
};

static int	rval;

static char	search_attribute = 0;

static char	**dir_list  = NULL,
		**file_list = NULL;

static unsigned dir_list_len  = 0,
		file_list_len = 0;

#if __TURBOC__

unsigned cdecl	_stklen = 16 * 1024;	/* reserve big stack for recursion */

void cdecl	_setenvp()
{	/* dummy function to reduce program size */
}

#endif

/************************************************************************
**		usage()
**		-------
**	exit with help message
*/
static void 	usage(void)
{
	printf(
	"Usage:\trj [-dfimrs] filename ...\n"
	"Options:\n\t"
		"-d  delete directories\n\t"
		"-f  force delete for read-only and hidden files\n\t"
		"-i  interactive\n\t"
#ifndef NT
		"-m  use menu to select files\n\t"
#endif
		"-r  recurse subdirectories\n\t"
		"-s  silent mode\n"
	"Specify a directory by adding a slash after the name\n"
	"Command\t\"rj -rdf *.*\" in root will erase the whole disk !\n");
	exit(1);
}


/************************************************************************
**		memerror()
**		----------
*/
static void 	memerror(void)
{
	printf("rm: ERROR: Out of memory!\n");
	exit(3);
}

/************************************************************************
**		file_list_cmp()
**		-------------
*/
static int	file_list_cmp (char** name1, char** name2)
{
	return strcmp(*name1, *name2);
}


/************************************************************************
**		sortif()
**		------
**	sorts the file list from the last sorted part to the end of the
**	current list if menu selection is used
*/
static void 	sortif (void)
{
	static int sortindex;
	
	if (mode.menu) {
		qsort(file_list + sortindex, file_list_len - sortindex,
			sizeof (char*), file_list_cmp);
		sortindex = file_list_len;
	}
}


/************************************************************************
**		mark()
**		------
**	mark file or directory to be deleted by linking its name
**	to list.
*/
static void 	mark (char *name, char **list[], unsigned *len)
{
	if (*list == NULL)
		*list = malloc (sizeof (char*) * NALLOC);
	else if (*len % NALLOC == 0)
		*list = realloc (*list, sizeof (char*) * (*len + NALLOC));
	if (*list == NULL)
		memerror();
	if (name != NULL && (name = strdup(name)) == NULL)
		memerror();
	(*list)[(*len)++] = name;
}


/************************************************************************
**		MARK_FILE()
**		-----------
**	This routine marks file to be deleted by linking it to list
**	of filenames.
*/
#define	MARK_FILE(fname)	mark((fname), &file_list, &file_list_len)


/************************************************************************
**		MARK_DIR()
**		----------
**	This routine does the same as mark_file(), but argument is
**	a directory name
*/
#define	MARK_DIR(dname)		mark((dname), &dir_list, &dir_list_len)


#ifndef NT
/**********************************************************************
**	menu_select()
**	-------------
*/
static void 	menu_select (char** files, char *txt)
{
	listmenu_t* l;

	if (*files == NULL)
		return;
	l = default_listmenu_t();
	l->header = txt;
	if (listmenu(files, l) == NULL)
		*files = NULL;
	fprintf(stderr, "\n");
}
#endif

/************************************************************************
**		expunge()
**		---------
**	does the real deletion for files and possibly directories.
*/
static void 	expunge (void)
{
	register char	**list;
	register char	*name;
	register int	retries;
	
	MARK_FILE(NULL);
	MARK_DIR(NULL);
#ifndef NT
	if (mode.menu) {
		menu_select(file_list, "Select files to remove:");
		menu_select(dir_list, "Select directories to remove:");
	}
#endif
	for (list = file_list; *list != NULL; list++) {
		retries = 0;
		name = *list;
		if (!mode.interactive && !mode.silent) {
			printf("rm %s", name);
		}
retry_deletion:
		if (remove(name)) {
			if (mode.force && !retries++)
				if (!chmod (name, S_IREAD|S_IWRITE))
					goto retry_deletion;
			rval = 2;
			if (mode.interactive || mode.silent)
				printf(name);
			printf(" : Permission denied");
			if (mode.interactive || mode.silent)
				printf("\n");
		}
		if (!mode.interactive && !mode.silent)
			printf("\n");
	}
	for (list = dir_list; *list != NULL; list++) {
		name = *list;
		if (!mode.interactive && !mode.silent)
			printf("rmdir %s", name);
		if (rmdir (name)) {
			if (mode.interactive || mode.silent)
				printf(name);
			printf(" : Directory not empty");
			if (mode.interactive || mode.silent)
				printf("\n");
			rval = 2;
		}
		if (!mode.interactive && !mode.silent)
			printf("\n");
	}
}


/************************************************************************
**		delete()
**		--------
**	delete file (after asking the user for permission in 
**	interactive mode).
*/
static void 	delete(char *fname, char attribute)
{
	register int	permission;
	register int	ans;
	
	if (mode.interactive) {
		fprintf(stderr, "remove ");
		if (attribute & DI_DIREC)
			fprintf(stderr, "directory ");
		fprintf(stderr, "%s? ", fname);
		do {
			ans = getch();
			if (ans == 3) {
				fprintf(stderr, "^C\n");
				exit(1);
			}
		} while (!(ans=='y' || ans=='Y' || ans=='n' || ans=='N'));
		permission = (ans == 'y') || (ans == 'Y');
		if (permission)
			fprintf(stderr, "Yes\n");
		else
			fprintf(stderr, "No\n");
	}
	if (!mode.interactive || permission)
		if (attribute & DI_DIREC)
			MARK_DIR(fname);
		else
			MARK_FILE(fname);
}


/************************************************************************
**		rm ()
**		-----
**	delete files matching fname, recursively if required.
*/
static void 	rm (char *fname)
{
	char		path[U_MAXPATH],
			file[U_MAXPATH/2],
			full_path[U_MAXPATH];
	register int	done;
	char		*fileptr;
	u_dirinfo_t	dirinfo;
	
	strcpy(file, fileptr = u_splitpath(fname));
	*fileptr = '\0';
	strcpy(path, fname);
	if (mode.recursive) {
		strcat(path, "*.*");
		done = u_findfirst(path, &dirinfo, DI_DIREC);
		for (; !done; done = u_findnext(&dirinfo)) {
			if ((dirinfo.di_attrib & DI_DIREC)
			  &&  strcmp(dirinfo.di_name, ".")
			  &&  strcmp(dirinfo.di_name, ".."))
				rm(
				  strcat(
				    strcat(
				      strcat(
				        strcpy(full_path, fname),
				        dirinfo.di_name),
				      "\\"),
				    file));
		}
		strcpy(path, fname);
	}
	strcat(path, file);
	if ((done = u_findfirst(path, &dirinfo, search_attribute)) != 0
	    && !mode.recursive)
		printf("%s: File not found\n", file);

	for (; !done; done = u_findnext(&dirinfo)) {
		strcat(strcpy(full_path, fname), dirinfo.di_name);
		if (((dirinfo.di_attrib & DI_DIREC)
		     && mode.directory
		     && strcmp(dirinfo.di_name, ".")
		     && strcmp(dirinfo.di_name, ".."))
		   || !(dirinfo.di_attrib & DI_DIREC))
			delete(full_path, dirinfo.di_attrib);
	}
	sortif();
}

/**********************************************************************
**	isdir
**
** File is expected to mean a directory if it ends with \ or /.
*/
static int 	isdir(register char *fname)
{
	register int pos;
	
	pos = strlen(fname) - 1;
	return fname[pos] == '\\' || fname[pos] == '/';
}

/**********************************************************************
**	rm_dir
**
** Remove all files in the directory and the directory itself if
** mode.directory is TRUE.
*/
static void 	rm_dir(char *dirname)
{
	register int	pos;
	char		path[U_MAXPATH];
	
	/* add *.* file mask to the directory */
	strcpy(path, dirname);
	strcat(path, "*.*");
	rm (path);
	if (mode.directory) {
		pos = strlen(dirname) - 1;
		/* if not root directory, delete it */
		if (pos && dirname[pos-1] != ':') {
			dirname[pos] = '\0';
			delete (dirname, DI_DIREC);
		}
	}
}

/************************************************************************
**		main()
**		------
*/
int cdecl	main(int argc, char **argv)
{
	register int	c;

	while ((c = getopt(argc, argv, "dfimrs")) != EOF) {
		switch (c) {
			case 'd' :
				mode.directory = TRUE;
				search_attribute |= DI_DIREC;
				break;
			case 'f' :
				mode.force = TRUE;
				search_attribute |=
				(DI_RDONLY | DI_HIDDEN | DI_SYSTEM);
				break;
			case 'i' :
				mode.interactive = TRUE;
				break;
#ifndef NT
			case 'm' :
				mode.menu = TRUE;
				break;
#endif
			case 'r' :
				mode.recursive = TRUE;
				search_attribute |= DI_DIREC;
				break;
			case 's':
				mode.silent = TRUE;
				break;
			default  :
				printf("Illegal option argument\n");
				usage();
		}
	}
	if  (argc <= optind)
		usage();
	for (; optind < argc; optind++) {
		if (isdir(argv[optind]))
			rm_dir (argv[optind]);
		else
			rm (argv[optind]);
	}
	expunge();
	return (rval);
}
