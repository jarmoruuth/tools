/**********************************************************************\
* C-preprocessor-postprocessor for borland cpp
\**********************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>

enum retcodes {
        OK = 0,
        FAILURE
};

int postprocess(FILE *fp_in, FILE *fp_out)
{
        char fname[512];
        char fname_prev[sizeof(fname)];
        char linenobuf[20];
        int c;
        int prev_c;

        fname_prev[0] = fname[0] = '\0';
        for (;;) {
            int i;

            for (i = 0; i < sizeof(fname)-1; i++) {
                c = getc(fp_in);
                switch (c) {
                    case '\n':
                        if (i == 0) {
                            putc(c, fp_out);
                            i--;
                            continue;
                        }
                        /* intentinally fall to next case */
                    case EOF:
                        if (i == 0) {
                            return (OK);
                        } else {
                            return (FAILURE);
                        }
                    case ' ':
                    case '\t':
                        fname[i] = '\0';
                        break;
                    default:
                        fname[i] = (char)c;
                        continue;
                }
                break;
            }
            if (i >= sizeof(fname)-1) {
                return (FAILURE);
            }
            for (i = 0; i < sizeof(linenobuf)-1; i++) {
                c = getc(fp_in);
                switch (c) {
                    case ':':
                        linenobuf[i] = '\0';
                        break;
                    case ' ':
                    case '\t':
                        i--;
                        continue;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        linenobuf[i] = (char)c;
                        continue;
                    case '\n':
                    case EOF:
                    default:
                        return (FAILURE);
                }
                break;
            }
            if (i >= sizeof(linenobuf)-1) {
                return (FAILURE);
            }
            if (strcmp(fname, fname_prev) != 0) {
                fprintf(fp_out, "#line %s \"%s\"\n", linenobuf, fname);
                strcpy(fname_prev, fname);
            }
            for (;;) {
                prev_c = c;
                c = getc(fp_in);
                switch (c) {
                    case EOF:
                        return (OK);
                    case '\n':
                        if (prev_c != '\\') {
                            putc(c, fp_out);
                            break;
                        }
                        /* fall into next case */
                    default:
                        putc(c, fp_out);
                        continue;
                }
                break;
            }
        }
}

int main(int argc, char **argv)
{
        FILE *fp_in, *fp_out;

        assert(argc > 2);

        fp_in = fopen(argv[1], "rt");
        fp_out = fopen(argv[2], "wt");

        assert(fp_in != NULL);
        assert(fp_out != NULL);
        if (postprocess(fp_in, fp_out) == OK) {
            return (0);
        }
        return (1);
}

