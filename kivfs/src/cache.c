/*----------------------------------------------------------------------------
 * cache.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <fuse.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <sqlite3.h>
#include <kivfs.h>
#include <pthread.h>

#include "config.h"
#include "cache.h"
#include "prepared_stmts.h"
#include "connection.h"

//XXX: odstranit nepotřebné includy
#include <stdio.h>


sqlite3 *db;

sqlite3_stmt *add_stmt;
sqlite3_stmt *rename_stmt;
sqlite3_stmt *remove_stmt;
sqlite3_stmt *log_stmt;
sqlite3_stmt *readdir_stmt;
sqlite3_stmt *getattr_stmt;
sqlite3_stmt *get_fmod_stmt;
sqlite3_stmt *chmod_stmt;

/* Each function can be processed without race condition */
pthread_mutex_t add_mutex;
pthread_mutex_t rename_mutex;
pthread_mutex_t remove_mutex;
pthread_mutex_t log_mutex;
pthread_mutex_t readdir_mutex;
pthread_mutex_t getattr_mutex;
pthread_mutex_t get_fmod_mutex;
pthread_mutex_t chmod_mutex;
pthread_mutex_t sync_mutex;


static inline void cache_check_stmt(void (*initializer)(sqlite3_stmt **, sqlite3 *), sqlite3_stmt **stmt){
	if( !*stmt ){
		initializer(stmt, db);
	}
}

void prepare_statements(){
	prepare_cache_add			(&add_stmt,		db);
	prepare_cache_rename		(&rename_stmt,	db);
	prepare_cache_remove		(&remove_stmt,	db);
	prepare_cache_log			(&log_stmt,		db);
	prepare_cache_readdir		(&readdir_stmt, db);
	prepare_cache_getattr		(&getattr_stmt,	db);
	prepare_cache_file_mode	(&get_fmod_stmt,db);
	prepare_cache_chmod		(&chmod_stmt, 	db);
}

void init_mutexes(){
	pthread_mutex_init(&add_mutex,		NULL);
	pthread_mutex_init(&rename_mutex, 	NULL);
	pthread_mutex_init(&remove_mutex, 	NULL);
	pthread_mutex_init(&log_mutex, 		NULL);
	pthread_mutex_init(&readdir_mutex, 	NULL);
	pthread_mutex_init(&getattr_mutex, 	NULL);
	pthread_mutex_init(&get_fmod_mutex, 	NULL);
	pthread_mutex_init(&chmod_mutex,		NULL);
	pthread_mutex_init(&sync_mutex,		NULL);
}

