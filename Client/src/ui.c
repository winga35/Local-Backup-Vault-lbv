#include <ncurses.h>
#include <stdio.h>

#include "job.h"
#include "ui.h"

int ui_init(void)
{
	if (initscr() == NULL) return UI_ERR_INIT;

	set_escdelay(25);

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	curs_set(0);

	return UI_OK;
}

static const char *ui_status_text(const b_job *job)
{
	if (!job->enabled) return "off";

	return "on";
}

static const char *ui_next_text(const b_job *job, char *buf, size_t size)
{
	long s;

	if (!job->enabled)
	{
		snprintf(buf, size, "-");
		return buf;
	}

	s = (long)job_time_until_due(job);

	if (s <= 0)
		snprintf(buf, size, "ya");
	else if (s < 3600)
		snprintf(buf, size, "%ld:%02ld", s / 60, s % 60);
	else if (s < 86400)
		snprintf(buf, size, "%ldh%02ld", s / 3600, (s % 3600) / 60);
	else
		snprintf(buf, size, "%ldd", s / 86400);

	return buf;
}

void ui_draw(const b_jobs *list, int selected, const char *pie)
{
	const b_job *job;
	char prox[16];
	int row;
	int i;

	erase();
	box(stdscr, 0, 0);

	mvprintw(0, 3, " Local Backup Vault ");
	mvprintw(2, 3, "%-3s %-12s %-14s %-14s %-6s %-8s %-6s %s",
		"ID", "NAME", "SOURCE", "DESTINATION", "MODE", "FREQ", "STATUS",
		"PROXIMO");

	if (list == NULL || list->head == NULL)
		mvprintw(4, 4, "(no hay jobs todavia)");

	row = 3;
	i = 0;
	for (job = (list ? list->head : NULL); job != NULL; job = job->next) {
		if (row >= LINES - 3) break;

		if (i == selected) attron(A_REVERSE);
		mvprintw(row, 2, " %-3d %-12.12s %-14.14s %-14.14s %-6s %-8d %-6s %-7s ",
			job->id, job->name, job->source, job->destination,
			job->mode == BACKUP_MODE_CRON ? "cron" : "signal",
			job->frequency, ui_status_text(job),
			ui_next_text(job, prox, sizeof(prox)));
		if (i == selected) attroff(A_REVERSE);

		row++;
		i++;
	}

	mvhline(LINES - 3, 1, ACS_HLINE, COLS - 2);
	mvprintw(LINES - 2, 4, "%.*s", COLS - 6, pie);

	refresh();
}

void ui_end(void)
{
	if (isendwin()) return;

	curs_set(1);
	endwin();
}
