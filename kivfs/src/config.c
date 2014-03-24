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
#include "cache.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Path should be shorter than PATH_MAX */
static char *cache_dir = "/tmp/fusetmp";
static char *cache_db = "/tmp";

static kivfs_connection_t server = {
		.ip ="192.168.2.14",
		.port = 30003,
		.socket = 0,

};


static int retry_count = 0;

static int connection_status = 0;

static kivfs_cache_policy_t cache_policy = KIVFS_FIFO;

kivfs_connection_t * get_server(void)
{
	return &server;
}

const char * get_cache_dir(void)
{
	return cache_dir;
}

const char * get_cache_db(void)
{
	return cache_db;
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

kivfs_cache_policy_t get_cache_policy(void)
{
	return cache_policy;
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

void set_cache_policy(kivfs_cache_policy_t policy)
{
	cache_policy = policy;
}

void set_server_ip(const char *ip)
{
	fprintf(stderr, "Setting new server IP %s\n", ip);
	server.ip = strdup( ip ); //XXX: posible memoryleak
}

void set_server_port(const char *port)
{
	fprintf(stderr, "Setting new server PORT %s\n", port);
	server.port = atol( port );
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
	int cache_len = strlen( cache_dir );
	char *full_path = malloc( path_len + cache_len + 1);

	memcpy(full_path, cache_dir, cache_len );
	memcpy(full_path + cache_len, path, path_len );

	/* Terminate string with zero */
	*(full_path + cache_len + path_len) = 0;

	return full_path;
}
