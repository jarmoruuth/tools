/**********************************************************************\
 *
 *	COVSORT.C
 *
 * Analyzer for Covpp output.
 *
 * Copyright (C) 1990-1991 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#include "cover.h"

char _Copyright_[] = "Copyright (C) 1990-1991 by Jarmo Ruuth";

#define OUTFILE "covsort.out"

typedef struct covlist_s {
        struct covlist_s* next;
        char*             filename;
        char*             function;
        int               lineno;
        long              count;
        char              type;
} covlist_t;

static int  exitcode = 0;
static long count_limit = 10000;
static long total_covered = 0;
static long total_covercount = 0;
static long total_funcovered = 0;
static long total_funcount = 0;

static covlist_t  dummy = { &dummy, NULL, NULL, 0, 0L, '\0' };
static covlist_t* z = &dummy;

/**********************************************************************
 *	output_time
 */
static void output_time(FILE* fp)
{
        time_t now;

        time(&now);
        fprintf(fp, "%s\n", ctime(&now));
}

/**********************************************************************
 *	covlist_add
 */
static covlist_t* covlist_add(
                    covlist_t* covlist,
                    char* filename,
                    char* function,
                    char type,
                    int lineno,
                    long count)
{
        covlist_t* new_covlist;

        new_covlist = malloc(sizeof(covlist_t));
        assert(new_covlist != NULL);

        new_covlist->filename = filename;
        new_covlist->function = function;
        new_covlist->type = type;
        new_covlist->lineno = lineno;
        new_covlist->count = count;
        new_covlist->next = covlist;

        return(new_covlist);
}

/***********************************************************************
 *      merge
 * 
 * Merges two sorted list into one sorted list in descending count order.
 */
static covlist_t* merge(covlist_t* a, covlist_t* b)
{
        covlist_t* c;

        c = z;
        do {
            if (a->count > b->count) {
                c->next = a;
                c = a;
                a = a->next;
            } else {
                c->next = b;
                c = b;
                b = b->next;
            }
        } while (c != z);
        c = z->next;
        z->next = z;
        return(c);
}

/***********************************************************************
 *      covlist_sort
 * 
 * Sorts cover list by count field using recursive merge sort into
 * descending order.
 */
static covlist_t* covlist_sort(covlist_t* c)
{
        covlist_t* a;
        covlist_t* b;

        if (c->next != z) {
            a = c;
            b = c->next->next->next;
            while (b != z) {
                c = c->next;
                b = b->next->next;
            }
            b = c->next;
            c->next = z;
            return(merge(covlist_sort(a), covlist_sort(b)));
        }
        return(c);
}

/**********************************************************************
 *	output_covered
 */
static void output_covered(
                FILE* fp,
                char* filename,
                char* function,
                char type,
                int lineno,
                long count)
{
        char* typestr;

        switch (type) {
	    case COVER_IF:
                typestr = "If";
                break;
	    case COVER_ELSE:
                typestr = "Else";
                break;
	    case COVER_FOR:
                typestr = "For";
                break;
	    case COVER_WHILE:
                typestr = "While";
                break;
	    case COVER_DO:
                typestr = "Do";
                break;
	    case COVER_CASE:
                typestr = "Case";
                break;
	    case COVER_FUNCTION:
                typestr = "Function entry";
                break;
            default:
                typestr = "*** Cover file format error! ***";
                break;
        }
        fprintf(fp, "%-12s  %-30s %5d  %9ld %s\n", filename, function, lineno,
                    count, typestr);
}

/**********************************************************************
 *	covsort_read
 *
 * Adds cover statistics from the list file to the output list that
 * is sorted later.
 */
static covlist_t* covsort_read(char* lstfile, covlist_t* covlist)
{
	int cnt;
	char type;
	int ind;
	int lineno;
	long hitcount;
	char buf[144];
	char fname[144];
        char function[33];
        char* list_filename = NULL;
        char* list_function = NULL;
	FILE* infp;

	infp = fopen(lstfile, "r");
	if (!infp) {
		printf("covsort: Error: can't open input file '%s'\n", lstfile);
		exitcode = 1;
		return(covlist);
	}
	
	printf("%s\n", lstfile);

        strcpy(fname, lstfile);

	while (fgets(buf, sizeof(buf), infp) != NULL) {
		switch ((cover_state_t)buf[0]) {
                case COVER_FILE_NAME:
                    sscanf(buf, "%c %s", &type, fname);
                    list_filename = strdup(fname);
                    assert(list_filename != NULL);
                    break;
                case COVER_FUNCTION_NAME:
                    sscanf(buf, "%c %s", &type, function);
                    list_function = strdup(function);
                    assert(list_function != NULL);
                    break;
			case COVER_COMMENT:
                    sscanf(buf, "%cFunction '%s'", &type, function);
				break;
			case COVER_POP:
                case COVER_SWITCH:
				sscanf(buf, "%c %d", &type, &lineno);
				break;
			case COVER_MODULE:
				sscanf(buf, "%c %s", &type, fname);
				break;
			default:
				cnt = sscanf(buf, "%c %d:%d:%ld", &type, &ind,
					&lineno, &hitcount);
				if (cnt != 4)
					break;
				if (hitcount >= count_limit) {
                        covlist = covlist_add(covlist, list_filename,
                                        list_function, type,
                                        lineno, hitcount);
                    }
				if (hitcount != 0) {
					total_covered++;
                    }
				total_covercount++;
                    if (type == COVER_FUNCTION) {
                        total_funcount++;
                        if (hitcount != 0)
                            total_funcovered++;
                    }
				break;
		}
	}
	fclose(infp);

	return(covlist);
}

/***********************************************************************
 *      covsort_write
 * 
 * Writes sorted cover list to file.
 */
static void covsort_write(covlist_t* covlist, FILE* fp)
{
        for (; covlist != z; covlist = covlist->next) {
            output_covered(fp, covlist->filename, covlist->function,
                           covlist->type, covlist->lineno, covlist->count);
        }
}

/**********************************************************************
 *	main
 */
void main(int argc, char* argv[])
{
        covlist_t* covlist = z;
	FILE* fp;
	
        if (argc > 1 && isdigit(argv[1][0])) {
            sscanf(argv[1], "%ld", &count_limit);
            argc--;
            argv++;
        }

	if (argc == 1) {
		printf("Usage: covsort [count limit] <cover list file>...\n");
		exit(1);
	}
	
	fp = fopen(OUTFILE, "w");
	if (!fp) {
		printf("covsort: Error: can't open output file '%s'\n", OUTFILE);
		exit(1);
	}

        output_time(fp);

        fprintf(fp, "File          Function                        Line  Count     Type\n");
        fprintf(fp, "------------------------------------------------------------------\n\n");

	printf("Reading...\n");
	for (argv++; *argv; argv++)
		covlist = covsort_read(*argv, covlist);

        printf("Sorting...\n");
        covlist = covlist_sort(covlist);

        printf("Writing...\n");
        covsort_write(covlist, fp);
	
        fprintf(fp, "\nAll modules\n");
        fprintf(fp, "--------------------------------------------------------\n\n");

	fprintf(fp, "Total in all modules\n"
		        "\tCovered    %3ld%%  (%ld/%ld)\n"
                    "\tFunctions  %3ld%%  (%ld/%ld)\n",
			total_covered * 100L / total_covercount,
			total_covered, total_covercount,
			total_funcovered * 100L / total_funcount,
			total_funcovered, total_funcount);
			
	fclose(fp);
	
	printf("Output is in file '%s'\n", OUTFILE);
	
	exit(exitcode);
}
