/**********************************************************************\
 *
 *	COVSTAT.C
 *
 * Analyzer for Covpp output.
 *
 * Copyright (C) 1990-1991 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cover.h"

char _Copyright_[] = "Copyright (C) 1990-1991 by Jarmo Ruuth";

#define OUTFILE "covstat.out"

static int  exitcode = 0;
static long total_covered = 0;
static long total_covercount = 0;
static long total_funcovered = 0;
static long total_funcount = 0;
static long total_files = 0;
static int  verbose = 0;

static long cover_prc;
static long func_prc;

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
 *	output_uncovered
 */
static void output_uncovered(
                FILE* fp,
                char* filename,
                char* function,
                char type,
                int lineno)
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
        fprintf(fp, "%-12s  %-30s %5d  %s\n", filename, function, lineno,
                    typestr);
}

/**********************************************************************
 *	covstat_write
 *
 * Write cover statistics from the list file to the output file.
 */
static void covstat_write(char* lstfile, FILE* outfp, int summary_only)
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
		printf("covstat: Error: can't open input file '%s'\n", lstfile);
		exitcode = 1;
		return;
	}

        total_files++;
	
	printf("%-13s ", lstfile);

        strcpy(fname, lstfile);

        if (!summary_only) {
            fprintf(outfp, "File          Function                        Line  Type\n");
            fprintf(outfp, "--------------------------------------------------------\n\n");
        }
	
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
				if (hitcount == 0) {
                        if (!summary_only) {
                            output_uncovered(outfp, fname, function, type,
                                            lineno);
                        }
				} else {
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

        if (!summary_only) {
            fprintf(outfp, "\n");
        }

        fprintf(outfp, "Total in %s%c",
            fname,
            summary_only ? ' ' : '\n');

        if (covercount == 0 || funcount == 0) {
		fprintf(outfp, "\tNo cover points!\n");
            if (verbose) {
                printf("No cover points!");
            }
        } else {
            long cover_prc;
            long func_prc;
            cover_prc = (long)covered * 100L / (long)covercount;
            func_prc = (long)funcovered * 100L / (long)funcount;
		fprintf(outfp, "\tCovered    %3ld%%  (%d/%d)%c"
                           "\tFunctions  %3ld%%  (%d/%d)\n",
			cover_prc,
                covered, covercount,
                summary_only ? ' ' : '\n',
			func_prc,
                funcovered, funcount);
            if (verbose) {
                printf("Covered %3ld%% Functions %3ld%%", cover_prc, func_prc);
            }
        }
        if (!summary_only) {
            fprintf(outfp, "\n");
        }
        printf("\n");

	total_covered += covered;
	total_covercount += covercount;
	total_funcovered += funcovered;
	total_funcount += funcount;
}

/**********************************************************************
 *	output_total
 */
static void output_total(FILE* fp)
{
        cover_prc = total_covercount ? (total_covered * 100L / total_covercount) : 0;
        func_prc = total_funcovered ? (total_funcovered * 100L / total_funcount) : 0;

	fprintf(fp, "\nTotal in all %ld modules\n"
		        "\tCovered    %3ld%%  (%ld/%ld)\n"
                    "\tFunctions  %3ld%%  (%ld/%ld)\n",
                total_files,
			cover_prc,
			total_covered, total_covercount,
			func_prc,
			total_funcovered, total_funcount);
			
}

/**********************************************************************
 *	update_gfile
 */
static void update_gfile(char* gfname)
{
        FILE* fp;
        char* format = "Global info %ld %ld %ld %ld %ld\n";

        if (total_files == 0) {
            return;
        }

        fp = fopen(gfname, "r");
        if (fp != NULL) {
		long gtotal_covered = 0;
            long gtotal_covercount = 0;
		long gtotal_funcovered = 0;
            long gtotal_funcount = 0;
            long gtotal_files = 0;

            fscanf(fp, format, &gtotal_covered, &gtotal_covercount, &gtotal_funcovered, &gtotal_funcount, &gtotal_files);
		
            total_covered += gtotal_covered;
            total_covercount += gtotal_covercount;
		total_funcovered += gtotal_funcovered;
            total_funcount += gtotal_funcount;
            total_files += gtotal_files;
            fclose(fp);
        }
        fp = fopen(gfname, "w");
        if (fp != NULL) {
            fprintf(fp, format, total_covered, total_covercount, total_funcovered, total_funcount, total_files);
            output_time(fp);
            output_total(fp);
            fclose(fp);
        }
}

/**********************************************************************
 *	usage
 */
static void usage(void)
{
	printf("Usage: covstat [-sv] [-g<global cover file>] <cover list file>...\n");
        exit(1);
}

/**********************************************************************
 *	main
 */
void main(int argc, char* argv[])
{
	FILE* fp;
        int summary_only = 0;
        char* gfname = NULL;
	
	if (argc == 1) {
            usage();
	}
	
	fp = fopen(OUTFILE, "w");
	if (!fp) {
		printf("covstat: Error: can't open output file '%s'\n", OUTFILE);
		exit(1);
	}

        for (argv++; *argv != NULL && (*argv)[0] == '-'; argv++) {
            int i;
            for (i = 1; (*argv)[i] != '\0'; i++) {
                switch((*argv)[i]) {
                    case 'g':
                        if ((*argv)[i+1] == '\0') {
                            usage();
                        }
                        gfname = &(*argv)[i+1];
                        break;
                    case 's':
                        summary_only = 1;
                        continue;
                    case 'v':
                        verbose = 1;
                        continue;
                    default:
                        usage();
                }
                break;
            }
        }

        output_time(fp);

	for (; *argv; argv++)
		covstat_write(*argv, fp, summary_only);
	
        if (!summary_only) {
            fprintf(fp, "All modules\n");
            fprintf(fp, "--------------------------------------------------------\n");
        }

        output_total(fp);

	fclose(fp);

        if (verbose) {
            printf("%-13s Covered %3ld%% Functions %3ld%%\n",
                "TOTAL", cover_prc, func_prc);
        }
	
        if (gfname != NULL) {
            update_gfile(gfname);
        }

	printf("Output is in file '%s'\n", OUTFILE);
	
	exit(exitcode);
}
