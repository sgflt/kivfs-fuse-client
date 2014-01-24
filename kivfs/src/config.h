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
kivfs_server_t *get_server();
char * get_cache_path();
char * get_full_path(const char *path);
size_t get_cache_size();
int get_retry_count(void);
pthread_mutex_t * get_mutex(void);
int is_connected(void);
void set_retry_count(int count);
void decrease_retry_count(void);
void set_is_connected(int status);
void set_server_ip(const char *ip);
/*----------------------------- Macros -------------------------------------*/


#endif /* CONFIG_H_ */
