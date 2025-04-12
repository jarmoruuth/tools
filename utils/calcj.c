/************************************************************************\
 *
 *		CALC.C
 *
 * Expression evaluator. Should understand most C-expressions.
 *
 * Version: 1.00 30-Jul-1988	first version using longs
 *	    1.10 14-Sep-1988	support for floats and user variables
 *	    1.20 20-Nov-1988	library functions and control variables
 *	    1.21 31-Jan-1989	fixed a bug handling float after dot
 *
 * Author: Jarmo Ruuth
 *
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <ectype.h>
#include <math.h>
#include <setjmp.h>
#include <conio.h>
#include <setjmp.h>

#include <c.h>

static char	version[] = "version 1.21";

typedef double	calc_t;
typedef long	int_t;

#define	MAXBUF		255
#define	MAXTOKEN	MAXBUF
#define MAX_ARG		2
#define ERROR_VALUE	(calc_t)-1
#define CTRL_C		3		/* ^C keycode */
#define ROUND_NAME	"round"
#define DIGITS_NAME	"digits"
#define BASE_NAME	"radix"
#define DEFAULT_ROUND	FALSE
#define DEFAULT_DIGITS	4
#define DEFAULT_BASE	2		/* binary */

typedef enum {
    L_PAR,	 R_PAR,	  PLUS,	   MINUS,   NEG,     NOT,
    MUL,	 DIV,	  REM,	   SHL,	    SHR,     LT,      GT,	LTE,
    GTE,	 EQ,	  NOT_EQ,  AND,	    XOR,     OR,      LOG_AND,	LOG_OR,
    QUEST,	 COLON,	  COMMA,   ASS,	    MUL_ASS, DIV_ASS, REM_ASS,	ADD_ASS,
    SUB_ASS, SHL_ASS, SHR_ASS, AND_ASS, XOR_ASS, OR_ASS,  INVALID,	CONST,
    EMPTY,	 DIGIT,	  ALPHA
} OPERATORS;

static char *operators[] = {	/* for help */
    "(",	")",	"+",	"-",	"~",	"!",	"*",	"/",	"%",
    "<<",	">>",	"<",	">",	"<=",	">=",	"==",	"!=",	"&",
    "^",	"|",	"&&",	"||",	"?",	":",	"=",	"*=",	"/=",
    "%=",	"+=",	"-=",	"<<=",	">>=",	"&=",	"^=",	"|=",	","
};

#define	NOPER	(sizeof(operators) / sizeof(operators[0]))

typedef enum {
    ACOS,	ASIN,	ATAN,	ATAN2,	CEIL,	COS,	COSH,	EXP,
    FABS,	FLOOR,	FMOD,	LOG,	LOG10,	MODF,	POW,	SIN,
    SINH,	SQRT,	TAN,	TANH
} FUNCS;

typedef struct {
    FUNCS	type;
    char	*name;
    double	(*fun)(double, ...);
    int	n_args;
} FUNC;

static FUNC funcs[] = {
    { ACOS,		"acos",		acos,  1 },
    { ASIN,		"asin",		asin,  1 },
    { ATAN,		"atan",		atan,  1 },
    { ATAN2,	"atan2",	atan2, 2 },
    { CEIL,		"ceil",		ceil,  1 },
    { COS,		"cos",		cos,   1 },
    { COSH,		"cosh",		cosh,  1 },
    { EXP,		"exp",		exp,   1 },
    { FABS,		"fabs",		fabs,  1 },
    { FLOOR,	"floor",	floor, 1 },
    { FMOD,		"fmod",		fmod,  2 },
    { LOG,		"log",		log,   1 },
    { LOG10,	"log10",	log10, 1 },
    { MODF,		"modf",		modf,  2 },
    { POW,		"pow",		pow,   2 },
    { SIN,		"sin",		sin,   1 },
    { SINH,		"sinh",		sinh,  1 },
    { SQRT,		"sqrt",		sqrt,  1 },
    { TAN,		"tan",		tan,   1 },
    { TANH,		"tanh",		tanh,  1 }
};

#define	NFUNC	(sizeof(funcs) / sizeof(FUNC))

