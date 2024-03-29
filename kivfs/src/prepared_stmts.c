/*----------------------------------------------------------------------------
 * prepared_stmts.c
 *
 *  Created on: 5. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <sqlite3.h>
#include <stdio.h>
#include <kivfs.h>

#include "kivfs_operations.h"
#include "prepared_stmts.h"
#include "cache.h"
#include "main.h"


void prepare_cache_add(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"INSERT INTO files(path, size, mtime, atime, mode, own, grp, srv_read_hits, srv_write_hits, version)"
					"VALUES(:path, :size, :mtime, :atime, :mode, :owner, :group, :srv_read_hits, :srv_write_hits, :version)";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_add:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "preparee_cache_add:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_rename(sqlite3_stmt **stmt, sqlite3 *db)
{

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
					"	path = :old_path";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_rename:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_rename:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_remove(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"DELETE FROM files WHERE path = :path";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_remove:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_remove:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}


void prepare_cache_readdir(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = "SELECT path FROM files WHERE (path LIKE (:path || '/%') AND path NOT LIKE (:path || '/%/%')) or ('/' LIKE :path AND path NOT LIKE (:path || '%/%'))";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_readdir:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_readdir:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_getattr(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"SELECT atime, mtime, size, mode FROM files WHERE path = :path";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_getattr:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_getattr:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

/* WRITE conflict with TOUCH, TRUNCATE or another WRITE*/
void prepare_log_write(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE log													"
					"SET action=												"
					"	CASE WHEN action=:touch 								"
					"		THEN action 										"
					"		ELSE 												"
					"			CASE WHEN action=:chmod OR action=:write_chmod	"
					"				THEN :write_chmod 							"
					"				ELSE :write END 							"
					"		END													"
					"WHERE path = :path										";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		bind_int(*stmt, ":touch", KIVFS_TOUCH);
		bind_int(*stmt, ":write", KIVFS_WRITE);
		bind_int(*stmt, ":chmod", KIVFS_CHMOD);
		bind_int(*stmt, ":write_chmod", KIVFS_WRITE_CHMOD);

		fprintf(stderr, VT_OK "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}



void prepare_cache_log(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"INSERT INTO log(path, new_path, action)"
					"VALUES(:path, :new_path, :action)";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_log:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_log:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

/*----------------------------------------------------------------------------
   Function : prepare_log_move(sqlite3_stmt **stmt, sqlite3 *db)
   In       : stmt .sqlite statement
   	   	   	  db opened database
   Out      : void
   Job      : Prepares statement.
   Notice   : What the SQL query really do?
   	   	   	  If in Db exists conflicting entry with action KIVFS_WRITE, then
   	   	   	  move appends new destination to the same row.

   	   	   	  If entry has action KIVFS_TOUCH or KIVFS_MKDIR, then replaces
   	   	   	  path with new_path, so no move have to be done.

   	   	   	  The problem is when user do `mv a b` and then `mv b a`
   	   	   	  and `mv a b` again. Implementation should check to existence
   	   	   	  of this circle and delete all midsteps.
 ---------------------------------------------------------------------------*/
void prepare_log_move(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE log 												"
					"SET 														"
					"	path= CASE WHEN (action = :touch OR action = :mkdir)	" /* MKDIR or TOUCH */
					"	 THEN													"
					"		replace(											" /* replace prefix with new prefix */
					"			substr(path, 1, length(:old_path)),				"
					"			:old_path,										"
					"			:new_path										"
					"		) 													"
					"		|| 													"
					"		substr(path, length(:old_path) + 1)					"
					"	ELSE													"
					"		path												"  /* don't change */
					"	END,													"
					"															"
					"	new_path = 												"
					"		CASE 												"
					"			WHEN NOT (action = :touch OR action = :mkdir)	"
					"		THEN												"
					"			:new_path										" /* replace destination */
					"		ELSE												"
					"			new_path										" /* don't change */
					"		END													"
					"															"
					"WHERE 														"
					"	path LIKE (:old_path || '/%') 							" /* subfolders			*/
					"	or 														"
					"	path = :old_path										" /* file or folder 	*/
					;

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		bind_int(*stmt, ":touch", KIVFS_TOUCH);
		bind_int(*stmt, ":mkdir", KIVFS_MKDIR);
		fprintf(stderr, VT_OK "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}


void prepare_log_remove(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"DELETE FROM log WHERE path = :path AND (action=:touch OR action=:mkdir)";


	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		bind_int(*stmt, ":touch", KIVFS_TOUCH);
		bind_int(*stmt, ":mkdir", KIVFS_MKDIR);
		fprintf(stderr, VT_OK "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_log_remote_remove(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE log										"
					"SET action = :action							"
					"WHERE path = :path							";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}


void prepare_cache_file_mode(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"SELECT mode FROM files WHERE path = :path	";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_log_move:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_chmod(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET mode = :mode WHERE path = :path";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_chmod:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_chmod:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_log_chmod(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE log SET action=CASE WHEN action=:write THEN :write_chmod ELSE action END WHERE path = :path"; //459 write+chmod

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		bind_int(*stmt, ":write", KIVFS_WRITE);
		bind_int(*stmt, ":write_chmod", KIVFS_WRITE_CHMOD);

		fprintf(stderr, VT_OK "prepare_log_chmod:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_log_chmod:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_update(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files						"
					"SET								"
					"	size = :size,					"
					"	mtime = :mtime,					"
					"	atime = :atime,					"
					"	mode = :mode,					"
					"	version = :version				"
					"WHERE path = :path					";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_update:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_update:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_get_version(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"SELECT version	FROM files WHERE path = :path ";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_update:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_update:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_inc_read_hits(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET read_hits = read_hits + 1 WHERE path = :path ";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_cache_read_hits:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_read_hits:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_inc_write_hits(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET write_hits = write_hits + 1 WHERE path = :path ";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ){
		fprintf(stderr, VT_OK "prepare_cache_write_hits:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_cache_write_hits:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_set_cached(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET cached = :cached WHERE path = :path ";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_set_cached:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_set_cached:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_update_read_hits(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET read_hits = :read_hits WHERE path = :path ";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_update_read_hits:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_update_read_hits:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_set_modified(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET modified = :modified WHERE path = :path ";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_set_modified:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_set_modified:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_get_used_size(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"SELECT sum(size) FROM files WHERE cached = 1";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_get_used_size:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_get_used_size:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_update_version(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET version = version + 1 WHERE path = :path";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_update_version:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_update_version:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_cache_update_time(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"UPDATE files SET atime = :atime, mtime = :mtime WHERE path = :path";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_update_time:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_update_time:\033[0;0m %s\n", sqlite3_errmsg( db ));
	}
}

void prepare_stats_insert(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"INSERT INTO stats (path, type, value) VALUES (:path, :type, :value)";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_stats_insert: %s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_stats_insert: %s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}
}

void prepare_cache_global_hits(sqlite3_stmt **stmt, sqlite3 *db)
{
	char *sql = 	"SELECT sum(read_hits), sum(write_hits) FROM files WHERE cached = 1";

	if ( !sqlite3_prepare_v2(db, sql, ZERO_TERMINATED, stmt, NULL) ) {
		fprintf(stderr, VT_OK "prepare_global_hits: %s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}
	else {
		fprintf(stderr, VT_ERROR "prepare_global_hits: %s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}
}
