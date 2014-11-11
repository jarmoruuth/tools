extern int yydebug;
/**********************************************************************\
 *
 *	COVPP.C
 *
 * C-source cover analyzer preprocessor.
 *
 * Copyright (C) 1990-1993 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>
#ifdef NT
#include <direct.h>
#endif

#include "cover.h"
#include "cover_ta.h"

#define FUNCLIST_FILE "funclist.out"
#define EXITLIST_FILE "covexit.lst"

#define TYPEHASH_SIZE   211
#define EXITHASH_SIZE   31

typedef list_t* hash_t;

char _Covpp_Copyright_[] = "Copyright (C) 1990-1995 by Jarmo Ruuth";

extern int cover_c_mode;
extern int cover_all;

/* entry point to the parser */
extern int yyparse(void);
static char* make_src_fname(char *fname);
static char* get_nthline(long line_no);

static hash_t type_hash[TYPEHASH_SIZE];
static hash_t exit_hash[EXITHASH_SIZE];

static char *filename;
static char *function;
static char *listfile;
static int cover_count;
static FILE* coverout;
static int covpush;
static int skip_next_pop;

#define MAX_BUFSIZE 255

static FILE* src_in;
static unsigned long file_pos = 1;
static char src_name[30];

static int exitcode = 0;
static int verbose = 0;
static int memdebug = 0;
static int profile = 0;
static int execution_count = 0;
static int count_McCabe = 0;		/* McCabe's cyclomatic complexity */
static int McCabe;				    /* for function */
static int min_McCabe = INT_MAX;
static int max_McCabe = 0;
static int tot_McCabe = 0;
static int function_lines;			/* lines in function */
static int min_function_lines = INT_MAX;
static int max_function_lines = 0;
static int tot_function_lines = 0;
static int function_cover;			/* cover points in function */
static int min_function_cover = INT_MAX;
static int max_function_cover = 0;
static int tot_function_cover = 0;
static int nfunctions = 0;
static cover_state_t cover_state = COVER_NONE;
static long last_offset;
static long last_offset_src_in;
static long last_file_pos;
static cover_state_t last_state;
static int last_McCabe;
static int last_was_exit_if = 0;
static int function_call_expr = 0;
static FILE* funclistout;
static char curdir[255];

static int do_funclist = 0;
static int do_yyout = 1;
static int do_coverout = 1;
static int do_sourceout = 0;

int line = 1;
int column = 0;
int debug = 0;

/***********************************************************************
 * 
 *      yyout_fputs
 */
static void yyout_fputs(char* str)
{
        if (do_yyout) {
            fputs(str, yyout);
        }
}

/***********************************************************************
 * 
 *      yyout_fputc
 */
static void yyout_fputc(char c)
{
        if (do_yyout) {
	    putc(c, yyout);
        }
}

/***********************************************************************
 * 
 *      yyout_fprintf
 */
static void yyout_fprintf(char* format, ...)
{
        va_list ap;

        if (!do_yyout) {
            return;
        }

        va_start(ap, format);
        vfprintf(yyout, format, ap);
        va_end(ap);
}

/***********************************************************************
 * 
 *      coverout_fprintf
 */
static void coverout_fprintf(char* format, ...)
{
        va_list ap;

        if (!do_coverout) {
            return;
        }

        va_start(ap, format);
        vfprintf(coverout, format, ap);
        va_end(ap);
}

/***********************************************************************
 * 
 *      funclist_output
 */
static void funclist_output(char* funcname, char* filename, int lineno)
{
        if (!do_funclist) {
            return;
        }
        fprintf(funclistout, "%-32s %-15s %u\n",
            funcname, filename, lineno);
}

/**********************************************************************
 *	c_malloc
 */
static void *c_malloc(int n)
{
	void *p_malloc;
	
	p_malloc = malloc(n);
	if (p_malloc == NULL) {
		printf("\ncovpp: Out of memory in %s, exiting...\n", "c_malloc");
		exit(1);
	}
	return p_malloc;
}

/**********************************************************************
 *	c_strdup
 */
