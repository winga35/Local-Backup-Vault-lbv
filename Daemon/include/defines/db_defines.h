#ifndef DB_DEFINES_H
#define DB_DEFINES_H

enum e_db_status {
	DB_OK = 0,
	DB_ERR_OPEN = -1,
	DB_ERR_SCHEMA = -2,
	DB_ERR_INSERT = -3,
	DB_ERR_CLOSE = -4,
	DB_ERR_BAD_ID = -6,
	DB_ERR_SELECT = -7,
	DB_ERR_DELETE = -8
};

#endif
