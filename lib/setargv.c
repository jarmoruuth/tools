/************************************************************************\
**									**
**			 SETARGV.C	v1.1				**
**			------------					**
**									**
**	Alternate, wild cards (* and ?) expanding _setargv() command	**
**	line parser for Turbo C v1.0. Designed to work with all		**
**	memory models without changes in the source code. To use this	**
**	routine instead of the standard _setargv() just link the	**
**	compiled object module of this file with your program. No call	**
**	for this function is needed, because the standard startup	**
**	routine makes the call before starting the main() function.	**
**	Compile this with the same memory model as your program.	**
**									**
**	Any non-quoted command line argument containing wildcards	**
**	is searched for a matching filename until no more matches are	**
**	found. Thus the original wildcard argument is replaced by a	**
**	list of matching filenames. If no match is found the argument	**
**	is taken literally. Arguments that are not filenames containing	**
**	wildcards are handled just like the standard command line	**
**	parser of Turbo C does.	This version also sorts wildcard	**
**	matches.							**
**									**
**	Quoting arguments enables you to qive arguments with embedded	**
**	blanks and reserved MSDOS commandline characters: <, > and |.	**
**	The quote character is the double quote: " . An argument can	**
**	still contain double quote characters when they are prefixed	**
**	with the escape character \ (backslash). In all other instances	**
**	the escape character is taken as an ordinary non-blank with	**
**	no special effects.						**
**									**
**	Note :								**
**	This routine uses almost all available stack space as temporary	**
**	storage. Do not initialize global variable _stklen smaller	**
**	than 1000 (default stack is 4 kB).				**
**									**
**	author  :	Petri Soini					**
**	date    :	July 26, 1987					**
**	modified:	Jarmo Ruuth, added sort				**
**			Jarmo Ruuth, added extended wild card matching	**
**			  #ifdef EXTWILD, 27-Mar-1988			**
**									**
**	Copyright (C) 1987, 88 by Petri Soini				**
**	Copying for non-commercial purposes is FREE.			**
**									**
**	Disclaimer:							**
**		The author is NOT responible for any damage caused	**
				        ^
**		by correct or incorrect use of this software.		**
**									**
\************************************************************************/
#pragma page

#include	<stdio.h>
#include	<stdlib.h>
#include	<dir.h>
#include	<string.h>
#include	<alloc.h>
#include	<process.h>
#include	<dos.h>

#define	TRUE	1
#define	FALSE	0
#define	LDATA	(defined(__COMPACT__)||defined(__LARGE__)||defined(__HUGE__))
#define	QUOTECH	'"'
#define	ESCCH	'\\'
#define	FATTR	0x00	/* file searching attribute	*/
/* for other search attributes see MSDOS programmer's	*\
** reference manual for information of file attributes	**
\*							*/
#define	ISWHITE(c)	((c) == ' ' || (c) == '\t')

#if	__TURBOC__ >= 150	/* TURBOC v1.5 or higher */

#define	__argc	_argc
#define	__argv  _argv

#else

extern unsigned char	_osmajor;	/* DOS version major number	*/
extern unsigned short	_psp,		/* PSP segment address		*/
			_stklen,	/* stack size in bytes		*/
			__argc;		/* 1st argument for main()	*/
extern char		**__argv;	/* 2nd arg for main()		*/
/* __argc and __argv will be pushed to stack for arguments of main() by	*\
\* the startup routine in c0x.obj (x = t,s,m,c,l or h).			*/

#endif

extern unsigned short	_envseg,	/* environment segment		*/
			_envLng;	/* environment length		*/

static int	sortindex1, sortindex2;	/* index variables for sorting	*/
static char	startsort = FALSE;	/* "boolean"			*/
static struct ffblk
		*ffblkp;
#ifdef EXTWILD
static char	*argpath;		/* path in wildcarded match	*/
#endif

