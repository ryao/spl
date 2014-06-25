/*****************************************************************************\
 *  Copyright (C) 2014 Zettabyte Software, LLC.
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
 *****************************************************************************
 *  Solaris Porting Layer (SPL) Cyclic Implementation.
\*****************************************************************************/

#ifdef SS_CYCLIC_SUBSYS
#undef SS_CYCLIC_SUBSYS
#endif

#include <linux/workqueue_compat.h>
#include <spl-debug.h>
#include <sys/cyclic.h>
#include <sys/kmem.h>
#include <sys/list.h>
#include <sys/types.h>

/*
 * The cyclic interface on Solaris is designed to enable events to occur with
 * high resolution granularity. The interfaces needed to implement that on
 * Linux are restricted to GPL code by symbol exports. However, we do have less
 * granular timers available to us via the schedule_delayed_work() interface.
 * This enables us to implement the subset of the cyclic interface that is
 * required to support the ZFS deadman timer.
 *
 * Since this is a minimal interface that exists (at this time) solely for the
 * ZFS deadman timer, not much effort has been put into optimization.  It is
 * quite possible that the use of the system workqueue would cause other things
 * to become starved.
 *
 * In addition, the cyclic ids are assigned in monotonically increasing order
 * and are accessed via traversal of a linked list. This design will scale
 * poorly should new code ever attempt to create and destroy many cyclics. Not
 * only will the destruction be O(N^2), but there is the risk of integer
 * wraparound.
 *
 * Such thingss should never manifest as problems with the ZFS deadman timer,
 * which requires only 1 cyclic timer, but if we were processing millions of
 * cyclic timers, such things could become a problem.
 *
 * In the interst of portability to other platforms such as Mac OS X, effort
 * has been taken to rely on emulated Solaris interfaces whenever possible.
 */

static list_t		cyb_timer_list;
static cyclic_id_t	cyb_next_id = 1;
static kmutex_t		cyb_mutex;
static kmem_cache_t	*cyb_cache;

typedef struct cyb_timer {
	cyclic_id_t		cyb_id;
	struct delayed_work	cyb_delayed_work;
	list_node_t		cyb_list;
	cyc_handler_t		cyb_handler;
	cyc_time_t;		cyb_time;
} cyb_timer_t;

/* XXX: Assign a debug identifier to cyclic */
#define SS_CYCLIC_SUBSYS SS_UNDEFINED

static cyb_timer_t *
spl_cyb_find(cyclic_id_t id)
{
	cyb_timer_t *cyb_timer;

	mutex_enter(&cyb_mutex);

	for ( cyb_timer = list_head(&cyb_timer_list); cyb_timer != NULL;
	    cyb_timer = list_next(&cyb_timer_list, cyb_timer)) {
		if (cyb_timer->cyb_id == id)
			break;
	}

	mutex_exit(&cyb_mutex);

	return cyb_timer;
}

static void
spl_cyb_work_func(void *data)
{
	cyb_timer_t *cyb_timer = spl_get_work_data(data, cyb_timer_t, data);
	cyc_handler_t *hdlr = &cyb_timer->cyb_handler;
	cyc_time_t *when = &cyb_timer->cyb_time;
	hrtimer_t next;

	hdlr->cyh_func(hdlr->cyh_arg);

	next = lbolt + when->cyt_interval;

	if (when->cyt_when > next)
		VERIFY(schedule_delayed_work(&cyb_timer->cyb_delayed_work,
		    next));
}

cyclic_id_t
spl_cyclic_add(cyc_handler_t *hdlr, cyc_time_t *when)
{
	cyb_timer_t *cyb_timer = kmem_cache_alloc(cyb_cache, KM_SLEEP);
	hrtimer_t firetime;

	cyb_timer->cyb_handler = *hdlr;
	cyb_timer->cyb_time = *when;

	mutex_enter(&cyb_mutex);
	cyb_timer->cyb_id = cyb_next_id++;
	list_insert_tail(cyb_timer_list, cyb_timer);
	mutex_exit(&cyb_mutex);

	spl_init_delayed_work(&cyb_timer->cyb_delayed_work, spl_cyb_work_func,
	    cyb_timer);
	firetime = lbolt + when->cyt_interval;
	VERIFY(schedule_delayed_work(&cyb_timer->cyb_delayed_work, firetime));

	return cyb_timer->cyb_id;
}
EXPORT_SYMBOL(spl_cyclic_add);

/* ARGSUSED */
void
spl_cyclic_remove(cyclic_id_t id)
{
	cyb_timer_t *cyb_timer = spl_cyb_find(id);

	ASSERT(cyb_timer);
	if (cyb_timer == NULL)
		return;

	/* XXX: Make this efficient */
	while (!cancel_delayed_work_sync(&cyb_timer->cyb_delayed_work));

	kmem_cache_free(cyb_cache, cyb_timer);
}
EXPORT_SYMBOL(spl_cyclic_remove);

/* ARGSUSED */
int
spl_cyclic_reprogram(cyclic_id_t id, hrtime_t expiration)
{
	cyb_timer_t *cyb_timer = spl_cyb_find(id);

	ASSERT(cyb_timer);
	if (cyb_timer == NULL)
		return;

	cyb_timer->cyb_time.cyt_when = expiration;

	return (1);
}
EXPORT_SYMBOL(spl_cyclic_reprogram);

int
spl_cyclic_init(void)
{
	int rc = 0;
	SENTRY;

	mutex_init(&cyb_mutex, "cyb_mutex", MUTEX_DEFAULT, NULL);

	cyb_cache = kmem_cache_create("cyb_cache", sizeof(cyb_timer_t), 0,
		NULL, NULL, NULL, NULL, NULL, 0);

	list_link_init(&cyb_timer_list);

	SRETURN(rc);
}

void
spl_cyclic_fini(void)
{
	SENTRY;

	ASSERT(list_is_empty(&cyb_timer_list));

	list_destroy(&cyb_timer_list);

	mutex_destroy(&cyb_mutex);

	kmem_cache_destroy(cyb_cache);

	SEXIT;
}
