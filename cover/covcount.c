/**********************************************************************\
 *
 *	COVCOUNT.C
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
#include <time.h>

#include "cover.h"

char _Copyright_[] = "Copyright (C) 1990-1991 by Jarmo Ruuth";

#define OUTFILE "covcount.out"

static int  exitcode = 0;
static long count_limit = 10000;
static long total_covered = 0;
static long total_covercount = 0;
static long total_funcovered = 0;
static long total_funcount = 0;

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
 *	covcount_write
 *
 * Write cover statistics from the list file to the output file.
 */
static void covcount_write(char* lstfile, FILE* outfp)
{
	int covercount;
	int cnt;
	char type;
	int ind;
	int lineno;
	long hitcount;
	int covered;
        int funcount;
        int funcovered;
	char buf[144];
	char fname[144];
        char function[33];
	FILE* infp;

	infp = fopen(lstfile, "r");
	if (!infp) {
		printf("covcount: Error: can't open input file '%s'\n", lstfile);
		exitcode = 1;
		return;
	}
	
	printf("%s\n", lstfile);

        strcpy(fname, lstfile);

        fprintf(outfp, "File          Function                        Line  Count     Type\n");
        fprintf(outfp, "------------------------------------------------------------------\n\n");
	
	covercount = 0;
	covered = 0;
        funcount = 0;
        funcovered = 0;
	while (fgets(buf, sizeof(buf), infp) != NULL) {
		switch ((cover_state_t)buf[0]) {
                case COVER_FILE_NAME:
                    sscanf(buf, "%c %s", &type, fname);
                    break;
                case COVER_FUNCTION_NAME:
                    sscanf(buf, "%c %s", &type, function);
                    break;
			case COVER_COMMENT:
                    sscanf(buf, "%cFunction '%s'", &type, function);
				break;
			case COVER_MODULE:
				sscanf(buf, "%c %s", &type, fname);
				break;
                case COVER_POP:
                case COVER_SWITCH:
				sscanf(buf, "%c %d", &type, &lineno);
				break;
			default:
				cnt = sscanf(buf, "%c %d:%d:%ld", &type, &ind,
					&lineno, &hitcount);
				if (cnt != 4)
					break;
				if (hitcount >= count_limit) {
                        output_covered(outfp, fname, function, type,
                                         lineno, hitcount);
                    }
				if (hitcount != 0) {
					covered++;
                    }
				covercount++;
                    if (type == COVER_FUNCTION) {
                        funcount++;
                        if (hitcount != 0)
                            funcovered++;
                    }
				break;
		}
	}
	fclose(infp);

        fprintf(outfp, "\nTotal in %s\n", fname);
	
        if (covercount == 0 || funcount == 0) {
		fprintf(outfp, "\tNo cover points!\n");
        } else {
		fprintf(outfp, "\tCovered    %3ld%%  (%d/%d)\n"
                           "\tFunctions  %3ld%%  (%d/%d)\n",
			(long)covered * 100L / (long)covercount,
                covered, covercount,
			(long)funcovered * 100L / (long)funcount,
                funcovered, funcount);
        }
        fprintf(outfp, "\n");

	total_covered += covered;
	total_covercount += covercount;
	total_funcovered += funcovered;
	total_funcount += funcount;
}

/**********************************************************************
 *	main
 */
void main(int argc, char* argv[])
{
	FILE* fp;
	
        if (argc > 1 && isdigit(argv[1][0])) {
            sscanf(argv[1], "%ld", &count_limit);
            argc--;
            argv++;
        }

	if (argc == 1) {
		printf("Usage: covcount [count limit] <cover list file>...\n");
		exit(1);
	}
	
	fp = fopen(OUTFILE, "w");
	if (!fp) {
		printf("covcount: Error: can't open output file '%s'\n", OUTFILE);
		exit(1);
	}

        output_time(fp);

	for (argv++; *argv; argv++)
		covcount_write(*argv, fp);
	
        fprintf(fp, "All modules\n");
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
