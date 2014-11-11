%{

#include <stdio.h>
#include <string.h>
#include "cover.h"

extern char last_typename[MAXLINE];

%}

%union {
  list_t*	list;
  char*		name;
  int		intval;
}

%token IDENTIFIER STRING_LITERAL
%token CONSTANT SIZEOF
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token XOR_ASSIGN OR_ASSIGN TYPE_NAME

%token TYPEDEF EXTERN STATIC AUTO REGISTER
%token CHAR SHORT INT INT8 INT16 INT32 INT64 LONG LONGLONG SIGNED UNSIGNED FLOAT DOUBLE
%token CONST VOLATILE VOID STRUCT UNION ENUM ELIPSIS

%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%token MODIFIER COVERPOP ASM

%type <list> 	init_declarator_list
%type <name>	init_declarator declarator direct_declarator

%start translation_unit
%%

lparen
	: '('
		{ cover_output('('); }
	;

rparen
	: ')'
		{ cover_output(')'); }
	;

lbracket
	: '['
		{ cover_output('['); }
	;

rbracket
	: ']'
		{ cover_output(']'); }
	;

lbrace
	: '{'
		{ cover_output('{'); }
	;

rbrace
	: '}'
		{ cover_output('}'); }
	;

semicolon
	: ';'
		{ cover_output(';'); }
	;

colon
	: ':'
		{ cover_output(':'); }
	;

comma
	: ','
		{ cover_output(','); }
	;

assign
	: '='
		{ cover_output('='); }
	;

star
	: '*'
		{ cover_output('*'); }
	;

string
	: STRING_LITERAL
		{ cover_output(STRING_LITERAL); }
	| string STRING_LITERAL
		{ cover_output(STRING_LITERAL); }
	;

primary_expr
	: identifier
	| CONSTANT
		{ cover_output(CONSTANT); }
	| string
	| lparen expr rparen
	;

inc_or_dec_op
	: INC_OP
		{ cover_output(INC_OP); }
	| DEC_OP
		{ cover_output(DEC_OP); }
	;

postfix_expr
	: primary_expr
	| postfix_expr lbracket expr rbracket
	| postfix_expr lparen rparen
                { set_function_call_expr(); }
	| postfix_expr lparen argument_expr_list rparen
                { set_function_call_expr(); }
	| postfix_expr '.'
		{ cover_output('.'); }
	  identifier
	| postfix_expr PTR_OP
		{ cover_output(PTR_OP); }
	  identifier
	| postfix_expr inc_or_dec_op
	;

argument_expr_list
	: assignment_expr
	| argument_expr_list comma assignment_expr
	;

sizeof
	: SIZEOF
		{ cover_output(SIZEOF); }
	;

unary_expr
	: postfix_expr
	| inc_or_dec_op unary_expr
	| unary_operator cast_expr
	| sizeof unary_expr
	| sizeof lparen type_name_decl rparen
	;

unary_operator
	: '&'
		{ cover_output('&'); }
	| star
	| '+'
		{ cover_output('+'); }
	| '-'
		{ cover_output('-'); }
	| '~'
		{ cover_output('~'); }
	| '!'
		{ cover_output('!'); }
	;

cast_expr
	: unary_expr
	| lparen type_name_decl rparen cast_expr
	;

multiplicative_expr
	: cast_expr
	| multiplicative_expr star cast_expr
	| multiplicative_expr '/'
		{ cover_output('/'); }
	  cast_expr
	| multiplicative_expr '%'
		{ cover_output('%'); }
	  cast_expr
	;

additive_expr
	: multiplicative_expr
	| additive_expr '+'
		{ cover_output('+'); }
	  multiplicative_expr
	| additive_expr '-'
		{ cover_output('-'); }
	  multiplicative_expr
	;

shift_expr
	: additive_expr
	| shift_expr LEFT_OP
		{ cover_output(LEFT_OP); }
	  additive_expr
	| shift_expr RIGHT_OP
		{ cover_output(RIGHT_OP); }
	  additive_expr
	;

