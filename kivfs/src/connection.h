/*----------------------------------------------------------------------------
 * connection.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CONNECTION_H_
#define CONNECTION_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

#define KIVFS_NO_SESSION 0LL
#define KIVFS_MOVE_RQST_FORMAT "%s %s"

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

int kivfs_get_to_cache(const char *path );
int kivfs_session_init();
int kivfs_connect(kivfs_connection_t *connection, int attempts);
int kivfs_remote_readdir(const char *path);
int kivfs_remote_sync(const char *path, const char *new_path, KIVFS_VFS_COMMAND cmd);

/*----------------------------- Macros -------------------------------------*/


#endif /* CONNECTION_H_ */
