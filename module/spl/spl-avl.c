/*
 *  Copyright (C) 2018 Zettabyte Software, LLC.
 *  Written by Richard Yao <ryao@gentoo.org>.
 *
 *  This file is part of the SPL, Solaris Porting Layer.
 *  For details, see <http://zfsonlinux.org/>.
 *
 *  The SPL is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  The SPL is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the SPL.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This is not an actual AVL tree implementation. It is just a shim that
 * substitutes the Linux kernel's Red-Black tree implementation into place of
 * an AVL implementation. It is intended to help debug issues involving the AVL
 * implementation that we inherited from OpenSolaris by allowing us to
 * substitute an entirely different implementation. If the issue persists after
 * the Red-Black trees have been substituted, then the issue is external to the
 * AVL tree code.
 *
 * This differs from the actual AVL tree implementation in the following ways:
 *
 * 1. It uses a Red-Black trees to implement a balanced binary tree.
 *
 * 2. avl_update(), avl_update_lt() and avl_update_gt() are unimplemented.
 *
 * 3. avl_destroy_nodes() is implemented in such a way that it is safe to call
 * the other functions after an invocation of it.
 */

#include <linux/module.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/avl.h>

void
avl_create(avl_tree_t *tree, int (*compar) (const void *, const void *),
    size_t size, size_t offset)
{
	ASSERT(tree != NULL);
	ASSERT(size != 0);
	ASSERT(compar != 0);

	tree->avl_root = RB_ROOT;
	tree->avl_comparator = compar;
	tree->avl_size = size;
	tree->avl_offset = offset;
	tree->avl_children = 0;
	tree->avl_first = NULL;
	tree->avl_last = NULL;
}

void *
avl_find(avl_tree_t *tree, const void *node, avl_index_t *where)
{
	struct rb_node *n, *prev;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);

	prev = NULL;
	n = tree->avl_root.rb_node;

	while (n) {
		switch (tree->avl_comparator(node, avl_object(tree, n))) {
		case 1:
			prev = n;
			n = n->rb_left;
			break;
		case -1:
			prev = n;
			n = n->rb_right;
			break;
		case 0:
			if (where != NULL)
				*where = (avl_index_t)n;
			return (avl_object(tree, n));
		default:
			VERIFY(0);
			break;
		}
	}

	if (where)
		*where = (avl_index_t)prev;

	return (NULL);
}

void
avl_insert(avl_tree_t *tree, void *node, avl_index_t where)
{
	void *here;
	int direction;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);

	/* A where value of 0 is only used when the tree is empty */
	if (where == 0) {
		struct rb_node **link = &tree->avl_root.rb_node;

		ASSERT0(tree->avl_children);

		tree->avl_children = 1;
		tree->avl_first = node;
		tree->avl_last = node;
		rb_link_node(node, NULL, link);
		rb_insert_color(node, &tree->avl_root);
		return;
	}

	here = avl_object(tree, where);
	direction = tree->avl_comparator(node, here) != -1;

	avl_insert_here(tree, node, here, direction);
}

void
avl_insert_here(avl_tree_t *tree, void *new_data, void *here, int direction)
{
	struct rb_node *new;
	struct rb_node *n;
	struct rb_node **link = NULL;

	ASSERT(tree != NULL);
	ASSERT(tree->avl_children > 0);
	ASSERT(new_data != NULL);
	ASSERT(here != NULL);

	new = &avl_o2n(tree, new_data)->avl_node;
	n = (struct rb_node *)avl_o2n(tree, here);

	switch (direction) {
		case AVL_AFTER:
			link = &n->rb_right;
			break;
		case AVL_BEFORE:
			link = &n->rb_left;
			break;
		default:
			VERIFY(0);
			break;
	}

	tree->avl_children++;
	rb_link_node(new, n, link);
	rb_insert_color(new, &tree->avl_root);

	if (here == tree->avl_first && direction == AVL_BEFORE)
		tree->avl_first = new_data;

	if (here == tree->avl_last && direction == AVL_AFTER)
		tree->avl_last = new_data;
}

