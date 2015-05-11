/**********************************************************************\
 *
 *	DISKFREE.C
 *
\**********************************************************************/

#ifdef DOS
#include <dos.h>
#endif

#include "diruti.h"

#if defined(TC)

long u_getdiskfree(int disk)
{
	struct dfree	df;
	
	getdfree(disk, &df);

	if (df.df_sclus == (unsigned)-1)
		return -1L;

	return (long)df.df_avail *
	       (long)df.df_sclus *
	       (long)df.df_bsec;
}

#endif

#if defined(DOS) && defined(MSC)

long u_getdiskfree(int disk)
{
	struct diskfree_t df;
	
	if (_dos_getdiskfree(disk, &df) != 0)
		return -1L;
	
	return (long)df.avail_clusters *
	       (long)df.sectors_per_cluster *
	       (long)df.bytes_per_sector;
}

#endif

#if defined(OS2) && defined(MSC)

long u_getdiskfree(int disk)
{
	register int	retcode;
	FSALLOCATE	df;
	
	retcode = DosQFSInfo(
			(USHORT)disk,
			(USHORT)1,
			(PBYTE)&df,
			(USHORT)sizeof(FSALLOCATE));
	
	if (retcode != 0)
		return -1L;
	
	return (long)df.cSectorUnit *
	       (long)df.cUnitAvail *
	       (long)df.cbSector;
}

#endif

#if defined(NT)

#include <windows.h>
#include <stdio.h>
#include <direct.h>

long u_getdiskfree(int disk)
{
        BOOL succp;
        char rootpath[10];
        DWORD sect_per_clust;
        DWORD bytes_per_sect;
        DWORD free_clust;
        DWORD clust;

        if (disk == 0) {
            disk = _getdrive();
        }
        sprintf(rootpath, "%c:\\", 'A' + disk - 1);

        succp = GetDiskFreeSpace(
                    rootpath,
                    &sect_per_clust,
                    &bytes_per_sect,
                    &free_clust,
                    &clust);
        if (succp) {
            return(free_clust * sect_per_clust * bytes_per_sect);
        } else {
            return(-1L);
        }
}

#endif