typedef struct VAR_NODE {
    char		*name;
    calc_t		value;
    struct VAR_NODE	*next;
} VAR_NODE;

static VAR_NODE		*var_list = NULL;
static char		buffer[MAXBUF];
static char		buf1[MAXTOKEN];
static char		buf2[MAXTOKEN];
static char		*current;
static char		*next;
static char		*tokstr;
static char		*error_msg;
static char		*error_tok;
static int		eval_err;	/* flag for errors in expression */
static int		debug = FALSE;
static OPERATORS	op_type;
static OPERATORS	next_op_type;
static OPERATORS	current_op_type;
static calc_t		current_value;
static calc_t		next_value;
static calc_t		token_value;
static jmp_buf		calc_error;
static int		n_var = 0;
static int		do_round = FALSE;
static int		digits = DEFAULT_DIGITS;
static int		base = DEFAULT_BASE;
static int		in_function;

static calc_t expression(void);

/**********************************************************************
 *
 *	eval_error
 */
static void eval_error(char *msg, char *tok)
{
    eval_err = TRUE;
    error_msg = msg;
    error_tok = tok;
    *tokstr = '\0';		/* don't allow any more tokens, */
    current_op_type = next_op_type = EMPTY;
    longjmp(calc_error, 1);	/* stop evaluating */
}

/**********************************************************************
 *
 *	matherr
 */
int matherr (struct _exception *e)
{
    register char *msg;

    switch (e->type) {
        case DOMAIN:
            msg = "Argument domain error (e.g. log(-1)) in %s";
            break;
        case SING:
            msg = "Argument singularity (e.g. pow(0,-2)) in %s";
            break;
        case OVERFLOW:
            msg = "Overflow range error (e.g. exp(1000)) in %s";
            break;
        case UNDERFLOW:
            msg = "Underflow range error (e.g. exp(-1000)) in %s";
            break;
        case TLOSS:
            msg = "Total loss of significance (e.g. sin(10e70)) in %s";
            break;
        case PLOSS:
            msg = "Partial loss of significance in %s";
            break;
        default:
            msg = "Unknown math error in %s";
            break;
    }
    eval_error(msg, e->name);
        e->retval = 1;
    return 0;
}

/**********************************************************************
 *
 *	calc_malloc
 */
static void *calc_malloc (unsigned size)
{
    register void	*tmp;

    tmp = malloc(size);
    if (tmp == NULL) {
        printf("Fatal error: out of heap space in malloc\n");
        exit(-1);
    }
    return tmp;
}

/**********************************************************************
 *
 *	calc_realloc
 */
static void *calc_realloc (void *ptr, unsigned size)
{
    register void	*tmp;

    tmp = realloc(ptr, size);
    if (tmp == NULL) {
        printf("Fatal error: out of heap space in realloc\n");
        exit(-1);
    }
    return tmp;
}

/**********************************************************************
 *
 *	func_sort_cmp
 */
static int func_sort_cmp (const FUNC *f1, const FUNC *f2)
{
    return strcmp(f1->name, f2->name);
}

/**********************************************************************
 *
 *	func_search_cmp
 */
static int func_search_cmp (const char *key, const FUNC *f)
{
    return strcmp(key, f->name);
}

/**********************************************************************
 *
 *	find_func
 */
#define find_func(fname)	\
    (FUNC *)bsearch((fname), funcs, NFUNC, sizeof(FUNC), func_search_cmp)

/**********************************************************************
 *
 *	var_sort_cmp
 */
static int var_sort_cmp (const VAR_NODE *v1, const VAR_NODE *v2)
{
    return strcmp(v1->name, v2->name);
}

/**********************************************************************
 *
 *	var_search_cmp
 */
static int var_search_cmp (const char *key, const VAR_NODE *v)
{
    return strcmp(key, v->name);
}

/**********************************************************************
 *
 *	find_var
 */
#define find_var(name)	\
    (VAR_NODE *)bsearch((name), var_list, n_var, sizeof(VAR_NODE), var_search_cmp);

/**********************************************************************
 *
 *	add_var
 */
