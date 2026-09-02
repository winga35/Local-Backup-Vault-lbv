#ifndef BACKUP_H
#define BACKUP_H

#include <limits.h>
#include <time.h>

#include "defines/backup_defines.h"

typedef struct b_backup {
	int backup_db_id;
	int job_id;
	char path[PATH_MAX];
	time_t created_at;
	struct b_backup *next;
	struct b_backup *prev;
} b_backup;

b_backup *backup_init(int backup_db_id, int job_id, const char *path,
	time_t created_at);
int backup_save(const char *source, b_backup *backup);
void backup_free(b_backup *backup);

#endif