relational_expr
	: shift_expr
	| relational_expr '<'
		{ cover_output('<'); }
	  shift_expr
	| relational_expr '>'
		{ cover_output('>'); }
	  shift_expr
	| relational_expr LE_OP
		{ cover_output(LE_OP); }
	  shift_expr
	| relational_expr GE_OP
		{ cover_output(GE_OP); }
	  shift_expr
	;

equality_expr
	: relational_expr
	| equality_expr EQ_OP
		{ cover_output(EQ_OP); }
	  relational_expr
	| equality_expr NE_OP
		{ cover_output(NE_OP); }
	  relational_expr
	;

and_expr
	: equality_expr
	| and_expr '&'
		{ cover_output('&'); }
	  equality_expr
	;

exclusive_or_expr
	: and_expr
	| exclusive_or_expr '^'
		{ cover_output('^'); }
	  and_expr
	;

inclusive_or_expr
	: exclusive_or_expr
	| inclusive_or_expr '|'
		{ cover_output('|'); }
	  exclusive_or_expr
	;

logical_and_expr
	: inclusive_or_expr
	| logical_and_expr AND_OP
		{ cover_output(AND_OP); }
	  inclusive_or_expr
	;

logical_or_expr
	: logical_and_expr
	| logical_or_expr OR_OP
		{ cover_output(OR_OP); }
	  logical_and_expr
	;

conditional_expr
	: logical_or_expr
	| logical_or_expr '?'
		{ cover_output('?'); }
	  expr colon conditional_expr
	;

assignment_expr
	: conditional_expr
	| unary_expr assignment_operator assignment_expr
	;

assignment_operator
	: assign
	| MUL_ASSIGN
		{ cover_output(MUL_ASSIGN); }
	| DIV_ASSIGN
		{ cover_output(DIV_ASSIGN); }
	| MOD_ASSIGN
		{ cover_output(MOD_ASSIGN); }
	| ADD_ASSIGN
		{ cover_output(ADD_ASSIGN); }
	| SUB_ASSIGN
		{ cover_output(SUB_ASSIGN); }
	| LEFT_ASSIGN
		{ cover_output(LEFT_ASSIGN); }
	| RIGHT_ASSIGN
		{ cover_output(RIGHT_ASSIGN); }
	| AND_ASSIGN
		{ cover_output(AND_ASSIGN); }
	| XOR_ASSIGN
		{ cover_output(XOR_ASSIGN); }
	| OR_ASSIGN
		{ cover_output(OR_ASSIGN); }
	;

expr
	: assignment_expr
	| expr comma assignment_expr
	;

constant_expr
	: conditional_expr
	;

declaration
	: declaration_specifiers semicolon
	| declaration_specifiers init_declarator_list semicolon
		{ end_declaration($2, $<intval>1); }
	;

declaration_specifiers
	: storage_class_specifier
		{ $<intval>$ = $<intval>1; }
	| storage_class_specifier declaration_specifiers
		{ $<intval>$ = $<intval>1; }
	| type_specifier
	| type_specifier declaration_specifiers
	| type_qualifier
	| type_qualifier declaration_specifiers
	;

init_declarator_list
	: init_declarator
		{ $$ = create_list($1); }
	| init_declarator_list comma init_declarator
		{ $$ = add_list($1, $3); }
	;

init_declarator
	: declarator
	| declarator assign initializer
	;

storage_class_specifier
	: TYPEDEF 
		{ cover_output(TYPEDEF); $<intval>$ = TYPEDEF; }
	| STATIC
		{ cover_output(STATIC); }
	| EXTERN
		{ cover_output(EXTERN); }
	| AUTO
		{ cover_output(AUTO); }
	| REGISTER
		{ cover_output(REGISTER); }
	;

