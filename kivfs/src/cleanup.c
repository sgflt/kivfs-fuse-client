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
#include <inttypes.h>

#include "cache.h"
#include "kivfs_remote_operations.h"
#include "prepared_stmts.h"
#include "config.h"
#include "main.h"
#include "heap.h"
#include "cache-algo-lfuss.h"
#include "cache-algo-wlfuss.h"
#include "cache-algo-fifo.h"
#include "cache-algo-common.h"

int kivfs_hits_compare(void *hits_1, void *hits_2)
{
	if ( ((kivfs_cfile_t *)hits_1)->weight == ((kivfs_cfile_t *)hits_2)->weight)
		return 0;

	return ((kivfs_cfile_t *)hits_1)->weight > ((kivfs_cfile_t *)hits_2)->weight ? 1 : -1;
}

void print_cfile(kivfs_cfile_t *cfile)
{
	fprintf(stderr, VT_INFO "path: %s | w: %f\n" VT_NORMAL, cfile->path, cfile->weight);
}



void kivfs_free_cfile(kivfs_cfile_t *cfile)
{
	free( cfile->path );
	free( cfile );
}

static int lru(const size_t size)
{
	int res = KIVFS_OK;
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(cache_get_db(), "SELECT path,size FROM files WHERE cached = 1 ORDER BY atime DESC", ZERO_TERMINATED, &stmt, NULL);

	res = purge_file(stmt, size);

	sqlite3_finalize( stmt );

	return res;
}

static int qlfuss(const size_t size)
{
	sqlite3_stmt *stmt;

	kivfs_cfile_t global_hits_server;
	kivfs_cfile_t global_hits_client;
	size_t used_size = cache_get_used_size();

	if ( kivfs_remote_global_hits( &global_hits_server )
			|| cache_global_hits( &global_hits_client ) )
	{
		return KIVFS_ERROR;
	}

	if ( used_size + size > get_cache_size() )
	{
		heap_t *heap = heap_create(kivfs_hits_compare, (void (*)(void *))print_cfile, (void (*)(void *))kivfs_free_cfile, 100, HEAP_MIN);

		if ( sqlite3_prepare_v2(cache_get_db(), "SELECT path,size,srv_read_hits,srv_write_hits FROM files WHERE cached = 1", ZERO_TERMINATED, &stmt, NULL))
		{
			fprintf(stderr, VT_ERROR "%s\n", sqlite3_errmsg(cache_get_db()));
			exit( EXIT_FAILURE );
		}

		/* build heap */
		while ( sqlite3_step( stmt ) == SQLITE_ROW )
		{
			kivfs_cfile_t *file_hits = malloc( sizeof( kivfs_cfile_t ) );

			file_hits->path = strdup( (char *)sqlite3_column_text(stmt, 0) );
			file_hits->size = sqlite3_column_int(stmt, 1);
			file_hits->read_hits = sqlite3_column_int(stmt, 2);
			file_hits->write_hits = sqlite3_column_int(stmt, 3);

			fprintf(stderr, VT_ACTION "heap add: %s | size: %lu | r: %" PRIu64 " | w: %" PRIu64 "\n" VT_NORMAL, file_hits->path, file_hits->size, file_hits->read_hits, file_hits->write_hits);

			file_hits->weight = kivfs_lfuss_weight(&global_hits_client, &global_hits_server, file_hits);

			heap_add(heap, file_hits);
			heap_print( heap );
		}

		fprintf(stderr, VT_INFO "HEAP COMPLETE\n" VT_NORMAL);


		/* remove files */
		while ( used_size + size > get_cache_size() )
		{
			kivfs_cfile_t *file_hits = heap_poll( heap );
			char *full_path = get_full_path( file_hits->path );

			used_size -= file_hits->size;

			unlink( full_path );

			cache_set_cached(file_hits->path, 0);
			fprintf(stderr, VT_INFO "File %s removed from cache\n" VT_ACTION, file_hits->path);
			free( full_path );
			kivfs_free_cfile( file_hits );
			heap_print( heap );
		}

		sqlite3_finalize( stmt );
		heap_destroy( heap );
	}

	return KIVFS_OK;
}


int cleanup(const size_t size)
{
	switch( get_cache_policy() )
	{
		case KIVFS_LFUSS:
			return lfuss( size );

		case KIVFS_LRU:
			return lru( size );

		case KIVFS_FIFO:
			return fifo( size );

		case KIVFS_QLFUSS:
			return qlfuss( size );

		case KIVFS_WLFUSS:
			return wlfuss( size );

		default:
			return -ENOSYS;
	}
}


