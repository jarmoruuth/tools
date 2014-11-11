/**********************************************************************\
 *
 *    COVINI.C
 *
 * Run time support for C language cover analyzer.
 *
 * Copyright (C) 1990-1991 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <ssenv.h>

#include <ssstdio.h>
#include <ssstdlib.h>
#include <ssstring.h>
#include <time.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssfile.h>
#include <ssdebug.h>

#include "cover.h"
#define _COVER_PROFILE_
#include "covini.h"

#ifdef assert
#undef assert
#endif
#include <assert.h>

#define PROFILE_FILE "profile.out"

typedef struct cover {
        void*           cov_tab;        /* cover table */
        int             cov_elem_size;  /* cover table element size */
        int             cov_count;      /* number of elements in table */
        char*           cov_srcfile;    /* source file name */
        char*           cov_lstfile;    /* list file name */
        char*           cov_curdir;     /* file directory name */
        struct cover *  cov_next;
} cover_t;

static cover_t* covlist;
static cover_t* clocklist;

static int  mainfound;
static long maxclock = 0;

/**********************************************************************
 *    cover_findpath
 */
static char* cover_findpath(
                char* fname,
                char* dir)
{
        static int initp = 0;
        static char* covpath;
        static char covdir[255];
        static char new_fname[255];
        static char tmp_fname[255];
        int i;
        char* p;
        FILE* f;

        if (!initp) {
            covpath = getenv("COVPATH");
            initp = 1;
        }
        if (dir != NULL && dir[0] != '\0') {
            strcpy(new_fname, dir);
            strcat(new_fname, "/");
            strcat(new_fname, fname);
            f = fopen(new_fname, "r");
            if (f != NULL) {
                /* file exists */
                fclose(f);
                return(new_fname);
            }
        }
        if (covpath == NULL) {
            return(fname);
        }
        /* first try in current directory */
        f = fopen(fname, "r");
        if (f != NULL) {
            fclose(f);
            return(fname);
        }
        covdir[0] = '\0';
        /* scan through COVPATH until file is found */
        for (p = covpath; *p != '\0'; ) {
            /* copy path entry to new_fname */
            for (i = 0; *p != ';' && *p != '\0'; p++, i++) {
                assert(i < sizeof(new_fname));
                new_fname[i] = *p;
            }
            if (new_fname[strlen(new_fname) - 1] == '\\') {
                /* This is a directory entry, store it into covdir. */
                strcpy(covdir, new_fname);
            } else {
                if (covdir[0] != '\0') {
                    strcpy(tmp_fname, covdir);
                    strcat(tmp_fname, new_fname);
                    strcpy(new_fname, tmp_fname);
                    i = strlen(new_fname);
                }
                assert(i + 1 + strlen(fname) + 1 < sizeof(new_fname));
                /* append slash and fname */
                new_fname[i++] = '/';
                new_fname[i++] = '\0';
                strcat(new_fname, fname);
                /* check file existency by trying to open it */
                f = fopen(new_fname, "r");
                if (f != NULL) {
                    /* file exists */
                    fclose(f);
                    return(new_fname);
                }
            }
            if (*p != '\0')
                p++;
        }
        /* file not found from COVPATH, return just the original file name */
        return(fname);
}

/**********************************************************************
 *    cover_fopen_oldlst
 */
static FILE* cover_fopen_oldlst(
                cover_t* cov,
                char*    buf)
{
        char* s;
        int retcode;

        strcpy(buf, cov->cov_lstfile);
        s = strrchr(buf, '.');
        assert(s != NULL);
    
        strcpy(s+1, "tmp");
    
        SsFRemove(buf);
    
        retcode = rename(cov->cov_lstfile, buf);
        if (retcode != 0)
            return(NULL);
    
        return(fopen(buf, "r"));
}

/**********************************************************************
 *    cover_write
 *
 * Write cover statistics to the list file. Old statistics are added
 * to the current one.
 */
