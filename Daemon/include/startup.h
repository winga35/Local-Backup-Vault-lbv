#ifndef STARTUP_H
#define STARTUP_H

#include <sqlite3.h>

#include "jobs.h"

int start_lock(const char *path);
int start_jobs(b_jobs **list);
int start_dir(const char *path);
int start_db(sqlite3 **db, const char *path);
int start_socket(int *fd, const char *path);
int start_daemon(void);
int cleanup(b_jobs *list, sqlite3 *db, int rc);

#endif
