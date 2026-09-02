#ifndef JOB_H
#define JOB_H

#include <limits.h>
#include <time.h>

#include "backup.h"
#include "defines/job_defines.h"

typedef struct b_job {
	int id;
	char name[JOB_NAME_MAX + 1];
	char source[PATH_MAX];
	char destination[PATH_MAX];
	unsigned char mode;
	int frequency;
	unsigned char enabled;
	int retention;
	b_backup *backups_head;
	b_backup *backups_tail;
	struct b_job *next;
	struct b_job *prev;
} b_job;

b_job *job_init(int id, const char *name, const char *source,
	const char *destination, unsigned char mode, int frequency);
int job_add_backup(b_job *job, int id, const char *path, time_t created_at);
int job_last_backup_time(const b_job *job, time_t *out);
time_t job_time_until_due(const b_job *job);
void job_free(b_job *job);

#endif
