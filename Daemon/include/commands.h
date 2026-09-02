#ifndef COMMANDS_H
#define COMMANDS_H

#include <sqlite3.h>

#include "jobs.h"

int handle_command(char *line, b_jobs *list, sqlite3 *db, int fd);

#endif