type_specifier
	: VOID
		{ cover_output(VOID); }
	| CHAR
		{ cover_output(CHAR); }
	| SHORT
		{ cover_output(SHORT); }
	| INT
		{ cover_output(INT); }
	| INT8
		{ cover_output(INT8); }
	| INT16
		{ cover_output(INT16); }
	| INT32
		{ cover_output(INT32); }
	| INT64
		{ cover_output(INT64); }
	| LONG
		{ cover_output(LONG); }
	| LONGLONG
		{ cover_output(LONGLONG); }
	| FLOAT
		{ cover_output(FLOAT); }
	| DOUBLE
		{ cover_output(DOUBLE); }
	| SIGNED
		{ cover_output(SIGNED); }
	| UNSIGNED
		{ cover_output(UNSIGNED); }
	| struct_or_union_specifier
	| enum_specifier
	| type_name
	;

struct_or_union_specifier
	: struct_or_union lbrace struct_declaration_list rbrace
	| struct_or_union identifier_or_type_name lbrace struct_declaration_list rbrace
	| struct_or_union identifier_or_type_name
	;

identifier_or_type_name
        : identifier
        | type_name
        ;

struct_or_union
	: STRUCT
		{ cover_output(STRUCT); }
	| UNION
		{ cover_output(UNION); }
	;

struct_declaration_list
	: struct_declaration
	| struct_declaration_list struct_declaration
	;

struct_declaration
	: specifier_qualifier_list struct_declarator_list semicolon
	| specifier_qualifier_list semicolon
	;

specifier_qualifier_list
	: type_specifier
	| type_specifier specifier_qualifier_list
	| type_qualifier
	| type_qualifier specifier_qualifier_list
	;

struct_declarator_list
	: struct_declarator
	| struct_declarator_list comma struct_declarator
	;

struct_declarator
	: declarator
		{ free($1); }
	| colon constant_expr
	| declarator colon constant_expr
		{ free($1); }
	;

enum
	: ENUM
		{ cover_output(ENUM); }
	;

enum_specifier
	: enum lbrace enumerator_list opt_comma rbrace
	| enum identifier lbrace enumerator_list opt_comma rbrace
	| enum identifier
	;

enumerator_list
	: enumerator
	| enumerator_list comma enumerator
	;

enumerator
	: identifier
	| identifier assign constant_expr
	;

opt_comma
        : comma
        | /* empty */
        ;

type_qualifier
	: CONST
		{ cover_output(CONST); }
	| VOLATILE
		{ cover_output(VOLATILE); }
        | modifier
	;

asm
	: ASM
		{ cover_output(ASM); }
	;

modifier
	: MODIFIER
		{ cover_output(MODIFIER); }
	;
/*
modifier_list
	: modifier
	| modifier_list modifier
	;
*/
declarator
	: direct_declarator
	| pointer direct_declarator
		{ $$ = $2; }
	;

direct_declarator
	: identifier
		{ $$ = c_strdup(last_identifier); }
	| lparen declarator rparen
		{ $$ = $2; }
	| lparen modifier declarator rparen
		{ $$ = $3; }
	| direct_declarator lbracket rbracket
	| direct_declarator lbracket constant_expr rbracket
	| direct_declarator lparen parameter_type_list rparen
	| direct_declarator lparen rparen
	| direct_declarator lparen identifier_list rparen
	;

pointer
	: star
	| star specifier_qualifier_list
	| star pointer
	| star specifier_qualifier_list pointer
	;

parameter_type_list
	: parameter_list
	| parameter_list comma ELIPSIS
		{ cover_output(ELIPSIS); }
	;

parameter_list
	: parameter_declaration
	| parameter_list comma parameter_declaration
	;

parameter_declaration
	: declaration_specifiers declarator
		{ free($2); }
	| declaration_specifiers
	| declaration_specifiers abstract_declarator
	;

identifier_list
	: identifier
	| identifier_list comma identifier
	;

type_name_decl
	: specifier_qualifier_list
	| specifier_qualifier_list abstract_declarator
	;

abstract_declarator
	: pointer
	| direct_abstract_declarator
	| pointer direct_abstract_declarator
	;

direct_abstract_declarator
	: lparen abstract_declarator rparen
	| lparen modifier abstract_declarator rparen
	| lbracket rbracket
	| lbracket constant_expr rbracket
	| direct_abstract_declarator lbracket rbracket
	| direct_abstract_declarator lbracket constant_expr rbracket
	| lparen rparen
	| lparen parameter_type_list rparen
	| direct_abstract_declarator lparen rparen
	| direct_abstract_declarator lparen parameter_type_list rparen
	;

