#ifndef DAEMON_H
#define DAEMON_H

#include <sqlite3.h>

#include "jobs.h"

void daemon_loop(int listen_fd, b_jobs *list, sqlite3 *db);

#endif
