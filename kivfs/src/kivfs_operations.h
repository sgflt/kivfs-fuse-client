/*----------------------------------------------------------------------------
 * kivfs_operations.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef KIVFS_OPERATIONS_H_
#define KIVFS_OPERATIONS_H_


/*---------------------------- Structures ----------------------------------*/
extern struct fuse_operations kivfs_operations;

/*---------------------------- CONSTANTS -----------------------------------*/

typedef enum {
	KIVFS_SYNC_UP,
	KIVFS_SYNC_DONW
} kivfs_sync_t;

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

/*----------------------------- Macros -------------------------------------*/


#endif /* KIVFS_OPERATIONS_H_ */