/*=======================================================================
**			ALLOCA()
**			--------
**	This macro works as the alloca() library function of many
**	C-compilers. However, this should be used only when the
**	caller has several local variables so that stack frame will be
**	returned with a "MOV SP,BP" instruction. this could be made sure
**	also by assigning the _SP to a variable before call
**	to ALLOCA() and assigning the value back to _SP as the last
**	expression of the function.
*/
#if LDATA
#	define	ALLOCA(nbytes)	MK_FP(_SS,(_SP -= (nbytes)))
#else
#	define	ALLOCA(nbytes)	(void *)(_SP -= (nbytes))
#endif

#pragma page
/*=======================================================================
**			cmp()
**			-----
**	Comparison function for qsort()
*/
static int	cmp(sp1, sp2)
char	**sp1, **sp2;
{
	return (strcmp(*sp1, *sp2));
}


/*=======================================================================
**			allocate()
**			----------
**	Like malloc(), but aborts if not successful
*/
static void * near pascal	allocate(unsigned nbytes)
{
	register void	*p;
	
	if ((p = malloc(nbytes)) == NULL)
		exit(1);
	return (p);
}


/*=======================================================================
**			string_save()
**			-------------
**	Like strdup(), but aborts if not successful
*/
static char * near pascal	string_save(char *s)
{
	return (strcpy(allocate(strlen(s) + 1), s));
}

#pragma page

#ifdef	EXTWILD

/*======================================================================
**
**			split()
**			-------
**	Splits given path to directory and file name. Doesn't change
**	path, but returns pointer to start of file name portion.
**	'\\' is used both in path separator and quote char
**	in regular expression, but it isn't a real problem, because
**	those characters that user might want to quote are not allowed
**	in file names under MS-DOS. (This will altough fail, if user
**	wants to quote non-special char.)
**	Split is defined external, because it might be useful to someone
**	else.
*/
char *split (register char *path)
{
	register char	*filebeg = path;
	
	while (*path) {
		switch (*path++) {
			case '\\':
			case ':':
			case '/':
				filebeg = path;
				break;
			case '[':
			case '*':
			case '?':
				return filebeg;
			default:
				break;
		}
	}
	return filebeg;
}	

/*=======================================================================
**			wild()
**			------
**	Extended UNIX-like file name matching. Matching routines are
**	stolen from GNU make.
**	Checks whether argument contains wildcards. Returns either the
**	matched filename or NULL if the argument has no wild cards or
**	if no match is found.
*/
static char * near pascal	wild(register char *arg)
{
	static char		*oldarg = NULL,
				*pattern;
	char			path[MAXPATH];

	if (oldarg != arg) {	/* first time for this argument */
		if (!glob_pattern_p(pattern = split(arg)))
			return NULL;
		argpath[strlen(strcpy(argpath, arg))-strlen(pattern)] = '\0';
		strcat(strcpy(path,argpath), "*.*");
		if (findfirst(path,ffblkp,FATTR))
			return NULL;
		oldarg = arg;
	} else
		if (findnext(ffblkp))
			return NULL;
	while (!glob_match(pattern, ffblkp->ff_name))
		if (findnext(ffblkp))
			return NULL;
	return string_save(strcat(strcpy(path,argpath), ffblkp->ff_name));
}

#else	/* !EXTWILD */

/*=======================================================================
**			wild()
**			------
**	Checks whether argument contains wildcards. Returns either the
**	matched filename or NULL if the argument has no wild cards or
**	if no match is found.
*/
static char * near pascal	wild(char *arg)
{
	static char		*oldarg = NULL;
	char			drive[MAXPATH];
	register char 		*dir = drive + MAXDRIVE;

	if (!(fnsplit(arg, drive, dir, NULL, NULL) & WILDCARDS)
	|| ((oldarg != arg)? findfirst(arg,ffblkp,FATTR) : findnext(ffblkp)))
		return (NULL);
	oldarg = arg;
	return (string_save(strcat(strcat(drive, dir), ffblkp->ff_name)));
}

#endif	/* EXTWILD */