int cache_init(){
	int res;
	char db_path[ PATH_MAX ];
	char *db_filename = "/kivfs.db";

	//XXX: Buffer overflow may occur.
	strcat(db_path, "/tmp");
	strcat(db_path, db_filename);

	printf("db path: %s mutex: %d \n", db_path, sqlite3_threadsafe());

	res = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
	if( res != SQLITE_OK ){

		res = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
		if( res != SQLITE_OK ){
			perror("Unable to create a database file.");
			return EXIT_FAILURE;
		}

		fprintf(stderr, "\033[31;1mcreating table\033[0;0m\n\n");
		res = sqlite3_exec(db,
						"PRAGMA journal_mode=WAL",
					    NULL,
					    NULL,
					    NULL
						);

		if( res != SQLITE_OK ){
			fprintf(stderr,"\033[31;1mcache_init\033[0;0m %s\n", sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}

		res = sqlite3_exec(db,
				"CREATE TABLE 'main'.'files' ("
			    "path 			TEXT PRIMARY KEY NOT NULL,"
				"size 			INT NOT NULL,"
				"mtime 			INT NOT NULL,"
				"atime 			INT NOT NULL,"
				"mode 			INT NOT NULL,"
				"own 			TEXT NOT NULL,"
				"grp 			TEXT NOT NULL,"
			    "read_hits 		INT NOT NULL DEFAULT ('0'),"
				"write_hits 	INT NOT NULL DEFAULT ('0'),"
				//"type 			INT NOT NULL,"
				"version 		TEXT NOT NULL)",
			    NULL,
			    NULL,
			    NULL
				);

		if( res != SQLITE_OK ){
			fprintf(stderr,"\033[31;1mcache_init\033[0;0m %s\n", sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}

		res = sqlite3_exec(db,
				"CREATE TABLE 'main'.'log' (					"
				"path 			TEXT PRIMARY KEY NOT NULL,		"
				"new_path 		TEXT UNIQUE,					"
				"action 		INT NOT NULL)					",
				NULL,
				NULL,
				NULL
				);

		if( res != SQLITE_OK ){
			fprintf(stderr,"\033[31;1mcache_init\033[0;0m %s\n", sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}
	}

	prepare_statements();
	init_mutexes();

	return res;
}


/*----------------------------------------------------------------------------
   Function : cache_close()
   In       : void
   Out      : void
   Job      : Clean all resources from memory.
   Notice   :
 ---------------------------------------------------------------------------*/
void cache_close(){

	pthread_mutex_destroy( &add_mutex );
	pthread_mutex_destroy( &rename_mutex );
	pthread_mutex_destroy( &remove_mutex );
	pthread_mutex_destroy( &log_mutex );
	pthread_mutex_destroy( &readdir_mutex );
	pthread_mutex_destroy( &getattr_mutex );
	pthread_mutex_destroy( &get_fmod_mutex );
	pthread_mutex_destroy( &chmod_mutex );
	pthread_mutex_destroy( &sync_mutex );

	sqlite3_finalize( add_stmt );
	sqlite3_finalize( rename_stmt );
	sqlite3_finalize( remove_stmt );
	sqlite3_finalize( log_stmt );
	sqlite3_finalize( readdir_stmt );
	sqlite3_finalize( getattr_stmt );
	sqlite3_finalize( get_fmod_stmt );
	sqlite3_finalize( chmod_stmt );

	sqlite3_close( db );
}

int cache_add(const char *path, kivfs_file_t *file){

	int res;



	if( file ){
		char tmp[4096]; //XXX: unsafe
		mode_t mode;

		memset(tmp, 0, 4096);
		strcat(tmp, path);
		if(strcmp(path, "/") != 0)
			strcat(tmp, "/");
		strcat(tmp, file->name);

		mode = (file->acl->owner << 6) | (file->acl->group << 3) | (file->acl->other);

		pthread_mutex_lock( &add_mutex );

		bind_text(add_stmt,	":path",		tmp);
		bind_int(add_stmt,	":size",		file->size);
		bind_int(add_stmt,	":mtime",		file->mtime);
		bind_int(add_stmt,	":atime",		file->atime);
		bind_int(add_stmt,	":mode",	 	mode | (file->type == FILE_TYPE_DIRECTORY ? 0040000 : 0100000) );
		bind_int(add_stmt,	":owner",		getuid());
		bind_int(add_stmt,	":group",		getgid());
		bind_int(add_stmt,	":read_hits",	file->read_hits);
		bind_int(add_stmt,	":write_hits",	file->write_hits);
		//bind_int(add_stmt,	":type",		type);
		bind_int(add_stmt,	":version",		file->version);

		fprintf(stderr," %d %d %d %o\n"
				"file type: %d\n"
				"path: %s",
				file->acl->owner,
				file->acl->group,
				file->acl->other,
				(file->acl->owner << 9) | (file->acl->group << 6) | (file->acl->other),
				file->type,
				tmp
			);

	}
	else{
		struct stat stbuf;
		char *full_path = get_full_path( path );

		stat(full_path, &stbuf);
		free( full_path );

		fprintf(stderr,"\033[31;1mcache_add: %s mode: %o \033[0;0m\n", path, stbuf.st_mode);

		pthread_mutex_lock( &add_mutex );

		bind_text(add_stmt, ":path", path);
		bind_int(add_stmt, ":size", stbuf.st_size);
		bind_int(add_stmt, ":mtime", stbuf.st_mtim.tv_sec);
		bind_int(add_stmt, ":atime", stbuf.st_atim.tv_sec);
		bind_int(add_stmt, ":mode", stbuf.st_mode);
		bind_int(add_stmt, ":owner", stbuf.st_uid);
		bind_int(add_stmt, ":group", stbuf.st_gid);
		bind_int(add_stmt, ":read_hits", 0);
		bind_int(add_stmt, ":write_hits", 0);
		//bind_int(add_stmt, ":type", type);
		bind_int(add_stmt, ":version", 0);

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

	if( (res = sqlite3_reset( add_stmt )) != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_add: %s\033[0;0m %s %d\n", path, sqlite3_errmsg( db ), res);
		res = -EEXIST;
	}

	pthread_mutex_unlock( &add_mutex );
	return res;
}

int cache_rename(const char *old_path, const char *new_path){

	pthread_mutex_lock( &rename_mutex );

	bind_text(rename_stmt, ":old_path", old_path);
	bind_text(rename_stmt, ":new_path", new_path);

	sqlite3_step( rename_stmt );

	if( sqlite3_reset( rename_stmt ) != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_rename: %s\033[0;0m %s %d\n", old_path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
		pthread_mutex_unlock( &rename_mutex );
		//TODO everwrite existing files, if new path is directory then move file/dir into it, else EISDIR
		return 0;
		return -EEXIST;
	}

	pthread_mutex_unlock( &rename_mutex );
	return KIVFS_OK;
}

int cache_remove(const char *path){

	int res = 0;
	pthread_mutex_lock( &remove_mutex );

	bind_text(remove_stmt, ":path", path);

	sqlite3_step( remove_stmt );

	if( sqlite3_reset( remove_stmt ) != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_remove: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
		res = -ENOENT;
	}

	pthread_mutex_unlock( &remove_mutex );
	return res;
}

/*----------------------------------------------------------------------------
   Function : cache_solve_conflic(const char *path, const char *new_path, KIVFS_VFS_COMMAND action)
   In       : path: Path to the source file is mandatory.
   	   	   	  new path: Path to the new file, if action is KIVFS_MOVE.
			  action: Type of action. All types are declared in kivfs-cmdn-vfs.h
   Out      : void
   Job      : Solves conflict in log table.
   Notice   :

   	   	   	  KIVFS_UNLINK:
   	   	   	  KIVFS_RMDIR:
    		  The idea is that if a file is created in offline mode and then removed before sync operation,
   	   	   	  conflict in log occurs between (KIVFS_TOUCH or KIVFS_MKDIR) and (KIVFS_RMDIR or KIVFS_UNLINK).
   	   	   	  It is useless sending create request to a server and immediately unlink newly created file.
   	   	   	  So if UNLINK/RMDIR occurs after TOUCH/MKDIR the record is deleted as anti-matter and matter annihilates.

   	   	   	  WRITE to unlinked file is not opossible.


			  KIVFS_TOUCH:
			  KIVFS_WRITE:
   	   	   	  Sometimes can occur opposite situation. Remote file is deleted locally and in near future before sync
   	   	   	  recreated. Informing server about unlink is not efficient due to overhead like in previous situation.
   	   	   	  KIVFS_UNLINK is changed to KIVFS_WRITE. Why WRITE? Because it means that the file is changed and
   	   	   	  have to be synced as soon as possible.

   	   	   	  WRITE can also conflict with another WRITE. In this case WRITE flag stays the same.




 ---------------------------------------------------------------------------*/
void cache_solve_conflict(const char *path, const char *new_path, KIVFS_VFS_COMMAND action){

	static sqlite3_stmt *move_stmt;
	static sqlite3_stmt *unlink_stmt;
	static sqlite3_stmt *unlink_remote_stmt;
	static sqlite3_stmt *write_stmt;
	static sqlite3_stmt *chmod_stmt;

	switch( action ){
		case KIVFS_RMDIR:
			/* fall through */
				fprintf(stderr,"\033[31;1mLOG RMDIR: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));

		case KIVFS_UNLINK:
				fprintf(stderr,"\033[31;1mLOG UNLINK: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			cache_check_stmt(prepare_log_remove, &unlink_stmt);
			bind_text(unlink_stmt, ":path", path);

			/* Deletes single row containging action == KIVFS_MKDIR or KIVFS_TOUCH */
			sqlite3_step( unlink_stmt );
			if( sqlite3_reset( unlink_stmt ) != SQLITE_OK ){
				fprintf(stderr,"\033[31;1mLOG update error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			}

			/* Nothing has been removed, but we have to solve conflict between KIVFS_WRITE or KIVFS_TRUNCATE */
			else if( sqlite3_changes( db ) == 0 ){
				cache_check_stmt(prepare_log_remote_remove, &unlink_remote_stmt);

				bind_text(unlink_remote_stmt, ":path", path);
				bind_int(unlink_remote_stmt, ":action", action);

				sqlite3_step( unlink_remote_stmt );
				if( sqlite3_reset( unlink_remote_stmt ) != SQLITE_OK ){
					fprintf(stderr,"\033[31;1mLOG update unlink error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
				}
			}

			break;

		case KIVFS_MOVE:
				fprintf(stderr,"\033[31;1mLOG MOVE: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			cache_check_stmt(prepare_log_move, &move_stmt);
			bind_text	(move_stmt,	":old_path", path);
			bind_text	(move_stmt,	":new_path", new_path);
			sqlite3_step( move_stmt );

			//TODO what if a file is moved to previous name? DELETE movement in log!
			if( sqlite3_reset( move_stmt ) != SQLITE_OK ){
				fprintf(stderr,"\033[31;1mLOG update move error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));

			}
			break;

		case KIVFS_TOUCH:
			/* fall through */

		case KIVFS_WRITE:
			cache_check_stmt(prepare_log_write, &write_stmt);
			bind_text(write_stmt, ":path", path);
			sqlite3_step( write_stmt );

			if( sqlite3_reset( write_stmt ) != SQLITE_OK ){
				fprintf(stderr,"\033[31;1mLOG update write error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			}
			fprintf(stderr,"\033[31;1mLOG update write: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			break;

		case KIVFS_CHMOD:
			cache_check_stmt(prepare_log_chmod, &chmod_stmt);
			bind_text(chmod_stmt, ":path", path);
			sqlite3_step( chmod_stmt );

			if( sqlite3_reset( chmod_stmt ) != SQLITE_OK ){
				fprintf(stderr,"\033[31;1mLOG update chmod error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			}
			break;

		default:
			fprintf(stderr,"\033[31;1mLOG update error: Undefined command\033[0;0m\n");
			break;
	}
}

void cache_log(const char *path, const char *new_path, KIVFS_VFS_COMMAND action){

	pthread_mutex_lock( &log_mutex );

	bind_text(log_stmt,":path", path);
	bind_text(log_stmt, ":new_path", new_path);
	bind_int(log_stmt, ":action", action);

	sqlite3_step( log_stmt );

	switch( sqlite3_reset( log_stmt ) ){
		case SQLITE_OK:
			break;
		case SQLITE_CONSTRAINT:
			fprintf(stderr,"\033[31;1mLOG update error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			cache_solve_conflict(path, new_path, action);
			break;
		default:
			fprintf(stderr,"\033[31;1mAction log failed: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &log_mutex );
}

int cache_readdir(const char *path, void *buf, fuse_fill_dir_t filler){

	pthread_mutex_lock( &readdir_mutex );

	bind_text(readdir_stmt, ":path", path);

	fprintf(stderr,"\033[34;1mCache readdir before: %s\033[0;0m %s err: %d %s\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ), path);

	while( sqlite3_step( readdir_stmt ) == SQLITE_ROW ){//basename( (char *)sqlite3_column_text(stmt, 0) )
		fprintf(stderr,"\033[34;1mCache readdir step: %s\033[0;0m %s err: %d %s\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ), basename( (char *)sqlite3_column_text(readdir_stmt, 0) ));
		if( filler(buf, basename( (char *)sqlite3_column_text(readdir_stmt, 0) ) , NULL, 0) ){
			break;
		}
	}
	fprintf(stderr,"\033[31;1mCache readdir: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));

	if( sqlite3_reset( readdir_stmt ) == SQLITE_OK ){
		pthread_mutex_unlock( &readdir_mutex );
		return KIVFS_OK;
	}
	else{
		fprintf(stderr,"\033[31;1mCache readdir failure: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &readdir_mutex );
	return -ENOENT;
}

int cache_getattr(const char *path, struct stat *stbuf){

	int res;

	pthread_mutex_lock( &getattr_mutex );

	bind_text(getattr_stmt, ":path", path);


	//should be ROW
	if( sqlite3_step( getattr_stmt ) == SQLITE_ROW ){

		stbuf->st_atim.tv_sec = sqlite3_column_int(getattr_stmt, 0);
		stbuf->st_mtim.tv_sec = sqlite3_column_int(getattr_stmt, 1);
		stbuf->st_size = sqlite3_column_int(getattr_stmt, 2);
		stbuf->st_mode = sqlite3_column_int(getattr_stmt, 3);
		stbuf->st_nlink = S_ISDIR(stbuf->st_mode) ? 2 : 1;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();

		res = KIVFS_OK;

		fprintf(stderr,"\033[33;1m found\n\033[0m");
	}
	else{
		res = -ENOENT;
	}

	if( sqlite3_reset( getattr_stmt ) == SQLITE_OK ){
		fprintf(stderr,"\033[31;1mCache getattr reset OK: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}
	else{
		fprintf(stderr,"\033[31;1mCache getattr failure: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	if(!res)
	fprintf(stderr,"\033[31;1mCache getattr EXTREME failure: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));

	pthread_mutex_unlock( &getattr_mutex );
	return res;

}

//rename to get_file_mode
mode_t cache_file_mode(const char *path ){

	mode_t mode;

	pthread_mutex_lock( &get_fmod_mutex );

	bind_text(get_fmod_stmt, ":path", path);

	if( sqlite3_step( get_fmod_stmt ) == SQLITE_ROW ){
		mode = sqlite3_column_int(get_fmod_stmt, 0);
	}

	if( sqlite3_reset( get_fmod_stmt ) == SQLITE_OK ){
		pthread_mutex_unlock( &get_fmod_mutex );
		return mode;
	}
	else{
		fprintf(stderr,"\033[31;1mCache readdir failure: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &get_fmod_mutex );
	return -ENOENT;
}

void cache_log_delete(const char *path, KIVFS_VFS_COMMAND action){
	sqlite3_stmt *delete_log_stmt;
	sqlite3_prepare_v2(db, "DELETE FROM log WHERE action=:action AND path LIKE :path", ZERO_TERMINATED, &delete_log_stmt, NULL);

	bind_text(delete_log_stmt,":path", path);
	bind_int(delete_log_stmt,":action", action);

	sqlite3_step( delete_log_stmt );
	sqlite3_reset( delete_log_stmt );
	sqlite3_finalize( delete_log_stmt );
}

int cache_sync(){

	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "SELECT * FROM log", ZERO_TERMINATED, &stmt, NULL);

	pthread_mutex_lock( &sync_mutex );

	while( sqlite3_step( stmt ) == SQLITE_ROW ){
		int res;
		const char *path 			= (const char *)sqlite3_column_text(stmt, 0);
		const char *new_path		= (const char *)sqlite3_column_text(stmt ,1);
		KIVFS_VFS_COMMAND action 	= sqlite3_column_int(stmt, 2);

		res = kivfs_remote_sync(path, new_path, action);

		/* Action was succesful on server side */
		if( !res ){
			cache_log_delete(path, action);
		}
		else{
			int stop = 0;

			/* Action failed, but why? */
			switch( res ){
				case KIVFS_ERC_FILE_LOCKED:
					/* File is locked so we can't delete entry from log */
					break;

				case KIVFS_ERC_NO_SUCH_FILE:
					/* File doesn't exist and entry in log is just wrong */
					cache_log_delete(path, action);
					break;


				default:
					/* Some serious error like no internet connection */
					stop = 1;
					break;
			}

			if( stop ){
				break;
			}
		}
	}

	pthread_mutex_unlock( &sync_mutex );

	return 0; //XXX
}

int cache_chmod(const char *path, mode_t mode){

	pthread_mutex_lock( &chmod_mutex );

	bind_text(chmod_stmt, ":path", path);
	bind_int(chmod_stmt, ":mode", mode);

	sqlite3_step( chmod_stmt );

	if( sqlite3_reset( readdir_stmt ) == SQLITE_OK ){
			pthread_mutex_unlock( &chmod_mutex );
			return KIVFS_OK;
	}
	else{
		fprintf(stderr,"\033[31;1mCache readdir failure: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	pthread_mutex_unlock( &chmod_mutex );

	return -EPERM;
}

int cache_updatedir(kivfs_list_t *files){

	for(kivfs_adt_item_t *item = files->first; item != NULL; item = item->next){

		kivfs_file_t *file = (kivfs_file_t *)(item->data);

		if( !cache_add(NULL, file) ){
			return KIVFS_ERROR;
		}
	}

	return KIVFS_OK;
}
