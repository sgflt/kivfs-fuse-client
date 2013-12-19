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

int mkdirs(const char *path){

	char *dir = strdup( path );

	/* dirname changes original string */
	dirname( dir );

	if( access( dir, F_OK ) == -1 ){
		mkdirs( dir );
		if( mkdir( dir, S_IRWXU) ){
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
