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
#include "config.h"
#include "cache.h"

//XXX: odstranit nepotřebné includy
#include <stdio.h>


#define ZERO_TERMINATED -1

static sqlite3 *db;


void prepare_cache_add(sqlite3_stmt **stmt){

	char *sql = 	"INSERT INTO files(path, size, mtime, atime, mode, type, read_hits, write_hits, version)"
					"VALUES(:path, :size, :mtime, :atime, :mode, :type, :read_hits, :write_hits, :version)";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_add:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mpreparee_cache_add:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

void prepare_cache_rename(sqlite3_stmt **stmt){

	char *sql = 	"UPDATE files "
					"SET "
					"	path="
					"		replace("
					"			substr(path, 1, length(:old_path)),"
					"			:old_path,"
					"			:new_path"
					"		) "
					"		|| "
					"		substr(path, length(:old_path) + 1) "
					"WHERE "
					"	path LIKE (:old_path || '/%') " //subfolders
					"	or "
					"	path LIKE :old_path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_rename:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_rename:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

void prepare_cache_remove(sqlite3_stmt **stmt){

	char *sql = 	"DELETE FROM files WHERE path LIKE :path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_remove:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_remove:\033[0;0m %s\n", sqlite3_errmsg( db ));
}


void prepare_cache_readdir(sqlite3_stmt **stmt){

	char *sql = "SELECT path FROM files WHERE (path LIKE (:path || '/%') AND path NOT LIKE (:path || '/%/%')) or ('/' LIKE :path AND path NOT LIKE (:path || '%/%'))";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_readdir:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_readdir:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

void prepare_cache_getattr(sqlite3_stmt **stmt){

	char *sql = 	"SELECT atime, mtime, size, mode FROM files WHERE path LIKE :path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_getattr:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_getattr:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

void prepare_cache_log(sqlite3_stmt **stmt){

	char *sql = 	"INSERT INTO log(path, new_path, action)"
					"VALUES(:path, :new_path, :action)";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_log:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_log:\033[0;0m %s\n", sqlite3_errmsg( db ));
}


void prepare_log_move(sqlite3_stmt **stmt){

	char *sql = 	"UPDATE log "
					"SET "
					"	path="
					"		replace("
					"			substr(path, 1, length(:old_path)),"
					"			:old_path,"
					"			:new_path"
					"		) "
					"		|| "
					"		substr(path, length(:old_path) + 1) "
					"WHERE "
					"	path LIKE (:old_path || '/%') " //subfolders
					"	or "
					"	path LIKE :old_path"; //file or folder

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
}


void prepare_log_remove(sqlite3_stmt **stmt){

	char *sql = 	"DELETE FROM log WHERE path LIKE :path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

static inline void cache_check_stmt(void (*initializer)(sqlite3_stmt **), sqlite3_stmt **stmt){
	if( !*stmt ){
		initializer( stmt );
	}
}



int cache_init(){
	int res;
	char db_path[ PATH_MAX ];
	char *db_filename = "/kivfs.db";

	//XXX: Buffer overflow may occur.
	strcat(db_path, "/tmp");
	strcat(db_path, db_filename);

	printf("db path: %s mutex: %d\n", db_path, sqlite3_threadsafe() );

	res = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
	if( res != SQLITE_OK ){

		res = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
		if( res != SQLITE_OK ){
			perror("Unable to create a database file.");
			return EXIT_FAILURE;
		}

		fprintf(stderr, "\033[31;1mcreating table\033[0;0m\n\n");
		res = sqlite3_exec(db,
				"CREATE TABLE \"main\".\"files\" ("
			    "path TEXT PRIMARY KEY NOT NULL,"
				"size INT NOT NULL,"
				"mtime INT NOT NULL,"
				"atime INT NOT NULL,"
				"mode INT NOT NULL,"
			    "read_hits INT NOT NULL DEFAULT ('0'),"
				"write_hits INT NOT NULL DEFAULT ('0'),"
				"type INT NOT NULL,"
				"version TEXT NOT NULL)",
			    NULL,
			    NULL,
			    NULL
				);

		if( res != SQLITE_OK ){
			fprintf(stderr,"\033[31;1mcache_init\033[0;0m %s\n", sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}

		res = sqlite3_exec(db,
						"CREATE TABLE \"main\".\"log\" ("
					    "path TEXT PRIMARY KEY NOT NULL,"
						"new_path TEXT,"
						"action INT NOT NULL)",
					    NULL,
					    NULL,
					    NULL
						);

		if( res != SQLITE_OK ){
			fprintf(stderr,"\033[31;1mcache_init\033[0;0m %s\n", sqlite3_errmsg( db ));
			return EXIT_FAILURE;
		}
	}

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
	sqlite3_close( db );
}

int cache_add(const char *path, int read_hits, int write_hits, kivfs_file_type_t type){

	int res;
	struct stat stbuf;
	static sqlite3_stmt *stmt = NULL;
	char *full_path = get_full_path( path );

	stat(full_path, &stbuf);

	cache_check_stmt(prepare_cache_add, &stmt);


	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":path"), path, ZERO_TERMINATED, NULL);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":size"), stbuf.st_size);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":mtime"), stbuf.st_mtim.tv_sec);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":atime"), stbuf.st_atim.tv_sec);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":mode"), stbuf.st_mode);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":read_hits"), read_hits);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":write_hits"), write_hits);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":type"), type);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":version"), 0);

	res = sqlite3_step( stmt );

	if( (res = sqlite3_reset( stmt )) != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_add: %s\033[0;0m %s %d\n", full_path, sqlite3_errmsg( db ), res);
		res = -EEXIST;
	}

	free( full_path );
	return res;
}

