/*----------------------------------------------------------------------------
 * cache-algo-lfuss.c
 *
 *  Created on: 28. 3. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <stdio.h>

#include "cache-algo-common.h"
#include "prepared_stmts.h"
#include "kivfs_remote_operations.h"
#include "cache.h"
#include "cleanup.h"
#include "main.h"

#define UPDATE_DELAY 15 * 60

double kivfs_lfuss_weight(kivfs_cfile_t *global_hits_client,
		kivfs_cfile_t *global_hits_server, kivfs_cfile_t *file_hits)
{
	double g_srv = global_hits_server->read_hits + global_hits_server->write_hits;
	double g_cli = global_hits_client->read_hits + global_hits_client->write_hits;

	fprintf(stderr, VT_INFO "f_r: %lu | f_w: %lu | g_srv [%f] | g_cli [%f]\n" VT_NORMAL,file_hits->read_hits, file_hits->write_hits, g_srv, g_cli);
	return ((double)(file_hits->read_hits - file_hits->write_hits) / g_srv ) *  g_cli;
}

double lfuss_read_hits(const void *data)
{
	static time_t last_update = 0;
	static kivfs_cfile_t global_hits_server = {0};
	kivfs_cfile_t global_hits_client;

	if ( time( NULL ) - last_update > UPDATE_DELAY )
	{
		last_update = time( NULL );
		kivfs_remote_global_hits( &global_hits_server );
		fprintf(stderr, VT_INFO "KIVFS GLOBAL HITS: r: %lu | w: %lu\n" VT_NORMAL, global_hits_server.read_hits, global_hits_server.write_hits);
	}

	if ( !global_hits_server.read_hits ||
			 cache_global_hits( &global_hits_client ) )
	{
		return 1;
	}

	return kivfs_lfuss_weight(&global_hits_client, &global_hits_server, (kivfs_cfile_t *)data);
}

int lfuss(const size_t size)
{
	int res = KIVFS_OK;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(cache_get_db(), "SELECT path,size FROM files WHERE cached = 1 ORDER BY read_hits ASC", ZERO_TERMINATED, &stmt, NULL);

	res = purge_file(stmt, size);

	sqlite3_finalize( stmt );

	return res;
}
