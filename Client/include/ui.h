#ifndef UI_H
#define UI_H

#include "jobs.h"

enum e_ui_status {
	UI_OK = 0,
	UI_ERR_INIT = -1
};

int ui_init(void);
void ui_end(void);
void ui_draw(const b_jobs *list, int selected, const char *pie);

#endif
