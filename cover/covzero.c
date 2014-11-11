/**********************************************************************\
 *
 *	COVZERO.C
 *
 * Zeroes cover list files.
 *
 * Copyright (C) 1990-1991 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "cover.h"

char _Copyright_[] = "Copyright (C) 1990-1991 by Jarmo Ruuth";

/**********************************************************************
 *    cover_fopen_oldlst
 */
static FILE* cover_fopen_oldlst(
                char* lstfile,
                char* buf)
{
        char* s;
        int retcode;

        strcpy(buf, lstfile);
        s = strrchr(buf, '.');
        assert(s != NULL);
    
        strcpy(s+1, "tmp");
    
        unlink(buf);
    
        retcode = rename(lstfile, buf);
        if (retcode != 0)
            return(NULL);
    
        return(fopen(buf, "r"));
}

/**********************************************************************
 *    cover_zero
 */
static int cover_zero(char* lstfile)
{
        char type;
        int ind;
        int i;
        int lineno;
        long hitcount;
        static char buf[144];
        static char oldlst[144];
        FILE* oldfp;
        FILE* newfp;

        oldfp = cover_fopen_oldlst(lstfile, oldlst);
        if (!oldfp)
            return(0);
        
        newfp = fopen(lstfile, "w");
        if (!newfp) {
            fclose(oldfp);
            return(0);
        }

        while (fgets(buf, sizeof(buf), oldfp) != NULL) {

            switch ((cover_state_t)buf[0]) {
                case COVER_MODULE:
                case COVER_FILE_NAME:
                case COVER_FUNCTION_NAME:
                case COVER_COMMENT:
                case COVER_POP:
                case COVER_SWITCH:
                    fprintf(newfp, "%s", buf);
                    break;
                default:
                    i = 0;
                    while (buf[i] != '\n' && buf[i] != '\0' && buf[i] != '|') {
                        i++;
                    }
                    sscanf(buf, "%c %d:%d:%ld", &type, &ind, &lineno, &hitcount);
                    fprintf(newfp, "%c %d:%d:%ld\t\t%s", type, ind, lineno, 0L, &buf[i]);
                    break;
            }
        }

        fclose(oldfp);
        fclose(newfp);
        unlink(oldlst);
        return(1);
}

/**********************************************************************
 *	main
 */
void main(int argc, char* argv[])
{
	if (argc == 1) {
		printf("Usage: covzero <cover list file>...\n");
		exit(1);
	}
	
	for (argv++; *argv; argv++) {
            printf("%s", *argv);
		if (!cover_zero(*argv)) {
                printf(": failed!");
            }
            printf("\n");
        }
	
	exit(0);
}
