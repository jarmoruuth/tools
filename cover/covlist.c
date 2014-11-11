/**********************************************************************\
 *
 *	COVLIST.C
 *
 * Listing utility for Covpp output.
 *
 * Copyright (C) 1990-1996 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "cover.h"

char _Copyright_[] = "Copyright (C) 1990-1996 by Jarmo Ruuth";

#define MAXSTACK    255

static int  exitcode = 0;
static long total_covered = 0;
static long total_covercount = 0;
static long total_funcovered = 0;
static long total_funcount = 0;
static int  verbose = 0;

static int  stack_pos = 0;
static char stack_type[MAXSTACK];
static int  stack_coveredp[MAXSTACK];

static void stack_set(char type, int coveredp)
{
        stack_type[stack_pos] = type;
        stack_coveredp[stack_pos] = coveredp;
}

static void stack_push(char type, int coveredp)
{
        stack_pos++;
        stack_set(type, coveredp);
}

static void stack_pop(void)
{
        if (stack_pos > 0) {
            stack_pos--;
        }   
}

/**********************************************************************
 *	skiptoline
 *
 */
static void skiptoline(
        FILE* outfp,
        FILE* cfp,
        int lineno,
        int* p_clineno)
{
        char linebuf[20];
        static char buf[4096];

        if (verbose > 1) {
            printf("skiptoline:lineno=%d, *p_clineno=%d, coveredp=%d, type=%c\n",
                lineno, *p_clineno, stack_coveredp[stack_pos], stack_type[stack_pos]);
        }

        if (*p_clineno == lineno) {
            sprintf(linebuf, "%5d", *p_clineno);
            if (verbose) {
                printf("%s %c %c | \n", linebuf, stack_type[stack_pos], stack_coveredp[stack_pos] ? ' ' : 'X');
            }
            fprintf(outfp, "%s %c %c | \n", linebuf, stack_type[stack_pos], stack_coveredp[stack_pos] ? ' ' : 'X');
        }

        while (*p_clineno < lineno) {
            if (fgets(buf, sizeof(buf), cfp) == NULL) {
                break;
            }
            sprintf(linebuf, "%5d", *p_clineno);
            if (verbose) {
                printf("%s %c %c | ", linebuf, stack_type[stack_pos], stack_coveredp[stack_pos] ? ' ' : 'X');
                fputs(buf, stdout);
            }
            fprintf(outfp, "%s %c %c | ", linebuf, stack_type[stack_pos], stack_coveredp[stack_pos] ? ' ' : 'X');
            fputs(buf, outfp);
            (*p_clineno)++;
        }
}

/**********************************************************************
 *	covlist_write
 *
 * Write cover statistics from the list file to the output file.
 */
static void covlist_write(char* covfile)
{
	int covercount;
	int cnt;
	char type;
	int ind;
	int lineno;
	int clineno;
	long hitcount;
	int covered;
        int funcount;
        int funcovered;
	char buf[144];
	char fname[144];
        char function[33];
	FILE* infp;
	FILE* outfp;
	FILE* cfp;
	char cfile[144];
	char clsfile[144];
        char* p;
        int last_was_pop = 0;

	printf("%s ", covfile);

        strcpy(cfile, covfile);
        p = strrchr(cfile, '.');
        assert(p != NULL);
        strcpy(p, ".c");

        strcpy(clsfile, covfile);
        p = strrchr(clsfile, '.');
        assert(p != NULL);
        strcpy(p, ".cls");

	infp = fopen(covfile, "r");
	if (!infp) {
		printf("covlist: Error: can't open cover file '%s'\n", covfile);
		exitcode = 1;
		return;
	}
	cfp = fopen(cfile, "r");
	if (!cfp) {
		printf("covlist: Error: can't open C source file '%s'\n", cfile);
		exitcode = 1;
		return;
	}
	outfp = fopen(clsfile, "w");
	if (!outfp) {
		printf("covlist: Error: can't create output file file '%s'\n", clsfile);
		exitcode = 1;
		return;
	}
	
        stack_pos = 0;
	covercount = 0;
	covered = 0;
        funcount = 0;
        funcovered = 0;
        lineno = 1;
        clineno = 1;
        stack_push(' ', 1);
	while (fgets(buf, sizeof(buf), infp) != NULL) {
            switch ((cover_state_t)buf[0]) {
                case COVER_FILE_NAME:
                    sscanf(buf, "%c %s", &type, fname);
                    break;
                case COVER_FUNCTION_NAME:
                    sscanf(buf, "%c %s", &type, function);
                    break;
                case COVER_POP:
				cnt = sscanf(buf, "%c %d", &type, &lineno);
                    /* if (cnt == 2 && lineno > clineno) { */
                    if (cnt == 2) {
                        if (!(lineno == clineno && last_was_pop)) {
                            skiptoline(outfp, cfp, lineno, &clineno);
                        }
                    }
                    stack_pop();
                    break;
			case COVER_COMMENT:
                    sscanf(buf, "%cFunction '%s'", &type, function);
				break;
			case COVER_MODULE:
				sscanf(buf, "%c %s", &type, fname);
                    skiptoline(outfp, cfp, -1, &clineno);
				break;
			case COVER_SWITCH:
				sscanf(buf, "%c %d", &type, &lineno);
                    if (!(lineno == clineno && last_was_pop)) {
                        skiptoline(outfp, cfp, lineno, &clineno);
                    }
                    stack_push(type, 1);
                    break;
			default:
				cnt = sscanf(buf, "%c %d:%d:%ld", &type, &ind,
					&lineno, &hitcount);
				if (cnt != 4) {
                        if (verbose) {
                            printf("Format error, type %c\n", type);
                        }
					break;
                    }
                    if (!(lineno == clineno && last_was_pop)) {
                        skiptoline(outfp, cfp, lineno, &clineno);
                    }
				if (hitcount > 0) {
					covered++;
                    }
				covercount++;
                    switch (type) {
	                case COVER_FUNCTION:
                            funcount++;
                            if (hitcount != 0)
                                funcovered++;
                            /* FALLTHROUGH */
	                case COVER_IF:
	                case COVER_FOR:
	                case COVER_WHILE:
	                case COVER_DO:
                            stack_push(type, hitcount > 0);
                            break;
	                case COVER_CASE:
	                case COVER_ELSE:
                            stack_set(type, hitcount > 0);
                            break;
                        default:
                            break;

                    }
				break;
		}
            last_was_pop = (cover_state_t)buf[0] == COVER_POP;

	}
	fclose(infp);
	fclose(cfp);

        fprintf(outfp, "Total in %s\n",fname);

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
		fprintf(outfp, "\tCovered    %3ld%%  (%d/%d)\n"
                           "\tFunctions  %3ld%%  (%d/%d)\n",
			cover_prc,
                covered, covercount,
			func_prc,
                funcovered, funcount);
            if (verbose) {
                printf("Covered %3ld%% Functions %3ld%%", cover_prc, func_prc);
            }
        }
        fprintf(outfp, "\n");
        printf("\n");

        fclose(outfp);

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
	if (argc == 1) {
		printf("Usage: covlist [-v] <cover list file>...\n");
		exit(1);
	}
	
        for (argv++; *argv != NULL && (*argv)[0] == '-'; argv++) {
            int i;
            for (i = 1; (*argv)[i] != '\0'; i++) {
                switch((*argv)[i]) {
                    case 'v':
                        verbose++;
                        break;
                    default:
		            printf("Usage: covlist [-v] <cover list file>...\n");
                        exit(1);
                }
            }
        }

	for (; *argv; argv++)
		covlist_write(*argv);
	
	exit(exitcode);
}
