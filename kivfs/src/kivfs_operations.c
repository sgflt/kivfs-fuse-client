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
#include <inttypes.h>

#include "main.h"
#include "config.h"
#include "cache.h"
#include "connection.h"
#include "tools.h"
#include "kivfs_operations.h"
#include "kivfs_remote_operations.h"
#include "kivfs_open_handling.h"
#include "cleanup.h"
#include "stats.h"

#define concat(cache_path, path) { strcat(full_path, cache_path); strcat(full_path, path); }
#define CONFLICT ".conflict"

#define READDIR_DELAY 60

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
		res = cache_getattr(path, stbuf);
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
		memcpy(&file->stbuf, stbuf, sizeof(struct stat));
	}
	else
	{
		res = cache_getattr(path, stbuf);
		memcpy(&file->stbuf, stbuf, sizeof(struct stat));
	}

	return res;
}

static int kivfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi)
{
	struct stat stbuf = {0};

	cache_getattr(path, &stbuf);

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	if ( time(NULL) - stbuf.st_atim.tv_sec > READDIR_DELAY )
	{
		struct timespec tv[2];
		kivfs_list_t *files;

		clock_gettime(CLOCK_REALTIME, &tv[0]);
		tv[1] = stbuf.st_mtim;

		kivfs_remote_readdir(path, &files);

		for (kivfs_adt_item_t *item = files->first; item != NULL; item = item->next)
		{
			if( cache_add(path, item->data) != KIVFS_OK )
			{
				kivfs_version_t version = cache_get_version( path );
				//cache_update_srv_hits()
				fprintf(stderr, "\033[33;1mFILE exists in database\n\t remote version: %" PRIu64 " | local version: %d\n\033[0m",((kivfs_file_t *)(item->data))->version, version);
			}
		}

		cache_update_time(path, tv);
	}
	else
	{
		fprintf(stderr, VT_CYAN "Readdir not performed due DELAY\n" VT_NORMAL);
	}

	/* Read dir from cache, because user want to see all available files */
	return cache_readdir(path, buf, filler);
}

static int kivfs_open(const char *path, struct fuse_file_info *fi)
{
	kivfs_ofile_t *file;
	char *full_path = get_full_path( path );

	file = calloc(1, sizeof( kivfs_ofile_t ));

	if ( !file )
	{
		return -errno;
	}

	fi->fh = (unsigned long)file;

	/* fetch at least file mode for creating a new file na cache */
	cache_getattr(path, &file->stbuf);

	print_open_mode( fi->flags );

	/* If WRONLY, don't check version, because file will be truncated anyway */
	if ( fi->flags & O_WRONLY )
	{
		open_file_wronly(path, file, fi->flags);
		cache_inc_write_hits( path );
	}

	/* READ_ONLY or RDWR. Remote file is copied into cache. */
	else
	{
		open_file(path, file, fi);
		cache_inc_read_hits( path );
	}

	free( full_path );

	fprintf(stderr, VT_WARNING "socket %d\n" VT_NORMAL, file->connection.socket);

	/* if some descriptor is set, then everything is OK */
	return file->fd == -1 && file->r_fd == 0 ? -errno : KIVFS_OK;
}

static int kivfs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi){

	ssize_t _size = -1;
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	/* if no remote descriptor, then local is set */
	if ( !file->r_fd )
	{
		_size = pread(file->fd, buf, size, offset);
	}
	else if( !(file->flags & KIVFS_FLG_ERR) )
	{
		if( size + offset > file->stbuf.st_size ){
			size = file->stbuf.st_size - offset;
		}

		_size = kivfs_remote_read(file, buf, size, offset);

		/* if file can fit into cache */
		if ( file->fd != -1 )
		{
			fprintf(stderr, VT_INFO "COPY to CACHE fd: %lu | _size: %lu | size: %lu\n" VT_NORMAL,file->fd, _size, size);
			//DBG fwrite(buf, 1, _size, stderr);
			if ( pwrite(file->fd, buf, _size, offset) != _size )
			{
				fprintf(stderr, VT_ERROR "COPY to CACHE FAILED fd: %lu | _size: %lu | size: %ld\n" VT_NORMAL,file->fd, _size, size);
				perror("pwrite: ");
			}
		}
		else
		{
			fprintf(stderr, VT_INFO "JUST REMOTE READ fd: %lu | _size: %lu | size: %lu\n" VT_NORMAL, file->r_fd, _size, size);
		}
	}

	return _size;
}

