#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "job.h"

b_job *job_init(int id, const char *name, const char *source,
	const char *destination, unsigned char mode, int frequency)
{
	b_job *job;

	if (name == NULL || source == NULL || destination == NULL) return NULL;

	if (strlen(name) > JOB_NAME_MAX) return NULL;

	if (strlen(source) >= PATH_MAX || strlen(destination) >= PATH_MAX)
		return NULL;

	job = malloc(sizeof(*job));
	if (job == NULL) return NULL;

	job->id = id;
	snprintf(job->name, sizeof(job->name), "%s", name);
	snprintf(job->source, sizeof(job->source), "%s", source);
	snprintf(job->destination, sizeof(job->destination), "%s", destination);
	job->mode = mode;
	job->frequency = frequency;
	job->enabled = JOB_ENABLED;
	job->retention = JOB_RETENTION_DEFAULT;
	job->backups_head = NULL;
	job->backups_tail = NULL;
	job->next = NULL;
	job->prev = NULL;

	return job;
}

int job_add_backup(b_job *job, int id, const char *path, time_t created_at)
{

	if (job == NULL || path == NULL) return BACKUP_ERR_ARG;

	b_backup *backup;
	backup = backup_init(id, job->id, path, created_at);

	if (backup == NULL) return BACKUP_ERR_NO_MEM;

	backup->prev = job->backups_tail;

	if (job->backups_tail == NULL)
		job->backups_head = backup;
	else
		job->backups_tail->next = backup;

	job->backups_tail = backup;

	return BACKUP_OK;
}

int job_last_backup_time(const b_job *job, time_t *out)
{

	if (job == NULL || out == NULL) return BACKUP_ERR_ARG;

	if (job->backups_tail == NULL) return BACKUP_ERR_NOT_FOUND;

	*out = job->backups_tail->created_at;

	return BACKUP_OK;
}

time_t job_time_until_due(const b_job *job)
{
	time_t last;
	time_t elapsed;

	if (job == NULL) return 0;

	if (job_last_backup_time(job, &last) != BACKUP_OK) return 0;

	elapsed = time(NULL) - last;
	if (elapsed >= job->frequency) return 0;

	return job->frequency - elapsed;
}

void job_free(b_job *job)
{
	b_backup *backup;
	b_backup *next;

	if (job == NULL) return;

	backup = job->backups_head;
	while (backup != NULL)
	{
		next = backup->next;
		backup_free(backup);
		backup = next;
	}

	free(job);
}