static VAR_NODE *add_var(register char *name, calc_t value)
{
    if (find_func(name) != NULL)
        eval_error("%s is predefined as a function", name);
    if (n_var == 0) {
        var_list = calc_malloc(sizeof(VAR_NODE));
        n_var = 1;
    } else
        var_list = calc_realloc(var_list, sizeof(VAR_NODE) * ++n_var);
    var_list[n_var-1].name = calc_malloc(strlen(name)+1);
    strcpy(var_list[n_var-1].name, name);
    var_list[n_var-1].value = value;
    qsort(var_list, n_var, sizeof(VAR_NODE), var_sort_cmp);
    return find_var(name);
}

/**********************************************************************
 *
 *	get_int
 */
static void get_int(char **tokstr_p, char **buf_p, int is_float)
{
    register char	*tokstr = *tokstr_p;
    register char	*buf = *buf_p;

    if (*tokstr == '0' && toupper(*(tokstr+1)) == 'X') {
        strcpy(buf, "0x");		/* Hex num */
        buf += 2;
        tokstr += 2;
        while(isxdigit(*tokstr))
            *buf++ = *tokstr++;
    } else if (*tokstr == '0' && !is_float) {/* Oct num */
        while(*tokstr >= '0' && *tokstr <= '7')
            *buf++ = *tokstr++;
    } else {				/* Dec num */
        while(isdigit(*tokstr))
            *buf++ = *tokstr++;
    }
    *tokstr_p = tokstr;
    *buf_p = buf;
}

/**********************************************************************
 *
 *	digit_token
 */
static calc_t digit_token(char *buf)
{
    int_t		int_value;
    calc_t		calc_value;
    register char	*buf_start = buf;
    register int	is_float = FALSE;

    get_int(&tokstr, &buf, is_float);
    if (*tokstr == '.') {
        is_float = TRUE;
        *buf++ = *tokstr++;
        get_int(&tokstr, &buf, is_float);
    }
    if (toupper(*tokstr) == 'E') {
        is_float = TRUE;
        *buf++ = *tokstr++;
        if (*tokstr == '+' || *tokstr == '-')
            *buf++ = *tokstr++;
        get_int(&tokstr, &buf, is_float);
    }
    *buf = '\0';
    if (is_float) {
        if (sscanf (buf_start, "%lf", &calc_value) != 1)
            eval_error("Invalid number constant '%s'", buf_start);
        return calc_value;
    } else {
        if (sscanf (buf_start, "%lI", &int_value) != 1)
            eval_error("Invalid number constant '%s'", buf_start);
        return (calc_t)int_value;
    }
}

/**********************************************************************
 *
 *	var_token
 */
static void var_token(register char *buf)
{
    while(isalnum(*tokstr) || *tokstr == '_')
        *buf++ = *tokstr++;
    *buf = '\0';
}

/**********************************************************************
 *
 *	operator_token
 *
 * Really ugly (but hopefully reasonable efficient) procedure to find
 * the operator token and it's type.
 */
