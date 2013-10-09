/*----------------------------------------------------------------------------
 * cache.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CACHE_H_
#define CACHE_H_

#include <sys/types.h>

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
int cache_readdir(const char *path, void *buf, fuse_fill_dir_t filler);
int cache_getattr(const char *path, struct stat *stbuf);
mode_t cache_file_mode(const char *path);

/*----------------------------- Macros -------------------------------------*/

#define bind_text(stmt, sql_var, var) sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, sql_var), var, ZERO_TERMINATED, NULL)
#define bind_int(stmt, sql_var, var) sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, sql_var), var)

#endif /* CACHE_H_ */
