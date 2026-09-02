#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <ncurses.h>

#include "proto.h"
#include "ui_client.h"

enum e_estado {
	EST_NORMAL,
	EST_NOMBRE,
	EST_ORIGEN,
	EST_DESTINO,
	EST_FREC,
	EST_DEL,
	EST_CLOSE,
	EST_RESET1,
	EST_RESET2
};

static b_jobs *g_parcial;
static int g_id;
static int g_mode;
static int g_freq;
static int g_enabled;
static char g_src[PATH_MAX];
static char g_dst[PATH_MAX];

static int g_estado;
static int g_del_id;
static char g_msg[256];
static char g_campo[PATH_MAX];
static size_t g_campo_len;
static char g_nombre[JOB_NAME_MAX + 1];
static char g_origen[PATH_MAX];
static char g_destino[PATH_MAX];

static int connect_to_daemon(const char *path)
{
	struct sockaddr_un addr;
	int fd;

	if (path == NULL || strlen(path) >= sizeof(addr.sun_path))
		return UI_CLIENT_ERR_SOCKET;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) return UI_CLIENT_ERR_SOCKET;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) return fd;

	close(fd);

	if (errno == ENOENT || errno == ECONNREFUSED) return UI_CLIENT_NO_DAEMON;

	return UI_CLIENT_ERR_SOCKET;
}

static int handle_line(char *line, b_jobs **vista)
{
	long created;
	int id;
	int off;

	if (strcmp(line, "BUSY") == 0) return UI_CLIENT_BUSY;

	if (strncmp(line, "MSG ", 4) == 0)
	{
		snprintf(g_msg, sizeof(g_msg), "%s", line + 4);
		return UI_CLIENT_OK;
	}

	if (strncmp(line, "SNAP ", 5) == 0)
	{
		jobs_free(g_parcial);
		g_parcial = jobs_init();
		return UI_CLIENT_OK;
	}

	if (g_parcial == NULL) return UI_CLIENT_OK;

	if (strcmp(line, "END") == 0)
	{
		jobs_free(*vista);
		*vista = g_parcial;
		g_parcial = NULL;
		return UI_CLIENT_OK;
	}

	if (sscanf(line, "JOB %d %d %d %d", &g_id, &g_mode, &g_freq, &g_enabled) == 4)
		return UI_CLIENT_OK;

	if (strncmp(line, "SRC ", 4) == 0)
	{
		snprintf(g_src, sizeof(g_src), "%s", line + 4);
		return UI_CLIENT_OK;
	}

	if (strncmp(line, "DST ", 4) == 0)
	{
		snprintf(g_dst, sizeof(g_dst), "%s", line + 4);
		return UI_CLIENT_OK;
	}

	if (strncmp(line, "NAME ", 5) == 0)
	{
		jobs_add_job(g_parcial, g_id, line + 5, g_src, g_dst,
			(unsigned char)g_mode, g_freq, (unsigned char)g_enabled,
			JOB_RETENTION_DEFAULT);
		return UI_CLIENT_OK;
	}

	off = 0;
	if (sscanf(line, "BKP %d %ld %n", &id, &created, &off) == 2 && off > 0)
	{
		if (g_id >= 0 && g_id < MAX_JOBS && g_parcial->id_table[g_id] != NULL)
			job_add_backup(g_parcial->id_table[g_id], id, line + off,
				(time_t)created);
	}

	return UI_CLIENT_OK;
}

static int feed(char *buf, size_t *len, b_jobs **vista)
{
	char *ini;
	char *nl;
	int rc;

	ini = buf;

	while ((nl = memchr(ini, '\n', (size_t)((buf + *len) - ini))) != NULL)
	{
		*nl = '\0';
		rc = handle_line(ini, vista);
		ini = nl + 1;

		if (rc != UI_CLIENT_OK)
		{
			*len = 0;
			return rc;
		}
	}

	*len = (size_t)((buf + *len) - ini);
	memmove(buf, ini, *len);

	return UI_CLIENT_OK;
}

static int id_seleccionado(const b_jobs *vista, int selected)
{
	const b_job *job;
	int i;

	if (vista == NULL) return -1;

	i = 0;
	for (job = vista->head; job != NULL; job = job->next)
	{
		if (i == selected) return job->id;
		i++;
	}

	return -1;
}

static const char *pie_texto(char *buf, size_t size)
{
	switch (g_estado)
	{
	case EST_NOMBRE:
		snprintf(buf, size, "nombre: %s_", g_campo);
		break;
	case EST_ORIGEN:
		snprintf(buf, size, "origen: %s_", g_campo);
		break;
	case EST_DESTINO:
		snprintf(buf, size, "destino: %s_", g_campo);
		break;
	case EST_FREC:
		snprintf(buf, size, "frecuencia en segundos: %s_", g_campo);
		break;
	case EST_DEL:
		snprintf(buf, size, "borrar el job %d? (s/n)", g_del_id);
		break;
	case EST_CLOSE:
		snprintf(buf, size, "bajar LBV? los backups dejan de correr (s/n)");
		break;
	case EST_RESET1:
		snprintf(buf, size, "RESET: borra todos los jobs de la base (s/n)");
		break;
	case EST_RESET2:
		snprintf(buf, size, "SEGURO? no se puede deshacer. Los .tar.gz quedan (s/n)");
		break;
	default:
		if (g_msg[0] != '\0')
			snprintf(buf, size, "%s", g_msg);
		else
			snprintf(buf, size,
				"[a]lta  [d]el  [q] salir  [C] bajar LBV  [R] reset");
	}

	return buf;
}