static void cover_write(cover_t* cov)
{
        int i;
        int j;
        int error;
        char type;
        int ind;
        int lineno;
        long hitcount;
        int covered;
        int nfunctions;
        int nfunctionscovered;
        static char buf[144];
        static char oldlst[144];
        FILE* oldfp;
        FILE* newfp;

        cov->cov_lstfile = cover_findpath(cov->cov_lstfile, cov->cov_curdir);

        oldfp = cover_fopen_oldlst(cov, oldlst);
        if (!oldfp)
            return;
        
        newfp = fopen(cov->cov_lstfile, "w");
        if (!newfp)
            return;

        i = 0;
        error = 0;
        covered = 0;
        nfunctions = 0;
        nfunctionscovered = 0;
        while (fgets(buf, sizeof(buf), oldfp) != NULL) {
            switch ((cover_state_t)buf[0]) {
                case COVER_MODULE:
                    fprintf(newfp,
                        "%c %s, Covered: %ld%% (%d/%d), Functions: %ld%% (%d/%d)\n",
                        (char)COVER_MODULE,
                        cov->cov_srcfile,
                        (long)covered * 100L / (long)cov->cov_count,
                        covered,
                        cov->cov_count,
                        (long)nfunctionscovered * 100L / (long)nfunctions,
                        nfunctionscovered,
                        nfunctions);
                    break;
                case COVER_FILE_NAME:
                case COVER_FUNCTION_NAME:
                case COVER_COMMENT:
                case COVER_POP:
                case COVER_SWITCH:
                    fprintf(newfp, "%s", buf);
                    break;
                default:
                    sscanf(buf, "%c %d:%d:%ld", &type, &ind, &lineno, &hitcount);
                    if (ind >= cov->cov_count) {
                        error = 1;
                        break;
                    }
                    switch (cov->cov_elem_size) {
                        case sizeof(char):
                            hitcount += ((unsigned char*)cov->cov_tab)[ind];
                            break;
                        case sizeof(short):
                            hitcount += ((unsigned short*)cov->cov_tab)[ind];
                            break;
                        case sizeof(long):
                            hitcount += ((long*)cov->cov_tab)[ind];
                            break;
                        default:
                            assert(0);
                    }

                    j = 0;
                    while (buf[j] != '\n' && buf[j] != '\0' && buf[j] != '|'){
                        j++;
                    }
                    fprintf(newfp, "%c %d:%d:%ld\t\t%s", type, ind, lineno, hitcount,&buf[j]);
                
                    if (hitcount != 0)
                        covered++;
                    if (type == COVER_FUNCTION) {
                        nfunctions++;
                        if (hitcount != 0)
                            nfunctionscovered++;
                    }
                    i++;
                    break;
            }
        }
        if (error)
            fprintf(newfp, "Error: list file and cover table does not match\n");

        fclose(oldfp);
        fclose(newfp);
        SsFRemove(oldlst);
}

/**********************************************************************
 *	clock_finish
 */
static void clock_finish(cover_t* cov)
{
        int             i;
        _cover_clock_t* ct;
        
        ct = cov->cov_tab;
        for (i = 0; i < cov->cov_count; i++, ct++) {
            if (ct->ct_count) {
                if (ct->ct_running)
                    _cover_stop_clock_(ct);
                mainfound |= (strcmp(ct->ct_function, "main") == 0);
                maxclock = max(maxclock, ct->ct_clock);
            }
        }
}

/**********************************************************************
 *	clock_write
 */
static void clock_write(FILE* f, cover_t* cov)
{
        int             i;
        _cover_clock_t* ct;
        
        ct = cov->cov_tab;
        for (i = 0; i < cov->cov_count; i++, ct++) {
            if (ct->ct_count) {
                fprintf(f, "%3ld%% %5ld.%03ld %6ld %5ld.%03ld  %-12s %s\n",
                    ct->ct_clock * 100 / maxclock,  /* percentage from max time */
                    (long)(ct->ct_clock / CLK_TCK), /* seconds */
                    (long)(ct->ct_clock % CLK_TCK), /* 1/100 seconds */
                    ct->ct_count,                   /* number of calls */
                    (long)((ct->ct_clock / ct->ct_count) / CLK_TCK), /* avg time */
                    (long)((ct->ct_clock / ct->ct_count) % CLK_TCK), /* per call */
                    cov->cov_srcfile,
                    ct->ct_function);
            }
        }
}

