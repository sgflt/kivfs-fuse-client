/*----------------------------------------------------------------------------
 * kivfs_remote_operations.h
 *
 *  Created on: 7. 1. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef KIVFS_REMOTE_OPERATIONS_H_
#define KIVFS_REMOTE_OPERATIONS_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

#define KIVFS_MOVE_FORMAT		"%s %s" 			/* source destination	*/
#define KIVFS_STRING_FORMAT		"%s"
#define KIVFS_OPEN_FORMAT		"%s %d"				/* path mode			*/
#define KIVFS_ULONG_FORMAT		"%llu"
#define KIVFS_FLUSH_FORMAT		KIVFS_ULONG_FORMAT	/* fd 					*/
#define	 KIVFS_READ_FORMAT		"%llu %llu %llu" 	/* fd size unused		*/
#define KIVFS_WRITE_FORMAT		KIVFS_READ_FORMAT	/* fd size unused		*/
#define KIVFS_CLOSE_FORMAT		KIVFS_ULONG_FORMAT	/* fd 					*/
#define	 KIVFS_FSEEK_FORMAT		"%llu %llu"			/* fd offset 			*/
#define KIVFS_READDIR_FORMAT	KIVFS_STRING_FORMAT	/* path					*/
#define KIVFS_FILE_INFO_FORMAT	KIVFS_STRING_FORMAT	/* path					*/
#define KIVFS_CHMOD_FORMAT		"%d %d %d %d %s"	/* u g o path			*/

#define KIVFS_UPCK_SRV_FORMAT	"%llu %s %su"		/* socket ip port		*/
#define KIVFS_UPCK_DIR_FORMAT	"%files"			/* kivfs_list_t			*/
#define KIVFS_UPCK_CLNT_FORMAT	"%client"			/* kivfs_client_t		*/
#define KIVFS_UPCK_FILE_FORMAT	"%file"				/* kivfs_file_t			*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

int kivfs_remote_readdir(const char *path, kivfs_list_t **files);
int kivfs_remote_sync(const char *path, const char *new_path, KIVFS_VFS_COMMAND cmd);
int kivfs_remote_open(const char *path, mode_t mode,  kivfs_ofile_t *file);
int kivfs_remote_close(kivfs_ofile_t *file);
int kivfs_remote_mkdir(const char *path);
int kivfs_remote_rmdir(const char *path);
int kivfs_remote_touch(const char *path);
int kivfs_remote_create(const char *path, mode_t mode, kivfs_ofile_t *file);
int kivfs_remote_unlink(const char *path);
int kivfs_remote_read( kivfs_ofile_t *file, char *buf, size_t size, off_t offset);
int kivfs_remote_write(kivfs_ofile_t *file, const char *buf, size_t size, off_t offset);
int kivfs_remote_file_info(const char * path, kivfs_file_t **file);
int kivfs_remote_rename(const char *old_path, const char *new_path);
int kivfs_remote_chmod(const char *path, mode_t mode);

int kivfs_get_to_cache(const char *path );
int kivfs_put_from_cache(const char *path);

/*----------------------------- Macros -------------------------------------*/


#endif /* KIVFS_REMOTE_OPERATIONS_H_ */
