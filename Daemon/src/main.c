#include <stdio.h>
#include <unistd.h>

#include "daemon.h"
#include "db.h"
#include "defines/daemon_defines.h"
#include "jobs.h"
#include "signals.h"
#include "startup.h"
#include "worker_pool.h"

int main(void)
{
	b_jobs *list = NULL;
	sqlite3 *db = NULL;
	int listen_fd = -1;
	int db_exists;

	db_exists = (access(LBV_DB_PATH, F_OK) == 0);

	if (start_dir(LBV_DIR_PATH) != LBV_EXIT_OK) return LBV_EXIT_ERROR;

	if (start_lock(LBV_LOCK_PATH) != LBV_EXIT_OK) return LBV_EXIT_ERROR;

	if (start_jobs(&list) != LBV_EXIT_OK)
		return cleanup(list, db, LBV_EXIT_ERROR);

	if (start_db(&db, LBV_DB_PATH) != LBV_EXIT_OK)
		return cleanup(list, db, LBV_EXIT_ERROR);

	if (db_exists && db_load_jobs(db, list) != DB_OK)
	{
		fprintf(stderr, "lbv-daemon: no se pudieron cargar los jobs de %s\n",
			LBV_DB_PATH);
		return cleanup(list, db, LBV_EXIT_ERROR);
	}

	db_close(db);
	db = NULL;

	if (start_signals() != LBV_EXIT_OK)
	{
		fprintf(stderr, "lbv-daemon: no se pudieron instalar los handlers\n");
		return cleanup(list, db, LBV_EXIT_ERROR);
	}

	if (start_socket(&listen_fd, LBV_SOCK_PATH) != LBV_EXIT_OK)
		return cleanup(list, db, LBV_EXIT_ERROR);

	if (start_daemon() != LBV_EXIT_OK)
		return cleanup(list, db, LBV_EXIT_ERROR);

	if (start_db(&db, LBV_DB_PATH) != LBV_EXIT_OK)
		return cleanup(list, db, LBV_EXIT_ERROR);

	daemon_loop(listen_fd, list, db);

	stop_workers();
	close(listen_fd);
	unlink(LBV_SOCK_PATH);

	return cleanup(list, db, LBV_EXIT_OK);
}