char *c_strdup(char *s)
{
	char	*ns;
	
	ns = strdup(s);
	if (ns == NULL) {
		printf("\ncovpp: Out of memory in %s, exiting...\n", "c_strdup");
		exit(1);
	}
	return ns;
}

/**********************************************************************
 *	hashstring
 */
static unsigned hashstring(char* s)
{
        int hashval;

        for (hashval = *s; *s != '\0'; s++) {
            hashval = 17 * hashval + *s;
        }
        return(hashval);
}

/**********************************************************************
 *	remove_list
 */
static void remove_list(list_t *list)
{
	list_t *tmp;
	
	while (list != NULL) {
		tmp = list->next;
		free(list);
		list = tmp;
	}
}

/**********************************************************************
 *	clear_list
 */
static void clear_list(list_t *list)
{
	list_t* lp;
	
	for (lp = list; lp != NULL; lp = lp->next) {
		free(lp->item);
        }
	remove_list(list);
}

/**********************************************************************
 *	create_list
 */
list_t *create_list(char *s)
{
	list_t	*lp;
	
	lp = c_malloc(sizeof(list_t));
	lp->next = NULL;
	lp->item = s;
	return lp;
}

/**********************************************************************
 *	add_list
 */
list_t *add_list(list_t *list, char *s)
{
	list_t	*lp;
	
	lp = create_list(s);
	lp->next = list;
	return lp;
}

/**********************************************************************
 *	is_type_name
 */
int is_type_name(char *name)
{
	list_t *lp;
        list_t *type_head;

        type_head = type_hash[hashstring(name) % TYPEHASH_SIZE];
	
	for (lp = type_head; lp != NULL; lp = lp->next) {
		if (strcmp(lp->item, name) == 0) {
			if (debug > 1) {
				printf("type name %s\n", name);
                }
			return TRUE;
		}
	}
	if (debug > 1) {
		printf("identifier %s\n", name);
        }

	return FALSE;
}

/**********************************************************************
 *	add_types
 */
static void add_types(list_t *type_names)
{
        unsigned hashpos;
	list_t *lp;
	
        assert(cover_count > -1);

	for (lp = type_names; lp != NULL; lp = lp->next) {
		if (debug > 1) {
			printf("add_types '%s'\n", lp->item);
            }
            hashpos = hashstring(lp->item) % TYPEHASH_SIZE;
		type_hash[hashpos] = add_list(type_hash[hashpos], lp->item);
	}
	remove_list(type_names);
        
        assert(cover_count > -1);

        if (!cover_c_mode) {
            cover_all = 0;
        }
}

/**********************************************************************
 *	end_declaration
 */
void end_declaration(list_t *list, int decl_spec)
{
        assert(cover_count > -1);

	switch (decl_spec) {
		case TYPEDEF:
			add_types(list);
			break;
		default:
			clear_list(list);
		  	break;
	  }
        
        assert(cover_count > -1);

}

/**********************************************************************
 *	check_exitlist
 */
static void check_exitlist(char* str)
{
	list_t *lp;
	list_t *exit_head;


        if (function == NULL || !do_coverout) {
            return;
        }
        exit_head = exit_hash[hashstring(str) % EXITHASH_SIZE];

	for (lp = exit_head; lp != NULL; lp = lp->next) {
            if (strcmp(lp->item, str) == 0) {
                fseek(coverout, last_offset, SEEK_SET);
                if (do_sourceout) {
                    fseek(src_in, last_offset_src_in, SEEK_SET);
                    file_pos = last_file_pos;
                }
                last_was_exit_if = (last_state == COVER_IF);
                cover_count--;
                function_cover--;
                if (last_was_exit_if) {
                    skip_next_pop++;
                }
                McCabe = last_McCabe;
       
                if (debug > 1) {
		        printf("exitfunction %s\n", str);
                }

			return;
		}
	}
}

/**********************************************************************
 *	cover_output
 */
