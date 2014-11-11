/*************************************************************************\
**  source       * tqmem.c
**  directory    * \common\os2_16\port\ss\test
**  description  * Tests for quick memory allocation.
**               * 
**  author       * SOLID / jarmo
**  date         * 1993-07-15
**               * 
**               * (C) Copyright Solid Information Technology Ltd 1993
**************************************************************************
** TLIB version ** '%v'
** This source was extracted from library version = ''
**************************************************************************
** TLIB history **
** TLIB history **
\*************************************************************************/
#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include "../ssstdio.h"
#include "../ssconio.h"
#include "../ssstdlib.h"
#include "../ssstring.h"
#include "../sstime.h"

#include "../ssc.h"
#include "../ssdebug.h"
#include "../ssqmem.h"
#include "../ssmem.h"
#include "../ssdtoa.h"
#include "../sslimits.h"

#ifdef SS_NLM
#undef malloc
#undef calloc
#undef realloc
#undef strdup
#undef free
#endif /* SS_NLM */

#define FILLCH(i)   ((uchar)((i) % 0xff))
#define NBASIC      3000

#define rand() irand()
#define srand(x) seed(x)

#define A            16807
#define M       2147483647L
#define Q           127773L      /* M div A */
#define R             2836      /* M mod A */

static long      Seed = 1;       /* seed value for all functions */

/******************************************************************************
*                                                                             *
* seed - load the Seed value used in irand and drand.  Should be used before  *
*       first call to irand or drand.                                         *
******************************************************************************/
static void seed(long val)
{

    if ( val < 0 )
            val = -val;

    Seed = val;
}

/*****************************************************************************
*                                                                            *
* irand - returns a 32 bit integer pseudo random number with a period of     *
*       1 to 2 ^ 32 - 1.                                                     *
*                                                                            *
* parameters:                                                                *
*       none.                                                                *
*                                                                            *
* returns:                                                                   *
*       32 bit integer - defined as long ( see above ).                       *
*                                                                            *
* side effects:                                                              *
*       seed get recomputed.                                                 *
*****************************************************************************/
#define irand() \
(\
    (Seed = A * (Seed % Q) - R * (Seed / Q)),\
    (Seed = Seed > 0 ? Seed : (Seed + M))\
)

static bool checkmem(uchar* p, uint sz, uchar chk)
{
        uint i;
        uint end;
        uint start;

        if (sz <= 10) {
            end = sz;
            start = 10;
        } else {
            end = 10;
            start = sz - 10;
        }

        for (i = 0; i < end; i++) {
            if (p[i] != chk) {
                printf("%d (pos = %d, chk = %d, val = %d)\n", sz, i, chk, p[i]);
                return(FALSE);
            }
        }
        for (i = start; i < sz; i++) {
            if (p[i] != chk) {
                printf("%d (pos = %d, chk = %d, val = %d)\n", sz, i, chk, p[i]);
                return(FALSE);
            }
        }
        return(TRUE);
}

static void test_basic(void)
{
        uint i;
        char* p;

        printf("test_basic\n");

        for (i = 0; i < NBASIC; i++) {
            if (i % 100 == 0) {
                printf("%d\r", i);
            }
            p = SsQmemAlloc(i);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            memset(p, FILLCH(i), i);
            SsQmemFree(p);
        }
        printf("%d\n", i);
        for (i = 0; i < NBASIC; i++) {
            if (i % 100 == 0) {
                printf("%d\r", i);
            }
            p = SsQmemAlloc(i);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            memset(p, FILLCH(i+1), i);
            p = SsQmemRealloc(p, i + 10);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            ss_assert(checkmem(p, i, FILLCH(i+1)));
            SsQmemFree(p);
        }
        printf("%d\n", i);
        for (i = 51; i < NBASIC; i++) {
            if (i % 100 == 0) {
                printf("%d\r", i);
            }
            p = SsQmemAlloc(i);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            memset(p, FILLCH(i+2), i);
            p = SsQmemRealloc(p, i - 51);
            ss_assert((ulong)p % SS_ALIGNMENT == 0);
            ss_assert(checkmem(p, i - 51, FILLCH(i+2)));
            SsQmemFree(p);
        }
        printf("%d\n", i);
}

