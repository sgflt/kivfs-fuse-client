/*----------------------------------------------------------------------------
 * kivfs_operations.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#define FUSE_USE_VERSION 28

#include <fuse.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <kivfs.h>
#include <ulockmgr.h>

#include "config.h"
#include "cache.h"
#include "connection.h"
#include "tools.h"
#include "kivfs_operations.h"

#define concat(cache_path, path) { strcat(full_path, cache_path); strcat(full_path, path); }


static int kivfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));


	fprintf(stderr,"\033[31;1mgetattr %s\033[0;0m ", path);

	if ( strcmp(path, "/") == 0 )
	{
		stbuf->st_mode = S_IFDIR | S_IRWXU;
		stbuf->st_nlink = 2;
	}
	else
	{
		char *full_path = get_full_path( path );

		/* If stat fails, retrieve info from db or remote server */
		if ( stat(full_path, stbuf) )
		{
			perror("\033[31;1mlstat\033[0;0m ");

			//TODO: remote get attr or cache dunno now
			res = cache_getattr(path, stbuf);
			//kivfs_file_t file;

		}

		free( full_path );
	}

	return res;
}

static int kivfs_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	int res;
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	if ( file->fd != -1 )
	{
		fprintf(stderr,"\033[31;1m fgetarr %lu\n\033[0m", file->fd);
		res = fstat(file->fd, stbuf);
	}
	else if ( file->r_fd )
	{
		res = cache_getattr(path, stbuf);
	}

	return res;
}

static int kivfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi)
{
		kivfs_list_t *files;

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		if ( !kivfs_remote_readdir(path, &files) )
		{
			for (kivfs_adt_item_t *item = files->first; item != NULL; item = item->next)
			{
				cache_add(path, item->data);
			}
		}
		else
		{
			fprintf(stderr, "\033[33;1mkivfs remote readdir error");
		}

		/* Read dir from cache, because user want to see all available files */
		return cache_readdir(path, buf, filler);
}