void cover_output(register int token)
{
        assert(cover_count > -1);

	switch (token) {
            case COVERPOP:
                if (skip_next_pop > 0) {
                    skip_next_pop--;
                    covpush--;
                } else if (covpush > 0) {
	            coverout_fprintf("%c %d\n", (char)COVER_POP, line);
                    covpush--;
                }
			break;
		case IDENTIFIER:
			yyout_fputs(last_identifier);
                check_exitlist(last_identifier);
			break;
		case STRING_LITERAL:
			yyout_fputs(yytext);
			break;
		case CONSTANT:
			yyout_fputs(yytext);
			break;
		case SIZEOF:
			yyout_fputs("sizeof");
			break;
		case PTR_OP:
			yyout_fputs("->");
			break;
		case INC_OP:
			yyout_fputs("++");
			break;
		case DEC_OP:
			yyout_fputs("--");
			break;
		case LEFT_OP:
			yyout_fputs("<<");
			break;
		case RIGHT_OP:
			yyout_fputs(">>");
			break;
		case LE_OP:
			yyout_fputs("<=");
			break;
		case GE_OP:
			yyout_fputs(">=");
			break;
		case EQ_OP:
			yyout_fputs("==");
			break;
		case NE_OP:
			yyout_fputs("!=");
			break;
		case AND_OP:
			if (count_McCabe) {
				McCabe++;
                }
			yyout_fputs("&&");
			break;
		case OR_OP:
			if (count_McCabe) {
				McCabe++;
                }
			yyout_fputs("||");
			break;
		case MUL_ASSIGN:
			yyout_fputs("*=");
			break;
		case DIV_ASSIGN:
			yyout_fputs("/=");
			break;
		case MOD_ASSIGN:
			yyout_fputs("%=");
			break;
		case ADD_ASSIGN:
			yyout_fputs("+=");
			break;
		case SUB_ASSIGN:
			yyout_fputs("-=");
			break;
		case LEFT_ASSIGN:
			yyout_fputs("<<=");
			break;
		case RIGHT_ASSIGN:
			yyout_fputs(">>=");
			break;
		case AND_ASSIGN:
			yyout_fputs("&=");
			break;
		case XOR_ASSIGN:
			yyout_fputs("^=");
			break;
		case OR_ASSIGN:
			yyout_fputs("|=");
			break;
		case TYPE_NAME:
			yyout_fputs(yytext);
			break;
		case TYPEDEF:
			yyout_fputs("typedef");
			break;
		case EXTERN:
			yyout_fputs("extern");
			break;
		case STATIC:
			yyout_fputs("static");
			break;
		case AUTO:
			yyout_fputs("auto");
			break;
		case REGISTER:
			yyout_fputs("register");
			break;
		case CHAR:
			yyout_fputs("char");
			break;
		case SHORT:
			yyout_fputs("short");
			break;
		case INT:
			yyout_fputs("int");
			break;
		case INT8:
			yyout_fputs("__int8");
			break;
		case INT16:
			yyout_fputs("__int16");
			break;
		case INT32:
			yyout_fputs("__int32");
			break;
		case INT64:
			yyout_fputs("__int64");
			break;
		case LONG:
			yyout_fputs("long");
			break;
		case LONGLONG:
			yyout_fputs("longlong");
			break;
		case SIGNED:
			yyout_fputs("signed");
			break;
		case UNSIGNED:
			yyout_fputs("unsigned");
			break;
		case FLOAT:
			yyout_fputs("float");
			break;
		case DOUBLE:
			yyout_fputs("double");
			break;
		case CONST:
			yyout_fputs("const");
			break;
		case VOLATILE:
			yyout_fputs("volatile");
			break;
		case VOID:
			yyout_fputs("void");
			break;
		case STRUCT:
			yyout_fputs("struct");
			break;
		case UNION:
			yyout_fputs("union");
			break;
		case ENUM:
			yyout_fputs("enum");
			break;
		case ELIPSIS:
			yyout_fputs("...");
			break;
		case CASE:
			cover_state = COVER_CASE;
			yyout_fputs("case");
			break;
		case DEFAULT:
			cover_state = COVER_CASE;
			yyout_fputs("default");
			break;
		case IF:
			cover_state = COVER_IF;
			yyout_fputs("if");
                covpush++;
			break;
		case ELSE:
			cover_state = COVER_ELSE;
			yyout_fputs("else");
			break;
		case SWITCH:
	        coverout_fprintf("%c %d\n", (char)COVER_SWITCH, line);
			yyout_fputs("switch");
                covpush++;
			break;
		case WHILE:
			cover_state = COVER_WHILE;
			yyout_fputs("while");
                covpush++;
			break;
		case DO:
			cover_state = COVER_DO;
			yyout_fputs("do");
                covpush++;
			break;
		case FOR:
			cover_state = COVER_FOR;
			yyout_fputs("for");
                covpush++;
			break;
		case GOTO:
			yyout_fputs("goto");
			break;
		case CONTINUE:
			yyout_fputs("continue");
			break;
		case BREAK:
			yyout_fputs("break");
			break;
		case RETURN:
			yyout_fputs("return");
			break;
            case MODIFIER:
			yyout_fputs(yytext);
                break;
            case ASM:
			yyout_fputs(yytext);
                break;
		case ';':
			yyout_fputs(";");
			break;
		case '{':
			yyout_fputs("{");
			break;
		case '}':
			yyout_fputs("}");
			break;
		default:
			yyout_fputs(yytext);
			break;
	}
        assert(cover_count > -1);

}

