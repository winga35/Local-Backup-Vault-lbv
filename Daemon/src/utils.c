#include <stddef.h>
#include <sys/stat.h>

#include "utils.h"

int check_directory(const char *path)
{
	struct stat st;

	if (path == NULL) return UTILS_ERR_ARG;

	if (stat(path, &st) != 0) return UTILS_ERR_STAT;

	if (!S_ISDIR(st.st_mode)) return UTILS_ERR_NOT_DIR;

	return UTILS_OK;
}

int is_directory(const char *path)
{
	return check_directory(path) == UTILS_OK;
}
