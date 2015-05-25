#ifndef __ARGS_H__
#define __ARGS_H__

#include "scllib.h"

#define ACTION_NONE 0
#define ACTION_LIST_COLLECTIONS 1
#define ACTION_LIST_PACKAGES 2
#define ACTION_COMMAND 3
#define ACTION_REGISTER 4
#define ACTION_DEREGISTER 5
#define ACTION_MAN 6
#define ACTION_LOAD 7
#define ACTION_UNLOAD 8
#define ACTION_VERSION 9
#define ACTION_LIST_ENABLED 10

struct scl_args {
	int action;
	char **collections; /* collection info array */
    char **colpaths; /* paths to collections */
	char *command; /* command to be run */

	bool force_flag;    /* if set, the collection deregistration was forced */
    bool exec_flag;     /* if set, exec() is used insted of system() to run command */
};

void scl_args_free(struct scl_args *args);
int scl_args_get(int argc, char *argv[], struct scl_args **_args);

#endif