/**********************************************************************
 *	count_verbose_lineno
 */
static void count_verbose_lineno(int show)
{
	static int counter = 0;
	static int linecount = 0;
	
        assert(cover_count > -1);

	if ((counter++ % 10 == 0) || show) {
		fprintf(stderr, "\rLine: %d", linecount);
        }
	linecount++;

        assert(cover_count > -1);

}

/**********************************************************************
 *	count_ppline
 */

#ifdef REMOVED

static void count_ppline(char* s)
{
	int i;
	int lineno;
	static char fname[128];
	
        assert(cover_count > -1);

	if (debug > 1) {
		printf("count_ppline '%s'\n", s);
        }
	s++;	/* skip '#' */
	while (isspace(*s)) {
		s++;
        }
	if (sscanf(s, "line %d", &lineno) == 1) {
		s = strchr(s, '"');
		if (s == NULL) {
			return;
            }
		s++;	/* skip '"' */
		for (i = 0; *s && *s != '"' && i < sizeof(fname)-1; i++, s++) {
			fname[i] = *s;
            }
		fname[i] = '\0';
		line = lineno-1; /* following newline will increment line */
		function_lines--;
		filename = fname;
		if (debug) {
			printf("file %s, line %d\n", filename, line);
            }
	}
        cover_output('#');
        assert(cover_count > -1);

}

#else /* REMOVED */

static void count_ppline(char* s)
{
	int i;
	int lineno;
	static char fname[128];
	
        assert(cover_count > -1);

	if (debug > 1) {
		printf("count_ppline '%s'\n", s);
        }
	s++;	/* skip '#' */
	while (isspace(*s)) {
		s++;
        }
	if (sscanf(s, "line %d", &lineno) == 1) {
		s = strchr(s, '"');
		if (s != NULL) {
		    s++;	/* skip '"' */
		    for (i = 0; *s && *s != '"' && i < sizeof(fname)-1; i++, s++) {
			    fname[i] = *s;
                }
		    fname[i] = '\0';
		    filename = fname;
            } 
		line = lineno-1; /* following newline will increment line */
		function_lines--;
		if (debug) {
			printf("file %s, line %d\n", filename, line);
            }
	}
        cover_output('#');

        assert(cover_count > -1);

}


#endif /* REMOVED */

/**********************************************************************
 *	count
 */
void count(void)
{
	register int   i;
	register char* str = yytext;

        assert(cover_count > -1);

	if (str[0] == '#') {
		count_ppline(str);
	} else {
		for (i = 0; str[i] != '\0'; i++) {
			if (str[i] == '\n') {
				column = 0;
				line++;
				function_lines++;
				if (verbose) {
					count_verbose_lineno(0);
                    }
                    if (i == 0 && str[1] == '\0') { /* yytext == "\n" */
                        cover_output('\n');
                    }
			} else if (str[i] == '\t') {
				column += 8 - (column % 8);
                } else {
				column++;
                }
		}
	}
        assert(cover_count > -1);

}

