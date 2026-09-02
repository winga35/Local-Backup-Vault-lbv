#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "job_worker.h"
#include "proto.h"
#include "worker_pool.h"

static pid_t workers[MAX_JOBS];

int reap_workers(b_jobs *list, sqlite3 *db, int fd)
{
	pid_t pid;
	int status;
	int id;
	int cosechados;

	cosechados = 0;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		for (id = 0; id < MAX_JOBS; id++)
			if (workers[id] == pid) break;

		if (id == MAX_JOBS) continue;

		workers[id] = 0;
		cosechados++;

		if (list->id_table[id] == NULL) continue;

		if (!WIFEXITED(status))
		{
			proto_send_msg(fd, "al worker del job %d lo mato una senal", id);
			continue;
		}

		if (WEXITSTATUS(status) != WORKER_OK)
		{
			proto_send_msg(fd, "el backup del job %d fallo (codigo %d)", id,
				WEXITSTATUS(status));
			continue;
		}

		if (db_select_last_backup(db, list->id_table[id]) != DB_OK)
		{
			proto_send_msg(fd, "no se pudo refrescar el ultimo backup del job %d",
				id);
			continue;
		}

		if (list->id_table[id]->backups_tail != NULL)
			proto_send_msg(fd, "backup del job %d listo: %s", id,
				list->id_table[id]->backups_tail->path);
	}

	return cosechados;
}

void spawn_workers(b_jobs *list, int listen_fd)
{
	sigset_t todas;
	sigset_t antes;
	b_job *job;
	pid_t padre;
	pid_t pid;

	for (job = list->head; job != NULL; job = job->next)
	{
		if (!job->enabled) continue;

		if (job->mode != BACKUP_MODE_SIGNAL) continue;

		if (workers[job->id] > 0) continue;

		sigfillset(&todas);
		sigprocmask(SIG_BLOCK, &todas, &antes);

		padre = getpid();
		pid = fork();

		if (pid < 0)
		{
			sigprocmask(SIG_SETMASK, &antes, NULL);
			continue;
		}

		if (pid == 0)
		{

			signal(SIGCHLD, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			signal(SIGPIPE, SIG_DFL);

			prctl(PR_SET_PDEATHSIG, SIGTERM);

			if (getppid() != padre) _exit(WORKER_ERR_ARG);

			sigprocmask(SIG_SETMASK, &antes, NULL);

			close(listen_fd);

			_exit(job_worker_main(job));
		}

		workers[job->id] = pid;

		sigprocmask(SIG_SETMASK, &antes, NULL);
	}
}

void stop_worker(int id)
{
	struct timespec espera;
	int intentos;
	int status;
	pid_t muerto;

	if (workers[id] <= 0) return;

	espera.tv_sec = 0;
	espera.tv_nsec = 10 * 1000 * 1000;

	kill(workers[id], SIGTERM);

	muerto = 0;

	for (intentos = 0; intentos < 100 && muerto == 0; intentos++)
	{
		muerto = waitpid(workers[id], &status, WNOHANG);

		if (muerto < 0 && errno != EINTR) muerto = workers[id];

		if (muerto == 0) nanosleep(&espera, NULL);
	}

	if (muerto == 0)
	{
		kill(workers[id], SIGKILL);

		while (waitpid(workers[id], &status, 0) < 0 && errno == EINTR)
			;
	}

	workers[id] = 0;
}

void stop_workers(void)
{
	int id;

	for (id = 0; id < MAX_JOBS; id++)
		stop_worker(id);
}