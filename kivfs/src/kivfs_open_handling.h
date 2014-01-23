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

void open_file_wronly(const char *path, kivfs_ofile_t *file, int flags);
void open_local_copy(const char *path, kivfs_ofile_t *file, int flags);
void open_file(const char *path, kivfs_ofile_t *file, struct fuse_file_info *fi);

/*----------------------------- Macros -------------------------------------*/


#endif /* KIVFS_OPEN_HANDLING_H_ */
