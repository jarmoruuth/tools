/**********************************************************************\
 *
 *	RBTREE.C
 *
 * Red-Black Binary Tree. Algorithm is from R.Sedgewick: Algorithms.
 *
\**********************************************************************/

#include <stdlib.h>

#include <c.h>

#include "lassert.h"
#include "rbtree.h"

static rbtree_node_t rbnode_z = {NULL, FALSE, &rbnode_z, &rbnode_z};

/**********************************************************************
 *	rbtree_rotate
 */
static rbtree_node_t* rbtree_rotate(
			rbtree_t*      rbt,
			void*          item,
			rbtree_node_t* y)
{
	rbtree_node_t* s;
	rbtree_node_t* gs;
	
	if ((*rbt->rbt_compare)(item, y->rbn_item) < 0)
		s = y->rbn_left;
	else
		s = y->rbn_right;

	if ((*rbt->rbt_compare)(item, s->rbn_item) < 0) {
		gs = s->rbn_left;
		s->rbn_left = gs->rbn_right;
		gs->rbn_right = s;
	} else {
		gs = s->rbn_right;
		s->rbn_right = gs->rbn_left;
		gs->rbn_left = s;
	}

	if ((*rbt->rbt_compare)(item, y->rbn_item) < 0)
		y->rbn_left = gs;
	else
		y->rbn_right = gs;

	return(gs);
}

/**********************************************************************
 *	rbtree_split
 */
static rbtree_node_t* rbtree_split(
			rbtree_t*      rbt,
			void*          item,
			rbtree_node_t* gg,
			rbtree_node_t* g,
			rbtree_node_t* f,
			rbtree_node_t* x)
{
	x->rbn_red = TRUE;
	x->rbn_left->rbn_red = FALSE;
	x->rbn_right->rbn_red = FALSE;

	if (f->rbn_red) {
		g->rbn_red = TRUE;
		if (((*rbt->rbt_compare)(item, g->rbn_item) < 0) !=
		    ((*rbt->rbt_compare)(item, f->rbn_item) < 0))
			f = rbtree_rotate(rbt, item, g);
		x = rbtree_rotate(rbt, item, gg);
		x->rbn_red = FALSE;
	}
	
	rbt->rbt_head.rbn_right->rbn_red = FALSE;
	
	return(x);
}

/**********************************************************************
 *	rbtree_insert
 */
bool rbtree_insert(
	rbtree_t* rbt,
	void*     item)
{
	int            cmp;
	rbtree_node_t* gg;
	rbtree_node_t* g;
	rbtree_node_t* f;
	rbtree_node_t* x;

	rbt->rbt_head.rbn_item = item;
	x = &rbt->rbt_head;
	f = x;
	g = x;
	
	do {
		gg = g;
		g = f;
		f = x;
		cmp = (*rbt->rbt_compare)(item, x->rbn_item);
		if (cmp < 0)
			x = x->rbn_left;
		else
			x = x->rbn_right;
		if (x->rbn_left->rbn_red && x->rbn_right->rbn_red)
			x = rbtree_split(rbt, item, gg, g, f, x);
	} while (x != &rbnode_z);
	
	if (f == &rbt->rbt_head)  /* empty tree */
		cmp = -1;
	
	x = malloc(sizeof(rbtree_node_t));
	lassert(x != NULL);
	x->rbn_item = item;
	x->rbn_left = &rbnode_z;
	x->rbn_right = &rbnode_z;
	
	if ((*rbt->rbt_compare)(item, f->rbn_item) < 0)
		f->rbn_left = x;
	else
		f->rbn_right = x;
	
	rbtree_split(rbt, item, gg, g, f, x);
	
	return(cmp != 0);
}

/**********************************************************************
 *	rbtree_delete
 */
bool rbtree_delete(
	rbtree_t* rbt,
	void*     item)
{
	lerror;
	return(0);
}

/**********************************************************************
 *	rbtree_search
 */
bool rbtree_search(
	rbtree_t* rbt,
	void*     item,
	void*     found_item)
{
	int            cmp;
	rbtree_node_t* x;
	
	rbnode_z.rbn_item = item;
	x = rbt->rbt_head.rbn_right;
	
	for (;;) {
		cmp = (*rbt->rbt_compare)(item, x->rbn_item);
		if (cmp == 0)
			break;
		else if (cmp < 0)
			x = x->rbn_left;
		else
			x = x->rbn_right;
	}
	
	if (x == &rbnode_z) {
		return(FALSE);
	} else {
		if (found_item != NULL)
			*(char**)found_item = x->rbn_item;
		return(TRUE);
	}
}

