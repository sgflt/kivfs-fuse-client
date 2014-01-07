/*----------------------------------------------------------------------------
 * connection.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CONNECTION_H_
#define CONNECTION_H_

#include <kivfs.h>

#include "kivfs_operations.h"

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

#define KIVFS_NO_SESSION		0LL

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/


int kivfs_session_init();
int kivfs_connect(kivfs_connection_t *connection, int attempts);
void kivfs_session_disconnect();

/*----------------------------- Macros -------------------------------------*/


#endif /* CONNECTION_H_ */