static int kivfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	ssize_t _size = 0;
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	/* XXX: file truncation workaround */
	if ( !(file->flags & KIVFS_FLG_ERR) && (file->flags & KIVFS_FLG_DELAYED) )
	{
		int res = kivfs_remote_open(path, O_WRONLY, file);
		if ( !res )
		{
			file->flags &= ~KIVFS_FLG_DELAYED;
		}
	}

	if ( file->r_fd && !(file->flags & KIVFS_FLG_ERR) )
	{
		_size = kivfs_remote_write(file, buf, size, offset);
		fprintf(stderr, "kivfs_write: REMOTE WRITE %ld bytes\n", _size);
		if ( _size != size )
		{
			file->flags |= KIVFS_FLG_ERR;
		}
	}


	/* remote connection can be lost and _size contains -ENOTCONN, but we can still write into cache */
	if ( file->fd != -1 )
	{
		_size = pwrite(file->fd, buf, size, offset);
		fprintf(stderr, "kivfs_write: LOCAL WRITE %ld bytes %lu\n", _size, (size_t)_size);
	}

	/* set write flag */
	file->flags |= KIVFS_FLG_WR;

	fprintf(stderr, "kivfs_write: TOTAL WRITE %ld bytes %lu\n", _size, (size_t)_size);
	return _size == -1 ? -errno : _size;
}

static int kivfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	kivfs_ofile_t *file;
	char *full_path = get_full_path( path );

	file = calloc(1, sizeof( kivfs_ofile_t ));

	if ( !file )
	{
		return -ENOMEM;
	}

	fi->fh = (unsigned long)file;

	print_open_mode(mode);

	if ( mkdirs( full_path ) )
	{
		return -errno;
	}

	file->fd = creat(full_path , mode);

	if ( file->fd == -1 )
	{
		free( full_path );
		return -errno;
	}


	free( full_path );

	if ( !cache_add(path, NULL) )
	{
		if ( !kivfs_remote_create(path, mode, file) )
		//if ( !kivfs_remote_open(path, mode | O_RDWR, file) )
		{
			//TODO check type of error
			cache_log(path, NULL, KIVFS_TOUCH);
		}

		cache_set_cached(path, 1);

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
		return (cache_file_mode( path ) >> 6) & mask ? F_OK : -EACCES;
	}

	return res;
}

static int kivfs_release(const char *path, struct fuse_file_info *fi)
{
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;


	if ( file->fd != -1 )
	{
		fstat(file->fd, &file->stbuf); /* update file info for cache_update */
		close( file->fd );
		fprintf(stderr, "Local file %lu CLOSED\n", file->fd);
	}

	if ( file->r_fd )
	{
		if ( kivfs_remote_close( file ) )
		{
			fprintf(stderr, "Error: Close remote file\n");
		}
		else
		{
			fprintf(stderr, "Remote file CLOSED\n");
		}
	}

	if ( file->flags & KIVFS_FLG_WR )
	{
		/* small file update from cache */
		if ( file->fd != -1 )
		{
			cache_update(path, fi, NULL);
			fprintf(stderr, "FILE attributes updated\n");
		}

		/* big from server */
		else
		{
			kivfs_file_t *file_info;
			kivfs_remote_file_info(path, &file_info);

			cache_update(path, fi, file_info);
			kivfs_free_file( file_info );
		}

		if ( (file->flags & KIVFS_FLG_ERR) || !is_connected() )
		{
			fprintf(stderr, "File %s marked as modified\n", path);
			cache_set_modified(path, 1); /* mark a file for synchronisation */
		}
		else
		{
			fprintf(stderr, "Version of a %s updated\n", path);
			cache_inc_version( path ); /* just update version because file is already synchronised */
		}
	}

	free( file );
	return 0;
}

static int kivfs_truncate(const char *path, off_t size)
{
	char *full_path = get_full_path( path );

	truncate(full_path, size);
	free( full_path );

	/*if( res == -1 ){
		//XXX: remote return -errno;
	}*/

	/* Write will also truncate */
	//cache_log(path, NULL, KIVFS_WRITE);

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
	//cache_log(path, NULL, KIVFS_WRITE);

	return 0;
}