/**********************************************************************
 *	rbtree_create
 */
rbtree_t* rbtree_create(
		int  (*compare)(void*, void*),
		void (*delete)(void*))
{
	rbtree_t* rbt;
	
	rbt = malloc(sizeof(rbtree_t));
	lassert(rbt != NULL);
	
	rbt->rbt_head.rbn_red = FALSE;
	rbt->rbt_head.rbn_left = &rbnode_z;
	rbt->rbt_head.rbn_right = &rbnode_z;
	rbt->rbt_compare = compare;
	rbt->rbt_delete = delete;
	
	return(rbt);
}

/**********************************************************************
 *	rbtree_delete_all
 */
static void rbtree_delete_all(
		rbtree_t*      rbt,
		rbtree_node_t* x)
{
	if (x != &rbnode_z) {
		rbtree_delete_all(rbt, x->rbn_left);
		rbtree_delete_all(rbt, x->rbn_right);
		if (rbt->rbt_delete != NULL)
			(*rbt->rbt_delete)(x->rbn_item);
		free(x);
	}
}

/**********************************************************************
 *	rbtree_destroy
 */
void rbtree_destroy(rbtree_t* rbt)
{
	rbtree_delete_all(rbt, rbt->rbt_head.rbn_right);
	rbt->rbt_head.rbn_right = &rbnode_z;
}

#ifdef TEST

#include <stdio.h>
#include <lrand.h>

static ulong* array;
static int  ndelete;
static int cnt = 1000;

static int testcmp(void* v1, void* v2)
{
	long cmp = (ulong)v1 - (ulong)v2;
	
	if (cmp < 0)
		return(-1);
	else if (cmp == 0)
		return(0);
	else
		return(1);
}

static void testdel(void* v)
{
	ndelete++;
}

static void insert_forever(void)
{
	ulong i, value;
	rbtree_t* rbt;

	puts("Create");	
	rbt = rbtree_create(testcmp, testdel);
	lassert(rbt != NULL);

	puts("Insert");
	for (i = 0; ; i++) {
		if (i % 100 == 0)
			printf("\r%lu", i);
		value = lrand();
		if (!rbtree_insert(rbt, (void*)value))
			printf("\r%lu\tValue %lu already in the tree\n",
				i, value);
	}
}

static void test(void)
{
	int i;
	rbtree_t* rbt;
	void* found_item;

	puts("Create");	
	rbt = rbtree_create(testcmp, testdel);
	lassert(rbt != NULL);

	puts("Insert");
	for (i = 0; i < cnt; i++) {
		if (i % 100 == 0)
			printf("\r%d", i);
		if (!rbtree_insert(rbt, (void*)array[i]))
			printf("\tValue %lu already in the tree\n", array[i]);
	}
	printf("\r%d\n", i);
	
	puts("Search");
	for (i = 0; i < cnt; i++) {
		if (i % 100 == 0)
			printf("\r%d", i);
		lassert(rbtree_search(rbt, (void*)array[i], &found_item));
		lassert(found_item == (void*)array[i]);
	}
	printf("\r%d\n", i);

	puts("Destroy");	
	rbtree_destroy(rbt);
	lassert(ndelete == cnt);
	ndelete = 0;
}

void main(int argc, char* argv[])
{
	int   i;
	ulong seed = 0;
	
	if (argc > 1)
		sscanf(argv[1], "%d", &cnt);
	if (argc > 2)
		sscanf(argv[2], "%ld", &seed);
	
	lsrand(seed);
	
	if (cnt == -1)
		insert_forever();
	
	array = malloc(sizeof(ulong) * cnt);
	lassert(array != NULL);

	puts("Generate random values");	
	for (i = 0; i < cnt; i++)
		array[i] = lrand();
	
	test();
	
	puts("Generate sequential values");	
	for (i = 0; i < cnt; i++)
		array[i] = i + 1;
	
	test();

	puts("Done");
}

#endif /* TEST */
