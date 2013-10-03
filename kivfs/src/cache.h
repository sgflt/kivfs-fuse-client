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
#define FILE_CHANGED 1

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
int cache_init();
void cache_close();
int is_cached(const char *path);
int set_sync_flag(const char *path, int flag);
int cache_add(const char *path, int read_hits, int write_hits, kivfs_file_type_t type);
int cache_remove(const char *path);
int cache_rename(const char *old_path, const char *new_path);
/*----------------------------- Macros -------------------------------------*/


#endif /* CACHE_H_ */
