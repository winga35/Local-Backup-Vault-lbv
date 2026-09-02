#ifndef UTILS_H
#define UTILS_H

enum e_utils_status {
	UTILS_OK = 0,
	UTILS_ERR_ARG = -1,
	UTILS_ERR_STAT = -2,
	UTILS_ERR_NOT_DIR = -3
};

int check_directory(const char *path);
int is_directory(const char *path);

#endif