static int kivfs_open(const char *path, struct fuse_file_info *fi)
{
	kivfs_ofile_t *file;
	char *full_path = get_full_path( path );
	struct stat stbuf;
	cache_getattr(path, &stbuf);

	file = (kivfs_ofile_t *)malloc( sizeof( kivfs_ofile_t ) );

	if ( !file )
	{
		return -errno;
	}

	fi->fh = (unsigned long)file;
	memset(file, 0, sizeof(kivfs_ofile_t));

	print_open_mode( fi->flags );

	/* If WRONLY, don't check version, because file will be truncated anyway */
	if ( fi->flags & (O_WRONLY | O_RDWR) )
	{
		/* There is not necessary check errors, because cache is used */
		kivfs_remote_open(path, fi->flags, file);

		//XXX: do while will be better
		file->fd = open(full_path, fi->flags | (access(full_path, F_OK) != 0 ? O_CREAT : 0), S_IRUSR | S_IWUSR ); /* Open if exists, or create in cache */

		if ( file->fd == -1 )
		{
			/* Maybe some dirs are just in database */
			file->fd = recreate_and_open(full_path, stbuf.st_mode);

			if ( file->fd == -1 )
			{
				fprintf(stderr, "\033[33;1mkivfs_open: LOCAL OPEN FAILED mode WRONLY\033[0m\n");
				return -ENOENT;
			}
		}
		else
		{
			fprintf(stderr, "\033[34;1mkivfs_open: LOCAL OPEN OK  mode WRONLY\033[0m\n");
		}

		//XXX: nesedí verze na lokálu se serverem, na serveru je o 1 větší
		cache_update_write_hits( path );
	}

	/* READ_ONLY or RDWR. Remote file is copied into cache. */
	else
	{
		int res;
		kivfs_file_t *file_info = NULL;

		res = kivfs_remote_file_info(path, &file_info);

		/* Some error on server side occured */
		if ( res )
		{
			//XXX: handle error and remove deleted file from db
			fprintf(stderr, "\033[34;1mkivfs_open: REMOTE FILE INFO, abort open %d\033[0m\n", res);
			return res;
		}

		fprintf(stderr, "remote version: %llu, local version: %d\n", file_info->version, cache_get_version( path ));

		/* if local file is older than remote or doesn't exist */
		if ( file_info->version != cache_get_version( path ) || access(full_path, F_OK) != 0 )
		{
			fprintf(stderr, "\033[34;1mRemote file is newer, local will be replaced.\033[0m\n");


			res = kivfs_remote_open(path, fi->flags, file);

			if ( res )
			{
				fprintf(stderr, "\033[34;1mkivfs_open: REMOTE OPEN FAILED %d\033[0m\n", res);
				return res;
			}
			else
			{
				fprintf(stderr, "\033[34;1mkivfs_open: REMOTE OPEN OK\033[0m\n");
			}

			/* If file is NOT too large */
			if ( file_info->size < get_cache_size() / 2 )
			{
				file->fd = open(full_path, fi->flags | O_RDWR | (access(full_path, F_OK) != 0 ? O_CREAT : 0), S_IRUSR | S_IWUSR ); /* Open if exists, or create in cache */

				if ( file->fd == -1 )
				{
					/* Maybe some dirs are just in database */
					file->fd = recreate_and_open(full_path, stbuf.st_mode);

					if ( file->fd == -1 )
					{
						fprintf(stderr, "\033[33;1mkivfs_open: LOCAL OPEN FAILED\033[0m\n");
						return -ENOENT;
					}
					fprintf(stderr, "\033[33;1mkivfs_open: LOCAL OPEN FAILED\033[0m\n");
					perror("File creation failed");
				}
				else
				{
					fprintf(stderr, "\033[34;1mkivfs_open: LOCAL OPEN OK\033[0m\n");
				}

				/* set the right size for fgetattr, otherwise first read of uncached file will fail*/
				ftruncate(file->fd, stbuf.st_size);
			}

			cache_update(path, fi, file_info);
		}

		/* local file is up to date, we don't need to open remote file */
		else
		{
			fprintf(stderr, "Local file is up to date.\n");

			file->fd = open(full_path , fi->flags); /* Open if exists, or create in cache */
			if ( file->fd == -1 )
			{
				fprintf(stderr, "\033[33;1mkivfs_open: LOCAL OPEN FAILED\033[0m\n");
			}
			else
			{
				fprintf(stderr, "\033[34;1mkivfs_open: LOCAL OPEN OK\033[0m\n");
			}
		}

		cache_update_read_hits( path );
		kivfs_free_file( file_info );
	}

	free( full_path );

	/* if some descriptor is set, then everything is OK */
	return file->fd == -1 && file->r_fd == 0 ? -ENOENT : KIVFS_OK;
}

static int kivfs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi){

	ssize_t _size;
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	/* if no remote descriptor, then local is set */
	if ( !file->r_fd )
	{
		_size = pread(file->fd, buf, size, offset);
	}
	else
	{

		struct stat stbuf;
		cache_getattr(path, &stbuf);

		if( size + offset > stbuf.st_size ){
			size = stbuf.st_size - offset;
		}

		_size = kivfs_remote_read(file, buf, size, offset);

		/* if file can fit into cache */
		if ( file->fd != -1 )
		{
			fprintf(stderr, "COPY to CACHE fd: %lu | _size: %lu | size: %lu\n",file->fd, _size, size);
			fwrite(buf, 1, _size, stderr);
			if ( pwrite(file->fd, buf, _size, offset) != _size )
			{
				perror("pwrite: ERRRRRRR");
			}
		}
	}

	return _size;
}

static int kivfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t _size = 0;
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	if ( file->r_fd )
	{
		_size = kivfs_remote_write(file, buf, size, offset);
		fprintf(stderr, "kivfs_write: REMOTE WRITE %ld bytes\n", _size);
	}

	/* set log flag */
	else
	{
		file->write = 1;
	}

	if ( file->fd != -1 && _size == size )
	{
		_size = pwrite(file->fd, buf, _size, offset);
		fprintf(stderr, "kivfs_write: LOCAL WRITE %ld bytes %lu\n", _size, (size_t)_size);
	}

	/*ssize_t local_size;
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	local_size = pwrite(file->fd, buf, size, offset);

	if( local_size == -1 ){

		//size = -errno;
	}

	printf("kivfs_write: r_fd: %lu\n", file->r_fd);
	if( file->r_fd ){

		 ssize_t remote_size;

		remote_size = kivfs_remote_write(file, buf, size, offset);

		file->write = 1;
		size = remote_size;
	}*/

	fprintf(stderr, "kivfs_write: TOTAL WRITE %ld bytes %lu\n", _size, (size_t)_size);
	return _size == -1 ? -errno : _size;
}