/**********************************************************************
 *	begin_McCabe
 */
void begin_McCabe(void)
{
        last_McCabe = McCabe;
	McCabe++;
	count_McCabe = 1;
}

/**********************************************************************
 *	end_McCabe
 */
void end_McCabe(void)
{
	count_McCabe = 0;
}

/**********************************************************************
 *	cover_point
 */
static void cover_point(void)
{
	char str[40];
        char *src_line;
	
        assert(cover_count > -1);
	assert(cover_state != COVER_NONE);
	sprintf(str, "_COVER_(%d);", cover_count);
	yyout_fputs(str);
        if (do_coverout) {
            last_offset = ftell(coverout);
            if (do_sourceout) {
                last_offset_src_in = ftell(src_in);
                last_file_pos = file_pos;
            }
        }
        last_state = cover_state;

        if (do_sourceout) {
            src_line = get_nthline(line);

            if (src_line == NULL) {
                src_line = "NULL";
            }
	    coverout_fprintf("%c %d:%d:%ld\t\t|%s\n", (char)cover_state, cover_count,
		    line, 0L,src_line);

        } else {
	    coverout_fprintf("%c %d:%d:%ld\n", (char)cover_state, cover_count,
		    line, 0L);

        }


	cover_count++;
	function_cover++;
	cover_state = COVER_NONE;
}

/**********************************************************************
 *	cover_case
 */
void cover_case(void)
{
	if (debug) {
		printf("cover_case\n");
        }
	cover_point();
	McCabe++;
        last_McCabe = McCabe;
}

/**********************************************************************
 *	cover_add_else
 *
 * After an IF-statement without ELSE, add empty else.
 */
void cover_add_else()
{
        if (last_was_exit_if) {
            last_was_exit_if = FALSE;
            return;
        }
	if (debug) {
		printf("cover_add_else\n");
        }
	cover_output(ELSE);
	cover_begin();
	cover_end();
}

/**********************************************************************
 *	cover_begin
 */
void cover_begin(void)
{
	if (debug) {
		printf("cover_begin\n");
        }
	yyout_fputc('{');
	cover_point();
}

/**********************************************************************
 *	cover_end
 */
void cover_end(void)
{
	if (debug) {
		printf("cover_end\n");
        }
	yyout_fputc('}');
}

/**********************************************************************
 *	cover_end_function
 */
static void cover_end_function(int returnp)
{
	char buf[40];

        if (!returnp) {
            cover_output(COVERPOP);
        }

	sprintf(buf, "_COVER_END_FUNCTION_(\"");
	yyout_fputs(buf);
	yyout_fputs(function);
	sprintf(buf, "\",%d);", nfunctions - 1);
	yyout_fputs(buf);
}

/**********************************************************************
 *	begin_function
 */
void begin_function(char *s)
{
	char buf[40];
        char *src_line;
	
	if (debug) {
		printf("begin_function '%s'\n", s);
        }
	if (verbose) {
		fprintf(stderr, "\rFunction '%s'\n", s);
        }
        if (do_sourceout) {
            src_line = get_nthline(line-1);
        }

	funclist_output(s, filename, line);
	
        if (do_sourceout) {
            coverout_fprintf("%c %-22.22s|%s\n", (char)COVER_FUNCTION_NAME,s,src_line);
        } else {
            coverout_fprintf("%c %.32s\n", (char)COVER_FUNCTION_NAME, s);
        }
	cover_state = COVER_FUNCTION;
	McCabe = 1;
	last_McCabe = 1;
	function_cover = 0;
	function_lines = 0;
	function = s;
	nfunctions++;
        covpush++;
	yyout_fputs("_COVER_INIT_FUNCTION_(\"");
	yyout_fputs(s);
	sprintf(buf, "\",%d);", nfunctions - 1);
	yyout_fputs(buf);
	cover_point();
	yyout_fputc('{');
}

/**********************************************************************
 *	end_function
 */
