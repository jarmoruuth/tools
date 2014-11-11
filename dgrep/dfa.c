/**********************************************************************\
 *
 *	DFA.C
 *
 * Constructs deterministic finite state automata from followpos.
 * Because of exponential space requirements on some input (and 64k
 * segmentation in PC !#?@#), a sort of lazy evaluation technique is used. 
 * FIXTRAN first transitions are always calculated and kept in memory. 
 * If more states are needed, they are computed while matching the 
 * expression. Calculated transitions are stored in a cache. If some 
 * transition is not in the fixed transition table and it is not found 
 * from those states currently in cache then one cache state is deleted 
 * and new one is added.
 *
 * Bugs: Things get considerably slower, if more than NTRAN
 *	 states are needed.
 *
 * Author: Jarmo Ruuth 15-Mar-1988
 *
 * Copyright (C) 1988-90 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

#include "system.h"
#include "dfa.h"

/**********************************************************************
 *
 *	init_dfa
 *
 * Allocates state sets and transition table.
 */
static bool init_dfa(void)
{
	REG1 int	i;
	
	for (i = 0; i < NTRAN; i++) {
		if ((rbuf.dstates[i] = calloc(1, sizeof(state_t))) == NULL)
			return FALSE;
		if ((rbuf.dstates[i]->set = calloc(1, rbuf.setsize)) == NULL)
			return FALSE;
#ifdef FAST
		dassert(rbuf.dstates[i] < MAXTRAN);
#endif
	}
	if ((rbuf.tmpstate = malloc(rbuf.setsize)) == NULL)
		return FALSE;
	return TRUE;
}

/**********************************************************************
 *
 *	is_accepting
 *
 * Tests if the given state set is accepting state. State is accepting
 * if right end marker position (at root-1) belongs to it.
 */
#define is_accepting(_state_set) in_set(_state_set, rbuf.root-1)

/**********************************************************************
 *
 *	transition_to
 *
 * Returns the transition to the state specified by its index.
 * In the fast version, it is the state pointer, or otherwise it
 * is index to the state.
 */
#ifdef FAST
#define transition_to(_index) ((tran_t)rbuf.dstates[_index])
#else
#define transition_to(_index) ((tran_t)(_index))
#endif

/**********************************************************************
 *
 *	build_state
 *
 * First builds a new state set to the parameter 'state_set' by taking a
 * union of all those followpositions that belong to the current position
 * 'pos' and that contain 'inp'.
 * Then finds a state that has same set of states as 'state_set' and returns 
 * transition to it. If there isn't equal position in dstates, returns
 * UNKNOWN. If RE_FULL is not specified and we have accepting state, returns
 * transition to the virtual ACCEPTING state.
 */
static tran_t build_state(set_t* state_set, unsigned inp, state_t* state)
{
	REG1 int	i;
	REG2 set_t*	dsset = state->set;
	REG3 node_t*	tptr;
	REG4 int	type;
	
	if (rbuf.ignorecase && islower(inp))
		inp = toupper(inp);
	set_clear(state_set, rbuf.setsize);
	/* build union of followpositions */
	for (i = rbuf.root; i >= 0; i--) {
		if (!in_set(dsset, i))
			continue;
		tptr = &rbuf.tree[i];
		type = tptr->type;
		if ((type == ID && tptr->val.item == inp)
		    || (type == CLASS_ID && in_set(tptr->val.class, inp)))
			set_union(state_set, state_set, rbuf.follow_array[i],
				  rbuf.setsize);
	}
	if (set_empty(state_set, rbuf.setsize))
		return rbuf.default_tran;
	if (!rbuf.full && is_accepting(state_set))
		return ACCEPTING;
	/* try to find state with same set of followpositions */
	for (i = rbuf.last_state; i >= 0; i--)
		if (set_equal(state_set, rbuf.dstates[i]->set, rbuf.setsize))
			return transition_to(i);
	if (is_accepting(state_set))
		return ACCEPTING;
	else
		return UNKNOWN;
}

/**********************************************************************
 *
 *	add_state
 *
 * Adds new state to dstates in position 'state'. Initializes also
 * state transition table in dstates.
 */
static void add_state(set_t* state_set, REG2 state_t* state)
{
	REG1 int	i;
	REG3 unsigned*	langptr;
	
	/* set default transition */
	if (is_accepting(state_set))
	        for (i = 0; i < NLANG; i++)
	                state->tran[i] = ACCEPTING;
	else
	        for (i = 0; i < NLANG; i++)
	                state->tran[i] = rbuf.default_tran;
	state->tran[EOL] = state->tran[rbuf.eol[0]];
	for (i = 0; i < rbuf.neol; i++)
		state->tran[rbuf.eol[i]] = END_OF_LINE;
	if (rbuf.full || !is_accepting(state_set)) {
		i = rbuf.language[0];
		langptr = rbuf.language + i;
		while (i--)
			state->tran[*langptr--] = UNKNOWN;
	}
	set_copy(state->set, state_set, rbuf.setsize);
}