static OPERATORS operator_token(char *buf)
{
    register int	op;
    register char	*ts;	/* copy of tokstr */

    ts = tokstr;
    switch (*ts++) {
        case '<':	/* check '<', '<<', '<<=' and '<=' */
            op = LT;
            switch (*ts) {
                case '<':
                    ts++;
                    op = SHL;
                    if (*ts == '=') {
                        ts++;
                        op = SHL_ASS;
                    }
                    break;
                case '=':
                    ts++;
                    op = LTE;
                    break;
            }
            break;
        case '>':	/*  check '>', '>>', '>>=' and '>=' */
            op = GT;
            switch (*ts) {
                case '>':
                    ts++;
                    op = SHR;
                    if (*ts == '=') {
                        ts++;
                        op = SHR_ASS;
                    }
                    break;
                case '=':
                    ts++;
                    op = GTE;
                    break;
            }
            break;
        case '!':	/* check '!=' */
            op = NOT;
            if (*ts == '=') {
                ts++;
                op = NOT_EQ;
            }
            break;
        case '*':
            op = MUL;
            if (*ts == '=') {
                ts++;
                op = MUL_ASS;
            }
            break;
        case '/':
            op = DIV;
            if (*ts == '=') {
                ts++;
                op = DIV_ASS;
            }
            break;
        case '%':
            op = REM;
            if (*ts == '=') {
                ts++;
                op = REM_ASS;
            }
            break;
        case '^':
            op = XOR;
            if (*ts == '=') {
                ts++;
                op = XOR_ASS;
            }
            break;
        case '+':
            op = PLUS;
            if (*ts == '=') {
                ts++;
                op = ADD_ASS;
            }
            break;
        case '-':
            op = MINUS;
            if (*ts == '=') {
                ts++;
                op = SUB_ASS;
            }
            break;
        case '=':
            op = ASS;
            if (*ts == '=') {
                ts++;
                op = EQ;
            }
            break;
        case '|':
            op = OR;
            switch (*ts) {
                case '=':
                    ts++;
                    op = OR_ASS;
                    break;
                case '|':
                    ts++;
                    op = LOG_OR;
                    break;
            }
            break;
        case '&':
            op = AND;
            switch (*ts) {
                case '=':
                    ts++;
                    op = AND_ASS;
                    break;
                case '&':
                    ts++;
                    op = LOG_AND;
                    break;
            }
            break;
        case '~':
            op = NEG;
            break;
        case '(':
            op = L_PAR;
            break;
        case ')':
            op = R_PAR;
            break;
        case ',':
            op = COMMA;
            break;
        case '?':
            op = QUEST;
            break;
        case ':':
            op = COLON;
            break;
        default:
            op = INVALID;
    }
    while (tokstr < ts)
        *buf++ = *tokstr++;
    *buf = '\0';
    if (op == INVALID)
        eval_error("invalid character %s", buf);
    return op;
}

/**********************************************************************
 *
 *	token
 */
static char *token (void)
{
    register char	*buf;
    static char	*prev = buf1;

    buf = prev = prev == buf1 ? buf2 : buf1;
    while (isspace(*tokstr))
        tokstr++;
    if (*tokstr == '\0') {
        op_type = EMPTY;
        return NULL;
    }
    if (isdigit(*tokstr) || *tokstr == '.') {
        token_value = digit_token(buf);
        op_type = DIGIT;
    } else if (isalpha(*tokstr)) {
        var_token(buf);
        op_type = ALPHA;
    } else
        op_type = operator_token(buf);
    if (debug)
        printf("token: '%s'\n", prev);
    return prev;
}

/**********************************************************************
 *
 *	fetch_next
 */
static void fetch_next(void)
{
    current = next;
    if ((current_op_type = next_op_type) == DIGIT)
        current_value = next_value;
    next = token();
    if ((next_op_type = op_type) == DIGIT)
        next_value = token_value;
}

/**********************************************************************
 *
 *	fetch_new
 */
static void fetch_new(void)
{
    current = token();
    if ((current_op_type = op_type) == DIGIT)
        current_value = token_value;
    next = token();
    if ((next_op_type = op_type) == DIGIT)
        next_value = token_value;
}

/**********************************************************************
 *
 *	to_int_t
 */
static int_t to_int_t (calc_t val)
{
    if (do_round)
        return (int_t)floor(val + 0.5);
    else
        return (int_t)val;
}

/**********************************************************************
 *
 *	primary_exp
 */
static calc_t primary_exp(void)
{
    register VAR_NODE	*var = NULL;
    calc_t			i;

    switch (current_op_type) {
        case L_PAR:
            fetch_next();		/* skip L_PAR */
            i = expression();
            if (next_op_type != R_PAR)
                eval_error("Unbalanced parenthesis", NULL);
            fetch_next();	/* skip R_PAR */
            return i;
        default:
            if (current_op_type != ALPHA && current_op_type != DIGIT)
                eval_error("Number expected instead of '%s'",
                       current);
            if (next_op_type == ALPHA || next_op_type == DIGIT)
                eval_error("Operator expected instead of '%s'",
                       next);
            switch (current_op_type) {
                case DIGIT:
                    return current_value;
                case ALPHA:
                    var = find_var(current);
                    if (!var)
                        eval_error("Unbound variable %s",
                               current);
                    return var->value;
                default:
                    eval_error("Internal error in primary_exp",
                           NULL);
            }
    }
    /* NOTREACHED */
    return ERROR_VALUE;	/* keep compiler happy */
}

