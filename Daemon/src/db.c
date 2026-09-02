#include "db.h"
#include "defines/job_defines.h"
#include "defines/jobs_defines.h"

#define DB_STR_(x) #x
#define DB_STR(x) DB_STR_(x)

static const char *const DB_SCHEMA =
	"CREATE TABLE IF NOT EXISTS jobs ("
	"    id          INTEGER PRIMARY KEY"
	"                CHECK (id >= 0 AND id < " DB_STR(MAX_JOBS) "),"
	"    name        TEXT NOT NULL,"
	"    source      TEXT NOT NULL,"
	"    destination TEXT NOT NULL,"
	"    mode        INTEGER NOT NULL"
	"                CHECK (mode IN (" DB_STR(BACKUP_MODE_SIGNAL) ", "
	                                  DB_STR(BACKUP_MODE_CRON) ")),"
	"    frequency   INTEGER NOT NULL"
	"                CHECK (frequency >= " DB_STR(BACKUP_FREQ_MIN)
	"                   AND frequency <= " DB_STR(BACKUP_FREQ_MAX) "),"
	"    enabled     INTEGER NOT NULL DEFAULT 1,"
	"    retention   INTEGER NOT NULL DEFAULT 5"
	"                CHECK (retention >= " DB_STR(JOB_RETENTION_MIN)
	"                   AND retention <= " DB_STR(JOB_RETENTION_MAX) ")"
	");"
	"CREATE TABLE IF NOT EXISTS backups ("
	"    id          INTEGER PRIMARY KEY,"
	"    job_id      INTEGER NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,"
	"    path        TEXT NOT NULL,"
	"    created_at  INTEGER NOT NULL"
	");"
	"CREATE INDEX IF NOT EXISTS idx_backups_job_time"
	"    ON backups(job_id, created_at);";

static const char *const DB_FOREIGN_KEY_ON = "PRAGMA foreign_keys = ON;";

static const char *const DB_JOURNAL_MODE_WAL = "PRAGMA journal_mode = WAL;";

static const char *const DB_BUSY_TIMEOUT = "PRAGMA busy_timeout = 5000;";

static const char *const DB_INSERT_JOB =
	"INSERT INTO jobs (id, name, source, destination, mode, frequency,"
	"    enabled, retention)"
	"    VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

static const char *const DB_INSERT_BACKUP =
	"INSERT INTO backups (job_id, path, created_at)"
	"    VALUES (?, ?, ?);";

static const char *const DB_SELECT_JOBS =
	"SELECT id, name, source, destination, mode, frequency, enabled,"
	"    retention FROM jobs ORDER BY id;";

static const char *const DB_SELECT_BACKUPS =
	"SELECT id, path, created_at FROM backups"
	"    WHERE job_id = ? ORDER BY created_at;";

static const char *const DB_DELETE_JOB = "DELETE FROM jobs WHERE id = ?;";

static const char *const DB_DELETE_ALL_JOBS = "DELETE FROM jobs;";

static const char *const DB_SELECT_LAST_BACKUP =
	"SELECT id, path, created_at FROM backups"
	"    WHERE job_id = ? ORDER BY created_at DESC LIMIT 1;";

int db_open(const char *path, sqlite3 **out)
{
	sqlite3 *db;

	if (sqlite3_open(path, &db) != SQLITE_OK)
		return DB_ERR_OPEN;

	if (sqlite3_exec(db, DB_FOREIGN_KEY_ON, NULL, NULL, NULL) != SQLITE_OK)
		return DB_ERR_SCHEMA;

	if (sqlite3_exec(db, DB_JOURNAL_MODE_WAL, NULL, NULL, NULL) != SQLITE_OK)
		return DB_ERR_SCHEMA;

	if (sqlite3_exec(db, DB_BUSY_TIMEOUT, NULL, NULL, NULL) != SQLITE_OK)
		return DB_ERR_SCHEMA;

	if (sqlite3_exec(db, DB_SCHEMA, NULL, NULL, NULL) != SQLITE_OK)
		return DB_ERR_SCHEMA;

	*out = db;

	return DB_OK;
}

int db_close(sqlite3 *db)
{

	if (db == NULL) return DB_OK;

	if (sqlite3_close(db) != SQLITE_OK) return DB_ERR_CLOSE;

	return DB_OK;
}

int db_insert_job(sqlite3 *db, int id, const char *name, const char *source,
	const char *destination, unsigned char mode, int frequency,
	unsigned char enabled, int retention)
{
	sqlite3_stmt *stmt;
	int rc;

	if (id < 0 || id >= MAX_JOBS) return DB_ERR_BAD_ID;

	if (sqlite3_prepare_v2(db, DB_INSERT_JOB, -1, &stmt, NULL) != SQLITE_OK)
		return DB_ERR_INSERT;

	sqlite3_bind_int(stmt, 1, id);
	sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, source, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 4, destination, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 5, mode);
	sqlite3_bind_int(stmt, 6, frequency);
	sqlite3_bind_int(stmt, 7, enabled);
	sqlite3_bind_int(stmt, 8, retention);

	rc = sqlite3_step(stmt);

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) return DB_ERR_INSERT;

	return DB_OK;
}

