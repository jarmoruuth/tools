/**********************************************************************\
 *
 *	DIRUTI.C
 *
 * Directory utilities for Turbo C, MSC and OS/2.
 *
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#ifdef DOS
#include <dos.h>
#endif

#ifdef OS2
#include <string.h>
#endif

#ifdef NT
#include <io.h>
#include <string.h>
#endif

#include "lassert.h"
#include "diruti.h"

#if defined(TC) && defined(DOS)

#include <dir.h>

int u_findfirst(char* path, u_dirinfo_t* dirinfo, int attrib)
{
	return findfirst(path, (struct ffblk *)dirinfo, attrib);
}

int u_findnext(u_dirinfo_t* dirinfo)
{
	return findnext((struct ffblk *)dirinfo);
}

#endif /* TC && DOS */

#if defined(MSC) && defined(DOS)

int u_findfirst(char* path, u_dirinfo_t* dirinfo, int attrib)
{
	lassert(sizeof(u_dirinfo_t) == sizeof(struct find_t));
	
	if (_dos_findfirst(path, attrib, (struct find_t *)dirinfo) == 0)
		return 0;
	else
		return -1;
}

int u_findnext(u_dirinfo_t* dirinfo)
{
	if (_dos_findnext((struct find_t *)dirinfo) == 0)
		return 0;
	else
		return -1;
}

#endif /* MSC && DOS */

#if defined(MSC) && defined(OS2)

static void copy_ffbuf(u_dirinfo_t* dirinfo, FILEFINDBUF* ffbuf)
{
	dirinfo->di_credate = ffbuf->fdateCreation;
	dirinfo->di_cretime = ffbuf->ftimeCreation;
	dirinfo->di_accdate = ffbuf->fdateLastAccess;
	dirinfo->di_acctime = ffbuf->ftimeLastAccess;
	dirinfo->di_moddate = ffbuf->fdateLastWrite;
	dirinfo->di_modtime = ffbuf->ftimeLastWrite;
	dirinfo->di_size = ffbuf->cbFile;
	dirinfo->di_filealloc = ffbuf->cbFileAlloc;
	dirinfo->di_attrib = ffbuf->attrFile;
	dirinfo->di_namelen = ffbuf->cchName;
	strcpy(dirinfo->di_name, ffbuf->achName);
}

int u_findfirst(char* path, u_dirinfo_t* dirinfo, int attrib)
{
	int	        retcode;
	USHORT	    count = 1;
        FILEFINDBUF ffbuf;
	
	dirinfo->di_handle = HDIR_CREATE;
	
	retcode = DosFindFirst(
			(PSZ)path,
			(PHDIR)&dirinfo->di_handle,
			(USHORT)attrib,
			&ffbuf,
			(USHORT)sizeof(ffbuf),
			&count,
			(ULONG)0);

	if (retcode == 0 && count == 1) {
            copy_ffbuf(dirinfo, &ffbuf);
		return 0;
	} else {
		if (retcode == 0) {
			DosFindClose((HDIR)dirinfo->di_handle);
            }
		return -1;
	}
}

int u_findnext(u_dirinfo_t* dirinfo)
{
	int	        retcode;
	USHORT	    count = 1;
        FILEFINDBUF ffbuf;
	
	retcode = DosFindNext(
			(HDIR)dirinfo->di_handle,
			&ffbuf,
			(USHORT)sizeof(ffbuf),
			&count);

	if (retcode == 0 && count == 1) {
            copy_ffbuf(dirinfo, &ffbuf);
		return 0;
	} else {
		DosFindClose((HDIR)dirinfo->di_handle);
		return -1;
	}
}

#endif /* MSC && OS2 */

#ifdef NT

#include <time.h>

static void copy_fileinfo(u_dirinfo_t* di, struct _finddata_t* fileinfo)
{
        struct tm* ltm;

        ltm = localtime(&fileinfo->time_write);

	di->di_attrib = fileinfo->attrib;
	di->di_modtime.twosecs = ltm->tm_sec;
	di->di_modtime.minutes = ltm->tm_min;
	di->di_modtime.hours = ltm->tm_hour;
	di->di_moddate.day = ltm->tm_mday;
	di->di_moddate.month = ltm->tm_mon + 1;
	di->di_moddate.year = ltm->tm_year - 80;
	di->di_size = fileinfo->size;
	strcpy(di->di_name, fileinfo->name);
}

static int match_attributes(u_dirinfo_t* dirinfo)
{
        int attrib;
        int searchattrib;

        attrib = dirinfo->di_attrib;
        searchattrib = dirinfo->di_searchattrib;

        if ((attrib & DI_HIDDEN) && !(searchattrib & DI_HIDDEN)) {
            return(0);
        }
        if ((attrib & DI_SYSTEM) && !(searchattrib & DI_SYSTEM)) {
            return(0);
        }
        if ((attrib & DI_DIREC) && !(searchattrib & DI_DIREC)) {
            return(0);
        }
        return(1);
}

static int du_findfirst(char* path, u_dirinfo_t* dirinfo, int attrib)
{
        struct _finddata_t fileinfo;

        dirinfo->di_searchattrib = attrib;

        dirinfo->di_handle = _findfirst(path, &fileinfo);
        if (dirinfo->di_handle == -1) {
            _findclose(dirinfo->di_handle);
            return(-1);
        }
        copy_fileinfo(dirinfo, &fileinfo);
	
	return 0;
}

static int du_findnext(u_dirinfo_t* dirinfo)
{
	long retcode;
        struct _finddata_t fileinfo;

        retcode = _findnext(dirinfo->di_handle, &fileinfo);
        if (retcode == -1) {
            _findclose(dirinfo->di_handle);
            return(-1);
        }

        copy_fileinfo(dirinfo, &fileinfo);
	
	return 0;
}

int u_findfirst(char* path, u_dirinfo_t* dirinfo, int attrib)
{
        int retcode;

        retcode = du_findfirst(path, dirinfo, attrib);
        if (retcode == -1) {
            return(-1);
        }

        if (match_attributes(dirinfo)) {
            return(0);
        } else {
            return(u_findnext(dirinfo));
        }
}

int u_findnext(u_dirinfo_t* dirinfo)
{
	int retcode;

        do  {
            retcode = du_findnext(dirinfo);
            if (retcode == -1) {
                return(-1);
            }
        } while (!match_attributes(dirinfo));
        return(0);
}
#endif /* NT */
