/* Definitions required for inter-process communication */
#ifndef SHERIFF_H
#define SHERIFF_H

enum update_types {
	UPDATE_DIRS,
	UPDATE_STATUS,
	UPDATE_ALL
};

void queue_master_update(enum update_types type);

#endif
