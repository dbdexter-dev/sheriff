/** 
 * Definitions required for inter-process communication live here. Other than
 * that, sheriff.c is the main source file, so it doesn't really have to export
 * any function names. As a result, most of the stuff is declared static.
 */

#ifndef SHERIFF_H
#define SHERIFF_H

enum update_types {
	UPDATE_DIRS,
	UPDATE_STATUS,
	UPDATE_ALL
};

void queue_master_update();

#endif