/**********************************************************************
 *
 *	add_transition
 *
 * Adds transition to dtran. Handles also end of line transition if 
 * transition isn't UNKNOWN. Returns transition, which can be changed
 * in the case of end of line transition.
 */
static tran_t add_transition(REG2 state_t* state, REG3 int input,
			     REG1 tran_t tran)
{
	REG4 int	i;

	if (tran != UNKNOWN) {
		for (i = 0; i < rbuf.neol; i++) {
			if (input == rbuf.eol[i]) {
				state->tran[EOL] = tran;
				tran = END_OF_LINE;
				break;
			}
		}
	}
	state->tran[input] = tran;
	if (rbuf.ignorecase && isalpha(input))
		state->tran[CHCASE(input)] = tran;
	return tran;
}

/**********************************************************************
 *
 *	free_dfamem
 *
 * Frees all unnecessary memory.
 */
static void free_dfamem(void)
{
	d_free(&rbuf.tree[rbuf.root].firstpos);
}

#if FIXTRAN > 0		/* don't include unused code */

/**********************************************************************
 *
 *	unmarked
 *
 * Finds next unmarked state.
 */
static state_t* unmarked(void)
{
	REG1 int	i;
	
	for (i = 0; i <= rbuf.last_state; i++)
		if (!rbuf.dstates[i]->marked)
			return rbuf.dstates[i];
	return NULL;
}

/**********************************************************************
 *
 *	build_maxtran
 *
 * Constructs FIXTRAN first transitions to dtran.
 */
static void build_maxtran(void)
{
	REG1 int	input;
	REG2 tran_t	tran;
	REG3 int	i;
	REG4 state_t*	state;
	REG5 int	state_cnt;

	state_cnt = 0;
	while (state_cnt++ < FIXTRAN && (state = unmarked()) != NULL) {
		state->marked = TRUE;
		for (i = 0; i < rbuf.language[0]; i++) {
			input = rbuf.language[i+1];
			if (input == EOL)
				continue;
			tran = build_state(rbuf.tmpstate, input, state);
			if (tran == UNKNOWN && rbuf.last_state < NTRAN-1) {
				rbuf.last_state++;
				tran = transition_to(rbuf.last_state);
				add_state(rbuf.tmpstate, state_of(tran));
				state_of(tran)->marked = FALSE;
			}
			add_transition(state, input, tran);
		}
	}
}

#endif	/* FIXTRAN > 0 */

/**********************************************************************
 *
 *	dfa
 */
bool dfa(void)
{
	dassert(MAXTRAN < UNKNOWN);
	if (!init_dfa()) {
		free_dfamem();
		return FALSE;
	}
	rbuf.last_state = 0;
	rbuf.default_tran = rbuf.search ? FIRST_STATE : STOPSTATE;
	/* create initial state */
	add_state(rbuf.tree[rbuf.root].firstpos, rbuf.dstates[0]);
	rbuf.dstates[0]->marked = FALSE;
#if FIXTRAN > 0
	build_maxtran();
#endif
	free_dfamem();
	return TRUE;
}

#ifdef TEST
static int 	removed_tran_cnt = 0;
#endif

/**********************************************************************
 *
 *	remove_transition
 *
 * Removes all transitions to given state.
 */
static void remove_transition(tran_t tran)
{
	REG3 int	i;
	REG1 int	j;
	REG2 tran_t*	pos;

#ifdef TEST
	++removed_tran_cnt;
#endif
	for (i = 0; i < NTRAN; i++) {
		pos = rbuf.dstates[i]->tran;
		for (j = 0; j < NLANG; j++, pos++) {
		        if (*pos == tran)
		                *pos = UNKNOWN;
		}
	}
}

/**********************************************************************
 *
 *	free_cache_pos
 *
 * Returns free cache position. If cache is full, removes old one
 * and returns it as free position. For each state rbuf.dstates[state].marked
 * is used as a use counter for that state (i.e. how many transitions there
 * are to that state). When cache position is needed, the one with smallest
 * reference count is freed and returned. If there are more than one state with
 * the same reference count then the last one is taken.
 */
static tran_t free_cache_pos(REG4 state_t* state)
{
	REG1 unsigned	i;
	REG2 unsigned	mincnt;
	REG3 unsigned	minpos;
	REG5 int	minstate;
	
	if (rbuf.last_state < NTRAN-1) {
		rbuf.last_state++;
		return transition_to(rbuf.last_state);
	}
	mincnt = ~0; /* == maxint */
	minstate = max(FIXTRAN, 1);	/* never remove state zero */
	/* find the removed state (the one with smallesr reference count) */
	for (i = rbuf.last_state; i >= minstate; i--) {
		if (mincnt > rbuf.dstates[i]->marked
		    && rbuf.dstates[i] != state)
		{
			mincnt = rbuf.dstates[i]->marked;
			minpos = i;
		}
	}
	rbuf.dstates[minpos]->marked = 1;
	remove_transition(transition_to(minpos));
	return transition_to(minpos);
}

