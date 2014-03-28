/*----------------------------------------------------------------------------
 * cache-algo-common.h
 *
 *  Created on: 28. 3. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CACHE_ALGO_COMMON_H_
#define CACHE_ALGO_COMMON_H_

#include <unistd.h>
#include <sqlite3.h>

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
double read_hit_1(const void *);
int purge_file(sqlite3_stmt *stmt, size_t size);
/*----------------------------- Macros -------------------------------------*/


#endif /* CACHE_ALGO_COMMON_H_ */
