/*----------------------------------------------------------------------------
 * stat.c
 *
 *  Created on: 23. 3. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

#include "prepared_stmts.h"
#include "stats.h"
#include "main.h"

static sqlite3 *db;

static sqlite3_stmt *insert_stmt;

static char *types[] = {
	"hit",
	"miss",
	"too large"
};

static void init_stmts(void)
{
	prepare_stats_insert(&insert_stmt, db);
}

void stats_init(void)
{
	int res;
	char *db_path = ":memory:";

	printf("db path: %s mutex: %d \n", db_path, sqlite3_threadsafe());

	res = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
	if ( res == SQLITE_OK )
	{
		fprintf(stderr, VT_ACTION "creating stats table\n\n" VT_NORMAL);
		res = sqlite3_exec(db,
						"PRAGMA journal_mode=WAL",
						NULL,
						NULL,
						NULL
						);

		res = sqlite3_exec(db,
				"CREATE TABLE 'main'.'stats' ("
				"id				INTEGER PRIMARY KEY,"
				"path 			TEXT NOT NULL,"
				"type 			TEXT NOT NULL,"
				"value			INT NOT NULL"
				")",
				NULL,
				NULL,
				NULL
				);

		if ( res != SQLITE_OK )
		{
			fprintf(stderr, VT_ERROR "cache_init %s\n" VT_NORMAL, sqlite3_errmsg( db ));
			return;
		}
		else
		{
			fprintf(stderr, VT_OK "cache_init %s\n" VT_NORMAL, sqlite3_errmsg( db ));
		}
	}
	else
	{
		fprintf(stderr, VT_ERROR "cache_init %s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}

	init_stmts();
}


void stats_close(void)
{
	sqlite3_finalize( insert_stmt );
	sqlite3_close( db );
}

void stats_log(const char *path, kivfs_cache_hit_t type, int value)
{
	bind_text(insert_stmt, ":path", path);
	bind_text(insert_stmt, ":type", types[type]);
	bind_int(insert_stmt, ":value", value);

	sqlite3_step( insert_stmt );

	if ( sqlite3_reset( insert_stmt ) != SQLITE_OK )
	{
		fprintf(stderr, VT_ERROR "stats_log insert %s\n" VT_NORMAL, sqlite3_errmsg( db ));
	}
	else
	{
		fprintf(stderr, VT_OK "stats_log insert %s\n" VT_NORMAL, path);
	}
}

static int stats_print_callback(void *data, int rows, char **row_data, char **columns)
{
	for(int row = 0; row < rows; row += 3)
	{
		printf("%10s: %20s | %10s: %10s | %10s: %10s\n", columns[row], row_data[row], columns[row + 1], row_data[row + 1], columns[row + 2], row_data[row + 2]);
	}

	return 0;
}

void stats_print_all(void)
{

	fprintf(stderr, VT_ACTION "STATS:\n" VT_NORMAL);
	sqlite3_exec(db, "SELECT path, avg( case when type = 'hit' then 1 else 0 end ) AS hitratio, sum(CASE WHEN type = 'hit' THEN value ELSE 0 END) AS saved FROM stats WHERE type != 'too large' GROUP BY path", stats_print_callback, NULL, NULL);
	fprintf(stderr, VT_ACTION "ENTRIES:\n" VT_NORMAL);
	sqlite3_exec(db, "SELECT path, type, value FROM stats", stats_print_callback, NULL, NULL);
	fprintf(stderr, VT_ACTION "GLOBAL STATS:\n" VT_NORMAL);
	sqlite3_exec(db, "SELECT count(*), avg( case when type = 'hit' then 1 else 0 end ) AS hitratio, sum(CASE WHEN type = 'hit' THEN value ELSE 0 END) AS saved FROM stats WHERE type != 'too large'", stats_print_callback, NULL, NULL);
}
