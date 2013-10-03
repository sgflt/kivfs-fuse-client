/*----------------------------------------------------------------------------
 * config.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"


/* Path should be shorter than PATH_MAX */
static char *cache_path = "/tmp/fusetmp"; //TODO: nastavit v init



char * get_cache_path(){
	return cache_path;
}

/*----------------------------------------------------------------------------
   Function : get_full_path(const char *path)
   In       : Path in mounted directory.
   Out      : Path in cache.
   Job      : Concatenates two paths into single absolute path.
   Notice   : Returned pointer have to be freed.
 ---------------------------------------------------------------------------*/
char * get_full_path(const char *path){
	int path_len = strlen( path );
	int cache_len = strlen( cache_path );
	char *full_path = malloc( path_len + cache_len + 1);

	memcpy(full_path, cache_path, cache_len );
	memcpy(full_path + cache_len, path, path_len );

	/* Terminate string with zero */
	*(full_path + cache_len + path_len) = 0;

	return full_path;
}