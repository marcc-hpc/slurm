/*****************************************************************************\
 *  preempt_partition_prio.c - job preemption plugin that selects preemptable
 *  jobs based upon their partition's priority.
 *****************************************************************************
 *  Copyright (C) 2009-2010 Lawrence Livermore National Security.
 *  Portions Copyright (C) 2010 SchedMD <http://www.schedmd.com>.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Morris jette <jette1@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <https://computing.llnl.gov/linux/slurm/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include <stdio.h>
#include <slurm/slurm_errno.h>

#include "src/common/bitstring.h"
#include "src/common/list.h"
#include "src/common/log.h"
#include "src/common/plugin.h"
#include "src/slurmctld/slurmctld.h"
#include "src/slurmctld/job_scheduler.h"

const char	plugin_name[]	= "Preempt by partition priority plugin";
const char	plugin_type[]	= "preempt/partition_prio";
const uint32_t	plugin_version	= 100;

static uint32_t _gen_job_prio(struct job_record *job_ptr);
static int  _sort_by_prio (void *x, void *y);

/**************************************************************************/
/*  TAG(                              init                              ) */
/**************************************************************************/
extern int init( void )
{
	verbose("preempt/partition_prio loaded");
	return SLURM_SUCCESS;
}

/**************************************************************************/
/*  TAG(                              fini                              ) */
/**************************************************************************/
extern void fini( void )
{
	/* Empty. */
}

/**************************************************************************/
/* TAG(                 find_preemptable_jobs                           ) */
/**************************************************************************/
extern List find_preemptable_jobs(struct job_record *job_ptr)
{
	ListIterator job_iterator;
	struct job_record *job_p;
	List preemptee_job_list = NULL;

	/* Validate the preemptor job */
	if (job_ptr == NULL) {
		error("find_preemptable_jobs: job_ptr is NULL");
		return preemptee_job_list;
	}
	if (!IS_JOB_PENDING(job_ptr)) {
		error("find_preemptable_jobs: job %u not pending",
		      job_ptr->job_id);
		return preemptee_job_list;
	}
	if (job_ptr->part_ptr == NULL) {
		error("find_preemptable_jobs: job %u has NULL partition ptr",
		      job_ptr->job_id);
		return preemptee_job_list;
	}
	if (job_ptr->part_ptr->node_bitmap == NULL) {
		error("find_preemptable_jobs: partition %s node_bitmap=NULL",
		      job_ptr->part_ptr->name);
		return preemptee_job_list;
	}

	/* Build an array of pointers to preemption candidates */
	job_iterator = list_iterator_create(job_list);
	while ((job_p = (struct job_record *) list_next(job_iterator))) {
		if (!IS_JOB_RUNNING(job_p) && !IS_JOB_SUSPENDED(job_p))
			continue;
		if ((job_p->part_ptr == NULL) ||
		    (job_p->part_ptr->priority >= job_ptr->part_ptr->priority))
			continue;
		if ((job_p->node_bitmap == NULL) ||
		    (bit_overlap(job_p->node_bitmap,
				 job_ptr->part_ptr->node_bitmap) == 0))
			continue;
		if (job_ptr->details &&
		    (job_ptr->details->expanding_jobid == job_p->job_id))
			continue;

		/* This job is a preemption candidate */
		if (preemptee_job_list == NULL) {
			preemptee_job_list = list_create(NULL);
			if (preemptee_job_list == NULL)
				fatal("list_create malloc failure");
		}
		list_append(preemptee_job_list, job_p);
	}
	list_iterator_destroy(job_iterator);

	if (preemptee_job_list)
		list_sort(preemptee_job_list, _sort_by_prio);
	return preemptee_job_list;
}

static uint32_t _gen_job_prio(struct job_record *job_ptr)
{
	uint32_t job_prio;

	if (job_ptr->part_ptr)
		job_prio = job_ptr->part_ptr->priority << 16;
	else
		job_prio = 0;

	if (job_ptr->node_cnt >= 0xffff)
		job_prio += 0xffff;
	else
		job_prio += job_ptr->node_cnt;

	return job_prio;
}

static int _sort_by_prio (void *x, void *y)
{
	int rc;
	uint32_t job_prio1, job_prio2;

	job_prio1 = _gen_job_prio((struct job_record *) x);
	job_prio2 = _gen_job_prio((struct job_record *) y);

	if (job_prio1 > job_prio2)
		rc = 1;
	else if (job_prio1 < job_prio2)
		rc = -1;
	else
		rc = 0;

	return rc;
}

/**************************************************************************/
/* TAG(                 job_preempt_mode                                ) */
/**************************************************************************/
extern uint16_t job_preempt_mode(struct job_record *job_ptr)
{
	if (job_ptr->part_ptr &&
	    (job_ptr->part_ptr->preempt_mode != (uint16_t) NO_VAL))
		return job_ptr->part_ptr->preempt_mode;

	return (slurm_get_preempt_mode() & (~PREEMPT_MODE_GANG));
}

/**************************************************************************/
/* TAG(                 preemption_enabled                              ) */
/**************************************************************************/
extern bool preemption_enabled(void)
{
	return (slurm_get_preempt_mode() != PREEMPT_MODE_OFF);
}

/***************************************************************************/
/* Return true if the preemptor can preempt the preemptee, otherwise false */
/***************************************************************************/
extern bool job_preempt_check(job_queue_rec_t *preemptor,
			      job_queue_rec_t *preemptee)
{
	if (preemptor->part_ptr && preemptee->part_ptr) {
		if (preemptor->part_ptr->priority >
		    preemptee->part_ptr->priority)
			return true;
	}

	return false;
}
