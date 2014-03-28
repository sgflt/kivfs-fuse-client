/*----------------------------------------------------------------------------
 * cache-algo-common.c
 *
 *  Created on: 28. 3. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <sqlite3.h>
#include <stdio.h>
#include <unistd.h>

#include "main.h"
#include "config.h"

double read_hit_1(const void *path)
{
	return 1;
}


int purge_file(sqlite3_stmt *stmt, size_t size)
{
	int res = 0;
	size_t used_size = cache_get_used_size();

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

	return res;
}
