/*----------------------------------------------------------------------------
 * kivfs_open_handling.c
 *
 *  Created on: 7. 1. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <fuse.h>
#include <inttypes.h>

#include "main.h"
#include "kivfs_operations.h"
#include "kivfs_remote_operations.h"
#include "kivfs_open_handling.h"
#include "config.h"
#include "cache.h"
#include "cleanup.h"
#include "tools.h"
#include "stats.h"


/**
 * Open file in cache and set proper size. If cache is full, the clean up is started.
 * @param path path to the file
 * @param file empty ofile structure
 * @param flags flags for opening
 */
static void open_with_cache(const char *path, kivfs_ofile_t *file,  int flags);

void open_file_wronly(const char *path, kivfs_ofile_t *file, int flags)
{

	if( is_connected() )
	{
		/* There is not necessary check errors, because cache is used */
		//kivfs_remote_open(path, flags, file);
		file->flags |= KIVFS_FLG_DELAYED;
	}

	open_local_copy(path, file, flags);
}

static void open_with_cache(const char *path, kivfs_ofile_t *file,  int flags)
{
	open_local_copy(path, file, flags);

	/* set the right size for fgetattr, otherwise first read of uncached file will fail*/
	ftruncate(file->fd, file->stbuf.st_size);

	fprintf(stderr, VT_INFO "Is file n cache? %s!\n" VT_NORMAL, cache_contains(path) ? "YES" : "NO");

	/* if a file is already in cache, we don't have to do more space */
	if ( !cache_contains(path) )
	{
		if ( cleanup( file->stbuf.st_size ) == KIVFS_OK )
		{
			fprintf(stderr, VT_INFO "FIFO DONE DONE. File size %ld | used: %ld | cache_size: %ld\n" VT_NORMAL, file->stbuf.st_size,cache_get_used_size(),get_cache_size());
		}
	}

	fprintf(stderr, VT_INFO "CLEANUP DONE. File size %ld | used: %ld | cache_size: %ld\n" VT_NORMAL, file->stbuf.st_size,cache_get_used_size(),get_cache_size());

}


void open_local_copy(const char *path, kivfs_ofile_t *file, int flags)
{
	char *full_path = get_full_path( path );
	print_open_mode( flags );
	print_stat( &file->stbuf );

	if ( mkdirs( path ) )
	{
		return;
	}

	file->fd = open(full_path, flags | file->flags);

	if ( file->fd == -1 )
	{
		fprintf(stderr, VT_ERROR "kivfs_open: LOCAL OPEN FAILED\n" VT_NORMAL);
	}
	else
	{
		fchmod(file->fd, file->stbuf.st_mode);
		fprintf(stderr, VT_OK "kivfs_open: LOCAL OPEN OK | fd: %" PRIu64 "\n" VT_NORMAL, file->fd);
	}

	free( full_path );
}

void try_open_remote_with_cache(const char *path, kivfs_ofile_t *file,  struct fuse_file_info *fi, kivfs_file_t *file_info)
{
	int res;
	fprintf(stderr, VT_YELLOW "Remote file is newer or missing, local will be replaced.\n" VT_NORMAL);

	res = kivfs_remote_open(path, fi->flags, file);

	if ( res )
	{
		fprintf(stderr, VT_ERROR "kivfs_open: REMOTE OPEN FAILED %d\n" VT_NORMAL, res);
		errno = -res;
		return;
	}
	else
	{
		fprintf(stderr, VT_OK "kivfs_open: REMOTE OPEN OK\n" VT_NORMAL);
	}

	/* If file is NOT too large */
	if ( file_info->size < get_cache_size() / 2 )
	{
		file->stbuf.st_size = file_info->size; /* copy new size to proper truncation in cache */
		open_with_cache(path, file, fi->flags);
		cache_set_cached(path, 1);
	}
	else
	{
		stats_log(path, KIVFS_CACHE_TOO_LARGE, file->stbuf.st_size);
	}

	cache_update(path, fi, file_info);
}

void open_file(const char *path, kivfs_ofile_t *file,  struct fuse_file_info *fi)
{
	int res;
	int file_exists;
	kivfs_file_t *file_info = NULL;
	char *full_path = get_full_path( path );

	res = kivfs_remote_file_info(path, &file_info);
	file->fd = -1; /* error state */

	/* Some error on server side occured, but we ignore connection err or mising remote file */
	switch ( res )
	{
		case KIVFS_OK:
			/* fall through */

		case -ENOTCONN:
			break;

		/* delete file from cache, because it does not exist on the server */
		case KIVFS_ERC_NO_SUCH_FILE:
			unlink( full_path );
			cache_remove( path );
			return;

		default:
			fprintf(stderr, VT_ERROR "kivfs_open: REMOTE FILE INFO, abort open %d\n" VT_NORMAL, res);
			return;
	}

	fprintf(stderr, VT_INFO "remote version: %" PRIu64 ", local version: %d\n" VT_NORMAL, file_info ? file_info->version : -1, cache_get_version( path ));
	kivfs_print_file(file_info);

	file_exists = access(full_path, F_OK) == 0;
	free( full_path );

	if ( file_exists )
	{
		stats_log(path, KIVFS_CACHE_HIT, file->stbuf.st_size);
	}
	else
	{
		stats_log(path, KIVFS_CACHE_MISS, file->stbuf.st_size);
	}

	/* if local file is older than remote or doesn't exist and online*/
	if ( file_info && (file_info->version > cache_get_version( path ) || !file_exists) )
	{
		/* temporarily set local create flag */
		file->flags = O_RDWR | ( !file_exists ? O_CREAT : 0);
		try_open_remote_with_cache(path, file, fi, file_info);
		file->flags = 0;
	}

	/* local file is up to date, we don't need to open remote file */
	else
	{
		open_local_copy(path, file, fi->flags);
	}

	kivfs_free_file( file_info );
}