/**********************************************************************
 *
 *	check_func
 */
static calc_t check_func (void)
{
    register FUNC	*func;
    register int	i;
    calc_t		arg[MAX_ARG];

    if (current_op_type != ALPHA || (func = find_func(current)) == NULL)
        return primary_exp();
    fetch_next();
    if (current_op_type != L_PAR)
        eval_error("'(' missing after function call %s", func->name);
    /* get the first argument */
    fetch_next();
    in_function = TRUE;
    arg[0] = expression();
    /* get other arguments */
    for (i=1; i < func->n_args; i++) {
        fetch_next();
        if (current_op_type != COMMA)
            eval_error("Too few arguments in call to %s", func->name);
        fetch_next();
        arg[i] = expression();
    }
    in_function = FALSE;
    fetch_next();
    if (current_op_type == COMMA)
        eval_error("Too many arguments in call to %s", func->name);
    if (current_op_type != R_PAR)
        eval_error("')' missing in call to function %s", func->name);
    switch (func->n_args) {
        case 1:
            if (debug)
                printf("call %s(%.4lf)\n", func->name, arg[0]);
            return (*func->fun)(arg[0]);
        case 2:
            if (debug)
                printf("call %s(%.4lf, .4lf)\n", func->name,
                    arg[0], arg[1]);
            return (*func->fun)(arg[0], arg[1]);
        default:
            eval_error("Internal error: "
                   "unsupported number of arguments in call to %s",
                   func->name);
    }
    return ERROR_VALUE;
}

/**********************************************************************
 *
 *	unary_exp
 */
static calc_t unary_exp(void)
{
    switch (current_op_type) {
        case PLUS:
            fetch_next();
            return check_func();
        case MINUS:
            fetch_next();
            return -check_func();
        case NEG:
            fetch_next();
            return (calc_t)~to_int_t(check_func());
        case NOT:
            fetch_next();
            return !check_func();
        default:
            return check_func();
    }
}

/**********************************************************************
 *
 *	multiplicative_exp
 */
