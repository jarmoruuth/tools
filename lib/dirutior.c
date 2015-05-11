/**********************************************************************\
 *
 *	DIRUTIOR.C
 *
 * Directory utilities for Turbo C, MSC and OS/2. Analogous to
 * u_findfirst and u_findnext except that the output order can
 * be specified.
 *
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "lassert.h"
#include "diruti.h"

#define NINFO 10

static u_dirinfo_t* ord_dirinfo = NULL;
static int          ord_size;
static int          ord_pos;
static u_dirord_t   ord_order;

static int time_cmp(u_dirinfo_t* di1, u_dirinfo_t* di2)
{
	int		retval;
	FTIME*  ft1 = (FTIME *)&di1->di_modtime;
	FDATE*  fd1 = (FDATE *)&di1->di_moddate;
	FTIME*  ft2 = (FTIME *)&di2->di_modtime;
	FDATE*  fd2 = (FDATE *)&di2->di_moddate;

	if (!(retval = fd1->year - fd2->year)
	    && !(retval = fd1->month - fd2->month)
	    && !(retval = fd1->day - fd2->day)
	    && !(retval = ft1->hours - ft2->hours)
	    && !(retval = ft1->minutes - ft2->minutes))
		retval = ft1->twosecs - ft2->twosecs;
	return(retval ? retval
                      : strcmp(di1->di_name, di2->di_name));
}

static int ord_cmp(void* p1, void* p2)
{
        u_dirinfo_t* di1 = p1;
        u_dirinfo_t* di2 = p2;

        switch (ord_order) {
            case ORD_NAME_ASC:
                return(strcmp(di1->di_name, di2->di_name));
            case ORD_NAME_DESC:
                return(-strcmp(di1->di_name, di2->di_name));
            case ORD_TIME_ASC:
                return(time_cmp(di1, di2));
            case ORD_TIME_DESC:
                return(-time_cmp(di1, di2));
            default:
                lerror;
                return(0);
        }
}

int u_findfirst_ord(char* path, u_dirinfo_t* dirinfo, int attrib,
                    u_dirord_t ord)
{
        if (ord_dirinfo != NULL) {
            free(ord_dirinfo);
            ord_dirinfo = NULL;
            ord_pos = 0;
            ord_size = 0;
        }

        if (u_findfirst(path, dirinfo, attrib) == 0) {
            ord_dirinfo = malloc(sizeof(u_dirinfo_t) * NINFO);
            lassert(ord_dirinfo != NULL);
            ord_size = NINFO;
            ord_pos = 1;
            ord_order = ord;
            ord_dirinfo[0] = *dirinfo;
            while (u_findnext(dirinfo) == 0) {
                if (ord_pos == ord_size) {
                    ord_size += NINFO;
                    ord_dirinfo = realloc(ord_dirinfo,
                                          sizeof(u_dirinfo_t) * ord_size);
                    lassert(ord_dirinfo != NULL);
                }
                ord_dirinfo[ord_pos++] = *dirinfo;
            }
            ord_size = ord_pos;
            ord_pos = 0;
            qsort(ord_dirinfo, ord_size, sizeof(u_dirinfo_t), ord_cmp);
            *dirinfo = ord_dirinfo[ord_pos++];
            return(0);
        } else {
            return(-1);
        }
}

int u_findnext_ord(u_dirinfo_t* dirinfo)
{
        if (ord_pos == ord_size) {
            if (ord_dirinfo != NULL) {
                free(ord_dirinfo);
                ord_dirinfo = NULL;
                ord_pos = 0;
                ord_size = 0;
            }
            return(-1);
        } else {
            *dirinfo = ord_dirinfo[ord_pos++];
            return(0);
        }
}