static int kivfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	kivfs_ofile_t *file;
	char *full_path = get_full_path( path );

	file = (kivfs_ofile_t *)malloc( sizeof( kivfs_ofile_t ) );

	if ( !file )
	{
		return -ENOMEM;
	}

	fi->fh = (unsigned long)file;
	memset(file, 0, sizeof(kivfs_ofile_t));

	print_open_mode(mode);

	file->fd = creat(full_path, mode);	// fi->fh pro rychlejší přístup, zatim k ničemu

	/* If create fails */
	if ( file->fd == -1 )
	{
		/* Maybe some dirs are just in database */
		file->fd = recreate_and_open(full_path , mode);

		if ( file->fd == -1 )
		{
			free( full_path );
			return -errno;
		}
	}

	free( full_path );

	if ( !cache_add(path, NULL) )
	{
		if ( !kivfs_remote_create(path, mode, file) )
		{
			//TODO check type of error
			cache_log(path, NULL, KIVFS_TOUCH);
		}
		return 0;
	}

	return -EEXIST;

}

static int kivfs_access(const char *path, int mask)
{
	int res;
	char *full_path = get_full_path( path );

	res = access(full_path, mask);
	free( full_path );

	/* If a file is not present in cache, compute acces rights from database. */
	if ( res )
	{
		fprintf(stderr, "\033[33;1m Access mode %o mask %o result %d \033[0;0m",cache_file_mode( path ), mask,  cache_file_mode( path ) & mask ? F_OK : -EACCES);
		return cache_file_mode( path ) & mask ? F_OK : -EACCES;
	}

	return res;
}

static int kivfs_release(const char *path, struct fuse_file_info *fi)
{
	//TODO: odeslat změny na server
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	if ( file->fd != -1 )
	{
		close( file->fd );
	}

	if ( file->r_fd )
	{
		if ( kivfs_remote_close( file ) )
		{
			fprintf(stderr, "Error: Close remote file");
		}
		else
		{
			fprintf(stderr, "File CLOSED");
		}
	}

	//TODO BIGFILE
	if ( file->write )
	{
		//cache_log(path, NULL, KIVFS_WRITE);
		//cache_sync();

		/* small file update from cache */
		if ( file->fd != -1 )
		{
			cache_update(path, fi, NULL);
		}

		/* big from server */
		else
		{
			kivfs_file_t *file_info;
			kivfs_remote_file_info(path, &file_info);

			cache_update(path, fi, file_info);
			kivfs_free_file( file_info );
		}
		/*printf("SLEEP\n");
		sleep(10);
		printf("SLEEP END\n");*/
		//kivfs_put_from_cache( path );
	}

	free( file );
	return 0;
}

static int kivfs_truncate(const char *path, off_t size)
{
	int res;
	char *full_path = get_full_path( path );

	truncate(full_path, size);
	free( full_path );

	/*if( res == -1 ){
		//XXX: remote return -errno;
	}*/

	/* Write will also truncate */
	cache_log(path, NULL, KIVFS_WRITE);

	return 0;
}

static int kivfs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	if ( file->fd != -1 && ftruncate(file->fd, size) )
	{
		return -errno;
	}

	/* Write will also truncate */
	cache_log(path, NULL, KIVFS_WRITE);

	return 0;
}

static int kivfs_rename(const char *old_path, const char *new_path)
{
	/* If renamed in databese, then rename in cache. */
	if ( !cache_rename(old_path, new_path) )
	{
		char *full_old_path = get_full_path( old_path );
		char *full_new_path = get_full_path( new_path );

		rename(full_old_path, full_new_path);
		cache_log(old_path, new_path, KIVFS_MOVE);
		//TODO remote rename or set SYNC + action

		free( full_new_path );
		free( full_old_path );

		return 0;
	}

	return -EISDIR;
}