static calc_t multiplicative_exp(void)
{
    calc_t	i, j;

    i = unary_exp();
    for (;;) {
        switch (next_op_type) {
            case MUL:
                fetch_new();
                i *= unary_exp();
                break;
            case DIV:
                fetch_new();
                if ((j = unary_exp()) == 0)
                    eval_error("Division by zero", NULL);
                i /= j;
                break;
            case REM:
                fetch_new();
                if ((j = unary_exp()) == 0)
                    eval_error("Division by zero", NULL);
                i = (calc_t)(to_int_t(i) % to_int_t(j));
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	additive_exp
 */
static calc_t additive_exp(void)
{
    calc_t	i;

    i = multiplicative_exp();
    for (;;) {
        switch (next_op_type) {
            case PLUS:
                fetch_new();
                i += multiplicative_exp();
                break;
            case MINUS:
                fetch_new();
                i -= multiplicative_exp();
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	shift_exp
 */
static calc_t shift_exp(void)
{
    calc_t	i;

    i = additive_exp();
    for (;;) {
        switch (next_op_type) {
            case SHL:
                fetch_new();
                i = (calc_t)(to_int_t(i) << to_int_t(additive_exp()));
                break;
            case SHR:
                fetch_new();
                i = (calc_t)(to_int_t(i) >> to_int_t(additive_exp()));
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	relational_exp
 */
static calc_t relational_exp(void)
{
    calc_t	i;

    i = shift_exp();
    for (;;) {
        switch (next_op_type) {
            case LT:
                fetch_new();
                i = i < shift_exp();
                break;
            case GT:
                fetch_new();
                i = i > shift_exp();
                break;
            case LTE:
                fetch_new();
                i = i <= shift_exp();
                break;
            case GTE:
                fetch_new();
                i = i >= shift_exp();
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	equality_exp
 */
static calc_t equality_exp(void)
{
    calc_t	i;

    i = relational_exp();
    for (;;) {
        switch (next_op_type) {
            case EQ:
                fetch_new();
                i = i == relational_exp();
                break;
            case NOT_EQ:
                fetch_new();
                i = i != relational_exp();
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	and_exp
 */
static calc_t and_exp(void)
{
    calc_t	i;

    i = equality_exp();
    for (;;) {
        switch (next_op_type) {
            case AND:
                fetch_new();
                i = (calc_t)(to_int_t(i) & to_int_t(equality_exp()));
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	exclusive_or_exp
 */
static calc_t exclusive_or_exp(void)
{
    calc_t	i;

    i = and_exp();
    for (;;) {
        switch (next_op_type) {
            case XOR:
                fetch_new();
                i = (calc_t)(to_int_t(i) ^ to_int_t(and_exp()));
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	inclusive_or_exp
 */
static calc_t inclusive_or_exp(void)
{
    calc_t	i;

    i = exclusive_or_exp();
    for (;;) {
        switch (next_op_type) {
            case OR:
                fetch_new();
                i = (calc_t)(to_int_t(i) | to_int_t(exclusive_or_exp()));
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	logical_and_exp
 */
static calc_t logical_and_exp(void)
{
    calc_t	i;

    i = inclusive_or_exp();
    for (;;) {
        switch (next_op_type) {
            case LOG_AND:
                fetch_new();
                i = i && inclusive_or_exp();
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	logical_or_exp
 */
static calc_t logical_or_exp(void)
{
    calc_t	i;

    i = logical_and_exp();
    for (;;) {
        switch (next_op_type) {
            case LOG_OR:
                fetch_new();
                i = i || logical_and_exp();
                break;
            default:
                return (i);
        }
    }
}

/**********************************************************************
 *
 *	conditional_exp
 */
static calc_t conditional_exp(void)
{
    calc_t	i,j;

    i = logical_or_exp();
    switch (next_op_type) {
        case QUEST:
            fetch_new();
            j = expression();
            fetch_new();		/* skip COLON */
            i = i ? j : conditional_exp();
            return(i);
        default:
            return (i);
    }
}

#ifdef NEVER_USED
/**********************************************************************
 *
 *	constant_expression
 */
static calc_t constant_expression(void)
{
    return (conditional_exp());
}
#endif

/**********************************************************************
 *
 *	assignment_exp
 */
static calc_t assignment_exp(void)
{
    calc_t			i;
    register VAR_NODE	*var = NULL;

    if (isalpha(*current))	/* Might be assignment */
        var = find_var(current);
    switch (next_op_type) {
        case ASS:
            if (var == NULL)
                var = add_var(current, (calc_t)0);
            fetch_new();
            var->value = assignment_exp();
            break;
        case MUL_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value *= assignment_exp();
            break;
        case DIV_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            if ((i = assignment_exp()) == 0)
                eval_error("Division by zero", NULL);
            var->value /= i;
            break;
        case REM_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            if ((i = assignment_exp()) == 0)
                eval_error("Division by zero", NULL);
            var->value =
                (calc_t)(to_int_t(var->value) % to_int_t(i));
            break;
        case ADD_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value += assignment_exp();
            break;
        case SUB_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value -= assignment_exp();
            break;
        case SHL_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value =
            (calc_t)(to_int_t(var->value) << to_int_t(assignment_exp()));
            break;
        case SHR_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value =
            (calc_t)(to_int_t(var->value) >> to_int_t(assignment_exp()));
            break;
        case AND_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value =
            (calc_t)(to_int_t(var->value) & to_int_t(assignment_exp()));
            break;
        case XOR_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value =
            (calc_t)(to_int_t(var->value) ^ to_int_t(assignment_exp()));
            break;
        case OR_ASS:
            if (var == NULL)
                eval_error("Unbound varible '%s'", current);
            fetch_new();
            var->value =
            (calc_t)(to_int_t(var->value) | to_int_t(expression()));
            break;
        default:
            return conditional_exp();
    }
    /* we came here only after assingment operator, now check if it was some
       of the control variables */
    if (strcmp(var->name, ROUND_NAME) == 0)
        do_round = var->value != (calc_t)FALSE;
    if (strcmp(var->name, DIGITS_NAME) == 0)
        digits = (int)to_int_t(var->value);
    if (strcmp(var->name, BASE_NAME) == 0)
        base = (int)to_int_t(var->value);
    return var->value;
}

/**********************************************************************
 *
 *	expression
 */
static calc_t expression(void)
{
    calc_t	i;

    i = assignment_exp();
    if (in_function)
        return i;
    for (;;) {
        switch (next_op_type) {
            case COMMA:
                fetch_new();
                i = assignment_exp();
                break;
            default:
                return i;
        }
    }
}

/**********************************************************************
 *
 *	eval
 */
static calc_t eval (char *str)
{
    calc_t	i;

    in_function = FALSE;
    eval_err = FALSE;
    tokstr = str;
    fetch_new();
    i = expression();
    if (next_op_type != EMPTY)
        eval_error("Error in expression", NULL);
    return i;
}

/**********************************************************************
 *
 *	getline
 */
static char *getline(void)
{
#ifdef __TURBOC__
    union REGS	rg;
    static char	*b = buffer+2;

    /* Dos function 0AH, 'Buffered input' is used because it works with
       most command line editors like CED. This requires small model. */
    memset(buffer, '\0', MAXBUF);
    buffer[0] = MAXBUF-2;
    rg.x.dx = FP_OFF(buffer);
    rg.h.ah = 0x0A;
    int86(0x21, &rg, &rg);
    *(b+buffer[1]) = '\0';
    printf("\n");
    return b;
#else
        static char b[255];

        if (gets(b) == NULL) {
            exit(1);
        }

        return(b);
#endif
}

/**********************************************************************
 *
 *	show_error
 */
static void show_error(void)
{
    printf("ERROR: ");
    printf(error_msg, error_tok);
    printf("\n");
}

/**********************************************************************
 *
 *	show_value
 */
static void show_value (calc_t value, int table_format)
{
    register VAR_NODE	*var;
    int_t			int_value;

    if (base < 2 || base > 36) {
        printf("WARNING: %s=%d not in accepted range (2, 36), using default %d\n",
            BASE_NAME, base, DEFAULT_BASE);
        base = DEFAULT_BASE;
        var = find_var(BASE_NAME);
        var->value = (calc_t)DEFAULT_BASE;
    }
    int_value = to_int_t(value);
    if (table_format)
        printf("%12.*lf %8ld 0x%08lX 0%012lo %s\n",
            digits, value, int_value, int_value,
            int_value, ltoa(int_value, buffer, base));
    else
        printf("%01.*lf %01ld 0x%01lX 0%01lo %s\n",
            digits, value, int_value, int_value,
            int_value, ltoa(int_value, buffer, base));
}

/**********************************************************************
 *
 *	init
 */
static void init (void)
{
    /* Add default variables. */
    add_var("M_E",		2.71828182845904524);
    add_var("M_LOG2E",	1.44269504088896341);
    add_var("M_LOG10E",	0.434294481903251828);
    add_var("M_LN2",	0.693147180559945309);
    add_var("M_LN10",	2.30258509299404568);
    add_var("M_PI",		3.14159265358979324);
    add_var("M_PI_2",	1.57079632679489662);
    add_var("M_PI_4",	0.785398163397448310);
    add_var("M_1_PI",	0.318309886183790672);
    add_var("M_2_PI",	0.636619772367581343);
    add_var("M_1_SQRTPI",	0.564189583547756287);
    add_var("M_2_SQRTPI",	1.12837916709551257);
    add_var("M_SQRT2",	1.41421356237309505);
    add_var("M_SQRT_2",	0.707106781186547524);
    add_var(ROUND_NAME,	(calc_t)DEFAULT_ROUND);
    add_var(DIGITS_NAME,	(calc_t)DEFAULT_DIGITS);
    add_var(BASE_NAME,	(calc_t)DEFAULT_BASE);
    add_var("TRUE",		1.0);
    add_var("FALSE",	0.0);
    /* sort functions */
    qsort(funcs, NFUNC, sizeof(FUNC), func_sort_cmp);
}

/**********************************************************************
 *
 *	more
 */
static int more (char *str)
{
    register int	key;

    printf(str);
    printf(" --More--");
    key = getch();
    printf("\n");
    if (key == CTRL_C)
        exit(-1);
    return toupper(key) != 'Q';
}

/**********************************************************************
 *
 *	give_help
 */
static void give_help(void)
{
    register int		i;

    printf("\n"
"	CALC - interactive expression evaluator\n\n"
" CALC reads an expression from standard input, evaluates it and prints\n"
" the result to the standard output in floating point, decimal, hexadecimal,\n"
" octal and user defined format. Expression can contain constant numbers in\n"
" C's floating point, decimal, hexadecimal and octal format. CALC treats all\n"
" numbers as C's double type, and conversions to signed long integer type\n"
" are performed when necessary. CALC expressions can also contain normal\n"
" C variables.\n"
);printf(
"\n CALC recognizes most C operators including comma and conditional (?:)\n"
" operators, and all assignment operators. Also some mathematical functions\n"
" are recognized.\n"
" User can control some operations with special variables:\n"
);printf(
"   %s\n"
"     Specifies, if CALC uses rounding when converting doubles to ints\n"
"     instead of truncating. Current mode uses %s.\n"
"   %s\n"
"     Specifies, how many digits are displayed in the floating point format.\n"
"     Current number of digits is %1.*f.\n"
"   %s\n"
"     Specifies the base to be used when displaying in the user defined\n"
"     format. Current base is %d.\n",
    ROUND_NAME, do_round ? "rounding" : "truncating",
    DIGITS_NAME, digits, (float)digits,
    BASE_NAME, base);
    if (!more(""))
        return;
    printf("\n Recognized operators are:\n");
    for (i=0; i < NOPER; i++) {
        if (i && i % 14 == 0)
            printf("\n");
        printf(" %-4s", operators[i]);
    }
    printf("\n\n Recognized functions are:\n");
    for (i=0; i < NFUNC; i++) {
        if (i && i % 10 == 0)
            printf("\n");
        printf(" %-6s", funcs[i].name);
    }
    printf("\n\n"
" Examples: 1 + 2                   displays result 3\n"
"           a = 1.1                 sets to a the value 1.1\n"
"           b = a * a - (8 * 6)     sets to b the result of the expression\n"
"           b                       displays b's value\n"
);printf(
"           b <<= 8                 shifts b 8 places to the left\n"
"           s = sin(a+5)            calculates sin(a+5)\n"
"           digits = 2              specifies CALC to display only 2 digits\n"
"\n");

    if (n_var == 0)
        return;
    if (!more(" List of predefined and user defined variables follows:\n"))
        return;
    for (i=0; i < n_var; i++) {
        printf(" %-10s ", var_list[i].name);
        show_value(var_list[i].value, TRUE);
    }
}

/**********************************************************************
 *
 *	check_command
 *
 * Check for internal commands in the beginning of the command line.
 */
static int check_command (register char *buf)
{
    if (*buf == '?' || stricmp(buf, "help") == 0) {
        give_help();
        return TRUE;
    }
    if (stricmp(buf, "exit") == 0 || stricmp(buf, "ex") == 0
            || stricmp(buf, "quit") == 0
        || stricmp(buf, "stop") == 0 || stricmp(buf, "bye") == 0)
        exit(0);
    return FALSE;
}

/**********************************************************************
 *
 *	main
 */
void main (int argc, char *argv[])
{
    calc_t		result;
    register char	*buf;
    int		opt;
    extern int	optind, opterr;
    extern int	getopt(int, char **, char *);

    init();
    opterr = FALSE;	/* handle errors ourselves */
    while ((opt = getopt(argc, argv, "d")) != EOF) {
        switch (opt) {
            case 'd':
                debug = TRUE;
                break;
            default:
                give_help();
                break;
        }
    }
    printf ("CALC - interactive expression evaluator, %s.\n"
        "Enter expression. Press ^C to quit, ? for help.\n", version);
    for (;;) {	/* error loop */
        if (setjmp(calc_error) != 0) {
            show_error();
            continue;
        }
        for (;;) {	/* command loop */
                printf("> ");
                if ((buf = getline()) == NULL)
                    break;
                while (isspace(*buf))
                    buf++;
            if (*buf == '\0')
                continue;
            if (check_command(buf))
                continue;
            result = eval(buf);
            printf("result: ");
            show_value(result, FALSE);
        }
    }
}
