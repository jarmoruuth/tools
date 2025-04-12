/*************************************************************************\
**  source       * tee.c
**  directory    * \jarmo\utils
**  description  * Copies stdin to stdout and stderr.
**               * 
**  author       * SOLID / jarmo
**  date         * 1993-08-29
**               * 
**               * (C) Copyright Solid Information Technology Ltd 1993
**************************************************************************
** TLIB version ** '%v'
** This source was extracted from library version = ''
**************************************************************************
** TLIB history **
** TLIB history **
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define VMS_MAXLEN  250

static int debug = 0;

static void output(char* p)
{
        if (*p) {
            fputs(p, stderr);
            fputs(p, stdout);
        }
}

static void output_nl(char* p)
{
        if (*p) {
            fprintf(stdout, "%s\n", p);
            fprintf(stderr, "%s\n", p);
        }
}

static int strnistr(register char* str, char* pat, int n)
{
        for (; *str != '\0'; str++) {
            if (toupper(*str) == *pat) {
                if (strnicmp(str, pat, n) == 0 && str[n] != '.') {
                    return(1);
                }
            }
        }
        return(0);
}

static void remove_trailing_spaces(char* s)
{
        int len;

        for (;;) {
            len = strlen(s);
            if (isspace(s[len - 1])) {
                s[len - 1] = '\0';
            } else {
                break;
            }
        }
}

static char* vms_cc(char* s)
{
        int i;
        int j;

        for (i = 0; s[i] != '\0'; i++) {
            switch (s[i]) {
                case '-':
                    switch (s[i+1]) {
                        case 'I':
                            for (j = i; s[j] != '\0' && !isspace(s[j]); j++) {
                                s[j] = ' ';
                            }
                            break;
                        case 'c':
                            s[i] = ' ';
                            s[i+1] = ' ';
                            break;
                        case 'D':
                            for (j = i; s[j] != '\0' && !isspace(s[j]); j++) {
                                s[j] = ' ';
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
        if (debug) {
            fprintf(stderr, "vms_cc1:'%s'\n", s);
        }
        for (i = j = 0; s[i] != '\0'; ) {
            if (isspace(s[i])) {
                s[j++] = ' ';
                while (isspace(s[i])) {
                    i++;
                }
            } else {
                s[j++] = s[i++];
            }
        }
        s[j] = '\0';
        if (debug) {
            fprintf(stderr, "vms_cc2:'%s'\n", s);
        }
        return(s);
}

static void vms_lib_split(char* s)
{
        int i;
        char* tmps;
        char libname[255];
        int last = 0;

        tmps = s;

        /* skip $library */
        s += 8;
        while (isspace(*s)) {
            s++;
        }

        /* skip options */
        while (*s == '/') {
            while (*s && !isspace(*s)) {
                s++;
            }
            while (isspace(*s)) {
                s++;
            }
        }

        /* copy library name */
        i = 0;
        while (*s && !isspace(*s)) {
            libname[i++] = *s++;
        }
        libname[i] = '\0';

        s = tmps;
        while (*tmps && *tmps != ',') {
            tmps++;
        }
        if (*tmps) {
            *tmps++ = '\0';
            output_nl(s);
        }
        while (*tmps) {
            int nbytes;
            output("$library/INSERT ");
            output(libname);
            output(" ");
            nbytes = 0;
            last = 0;
            while (*tmps && !last) {
                s = tmps;
                while (*tmps && *tmps != ',') {
                    tmps++;
                }
                if (*tmps) {
                    *tmps++ = '\0';
                } else {
                    last = 1;
                }
                output(s);
                nbytes += strlen(s) + 1;
                if (nbytes > 100) {
                    last = 1;
                }
                if (!last) {
                    output(",");
                }
            }
            output("\n");
        }
}

static char* vms_lib(char* s)
{
        char* org_s = s;

        /* skip $library */
        s += 8;
        while (isspace(*s)) {
            s++;
        }

        /* skip options */
        while (*s == '/') {
            while (*s && !isspace(*s)) {
                s++;
            }
            while (isspace(*s)) {
                s++;
            }
        }

        /* skip library name */
        while (*s && !isspace(*s)) {
            s++;
        }
        while (isspace(*s)) {
            s++;
        }

        /* replace first space after object file with comma  */
        while (*s) {
            /* skip object name */
            while (*s && !isspace(*s)) {
                s++;
            }
            if (isspace(*s)) {
                *s++ = ',';
            }
            while (isspace(*s)) {
                s++;
            }
        }
        if (strlen(org_s) > VMS_MAXLEN) {
            vms_lib_split(org_s);
            *org_s = '\0';
        }
        return(org_s);
}

