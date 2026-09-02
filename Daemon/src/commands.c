#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "db.h"
#include "proto.h"
#include "utils.h"
#include "worker_pool.h"

static char *next_arg(char **rest)
{
	char *ini;
	char *fin;

	if (*rest == NULL) return NULL;

	while (**rest == ' ' || **rest == '\t' || **rest == '\n') (*rest)++;

	if (**rest == '\0') return NULL;

	if (**rest == '"')
	{
		(*rest)++;
		ini = *rest;
		fin = strchr(*rest, '"');

		if (fin == NULL)
		{
			*rest = ini + strlen(ini);
			return ini;
		}

		*fin = '\0';
		*rest = fin + 1;

		return ini;
	}

	ini = *rest;

	while (**rest != '\0' && **rest != ' ' && **rest != '\t' && **rest != '\n')
		(*rest)++;

	if (**rest != '\0')
	{
		**rest = '\0';
		(*rest)++;
	}

	return ini;
}

static void cmd_add(char **rest, b_jobs *list, sqlite3 *db, int fd)
{
	char *name;
	char *source;
	char *destination;
	char *freq_str;
	char *sobra;
	long frequency;
	int id;
	int rc;

	name = next_arg(rest);
	source = next_arg(rest);
	destination = next_arg(rest);
	freq_str = next_arg(rest);

	if (name == NULL || source == NULL || destination == NULL
		|| freq_str == NULL)
	{
		proto_send_msg(fd, "faltan datos para el alta");
		return;
	}

	frequency = strtol(freq_str, &sobra, 10);

	if (*sobra != '\0')
	{
		proto_send_msg(fd, "'%s' no es un numero de segundos", freq_str);
		return;
	}

	if (frequency < BACKUP_FREQ_MIN || frequency > BACKUP_FREQ_MAX)
	{
		proto_send_msg(fd, "la frecuencia va de %d a %d segundos",
			BACKUP_FREQ_MIN, BACKUP_FREQ_MAX);
		return;
	}

	if (!is_directory(source))
	{
		proto_send_msg(fd, "el origen no es un directorio: %s", source);
		return;
	}

	if (!is_directory(destination))
	{
		proto_send_msg(fd, "el destino no es un directorio: %s", destination);
		return;
	}

	id = jobs_free_slot(list);
	if (id < 0)
	{
		proto_send_msg(fd, "no hay slots libres (maximo %d jobs)", MAX_JOBS);
		return;
	}

	rc = jobs_add_job(list, id, name, source, destination, BACKUP_MODE_SIGNAL,
		(int)frequency, JOB_ENABLED, JOB_RETENTION_DEFAULT);

	if (rc != JOBS_OK)
	{
		proto_send_msg(fd, "no se pudo agregar (codigo %d, nombre max %d)",
			rc, JOB_NAME_MAX);
		return;
	}

	if (db_insert_job(db, id, name, source, destination, BACKUP_MODE_SIGNAL,
		(int)frequency, JOB_ENABLED, JOB_RETENTION_DEFAULT) != DB_OK)
	{
		jobs_remove_job(list, id);
		proto_send_msg(fd, "no se pudo guardar el job en la base");
		return;
	}

	proto_send_msg(fd, "job %d '%s' creado", id, name);
}

static void cmd_del(char **rest, b_jobs *list, sqlite3 *db, int fd)
{
	char *id_str;
	char *sobra;
	long id;

	id_str = next_arg(rest);
	if (id_str == NULL)
	{
		proto_send_msg(fd, "falta el id");
		return;
	}

	id = strtol(id_str, &sobra, 10);

	if (*sobra != '\0' || id < 0 || id >= MAX_JOBS)
	{
		proto_send_msg(fd, "el id va de 0 a %d", MAX_JOBS - 1);
		return;
	}

	if (list->id_table[id] == NULL)
	{
		proto_send_msg(fd, "no hay ningun job con id %ld", id);
		return;
	}

	stop_worker((int)id);

	if (db_delete_job(db, (int)id) != DB_OK)
	{
		proto_send_msg(fd, "no se pudo borrar el job de la base");
		return;
	}

	jobs_remove_job(list, (int)id);

	proto_send_msg(fd, "job %ld borrado, los .tar.gz siguen en el disco", id);
}

static void cmd_reset(b_jobs *list, sqlite3 *db, int fd)
{
	int id;

	stop_workers();

	if (db_reset(db) != DB_OK)
	{
		proto_send_msg(fd, "no se pudo resetear la base");
		return;
	}

	for (id = 0; id < MAX_JOBS; id++)
		jobs_remove_job(list, id);

	proto_send_msg(fd, "reset listo: la base quedo vacia, los .tar.gz no se tocaron");
}

int handle_command(char *line, b_jobs *list, sqlite3 *db, int fd)
{
	char *rest;
	char *cmd;

	rest = line;
	cmd = next_arg(&rest);

	if (cmd == NULL) return 0;

	if (strcmp(cmd, "quit") == 0) return 1;

	if (strcmp(cmd, "add") == 0)
		cmd_add(&rest, list, db, fd);
	else if (strcmp(cmd, "del") == 0)
		cmd_del(&rest, list, db, fd);
	else if (strcmp(cmd, "reset") == 0)
		cmd_reset(list, db, fd);
	else
		proto_send_msg(fd, "no conozco '%s'", cmd);

	return 0;
}