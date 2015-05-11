#include <stdio.h>

#include "diruti.h"

void main(int argc, char* argv[])
{
        u_dirinfo_t dirinfo;
        u_dirord_t ord = ORD_NAME_ASC;

        argc;

        switch (argv[1][0]) {
            case 'n':
                ord = ORD_NAME_ASC;
                break;
            case 'N':
                ord = ORD_NAME_DESC;
                break;
            case 't':
                ord = ORD_TIME_ASC;
                break;
            case 'T':
                ord = ORD_TIME_DESC;
                break;
        }

        if (u_findfirst_ord("*.*", &dirinfo, 0, ord) == 0) {
            do {
                printf("%s\n", dirinfo.di_name);
            } while (u_findnext_ord(&dirinfo) == 0);
        } else {
            printf("Not found.\n");
        }
}
