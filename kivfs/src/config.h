/*----------------------------------------------------------------------------
 * config.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CONFIG_H_
#define CONFIG_H_

#include <kivfs.h>

#include "cache.h"

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

/**
 * Obtain server configuration.
 * @return server config
 */
kivfs_connection_t *get_server(void);

/**
 * Get actual path to cache.
 * @return path to cache
 */
const char * get_cache_dir(void);

/**
 * Get path to the database directory
 * @return path to database directory
 */
const char *get_cache_db(void);

/**
 * Concatenate kivfs path and cache path.
 * @param path kivfs path
 * @return path to file in cache
 */
char * get_full_path(const char *path);

/**
 * Obtain maximum size of cache.
 * @return size of cache
 */
size_t get_cache_size();

kivfs_cache_policy_t get_cache_policy(void);

int get_retry_count(void);
pthread_mutex_t * get_mutex(void);
int is_connected(void);
void set_retry_count(int count);
void decrease_retry_count(void);
void set_cache_policy(kivfs_cache_policy_t policy);
void set_is_connected(int status);
void set_server_ip(const char *ip);
void set_server_port(const char *port);
void set_cache_size(size_t size);
/*----------------------------- Macros -------------------------------------*/


#endif /* CONFIG_H_ */
