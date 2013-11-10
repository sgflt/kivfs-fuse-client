/*----------------------------------------------------------------------------
 * cache.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CACHE_H_
#define CACHE_H_

#include <fuse.h>
#include <sys/types.h>

#include "kivfs_operations.h"

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
int cache_init();
void cache_close();
int cache_add(const char *path, kivfs_file_t *file);
int cache_remove(const char *path);
int cache_rename(const char *old_path, const char *new_path);
void cache_log(const char *path, const char *new_path, KIVFS_VFS_COMMAND action);
int cache_readdir(const char *path, void *buf, fuse_fill_dir_t filler);
int cache_getattr(const char *path, struct stat *stbuf);
mode_t cache_file_mode(const char *path);
int cache_chmod(const char *path, mode_t mode);
int cache_sync();
int cache_updatedir(kivfs_list_t *files);
int cache_update(const char *path, struct fuse_file_info *fi, kivfs_file_t *file_info);
void cache_update_read_hits(const char *path );
void cache_update_write_hits(const char *path );
kivfs_version_t cache_get_version(const char * path );


/*----------------------------- Macros -------------------------------------*/

#define bind_text(stmt, sql_var, var) sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, sql_var), var, ZERO_TERMINATED, NULL)
#define bind_int(stmt, sql_var, var) sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, sql_var), var)

#endif /* CACHE_H_ */
