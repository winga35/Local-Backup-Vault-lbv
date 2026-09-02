#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "backup.h"
#include "db.h"
#include "defines/daemon_defines.h"
#include "job_worker.h"

static int job_worker_build_path(char *out, size_t size, const b_job *job,
	time_t when)
{
	struct tm broken;
	char stamp[20];
	int len;

	if (localtime_r(&when, &broken) == NULL) return WORKER_ERR_PATH;

	if (strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &broken) == 0)
		return WORKER_ERR_PATH;

	len = snprintf(out, size, "%s/%s-%s.tar.gz", job->destination, job->name,
		stamp);

	if (len < 0 || (size_t)len >= size) return WORKER_ERR_PATH;

	return WORKER_OK;
}

int job_worker_main(const b_job *job)
{
	time_t remaining;
	time_t now;
	sqlite3 *db;
	b_backup *backup;
	char path[PATH_MAX];
	int rc;

	if (job == NULL) return WORKER_ERR_ARG;

	remaining = job_time_until_due(job);
	while (remaining > 0)
		remaining = sleep((unsigned int)remaining);

	now = time(NULL);

	if (job_worker_build_path(path, sizeof(path), job, now) != WORKER_OK)
		return WORKER_ERR_PATH;

	backup = backup_init(0, job->id, path, now);
	if (backup == NULL) return WORKER_ERR_NO_MEM;

	if (db_open(LBV_DB_PATH, &db) != DB_OK)
	{
		backup_free(backup);
		return WORKER_ERR_DB;
	}

	rc = backup_save(job->source, backup);

	if (rc == BACKUP_OK && db_insert_backup(db, backup->job_id, backup->path,
		backup->created_at, &backup->backup_db_id) != DB_OK)
		rc = BACKUP_ERR_DB;

	backup_free(backup);
	db_close(db);

	if (rc != BACKUP_OK) return WORKER_ERR_BACKUP;

	return WORKER_OK;
}