#pragma page
/*=======================================================================
**			nextarg()
**			---------
**	Searches next argument and returns safely allocated copy of it
**	or NULL, if no argument was found.
*/
static char * near pascal	nextarg(char **arglist)
{
	static char	*oldargp,		/* old argument ptr	*/
			wfound = FALSE;		/* wildcard match found	*/
	register char	*scanp, *cp,
			quoted = FALSE;

	if (wfound && (wfound = !(startsort = ((cp = wild(oldargp)) == NULL))))
		return (cp);
	sortindex2 = sortindex1;
	while (ISWHITE(**arglist))		/* skip white space */
		++(*arglist);
	for (scanp = *arglist; *scanp && !ISWHITE(*scanp); ) {
		if (*scanp == QUOTECH) {		/* quote parser */
			if (scanp[-1] == ESCCH) {
				(void)strcpy(scanp - 1, scanp);
				continue;
			}
			for (cp = scanp; (cp = strchr(cp+1, QUOTECH)) != NULL;)
				if (cp[-1] != ESCCH)
					break;
			if (cp != NULL) {
				quoted = TRUE;
				oldargp = cp+1;
				for (*cp = '\0', cp = scanp + 1; *cp; )
					if (*cp == ESCCH && cp[1] == QUOTECH)
						(void)strcpy(cp, cp + 1);
					else
						++cp;
				(void)strcpy(cp, oldargp);
				(void)strcpy(scanp, scanp + 1);
				scanp = cp - 2;
			}
		}
		++scanp;
	}
	if (scanp == (oldargp = *arglist) && !quoted)	/* end of cmdline? */
		return (NULL);
	*arglist = (*scanp ? (scanp + 1) : scanp);
	*scanp = '\0';
	return ((!quoted && (wfound = ((cp = wild(oldargp)) != NULL))) ?
		(sortindex1 = __argc, cp) : string_save(oldargp));
}

#pragma page
/************************************************************************
**			_setargv()
**			----------
**	Wildcards expanding commandline parser for Turbo C
*/
void	_setargv(void)
{
	char far		*pspp;
	char			*cline,	**argv_aid;
	register unsigned	i, maxarg;
	struct ffblk		ffblk;
#ifdef EXTWILD
	char			path[MAXPATH];

	argpath = path;
#endif
	ffblkp = &ffblk;
	pspp = MK_FP(_psp,0x81);	/* arguments are at address _psp:0x81 */
	maxarg = _stklen - 0x200;	/* use almost all stack space */
	cline = ALLOCA(maxarg);
	argv_aid = (char **)(cline + ((i = (unsigned char)pspp[-1]) + 2));
	maxarg = (maxarg - (i + 2))/sizeof(char **) - 1;
	cline[i] = '\0';		/* string terminator */
#if LDATA
	(void)memcpy(cline, pspp, i);
#else
	while (i--)	/* copy commandline string to stack */
		cline[i] = pspp[i];
#endif
	if (_osmajor < 3)		/* program name not available on */
		argv_aid[0] = "";	/* DOS versions prior to 3.0     */
	else {
		pspp = MK_FP(_envseg, _envLng + 2); /* get program name */
#if LDATA
		argv_aid[0] = string_save(pspp);
#else
		for (i = 0; pspp[i++]; )	/* measure length */
			;
		argv_aid[0] = allocate(i);	/* reserve space */
		while (i--)			/* and copy it */
			argv_aid[0][i] = pspp[i];
#endif
	}
	for (__argc = 1; __argc < maxarg; ++__argc) {
		argv_aid[__argc] = nextarg(&cline);	/* get argument */
		if (startsort) {
			qsort(argv_aid + sortindex2,	/* array to be sorted */
			      __argc - sortindex2,	/* element count */
			      sizeof(char *),		/* element size */
			      cmp);			/* cmp function */
			startsort = FALSE;
		}
		if (argv_aid[__argc] == NULL)
			break;
	}
	argv_aid[__argc] = NULL;		/* terminator for __argv[] */
	__argv = allocate(i = ((__argc + 1)*sizeof(char *)));
	(void)memcpy(__argv, argv_aid, i);	/* __argv[] to safe place */
}

#ifdef TEST

void main (int argc, char *argv[])
{
	register int	i;

	for (i=1; i < argc; i++)
		puts(argv[i]);
}

#endif /* TEST */
