/*----------------------------------------------------------------------------
 * cleanup.c
 *
 *  Created on: 22. 12. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <kivfs.h>
#include <sqlite3.h>

#include "cache.h"
#include "prepared_stmts.h"
#include "config.h"
#include "main.h"

static int fifo(const size_t size)
{
	int res = KIVFS_OK;
	size_t used_size = cache_get_used_size();
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(cache_get_db(), "SELECT path,size,read_hits,write_hits FROM files WHERE cached = 1 ORDER BY atime DESC", ZERO_TERMINATED, &stmt, NULL);

	fprintf(stderr, VT_INFO "Used: %lu | needed: %lu\n" VT_NORMAL, used_size, size);
	while ( used_size + size > get_cache_size() )
	{
		if ( sqlite3_step( stmt ) == SQLITE_ROW )
		{
			const char *path = (const char *)sqlite3_column_text(stmt, 0);
			const char *full_path = get_full_path(path);

			used_size -= sqlite3_column_int(stmt, 1);

			unlink( full_path );

			cache_set_cached(path, 0);
			fprintf(stderr, "\033[34;1mFile %s removed from cache\n	\033[0m", path);
			free( (void *)full_path );
		}
		else
		{
			fprintf(stderr, "\033[34;1mFile NOT removed from cache\n	\033[0m");
			res = -1;
			break;
		}
	}

	sqlite3_finalize( stmt );

	return res;
}

static int lfuss(const size_t size)
{

}


int cleanup(const size_t size)
{
	switch( get_cache_policy() )
	{
		case KIVFS_LFUSS:
			//return lfuss( size );

		case KIVFS_FIFO:
			return fifo( size );

		default:
			return -ENOSYS;
	}
}


