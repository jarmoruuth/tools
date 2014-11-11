/**********************************************************************\
 *
 *	DFATREE.C
 *
 * Creates a syntax tree from the regular expression.
 *
 * Before parsing the regular expression is changed from the character
 * array to an integer array and also changed to a simpler syntax. Same 
 * process does also lexical analyzing so that special symbols are in 
 * the higher byte and normal characters in the lower byte.
 *
 * Parser is a recursive descent parser that builds the syntax tree.
 *
 * Parser syntax:
 *
 * primary-expression:
 *	constant
 *	character-class
 *	empty
 *	( expression )
 *
 * closure-expression:
 *	primary-expression
 *	closure-expression primary-expression
 *
 * cat-expression:
 *	closure-expression
 *	cat-expression closure-expression
 *
 * or-expression:
 *	cat-expression
 *	or-expression cat-expression
 *
 * expression: 
 *	or-expression
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include "system.h"
#include "dfa.h"

#define NCLASSITEMS	NLANG
#define REGBLOCK	10
#define NEGATE_CLASS	'^'

#define EMPTY		( '@' << CHARBITS )	/* epsilon */
#define CLASS		( '[' << CHARBITS )
#define	L_PAR		( '(' << CHARBITS )
#define	R_PAR		( ')' << CHARBITS )
#define	CLOSURE_OP	( '*' << CHARBITS )
#define	OR_OP		( '|' << CHARBITS )
#define BOL_OP		( '^' << CHARBITS )
#define EOL_OP		( '$' << CHARBITS )
#define	EOS		0

static node_t*	treeptr;	/* current syntax tree pointer */
static node_t*	treestart;	/* pointer to start of the syntax tree */
static int*	regexp;		/* modified regular expression */
static int*	regptr;		/* current pointer to regexp */
static int	memerr;		/* flag for out of memory error */
static int	current;	/* current regexp token type */
static int	next;		/* next regexp token type */
static int	pos;		/* position in syntax tree */
static int	regsize;	/* allocated size for regexp */
static int	regpos;		/* index to regexp */
static int	last_class;	/* last character class in classes */
static set_t**	classes;	/* array of character classes for regexp */
static uchar*	language;	/* boolean array of characters in language */

static int expression (void);

/**********************************************************************
 *
 *	CLEAR_LANGUAGE
 */
#ifdef BSD
#define CLEAR_LANGUAGE(l)	bzero(l, NLANG)
#else
#define CLEAR_LANGUAGE(l)	memset(l, FALSE, NLANG)
#endif

/**********************************************************************
 *
 *	tree_error
 */
static int tree_error(char* msg)
{
	current = next = pos = ERROR;	/* stop parsing */
	rbuf.reg_error = msg;
	return ERROR;
}

/**********************************************************************
 *
 *	new_pos
 *
 * Gets new tree node and sets pos to point to it.
 */
static int new_pos(void)	
{
	++pos;
	if (pos == 0) {
		if ((treestart = malloc(sizeof(node_t))) == NULL)
			return tree_error("Out of memory");
		treeptr = treestart;
	} else {
		if ((treestart=realloc(treestart,(pos+1)*sizeof(node_t)))==NULL)
			return tree_error("Out of memory");
		treeptr = treestart + pos;
	}
	return TRUE;
}

/**********************************************************************
 *
 *	add_language
 *
 * Adds a new character to the recognized language.
 */
static void add_language(int ch)
{
	language[ch] = TRUE;
	if (rbuf.ignorecase && isalpha(ch))
		language[CHCASE(ch)] = TRUE;
}

/**********************************************************************
 *
 *	add_item
 *
 * Adds a new leaf item to the syntax tree and returns tree position.
 */
static int add_item(node_type_t type, unsigned value)
{
	if (new_pos() == ERROR)
		return ERROR;
	treeptr->type = type;
	treeptr->val.item = value;
	if (type != EMPTY_ID)
		add_language(value);
	return pos;
}

/**********************************************************************
 *
 *	add_eol
 *
 * Add end of line character class to the regexp.
 */
static int add_eol(void)
{
	int	i;
	set_t*	class;
	
	if ((class = malloc(set_bytesize(NCLASSITEMS))) == NULL) {
		memerr = TRUE;
		return(ERROR);
	}
	set_clear(class, set_bytesize(NCLASSITEMS));
	
	for (i = 0; i < rbuf.neol; i++) {
		if (!language[rbuf.eol[i]])
			add_language(rbuf.eol[i]);
		add_set(class, rbuf.eol[i]);
	}
	
	if (new_pos() == ERROR)
		return(ERROR);
	treeptr->type = CLASS_ID;
	treeptr->val.class = class;
	return(pos);
}

