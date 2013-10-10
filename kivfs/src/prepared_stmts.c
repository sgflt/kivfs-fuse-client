/*----------------------------------------------------------------------------
 * prepared_stmts.c
 *
 *  Created on: 5. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <sqlite3.h>
#include <stdio.h>

#include "prepared_stmts.h"


void prepare_cache_add(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = 	"INSERT INTO files(path, size, mtime, atime, mode, type, read_hits, write_hits, version)"
					"VALUES(:path, :size, :mtime, :atime, :mode, :type, :read_hits, :write_hits, :version)";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_add:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mpreparee_cache_add:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

void prepare_cache_rename(sqlite3_stmt **stmt, sqlite3 *db){

	//TODO merge with log rename
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

void prepare_cache_remove(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = 	"DELETE FROM files WHERE path LIKE :path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_remove:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_remove:\033[0;0m %s\n", sqlite3_errmsg( db ));
}


void prepare_cache_readdir(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = "SELECT path FROM files WHERE (path LIKE (:path || '/%') AND path NOT LIKE (:path || '/%/%')) or ('/' LIKE :path AND path NOT LIKE (:path || '%/%'))";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_readdir:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_readdir:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

void prepare_cache_getattr(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = 	"SELECT atime, mtime, size, mode FROM files WHERE path LIKE :path";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_getattr:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_getattr:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

/* WRITE conflict with TOUCH, TRUNCATE or another WRITE*/
void prepare_log_write(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = 	"UPDATE log										"
					"SET action=									"
					"	CASE WHEN action = 43 THEN 43 ELSE 9 END	"
					"WHERE path LIKE :path							";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
}



void prepare_cache_log(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = 	"INSERT INTO log(path, new_path, action)"
					"VALUES(:path, :new_path, :action)";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_cache_log:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_cache_log:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

/* if locally created file is moved before upload*/
void prepare_log_move(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = 	"UPDATE log 										"
					"SET 												"
					"	path= CASE WHEN (action = 43 OR action = 4)		" /* MKDIR or TOUCH */
					"	 THEN											"
					"		replace(									" /* replace prefix with new prefix */
					"			substr(path, 1, length(:old_path)),		"
					"			:old_path,								"
					"			:new_path								"
					"		) 											"
					"		|| 											"
					"		substr(path, length(:old_path) + 1)			"
					"	ELSE											"
					"		path										"  /* don't change */
					"	END,											"
					"													"
					"	new_path = CASE WHEN NOT (action = 43 OR action = 4)	"
					"	THEN											"
					"		:new_path									" /* replace destination */
					"	ELSE											"
					"		new_path									" /* don't change */
					"	END												"
					"													"
					"WHERE 												"
					"	path LIKE (:old_path || '/%') 					" /* subfolders			*/
					"	or 												"
					"	path LIKE :old_path								" /* file or folder 	*/
					;

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
}


void prepare_log_remove(sqlite3_stmt **stmt, sqlite3 *db){

	char *sql = 	"DELETE FROM log WHERE path LIKE :path AND (action = 43 OR action = 4)";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

void prepare_log_remote_remove(sqlite3_stmt **stmt, sqlite3 *db){
	char *sql = 	"UPDATE log										"
					"SET action = :action							"
					"WHERE path LIKE :path							";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));

}


void prepare_cache_file_mode(sqlite3_stmt **stmt, sqlite3 *db){
	char *sql = 	"SELECT mode FROM files WHERE path LIKE :path	";

	if( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr,"\033[31;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else
		fprintf(stderr,"\033[33;1mprepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
}

