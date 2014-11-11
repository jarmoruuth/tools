D			[0-9]
O			[0-7]
L			[a-zA-Z_]
H			[a-fA-F0-9]
E			[Ee][+-]?{D}+
FS			(f|F|l|L)
IS			(u|U|l|L)*

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cover.h"
#include "cover_ta.h"

#define FLEX

void count(void);
static void count_output(void);

int  cover_c_mode = 1;
int  cover_all = 0;
char last_identifier[MAXLINE];
char last_typename[MAXLINE];

%}

%%
"/*"			{ comment(); }

"auto"			{ count_output(); if (cover_all) return(AUTO); }
"break"			{ count_output(); if (cover_all) return(BREAK); }
"case"			{ count_output(); if (cover_all) return(CASE); }
"char"			{ count_output(); if (cover_all) return(CHAR); }
"const"			{ count_output(); if (cover_all) return(CONST); }
"continue"		{ count_output(); if (cover_all) return(CONTINUE); }
"default"		{ count_output(); if (cover_all) return(DEFAULT); }
"do"			{ count_output(); if (cover_all) return(DO); }
"double"		{ count_output(); if (cover_all) return(DOUBLE); }
"else"			{ count_output(); if (cover_all) return(ELSE); }
"enum"			{ count_output(); if (cover_all) return(ENUM); }
"extern"		{ count_output(); if (cover_all) return(EXTERN); }
"float"			{ count_output(); if (cover_all) return(FLOAT); }
"for"			{ count_output(); if (cover_all) return(FOR); }
"goto"			{ count_output(); if (cover_all) return(GOTO); }
"if"			{ count_output(); if (cover_all) return(IF); }
"int"			{ count_output(); if (cover_all) return(INT); }
"__int8"		{ count_output(); if (cover_all) return(INT8); }
"__int16"		{ count_output(); if (cover_all) return(INT16); }
"__int32"		{ count_output(); if (cover_all) return(INT32); }
"__int64"		{ count_output(); if (cover_all) return(INT64); }
"long"			{ count_output(); if (cover_all) return(LONG); }
"longlong"		{ count_output(); if (cover_all) return(LONGLONG); }
"register"		{ count_output(); if (cover_all) return(REGISTER); }
"return"		{ count_output(); if (cover_all) return(RETURN); }
"short"			{ count_output(); if (cover_all) return(SHORT); }
"signed"		{ count_output(); if (cover_all) return(SIGNED); }
"sizeof"		{ count_output(); if (cover_all) return(SIZEOF); }
"static"		{ count_output(); if (cover_all) return(STATIC); }
"struct"		{ count_output(); if (cover_all) return(STRUCT); }
"switch"		{ count_output(); if (cover_all) return(SWITCH); }
"typedef"		{ count(); cover_all = 1; return(TYPEDEF); }
"union"			{ count_output(); if (cover_all) return(UNION); }
"unsigned"		{ count_output(); if (cover_all) return(UNSIGNED); }
"void"			{ count_output(); if (cover_all) return(VOID); }
"volatile"		{ count_output(); if (cover_all) return(VOLATILE); }
"while"			{ count_output(); if (cover_all) return(WHILE); }