/**********************************************************************
 *
 *	add_node
 *
 * Adds a new node to the syntax tree and returns tree position.
 */
static int add_node(node_type_t type, int left, int right)
{
	if (new_pos() == ERROR)
		return ERROR;
	treeptr->type = type;
	treeptr->val.next.left = left;
	treeptr->val.next.right = right;
	return pos;
}

/**********************************************************************
 *
 *	add_regexp
 *
 * Adds a new item to the modified regular expression.
 */
static void add_regexp(int item)
{
	if (regexp == NULL) {
		regpos = -1;
		regsize = REGBLOCK;
		if ((regexp = malloc(regsize*sizeof(int))) == NULL)
			memerr = TRUE;
	}
	if (regpos == regsize-1) {
		regsize += REGBLOCK;
		if ((regexp = realloc(regexp, regsize*sizeof(int))) == NULL)
			memerr = TRUE;
	}
	++regpos;
	if  (!(item & ~MAXCHAR) && rbuf.ignorecase && islower(item))
		regexp[regpos] = toupper(item);
	else
		regexp[regpos] = item;
}

/**********************************************************************
 *
 *	block_beg
 *
 * Finds the beginning of a possible block that starts from regpos.
 */
static int block_beg(int regpos)
{
	REG1 int	i, cnt;
	
	if (regexp[regpos] != R_PAR)
		return regpos;
	for (cnt = 1, i = regpos-1; i >= 0 && cnt; i--) {
		if (regexp[i] == L_PAR)
			--cnt;
		if (regexp[i] == R_PAR)
			++cnt;
	}
	return i+1;
}

/**********************************************************************
 *
 *	copy_class
 *
 * Copies class to the classes array. We can't use the same class twice, 
 * because each class is released by calling free().
 */
static int copy_class(set_t *class)
{
	REG1 int	bsize;
	REG2 set_t	*tmpclass;
	
	bsize = set_bytesize(NCLASSITEMS);
	if ((tmpclass = malloc(bsize)) == NULL) {
		memerr = TRUE;
		return 0;
	}
	set_copy(tmpclass, class, bsize);
	if ((classes=realloc(classes,(++last_class+1)*sizeof(set_t *)))==NULL) {
		memerr = TRUE;
		return 0;
	}
	classes[last_class] = tmpclass;
	return last_class;
}
	
/**********************************************************************
 *
 *	add_one_or_more
 *
 * Changes r+ to rr*.
 */
static void add_one_or_more(void)
{
	REG1 int	i, pos;

	for (i = block_beg(regpos), pos = regpos; i <= pos; i++)
		add_regexp(regexp[i]);
	for (i = pos+1, pos = regpos; i <= pos; i++)
		if (regexp[i] == CLASS)
		    	regexp[i+1] = copy_class(classes[regexp[i+1]]);
	add_regexp(CLOSURE_OP);
}

/**********************************************************************
 *
 *	add_left_paren
 *
 * Finds previous block in regexp and adds left parethesis before it.
 */
static void add_left_paren(int regpos)
{
	REG1 int	i, pos, save;

	save = regexp[regpos];
	pos = block_beg(regpos);
	for (i = regpos; i > pos; i--)
		regexp[i] = regexp[i-1];
	regexp[pos] = L_PAR;
	add_regexp(save);
}

/**********************************************************************
 *
 *	add_zero_or_one
 *
 * Changes r? to (r|î).
 */
static void add_zero_or_one(void)
{
	add_left_paren(regpos);
	add_regexp(OR_OP);
	add_regexp(EMPTY);
	add_regexp(R_PAR);
}

/**********************************************************************
 *
 *	add_class
 *
 * Adds character class to regexp. Classes are added to regexp so that
 * it is surrounded by parenthesis and it contains CLASS marker followed
 * by an integer that is class index to the classes array.
 */
