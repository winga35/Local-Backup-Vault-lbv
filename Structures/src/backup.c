#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "backup.h"

extern char **environ;

b_backup *backup_init(int backup_db_id, int job_id, const char *path,
	time_t created_at)
{
	b_backup *backup;

	if (path == NULL) return NULL;

	if (strlen(path) >= PATH_MAX) return NULL;

	backup = malloc(sizeof(*backup));
	if (backup == NULL) return NULL;

	backup->backup_db_id = backup_db_id;
	backup->job_id = job_id;
	snprintf(backup->path, sizeof(backup->path), "%s", path);
	backup->created_at = created_at;
	backup->next = NULL;
	backup->prev = NULL;

	return backup;
}

int backup_save(const char *source, b_backup *backup)
{
	pid_t pid;
	int status;

	if (source == NULL || backup == NULL) return BACKUP_ERR_ARG;

	char *argv[] = { "tar", "-czf", backup->path, "-C", (char *)source, ".",
		NULL };

	if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ) != 0)
		return BACKUP_ERR_FORK;

	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR) return BACKUP_ERR_RUN;

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return BACKUP_ERR_RUN;

	return BACKUP_OK;
}

void backup_free(b_backup *backup)
{
	if (backup == NULL) return;

	free(backup);
}
