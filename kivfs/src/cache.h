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
#include <sqlite3.h>

#include "kivfs_operations.h"
#include "cleanup.h"

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

typedef enum {
	KIVFS_FIFO,
	KIVFS_LFUSS,
	KIVFS_LRU
} kivfs_cache_policy_t;
/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

/**
 * Open database and prepare statements.
 * @return
 */
int cache_init( void );

/**
 * Free all resources from memory.
 */
void cache_close( void );

/**
 * Returns pointer to active database.
 * @return pointer to active database
 */
sqlite3 * cache_get_db( void );

/**
 * Add a new file to database. For files created in cache is second parameter NULL.
 * All metadata are gotten from stat syscall.
 * New files from server are passed as pointer.
 * @param path path to the file in kivfs namespace
 * @param file NULL if file is locally created or pointer obtained from server.
 * @return KIVFS_OK or -EEXIST if file already is in database
 */
int cache_add(const char *path, kivfs_file_t *file);

/**
 * Removes file from database.
 * @param path path to the file.
 * @return KIVFS_OK if file is succesfully removed, otherwise -ENOENT
 */
int cache_remove(const char *path);

/**
 * Move or rename object.
 * @param old_path old location
 * @param new_path new location
 * @return KIVFS_OK if object moved, -EEXIST otherwise.
 */
int cache_rename(const char *old_path, const char *new_path);

/**
 * Log performed action for further synchronisation.
 * @param path location of an object
 * @param new_path new location or NULL or mode
 * @param action action
 */
void cache_log(const char *path, const char *new_path, KIVFS_VFS_COMMAND action);

/**
 * Read whole directory.
 * @param path location which should be read
 * @param buf buffer for metadata
 * @param filler filler function
 * @return KIVFS_OK if directory has been found, -ENOENT otherwise
 */
int cache_readdir(const char *path, void *buf, fuse_fill_dir_t filler);

/**
 * Obtain attributes of file or directory.
 * 		size
 * 		atime
 * 		mtime
 * 		u rwx
 * 		g rwx
 * 		o rwx
 * 		uid
 * 		gid
 * @note uid and gid are just local values
 * @param path pat to an object
 * @param stbuf buffer to fill
 * @return KIVFS_OK if obect has been found in database, -ENOENT otherwise
 */
int cache_getattr(const char *path, struct stat *stbuf);

/**
 * Obtain mode of an object
 * @param path path of an object
 * @return it's mode
 */
mode_t cache_file_mode(const char *path);

/**
 * Change mode of an object
 * @param path path of an object
 * @param mode mode to set
 * @return KIVFS_OK, permissions should be checked befory this call.
 */
int cache_chmod(const char *path, mode_t mode);

/**
 * Perform synchronisation of pending actions and
 * upload modified files.
 * In case of error choose adequate solution.
 * @return KIVFS_OK
 */
int cache_sync(void);

/**
 * @deprecated Do NOT use this.
 * @see cache_update
 * @param files
 * @return KIVFS_OK
 */
int cache_updatedir(kivfs_list_t *files);

/**
 * Update attributes of an object from cache or from remote server.
 *
 * @param path path of an object
 * @param fi fuse_file_info stucture for local update if last parameter is NULL
 * @param file_info  or last parameter for remote update
 * @return KIVFS_OK
 */
int cache_update(const char *path, struct fuse_file_info *fi, kivfs_file_t *file_info);

/**
 * Increment read hits by 1.
 * @param path path of an object which is beeing accessed
 */
void cache_update_read_hits(const char *path );

/**
 * Increment rite hits by 1.
 * @param path path of an object which is beeing accessed
 */
void cache_update_write_hits(const char *path );

/**
 * Obtain version of an object.
 * @param path path of an object which is beeing accessed
 * @return version of an object
 */
kivfs_version_t cache_get_version(const char * path );

/**
 * Mark or unmark an object as cached.
 * @param path path of an object
 * @param cached 1 for cached or 0 for deleted
 */
void cache_set_cached(const char *path, int cached);

/**
 * Mark or unmark file as modifed.
 * @param path path of an object
 * @param status 1 for modified or 0 for up to date
 */
void cache_set_modified(const char *path, int status);

/**
 * Obtain used capacity within cache folder.
 * @return used bytes
 */
size_t cache_get_used_size( void );

/**
 * Determine whether is file cached.
 * @param path path to a file
 * @return 1 if file is cached, 0 otherwise
 */
int cache_contains(const char *path);

/**
 * Increment version of a file by 1.
 * This is useful if there is no connection and file has been modified.
 * @param path path to a file
 */
void cache_update_version(const char*path);

int cache_global_hits(kivfs_cfile_t *global_hits_client);


/*----------------------------- Macros -------------------------------------*/


#endif /* CACHE_H_ */