static void add_class(REG1 uchar* class)
{
	REG2 int	i;
	
	for (i = 0; i < rbuf.neol; i++)
		class[rbuf.eol[i]] = FALSE;
	class[EOL] = class[REM] = FALSE;
	if (classes == NULL) {
		if ((classes = malloc(sizeof(set_t *))) == NULL) {
			memerr = TRUE;
			return;
		}
		last_class = 0;
	} else {
		if ((classes=realloc(classes,(++last_class+1)*sizeof(set_t *)))
		     == NULL) {
			memerr = TRUE;
			return;
		}
	}
	if ((classes[last_class] = malloc(set_bytesize(NCLASSITEMS))) == NULL) {
		memerr = TRUE;
		return;
	}
	set_clear(classes[last_class], set_bytesize(NCLASSITEMS));
	for (i = 0; i < NCLASSITEMS; i++)
		if (class[i]) {
			if (!language[i])
				add_language(i);
			add_set(classes[last_class], i);
		}
	if (set_empty(classes[last_class], set_bytesize(NCLASSITEMS)))
		add_regexp(EMPTY);
	else {
		add_regexp(L_PAR);
		add_regexp(CLASS);
		add_regexp(last_class);
		add_regexp(R_PAR);
	}
}

/**********************************************************************
 *
 *	init_class
 */
#define init_class(c, v)	memset(c, v, NCLASSITEMS);

/**********************************************************************
 *
 *	get_class
 *
 * Collects class characters to boolean array indexed by characters
 * and adds it to regexp.
 */
static uchar* get_class(REG3 uchar* str)
{
	REG1 int	i, end_range;
	REG4 int	neg = FALSE;
	uchar		class[NCLASSITEMS];

	init_class(class, FALSE);
	++str;	/* skip starting '[' */
	if (*str == NEGATE_CLASS) {
		++str;
		neg = TRUE;
	}
	if (*str == ']') {
		++str;
		class[']'] = TRUE;
	}
	for (; *str && *str != ']'; str++) {
		if (*str == '\\' && !*(++str))
			return NULL;
		if (*(str+1) == '-' && *(str+2) != ']') {
			i = *str;
			str +=  *(str+2) == '\\' ? 3 : 2;
			if ((end_range = *str) == '\0')
				return NULL;
			if (end_range < i) {
				int tmp = i;
				i = end_range;
				end_range = tmp;
			}
			for (; i <= end_range; i++)
				class[i] = TRUE;
			continue;
		}
		class[*str] = TRUE;
	}
	if (rbuf.ignorecase) {
		for (i = 1; i < NCHARS; i++)
			if (isalpha(i))
				class[i] |= class[CHCASE(i)];
	}
	if (neg)
		for (i = 1; i < NCLASSITEMS; i++)
			class[i] = !class[i];
	return *str == ']' ? add_class(class),str : NULL;
}

/**********************************************************************
 *
 *	add_any
 *
 * Adds a character class to the regexp that matches any character except
 * end of line.
 */
static void add_any(void)
{
	int	i;
	uchar	class[NCLASSITEMS];

	init_class(class, TRUE);
	for (i = 0; i < rbuf.neol; i++)
		class[rbuf.eol[i]] = FALSE;
	class[BOL] = class[EOL] = FALSE;
	add_class(class);
}

/**********************************************************************
 *
 *	add_search_init
 *
 * Adds [0-MAXCHAR]*( to the beginning of the regexp if we are building
 * a substring searcher. Note, that no characters from here are added 
 * to the language.
 */
static void add_search_init(void)
{
	uchar	class[NCLASSITEMS];
	
	init_class(class, TRUE);
	add_class(class);
	add_regexp(CLOSURE_OP);
	add_regexp(L_PAR);
}

/**********************************************************************
 *
 *	make_regexp
 *
 * Changes regular expression to simpler syntax (uses only '(', ')', '|',
 * '*' and 'î'). Integer array is used, so full eight bit character set
 * except '\0' can be used.
 */
