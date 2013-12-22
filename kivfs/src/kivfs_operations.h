/*----------------------------------------------------------------------------
 * kivfs_operations.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef KIVFS_OPERATIONS_H_
#define KIVFS_OPERATIONS_H_

#include <kivfs.h>
#include <pthread.h>

/*---------------------------- Structures ----------------------------------*/

typedef struct kivfs_ofile_st {
	unsigned long fd;				/* local file descriptor				*/
	unsigned long r_fd;			/* remote file descriptor				*/
	kivfs_connection_t connection;	/* connection to the fs layer			*/
	pthread_mutex_t mutex;			/* connection can't be used parallel	*/
	int write;						/* write flag							*/
	struct stat stbuf;
} kivfs_ofile_t; 					/* "opened file" type					*/

/*---------------------------- CONSTANTS -----------------------------------*/

typedef enum {
	KIVFS_SYNC_UP,
	KIVFS_SYNC_DONW
} kivfs_sync_t;

#define KIVFS_WRITE_CHMOD 459

typedef int kivfs_version_t;

/*---------------------------- Variables -----------------------------------*/
extern struct fuse_operations kivfs_operations;

/*------------------- Functions: ANSI C prototypes -------------------------*/

/*----------------------------- Macros -------------------------------------*/


#endif /* KIVFS_OPERATIONS_H_ */
