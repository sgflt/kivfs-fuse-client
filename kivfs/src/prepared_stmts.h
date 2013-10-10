/*----------------------------------------------------------------------------
 * prepared_stmts.h
 *
 *  Created on: 6. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef PREPARED_STMTS_H_
#define PREPARED_STMTS_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/
#define ZERO_TERMINATED -1

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

 void prepare_cache_add(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_cache_rename(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_cache_remove(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_cache_readdir(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_cache_getattr(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_cache_log(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_log_move(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_log_remove(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_log_write(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_log_remote_remove(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_cache_file_mode(sqlite3_stmt **stmt, sqlite3 *db);
 void prepare_cache_chmod(sqlite3_stmt **stmt, sqlite3 *db);

/*----------------------------- Macros -------------------------------------*/


#endif /* PREPARED_STMTS_H_ */
