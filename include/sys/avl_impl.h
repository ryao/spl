/*
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

#ifndef _SPL_AVL_IMPL_H
#define	_SPL_AVL_IMPL_H

#include <sys/types.h>
#include <linux/rbtree.h>


typedef int (*avl_compar) (const void *, const void *);

struct avl_tree {
	struct rb_root		avl_root;
	avl_compar		avl_comparator;
	size_t			avl_size;
	size_t			avl_offset;
	ulong_t			avl_children;
	void			*avl_first;
	void			*avl_last;
};

struct avl_node {
	struct rb_node avl_node;
};


/* This points to a rb_node in our implementation. */
typedef uintptr_t avl_index_t;

typedef struct avl_tree avl_tree_t;

extern void *avl_walk(avl_tree_t *tree, void *node, int direction);

/*
 * Note that since the start of our avl_node is rb_node, it is possible to use
 * avl_object to access both the avl_node or the rb_node. YOu can also go in
 * the reverse direction if you use a typedef. This is a quirk of using a wrapper.
 */

#define	avl_o2n(a, obj) ((avl_node_t *)(((char *)obj) + (a)->avl_offset))
#define	avl_object(a, node) ((void *)(((char *)node) - (a)->avl_offset))

#endif	/* _SPL_AVL_IMPL_H */
