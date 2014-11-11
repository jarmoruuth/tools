/**********************************************************************\
 *
 *	DFA.H
 *
 * Definitions for deterministic finite state automata.
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include "set.h"

/* Definitions for different nodes in syntax tree. */

typedef enum {
	CAT,		/* cat node */
	OR,		/* or node */
	CLOSURE,	/* closure node */
	ID,		/* id node, leaf */
	EMPTY_ID,	/* id node, empty leaf */
	CLASS_ID	/* id node, character class leaf */
} node_type_t;
	
/* tree node structure */
typedef struct {
	int	left;
	int	right;
} next_node_t; 

/* union for possible values of tree node */
typedef union {
	unsigned	item;
	set_t*		class;
	next_node_t	next;
} node_value_t;

/* structure for tree node  */
typedef struct {
	node_type_t	type;
	node_value_t	val;
	unsigned	nullable;
	set_t*		firstpos;
	set_t*		lastpos;
} node_t;

#define BOL		NCHARS		/* beginning of line */
#define EOL		(NCHARS+1)	/* end of line */
#define REM		(NCHARS+2)	/* right-end marker */

#define NLANG		(REM+1)		/* characters in recognized language */

#ifdef TEST

/*
	In test version use large FIXTRAN, because it is easier to
	debug transition tables when they are fully done.
*/
#define FIXTRAN		30
#define CACHETRAN	10
#define NTRAN		(FIXTRAN+CACHETRAN)

#else

/*
	If tran_t is a 8 bit char, then 40 states reserves 10 kb. In the fast
	version, with 16 bit pointers (PC small model) it is 20 kb and with 32 
	bit pointers (PC large model, most Unix systems) 40 kb.
*/
#define FIXTRAN		0		/* number of fixed transition tables */
#define CACHETRAN	40
#define NTRAN		(FIXTRAN+CACHETRAN)

#endif

/*
	Special states. Note that the number of special states has
	effect on the tran_t type in the below. In the fast version
	we rely on the fact that none of the transition table pointers
	will be allocated from the memory at addresses 0xfffb - 0xffff.
	Also we rely on the fact that the tran_t type conversion will
	make these values actually very large unsigned values, so that
	they will be larger than the last pointer allocated to the 
	rbuf.dstates.
*/
#define NSPECIAL	4


#define	UNKNOWN		(tran_t)-1	/* transition not known */
#define END_OF_LINE	(tran_t)-2	/* used to detect line boundaries */
#define STOPSTATE	(tran_t)-3	/* stops matching */
#define ACCEPTING	(tran_t)-4	/* real transition would be to the */
					/* accepting state */
#define MAXTRAN		ACCEPTING	/* smallest special transition */

/* forward definition needed in FAST case */
typedef struct statestruct state_t;

#ifdef FAST

/*
	Fast version stores direct pointers to other states to the
	transition tables. It makes the reg_exec faster by removing
	memory accesses.
*/
typedef state_t*	tran_t;

#define FIRST_STATE	rbuf.dstates[0]

#else

/*
	Normally, transition table contains only index of the next state.
	If possible, we use characters for indexing to save space. Note
	that the tran_t must be some unsigned type (see comment of the
	special transitions).
*/
#if (NTRAN+NSPECIAL > MAXCHAR)
typedef	unsigned short	tran_t;
#else
typedef	unsigned char	tran_t;
#endif

#define FIRST_STATE	0

#endif

/* Structure for state positions and transition tables. */
struct statestruct {
	tran_t		tran[NLANG];	/* transition tables */
	set_t*		set;		/* state followpositions */
	unsigned	marked;
};

/* regular expression buffer structure */
typedef struct {
	state_t*	dstates[NTRAN];	/* dfa states */
	set_t**		follow_array;	/* followpositions */
	int		setsize;	/* position set size */
	int		last_state;	/* last used state in dstates */
	tran_t		default_tran;	/* default transition */
	int		root;		/* tree root position */
	node_t*		tree;		/* syntax tree for regexp */
	uint*		language;	/* characters in language */
	char*		eol;		/* end of line characters */
	char		neol;		/* number of end of line characters */
	char*		regmust;	/* regmust, if any */
	char*		reg_error;	/* error message, if any */
	char		ignorecase;	/* flag to ignore case */
	char		search;		/* are we building substring searcher*/
	char		full;		/* must we get longest possible match*/
	char		mem_allocated;	/* is there allocated memory in rbuf */
	char		begline;	/* can we match beginning of line */
	char		endline;	/* can we match end of line */
	set_t*		tmpstate;	/* temporary working space */
} regbuf_t;

extern regbuf_t rbuf;
	
extern int create_tree(uchar* str);
extern bool find_regmust(node_t* tree, int root);
extern bool calcpos(void);
extern bool dfa(void);
extern tran_t get_transition(tran_t tran, unsigned input);

/**********************************************************************
 *
 *	state_of
 *
 * Returns the state structure specified by the transition.
 * In the FAST version it is the same as the transition, otherwise
 * it is the state pointer indexed by the transition.
 */
#ifdef FAST
#define state_of(_tran)	(_tran)
#else
#define state_of(_tran)	(rbuf.dstates[_tran])
#endif