int cache_rename(const char *old_path, const char *new_path){

	static sqlite3_stmt *stmt = NULL;

	cache_check_stmt(prepare_cache_rename, &stmt);

	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":old_path"), old_path, ZERO_TERMINATED, NULL);
	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":new_path"), new_path, ZERO_TERMINATED, NULL);

	sqlite3_step( stmt );

	if( sqlite3_reset( stmt ) != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_rename: %s\033[0;0m %s %d\n", old_path, sqlite3_errmsg( db ), sqlite3_errcode( db ));

		//TODO everwrite existing files, if new path is directory then move file/dir into it, else EISDIR
		return 0;
		return -EEXIST;
	}

	return SQLITE_OK;
}

int cache_remove(const char *path){

	static sqlite3_stmt *stmt = NULL;

	cache_check_stmt(prepare_cache_remove, &stmt);

	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":path"), path, ZERO_TERMINATED, NULL);

	sqlite3_step( stmt );

	if( sqlite3_reset( stmt ) != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_remove: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
		return -ENOENT;
	}

	return SQLITE_OK;
}

void cache_solve_conflict(const char *path, const char *new_path, cache_conflict_t type, KIVFS_VFS_COMMAND action){

	static sqlite3_stmt *move_stmt;
	static sqlite3_stmt *unlink_stmt;

	switch( action ){
		case KIVFS_RMDIR:
			/* fall through */
				fprintf(stderr,"\033[31;1mLOG RMDIR: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));

		case KIVFS_UNLINK:
				fprintf(stderr,"\033[31;1mLOG UNLINK: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			cache_check_stmt(prepare_log_remove, &unlink_stmt);
			sqlite3_bind_text(unlink_stmt, sqlite3_bind_parameter_index(unlink_stmt, ":path"), path, ZERO_TERMINATED, NULL);
			sqlite3_step( unlink_stmt );
			if( sqlite3_reset( unlink_stmt ) != SQLITE_OK ){
				fprintf(stderr,"\033[31;1mLOG update error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			}
			break;

		case KIVFS_MOVE:
				fprintf(stderr,"\033[31;1mLOG MOVE: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
			cache_check_stmt(prepare_log_move, &move_stmt);
			sqlite3_bind_text(move_stmt, sqlite3_bind_parameter_index(move_stmt, ":old_path"), path, ZERO_TERMINATED, NULL);
			sqlite3_bind_text(move_stmt, sqlite3_bind_parameter_index(move_stmt, ":new_path"), new_path, ZERO_TERMINATED, NULL);
			sqlite3_step( move_stmt );
			if( sqlite3_reset( move_stmt ) != SQLITE_OK ){
				fprintf(stderr,"\033[31;1mLOG update error: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));

			}
			break;

		case KIVFS_TOUCH:
			//TODO if unlink, then write
			break;
		default:
			fprintf(stderr,"\033[31;1mLOG update error: Undefined command\033[0;0m\n");
			break;
	}

}

void cache_log(const char *path, const char *new_path, KIVFS_VFS_COMMAND action){

	static sqlite3_stmt *stmt = NULL;

	cache_check_stmt(prepare_cache_log, &stmt);

	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":path"), path, ZERO_TERMINATED, NULL);
	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":new_path"), new_path, ZERO_TERMINATED, NULL);
	sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":action"), action);

	sqlite3_step( stmt );

	switch( sqlite3_reset( stmt ) ){
		case SQLITE_OK:
			//fall through
		case SQLITE_DONE:
			return;
		case SQLITE_CONSTRAINT:
			cache_solve_conflict(path, new_path, LOG_CONFLICT, action);
			break;
		default:
			fprintf(stderr,"\033[31;1mAction log failed: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}


}

int cache_readdir(const char *path, void *buf, fuse_fill_dir_t filler){

	static sqlite3_stmt *stmt = NULL;

	cache_check_stmt(prepare_cache_readdir, &stmt);

	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":path"), path, ZERO_TERMINATED, NULL);


	fprintf(stderr,"\033[34;1mCache readdir before: %s\033[0;0m %s err: %d %s\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ), path);

	while( sqlite3_step( stmt ) == SQLITE_ROW ){//basename( (char *)sqlite3_column_text(stmt, 0) )
		fprintf(stderr,"\033[34;1mCache readdir step: %s\033[0;0m %s err: %d %s\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ), basename( (char *)sqlite3_column_text(stmt, 0) ));
		if( filler(buf, basename( (char *)sqlite3_column_text(stmt, 0) ) , NULL, 0) ){
			break;
		}
	}
	fprintf(stderr,"\033[31;1mCache readdir: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));

	if( sqlite3_reset( stmt ) == SQLITE_OK ){
		return SQLITE_OK;
	}
	else{
		fprintf(stderr,"\033[31;1mCache readdir failure: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
	}

	return -ENOENT;
}

int cache_getattr(const char *path, struct stat *stbuf){

	static sqlite3_stmt *stmt = NULL;

	cache_check_stmt(prepare_cache_getattr, &stmt);

	sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":path"), path, ZERO_TERMINATED, NULL);


	if( sqlite3_step( stmt ) != SQLITE_DONE){
		if( sqlite3_reset( stmt ) == SQLITE_OK ){

			stbuf->st_atim.tv_sec = sqlite3_column_int(stmt, 0);
			stbuf->st_mtim.tv_sec = sqlite3_column_int(stmt, 1);
			stbuf->st_size = sqlite3_column_int(stmt, 2);
			stbuf->st_mode = sqlite3_column_int(stmt, 3);
			stbuf->st_nlink =  S_ISDIR(stbuf->st_mode) ? 2 : 1;

			printf("\n\033[33;1m\tmode: %c%c%c %c%c%c %c%c%c\033[0;0m\n",
							stbuf->st_mode & S_IRUSR ? 'r' : '-',
							stbuf->st_mode & S_IWUSR ? 'w' : '-',
							stbuf->st_mode & S_IXUSR ? 'x' : '-',
							stbuf->st_mode & S_IRGRP ? 'r' : '-',
							stbuf->st_mode & S_IWGRP ? 'w' : '-',
							stbuf->st_mode & S_IXGRP ? 'x' : '-',
							stbuf->st_mode & S_IROTH ? 'r' : '-',
							stbuf->st_mode & S_IWOTH ? 'w' : '-',
							stbuf->st_mode & S_IXOTH ? 'x' : '-'
							);
			return SQLITE_OK;
		}
		else{
			fprintf(stderr,"\033[31;1mCache readdir failure: %s\033[0;0m %s err: %d\n", path, sqlite3_errmsg( db ), sqlite3_errcode( db ));
			return -ENOENT;
		}
	}

	return -ENOENT;

}
