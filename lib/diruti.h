/**********************************************************************\
 *
 *	DIRUTI.H
 *
 * Directory utilities.
 *
\**********************************************************************/

#define U_MAXPATH	1024
#define U_MAXDRIVE	3
#define U_MAXDIR	1024
#define U_MAXFILE	1024
#define U_MAXEXT	5

#if defined(OS2) || defined(DOS)
/* Attribute bits used in MS-DOS and OS/2 */
#define DI_NORMAL	0x00		/* Normal file */
#define DI_RDONLY	0x01		/* Read only attribute */
#define DI_HIDDEN	0x02		/* Hidden file */
#define DI_SYSTEM	0x04		/* System file */
#define DI_LABEL	0x08		/* Volume label */
#define DI_DIREC	0x10		/* Directory */
#define DI_ARCH		0x20		/* Archive */
#endif

typedef enum {
        ORD_NAME_ASC,
        ORD_NAME_DESC,
        ORD_TIME_ASC,
        ORD_TIME_DESC
} u_dirord_t;

#if defined(DOS) || defined(NT)

typedef struct	{
	unsigned	twosecs	: 5;	/* Two second interval */
	unsigned	minutes	: 6;	/* Minutes */
	unsigned	hours	: 5;	/* Hours */
} FTIME;

typedef struct	{
	unsigned	day	: 5;	    /* Days */
	unsigned	month 	: 4;	/* Months */
	unsigned	year	: 7;	/* Year - 1980 */
} FDATE;

#endif

#ifdef DOS

typedef struct {
	char		di_reserved[21];
	char		di_attrib;
	unsigned	di_modtime;
	unsigned	di_moddate;
	long		di_size;
	char		di_name[13];
} u_dirinfo_t;

#endif

#ifdef OS2

#define INCL_DOSFILEMGR
#define INCL_NOPM
#include <os2.h>

typedef struct {
	FDATE	di_credate;	/* OS/2 only */
	FTIME	di_cretime;	/* OS/2 only */
	FDATE	di_accdate;	/* OS/2 only */
	FTIME	di_acctime;	/* OS/2 only */
	FDATE	di_moddate;
	FTIME	di_modtime;
	ULONG	di_size;
	ULONG	di_filealloc;	/* OS/2 only */
	USHORT	di_attrib;
	UCHAR	di_namelen;	/* OS/2 only */
	CHAR	di_name[CCHMAXPATHCOMP];
	HDIR	di_handle;	/* OS/2 only */
} u_dirinfo_t;

#endif

#ifdef NT

#include <io.h>

#define DI_NORMAL	_A_NORMAL		/* Normal file */
#define DI_RDONLY	_A_RDONLY		/* Read only attribute */
#define DI_HIDDEN	_A_HIDDEN		/* Hidden file */
#define DI_SYSTEM	_A_SYSTEM		/* System file */
/* #define DI_LABEL	0 */            /* Volume label */
#define DI_DIREC	_A_SUBDIR		/* Directory */
#define DI_ARCH		_A_ARCH		    /* Archive */

typedef struct {
	unsigned	di_attrib;
	FTIME       di_modtime;
	FDATE       di_moddate;
	long		di_size;
	char		di_name[260];
	unsigned	di_searchattrib;
        long        di_handle;
} u_dirinfo_t;

#endif

int u_findfirst(char* path, u_dirinfo_t* dirinfo, int attrib); /* diruti.c */
int u_findnext(u_dirinfo_t* dirinfo);	/* diruti.c */

int u_findfirst_ord(char* path, u_dirinfo_t* dirinfo, int attrib,
                    u_dirord_t ord); /* dirutior.c */
int u_findnext_ord(u_dirinfo_t* dirinfo);	/* dirutior.c */

char* u_splitpath(char* path);		/* splitpat.c */
long u_getdiskfree(int disk);		/* diskfree.c */
