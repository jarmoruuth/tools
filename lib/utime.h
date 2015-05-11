/**********************************************************************\
 *
 *	UTIME.H
 *
 * Utime for Turbo C
 *
\**********************************************************************/

struct utimbuf {
	time_t actime;
	time_t modtime;
};

int utime(char* path, struct utimbuf* stamp);
