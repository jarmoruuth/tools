/**********************************************************************\
 *
 *	COVER.H
 *
 * Copyright (C) 1990-1991 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#ifdef NT
#include <malloc.h>
#endif

#define FALSE	0
#define TRUE	1
#define MAXLINE	10240

typedef enum {
	COVER_NONE	    = '\0',
	COVER_IF	    = 'i',
	COVER_ELSE	    = 'e',
	COVER_FOR	    = 'f',
	COVER_WHILE	    = 'w',
	COVER_DO	    = 'd',
	COVER_SWITCH	= 's',
	COVER_CASE	    = 'c',
	COVER_FUNCTION	= 'F',
	COVER_POP       = 'p',
	COVER_MODULE	= 'M',
        COVER_FILE_NAME = 'S',
        COVER_FUNCTION_NAME = 'N',
	COVER_COMMENT	= '-'	/* comment line */
} cover_state_t;

typedef struct list {
	struct list *	next;
	char*		item;
} list_t;

/* last identifier seen by lexer */
extern char last_identifier[];

/* variables from lexer */
extern FILE *yyin, *yyout;
extern char* yytext;

extern int debug;

void yyerror(char *s);

int is_type_name(char *name);
void begin_function(char *name);
void end_function(void);
void set_function_call_expr(void);
void end_declaration(list_t *list, int decl_spec);
void begin_return(void);
void end_return(void);
list_t *create_list(char *name);
list_t *add_list(list_t *list, char *name);
char *c_strdup(char *s);
void cover_case(void);
void cover_begin(void);
void cover_end(void);
void cover_add_else(void);
void cover_output(int token);
void begin_McCabe(void);
void end_McCabe(void);

