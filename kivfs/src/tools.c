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
