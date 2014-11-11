/**********************************************************************\
 *
 *	REGMUST.C
 *
 * Finds regmust string from syntax tree and stores it to rbuf.regmust.
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
 *	save_must
 *
 * Saves regmust.
 */
static bool save_must(REG1 node_t* tree, REG2 int pos, int len)
{
	REG3 char*	ptr;
	REG4 int	left, right;

	if (len <= 0) {
		rbuf.regmust = NULL;
		return TRUE;
	}
	if ((ptr = malloc(len+1)) == NULL)
		return FALSE;
	rbuf.regmust = ptr;
	ptr += len;
	*ptr-- = '\0';
	while (tree[pos].type == CAT && len--) {
		left = tree[pos].val.next.left;
		right = tree[pos].val.next.right;
		if (tree[right].type != ID)
			break;
		*ptr-- = tree[right].val.item;
		pos = left;
	}
	if (len && tree[pos].type == ID)
		*ptr-- = tree[pos].val.item;
	if (strlen(rbuf.regmust) == 0)
		d_free(&rbuf.regmust);
	return TRUE;
}

/**********************************************************************
 *
 *	IS_MUST_CH
 */
#define IS_MUST_CH(c)	((int)(c) < NCHARS)

/**********************************************************************
 *
 *	must_len
 *
 * Calculates regmust lenght starting from 'pos'. Length is zero if
 * current position doesn't start any regmust.
 */
static int must_len(REG1 node_t* tree, REG2 int* pos)
{
	REG3 int	len = 0;
	REG4 int	left, right;
	
	while (tree[*pos].type == CAT) {
		left = tree[*pos].val.next.left;
		right = tree[*pos].val.next.right;
		if (tree[right].type != ID)
			return len;
		if (!IS_MUST_CH(tree[right].val.item))
			return len;
		++len;
		if (tree[left].type == ID) {
			if (IS_MUST_CH(tree[left].val.item))
				return len+1;
			else
				return len;
		}
		*pos = left;
	}
	return len;
}

/**********************************************************************
 *
 *	find_regmust
 *
 * Finds longest regmust, i.e. string that must match exactly in regular
 * expression. A very simple linear algorithm is used: starting from root go
 * always to the left and from every CAT node try to calculate regmust
 * that begins from that position. Routine stops to first node that isn't
 * CAT node. A better algorithm should also check possible common
 * substrings from OR's, now we just give up if we find OR.
 */
bool find_regmust(REG1 node_t* tree, int root)
{
	REG2 int	len, new_len;
	REG4 int	pos, save;

	rbuf.regmust = NULL;
	root -= rbuf.search ? 3 : 2;
	if (root == 0) {
		if (rbuf.tree[0].type == ID 
		    && IS_MUST_CH(rbuf.tree[0].val.item))
			len = 1;
		else
			len = 0;
		return save_must(tree, 0, len);
	}
	len = 0;
	while (tree[root].type == CAT) {
		save = root;
		new_len = must_len(tree, &root);
		if (new_len > len) {
			len = new_len;
			pos = save;
		}
		if (root == save)
			root = tree[root].val.next.left;
	}
	if (tree[root].type == ID && len < 1 
	    && IS_MUST_CH(rbuf.tree[root].val.item))
	{
		len = 1;
		pos = root;
	}
	return save_must(tree, pos, len > MAXREGMUST ? MAXREGMUST : len);
}