void end_function()
{

        char com;
        char *src_line;

        assert(cover_count > -1);

	if (debug) {
		printf("end_function '%s'\n", function);
        }
        if (do_sourceout) {
            src_line = get_nthline(line+1);
        }
	cover_state = COVER_NONE;
	cover_end_function(0);
	yyout_fputs("}");

        if (do_sourceout) {
            com = (char)COVER_COMMENT;
            coverout_fprintf("%c\t\t\t|\n",COVER_COMMENT);
	    coverout_fprintf("%clines %d,\t\t|\n%ccover points %d,\t|\n%cMcCabe %d\t\t|\n",
		    com,function_lines,com,function_cover,com,McCabe);
            coverout_fprintf("%c\t\t\t|\n",COVER_COMMENT);
	    coverout_fprintf("%c\t\t\t|\n",COVER_COMMENT);

        } else {
	    coverout_fprintf("%clines %d, cover points %d, McCabe %d\n",
		    (char)COVER_COMMENT,function_lines, function_cover, McCabe);
            coverout_fprintf("%c\n", (char)COVER_COMMENT);
        }
	min_function_lines = min(function_lines, min_function_lines);
	max_function_lines = max(function_lines, max_function_lines);
	tot_function_lines += function_lines;
	min_function_cover = min(function_cover, min_function_cover);
	max_function_cover = max(function_cover, max_function_cover);
	tot_function_cover += function_cover;
	min_McCabe = min(McCabe, min_McCabe);
	max_McCabe = max(McCabe, max_McCabe);
	tot_McCabe += McCabe;
	assert(function);
	free(function);
	function = NULL;

        assert(cover_count > -1);

}

/**********************************************************************
 *      set_function_call_expr
 */
void set_function_call_expr(void)
{
        function_call_expr = 1;
}

/**********************************************************************
 *	begin_return
 */
void begin_return(void)
{
	if (debug) {
		printf("begin_return\n");
        }
	yyout_fputc('{');
	cover_end_function(1);
        function_call_expr = 0;
}

/**********************************************************************
 *	end_return
 */
void end_return(void)
{
	if (debug) {
		printf("end_return\n");
        }
	yyout_fputc('}');
        if (function_call_expr && memdebug) {
		fprintf(stdout,
"%s (%d): covpp warning: return funcall(); in %s does not preserve call stack correct\n",
            filename, line, function);
        }
}

/**********************************************************************
 *	c_from_i
 */
static char* c_from_i(char* i_file)
{
        int c_len;
        char* c_file;

        c_file = c_strdup(i_file);
        c_len = strlen(c_file);
        if (c_len > 2 && c_file[c_len-2] == '.' && c_file[c_len-1] == 'i') {
            c_file[c_len-1] = 'c';
        }
        return(c_file);
}

/**********************************************************************
 *	init_file
 */
static void init_file(void)
{
        char* tmp;
        char linedir[100];

        if (memdebug) {
            yyout_fputs("#ifndef SSMEM_TRACE\n");
            yyout_fputs("#define SSMEM_TRACE 1\n");
            yyout_fputs("#endif\n");
        } else {
            if (profile) {
                yyout_fputs("#define _COVER_PROFILE_ 1\n");
            }
            if (execution_count) {
                yyout_fputs("#define _COVER_COUNT_ 1\n");
            }
        }
	yyout_fputs("#include \"covini.h\"\n");
        tmp = c_from_i(filename);
        sprintf(linedir, "#line %d \"%s\"", (int)line, tmp);

	coverout_fprintf("%c %s\n", (char)COVER_FILE_NAME, tmp);
        free(tmp);
	coverout_fprintf("%cid:line:hitcount\n", (char)COVER_COMMENT);
}

/**********************************************************************
 *	finish_file
 */