static int kivfs_rename(const char *old_path, const char *new_path)
{
	int res = KIVFS_OK;

	char *full_old_path = get_full_path( old_path );
	char *full_new_path = get_full_path( new_path );


	if ( mkdirs( full_new_path ) )
	{
		return -errno;
	}

	res = kivfs_remote_rename(old_path, new_path);
	if ( res != KIVFS_OK && !(res == KIVFS_ERC_DIR_EXISTS || res == KIVFS_ERC_FILE_EXISTS))
	{
		cache_log(old_path, new_path, KIVFS_MOVE);
	}


	/* if rename was succesfull or file is just in database */
	else if ( res == KIVFS_OK )
	{
		cache_rename(old_path, new_path);
	}

	rename(full_old_path, full_new_path);

	free( full_new_path );
	free( full_old_path );
	return -kivfs2unix_err( res );
}

static int kivfs_lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock)
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
	if ( !cache_remove( path ) )
	{
		int res;
		char *full_path = get_full_path( path );

		res = unlink( full_path );
		free( full_path );

		/* file can exist only in db, so ignore ENOENT */
		if( res && errno != ENOENT )
		{
			return res;
		}

		res = kivfs_remote_unlink( path );

		if ( res != KIVFS_OK && kivfs2unix_err( res ) != ENOENT )
		{
			cache_log(path, NULL, KIVFS_UNLINK);
			return KIVFS_OK;
		}

		return kivfs2unix_err( res );
	}

	return -ENOENT;
}

static int kivfs_mkdir(const char *path, mode_t mode)
{
	int res;
	char *full_path = get_full_path( path );

	if ( mkdirs( full_path ) )
	{
		return -errno;
	}

	res = mkdir(full_path, mode);

	if( res == -1 )
	{
		res = -errno;
	}

	free( full_path );

	if ( !cache_add(path, NULL) )
	{
		if ( !kivfs_remote_mkdir(path, mode) )
		{
			cache_log(path, NULL, KIVFS_MKDIR);
		}

		return 0;
	}

	fprintf(stderr, "\033[33;1mdir exists\033[0;0m");
	return -res;
}

static int kivfs_rmdir(const char *path)
{
	if ( !cache_remove( path ) )
	{
		int res;
		char *full_path = get_full_path( path );

		/* If a directory contains files, then rmdir will fail */
		res = rmdir( full_path );
		free( full_path );

		/* rmdir may fail too if we are removing remote directory, this case we should ignore */
		if( res && errno != ENOENT )
		{
			return res;
		}


		res = kivfs_remote_rmdir( path );
		/* Only empty directory can be logged */
		if ( res != KIVFS_OK && res != KIVFS_ERC_DIR_NOT_EMPTY )
		{
			/* Log rmdir  */
			cache_log(path, NULL, KIVFS_RMDIR);
		}

		return -kivfs2unix_err( res );
	}

	return -ENOENT;
}



static int kivfs_utimens(const char *path, const struct timespec tv[2])
{
	char *full_path = get_full_path( path );

	utimensat(0, full_path, tv, AT_SYMLINK_NOFOLLOW);

	cache_update_time(path, tv);

	free( full_path );
	return 0; // Don't worry about ENOENT
}

static int kivfs_chmod(const char *path, mode_t mode)
{
	int res;
	char *full_path = get_full_path( path );

	chmod(full_path, mode);
	cache_chmod(path, mode);

	res = kivfs_remote_chmod(path, mode);

	/* if file already have that permissions KIVFS_ERROR is returned, don't log */
	if( res != KIVFS_OK && res != KIVFS_ERROR )
	{
		cache_log(path, NULL, KIVFS_CHMOD);
	}

	free( full_path );
	return 0;
}

static void *kivfs_init(struct fuse_conn_info *conn)
{
	kivfs_session_init();
	stats_init();
	return NULL;
}

static void kivfs_destroy(void * ptr)
{
	cache_close();
	kivfs_session_destroy();

	stats_print_all();
	stats_close();

	fprintf(stderr, "ALL OK\n");
}

static int kivfs_fsync(const char *path, int data, struct fuse_file_info *fi)
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
	.access		= kivfs_access,
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
