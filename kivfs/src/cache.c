/*----------------------------------------------------------------------------
 * cache.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <sqlite3.h>
#include <kivfs.h>
#include <pthread.h>
#include <limits.h>

#include "main.h"
#include "config.h"
#include "cache.h"
#include "prepared_stmts.h"
#include "connection.h"
#include "kivfs_remote_operations.h"
#include "cleanup.h"
#include "cache-algo-lfuss.h"
#include "cache-algo-wlfuss.h"
#include "cache-algo-common.h"

//XXX: odstranit nepotřebné includy
#include <stdio.h>


static sqlite3 *db;

static sqlite3_stmt *add_stmt;
static sqlite3_stmt *rename_stmt;
static sqlite3_stmt *remove_stmt;
static sqlite3_stmt *log_stmt;
static sqlite3_stmt *readdir_stmt;
static sqlite3_stmt *getattr_stmt;
static sqlite3_stmt *get_fmod_stmt;
static sqlite3_stmt *chmod_stmt;
static sqlite3_stmt *update_stmt;
static sqlite3_stmt *get_version_stmt;
static sqlite3_stmt *inc_read_hits_stmt;
static sqlite3_stmt *inc_write_hits_stmt;
static sqlite3_stmt *set_cached_stmt;
static sqlite3_stmt *set_modified_stmt;
static sqlite3_stmt *get_used_size_stmt;
static sqlite3_stmt *update_version_stmt;
static sqlite3_stmt *global_hits_stmt;
static sqlite3_stmt *update_read_hits_stmt;
static sqlite3_stmt *update_time_stmt;

/* Each function can be processed without race condition */
static pthread_mutex_t add_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rename_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t remove_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t readdir_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t getattr_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t get_fmod_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t chmod_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t get_version_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t inc_read_hits_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t inc_write_hits_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t set_cached_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t set_modified_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t inc_version_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t update_read_hits_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t update_time_mutex = PTHREAD_MUTEX_INITIALIZER;

static double (*read_hits_fn) (const void *);

static inline void cache_check_stmt(void (*initializer)(sqlite3_stmt **, sqlite3 *), sqlite3_stmt **stmt)
{
	if( !*stmt ){
		initializer(stmt, db);
	}
}

static void choose_read_hits_algo(void)
{
	kivfs_cache_policy_t policy = get_cache_policy();

	switch ( policy )
	{
		case KIVFS_LFUSS:
			fprintf(stderr, VT_YELLOW "LFUSS POLICY %d\n" VT_NORMAL, policy);
			read_hits_fn = lfuss_read_hits;
			break;

		case KIVFS_WLFUSS:
			fprintf(stderr, VT_YELLOW "WLFUSS POLICY %d\n" VT_NORMAL, policy);
			read_hits_fn = wlfuss_read_hits;
			break;

		default:
			fprintf(stderr, VT_YELLOW "DEFAULT POLICY %d\n" VT_NORMAL, policy);
			read_hits_fn = read_hit_1;
			break;
	}
}

double (*cache_read_hits_fn(void)) (const void *)
{
	return read_hits_fn;
}

static void prepare_statements(void)
{
	prepare_cache_add				(&add_stmt,					db);
	prepare_cache_rename			(&rename_stmt,				db);
	prepare_cache_remove			(&remove_stmt,				db);
	prepare_cache_log				(&log_stmt,					db);
	prepare_cache_readdir			(&readdir_stmt,				db);
	prepare_cache_getattr			(&getattr_stmt,				db);
	prepare_cache_file_mode			(&get_fmod_stmt,			db);
	prepare_cache_chmod				(&chmod_stmt,				db);
	prepare_cache_update			(&update_stmt,				db);
	prepare_cache_get_version		(&get_version_stmt,			db);
	prepare_cache_inc_read_hits		(&inc_read_hits_stmt,		db);
	prepare_cache_inc_write_hits	(&inc_write_hits_stmt,		db);
	prepare_cache_set_cached		(&set_cached_stmt,			db);
	prepare_cache_set_modified		(&set_modified_stmt,		db);
	prepare_cache_get_used_size		(&get_used_size_stmt,		db);
	prepare_cache_update_version	(&update_version_stmt,		db);
	prepare_cache_global_hits		(&global_hits_stmt,			db);
	prepare_cache_update_read_hits	(&update_read_hits_stmt, 	db);
	prepare_cache_update_time		(&update_time_stmt, 		db);
}