/**********************************************************************
 *
 *	get_transition
 *
 * Gets real transition for UNKNOWN transition. This function is called
 * when matching the pattern.
 */
tran_t get_transition(REG2 tran_t tran, REG3 unsigned input)
{
	REG1 tran_t	new_tran;

	new_tran = build_state(rbuf.tmpstate, input, state_of(tran));
	if (new_tran == UNKNOWN) {
		/* new state is not one of the old states */
		new_tran = free_cache_pos(state_of(tran));
		add_state(rbuf.tmpstate, state_of(new_tran));
	} else if (new_tran < MAXTRAN)
		state_of(new_tran)->marked++;
	return add_transition(state_of(tran), input, new_tran);
}

#ifdef TEST

/**********************************************************************
 *
 *	show_dfa_report
 */
void show_dfa_report(void)
{
	int	i, j;
	long	ntran, utran;
	
	ntran = utran = 0L;
	printf ("%u states of %u possible states used\n"
		"%u cache states deleted\n",
		rbuf.last_state+1,
		NTRAN,
		removed_tran_cnt);
	for (i = 0; i <= rbuf.last_state; i++)
		for (j = 0; j < rbuf.language[0]; j++)
			if (rbuf.dstates[i]->tran[rbuf.language[j+1]] == UNKNOWN)
				utran++;
	ntran = (rbuf.last_state+1)*rbuf.language[0];
	printf("%lu transitions of %lu possible unknown transitions (%lu%%) are unknown\n",
		utran, ntran, ntran == 0 ? 0 : utran*100/ntran);
#ifdef __TURBOC__
	printf("%lu bytes memory free\n", (unsigned long)coreleft());
#endif
	fflush(stdout);
}
		
/**********************************************************************
 *
 *	print_accepting_states
 */
void print_accepting_states(void)
{
	REG1 int	i;
	
	printf("Accepting states: ");
	for (i = 0; i <= rbuf.last_state; i++)
		if (is_accepting(rbuf.dstates[i]->set))
			printf("%d ", i);
	printf("\n");
	fflush(stdout);
}

/**********************************************************************
 *
 *	char_image
 */
char* char_image(int c)
{
	static char	s[3];
	char*		s1;
	
	switch (c) {
		case REM:
			s1 = "REM";
			break;
		case EOL:
			s1 = "EOL";
			break;
		case BOL:
			s1 = "BOL";
			break;
		default:
			if (iscntrl(c)) {
				s[0] = '^';
				s[1] = '@' + c;
				s[2] = '\0';
			} else {
				s[0] = c;
				s[1] = '\0';
			}
			s1 = s;
			break;
	}
	return s1;
}

/**********************************************************************
 *
 *	print_language
 */
void print_language(void)
{
	int	i;

	printf("    ");
	for (i = 0; i < rbuf.language[0]; i++)
		printf("%3s ", char_image(rbuf.language[i+1]));
	printf("\n");
	fflush(stdout);
}

/**********************************************************************
 *
 *	show_tran
 */
static void show_tran(tran_t t)
{
	int i;

	if (t == ACCEPTING)
		printf("%3s ", "Acc");
	else if (t == STOPSTATE)
		printf("%3s ", "Sto");
	else if (t == END_OF_LINE)
		printf("%3s ", "Eol");
	else if (t == UNKNOWN)
		printf("%3s ", "Unk");
	else {
#ifdef FAST
		/* find index of the t */
		for (i = 0; i < NTRAN; i++)
			if (rbuf.dstates[i] == t)
				break;
		assert(i < NTRAN);
#else
		i = (int)t;
#endif
		printf("%3d ", i);
	}
	fflush(stdout);
}

/**********************************************************************
 *
 *	print_dtran
 */
void print_dtran(void)
{
	int	i, j;
	
	printf("Dstates for each state:\n");
	for (i = 0; i <= rbuf.last_state; i++) {
		printf("%2d:", i);
		for (j = 0; j <= rbuf.root; j++)
			if (in_set(rbuf.dstates[i]->set, j))
				printf(" %d", j);
		printf("\n");
	}
	printf("Transition table (default transition is ");
	show_tran(rbuf.default_tran);
	printf("):\n");
	print_language();
	for (i = 0; i <= rbuf.last_state; i++) {
		printf("%2d: ", i);
		for (j = 0; j < rbuf.language[0]; j++)
			show_tran(rbuf.dstates[i]->tran[rbuf.language[j+1]]);
		printf("\n");
	}
	fflush(stdout);
}

#endif /* TEST */
