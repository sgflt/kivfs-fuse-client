/*----------------------------------------------------------------------------
 * kivfs_open_handling.h
 *
 *  Created on: 7. 1. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef KIVFS_OPEN_HANDLING_H_
#define KIVFS_OPEN_HANDLING_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

/**
 * Try to open remote file. If some error occurs, just ignore it and open local file.
 * @param path path to the file
 * @param file empty ofile structure
 * @param flags flags for opening
 */
void open_file_wronly(const char *path, kivfs_ofile_t *file, int flags);

/**
 * Try to open file in cache. If file or folder structure doesn't exist, then create a new file and folders.
 * @param path path to the file
 * @param file empty ofile structure
 * @param flags flags for opening
 */
void open_local_copy(const char *path, kivfs_ofile_t *file, int flags);

/**
 * Determines wether open just actual cached file or synchronise newer remote file.
 * @param path path to the file
 * @param file empty ofile structure
 * @param fi fuse structure with actual file's metadata
 */
void open_file(const char *path, kivfs_ofile_t *file, struct fuse_file_info *fi);

/*----------------------------- Macros -------------------------------------*/


#endif /* KIVFS_OPEN_HANDLING_H_ */
