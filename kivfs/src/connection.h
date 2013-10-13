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

#define KIVFS_NO_SESSION		0LL
#define KIVFS_MOVE_RQST_FORMAT	"%s %s" 			/* source destination */
#define KIVFS_STRING_FORMAT		"%s"
#define KIVFS_OPEN_FORMAT		"%s %d"				/* path mode */
#define KIVFS_FLUSH_FORMAT		"%llu"				/* fd */
#define	 KIVFS_READ_FORMAT		"%llu %llu %llu" 	/* fd size "asi offset sakra kdo to má luštit bez dokumentace" */
#define KIVFS_CLOSE_FORMAT		KIVFS_STRING_FORMAT	/* path */
#define KIVFS_READDIR_FORMAT	KIVFS_STRING_FORMAT	/* path */

#define KIVFS_UPCK_SRV_FORMAT	"%llu %s %su"		/* socket ip port */
#define KIVFS_UPCK_DIR_FORMAT	"%files"			/* kivfs_list_t */

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

int kivfs_get_to_cache(const char *path );
int kivfs_session_init();
int kivfs_connect(kivfs_connection_t *connection, int attempts);
int kivfs_remote_readdir(const char *path, kivfs_list_t **files);
int kivfs_remote_sync(const char *path, const char *new_path, KIVFS_VFS_COMMAND cmd);

/*----------------------------- Macros -------------------------------------*/


#endif /* CONNECTION_H_ */
