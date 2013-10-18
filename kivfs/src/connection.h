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
#define KIVFS_MOVE_RQST_FORMAT	"%s %s" 			/* source destination	*/
#define KIVFS_STRING_FORMAT		"%s"
#define KIVFS_OPEN_FORMAT		"%s %d"				/* path mode			*/
#define KIVFS_ULONG_FORMAT		"%llu"
#define KIVFS_FLUSH_FORMAT		KIVFS_ULONG_FORMAT	/* fd 					*/
#define	 KIVFS_READ_FORMAT		"%llu %llu %llu" 	/* fd size unused		*/
#define KIVFS_CLOSE_FORMAT		KIVFS_ULONG_FORMAT	/* fd 					*/
#define KIVFS_READDIR_FORMAT	KIVFS_STRING_FORMAT	/* path					*/
#define KIVFS_FILE_INFO_FORMAT	KIVFS_STRING_FORMAT	/* path					*/

#define KIVFS_UPCK_SRV_FORMAT	"%llu %s %su"		/* socket ip port		*/
#define KIVFS_UPCK_DIR_FORMAT	"%files"			/* kivfs_list_t			*/
#define KIVFS_UPCK_CLNT_FORMAT	"%client"			/* kivfs_client_t		*/
#define KIVFS_UPCK_FILE_FORMAT	"%file"				/* kivfs_file_t			*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

int kivfs_get_to_cache(const char *path );
int kivfs_session_init();
int kivfs_connect(kivfs_connection_t *connection, int attempts);
void kivfs_session_disconnect();
int kivfs_remote_readdir(const char *path, kivfs_list_t **files);
int kivfs_remote_sync(const char *path, const char *new_path, KIVFS_VFS_COMMAND cmd);
int kivfs_remote_open(const char *path, mode_t mode,  kivfs_ofile_t *file);
int kivfs_remote_close(kivfs_ofile_t *file);
int kivfs_remote_mkdir(const char *path);
int kivfs_remote_rmdir(const char *path);
int kivfs_remote_touch(const char *path);
int kivfs_remote_create(const char *path, mode_t mode, kivfs_ofile_t *file);
int kivfs_remote_unlink(const char *path);

/*----------------------------- Macros -------------------------------------*/


#endif /* CONNECTION_H_ */
