/*----------------------------------------------------------------------------
 * cache.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CACHE_H_
#define CACHE_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/
typedef enum{
	LOG_CONFLICT
} cache_conflict_t;


/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
int cache_init();
void cache_close();
int cache_add(const char *path, int read_hits, int write_hits, kivfs_file_type_t type);
int cache_remove(const char *path);
int cache_rename(const char *old_path, const char *new_path);
void cache_log(const char *path, const char *new_path, KIVFS_VFS_COMMAND action);

/*----------------------------- Macros -------------------------------------*/


#endif /* CACHE_H_ */
