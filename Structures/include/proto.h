#ifndef PROTO_H
#define PROTO_H

#include "defines/proto_defines.h"
#include "jobs.h"

int proto_send_snapshot(int fd, const b_jobs *list);

int proto_send_busy(int fd);
int proto_send_msg(int fd, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
int proto_send_cmd(int fd, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

#endif