static size_t largo_max(void)
{
	if (g_estado == EST_NOMBRE) return JOB_NAME_MAX;
	if (g_estado == EST_FREC) return 20;

	return PATH_MAX - 1;
}

static void limpiar_campo(void)
{
	g_campo_len = 0;
	g_campo[0] = '\0';
}

static void avanzar(int fd)
{
	switch (g_estado)
	{
	case EST_NOMBRE:
		memcpy(g_nombre, g_campo, g_campo_len + 1);
		g_estado = EST_ORIGEN;
		break;
	case EST_ORIGEN:
		snprintf(g_origen, sizeof(g_origen), "%s", g_campo);
		g_estado = EST_DESTINO;
		break;
	case EST_DESTINO:
		snprintf(g_destino, sizeof(g_destino), "%s", g_campo);
		g_estado = EST_FREC;
		break;
	case EST_FREC:
		proto_send_cmd(fd, "add \"%s\" \"%s\" \"%s\" %s",
			g_nombre, g_origen, g_destino, g_campo);
		g_estado = EST_NORMAL;
		break;
	default:
		g_estado = EST_NORMAL;
	}

	limpiar_campo();
}

static int es_si(int ch)
{
	return ch == 's' || ch == 'S' || ch == 'y' || ch == 'Y';
}

static int tecla(int ch, int fd, const b_jobs *vista, int *selected)
{
	if (g_estado >= EST_NOMBRE && g_estado <= EST_FREC)
	{
		if (ch == 27)
		{
			g_estado = EST_NORMAL;
			limpiar_campo();
		}
		else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
		{
			avanzar(fd);
		}
		else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
		{
			if (g_campo_len > 0) g_campo[--g_campo_len] = '\0';
		}
		else if (ch >= 32 && ch < 127 && ch != '"'
			&& g_campo_len < largo_max())
		{
			g_campo[g_campo_len++] = (char)ch;
			g_campo[g_campo_len] = '\0';
		}

		return 0;
	}

	if (g_estado == EST_DEL)
	{
		if (es_si(ch)) proto_send_cmd(fd, "del %d", g_del_id);
		g_estado = EST_NORMAL;
		return 0;
	}

	if (g_estado == EST_CLOSE)
	{
		if (es_si(ch)) proto_send_cmd(fd, "quit");
		g_estado = EST_NORMAL;
		return 0;
	}

	if (g_estado == EST_RESET1)
	{
		g_estado = es_si(ch) ? EST_RESET2 : EST_NORMAL;
		return 0;
	}

	if (g_estado == EST_RESET2)
	{
		if (es_si(ch)) proto_send_cmd(fd, "reset");
		g_estado = EST_NORMAL;
		return 0;
	}

	switch (ch)
	{
	case 'q':
		return 1;
	case 'a':
		g_msg[0] = '\0';
		limpiar_campo();
		g_estado = EST_NOMBRE;
		break;
	case 'd':
		g_del_id = id_seleccionado(vista, *selected);
		if (g_del_id >= 0)
		{
			g_msg[0] = '\0';
			g_estado = EST_DEL;
		}
		break;
	case 'C':
		g_msg[0] = '\0';
		g_estado = EST_CLOSE;
		break;
	case 'R':
		g_msg[0] = '\0';
		g_estado = EST_RESET1;
		break;
	case KEY_UP:
	case 'k':
		if (*selected > 0) (*selected)--;
		break;
	case KEY_DOWN:
	case 'j':
		if (vista != NULL && *selected + 1 < (int)vista->size) (*selected)++;
		break;
	default:
		break;
	}

	return 0;
}

int ui_client_main(const char *path)
{
	struct pollfd fds[2];
	char buf[UI_CLIENT_BUF];
	char pie[512];
	b_jobs *vista;
	size_t len;
	ssize_t n;
	int fd;
	int ch;
	int selected;
	int rc;
	int salir;

	fd = connect_to_daemon(path);
	if (fd < 0) return fd;

	if (ui_init() != UI_OK)
	{
		close(fd);
		return UI_CLIENT_ERR_UI;
	}

	vista = NULL;
	len = 0;
	selected = 0;
	rc = UI_CLIENT_OK;
	salir = 0;
	g_estado = EST_NORMAL;
	g_msg[0] = '\0';
	limpiar_campo();

	while (!salir)
	{
		if (vista == NULL || selected >= (int)vista->size)
			selected = vista == NULL ? 0 : (int)vista->size - 1;
		if (selected < 0) selected = 0;

		ui_draw(vista, selected, pie_texto(pie, sizeof(pie)));

		fds[0].fd = fd;
		fds[0].events = POLLIN;
		fds[1].fd = STDIN_FILENO;
		fds[1].events = POLLIN;

		if (poll(fds, 2, UI_CLIENT_TICK_MS) < 0)
		{
			if (errno == EINTR) continue;
			rc = UI_CLIENT_ERR_READ;
			break;
		}

		if (fds[0].revents & (POLLIN | POLLHUP))
		{
			n = read(fd, buf + len, sizeof(buf) - len);
			if (n <= 0) break;

			len += (size_t)n;

			rc = feed(buf, &len, &vista);
			if (rc != UI_CLIENT_OK) break;

			if (len == sizeof(buf))
			{
				rc = UI_CLIENT_ERR_READ;
				break;
			}
		}

		if (fds[1].revents & POLLIN)
			while ((ch = getch()) != ERR)
				if (tecla(ch, fd, vista, &selected)) salir = 1;
	}

	ui_end();

	jobs_free(vista);
	jobs_free(g_parcial);
	g_parcial = NULL;
	close(fd);

	return rc;
}
