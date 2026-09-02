#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "db.h"
#include "defines/daemon_defines.h"
#include "startup.h"
#include "utils.h"

int start_lock(const char *path)
{
	int fd;

	fd = open(path, O_CREAT | O_RDWR, 0600);
	if (fd < 0)
	{
		fprintf(stderr, "lbv-daemon: no se pudo abrir %s\n", path);
		return LBV_EXIT_ERROR;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) != 0)
	{
		fprintf(stderr, "lbv-daemon: ya hay un lbv-daemon corriendo\n");
		close(fd);
		return LBV_EXIT_ERROR;
	}

	return LBV_EXIT_OK;
}

int start_jobs(b_jobs **list)
{
	*list = jobs_init();
	if (*list == NULL)
	{
		fprintf(stderr, "lbv-daemon: start_jobs error\n");
		return LBV_EXIT_ERROR;
	}

	return LBV_EXIT_OK;
}

int start_dir(const char *path)
{
	if (mkdir(path, 0700) != 0 && !is_directory(path))
	{
		fprintf(stderr, "lbv-daemon: no se pudo crear el directorio %s\n", path);
		return LBV_EXIT_ERROR;
	}

	return LBV_EXIT_OK;
}

int start_db(sqlite3 **db, const char *path)
{
	if (db_open(path, db) != DB_OK)
	{
		fprintf(stderr, "lbv-daemon: no se pudo abrir la base en %s\n", path);
		return LBV_EXIT_ERROR;
	}

	return LBV_EXIT_OK;
}

int start_socket(int *fd, const char *path)
{
	struct sockaddr_un addr;
	int s;

	if (strlen(path) >= sizeof(addr.sun_path))
	{
		fprintf(stderr, "lbv-daemon: el path del socket no entra en %zu bytes\n",
			sizeof(addr.sun_path));
		return LBV_EXIT_ERROR;
	}

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
	{
		fprintf(stderr, "lbv-daemon: no se pudo crear el socket\n");
		return LBV_EXIT_ERROR;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

	unlink(path);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		fprintf(stderr, "lbv-daemon: no se pudo bindear %s\n", path);
		close(s);
		return LBV_EXIT_ERROR;
	}

	if (listen(s, 1) < 0)
	{
		fprintf(stderr, "lbv-daemon: listen fallo en %s\n", path);
		close(s);
		unlink(path);
		return LBV_EXIT_ERROR;
	}

	*fd = s;

	return LBV_EXIT_OK;
}

int start_daemon(void)
{
	pid_t pid;
	int fd;

	pid = fork();

	if (pid < 0)
	{
		fprintf(stderr, "lbv-daemon: no se pudo soltar la terminal\n");
		return LBV_EXIT_ERROR;
	}

	if (pid > 0)
	{
		printf("lbv-daemon: corriendo con pid %d\n", (int)pid);
		fflush(stdout);
		_exit(LBV_EXIT_OK);
	}

	if (setsid() < 0) return LBV_EXIT_ERROR;

	fd = open("/dev/null", O_RDWR);
	if (fd < 0) return LBV_EXIT_ERROR;

	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);

	if (fd > STDERR_FILENO) close(fd);

	return LBV_EXIT_OK;
}

int cleanup(b_jobs *list, sqlite3 *db, int rc)
{
	jobs_free(list);
	db_close(db);

	return rc;
}