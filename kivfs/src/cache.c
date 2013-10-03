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

//XXX: odstranit nepotřebné includy
#include <stdio.h>


#define ZERO_TERMINATED -1
#define concat(cache_path, path) { strcat(full_path, cache_path); strcat(full_path, path); }

static sqlite3 *db;
static sqlite3_stmt *is_cached_stmt;
static sqlite3_stmt *set_sync_stmt;
static sqlite3_stmt *cache_add_stmt;
static sqlite3_stmt *cache_rename_stmt;
static sqlite3_stmt *cache_remove_stmt;

void prepare_is_cached(){

	char *sql = 	"SELECT count(*) FROM files WHERE path LIKE :path AND cached == 1";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, &is_cached_stmt, NULL) ){
		perror("prepare_is_cached");
	}
}

void prepare_set_sync_flag(){

	char *sql = 	"UPDATE files SET sync = ?flag WHERE path LIKE :path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, &set_sync_stmt, NULL) ){
		perror("prepare_set_sync_flag");
	}
}

void prepare_cache_add(){

	char *sql = 	"INSERT INTO files(path, size, mtime, atime, read_hits, write_hits, type, version)"
					"VALUES(:path, ?size, ?mtime, ?atime, ?read_hits, ?write_hits, ?type, ?version)";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, &set_sync_stmt, NULL) ){
		perror("prepare_cache_add");
	}
}

void prepare_cache_rename(){

	char *sql = 	"UPDATE files SET path = :new_path, sync = '1' WHERE path LIKE :old_path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, &cache_rename_stmt, NULL) ){
		perror("prepare_cache_rename");
	}
}

void prepare_cache_remove(){

	char *sql = 	"DELETE FROM files WHERE path LIKE :path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, &cache_remove_stmt, NULL) ){
		perror("prepare_cache_rename");
	}
}

void prepare_statements(){
	prepare_is_cached();
	prepare_set_sync_flag();
	prepare_cache_add();
	prepare_cache_rename();
	prepare_cache_remove();
}

int cache_init(){
	int res;
	char db_path[ PATH_MAX ];
	char *db_filename = "/kivfs.db";

	//XXX: Buffer overflow may occur.
	strcat(db_path, "/tmp");
	strcat(db_path, db_filename);



	printf("db path: %s\n", db_path);

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
			    "read_hits INT NOT NULL DEFAULT ('0'),"
				"write_hits INT NOT NULL DEFAULT ('0'),"
				"cached INT NOT NULL DEFAULT ('0'),"
				"sync INT NOT NULL DEFAULT ('1'),"
				"removed INT NOT NULL DEFAULT('0'),"
				"type INT NOT NULL,"
				"version TEXT NOT NULL)",
			    NULL,
			    NULL,
			    NULL
				);

		if( res != SQLITE_OK ){
			fprintf(stderr,"\033[31;1mcache_init\033[0;0m %s\n", sqlite3_errmsg(db));
			return EXIT_FAILURE;
		}
	}

	prepare_statements();
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

	sqlite3_finalize( is_cached_stmt );
	sqlite3_close( db );
}

/*----------------------------------------------------------------------------
   Function : is_cachedr(const char *path)
   In       :
   Out      :
   Job      :
   Notice   :
 ---------------------------------------------------------------------------*/
int is_cached(const char *path){

	int res;

	sqlite3_bind_text(is_cached_stmt, sqlite3_bind_parameter_index(is_cached_stmt, ":path"), path, ZERO_TERMINATED, NULL);
	res = sqlite3_step( is_cached_stmt );

	if( res == SQLITE_ROW ){
		return sqlite3_column_int(is_cached_stmt, 0);
	}

	return 0;
}

/*----------------------------------------------------------------------------
   Function : set_sync_flag(const char *path)
   In       :
   Out      :
   Job      :
   Notice   :
 ---------------------------------------------------------------------------*/
int set_sync_flag(const char *path, int flag){

	int res;

	sqlite3_bind_text(set_sync_stmt, sqlite3_bind_parameter_index(set_sync_stmt, ":path"), path, ZERO_TERMINATED, NULL);
	sqlite3_bind_int(set_sync_stmt, sqlite3_bind_parameter_index(set_sync_stmt, "?flag"), flag);
	res = sqlite3_step( set_sync_stmt );

	if( res != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mset_flag: %s\033[0;0m %s\n", path, sqlite3_errmsg(db));

	}

	return 0;
}

int cache_add(const char *path, int read_hits, int write_hits, kivfs_file_type_t type){

	int res;
	struct stat stbuf;
	char *full_path = get_full_path( path );

	stat(full_path, &stbuf);

	sqlite3_bind_text(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, ":path"), path, ZERO_TERMINATED, NULL);
	sqlite3_bind_int(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, "?size"), stbuf.st_size);
	sqlite3_bind_int(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, "?mtime"), stbuf.st_mtim.tv_sec);
	sqlite3_bind_int(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, "?atime"), stbuf.st_atim.tv_sec);
	sqlite3_bind_int(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, "?read_hits"), read_hits);
	sqlite3_bind_int(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, "?write_hits"), write_hits);
	sqlite3_bind_int(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, "?type"), type);
	sqlite3_bind_int(cache_add_stmt, sqlite3_bind_parameter_index(cache_add_stmt, "?version"), 0);

	res = sqlite3_step( cache_add_stmt );

	if( res != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_add: %s\033[0;0m %s\n", full_path, sqlite3_errmsg( db ));
		res = -ENOENT;
	}

	free( full_path );
	return res;
}

int cache_rename(const char *old_path, const char *new_path){

	int res;

	sqlite3_bind_text(cache_rename_stmt, sqlite3_bind_parameter_index(cache_rename_stmt, ":old_path"), old_path, ZERO_TERMINATED, NULL);
	sqlite3_bind_text(cache_rename_stmt, sqlite3_bind_parameter_index(cache_rename_stmt, ":new_path"), new_path, ZERO_TERMINATED, NULL);

	res = sqlite3_step( cache_rename_stmt );

	if( res != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_rename: %s\033[0;0m %s\n", old_path, sqlite3_errmsg( db ));
		res = -EEXIST;
	}

	return res;
}

int cache_remove(const char *path){
	int res;

	sqlite3_bind_text(cache_remove_stmt, sqlite3_bind_parameter_index(cache_remove_stmt, ":path"), path, ZERO_TERMINATED, NULL);

	res = sqlite3_step( cache_remove_stmt );

	if( res != SQLITE_OK ){
		fprintf(stderr,"\033[31;1mcache_rename: %s\033[0;0m %s\n", path, sqlite3_errmsg( db ));
	}

	return res;
}
