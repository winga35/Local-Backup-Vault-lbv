#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <sqlite3.h>

#include "jobs.h"

int reap_workers(b_jobs *list, sqlite3 *db, int fd);
void spawn_workers(b_jobs *list, int listen_fd);
void stop_worker(int id);
void stop_workers(void);

#endif
