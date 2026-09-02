#ifndef DB_H
#define DB_H

#include <time.h>

#include <sqlite3.h>

#include "defines/db_defines.h"
#include "jobs.h"

int db_open(const char *path, sqlite3 **out);
int db_close(sqlite3 *db);
int db_insert_job(sqlite3 *db, int id, const char *name, const char *source,
	const char *destination, unsigned char mode, int frequency,
	unsigned char enabled, int retention);
int db_insert_backup(sqlite3 *db, int job_id, const char *path,
	time_t created_at, int *out_backup_db_id);
int db_select_jobs(sqlite3 *db, b_jobs *list);
int db_select_backups(sqlite3 *db, b_job *job);
int db_select_last_backup(sqlite3 *db, b_job *job);
int db_delete_job(sqlite3 *db, int id);
int db_load_jobs(sqlite3 *db, b_jobs *list);
int db_reset(sqlite3 *db);

#endif