int db_insert_backup(sqlite3 *db, int job_id, const char *path,
	time_t created_at, int *out_backup_db_id)
{
	sqlite3_stmt *stmt;
	int rc;

	if (out_backup_db_id == NULL) return DB_ERR_INSERT;

	if (sqlite3_prepare_v2(db, DB_INSERT_BACKUP, -1, &stmt, NULL) != SQLITE_OK)
		return DB_ERR_INSERT;

	sqlite3_bind_int(stmt, 1, job_id);
	sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 3, created_at);

	rc = sqlite3_step(stmt);

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) return DB_ERR_INSERT;

	*out_backup_db_id = (int)sqlite3_last_insert_rowid(db);

	return DB_OK;
}

int db_select_jobs(sqlite3 *db, b_jobs *list)
{
	sqlite3_stmt *stmt;
	int rc;
	int add;

	if (list == NULL) return DB_ERR_SELECT;

	if (sqlite3_prepare_v2(db, DB_SELECT_JOBS, -1, &stmt, NULL) != SQLITE_OK)
		return DB_ERR_SELECT;

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
	{
		add = jobs_add_job(list, sqlite3_column_int(stmt, 0),
			(const char *)sqlite3_column_text(stmt, 1),
			(const char *)sqlite3_column_text(stmt, 2),
			(const char *)sqlite3_column_text(stmt, 3),
			(unsigned char)sqlite3_column_int(stmt, 4),
			sqlite3_column_int(stmt, 5),
			(unsigned char)sqlite3_column_int(stmt, 6),
			sqlite3_column_int(stmt, 7));

		if (add != JOBS_OK)
		{
			sqlite3_finalize(stmt);
			return DB_ERR_SELECT;
		}
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) return DB_ERR_SELECT;

	return DB_OK;
}

int db_select_backups(sqlite3 *db, b_job *job)
{
	sqlite3_stmt *stmt;
	int rc;

	if (job == NULL) return DB_ERR_SELECT;

	if (sqlite3_prepare_v2(db, DB_SELECT_BACKUPS, -1, &stmt, NULL) != SQLITE_OK)
		return DB_ERR_SELECT;

	sqlite3_bind_int(stmt, 1, job->id);

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
	{
		if (job_add_backup(job, sqlite3_column_int(stmt, 0),
			(const char *)sqlite3_column_text(stmt, 1),
			(time_t)sqlite3_column_int64(stmt, 2)) != BACKUP_OK)
		{
			sqlite3_finalize(stmt);
			return DB_ERR_SELECT;
		}
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) return DB_ERR_SELECT;

	return DB_OK;
}

int db_select_last_backup(sqlite3 *db, b_job *job)
{
	sqlite3_stmt *stmt;
	int rc;
	int add;

	if (job == NULL) return DB_ERR_SELECT;

	if (sqlite3_prepare_v2(db, DB_SELECT_LAST_BACKUP, -1, &stmt, NULL) != SQLITE_OK)
		return DB_ERR_SELECT;

	sqlite3_bind_int(stmt, 1, job->id);

	rc = sqlite3_step(stmt);

	if (rc == SQLITE_DONE)
	{
		sqlite3_finalize(stmt);
		return DB_OK;
	}

	if (rc != SQLITE_ROW)
	{
		sqlite3_finalize(stmt);
		return DB_ERR_SELECT;
	}

	add = job_add_backup(job, sqlite3_column_int(stmt, 0),
		(const char *)sqlite3_column_text(stmt, 1),
		(time_t)sqlite3_column_int64(stmt, 2));

	sqlite3_finalize(stmt);

	if (add != BACKUP_OK) return DB_ERR_SELECT;

	return DB_OK;
}

int db_delete_job(sqlite3 *db, int id)
{
	sqlite3_stmt *stmt;
	int rc;

	if (id < 0 || id >= MAX_JOBS) return DB_ERR_BAD_ID;

	if (sqlite3_prepare_v2(db, DB_DELETE_JOB, -1, &stmt, NULL) != SQLITE_OK)
		return DB_ERR_DELETE;

	sqlite3_bind_int(stmt, 1, id);

	rc = sqlite3_step(stmt);

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) return DB_ERR_DELETE;

	return DB_OK;
}

int db_reset(sqlite3 *db)
{
	if (db == NULL) return DB_ERR_DELETE;

	if (sqlite3_exec(db, DB_DELETE_ALL_JOBS, NULL, NULL, NULL) != SQLITE_OK)
		return DB_ERR_DELETE;

	return DB_OK;
}

int db_load_jobs(sqlite3 *db, b_jobs *list)
{
	b_job *job;

	if (db == NULL || list == NULL) return DB_ERR_SELECT;

	if (db_select_jobs(db, list) != DB_OK) return DB_ERR_SELECT;

	for (job = list->head; job != NULL; job = job->next)
		if (db_select_backups(db, job) != DB_OK) return DB_ERR_SELECT;

	return DB_OK;
}
