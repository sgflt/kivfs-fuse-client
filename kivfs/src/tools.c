/*----------------------------------------------------------------------------
 * tools.c
 *
 *  Created on: 12. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kivfs.h>
#include <fuse.h>

#include "main.h"

int mkdirs(const char *path){

	char *dir = strdup( path );

	/* dirname changes original string */
	dirname( dir );

	if( access( dir, F_OK ) == -1 ){
		mkdirs( dir );
		if( mkdir( dir, S_IRWXU) ){
			perror("mkdirs");
			free( dir );
			return -errno;
		}
	}

	free( dir );
	return 0;
}

int recreate_and_open(const char *path, mode_t mode)
{
	if ( mkdirs( path ) )
	{
		return -errno;
	}

	/* Try again */
	return creat(path, mode);
}

void print_open_mode(int mode)
{
	fprintf(stderr,"\n\033[33;1m\t PRINT MODE: %o\033[0;0m\n", mode);
	if ( mode & O_RDONLY )
		fprintf(stderr, "\n\033[33;1m\tmode: O_RDONLY\033[0;0m\n");

	if ( mode & O_WRONLY )
		fprintf(stderr,"\n\033[33;1m\tmode: O_WRONLY\033[0;0m\n");

	if ( mode & O_RDWR )
		fprintf(stderr,"\n\033[33;1m\tmode: O_RDWR\033[0;0m\n");

	if ( mode & O_APPEND)
		fprintf(stderr,"\n\033[33;1m\tmode: O_APPEND\033[0;0m\n");

	if ( mode & O_ASYNC)
		fprintf(stderr,"\n\033[33;1m\tmode: O_ASYNC\033[0;0m\n");

	if ( mode & O_ACCMODE)
		fprintf(stderr,"\n\033[33;1m\tmode: O_ACCMODE\033[0;0m\n");

	if ( mode & O_CLOEXEC)
		fprintf(stderr,"\n\033[33;1m\tmode: O_CLOEXEC\033[0;0m\n");

	if ( mode & O_CREAT)
		fprintf(stderr,"\n\033[33;1m\tmode: O_CREAT\033[0;0m\n");

	if ( mode & O_DIRECTORY)
		fprintf(stderr,"\n\033[33;1m\tmode: O_DIRECTORY\033[0;0m\n");

	if ( mode & O_DSYNC)
		fprintf(stderr,"\n\033[33;1m\tmode: O_DSYNC\033[0;0m\n");

	if ( mode & O_EXCL)
		fprintf(stderr,"\n\033[33;1m\tmode: O_EXCL\033[0;0m\n");

	if ( mode & O_FSYNC)
		fprintf(stderr,"\n\033[33;1m\tmode: O_FSYNC\033[0;0m\n");

	if ( mode & O_NDELAY)
		fprintf(stderr,"\n\033[33;1m\tmode: O_NDELAY\033[0;0m\n");

	if ( mode & O_NOCTTY)
		fprintf(stderr,"\n\033[33;1m\tmode: O_NOCTTY\033[0;0m\n");

	if ( mode & O_NOFOLLOW)
		fprintf(stderr,"\n\033[33;1m\tmode: O_NOFOLLOW\033[0;0m\n");

	if ( mode & O_NONBLOCK)
		fprintf(stderr,"\n\033[33;1m\tmode: O_NONBLOCK\033[0;0m\n");

	if ( mode & O_RSYNC)
		fprintf(stderr,"\n\033[33;1m\tmode: O_RSYNC\033[0;0m\n");

	if ( mode & O_SYNC)
		fprintf(stderr,"\n\033[33;1m\tmode: O_OSYNC\033[0;0m\n");

	if ( mode & O_TRUNC)
		fprintf(stderr,"\n\033[33;1m\tmode: O_TRUNC\033[0;0m\n");

}

void print_stat(struct stat *stbuf)
{
	printf( VT_INFO "\n\tinode: %lu\033[0;0m\n", stbuf->st_ino);
	printf("\033[33;1m\t: %u\033[0;0m\n", stbuf->st_mode);
	printf("\n\033[33;1m\tmode: %c%c%c %c%c%c %c%c%c\033[0;0m\n",
				stbuf->st_mode & S_IRUSR ? 'r' : '-',
				stbuf->st_mode & S_IWUSR ? 'w' : '-',
				stbuf->st_mode & S_IXUSR ? 'x' : '-',
				stbuf->st_mode & S_IRGRP ? 'r' : '-',
				stbuf->st_mode & S_IWGRP ? 'w' : '-',
				stbuf->st_mode & S_IXGRP ? 'x' : '-',
				stbuf->st_mode & S_IROTH ? 'r' : '-',
				stbuf->st_mode & S_IWOTH ? 'w' : '-',
				stbuf->st_mode & S_IXOTH ? 'x' : '-'
				);
	printf("\033[33;1m\tblksize: %ld\033[0;0m\n", stbuf->st_blksize);
	printf("\033[33;1m\tblkcnt: %lu\033[0;0m\n" VT_NORMAL, stbuf->st_blocks);
}

void print_fuse_file_info(struct fuse_file_info *fi)
{
	printf("\n\033[33;1m\tfh: %lu\033[0;0m\n", fi->fh);
	printf("\033[33;1m\twritepage: %u\033[0;0m\n", fi->writepage);
	printf("\033[33;1m\tdirect_io: %u\033[0;0m\n", fi->direct_io);
	printf("\033[33;1m\tkeep_cache: %u\033[0;0m\n", fi->keep_cache);
	printf("\033[33;1m\tflush: %u\033[0;0m\n", fi->flush);
	printf("\033[33;1m\tnonseekable: %u\033[0;0m\n", fi->nonseekable);
	printf("\033[33;1m\tpadding: %u\033[0;0m\n", fi->padding);
	printf("\033[33;1m\tlock_owner: %lu\033[0;0m\n", fi->lock_owner);

}


int kivfs2unix_err(int error)
{
	switch ( error )
	{
		case KIVFS_OK:
			return KIVFS_OK;

		case KIVFS_ERC_FILE_LOCKED:
			return 	EMFILE;

		case KIVFS_ERC_CONNECTION_TERMINATED:
			return ENOTCONN;

		case KIVFS_ERC_NO_SUCH_DIR:
		case KIVFS_ERC_NO_SUCH_FILE:
			return ENOENT;

		case KIVFS_ERC_FILE_EXISTS:
		case KIVFS_ERC_DIR_EXISTS:
			return EEXIST;

		case KIVFS_ERC_DIR_NOT_EMPTY:
			return ENOTEMPTY;

		default:
			return KIVFS_ERC_UNKNOWN_ERROR_CODE;
	}
}
