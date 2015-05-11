/**********************************************************************\
 *
 *	UTIME.C
 *
 * Utime function for Turbo C.
 *
\**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <time.h>

#include "utime.h"

int utime(char* path, struct utimbuf* stamp)
{
	struct ftime	ft;
	struct utimbuf	localstamp;
	struct tm*	stamp_tm;
	int		handle;
	int		retcode;

	if (stamp == NULL) {
		localstamp.actime = localstamp.modtime = time(NULL);
		stamp = &localstamp;
	}
	stamp_tm = localtime(&stamp->modtime);
	
	ft.ft_tsec = stamp_tm->tm_sec / 2;
	ft.ft_min = stamp_tm->tm_min;
	ft.ft_hour = stamp_tm->tm_hour;
	ft.ft_day = stamp_tm->tm_mday;
	ft.ft_month = stamp_tm->tm_mon + 1;
	ft.ft_year = stamp_tm->tm_year - 80;
	
	handle = open(path, O_RDWR);
	
	if (handle == -1)
		return -1;

	retcode = setftime(handle, &ft);
	
	close(handle);
	
	return retcode;
}
