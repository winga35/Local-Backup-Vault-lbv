#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "commands.h"
#include "daemon.h"
#include "defines/daemon_defines.h"
#include "proto.h"
#include "signals.h"
#include "worker_pool.h"

void daemon_loop(int listen_fd, b_jobs *list, sqlite3 *db)
{
	struct pollfd fds[2];
	char buf[1024];
	char *ini;
	char *nl;
	size_t len;
	ssize_t n;
	int client_fd;
	int cortar;
	int rc;
	nfds_t nfds;

	client_fd = -1;
	len = 0;
	cortar = 0;

	while (!cortar)
	{
		fds[0].fd = listen_fd;
		fds[0].events = POLLIN;
		nfds = 1;

		if (client_fd >= 0)
		{
			fds[1].fd = client_fd;
			fds[1].events = POLLIN;
			nfds = 2;
		}

		rc = poll(fds, nfds, LBV_TICK_MS);

		if (rc < 0 && errno != EINTR)
		{
			fprintf(stderr, "lbv-daemon: poll fallo\n");
			break;
		}

		if (reap_workers(list, db, client_fd) > 0 && client_fd >= 0)
			proto_send_snapshot(client_fd, list);

		spawn_workers(list, listen_fd);

		if (g_stop) break;

		if (rc <= 0) continue;

		if (fds[0].revents & POLLIN)
		{
			int nuevo = accept(listen_fd, NULL, NULL);

			if (nuevo >= 0)
			{
				if (client_fd >= 0)
				{
					proto_send_busy(nuevo);
					close(nuevo);
				}
				else
				{
					client_fd = nuevo;
					len = 0;

					if (proto_send_snapshot(client_fd, list) != PROTO_OK)
					{
						close(client_fd);
						client_fd = -1;
					}
				}
			}
		}

		if (nfds == 2 && (fds[1].revents & (POLLIN | POLLHUP)))
		{
			n = read(client_fd, buf + len, sizeof(buf) - len);

			if (n <= 0)
			{
				close(client_fd);
				client_fd = -1;
				len = 0;
				continue;
			}

			len += (size_t)n;
			ini = buf;

			while ((nl = memchr(ini, '\n', (size_t)((buf + len) - ini))) != NULL)
			{
				*nl = '\0';

				if (handle_command(ini, list, db, client_fd)) cortar = 1;

				ini = nl + 1;
			}

			len = (size_t)((buf + len) - ini);
			memmove(buf, ini, len);

			if (len == sizeof(buf))
			{
				close(client_fd);
				client_fd = -1;
				len = 0;
				continue;
			}

			if (!cortar && client_fd >= 0)
				proto_send_snapshot(client_fd, list);
		}
	}

	if (client_fd >= 0) close(client_fd);
}