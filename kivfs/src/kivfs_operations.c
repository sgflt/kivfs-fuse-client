/*----------------------------------------------------------------------------
 * kivfs_operations.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/


int kivfs_getattr(const char *path, struct stat *stbuf){

}

struct fuse_operations kivfs_operations = {
	.getattr	= kivfs_getattr,
};