static char* make_regexp(REG1 uchar* str)
{
	REG2 uchar*	strbeg = str;
	REG3 int	paren_cnt = 0;
	
	regexp = NULL;
	/* Check special cases and make sure, that they match to every line. */
	/* Is this really necessary and correct ? */
	if (((*str == '$' || *str == '^') && *(str+1) == '\0')
	    || *str == '\0')
		str = "a*";
	/* If we are building a substring searcher, change regexp r to form
	   [0-MAXCHAR]*(r). It guarantees, that substring is found in 
	   O(n+m)-time.
	*/
	memerr = FALSE;
	if (rbuf.search)
		add_search_init();
	CLEAR_LANGUAGE(language);
	rbuf.begline = rbuf.endline = FALSE;
	for (; *str && !memerr; str++) {
		switch (*str) {
			case '\\':
				if (*(str+1) != '\0')
					add_regexp(*(++str));
				else
					add_regexp(*str);
				break;
			case '(':
				if (*(str+1) == ')')
					++str;
				else if (*(str+1) == '\0')
					add_regexp(*str);
				else {
					add_regexp(L_PAR);
					++paren_cnt;
				}
				break;
			case ')':
				--paren_cnt;
				add_regexp(R_PAR);
				break;
			case '[':
				str = get_class(str);
				if (str == NULL)
					return "Unterminated character class";
				break;
			case ']':
				add_regexp(*str);
				break;
			case '*':
				if (str != strbeg)
					add_regexp(CLOSURE_OP);
				else
					add_regexp(*str);
				break;
			case '?':
				if (str != strbeg)
					add_zero_or_one();
				else
					add_regexp(*str);
				continue;
			case '+':
				if (str != strbeg)
					add_one_or_more();
				else
					add_regexp(*str);
				break;
			case '.':
				add_any();
				break;
			case '|':
				if (str == strbeg || *(str+1) == '\0')
					add_regexp(*str);
				else if (*(str-1) == '(') {
					add_regexp(EMPTY);
					add_regexp(OR_OP);
				} else if (*(str+1) == ')') {
					add_regexp(OR_OP);
					add_regexp(EMPTY);
				} else
					add_regexp(OR_OP);
				break;
			case '^':
				rbuf.begline = TRUE;
				add_regexp(BOL_OP);
				break;
			case '$':
				rbuf.endline = TRUE;
				add_regexp(EOL_OP);
				break;
			default:
				if (IS_EOL(*str))
					return  "End of line character "
						"in regular expression";
				add_regexp(*str);
				break;
		}
	}
	/* Finish L_PAR started at add_search_init() */
	if (rbuf.search)
		add_regexp(R_PAR);
	add_regexp(EOS);
	if (memerr)
		return "Out of memory";
	return paren_cnt ? "Unbalanced parenthesis" : NULL;
}

/**********************************************************************
 *
 *	fetch_next
 */
#define fetch_next() \
{ \
	current = next; \
	next = *(++regptr); \
}

/**********************************************************************
 *
 *	fetch_new
 */
#define fetch_new() \
{ \
	current = *(++regptr); \
	next = *(++regptr); \
}

/**********************************************************************
 *
 *	primary_exp
 */
static int primary_exp(void)
{
	REG1 int	last_pos;
	
	switch (current) {
		case L_PAR:
			fetch_next();		/* skip L_PAR */
			last_pos = expression();
			if (next != R_PAR)
				return tree_error("Error in expression");
			fetch_next();		/* skip R_PAR */
			return last_pos;
		case R_PAR:
		case CLOSURE_OP:
		case OR_OP:
			return tree_error("Error in expression");
		case EOS:
			return tree_error("Premature end of expression");
		case CLASS:
			fetch_next();
			if (new_pos() == ERROR)
				return ERROR;
			treeptr->type = CLASS_ID;
			treeptr->val.class = classes[current];
			return pos;
		case EMPTY:
			return add_item(EMPTY_ID, 0);
		case EOL_OP:
			add_language(EOL);
			return add_eol();
		case BOL_OP:
			return add_item(ID, BOL);
		case ERROR:
			return ERROR;
		default:
			return add_item(ID, current);
	}
}

/**********************************************************************
 *
 *	closure_exp
 */
static int closure_exp(void)
{
	REG1 int	left;

	left = primary_exp();
	switch (next) {
		case CLOSURE_OP:
			fetch_next();
			return add_node(CLOSURE, left, 0);
		default:
			return left;
	}
}

/**********************************************************************
 *
 *	cat_exp
 */
static int cat_exp(void)
{
	REG1 int	left, right;
	
	left = closure_exp();
	for (;;) {
		switch (next) {
			case R_PAR:
			case CLOSURE_OP:
			case OR_OP:
			case EOS:
			case ERROR:
				return left;
			case L_PAR:
			default:
				fetch_next();
				right = closure_exp();
				if (right == ERROR)
					return ERROR;
				if (add_node(CAT, left, right) == ERROR)
					return ERROR;
				left = pos;
				continue;
		}
	}
}

/**********************************************************************
 *
 *	expression
 */