static void create_triggers(void)
{
	int res;
	char *sql = "CREATE TRIGGER IF NOT EXISTS trigger_normalize_hits AFTER UPDATE	"
				"ON files												"
				"WHEN (SELECT max(read_hits) FROM files) > 15			"
				"OR (SELECT max(write_hits) FROM files) > 15			"
				"BEGIN													"
				"	UPDATE files SET read_hits = read_hits / 2,			"
				"	write_hits = write_hits / 2;						"
				"END													";

	res = sqlite3_exec(db,
					sql,
					NULL,
					NULL,
					NULL
					);

	if ( res != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "%s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}
}

static void create_indexes(void)
{
	int res;

	res = sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS path_idx ON files (path)", NULL, NULL, NULL);

	if ( res != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "%s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}

	res = sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS cached_idx ON files (cached)", NULL, NULL, NULL);

	if ( res != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "%s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}
}

int cache_init()
{
	int res;
	char db_path[ PATH_MAX ] = {0};
	char *db_filename = "/kivfs.db";

	strncat(db_path, get_cache_db(), PATH_MAX - 1);
	strncat(db_path, db_filename, PATH_MAX - 1);

	printf("db path: %s mutex: %d \n", db_path, sqlite3_threadsafe());

	res = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
	if ( res != SQLITE_OK )
	{

		res = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
		if ( res != SQLITE_OK )
		{
			perror("Unable to create a database file.");
			return EXIT_FAILURE;
		}

		fprintf(stderr, VT_ACTION "mcreating table\n\n" VT_NORMAL);
		res = sqlite3_exec(db,
						"PRAGMA journal_mode=WAL",
					    NULL,
					    NULL,
					    NULL
						);

		if ( res != SQLITE_OK )
		{
			fprintf(stderr, VT_ERROR "cache_init %s\n" VT_NORMAL, sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}

		res = sqlite3_exec(db,
				"CREATE TABLE 'main'.'files' (				"
			    "path 			TEXT PRIMARY KEY,			"
				"size 			INT NOT NULL,				"
				"mtime 			INT NOT NULL,				"
				"atime 			INT NOT NULL,				"
				"mode 			INT NOT NULL,				"
				"own 			TEXT NOT NULL,				"
				"grp 			TEXT NOT NULL,				"
			    "read_hits 		REAL DEFAULT ('0.0'),		"
				"write_hits 	INT DEFAULT ('0'),			"
				"srv_read_hits 	INT DEFAULT ('0'),			"
				"srv_write_hits INT DEFAULT ('0'),			"
				"cached 		INT DEFAULT ('0'),			"
				"modified		INT DEFAULT ('0'),			"
				"version 		INT NOT NULL)				",
			    NULL,
			    NULL,
			    NULL
				);

		if ( res != SQLITE_OK )
		{
			fprintf(stderr, VT_ERROR "cache_init %s\n" VT_NORMAL, sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}

		res = sqlite3_exec(db,
				"CREATE TABLE 'main'.'log' (					"
				"path 			TEXT PRIMARY KEY,				"
				"new_path 		TEXT UNIQUE,					"
				"action 		INT NOT NULL)					",
				NULL,
				NULL,
				NULL
				);

		if( res != SQLITE_OK ){
			fprintf(stderr, VT_ERROR "cache_init %s\n" VT_NORMAL, sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}
	}

	prepare_statements();
	create_triggers();
	create_indexes();
	choose_read_hits_algo();

	return res;
}


/*----------------------------------------------------------------------------
   Function : cache_close()
   In       : void
   Out      : void
   Job      : Clean all resources from memory.
   Notice   :
 ---------------------------------------------------------------------------*/
void cache_close()
{
	sqlite3_finalize( add_stmt );
	sqlite3_finalize( rename_stmt );
	sqlite3_finalize( remove_stmt );
	sqlite3_finalize( log_stmt );
	sqlite3_finalize( readdir_stmt );
	sqlite3_finalize( getattr_stmt );
	sqlite3_finalize( get_fmod_stmt );
	sqlite3_finalize( chmod_stmt );
	sqlite3_finalize( update_stmt );
	sqlite3_finalize( get_version_stmt );
	sqlite3_finalize( inc_read_hits_stmt );
	sqlite3_finalize( inc_write_hits_stmt );
	sqlite3_finalize( set_cached_stmt );
	sqlite3_finalize( update_version_stmt );
	sqlite3_finalize( update_read_hits_stmt );

	sqlite3_close( db );
}

sqlite3 * cache_get_db( void )
{
	return db;
}

int cache_add(const char *path, kivfs_file_t *file)
{
	int res;

	if ( file )
	{
		char tmp[ PATH_MAX ] = {0};
		mode_t mode;

		strncat(tmp, path, PATH_MAX - 1);
		if ( strcmp(path, "/") != 0 )
		{
			strncat(tmp, "/", PATH_MAX - 1);
		}
		strncat(tmp, file->name, PATH_MAX - 1);

		mode = (file->acl->owner << 6) | (file->acl->group << 3) | (file->acl->other);

		pthread_mutex_lock( &add_mutex );

		bind_text(add_stmt,	":path",			tmp);
		bind_int(add_stmt,	":size",			file->size);
		bind_int(add_stmt,	":mtime",			file->mtime);
		bind_int(add_stmt,	":atime",			file->atime);
		bind_int(add_stmt,	":mode",	 		mode | (file->type == FILE_TYPE_DIRECTORY ? S_IFDIR : S_IFREG) );
		bind_text(add_stmt,	":owner",			file->owner);
		bind_text(add_stmt,	":group",			file->group);
		bind_int(add_stmt,	":srv_read_hits",	file->read_hits);
		bind_int(add_stmt,	":srv_write_hits",	file->write_hits);
		bind_int(add_stmt,	":version",			file->version);

		kivfs_print_file( file );
	}
	else
	{
		struct stat stbuf;
		char *full_path = get_full_path( path );

		stat(full_path, &stbuf);
		free( full_path );

		fprintf(stderr, VT_ERROR "cache_add: %s mode: %o \033[0;0m\n", path, stbuf.st_mode);

		pthread_mutex_lock( &add_mutex );

		bind_text(add_stmt, ":path", path);
		bind_int(add_stmt, ":size", stbuf.st_size);
		bind_int(add_stmt, ":mtime", stbuf.st_mtim.tv_sec);
		bind_int(add_stmt, ":atime", stbuf.st_atim.tv_sec);
		bind_int(add_stmt, ":mode", stbuf.st_mode);
		bind_int(add_stmt, ":owner", stbuf.st_uid);
		bind_int(add_stmt, ":group", stbuf.st_gid);
		bind_int(add_stmt, ":version", 1);

		printf("\n\033[33;1m\tmode: %c%c%c %c%c%c %c%c%c  %o %o \033[0;0m\n",
									stbuf.st_mode & S_IRUSR ? 'r' : '-',
									stbuf.st_mode & S_IWUSR ? 'w' : '-',
									stbuf.st_mode & S_IXUSR ? 'x' : '-',
									stbuf.st_mode & S_IRGRP ? 'r' : '-',
									stbuf.st_mode & S_IWGRP ? 'w' : '-',
									stbuf.st_mode & S_IXGRP ? 'x' : '-',
									stbuf.st_mode & S_IROTH ? 'r' : '-',
									stbuf.st_mode & S_IWOTH ? 'w' : '-',
									stbuf.st_mode & S_IXOTH ? 'x' : '-',
											stbuf.st_mode,
											S_IFDIR | S_IRWXU
								);
	}

	sqlite3_step( add_stmt );

	if ( (res = sqlite3_reset( add_stmt )) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_add: %s\033[0;0m %s %d\n", path, sqlite3_errmsg( db ), res);
		res = -EEXIST;
	}

	pthread_mutex_unlock( &add_mutex );
	return res;
}

int cache_rename(const char *old_path, const char *new_path){

	pthread_mutex_lock( &rename_mutex );

	bind_text(rename_stmt, ":old_path", old_path);
	bind_text(rename_stmt, ":new_path", new_path);

	/* overwrite destination */
	cache_remove( new_path );

	sqlite3_step( rename_stmt );

	if ( sqlite3_reset( rename_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_rename: %s\033[0;0m %s %d\n", old_path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &rename_mutex );
	return KIVFS_OK;
}

int cache_remove(const char *path){

	int res = 0;
	pthread_mutex_lock( &remove_mutex );

	bind_text(remove_stmt, ":path", path);

	sqlite3_step( remove_stmt );

	if ( sqlite3_reset( remove_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_remove: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
		res = -ENOENT;
	}

	pthread_mutex_unlock( &remove_mutex );
	return res;
}

/**
 * Solves conflict in log table.
 *
 *
 *		KIVFS_UNLINK:
 *		KIVFS_RMDIR:
 *		The idea is that if a file is created in offline mode and then removed before sync operation,
 *		conflict in log occurs between (KIVFS_TOUCH or KIVFS_MKDIR) and (KIVFS_RMDIR or KIVFS_UNLINK).
 *		It is useless sending create request to a server and immediately unlink newly created file.
 *		So if UNLINK/RMDIR occurs after TOUCH/MKDIR the record is deleted as anti-matter and matter annihilates.
 *
 *		WRITE to unlinked file is not opossible.
 *
 *
 *		KIVFS_TOUCH:
 *		KIVFS_WRITE:
 *		Sometimes can occur opposite situation. Remote file is deleted locally and in near future before sync
 *		recreated. Informing server about unlink is not efficient due to overhead like in previous situation.
 *		KIVFS_UNLINK is changed to KIVFS_WRITE. Why WRITE? Because it means that the file is changed and
 *		have to be synced as soon as possible.
 *
 *		WRITE can also conflict with another WRITE. In this case WRITE flag stays the same.
 * @param path Path to the source file is mandatory.
 * @param new_path Path to the new file, if action is KIVFS_MOVE.
 * @param action Type of action. All types are declared in kivfs-cmdn-vfs.h
 * @see kivfs-cmdn-vfs.h
 */
static void cache_solve_conflict(const char *path, const char *new_path, KIVFS_VFS_COMMAND action)
{
	static sqlite3_stmt *move_stmt = NULL;
	static sqlite3_stmt *unlink_stmt= NULL;
	static sqlite3_stmt *unlink_remote_stmt = NULL;
	static sqlite3_stmt *write_stmt = NULL; //XXX: deprecated
	static sqlite3_stmt *chmod_stmt = NULL;

	switch( action ){
		case KIVFS_RMDIR:
			fprintf(stderr, VT_ERROR "LOG RMDIR: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			/* fall through */

		case KIVFS_UNLINK:
			fprintf(stderr, VT_ERROR "LOG UNLINK: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			cache_check_stmt(prepare_log_remove, &unlink_stmt);
			bind_text(unlink_stmt, ":path", path);

			/* Deletes single row containging action == KIVFS_MKDIR or KIVFS_TOUCH */
			sqlite3_step( unlink_stmt );
			if ( sqlite3_reset( unlink_stmt ) != SQLITE_OK )
			{
				fprintf(stderr, VT_ERROR "LOG update error: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			}

			/* Nothing has been removed, but we have to solve conflict between KIVFS_WRITE or KIVFS_TRUNCATE */
			else if( sqlite3_changes( db ) == 0 )
			{
				cache_check_stmt(prepare_log_remote_remove, &unlink_remote_stmt);

				bind_text(unlink_remote_stmt, ":path", path);
				bind_int(unlink_remote_stmt, ":action", action);

				sqlite3_step( unlink_remote_stmt );
				if ( sqlite3_reset( unlink_remote_stmt ) != SQLITE_OK )
				{
					fprintf(stderr, VT_ERROR "LOG update unlink error: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
				}
			}

			break;

		case KIVFS_MOVE:
				fprintf(stderr, VT_ERROR "LOG MOVE: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			cache_check_stmt(prepare_log_move, &move_stmt);
			bind_text	(move_stmt,	":old_path", path);
			bind_text	(move_stmt,	":new_path", new_path);
			sqlite3_step( move_stmt );

			//TODO what if a file is moved to previous name? DELETE movement in log!
			if ( sqlite3_reset( move_stmt ) != SQLITE_OK )
			{
				fprintf(stderr, VT_ERROR "LOG update move error: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			}
			break;

		case KIVFS_TOUCH:
			/* fall through */

		case KIVFS_WRITE:
			cache_check_stmt(prepare_log_write, &write_stmt);
			bind_text(write_stmt, ":path", path);
			sqlite3_step( write_stmt );

			if ( sqlite3_reset( write_stmt ) != SQLITE_OK )
			{
				fprintf(stderr, VT_ERROR "LOG update write error: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			}
			fprintf(stderr, VT_ERROR "LOG update write: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			break;

		case KIVFS_CHMOD:
			cache_check_stmt(prepare_log_chmod, &chmod_stmt);
			bind_text(chmod_stmt, ":path", path);
			sqlite3_step( chmod_stmt );

			if ( sqlite3_reset( chmod_stmt ) != SQLITE_OK )
			{
				fprintf(stderr, VT_ERROR "LOG update chmod error: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			}
			break;

		default:
			fprintf(stderr, VT_ERROR "LOG update error: Undefined command\n" VT_NORMAL);
			break;
	}
}

void cache_log(const char *path, const char *new_path, KIVFS_VFS_COMMAND action)
{
	pthread_mutex_lock( &log_mutex );

	bind_text(log_stmt,":path", path);
	bind_text(log_stmt, ":new_path", new_path);
	bind_int(log_stmt, ":action", action);

	sqlite3_step( log_stmt );

	switch ( sqlite3_reset( log_stmt ) )
	{
		case SQLITE_OK:
			break;

		case SQLITE_CONSTRAINT:
			fprintf(stderr, VT_ERROR "LOG update error: %s %s\n" VT_NORMAL, path, sqlite3_errmsg( db ));
			cache_solve_conflict(path, new_path, action);
			break;

		default:
			fprintf(stderr, VT_ERROR "Action log failed: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
			break;
	}

	pthread_mutex_unlock( &log_mutex );
}

int cache_readdir(const char *path, void *buf, fuse_fill_dir_t filler)
{
	pthread_mutex_lock( &readdir_mutex );

	bind_text(readdir_stmt, ":path", path);

	fprintf(stderr, VT_CYAN "Cache readdir before: %s %s err: %d %s\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ), path);

	while ( sqlite3_step( readdir_stmt ) == SQLITE_ROW )
	{
		fprintf(stderr, VT_ACTION "Cache readdir step: %s %s err: %d %s\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ), basename( (char *)sqlite3_column_text(readdir_stmt, 0) ));
		if ( filler(buf, basename( (char *)sqlite3_column_text(readdir_stmt, 0) ) , NULL, 0) )
		{
			break;
		}
	}

	fprintf(stderr, VT_ERROR "Cache readdir: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));

	if ( sqlite3_reset( readdir_stmt ) == SQLITE_OK )
	{
		pthread_mutex_unlock( &readdir_mutex );
		return KIVFS_OK;
	}
	else
	{
		fprintf(stderr, VT_ERROR "Cache readdir failure: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &readdir_mutex );
	return -ENOENT;
}

int cache_getattr(const char *path, struct stat *stbuf)
{

	int res;

	pthread_mutex_lock( &getattr_mutex );

	bind_text(getattr_stmt, ":path", path);


	//should be ROW
	if ( sqlite3_step( getattr_stmt ) == SQLITE_ROW )
	{

		stbuf->st_atim.tv_sec = sqlite3_column_int(getattr_stmt, 0);
		stbuf->st_mtim.tv_sec = sqlite3_column_int(getattr_stmt, 1);
		stbuf->st_size = sqlite3_column_int(getattr_stmt, 2);
		stbuf->st_mode = sqlite3_column_int(getattr_stmt, 3);
		stbuf->st_nlink = S_ISDIR(stbuf->st_mode) ? 2 : 1;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();

		res = KIVFS_OK;

		fprintf(stderr,"\033[33;1m found\n size: %lu\n \033[0m", stbuf->st_size);
	}
	else
	{
		res = -ENOENT;
	}

	if ( sqlite3_reset( getattr_stmt ) == SQLITE_OK )
	{
		fprintf(stderr,"\033[34;1mCache getattr reset OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_ERROR "Cache getattr failure: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	if(res)
		fprintf(stderr, VT_ERROR "Cache getattr EXTREME failure: %s %s err: %d | res: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ), res);

	pthread_mutex_unlock( &getattr_mutex );
	return res;

}

kivfs_version_t cache_get_version(const char *path)
{
	kivfs_version_t version = -1;

	pthread_mutex_lock( &get_version_mutex );

	bind_text(get_version_stmt, ":path", path);

	if ( sqlite3_step( get_version_stmt ) == SQLITE_ROW )
	{
		version = sqlite3_column_int(get_version_stmt, 0);
	}

	if ( sqlite3_reset( get_version_stmt ) == SQLITE_OK )
	{
		pthread_mutex_unlock( &get_version_mutex );
		return version;
	}
	else
	{
		fprintf(stderr, VT_ERROR "Cache readdir failure: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	pthread_mutex_unlock( &get_version_mutex );

	return version;
}


mode_t cache_file_mode(const char *path )
{
	mode_t mode;

	pthread_mutex_lock( &get_fmod_mutex );

	bind_text(get_fmod_stmt, ":path", path);

	if ( sqlite3_step( get_fmod_stmt ) == SQLITE_ROW )
	{
		mode = sqlite3_column_int(get_fmod_stmt, 0);
	}

	if ( sqlite3_reset( get_fmod_stmt ) == SQLITE_OK )
	{
		pthread_mutex_unlock( &get_fmod_mutex );
		return mode;
	}
	else
	{
		fprintf(stderr, VT_ERROR "Cache readdir failure: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &get_fmod_mutex );
	return -ENOENT;
}

void cache_log_delete(const char *path, KIVFS_VFS_COMMAND action)
{
	sqlite3_stmt *delete_log_stmt;
	sqlite3_prepare_v2(db, "DELETE FROM log WHERE action=:action AND path LIKE :path", ZERO_TERMINATED, &delete_log_stmt, NULL);

	bind_text(delete_log_stmt,":path", path);
	bind_int(delete_log_stmt,":action", action);

	sqlite3_step( delete_log_stmt );
	sqlite3_reset( delete_log_stmt );
	sqlite3_finalize( delete_log_stmt );
}

void cache_sync_log()
{

	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "SELECT * FROM log", ZERO_TERMINATED, &stmt, NULL);
	fprintf(stderr, VT_ERROR "CACHE SYNC LOG\n");

	while ( sqlite3_step( stmt ) == SQLITE_ROW )
	{
		int res;
		const char *path 			= (const char *)sqlite3_column_text(stmt, 0);
		const char *new_path		= (const char *)sqlite3_column_text(stmt ,1);
		KIVFS_VFS_COMMAND action 	= sqlite3_column_int(stmt, 2);

		res = kivfs_remote_sync(path, new_path, action);

		fprintf(stderr,"\t path: %s | action: %d | res: %d\033[0;m\n", path, action, res);

		/* Action was succesful on server side */
		if ( !res ){
			cache_log_delete(path, action);
		}
		else
		{
			int stop = 0;

			/* Action failed, but why? */
			switch ( res )
			{
				case KIVFS_ERC_FILE_LOCKED:
					/* File is locked so we can't delete entry from log */
					break;

				case KIVFS_ERROR:
					/* File already exists and entry in log is just wrong */
				case KIVFS_ERC_DIR_EXISTS:
				case KIVFS_ERC_FILE_EXISTS:

					/* File doesn't exist and entry in log is just wrong */
				case KIVFS_ERC_NO_SUCH_DIR:
				case KIVFS_ERC_NO_SUCH_FILE:
					cache_log_delete(path, action);
					break;


				default:
					/* Some serious error like no internet connection */
					stop = 1;
					break;
			}

			if ( stop )
			{
				break;
			}
		}
	}

	sqlite3_finalize(stmt);
}

void cache_sync_modified()
{
	int res;
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "SELECT path,version,mode FROM files WHERE modified = 1", ZERO_TERMINATED, &stmt, NULL);

	fprintf(stderr, VT_ERROR "CACHE SYNC MODIFIED\n");

	while ( sqlite3_step( stmt ) == SQLITE_ROW )
	{
		kivfs_file_t *file = NULL;
		char *path = (char *)sqlite3_column_text(stmt, 0);
		kivfs_version_t version = sqlite3_column_int(stmt, 1);
		mode_t	mode = sqlite3_column_int(stmt, 2);

		res = kivfs_remote_file_info(path, &file);

		fprintf(stderr,"\t path: %s | version: %d | res: %d\033[0;m\n", path, version, res);

		switch ( res )
		{
			case KIVFS_OK:
				break;

			case KIVFS_ERC_NO_SUCH_DIR:
				kivfs_remote_mkdir(path, mode);
				break;

			case KIVFS_ERC_NO_SUCH_FILE:
				//kivfs_remote_create(path, mode, NULL);
				break;

			default:
				fprintf(stderr, "UNKNOWN ERROR\n");
				return;
		}

		if ( !file || version <= file->version)
		{
			res = kivfs_put_from_cache(path);

			/* if upload fails, remove file from cache */
			if ( res == KIVFS_ERC_NO_SUCH_FILE )
			{
				char *full_path = get_full_path(path);

				unlink( full_path );
				free( full_path );

				cache_remove(path);
			}
			else if( !res )
			{
				fprintf(stderr, "File is up to date on the server from now!\n");
				cache_set_modified(path, 0);
				cache_inc_version( path );
			}

		}
		else
		{
			fprintf(stderr, "Remote file has different version, can not sync!\n");
		}

		kivfs_free_file( file );
	}

	sqlite3_finalize( stmt );
}

int cache_sync()
{
	fprintf(stderr, VT_ACTION "CACHE SYNC\n" VT_NORMAL);


	pthread_mutex_lock( &sync_mutex );
	{
		cache_sync_log();
		cache_sync_modified();
	}
	pthread_mutex_unlock( &sync_mutex );

	return 0; //XXX
}

int cache_chmod(const char *path, mode_t mode)
{

	pthread_mutex_lock( &chmod_mutex );

	bind_text(chmod_stmt, ":path", path);
	bind_int(chmod_stmt, ":mode", mode);

	sqlite3_step( chmod_stmt );

	if( sqlite3_reset( chmod_stmt ) == SQLITE_OK )
	{
			pthread_mutex_unlock( &chmod_mutex );
			fprintf(stderr, VT_OK "Cache chmod OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
			return KIVFS_OK;
	}
	else
	{
		fprintf(stderr, VT_ERROR "Cache chmod failure: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &chmod_mutex );

	return -EPERM;
}

int cache_update_time(const char *path, const struct timespec tv[2])
{

	pthread_mutex_lock( &update_time_mutex );

	bind_text(update_time_stmt, ":path", path);
	bind_int(update_time_stmt, ":atime", tv[0].tv_sec);
	bind_int(update_time_stmt, ":mtime", tv[1].tv_sec);

	sqlite3_step( update_time_stmt );

	if( sqlite3_reset( update_time_stmt ) == SQLITE_OK )
	{
			pthread_mutex_unlock( &update_time_mutex );
			fprintf(stderr, VT_OK "Cache utime OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
			return KIVFS_OK;
	}
	else
	{
		fprintf(stderr, VT_ERROR "Cache utime failure: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &update_time_mutex );

	return -EPERM;
}

int cache_updatedir(kivfs_list_t *files)
{

	for(kivfs_adt_item_t *item = files->first; item != NULL; item = item->next){

		kivfs_file_t *file = (kivfs_file_t *)(item->data);

		if( !cache_add(NULL, file) )
		{
			return KIVFS_ERROR;
		}
	}

	return KIVFS_OK;
}

int cache_update(const char *path, struct fuse_file_info *fi, kivfs_file_t *file_info)
{
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	pthread_mutex_lock( & update_mutex );

	fprintf(stderr,"\033[34;1mcache_update: %s fd: %lu\n",  path, file->fd);

	/* update from local file */
	if( file_info == NULL )
	{
		bind_text(update_stmt,	":path",	path);
		bind_int(update_stmt,	":size",	file->stbuf.st_size);
		bind_int(update_stmt,	":mtime",	file->stbuf.st_mtim.tv_sec);
		bind_int(update_stmt,	":atime",	file->stbuf.st_atim.tv_sec);
		bind_int(update_stmt,	":mode",	file->stbuf.st_mode);
		bind_int(update_stmt,	":version",	cache_get_version(path));

		fprintf(stderr, "UPDATE FROM CACHE size %lu\n", file->stbuf.st_size);
	}
	else
	{
		mode_t mode;

		fprintf(stderr, "REMOTE UPDATE %p\n", file_info);

		mode = (file_info->acl->owner << 6) | (file_info->acl->group << 3) | (file_info->acl->other);

		bind_text(update_stmt, ":path", 		path);
		bind_int(update_stmt,	":size",		file_info->size);
		bind_int(update_stmt,	":mtime",		file_info->mtime);
		bind_int(update_stmt,	":atime",		file_info->atime);
		bind_int(update_stmt,	":mode",		mode | (file_info->type == FILE_TYPE_DIRECTORY ? S_IFDIR : S_IFREG));
		bind_int(update_stmt,	":version",		file_info->version);
		bind_int(update_stmt,	":srv_read_hits",		file_info->read_hits);
		bind_int(update_stmt,	":srv_write_hits",		file_info->write_hits);

	}


	fprintf(stderr,"\n\033[37m%d %s %d %d\033[0m\n",sqlite3_step( update_stmt ), sqlite3_errmsg( db ), sqlite3_errcode( db ), sqlite3_changes( db ));

	if( sqlite3_reset( update_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_update: failure%s %s %d \n" VT_NORMAL,  path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "Cache update ok: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &update_mutex );

	return 0;
}

void cache_update_read_hits(const char *path, double read_hits)
{

	pthread_mutex_lock( &update_read_hits_mutex );

	bind_text(update_read_hits_stmt, ":path", path);
	bind_real(update_read_hits_stmt, ":read_hits", read_hits);

	sqlite3_step( update_read_hits_stmt );

	if( sqlite3_reset( inc_read_hits_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_set_read_hits: failure%s %s %d \n" VT_NORMAL,  path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "cache_update_read_hits OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &update_read_hits_mutex );
}

void cache_inc_read_hits(const char *path)
{

	pthread_mutex_lock( &inc_read_hits_mutex );

	bind_text(inc_read_hits_stmt, ":path", path);

	sqlite3_step( inc_read_hits_stmt );

	if( sqlite3_reset( update_read_hits_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_inc_read_hits: failure %s %s %d\n" VT_NORMAL,  path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "cache_inc_read_hits OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &inc_read_hits_mutex );
}

void cache_inc_write_hits(const char *path)
{
	pthread_mutex_lock( &inc_write_hits_mutex );

	bind_text(inc_write_hits_stmt, ":path", path);

	sqlite3_step( inc_write_hits_stmt );

	if( sqlite3_reset( inc_write_hits_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_inc_write_hits: failure %s %s %d\n" VT_NORMAL,  path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "cache_update_write_hits OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &inc_write_hits_mutex );
}

void cache_set_cached(const char *path, int cached)
{
	pthread_mutex_lock( &set_cached_mutex );

	bind_text(set_cached_stmt, ":path", path);
	bind_int(set_cached_stmt, ":cached", cached);

	sqlite3_step( set_cached_stmt );

	if( sqlite3_reset( set_cached_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_set_cached: failure %s %s %d\n" VT_NORMAL,  path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "cache_set_cached OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &set_cached_mutex );
}

size_t cache_get_used_size( void )
{
	size_t size = 0;

	//if( sqlite3_step( get_used_size_stmt ) == SQLITE_ROW )
	fprintf(stderr,"\033[33;1mresult %d \n\033[0m",sqlite3_step( get_used_size_stmt ));
	{
		size = sqlite3_column_int(get_used_size_stmt, 0);
		fprintf(stderr,"\033[33;1msize: %lu \n\033[0m", size);
	}

	if( sqlite3_reset( get_used_size_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_get_used_size: failure %s  %d\n" VT_NORMAL, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "cache_get_used_size OK: size: %ld %s err: %d\n" VT_NORMAL, size, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	return size;
}

int cache_contains(const char *path)
{
	int cached = 0;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(db, "SELECT * FROM files WHERE cached = 1 AND path = :path", ZERO_TERMINATED, &stmt, NULL);

	bind_text(stmt, ":path", path);

	if( sqlite3_step( stmt ) == SQLITE_ROW )
	{
		cached = 1;
	}

	sqlite3_finalize( stmt );

	return cached;
}

void cache_set_modified(const char *path, int status)
{
	pthread_mutex_lock( &set_modified_mutex );

	bind_text(set_modified_stmt, ":path", path);
	bind_int(set_modified_stmt, ":modified", status);

	sqlite3_step( set_modified_stmt );

	if( sqlite3_reset( set_modified_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "set_modified: failure %s %s %d\n" VT_NORMAL,  path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "set_modified OK: %s %s err: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &set_modified_mutex );
}

void cache_inc_version(const char *path)
{
	pthread_mutex_lock( &inc_version_mutex );

	bind_text(update_version_stmt, ":path", path);

	sqlite3_step( update_version_stmt );

	if( sqlite3_reset( update_version_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "cache_inc_version: failure%s\033[0;0m %s %d | changes: %d\n" VT_NORMAL,  path, sqlite3_errmsg( db ), sqlite3_errcode( db ), sqlite3_changes(db));
	}
	else
	{
		fprintf(stderr, VT_OK "cache_inc_version OK: %s %s err: %d | changes: %d\n" VT_NORMAL, path, sqlite3_errmsg( db ), sqlite3_errcode( db ), sqlite3_changes(db));
	}

	pthread_mutex_unlock( &inc_version_mutex );
}

int cache_global_hits(kivfs_cfile_t *global_hits)
{
	sqlite3_step( global_hits_stmt );

	global_hits->c_read_hits = sqlite3_column_double(global_hits_stmt, 0);
	global_hits->write_hits = sqlite3_column_int(global_hits_stmt, 1);

	if( sqlite3_reset( global_hits_stmt ) == SQLITE_OK )
	{
		fprintf(stderr, VT_OK "cache_global_hits OK: %s err: %d | changes: %d\n" VT_NORMAL, sqlite3_errmsg( db ), sqlite3_errcode( db ), sqlite3_changes(db));
		fprintf(stderr, VT_WARNING "g_r %f | g_w %lu\n" VT_NORMAL, global_hits->c_read_hits, global_hits->write_hits);
		return KIVFS_OK;
	}
	else
	{
		fprintf(stderr, VT_ERROR "cache_global_hits: failure %s err: %d | changes: %d\n" VT_NORMAL,  sqlite3_errmsg( db ), sqlite3_errcode( db ), sqlite3_changes(db));
	}

	return KIVFS_ERROR;
}

