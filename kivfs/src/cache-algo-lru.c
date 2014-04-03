/*----------------------------------------------------------------------------
 * cache-algo-lru.c
 *
 *  Created on: 3. 4. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <sqlite3.h>
#include <kivfs.h>

#include "cache-algo-common.h"
#include "prepared_stmts.h"
#include "cache.h"

int lru(const size_t size)
{
	int res = KIVFS_OK;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(cache_get_db(), "SELECT path,size FROM files WHERE cached = 1 ORDER BY atime ASC", ZERO_TERMINATED, &stmt, NULL);

	res = purge_file(stmt, size);

	sqlite3_finalize( stmt );

	return res;
}
