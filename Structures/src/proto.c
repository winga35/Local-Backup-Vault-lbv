#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "proto.h"

#define PROTO_LINE_MAX (PATH_MAX + 128)

static int write_all(int fd, const char *buf, size_t len)
{
	ssize_t n;
	size_t escrito;

	escrito = 0;

	while (escrito < len)
	{
		n = write(fd, buf + escrito, len - escrito);

		if (n < 0 && errno == EINTR) continue;

		if (n <= 0) return PROTO_ERR_WRITE;

		escrito += (size_t)n;
	}

	return PROTO_OK;
}

static int send_line(int fd, const char *line, int len)
{
	if (len < 0 || (size_t)len >= PROTO_LINE_MAX) return PROTO_ERR_ARG;

	return write_all(fd, line, (size_t)len);
}

int proto_send_busy(int fd)
{
	const char *linea = "BUSY\n";

	if (fd < 0) return PROTO_ERR_ARG;

	return write_all(fd, linea, strlen(linea));
}

int proto_send_msg(int fd, const char *fmt, ...)
{
	char msg[PROTO_LINE_MAX / 2];
	char line[PROTO_LINE_MAX];
	va_list ap;
	char *p;
	int len;

	if (fd < 0) return PROTO_ERR_ARG;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	for (p = msg; *p != '\0'; p++)
		if (*p == '\n' || *p == '\r') *p = ' ';

	len = snprintf(line, sizeof(line), "MSG %s\n", msg);

	return send_line(fd, line, len);
}

int proto_send_cmd(int fd, const char *fmt, ...)
{
	char line[PROTO_LINE_MAX];
	va_list ap;
	int len;

	if (fd < 0) return PROTO_ERR_ARG;

	va_start(ap, fmt);
	len = vsnprintf(line, sizeof(line) - 2, fmt, ap);
	va_end(ap);

	if (len < 0 || (size_t)len >= sizeof(line) - 2) return PROTO_ERR_ARG;

	line[len] = '\n';
	line[len + 1] = '\0';

	return write_all(fd, line, (size_t)len + 1);
}

int proto_send_snapshot(int fd, const b_jobs *list)
{
	char line[PROTO_LINE_MAX];
	const b_job *job;
	const b_backup *backup;
	int len;

	if (fd < 0 || list == NULL) return PROTO_ERR_ARG;

	len = snprintf(line, sizeof(line), "SNAP %zu\n", list->size);
	if (send_line(fd, line, len) != PROTO_OK) return PROTO_ERR_WRITE;

	for (job = list->head; job != NULL; job = job->next)
	{
		len = snprintf(line, sizeof(line), "JOB %d %u %d %u\n",
			job->id, job->mode, job->frequency, job->enabled);
		if (send_line(fd, line, len) != PROTO_OK) return PROTO_ERR_WRITE;

		len = snprintf(line, sizeof(line), "SRC %s\n", job->source);
		if (send_line(fd, line, len) != PROTO_OK) return PROTO_ERR_WRITE;

		len = snprintf(line, sizeof(line), "DST %s\n", job->destination);
		if (send_line(fd, line, len) != PROTO_OK) return PROTO_ERR_WRITE;

		len = snprintf(line, sizeof(line), "NAME %s\n", job->name);
		if (send_line(fd, line, len) != PROTO_OK) return PROTO_ERR_WRITE;

		for (backup = job->backups_head; backup != NULL; backup = backup->next)
		{
			len = snprintf(line, sizeof(line), "BKP %d %ld %s\n",
				backup->backup_db_id, (long)backup->created_at, backup->path);
			if (send_line(fd, line, len) != PROTO_OK) return PROTO_ERR_WRITE;
		}
	}

	len = snprintf(line, sizeof(line), "END\n");
	if (send_line(fd, line, len) != PROTO_OK) return PROTO_ERR_WRITE;

	return PROTO_OK;
}
