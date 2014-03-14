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
char * get_cache_path(void);

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


int get_retry_count(void);
pthread_mutex_t * get_mutex(void);
int is_connected(void);
void set_retry_count(int count);
void decrease_retry_count(void);
void set_is_connected(int status);
void set_server_ip(const char *ip);
void set_server_port(const char *port);
/*----------------------------- Macros -------------------------------------*/


#endif /* CONFIG_H_ */