"cdecl"			{ count_output(); if (cover_all) return(MODIFIER); }
"pascal"		{ count_output(); if (cover_all) return(MODIFIER); }
"interrupt"		{ count_output(); if (cover_all) return(MODIFIER); }
"far"			{ count_output(); if (cover_all) return(MODIFIER); }
"near"			{ count_output(); if (cover_all) return(MODIFIER); }
"huge"			{ count_output(); if (cover_all) return(MODIFIER); }
"_based"		{ count_output(); if (cover_all) return(MODIFIER); }
"__based"		{ count_output(); if (cover_all) return(MODIFIER); }
"_cdecl"		{ count_output(); if (cover_all) return(MODIFIER); }
"__cdecl"		{ count_output(); if (cover_all) return(MODIFIER); }
"_export"		{ count_output(); if (cover_all) return(MODIFIER); }
"_Export"		{ count_output(); if (cover_all) return(MODIFIER); }
"__export"		{ count_output(); if (cover_all) return(MODIFIER); }
"_far"			{ count_output(); if (cover_all) return(MODIFIER); }
"__far"			{ count_output(); if (cover_all) return(MODIFIER); }
"_fastcall"		{ count_output(); if (cover_all) return(MODIFIER); }
"__fastcall"		{ count_output(); if (cover_all) return(MODIFIER); }
"_fortran"		{ count_output(); if (cover_all) return(MODIFIER); }
"_huge"			{ count_output(); if (cover_all) return(MODIFIER); }
"_interrupt"		{ count_output(); if (cover_all) return(MODIFIER); }
"__interrupt"		{ count_output(); if (cover_all) return(MODIFIER); }
"__inline"		{ count_output(); if (cover_all) return(MODIFIER); }
"_loadds"		{ count_output(); if (cover_all) return(MODIFIER); }
"_near"		        { count_output(); if (cover_all) return(MODIFIER); }
"__near"                { count_output(); if (cover_all) return(MODIFIER); }
"_pascal"		{ count_output(); if (cover_all) return(MODIFIER); }
"_Pascal"		{ count_output(); if (cover_all) return(MODIFIER); }
"__pascal"		{ count_output(); if (cover_all) return(MODIFIER); }
"_saveregs"		{ count_output(); if (cover_all) return(MODIFIER); }
"_segname"		{ count_output(); if (cover_all) return(MODIFIER); }
"_self"			{ count_output(); if (cover_all) return(MODIFIER); }
"__fastcall"     	{ count_output(); if (cover_all) return(MODIFIER); }
"_far16"  		{ count_output(); if (cover_all) return(MODIFIER); }
"_Far16"  		{ count_output(); if (cover_all) return(MODIFIER); }
"__far16"  		{ count_output(); if (cover_all) return(MODIFIER); }
"_stdcall"  		{ count_output(); if (cover_all) return(MODIFIER); }
"__stdcall"  		{ count_output(); if (cover_all) return(MODIFIER); }
"_syscall"  		{ count_output(); if (cover_all) return(MODIFIER); }
"__syscall"  		{ count_output(); if (cover_all) return(MODIFIER); }
"_Seg16"                { count_output(); if (cover_all) return(MODIFIER); }
"__w64"                 { count_output(); if (cover_all) return(MODIFIER); }

"_Optlink"  		{ count_output(); if (cover_all) return(MODIFIER); }
"_Builtin"  		{ count_output(); if (cover_all) return(MODIFIER); }
"_System"  		{ count_output(); if (cover_all) return(MODIFIER); }

"thread"  		{ count_output(); if (cover_all) return(MODIFIER); }
"naked"  		{ count_output(); if (cover_all) return(MODIFIER); }
"dllimport"  		{ count_output(); if (cover_all) return(MODIFIER); }
"dllexport"  		{ count_output(); if (cover_all) return(MODIFIER); }

"__declspec"[ \r\t\n]*"(".*")"  { count_output(); if (cover_all) return(MODIFIER); }
"__asm"[ \r\t\n]*"{"[^}]*"}"    { count_output(); if (cover_all) return(ASM); }
"_asm"[ \r\t\n]*"{"[^}]*"}"     { count_output(); if (cover_all) return(ASM); }

"L("\"(\\(.|\n)|[^\\"])*\"")" { count_output(); if (cover_all) return(STRING_LITERAL); }
"L"\"(\\(.|\n)|[^\\"])*\"     { count_output(); if (cover_all) return(STRING_LITERAL); }

"#".*			{ count(); 
                          if (strstr(yytext, ".c") != NULL) {
                              cover_c_mode = 1;
                              cover_all = 1;
                          } else if (strstr(yytext, ".h") != NULL) {
                              cover_c_mode = 0;
                          }
                        }

{L}({L}|{D})*		{ count_output(); 
                          if (is_type_name(yytext)) {
                              strcpy(last_typename, yytext);
			      if (cover_all) {
                                  return(TYPE_NAME);
                              }
                          } else {
                              strcpy(last_identifier, yytext);
			      if (cover_all) {
                                  return(IDENTIFIER);
                              }
                          }
			}

0[xX]{H}+{IS}?		{ count_output(); if (cover_all) return(CONSTANT); }
0{O}*{IS}?		{ count_output(); if (cover_all) return(CONSTANT); }
{D}+{IS}?		{ count_output(); if (cover_all) return(CONSTANT); }
'(\\.|[^\\'])+'		{ count_output(); if (cover_all) return(CONSTANT); }
"L"'(\\.|[^\\'])+'	{ count_output(); if (cover_all) return(CONSTANT); }