initializer
	: assignment_expr
	| lbrace initializer_list rbrace
	| lbrace initializer_list comma rbrace
	;

initializer_list
	: initializer
	| initializer_list comma initializer
	;

statement
	: labeled_statement
	| compound_statement
	| expression_statement
	| selection_statement
		{ cover_output(COVERPOP); }
	| iteration_statement
		{ cover_output(COVERPOP); }
	| jump_statement
	| asm
	;

labeled_statement
	: identifier colon statement
	| CASE
		{ cover_output(CASE); }
	  constant_expr colon
		{ cover_case(); }
	  statement
	| DEFAULT
		{ cover_output(DEFAULT); }
	  colon
		{ cover_case(); }
	  statement
	;

cover_statement
	:	{ cover_begin(); }
	  statement
		{ cover_end(); }
	;

compound_statement
	: lbrace rbrace
	| lbrace statement_list rbrace
	| lbrace declaration_list rbrace
	| lbrace declaration_list statement_list rbrace
	;

declaration_list
	: declaration
	| declaration_list declaration
	;

statement_list
	: statement
	| statement_list statement
	;

expression_statement
	: semicolon
	| expr semicolon
	;

mc_expr
	: { begin_McCabe(); }
	  expr
	  { end_McCabe(); }

if
	: IF
		{ cover_output(IF); }
	;

selection_statement
	: if lparen mc_expr rparen cover_statement
		{ cover_add_else(); }
	| if lparen mc_expr rparen cover_statement ELSE
		{ cover_output(ELSE); }
	  cover_statement
	| SWITCH
		{ cover_output(SWITCH); }
	  lparen expr rparen statement
	;

while
	: WHILE
		{ cover_output(WHILE); }
	;

for
	: FOR
		{ cover_output(FOR); }
	;

iteration_statement
	: while lparen mc_expr rparen cover_statement
	| DO
		{ cover_output(DO); }
	  cover_statement while lparen mc_expr rparen semicolon
	| for lparen      semicolon         semicolon      rparen cover_statement
	| for lparen expr semicolon         semicolon      rparen cover_statement
	| for lparen      semicolon mc_expr semicolon      rparen cover_statement
	| for lparen      semicolon         semicolon expr rparen cover_statement
	| for lparen      semicolon mc_expr semicolon expr rparen cover_statement
	| for lparen expr semicolon         semicolon expr rparen cover_statement
	| for lparen expr semicolon mc_expr semicolon      rparen cover_statement
	| for lparen expr semicolon mc_expr semicolon expr rparen cover_statement
	;

jump_statement
	: GOTO
		{ cover_output(GOTO); }
	  identifier semicolon
	| CONTINUE
		{ cover_output(CONTINUE); }
	  semicolon
	| BREAK
		{ cover_output(BREAK); }
	  semicolon
	| return semicolon
		{ end_return(); }
	| return expr semicolon
		{ end_return(); }
	;

return
	: RETURN
		{ begin_return(); cover_output(RETURN); }
	;

translation_unit
	: external_declaration
	| translation_unit external_declaration
	;

external_declaration
	: function_definition
	| declaration
	;

fun_compound_statement
	: fun_end
	| statement_list fun_end
	| declaration_list fun_end
	| declaration_list statement_list fun_end
	;

fun_end
	: rbrace
		{ end_function(); }
	;

function_definition
	: declarator lbrace
		{ begin_function($1); }
	  fun_compound_statement
	| declarator declaration_list lbrace 
		{ begin_function($1); }
	  fun_compound_statement 
	| declaration_specifiers declarator lbrace 
		{ begin_function($2); }
	  fun_compound_statement 
	| declaration_specifiers declarator declaration_list lbrace
		{ begin_function($2); }
	  fun_compound_statement 
        | /* empty */
	;

identifier
	: IDENTIFIER
		{ cover_output(IDENTIFIER); }
	;

type_name
	: TYPE_NAME
		{ cover_output(TYPE_NAME); }
        ;
%%