static void vms_link_split(char* s)
{
        char fname[50];
        char* tmps;
        static int ctr = 0;
        FILE* fp;
        int last = 0;

        tmps = s;
        while (*tmps && *tmps != ',') {
            tmps++;
        }
        if (*tmps) {
            *tmps++ = '\0';
            output(s);
            sprintf(fname, "LINK%d.OPT", ctr++);
            output(",");
            output(fname);
            output("/OPTIONS\n");
            fp = fopen(fname, "wt");
            assert(fp != NULL);
            while (*tmps) {
                s = tmps;
                while (*tmps && *tmps != ',') {
                    tmps++;
                }
                if (*tmps) {
                    *tmps++ = '\0';
                } else {
                    last = 1;
                }
                fprintf(fp, "%s", s);
                if (!last) {
                    fprintf(fp, ",-");
                }
                fprintf(fp, "\n");
            }
            fclose(fp);
        }
}

static char* vms_link(char* s)
{
        char* org_s = s;
        char* org_ns;
        char* ns;
        char* options;
        char* objs;
        char* exe;
        char* libs;
        static char* soliddir1 = "\\solid\\vms\\";
        static char* soliddir2 = "\\solid\\alpha\\";
        static char* solpro1 = "\\solpro\\vms\\";
        static char* solpro2 = "\\solpro\\alpha\\";
        static char* dotdot = "..\\";

        /* copy options */
        options = ns = strdup(s);
        assert(options != NULL);
        /* skip $link */
        s += 5;
        strcpy(ns, "$link");
        ns += strlen(ns);
        while (isspace(*s)) {
            *ns++ = *s++;
        }
        /* skip options */
        while (*s == '/') {
            while (*s && !isspace(*s)) {
                *ns++ = *s++;
            }
            while (isspace(*s)) {
                *ns++ = *s++;
            }
        }
        *ns = '\0';
        remove_trailing_spaces(options);
        if (debug) {
            fprintf(stderr, "options:'%s'\n", options);
        }

        /* copy object files */
        while (isspace(*s)) {
            s++;
        }
        objs = ns = strdup(s);
        assert(objs != NULL);
        while (*s && *s != ',') {
            *ns++ = *s++;
        }
        *ns = '\0';
        remove_trailing_spaces(objs);
        if (debug) {
            fprintf(stderr, "objs:'%s'\n", objs);
        }
        if (*s) {
            s++;
        }

        /* copy executable name */
        while (isspace(*s)) {
            s++;
        }
        exe = ns = strdup(s);
        assert(exe != NULL);
        while (*s && *s != ',') {
            *ns++ = *s++;
        }
        *ns = '\0';
        remove_trailing_spaces(exe);
        if (debug) {
            fprintf(stderr, "exe:'%s'\n", exe);
        }
        if (*s) {
            s++;
        }

        /* skip map file */
        while (*s && *s != ',') {
            s++;
        }
        if (*s) {
            s++;
        }

        /* copy libraries */
        while (isspace(*s)) {
            s++;
        }
        libs = ns = strdup(s);
        assert(libs != NULL);
        while (*s && *s != ',') {
            *ns++ = *s++;
        }
        *ns = '\0';
        remove_trailing_spaces(libs);
        if (debug) {
            fprintf(stderr, "libs:'%s'\n", libs);
        }

        /* copy executable option */
        strcat(options, "/EXECUTABLE=");
        strcat(options, exe);

        /* allocate final new command string */
        org_ns = ns = malloc(10 * 1024);
        assert(org_s != NULL);
        strcpy(ns, options);
        strcat(ns, " ");
        ns += strlen(ns);
        if (debug) {
            fprintf(stderr, "added options:'%s'\n", org_ns);
        }

        /* append object files */
        s = objs;
        /* replace all slash characters with backslash characters */
        while (*s) {
            if (*s == '/') {
                *s = '\\';
            }
            s++;
        }
        s = objs;
        while (isspace(*s)) {
            s++;
        }
        while (*s) {
            if (isalpha(s[0]) && s[1] == ':') {
                s += 2;
                if (strncmp(s, solpro1, strlen(solpro1)) == 0) {
                    strcpy(ns, "solprodir:");
                    ns += strlen(ns);
                    s += strlen(solpro1);
                } else if (strncmp(s, solpro2, strlen(solpro2)) == 0) {
                    strcpy(ns, "solprodir:");
                    ns += strlen(ns);
                    s += strlen(solpro2);
                } else if (strncmp(s, soliddir1, strlen(soliddir1)) == 0) {
                    strcpy(ns, "soliddir:");
                    ns += strlen(ns);
                    s += strlen(soliddir1);
                } else if (strncmp(s, soliddir2, strlen(soliddir2)) == 0) {
                    strcpy(ns, "soliddir:");
                    ns += strlen(ns);
                    s += strlen(soliddir2);
                } else {
                    /* Restore old position. */
                    s -= 2;
                }
            } else if (strncmp(s, dotdot, strlen(dotdot)) == 0) {
                strcpy(ns, "[-]");
                ns += strlen(ns);
                s += strlen(dotdot);
            }
            while (*s && !isspace(*s)) {
                *ns++ = *s++;
            }
            if (*s) {
                *ns++ = ',';
            }
            while (isspace(*s)) {
                s++;
            }
        }
        *ns++ = ',';
        *ns = '\0';
        if (debug) {
            fprintf(stderr, "added objs:'%s'\n", org_ns);
        }

        /* append library files */
        s = libs;
        /* replace all slash characters with backslash characters */
        while (*s) {
            if (*s == '/') {
                *s = '\\';
            }
            s++;
        }
        s = libs;
        while (isspace(*s)) {
            s++;
        }
        while (*s) {
            if (isalpha(s[0]) && s[1] == ':') {
                s += 2;
                if (strncmp(s, solpro1, strlen(solpro1)) == 0) {
                    strcpy(ns, "solprodir:");
                    ns += strlen(ns);
                    s += strlen(solpro1);
                } else if (strncmp(s, solpro2, strlen(solpro2)) == 0) {
                    strcpy(ns, "solprodir:");
                    ns += strlen(ns);
                    s += strlen(solpro2);
                } else if (strncmp(s, soliddir1, strlen(soliddir1)) == 0) {
                    strcpy(ns, "soliddir:");
                    ns += strlen(ns);
                    s += strlen(soliddir1);
                } else if (strncmp(s, soliddir2, strlen(soliddir2)) == 0) {
                    strcpy(ns, "soliddir:");
                    ns += strlen(ns);
                    s += strlen(soliddir2);
                } else {
                    /* Restore old position. */
                    s -= 2;
                }
            } else if (strncmp(s, dotdot, strlen(dotdot)) == 0) {
                strcpy(ns, "[-]");
                ns += strlen(ns);
                s += strlen(dotdot);
            }
            while (*s && !isspace(*s)) {
                *ns++ = *s++;
            }
            strcpy(ns, "/LIBRARY");
            ns += strlen(ns);
            if (*s) {
                *ns++ = ',';
            }
            while (isspace(*s)) {
                s++;
            }
        }
        *ns = '\0';
        free(org_s);
        free(options);
        free(objs);
        free(libs);
        if (strlen(org_ns) > VMS_MAXLEN) {
            vms_link_split(org_ns);
            *org_ns = '\0';
        }
        return(org_ns);
}