int kivfs_lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock)
{
	int res;

	res = ulockmgr_op(((kivfs_ofile_t *)fi->fh)->fd, cmd,lock, &fi->lock_owner, sizeof(fi->lock_owner));

	if ( res == -1 )
	{
		res = -errno;
	}

	return res;
}

static int kivfs_unlink(const char *path)
{
	//TODO remote remove and add second parameter for cache_remove
	// FLAG_AS_REMOVED + SYNC || DELETE
	if ( !cache_remove( path ) )
	{
		char *full_path = get_full_path( path );

		cache_log(path, NULL, KIVFS_UNLINK);
		unlink( full_path );
		free( full_path );
		cache_sync();
		return 0;
	}

	return -ENOENT;
}

static int kivfs_mkdir(const char *path, mode_t mode)
{
	int res;
	char *full_path = get_full_path( path );

	res = mkdir(full_path, mode);

	if ( res == -1 )
	{
		/* Maybe some dirs are just in database */
		if ( mkdirs( full_path ) )
		{
			return -errno;
		}

		/* Try again */
		res = mkdir(full_path, mode);

		if ( res == -1 )
		{
			free( full_path );
			return -errno;
		}
	}

	free( full_path );

	if ( !cache_add(path, NULL) )
	{
		cache_log(path, NULL, KIVFS_MKDIR);
		cache_sync();
		return 0;
	}

	fprintf(stderr, "\033[33;1mdir exists\033[0;0m");
	return -EEXIST;
}

static int kivfs_rmdir(const char *path)
{
	//TODO remote remove and add second parameter for cache_remove
	// FLAG_AS_REMOVED + SYNC || DELETE
	if ( !cache_remove( path ) )
	{
		char *full_path = get_full_path( path );

		/* If a directory contains files, then rmdir will fail */
		rmdir( full_path );
		free( full_path );



		/* Log rmdir  */

		cache_log(path, NULL, KIVFS_RMDIR);

		/* Upload action if possible */
		cache_sync();

		return KIVFS_OK;
	}

	return -ENOENT;

}



static int kivfs_utimens(const char *path, const struct timespec tv[2])
{
	int res;
	char *full_path = get_full_path( path );

	res = utimensat(0, full_path, tv, AT_SYMLINK_NOFOLLOW);

	free( full_path );
	return 0; // Don't worry about ENOENT
}

static int kivfs_chmod(const char *path, mode_t mode)
{
	//TODO online chmod

	char *full_path = get_full_path( path );

	chmod(full_path, mode);
	cache_chmod(path, mode);
	cache_log(path, NULL, KIVFS_CHMOD);

	free( full_path );
	return 0;
}

static void *kivfs_init(struct fuse_conn_info *conn)
{
	kivfs_session_init();
	//TODO
	return NULL;
}

static void kivfs_destroy(void * ptr)
{
	//TODO

	cache_close();
	kivfs_session_disconnect();

	fprintf(stderr, "ALL OK\n");
}

int kivfs_fsync(const char *path, int data, struct fuse_file_info *fi)
{

	if ( fi )
	{
		data ? fdatasync( ((kivfs_ofile_t *)fi->fh)->fd ) : fsync( ((kivfs_ofile_t *)fi->fh)->fd );
	}
	else
	{
		cache_sync();
	}

	return 0;
}

struct fuse_operations kivfs_operations = {
	.getattr	= kivfs_getattr,
	.fgetattr	= kivfs_fgetattr,
	.readdir	= kivfs_readdir,
	.open		= kivfs_open,
	.read		= kivfs_read,
	//.access		= kivfs_access,
	.write		= kivfs_write,
	.create		= kivfs_create,
	.release	= kivfs_release,
	.truncate	= kivfs_truncate,
	.ftruncate	= kivfs_ftruncate,
	.rename		= kivfs_rename,
	.lock		= kivfs_lock,
	.unlink		= kivfs_unlink,
	.mkdir		= kivfs_mkdir,
	.rmdir		= kivfs_rmdir,
	.utimens	= kivfs_utimens,
	.chmod		= kivfs_chmod,
	.init		= kivfs_init,
	.fsync		= kivfs_fsync,
	.destroy	= kivfs_destroy,
};