void *
avl_first(avl_tree_t *tree)
{
	ASSERT(tree != NULL);
	return (tree->avl_first);
}

void *
avl_last(avl_tree_t *tree)
{
	ASSERT(tree != NULL);
	return (tree->avl_last);
}

void *
avl_nearest(avl_tree_t *tree, avl_index_t where, int direction)
{
	struct rb_node *node = (struct rb_node *) where;

	ASSERT(tree != NULL);

	if (avl_is_empty(tree))
		return (NULL);

	switch (direction) {
	case AVL_AFTER:
		return (avl_object(tree, rb_next(node)));
	case AVL_BEFORE:
		return (avl_object(tree, rb_prev(node)));
	default:
		VERIFY(0);
	}

	return (NULL);
}

void *
avl_walk(avl_tree_t *tree, void *node, int direction)
{
	avl_index_t n;

	ASSERT(tree != NULL);

	n = (avl_index_t)avl_o2n(tree, node);

	return (avl_nearest(tree, n, direction));
}


void
avl_add(avl_tree_t *tree, void *node)
{
	avl_index_t where;

	(void) avl_find(tree, node, &where);
	avl_insert(tree, node, where);
}

void
avl_remove(avl_tree_t *tree, void *node)
{
	struct rb_node *n;

	ASSERT(tree != NULL);
	ASSERT(node != NULL);

	n = (struct rb_node *)avl_o2n(tree, node);
	tree->avl_children--;

	if (node == tree->avl_first) {
		tree->avl_first = (tree->avl_children) ?
		    avl_object(tree, rb_next(n)) : NULL;
	}

	if (node == tree->avl_last) {
		tree->avl_last = (tree->avl_children) ?
		    avl_object(tree, rb_prev(n)) : NULL;
	}

	rb_erase(n, &tree->avl_root);
}

void
avl_swap(avl_tree_t *tree1, avl_tree_t *tree2)
{
	avl_tree_t temp;

	ASSERT(tree1 != NULL);
	ASSERT(tree2 != NULL);

	temp = *tree1;
	*tree1 = *tree2;
	*tree2 = temp;
}

ulong_t
avl_numnodes(avl_tree_t *tree)
{
	ASSERT(tree != NULL);

	return (tree->avl_children);
}

boolean_t
avl_is_empty(avl_tree_t *tree)
{
	ASSERT(tree != NULL);

	return (RB_EMPTY_ROOT((struct rb_root *)tree));
}

void *
avl_destroy_nodes(avl_tree_t *tree, void **cookie)
{
	ASSERT(tree != NULL);
	ASSERT(cookie != NULL);

	*cookie = tree->avl_first;

	if (*cookie == NULL)
		return (NULL);

	avl_remove(tree, *cookie);

	return (*cookie);
}

void
avl_destroy(avl_tree_t *tree)
{
	ASSERT(tree != NULL);

	/*
	 * A size argument of zero is wrong, but due to our internal
	 * implemntation, the size argument is ignored, so this is safe
	 */
	spl_kmem_free(tree, 0);
}

EXPORT_SYMBOL(avl_create);
EXPORT_SYMBOL(avl_find);
EXPORT_SYMBOL(avl_insert);
EXPORT_SYMBOL(avl_insert_here);
EXPORT_SYMBOL(avl_walk);
EXPORT_SYMBOL(avl_first);
EXPORT_SYMBOL(avl_last);
EXPORT_SYMBOL(avl_nearest);
EXPORT_SYMBOL(avl_add);
EXPORT_SYMBOL(avl_swap);
EXPORT_SYMBOL(avl_is_empty);
EXPORT_SYMBOL(avl_remove);
EXPORT_SYMBOL(avl_numnodes);
EXPORT_SYMBOL(avl_destroy_nodes);
EXPORT_SYMBOL(avl_destroy);
