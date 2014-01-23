/*----------------------------------------------------------------------------
 * kivfs_open_handling.c
 *
 *  Created on: 7. 1. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <fuse.h>

#include "kivfs_operations.h"
#include "kivfs_remote_operations.h"
#include "kivfs_open_handling.h"
#include "config.h"
#include "cache.h"
#include "cleanup.h"
#include "tools.h"

void open_file_wronly(const char *path, kivfs_ofile_t *file, int flags)
{
	/* There is not necessary check errors, because cache is used */
	if( is_connected() )
	{
		kivfs_remote_open(path, flags, file);
	}

	open_local_copy(path, file, flags);
}

void open_with_cache(const char *path, kivfs_ofile_t *file,  int flags)
{
	open_local_copy(path, file, flags);

	/* set the right size for fgetattr, otherwise first read of uncached file will fail*/
	ftruncate(file->fd, file->stbuf.st_size);


	/* if a file is already in cache, we don't have to do more space */
	while ( !cache_contains(path) && cache_get_used_size() + file->stbuf.st_size > get_cache_size() )
	{
		fifo();
		fprintf(stderr, "FIFO DONE DONE. File size %ld | used: %ld | cache_size: %ld\n", file->stbuf.st_size,cache_get_used_size(),get_cache_size());
	}

	fprintf(stderr, "CLEANUP DONE. File size %ld | used: %ld | cache_size: %ld\n", file->stbuf.st_size,cache_get_used_size(),get_cache_size());

}


void open_local_copy(const char *path, kivfs_ofile_t *file, int flags)
{
	char *full_path = get_full_path( path );

	print_open_mode( flags );

	//XXX: do while will be better
	file->fd = open(full_path, flags, S_IRUSR | S_IWUSR ); /* Open if exists, or create in cache */

	if ( file->fd == -1 )
	{
		/* Maybe some dirs are just in database */
		file->fd = recreate_and_open(full_path, file->stbuf.st_mode);

		if ( file->fd == -1 )
		{
			fprintf(stderr, "\033[33;1mkivfs_open: LOCAL OPEN FAILED\033[0m\n");
			//return -ENOENT;
		}
	}
	else
	{
		fprintf(stderr, "\033[34;1mkivfs_open: LOCAL OPEN OK\033[0m\n");
	}

	free( full_path );
}


void open_file(const char *path, kivfs_ofile_t *file,  struct fuse_file_info *fi)
{
	int res;
	kivfs_file_t *file_info = NULL;
	char *full_path = get_full_path( path );

	res = kivfs_remote_file_info(path, &file_info);

	/* Some error on server side occured */
	if ( res )
	{
		//XXX: handle error and remove deleted file from db
		fprintf(stderr, "\033[34;1mkivfs_open: REMOTE FILE INFO, abort open %d\033[0m\n", res);
		return;
	}

	fprintf(stderr, "remote version: %llu, local version: %d\n", file_info->version, cache_get_version( path ));

	/* if local file is older than remote or doesn't exist */
	if ( file_info->version > cache_get_version( path ) || access(full_path, F_OK) != 0 )
	{
		fprintf(stderr, "\033[34;1mRemote file is newer, local will be replaced.\033[0m\n");
		free( full_path );

		res = kivfs_remote_open(path, fi->flags, file);

		if ( res )
		{
			fprintf(stderr, "\033[34;1mkivfs_open: REMOTE OPEN FAILED %d\033[0m\n", res);
			kivfs_free_file( file_info );
			errno = -res;
			return;
		}
		else
		{
			fprintf(stderr, "\033[34;1mkivfs_open: REMOTE OPEN OK\033[0m\n");
		}

		/* If file is NOT too large */
		if ( file_info->size < get_cache_size() / 2 )
		{
			file->stbuf.st_size = file_info->size; /* copy new size to proper truncation in cache */
			open_with_cache(path, file, fi->flags | O_RDWR | (access(full_path, F_OK) != 0 ? O_CREAT : 0));
			cache_set_cached(path, 1);
		}

		cache_update(path, fi, file_info);
	}

	/* local file is up to date, we don't need to open remote file */
	else
	{
		open_local_copy(path, file, fi->flags);
	}

	kivfs_free_file( file_info );
}
