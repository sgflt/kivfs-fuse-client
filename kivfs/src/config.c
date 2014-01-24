/*----------------------------------------------------------------------------
 * config.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#include <fuse.h>
#include <kivfs.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Path should be shorter than PATH_MAX */
static char *cache_path = "/tmp/fusetmp"; //TODO: nastavit v init

kivfs_server_t server = {
		.id = 0,
		.name = "zcu",
		.priority = 0,
		.public_ip = "192.168.2.14", //147.228.63.46 147.228.63.47 147.228.63.48 147.228.67.121
};

kivfs_client_t  *client = NULL;

int retry_count = 0;

int connection_status = 0;

kivfs_server_t * get_server(void)
{
	return &server;
}

char * get_cache_path(void)
{
	return cache_path;
}

size_t get_cache_size(void)
{
	return 128;
}

int get_retry_count(void)
{
	return retry_count;
}

pthread_mutex_t * get_mutex(void)
{
	return &mutex;
}

int is_connected(void)
{
	int status;

	pthread_mutex_lock( &mutex );
	{
		status = connection_status;
	}
	pthread_mutex_unlock( &mutex );

	return status;
}

void set_retry_count(int count)
{
	retry_count = count;
}

void decrease_retry_count(void)
{
	retry_count--;
}

void set_is_connected(int status)
{
	pthread_mutex_lock( &mutex );
	{
		connection_status = status;
	}
	pthread_mutex_unlock( &mutex );
}


void set_server_ip(const char *ip)
{
	fprintf(stderr, "Setting new server IP %s\n", ip);
	server.public_ip = strdup( ip ); //XXX: posible memoryleak
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