static char* get_time_in_string(double d)
{
        static char str[20];

        SsDoubleToAscii(d, str, 15);

        return(str);
}

#define NPTR    3000
#define MAXSIZE 2048

static void test_speed(bool qmemp, long nspeed)
{
        long   i;
        uint   perc;
        uint   r;
        size_t pi;
        size_t size;
        double tmd;
        long   nalloc = 0, nfree = 0, nrealloc = 0;

        static uint* p[NPTR];

        printf("test_speed: %s, nspeed = %ld\n",
            qmemp ? "Qmem" : "system", nspeed);

        srand(7);

        printf("warm up\n");

        for (i = 0; i < NPTR; i++) {

            r = rand();
            perc = r % 100;
            r = rand();
            pi = r % NPTR;

            if (perc < 70) {            /* 70% 0 - 100 bytes */
                size = r % 100;
            } else if (perc < 90) {     /* 20% 100 - 500 bytes */
                size = 100 + r % (500 - 100);
            } else {                    /* 10% 500 - MAXSIZE bytes */
                size = 500 + r % (MAXSIZE - 500);
            }

            if (p[pi] != NULL) {
                if (qmemp) {
                    SsQmemFree(p[pi]);
                } else {
                    free(p[pi]);
                }
            } else {
                nalloc++;
            }
            if (qmemp) {
                p[pi] = SsQmemAlloc(size);
            } else {
                p[pi] = malloc(size);
            }
        }

        printf("go (warm up added %ld allocations)\n", nalloc);
        nalloc = 0;

        tmd = SsTimeStamp();

        for (i = 0; i < nspeed; i++) {

            r = rand();
            perc = r % 100;
            r = rand();
            pi = r % NPTR;

            if (perc < 70) {            /* 70% 0 - 100 bytes */
                size = r % 100;
            } else if (perc < 90) {     /* 20% 100 - 500 bytes */
                size = 100 + r % (500 - 100);
            } else {                    /* 10% 500 - MAXSIZE bytes */
                size = 500 + r % (MAXSIZE - 500);
            }

            perc = r % 100;

            if (perc < 45) {

                /* alloc */
                nalloc++;
                if (p[pi] != NULL) {
                    nfree++;
                    if (qmemp) {
                        SsQmemFree(p[pi]);
                    } else {
                        free(p[pi]);
                    }
                }
                if (qmemp) {
                    p[pi] = SsQmemAlloc(size);
                } else {
                    p[pi] = malloc(size);
                }

            } else if (perc < 90) {

                /* free */
                if (p[pi] != NULL) {
                    nfree++;
                    if (qmemp) {
                        SsQmemFree(p[pi]);
                    } else {
                        free(p[pi]);
                    }
                    p[pi] = NULL;
                } else {
                    nalloc++;
                    if (qmemp) {
                        p[pi] = SsQmemAlloc(size);
                    } else {
                        p[pi] = malloc(size);
                    }
                }

            } else {

                /* realloc */
                if (p[pi] != NULL) {
                    nrealloc++;
                    if (qmemp) {
                        p[pi] = SsQmemRealloc(p[pi], size);
                    } else {
                        p[pi] = realloc(p[pi], size);
                    }
                } else {
                    nalloc++;
                    if (qmemp) {
                        p[pi] = SsQmemAlloc(size);
                    } else {
                        p[pi] = malloc(size);
                    }
                }
            }
        }

        tmd = SsTimeStamp() - tmd;
        if (tmd == 0.0) {
            tmd = 0.00001;
        }

        printf("Time %s seconds, ",
            get_time_in_string(tmd));
        printf("%s operations/second\n",
            get_time_in_string((double)nspeed / tmd));
        printf("nalloc = %ld, nfree = %ld, nrealloc = %ld\n",
            nalloc, nfree, nrealloc);
}

int SS_CDECL main(int argc, char* argv[])
{
        SS_INIT_DEBUG;

        SsQmemGlobalInit();

        if (argc == 1) {

            printf("Basic test\n");

            test_basic();
            printf("Speed test\n");
            test_speed(TRUE, 50000);

        } else {

            printf("Speed test\n");
            test_speed(argc == 2, atol(argv[1]));
        }

        SS_WINGETCH;

        return(0);
}
