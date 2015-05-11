/**********************************************************************\
 *
 *	RBTREE.H
 *
 * Red-Black Binary Tree.
 *
\**********************************************************************/

#ifndef _RBTREE_H_
#define _RBTREE_H_

typedef struct rbn_struct {
	void*              rbn_item;
	int                rbn_red;
	struct rbn_struct* rbn_left;
	struct rbn_struct* rbn_right;
} rbtree_node_t;

typedef struct rbt_struct {
	rbtree_node_t rbt_head;
	int  (*rbt_compare)(void*, void*);
	void (*rbt_delete)(void*);
} rbtree_t;

bool      rbtree_insert(rbtree_t* rbt, void* item);
bool      rbtree_search(rbtree_t* rbt, void* item, void* found_item);
rbtree_t* rbtree_create(int (*compare)(void*, void*), void (*delete)(void*));
void      rbtree_destroy(rbtree_t* rbt);

#endif /* _RBTREE_H_ */