{D}+{E}{FS}?		{ count_output(); if (cover_all) return(CONSTANT); }
{D}*"."{D}+({E})?{FS}?	{ count_output(); if (cover_all) return(CONSTANT); }
{D}+"."({E})?{FS}?	{ count_output(); if (cover_all) return(CONSTANT); }

\"(\\(.|\n)|[^\\"])*\"	{ count_output(); if (cover_all) return(STRING_LITERAL); }

"..."			{ count_output(); if (cover_all) return(ELIPSIS); }
">>="			{ count_output(); if (cover_all) return(RIGHT_ASSIGN); }
"<<="			{ count_output(); if (cover_all) return(LEFT_ASSIGN); }
"+="			{ count_output(); if (cover_all) return(ADD_ASSIGN); }
"-="			{ count_output(); if (cover_all) return(SUB_ASSIGN); }
"*="			{ count_output(); if (cover_all) return(MUL_ASSIGN); }
"/="			{ count_output(); if (cover_all) return(DIV_ASSIGN); }
"%="			{ count_output(); if (cover_all) return(MOD_ASSIGN); }
"&="			{ count_output(); if (cover_all) return(AND_ASSIGN); }
"^="			{ count_output(); if (cover_all) return(XOR_ASSIGN); }
"|="			{ count_output(); if (cover_all) return(OR_ASSIGN); }
">>"			{ count_output(); if (cover_all) return(RIGHT_OP); }
"<<"			{ count_output(); if (cover_all) return(LEFT_OP); }
"++"			{ count_output(); if (cover_all) return(INC_OP); }
"--"			{ count_output(); if (cover_all) return(DEC_OP); }
"->"			{ count_output(); if (cover_all) return(PTR_OP); }
"&&"			{ count_output(); if (cover_all) return(AND_OP); }
"||"			{ count_output(); if (cover_all) return(OR_OP); }
"<="			{ count_output(); if (cover_all) return(LE_OP); }
">="			{ count_output(); if (cover_all) return(GE_OP); }
"=="			{ count_output(); if (cover_all) return(EQ_OP); }
"!="			{ count_output(); if (cover_all) return(NE_OP); }
";"			{ count_output(); if (cover_all) return(';'); }
"{"			{ count_output(); if (cover_all) return('{'); }
"}"			{ count_output(); if (cover_all) return('}'); }
","			{ count_output(); if (cover_all) return(','); }
":"			{ count_output(); if (cover_all) return(':'); }
"="			{ count_output(); if (cover_all) return('='); }
"("			{ count_output(); if (cover_all) return('('); }
")"			{ count_output(); if (cover_all) return(')'); }
"["			{ count_output(); if (cover_all) return('['); }
"]"			{ count_output(); if (cover_all) return(']'); }
"."			{ count_output(); if (cover_all) return('.'); }
"&"			{ count_output(); if (cover_all) return('&'); }
"!"			{ count_output(); if (cover_all) return('!'); }
"~"			{ count_output(); if (cover_all) return('~'); }
"-"			{ count_output(); if (cover_all) return('-'); }
"+"			{ count_output(); if (cover_all) return('+'); }
"*"			{ count_output(); if (cover_all) return('*'); }
"/"			{ count_output(); if (cover_all) return('/'); }
"%"			{ count_output(); if (cover_all) return('%'); }
"<"			{ count_output(); if (cover_all) return('<'); }
">"			{ count_output(); if (cover_all) return('>'); }
"^"			{ count_output(); if (cover_all) return('^'); }
"|"			{ count_output(); if (cover_all) return('|'); }
"?"			{ count_output(); if (cover_all) return('?'); }

[ \t\v\f]		{ count(); cover_output(' '); }
[\n]                    { count(); }
.			{ count(); cover_output((int)(unsigned char)yytext[0]); }

%%

#ifndef FLEX
yywrap()
{
	return(1);
}
#endif

comment()
{
#ifdef FLEX
	fprintf(stderr,"\ncover: Input file contains a comment. Sorry, aborting...\n");
	exit(3);
#else
	char c, c1;

	ECHO;	/* leading "/*" */

loop:
	while ((c = input()) != '*' && c != 0)
		putc(c, yyout);

	putc(c, yyout);	/* the  '*' */

	if (c != 0 && (c1 = input()) != '/' && c1 != 0)
	{
		unput(c1);
		goto loop;
	}

	if (c != 0 && c1 != 0)
		putc(c1, yyout);
#endif
}

static void count_output(void)
{
        count();
        if (!cover_all) {
            fputs(yytext, yyout);
        }
}
