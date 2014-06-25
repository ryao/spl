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
\*****************************************************************************/

#ifndef _SPL_CYLIC_H
#define	_SPL_CYCLIC_H

#include <sys/time.h>

typedef uintptr_t cyclic_id_t;
typedef uint16_t cyc_level_t;
typedef void (*cyc_func_t)(void *);

#define	CY_LOW_LEVEL		0
#define	CYCLIC_NONE		((cyclic_id_t)0)
#define	CY_INFINITY		INT64_MAX

typedef struct cyc_handler {
	cyc_func_t cyh_func;
	void *cyh_arg;
	cyc_level_t cyh_level;
} cyc_handler_t;

typedef struct cyc_time {
	hrtime_t cyt_when;
	hrtime_t cyt_interval;
} cyc_time_t;

#define cyclic_add(hdlr, when)		spl_cyclic_add((hdlr), (when))
#define cyclic_remove(id)		spl_cyclic_remove((id))
#define cyclic_reprogram(id, expire)	spl_cyclic_reprogram((id), (expire))

extern cyclic_id_t spl_cyclic_add(cyc_handler_t *hdlr, cyc_time_t *when);
extern void spl_cyclic_remove(cyclic_id_t id);
extern int spl_cyclic_reprogram(cyclic_id_t id, hrtime_t expiration);

#endif