static void finish_file(void)
{
        char *src_line;

        assert(cover_count > -1);

	if (verbose) {
		count_verbose_lineno(1);
		printf("\n");
	}

	if (exitcode != 0) {
		return;
        }
        
        if (do_sourceout) {
            src_line = get_nthline(line+1);
        }

        if (!memdebug) {
	    yyout_fprintf(
	        "\n"
	        "static void _cover_init_()\n"
	        "{\n"
	        "  if (!_cover_initp_){\n"
	        "    _cover_init_file_(\"%s\",\"%s\",\"%s\",&_cover_tab_,sizeof(_cover_t),%d);\n"
	        "#ifdef _COVER_PROFILE_\n"
	        "    _cover_init_clock_(\"%s\",\"%s\",&_cover_clock_,sizeof(_cover_clock_t),%d);\n"
	        "#endif\n"
	        "   }"
	        "  _cover_initp_=1;\n"
	        "}\n"
		        ,filename, listfile, curdir, cover_count
		        ,filename, "", nfunctions);
        }
	coverout_fprintf("%c %s, Covered: 0%% (0/%d), Functions: 0%% (0/%d)\n",
			(char)COVER_MODULE, filename, cover_count, nfunctions);
	if (nfunctions) {
            coverout_fprintf("%c\n", (char)COVER_COMMENT);
		coverout_fprintf("%clines/function:        min %3d, max %3d, avg %3d\n",
			(char)COVER_COMMENT, min_function_lines,
			max_function_lines, tot_function_lines / nfunctions);
		coverout_fprintf("%ccover points/function: min %3d, max %3d, avg %3d\n",
			(char)COVER_COMMENT, min_function_cover,
			max_function_cover, tot_function_cover / nfunctions);
		coverout_fprintf("%cMcCabe/function:       min %3d, max %3d, avg %3d\n",
			(char)COVER_COMMENT, min_McCabe, max_McCabe,
			tot_McCabe / nfunctions);
	}
        assert(cover_count > -1);

}

/**********************************************************************
 *	yyerror
 */
void yyerror(char *s)
{
	exitcode = 1;
	yyout_fprintf("\n%*s\n%*s\n", column, "^", column, s);
	if (verbose) {
		fprintf(stderr, "\n");
        }
	printf("File %s %d, function %s: Error: %s, Token: '%s'\n",
		filename, line, function, s, yytext);
	fflush(stdout);
}

/**********************************************************************
 *	open_file
 */
static FILE *open_file(char *fname, char *mode)
{
	FILE *fp;
	
	fp = fopen(fname, mode);
	if (fp == NULL) {
		printf("covpp: Can't open file '%s' with mode '%s'\n", fname, mode);
		exit(1);
	}
	return fp;
}

/**********************************************************************
 *	list_names
 */
static void list_names(hash_t *hash, int hashsize)
{
        int i;
        list_t *head;

        for (i = 0; i < hashsize; i++) {
            head = hash[i];
	    for (; head != NULL; head = head->next) {
		    printf("'%s'\n", head->item);
            }
        }
}

/**********************************************************************
 *	add_exitfunc
 */
static void add_exitfunc(char* func)
{
        unsigned hashpos;

        hashpos = hashstring(func) % EXITHASH_SIZE;
	exit_hash[hashpos] = add_list(exit_hash[hashpos], func);
}

/**********************************************************************
 *	init_exitlist
 */
static void init_exitlist(void)
{
        FILE* fp;
        char name[80];

        add_exitfunc("exit");
        add_exitfunc("_exit");
        add_exitfunc("abort");
        add_exitfunc("SsAssertionFailure");
        add_exitfunc("SsRcAssertionFailure");
        add_exitfunc("SsErrorExit");
        add_exitfunc("su_rc_assertionfailure");
        add_exitfunc("su_fatal_error");
        add_exitfunc("su_rc_fatal_error");
        add_exitfunc("dbe_fatal_error");

        fp = fopen(EXITLIST_FILE, "r");
        if (fp != NULL) {
            while (fscanf(fp, "%s", name) == 1) {
                add_exitfunc(c_strdup(name));
            }
            fclose(fp);
        }
}
/**********************************************************************
 *	make_src_fname
 */

char* make_src_fname(char *fname)
{
        int i;

        for (i = 0; fname[i] != '\0'; i++) {
            if (fname[i] == '.' && fname[i+1] != '\0') {
                fname[i+1] = 'c';
                fname[i+2] = '\0';
                break;
            }
        }
        return fname;
}

/**********************************************************************
 *	get_nthline
 */
