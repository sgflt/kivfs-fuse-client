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

#define concat(cache_path, path) { strcat(full_path, cache_path); strcat(full_path, path); }


static int kivfs_getattr(const char *path, struct stat *stbuf){

	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));


	fprintf(stderr,"\033[31;1mgetattr %s\033[0;0m ", path);

	if( strcmp(path, "/") == 0 ){
		stbuf->st_mode = S_IFDIR | S_IRWXU;
		stbuf->st_nlink = 2;
	}
	else {
		char *full_path = get_full_path( path );

		/* If stat fails, retrieve info from db or remote server */
		if( stat(full_path, stbuf) ){
			perror("\033[31;1mlstat\033[0;0m ");

			//TODO: remote get attr or cache dunno now
			res = cache_getattr(path, stbuf);
			//kivfs_file_t file;

		}

		free( full_path );
	}

	return res;
}

static int kivfs_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
	return fstat(fi->fh, stbuf);
}

static int kivfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi){

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		/* Read dir from cache, because user want to see all available files */
		return cache_readdir(path, buf, filler);
}

static int kivfs_open(const char *path, struct fuse_file_info *fi){

	int res = 0;
	char *full_path = get_full_path( path );

	if( access(full_path, F_OK) != F_OK ){
		//TODO: download from server

		return kivfs_get_to_cache( path );
	}

	//TODO: check version

	fi->fh = open(full_path , fi->flags);

	if( fi->fh == -1 ){
		res = -errno;
	}


	free( full_path );
	return res;
}

static int kivfs_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi){

		size = pread(fi->fh, buf, size, offset);

		if( size == -1 ){
			size = -errno;
		}

		return size;
}

static int kivfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi){

	size = pwrite(fi->fh, buf, size, offset);		// by 1 byte, zaznamenána velikost skutečně načtených dat

	if( size == -1 ){
		size = -errno;
	}
	else{
		cache_log(path, NULL, KIVFS_WRITE);
	}

	return size;
}

static int kivfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){


	char *full_path = get_full_path( path );

	fi->fh = creat(full_path, mode);	// fi->fh pro rychlejší přístup, zatim k ničemu
	free( full_path );

	if( fi->fh == -1 ){
		return -errno;
	}


	if( !cache_add(path, 0, 0, FILE_TYPE_REGULAR_FILE) ){
		cache_log(path, NULL, KIVFS_TOUCH);
		return 0;
	}

	return -EEXIST;

}

static int kivfs_access(const char *path, int mask){

	int res;
	char *full_path = get_full_path( path );

	res = access(full_path, mask);
	free( full_path );

	/* If a file is not present in cache, compute acces rights from database. */
	if( res ){
		fprintf(stderr, "\033[33;1m Access mode %o %d \033[0;0m",cache_file_mode( path ) & mask, cache_file_mode( path ) & mask ? F_OK : -EACCES);
		return cache_file_mode( path ) & mask ? F_OK : -EACCES;
	}

	return res;
}

static int kivfs_release(const char *path, struct fuse_file_info *fi){

	//TODO: odeslat změny na server
	//fcntl(fd, F_GETFD) pro zjištění lokality deskriptoru
	return close( fi->fh );
}

static int kivfs_truncate(const char *path, off_t size){

	int res;
	char *full_path = get_full_path( path );

	res = truncate(full_path, size);
	free( full_path );

	if( res == -1 ){
		return -errno;
	}

	/* Write will also truncate */
	cache_log(path, NULL, KIVFS_WRITE);

	return res;
}

static int kivfs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi){

	if( ftruncate(fi->fh, size) ){
		return -errno;
	}

	/* Write will also truncate */
	cache_log(path, NULL, KIVFS_WRITE);

	return 0;
}

static int kivfs_rename(const char *old_path, const char *new_path){

	/* If renamed in databese, then rename in cache. */
	if( !cache_rename(old_path, new_path) ){
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

int kivfs_lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock){

	int res;

	res = ulockmgr_op(fi->fh, cmd,lock, &fi->lock_owner, sizeof(fi->lock_owner));

	if( res == -1 ){
		res = -errno;
	}

	return res;
}

static int kivfs_unlink(const char *path){

	//TODO remote remove and add second parameter for cache_remove
	// FLAG_AS_REMOVED + SYNC || DELETE
	if( !cache_remove( path ) ){
		char *full_path = get_full_path( path );

		cache_log(path, NULL, KIVFS_UNLINK);
		unlink( full_path );
		free( full_path );
		return 0;
	}


	return -ENOENT;
}

static int kivfs_mkdir(const char *path, mode_t mode){

	int res;
	char *full_path = get_full_path( path );

	res = mkdir(full_path, mode);
	free( full_path );


	if( res == -1 ){
		return -errno;
	}

	if( !cache_add(path, 0, 0, FILE_TYPE_DIRECTORY) ){
		cache_log(path, NULL, KIVFS_MKDIR);
		return 0;
	}

	fprintf(stderr, "\033[33;1mdir exists\033[0;0m");
	return -EEXIST;
}

static int kivfs_rmdir(const char *path){

	//TODO remote remove and add second parameter for cache_remove
	// FLAG_AS_REMOVED + SYNC || DELETE
	if( !cache_remove( path ) ){
		char *full_path = get_full_path( path );
		int res;

		/* If a directory contains files, then rmdir will fail */
		res =  rmdir( full_path );
		free( full_path );

		/* Log rmdir only if it was succesful */
		if( !res ){
			cache_log(path, NULL, KIVFS_RMDIR);
			return 0;
		}

		return -errno;
	}

	return -ENOENT;

}



static int kivfs_utimens(const char *path, const struct timespec tv[2]){

	int res;
	char *full_path = get_full_path( path );

	res = utimensat(0, full_path, tv, AT_SYMLINK_NOFOLLOW);

	free( full_path );
	return 0; // Don't worry about ENOENT
}

static int kivfs_chmod(const char *path, mode_t mode){
	//TODO online chmod

	return -ENOSYS;
}

static void *kivfs_init(struct fuse_conn_info *conn){
	kivfs_session_init();
	//TODO
	return NULL;
}

static void kivfs_destroy(void * ptr){
	//TODO

	cache_close();
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
};