/**********************************************************************
 *    cover_exit
 */
static void SS_CLIBCALLBACK cover_exit(void)
{
        cover_t* list;
        time_t   ptime;
        FILE*    f;
        
        list = clocklist;
        while (list) {
            clock_finish(list);
            list = list->cov_next;
        }

        while (covlist) {
            cover_write(covlist);
            covlist = covlist->cov_next;
        }

        if (clocklist == NULL)
            return;

        f = fopen(PROFILE_FILE, "a");
        if (f == NULL) {
            printf("Failed to open file '%s'\n", PROFILE_FILE);
            return;
        }

        time(&ptime);
        fprintf(f, "Profile output at %s", ctime(&ptime));

        if (!mainfound)
            fprintf(f, "Warning: function main not found from profile list\n");
        fprintf(f, "%4c %9s %6s %9s  %-12s %s\n",
            '%', "time", "calls", "avg", "file", "function");
        if (maxclock == 0)
            maxclock = 1;

        while (clocklist) {
            clock_write(f, clocklist);
            clocklist = clocklist->cov_next;
        }

        fprintf(f, "\n");
        fclose(f);
}

/**********************************************************************
 *    cover_allocate
 */
static cover_t* cover_allocate(
                    char*    srcfile,
                    char*    lstfile,
                    char*    curdir,
                    int      elem_size,
                    int      count,
                    cover_t* next)
{
        cover_t* tmpcover;
        
        tmpcover = malloc(sizeof(cover_t));
        assert(tmpcover != NULL);

        tmpcover->cov_tab = calloc(count, elem_size);
        assert(tmpcover->cov_tab);
        tmpcover->cov_elem_size = elem_size;
        tmpcover->cov_count = count;
        tmpcover->cov_srcfile = srcfile;
        tmpcover->cov_lstfile = lstfile;
        tmpcover->cov_curdir = curdir;
        tmpcover->cov_next = next;
        
        return(tmpcover);
}

/**********************************************************************
 *    _cover_init_file_
 */
void _cover_init_file_(
            char* srcfile,
            char* lstfile,
            char* curdir,
            void* p_cover_tab,
            int   elem_size,
            int   cover_count)
{
        static int initp = 0;

        ss_dprintf_1(("_cover_init_file_:srcfile=%s, lstfile=%s\n", srcfile, lstfile));
        
        covlist = cover_allocate(srcfile, lstfile, curdir, elem_size, cover_count, covlist);
        *(char**)p_cover_tab = covlist->cov_tab;
        
        if (!initp) {
            int rc;
            rc = atexit(cover_exit);
            ss_dprintf_1(("_cover_init_file_:atexit(cover_exit) returns %d\n", rc));
            initp = 1;
        }
}

/**********************************************************************
 *	_cover_init_clock_
 */
void _cover_init_clock_(
            char* srcfile,
            char* lstfile,
            void* p_cover_clock,
            int   elem_size,
            int   elem_count)
{
        clocklist = cover_allocate(srcfile, lstfile, "", elem_size, elem_count, clocklist);
        *(_cover_clock_t**)p_cover_clock = clocklist->cov_tab;
}

/**********************************************************************
 *	_cover_start_clock_
 */
void _cover_start_clock_(
            char*           function,
            _cover_clock_t* ct)
{
        if (!ct->ct_count) {
            ct->ct_function = function;
            ct->ct_clock = (long)clock();
        } else {
            ct->ct_clock = (long)clock() - ct->ct_clock;
        }
        ct->ct_running = 1;
        ct->ct_count++;
}

/**********************************************************************
 *	_cover_stop_clock_
 */
void _cover_stop_clock_(
            _cover_clock_t* ct)
{
        ct->ct_clock = (long)clock() - ct->ct_clock;
        ct->ct_running = 0;
}

/**********************************************************************
 *    _cover_assert_
 */
void _cover_assert_(int not_exp, char* file, int line)
{
        if (not_exp) {
            printf("\nAssertion failure: file %s, line %d\n", file, line);
            exit(1);
        }
}
