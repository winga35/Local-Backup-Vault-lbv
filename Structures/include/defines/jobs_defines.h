#ifndef JOBS_DEFINES_H
#define JOBS_DEFINES_H

#define MAX_JOBS 50

enum e_jobs_status {
	JOBS_OK = 0,
	JOBS_FULL = -1,
	JOBS_BAD_ID = -2,
	JOBS_ID_TAKEN = -3,
	JOBS_NO_MEM = -4,
	JOBS_DB_ERROR = -5,
	JOBS_BAD_ARG = -6,
	JOBS_NOT_FOUND = -9
};

#endif
