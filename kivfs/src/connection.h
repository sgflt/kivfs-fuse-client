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
int kivfs_login(const char *username, const char *password);
void kivfs_session_destroy();
void kivfs_restore_connnection(void);

/*----------------------------- Macros -------------------------------------*/
#define kivfs_send_and_receive kivfs_send_and_receive_v2

#endif /* CONNECTION_H_ */