static int expression(void)
{
	REG1 int	left, right;

	left = cat_exp();
	for (;;) {
		switch (next) {
			case OR_OP:
				fetch_new();
				right = cat_exp();
				if (right == ERROR)
					return ERROR;
				if (add_node(OR, left, right) == ERROR)
					return ERROR;
				left = pos;
				continue;
			default:
				return left;
		}
	}
}

/**********************************************************************
 *
 *	add_endmarker
 *
 * Adds unique right-end marker to the regular expression.
 */
static int add_endmarker(REG1 int left)
{
	REG2 int	right;

	if (add_item(ID, REM) == ERROR)
		return ERROR;
	/* catenate original expression and right-end marker */
	right = pos;
	if (add_node(CAT, left, right) == ERROR)
		return ERROR;
	return TRUE;
}

/**********************************************************************
 *
 *	convert_language
 *
 * Converts language from boolean array to a list of those characters 
 * that belong to the language. Rbuf.language[0] is number of characters
 * in language.
 */
static bool convert_language(void)
{
	REG1 unsigned	i, j;
	
	/* calculate number of items in language */
	for (i = j = 0; i < NLANG; i++)
		if (language[i])
			j++;
	/* allocate buffer for language items */
	if ((rbuf.language = malloc((j+1)*sizeof(rbuf.language[0]))) == NULL)
		return FALSE;
	/* Copy language elements to the rbuf. Note that the zeroth element
	   contains the number of characters in language. */
	rbuf.language[0] = j;
	for (i = 0, j = 1; i < NLANG; i++)
		if (language[i])
			rbuf.language[j++] = i;
	return TRUE;
}
	
#ifdef TEST

#if defined(__TURBOC__) || defined(MSDOS)
#define EMPTYCH	0xee
#else
#define EMPTYCH	'@'
#endif

extern int debug;

/**********************************************************************
 *
 *	print_regexp
 */
static void print_regexp(void)
{
	int* ptr;
	
	for (ptr = regexp; *ptr; ptr++) {
		switch (*ptr) {
			case L_PAR:
				putchar('(');
				break;
			case R_PAR:
				putchar(')');
				break;
			case CLOSURE_OP:
				putchar('*');
				break;
			case OR_OP:
				putchar('|');
				break;
			case EMPTY:
				putchar(EMPTYCH);
				break;
			case BOL_OP:
				putchar('^');
				break;
			case EOL_OP:
				putchar('$');
				break;
			case CLASS:
				putchar('[');
				++ptr;
				printf("%d", *ptr);
				putchar(']');
				break;
			default:
				putchar(*ptr);
				break;
		}
	}
	putchar('\n');
}		
#endif /* TEST */

/**********************************************************************
 *
 *	free_treemem
 *
 * Releases all unnecessary memory.
 */
static void free_treemem(void)
{
	d_free(&language);
	d_free(&classes);
	d_free(&regexp);
}	

/**********************************************************************
 *
 *	error_return
 */
static int error_return(REG1 char* msg)
{
	d_free(&treestart);
	free_treemem();
	if (msg)
		rbuf.reg_error = msg;
	else if (!rbuf.reg_error)
		rbuf.reg_error = "Error in expression";
	return ERROR;
}

/**********************************************************************
 *
 *	init_tree
 */
static char* init_tree(void)
{
	classes = NULL;
	treestart = treeptr = rbuf.tree = NULL;
	rbuf.root = -1;
	regexp = NULL;
	if ((language = malloc(NLANG)) == NULL)
		return "Out of memory";
	return NULL;
}

/**********************************************************************
 *
 *	create_tree
 *
 * Creates syntax tree from given regular expression. Returns root
 * of the tree or -1, if there was errors.
 */
int create_tree(REG2 uchar* str)
{
	REG1 int	left;
	REG3 char*	msg;
	
	if ((msg = init_tree()) != NULL)
		return error_return(msg);
	if ((msg = make_regexp(str)) != NULL)
		return error_return(msg);
#ifdef TEST
	if (debug)
		print_regexp();
#endif
	regptr = regexp;
	current = *regptr;
	next = *(++regptr);
	pos = -1;
	left = expression();
	if (left == ERROR || next != EOS)
		return error_return(NULL);
	if (add_endmarker(left) == ERROR)
		return error_return(NULL);
	if (!convert_language())
		return error_return("Out of memory");
	free_treemem();
	rbuf.tree = treestart;
	rbuf.root = pos;
	return pos;
}
