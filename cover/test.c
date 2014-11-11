/* This program counts the number of lines in files
   given in the command line. If no files are given,
   stdin is used.
*/

#include <stdio.h>

static long count_file(FILE* f)
{
        int  ch;
        long count = 0;

        while ((ch = getc(f)) != EOF) {
            if (ch == '\n') {
                count++;
            }
        }
        return(count);
}

static long count_filename(char* fname)
{
        FILE* f;
        long  count;

        f = fopen(fname, "rt");

        if (f == NULL) {
            printf("Unable to open file %s\n", fname);
            exit(0);
        }

        count = count_file(f);

        if (ferror(f)) {
            printf("Error readinf file %s\n", fname);
            fclose(f);
            return(-1L);
        }

        fclose(f);

        return(count);
}

void main(int argc, char* argv[])
{
        int total = 0;
        long count;
        long total_count = 0;

        if (argc == 1) {
            count = count_file(stdin);
            printf("%ld\n", count);
        } else {
            for (argv++; *argv != NULL && (*argv)[0] == '-'; argv++) {
                int i;
                for (i = 1; (*argv)[i] != '\0'; i++) {
                    switch((*argv)[i]) {
                        case 't':
                            total = 1;
                            break;
                        default:
                        printf("Usage: test [-t] <file>...\n");
                            exit(1);
                    }
                }
            }
            for (; *argv != NULL; argv++) {
                count = count_filename(*argv);
                if (!total) {
                    printf("%s:%ld\n", *argv, count);
                }
                total_count += count;
            }
            printf("%ld\n", total_count);
        }
}
