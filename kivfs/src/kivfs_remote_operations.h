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

#include "kivfs_operations.h"
#include "cleanup.h"

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

#define KIVFS_MOVE_FORMAT		"%s %s" 			/**< source destination	*/
#define KIVFS_STRING_FORMAT		"%s"
#define KIVFS_OPEN_FORMAT		"%s %d"				/**< path mode			*/
#define KIVFS_ULONG_FORMAT		"%llu"
#define KIVFS_FLUSH_FORMAT		KIVFS_ULONG_FORMAT	/**< fd 				*/
#define	KIVFS_READ_FORMAT		"%llu %llu %llu" 	/**< fd size unused		*/
#define KIVFS_WRITE_FORMAT		KIVFS_READ_FORMAT	/**< fd size unused		*/
#define KIVFS_CLOSE_FORMAT		KIVFS_ULONG_FORMAT	/**< fd 				*/
#define	KIVFS_FSEEK_FORMAT		"%llu %llu"			/**< fd offset 			*/
#define KIVFS_READDIR_FORMAT	KIVFS_STRING_FORMAT	/**< path				*/
#define KIVFS_FILE_INFO_FORMAT	KIVFS_STRING_FORMAT	/**< path				*/
#define KIVFS_CHMOD_FORMAT		"%d %d %d %d %s"	/**< u g o path			*/

#define	KIVFS_UPCK_HITS_FROMAT	"%llu %llu"			/**< read write			*/
#define KIVFS_UPCK_SRV_FORMAT	"%llu %s %su"		/**< socket ip port		*/
#define KIVFS_UPCK_DIR_FORMAT	"%files"			/**< kivfs_list_t		*/
#define KIVFS_UPCK_CLNT_FORMAT	"%client"			/**< kivfs_client_t		*/
#define KIVFS_UPCK_FILE_FORMAT	"%file"				/**< kivfs_file_t		*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

/**
 * Read directiory directly from server.
 * @param path location of a directory
 * @param files list of files within directory
 * @return -ENOTCONN if connection is lost otherwise KIVFS_ERC_* or KIVFS_OK
 */
int kivfs_remote_readdir(const char *path, kivfs_list_t **files);

/**
 * Perform synchronisation.
 * @param path path of an object
 * @param new_path new path of an oject or mode
 * @param cmd command type
 * @return -ENOSYS if action is invalid, otherwise specific return value
 */
int kivfs_remote_sync(const char *path, const void *new_path, KIVFS_VFS_COMMAND cmd);

/**
 * Open remote file.
 * @param path kivfs path
 * @param mode mode of opening
 * @param file file structure to fill
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_open(const char *path, mode_t mode,  kivfs_ofile_t *file);

/**
 * Close remotely opened file
 * @param file ofile structure containing opened file
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_close(kivfs_ofile_t *file);

/**
 * Create directory on the server.
 * @param path path to directory which should be created
 * @param mode acces rights
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_mkdir(const char *path, mode_t mode);

/**
 * Remove directory from the server.
 * @param path path to directory which should be removed
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_rmdir(const char *path);

/**
 * Create an empty file on the server.
 * @param path path to a new file
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_touch(const char *path);

/**
 * Create and open a new file on the server.
 * @param path path to a new file
 * @param mode mode of opening
 * @param file ofile structure to fill
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_create(const char *path, mode_t mode, kivfs_ofile_t *file);

/**
 * Delete remote file.
 * @param path path to the file
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_unlink(const char *path);

/**
 * Read contignuous block from remote file.
 * @param file opened file structure
 * @param buf buffer to fill with data
 * @param size how maby bytes should be filled
 * @param offset from which place read $size bites
 * @return how many bytes has been actually read
 */
int kivfs_remote_read( kivfs_ofile_t *file, char *buf, size_t size, off_t offset);

/**
 * Write contignuous block to remote file.
 * @param file opened file structure
 * @param buf buffer with data
 * @param size how maby bytes should be written
 * @param offset from which place write $size bites
 * @return how many bytes has been actually written
 */
int kivfs_remote_write(kivfs_ofile_t *file, const char *buf, size_t size, off_t offset);

/**
 * Obtain remote metadata for a file.
 * @param path path of a file
 * @param file pointer to file structure to dill
 * @return -ENOTCONN or KIVFS_ERC_* or KIVFS_OK
 * @see kivfs-constants.h
 */
int kivfs_remote_file_info(const char * path, kivfs_file_t **file);

/**
 * Rename remote file.
 * @param old_path old location
 * @param new_path new location
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_rename(const char *old_path, const char *new_path);

/**
 * Change mode of remote file.
 * @param path path to a remote file
 * @param mode mode to set
 * @return KIVFS_OK or KIVFS_ERC_*
 * @see kivfs-constants.h
 */
int kivfs_remote_chmod(const char *path, mode_t mode);

/**
 * Download whole file to cache folder.
 * @param path path of a file
 * @return errno or KIVFS_OK
 */
int kivfs_get_to_cache(const char *path );

/**
 * Upload whole file from cache.
 * @param path path to a file
 * @return errno or KIVFS_OK
 */
int kivfs_put_from_cache(const char *path);

int kivfs_remote_global_hits(kivfs_cfile_t *global_hits_server );

/*----------------------------- Macros -------------------------------------*/


#endif /* KIVFS_REMOTE_OPERATIONS_H_ */
