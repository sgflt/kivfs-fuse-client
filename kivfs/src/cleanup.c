/*----------------------------------------------------------------------------
 * cleanup.c
 *
 *  Created on: 22. 12. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <sqlite3.h>
#include "cache.h"
#include "prepared_stmts.h"
#include "config.h"

void fifo(void)
{
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(cache_get_db(), "SELECT path FROM files WHERE cached = 1 LIMIT 1", ZERO_TERMINATED, &stmt, NULL);

	if ( sqlite3_step( stmt ) == SQLITE_ROW )
	{
		const char *path = (const char *)sqlite3_column_text(stmt, 0);
		const char *full_path = get_full_path(path);

		unlink( full_path );

		cache_set_cached(path, 0);
		fprintf(stderr, "\033[34;1mFile %s removed from cache\n	\033[0m", path);
		free( (void *)full_path );
	}
	else
	{
		fprintf(stderr, "\033[34;1mFile NOT removed from cache\n	\033[0m");
	}

	sqlite3_finalize( stmt );
}
