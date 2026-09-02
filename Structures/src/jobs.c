#include <stdlib.h>
#include <string.h>

#include "jobs.h"

b_jobs *jobs_init(void)
{
	b_jobs *list;

	list = malloc(sizeof(*list));
	if (list == NULL) return NULL;

	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	memset(list->id_table, 0, sizeof(list->id_table));

	return list;
}

int jobs_add_job(b_jobs *list, int id, const char *name, const char *source,
	const char *destination, unsigned char mode, int frequency,
	unsigned char enabled, int retention)
{

	if (list == NULL || name == NULL || source == NULL || destination == NULL) return JOBS_BAD_ARG;

	if (list->size >= MAX_JOBS) return JOBS_FULL;

	if (id < 0 || id >= MAX_JOBS) return JOBS_BAD_ID;

	if (list->id_table[id] != NULL) return JOBS_ID_TAKEN;

	b_job *job;
	job = job_init(id, name, source, destination, mode, frequency);

	if (job == NULL) return JOBS_NO_MEM;

	job->enabled = enabled;
	job->retention = retention;

	job->prev = list->tail;

	if (list->tail == NULL)
		list->head = job;
	else
		list->tail->next = job;

	list->tail = job;
	list->id_table[id] = job;
	list->size++;

	return JOBS_OK;
}

int jobs_remove_job(b_jobs *list, int id)
{
	b_job *job;

	if (list == NULL) return JOBS_BAD_ARG;

	if (id < 0 || id >= MAX_JOBS) return JOBS_BAD_ID;

	job = list->id_table[id];
	if (job == NULL) return JOBS_NOT_FOUND;

	if (job == list->head && job == list->tail)
	{
		list->head = NULL;
		list->tail = NULL;
	}

	else if (job == list->head)
	{
		list->head = job->next;
		job->next->prev = NULL;
	}

	else if (job == list->tail)
	{
		list->tail = job->prev;
		job->prev->next = NULL;
	}

	else
	{
		job->prev->next = job->next;
		job->next->prev = job->prev;
	}

	list->id_table[id] = NULL;
	list->size--;
	job_free(job);

	return JOBS_OK;
}

int jobs_free_slot(const b_jobs *list)
{
	int id;

	if (list == NULL) return -1;

	for (id = 0; id < MAX_JOBS; id++)
		if (list->id_table[id] == NULL) return id;

	return -1;
}

void jobs_free(b_jobs *list)
{
	int id;

	if (list == NULL) return;

	for (id = 0; id < MAX_JOBS; id++)
		jobs_remove_job(list, id);

	free(list);
}