static char* get_nthline(long line_no)
{
        char*           line;
        size_t          len;
        static char     buffer[MAX_BUFSIZE];

        line = "";

        if (line_no == -1) {
            while ((line = fgets(buffer,MAX_BUFSIZE,src_in)) != NULL) {
	        coverout_fprintf("%c\t\t\t|%s",COVER_COMMENT,line);
            }
	    coverout_fprintf("%c\t\t\t|\n",COVER_COMMENT);
	    coverout_fprintf("%c\t\t\t|\n",COVER_COMMENT);
	    coverout_fprintf("%c\t\t\t|\n",COVER_COMMENT);
            file_pos = line_no;

        } else if (file_pos <= line_no) {
            for (; file_pos <= line_no; file_pos++) {
                line = fgets(buffer,MAX_BUFSIZE,src_in);
                if (line == NULL) {
                    file_pos = 0;
                    line = buffer;
                    break;
                } else {
                    len = strlen(line);
                    if (len > 0) {
                        line[len-1] = '\0';
                    }
                    if (file_pos < line_no) {
	                coverout_fprintf("%c\t\t\t|%s\n",COVER_COMMENT,line);
                    }
                }
            }
        }

        return line;
}

/**********************************************************************
 *	get_curdir
 */
static void get_curdir(void)
{
        int i;

        getcwd(curdir, sizeof(curdir)-1);

        for (i = 0; curdir[i] != '\0'; i++) {
            if (curdir[i] == '\\') {
                curdir[i] = '/';
            }
        }
}

/**********************************************************************
 *	main
 */
void main(int argc, char *argv[])
{
	int   i;
	char  *s,*src_fname;
	
	for (i = 1; argv[i] && argv[i][0] == '-'; i++) {
		s = argv[i];
		for (s++; *s; s++) {
			switch (*s) {
                    case 'c':
                        execution_count = 1;
                        break;
				case 'd':
					debug++;
#ifdef YYDEBUG
                        yydebug = 1;
#endif
					break;
                    case 'f':
                        do_funclist = 1;
                        do_yyout = 0;
                        do_coverout = 0;
                        break;
                    case 'p':
                        profile = 1;
                        break;
                    case 'm':
                        memdebug = 1;
                        do_coverout = 0;
                        break;
				case 'v':
					verbose = 1;
					break;
                    case 'a':
                    case 'k':
                        /* ignore covcl options */
                        break;
                    case 's':
                        do_sourceout = 1;
                        break;
				default:
					printf("covpp: Ignoring unknown option '%c'\n", *s);
					break;
			}
		}
	}
		
	if (argc - i != 3) {
		printf("Usage: covpp [options] <infile> <outfile> <listfile>\n"
		       "Options: -c count how many times each cover point is executed\n"
		       "         -d debug\n"
		       "         -f generate function list into file %s\n"
		       "         -p profile\n"
		       "         -v verbose\n"
                   "         -s include source code to cover file\n"
                   "         -m memory debug (no cov. analysis)",
                   FUNCLIST_FILE);
		exit(1);
	}
	filename = argv[i];
	yyin = open_file(argv[i++], "r");
        if (do_yyout) {
	    yyout = open_file(argv[i++], "w");
        } else {
            yyout = stdout;
        }
	listfile = argv[i];
        
        get_curdir();

        if (do_coverout) {
	    coverout = open_file(argv[i++], "w");
            
            if (do_sourceout) {
                src_fname = make_src_fname(filename);
                strncpy(src_name,src_fname,20);
                src_in = open_file(src_fname,"r");

            }
        }
        if (do_funclist) {
	    funclistout = open_file(FUNCLIST_FILE, "a+");
        }
	init_exitlist();
        init_file();

	yyparse();	/* the real action is here */

	finish_file();

	if (debug) {
		printf("Typedefs:\n");
		list_names(type_hash, TYPEHASH_SIZE);
	}
	fclose(yyin);
        if (do_yyout) {
	    fclose(yyout);
        }
        if (do_coverout) {
	    fclose(coverout);
            if (do_sourceout) {
                fclose(src_in);
            }
        }
        if (do_funclist) {
	    fclose(funclistout);
        }
	exit(exitcode);
}