static char* translate_vms(char* str)
{
        static char* s = NULL;

        if (s != NULL) {
            free(s);
        }
        s = malloc(strlen(str) + 2 + 1 + 1);
        assert(s != NULL);
        strcpy(s, str);

        remove_trailing_spaces(s);

        if (strncmp(s, "$cc", 3) == 0) {
            s = vms_cc(s);
        } else if (strncmp(s, "$del", 4) == 0) {
            strcat(s, ";*");
        } else if (strncmp(s, "$library", 8) == 0) {
            s = vms_lib(s);
        } else if (strncmp(s, "$link", 5) == 0) {
            s = vms_link(s);
        }
        if (*s) {
            strcat(s, "\n");
        }
        return(s);
}

static char* teej_stristr(char* src, char* pat)
{
        char c;
        char* pp;
        char* sp;

        if (src == NULL || pat == NULL) {
            return (NULL);
        }
        pp = (char*)pat;
        sp = (char*)src;
        c = (char)toupper(*pp);
        if (c == '\0') {
            return (src);
        }
        for ( ;*sp != '\0'; sp++) {
            if (c == toupper(*sp)) {
                char* pp1;
                char* sp1;
                for  (pp1 = pp + 1, sp1 = sp + 1; ; pp1++, sp1++) {
                    if (*pp1 == '\0') {
                        return ((char*)sp);
                    }
                    if (toupper(*pp1) != toupper(*sp1)) {
                        break;
                    }
                }
            }
        }
        return (NULL);
}

int main(int argc, char* argv[])
{
        int exitcode;
        int vms = 0;
        char* p;
        static char buf[6*1024];

        if (argc > 3) {
            printf("Usage: teej [-VMS] \n");
            printf("Copies stdout to stdin and strerr.\n");
            return(3);
        }

        argv++;

        if (*argv != NULL && strcmp(*argv, "-d") == 0) {
            argv++;
            debug = 1;
        }
        if (*argv != NULL && strcmp(*argv, "-VMS") == 0) {
            argv++;
            vms = 1;
        }

        exitcode = 0;
        buf[sizeof(buf)-1] = '\0';

        while (fgets(buf, sizeof(buf)-1, stdin) != NULL) {
            char* tmp;
            if (strstr(buf, "Warning! W203:") != NULL) {
                continue;
            }
            if (strstr(buf, "warning C4018:") != NULL) {
                continue;
            }
            if (vms) {
                if (debug) {
                    fprintf(stderr, "line:'%s'\n", buf);
                }
                p = translate_vms(buf);
            } else {
                p = buf;
            }
            tmp = p;
            for (;;) {
                tmp = teej_stristr(tmp, "ERROR");
                if (tmp == NULL) {
                    break;
                }
                if (tmp == p) {
                    exitcode = 1;
                    break;
                }
                if (!isdigit(tmp[-1]) && tmp[-1] != 'y' && tmp[-1] != '_') {
                    exitcode = 1;
                    break;
                }
                tmp += 5;
            }

            output(p);
        }
        return(exitcode);
}
