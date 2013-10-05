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

#define concat(cache_path, path) { strcat(full_path, cache_path); strcat(full_path, path); }


static int kivfs_getattr(const char *path, struct stat *stbuf){

	memset(stbuf, 0, sizeof(struct stat));

	if( strcmp(path, "/") == 0 ){
		stbuf->st_mode = S_IFDIR | S_IRWXU;
		stbuf->st_nlink = 2;
	}
	else {
		char *full_path = get_full_path( path );

		if( stat(full_path, stbuf) ){
			perror("\033[31;1mlstat\033[0;0m ");

			//TODO: remote get attr or cache dunno now
			return cache_getattr(path, stbuf);
			//kivfs_file_t file;

			return -ENOENT; //TODO zatim vrací nenalezeno
		}

		free( full_path );
	}


	printf("\n\033[33;1m\tinode: %lu\033[0;0m\n", stbuf->st_ino);
	printf("\033[33;1m\t: %u\033[0;0m\n", stbuf->st_mode);
	printf("\n\033[33;1m\tmode: %c%c%c %c%c%c %c%c%c\033[0;0m\n",
				stbuf->st_mode & S_IRUSR ? 'r' : '-',
				stbuf->st_mode & S_IWUSR ? 'w' : '-',
				stbuf->st_mode & S_IXUSR ? 'x' : '-',
				stbuf->st_mode & S_IRGRP ? 'r' : '-',
				stbuf->st_mode & S_IWGRP ? 'w' : '-',
				stbuf->st_mode & S_IXGRP ? 'x' : '-',
				stbuf->st_mode & S_IROTH ? 'r' : '-',
				stbuf->st_mode & S_IWOTH ? 'w' : '-',
				stbuf->st_mode & S_IXOTH ? 'x' : '-'
				);
	printf("\033[33;1m\tblksize: %ld\033[0;0m\n", stbuf->st_blksize);
	printf("\033[33;1m\tblkcnt: %lu\033[0;0m\n", stbuf->st_blocks);

	return 0;
}

static int kivfs_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
	return fstat(fi->fh, stbuf);
}

static int kivfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi){

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		return cache_readdir(path, buf, filler);
}

static int kivfs_open(const char *path, struct fuse_file_info *fi){

	int res = 0;
	char *full_path = get_full_path( path );

	if( access(full_path, F_OK) != F_OK ){
		//TODO: download from server
		return -ENOENT;
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
		//set_sync_flag(path, FILE_CHANGED);
	}

	return size;
}

static int kivfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){


	char *full_path = get_full_path( path );

	fi->fh = creat(full_path, mode);	// fi->fh pro rychlejší přístup, zatim k ničemu

	if( fi->fh == -1 ){
		return -errno;
	}

	free( full_path );

	cache_add(path, 0, 0, FILE_TYPE_REGULAR_FILE);
	cache_log(path, NULL, KIVFS_TOUCH);

	return 0;
}

static int kivfs_access(const char *path, int mask){

	int res;
	char *full_path = get_full_path( path );

	res = access(full_path, mask);

	if( res == -1 ){
		res = -errno;
	}

	free( full_path );
	return res;
}

static int kivfs_release(const char *path, struct fuse_file_info *fi){

	//TODO: odeslat změny na server

	return close( fi->fh );
}

static int kivfs_truncate(const char *path, off_t size){

	int res;
	char *full_path = get_full_path( path );

	res = truncate(full_path, size);

	if (res == -1){
		res = -errno;
	}

	free( full_path );
	return res;
}

static int kivfs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi){

	int res = 0;

	res = ftruncate(fi->fh, size);

	if( res == -1 ){
		res = -errno;
	}

	return res;
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

	if( !cache_add(path, 0, 0, FILE_TYPE_DIRECTORY) ){
		char *full_path = get_full_path( path );

		cache_log(path, NULL, KIVFS_MKDIR);
		mkdir(full_path, mode);
		free( full_path );
		return 0;
	}

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
	//TODO implement db
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
};
