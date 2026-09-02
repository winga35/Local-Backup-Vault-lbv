#ifndef JOBS_H
#define JOBS_H

#include <stddef.h>

#include "defines/jobs_defines.h"
#include "job.h"

typedef struct b_jobs {
	b_job *head;
	b_job *tail;
	b_job *id_table[MAX_JOBS];
	size_t size;
} b_jobs;

b_jobs *jobs_init(void);
int jobs_add_job(b_jobs *list, int id, const char *name, const char *source,
	const char *destination, unsigned char mode, int frequency,
	unsigned char enabled, int retention);
int jobs_remove_job(b_jobs *list, int id);
int jobs_free_slot(const b_jobs *list);
void jobs_free(b_jobs *list);

#endif
