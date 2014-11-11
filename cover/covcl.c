/**********************************************************************\
 *
 *	COVCL.C
 *
 * C-source cover analyzer cl-compiler driver.
 *
 * Copyright (C) 1991-1993 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <malloc.h>
#include <assert.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>

#define C_PP_EXT            ".i"    /* C preprocessor output file */
#define COVER_PP_EXT        ".tmp"  /* cover preprocessor output file */
#define COVER_OUTPUT_EXT    ".cov"  /* cover statistics output file */
#define FORMAT_STR          "-Tc%s" COVER_PP_EXT

static void display(char** tmp_argv)
{
        for (; *tmp_argv != NULL; tmp_argv++) {
            printf("%s ", *tmp_argv);
        }
        printf("\n");
}

void main(int argc, char* argv[])
{
        int retcode;
        int empty_argv_pos;
        char** new_argv;
        char* cov_argv[6];
        char* basename;
        char* p;
        char* covpp_options = "";
        int handle;
        int new_stdout;
        int keep = 0;
        int analyze_only = 0;

        if (argc == 1) {
            printf("Usage: covcl [-C<cover options>] <cl arguments ...>\n"
                   "Cover options:\n"
                   "  a analyze only, do not create the object file\n"
                   "  c count how many times each cover point is executed\n"
                   "  d debug\n"
                   "  f create function list file\n"
                   "  k keep temporary files\n"
                   "  p profile\n"
                   "  s source output\n"
                   "  v verbose\n"
                   "Example:\n"
                   "  covcl -Ccv -c -W3 test.c\n"
                   );
            exit(1);
        }

        argv[0] = "cl";
        new_argv = calloc(argc + 2, sizeof(char*));
        memcpy(new_argv, argv, sizeof(char*) * argc);
        basename = strdup(argv[argc - 1]);
        assert(basename != NULL);
        p = strchr(basename, '.');
        assert(p != NULL);
        *p = '\0';
        empty_argv_pos = argc - 1;
        new_argv[empty_argv_pos + 1] = new_argv[empty_argv_pos];

        /* get options for covpp */
        if (new_argv[1][0] == '-' && new_argv[1][1] == 'C') {
            covpp_options = new_argv[1];
            covpp_options++;
            covpp_options[0] = '-';
            new_argv[1] = "";
            keep = (strchr(covpp_options, 'k') != NULL);
            analyze_only = (strchr(covpp_options, 'a') != NULL ||
                            strchr(covpp_options, 'f') != NULL);
        }

        /* execute preprocessor */
        new_argv[empty_argv_pos] = "-E";
        display(new_argv);
        p = malloc(strlen(basename) + 2 + 1);
        assert(p != NULL);
        strcat(strcpy(p, basename), ".i");
        new_stdout = dup(fileno(stdout));
        assert(new_stdout != -1);
        close(fileno(stdout));
        handle = creat(p, S_IREAD|S_IWRITE);
        assert(handle != -1);
        assert(handle == fileno(stdout));
        retcode = spawnvp(P_WAIT, new_argv[0], new_argv);
        if (retcode != 0) {
            exit(retcode);
        }
        free(p);
        retcode = dup2(new_stdout, handle);
        assert(retcode == 0);
        close(new_stdout);

        /* execute cover analyzer */
        cov_argv[0] = "covpp";
        cov_argv[1] = covpp_options;
        cov_argv[2] = malloc(strlen(basename) + 1 + 3 + 1);
        assert(cov_argv[2] != NULL);
        cov_argv[3] = malloc(strlen(basename) + 1 + 3 + 1);
        assert(cov_argv[3] != NULL);
        cov_argv[4] = malloc(strlen(basename) + 1 + 3 + 1);
        assert(cov_argv[4] != NULL);
        cov_argv[5] = NULL;
        strcat(strcpy(cov_argv[2], basename), C_PP_EXT);
        strcat(strcpy(cov_argv[3], basename), COVER_PP_EXT);
        strcat(strcpy(cov_argv[4], basename), COVER_OUTPUT_EXT);
        display(cov_argv);
        retcode = spawnvp(P_WAIT, cov_argv[0], cov_argv);
        if (retcode != 0) {
            exit(retcode);
        }
        if (!keep) {
            unlink(cov_argv[2]);
        }
        free(cov_argv[2]);
        free(cov_argv[4]);

        if (!analyze_only) {
            /* execute C compiler */
            p = malloc(strlen(FORMAT_STR) + strlen(basename) + 1);
            sprintf(p, FORMAT_STR, basename);
            new_argv[empty_argv_pos] = p;
            new_argv[empty_argv_pos + 1] = NULL;
            display(new_argv);
            retcode = spawnvp(P_WAIT, new_argv[0], new_argv);
            if (retcode != 0) {
                exit(retcode);
            }
        }
        if (!keep) {
            unlink(cov_argv[3]);
        }
        free(cov_argv[3]);
        exit(0);
}
