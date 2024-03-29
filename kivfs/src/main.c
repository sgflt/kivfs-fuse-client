/*----------------------------------------------------------------------------
 * main.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#define FUSE_USE_VERSION 28

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <kivfs.h>

#include "kivfs_operations.h"
#include "config.h"
#include "cache.h"
#include "main.h"

enum{
	KEY_HELP,
	KEY_IP,
	KEY_PORT,
	KEY_POLICY,
	KEY_SIZE
};

int kivfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs){

	printf("\033[31;mparser %d\033[0;m\n", key);

	while ( arg && *arg != '=' )
	{
		arg++;
	}

	arg++;

	switch( key ){
		case KEY_HELP:
			kivfs_help();
			return EXIT_SUCCESS;

		case KEY_IP:
			set_server_ip( arg );
			return EXIT_SUCCESS;

		case KEY_PORT:
			set_server_port( arg );
			return EXIT_SUCCESS;

		case KEY_SIZE:
			set_cache_size( strtoul(arg, NULL, 10) );
			return EXIT_SUCCESS;

		case KEY_POLICY:
			if ( strcmp(arg, "fifo") == 0 )	{ set_cache_policy( KIVFS_FIFO ); return EXIT_SUCCESS; }
			else if ( strcmp(arg, "lfuss") == 0 ) { set_cache_policy( KIVFS_LFUSS ); return EXIT_SUCCESS; }
			else if ( strcmp(arg, "lfu") == 0 ) { set_cache_policy( KIVFS_LFU ); return EXIT_SUCCESS; }
			else if ( strcmp(arg, "lru") == 0 ) { set_cache_policy( KIVFS_LRU ); return EXIT_SUCCESS; }
			else if ( strcmp(arg, "wlfuss") == 0 ) { set_cache_policy( KIVFS_WLFUSS ); return EXIT_SUCCESS; }
			return EXIT_FAILURE;

		default:
			fprintf(stderr, "Unknown option %s\n", arg);
			break;
	}

	return EXIT_FAILURE;
}

int init(){
	const char *cache_path = (const char *)get_cache_dir();

	/* Create cache directory if it doesn't exist */
	if( access(cache_path, F_OK) ){
		if( mkdir(cache_path, S_IRWXU) ){
			perror( cache_path );
			return EXIT_FAILURE;
		}
	}

	if( cache_init() ){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void kivfs_help(){
	fprintf(stderr, "This is help for you\n");
	fprintf(stderr, "kivfs [OPTION]... MOUNT_PATH...\n");
}

int main(int argc, char **argv){

	if ( argc <= 1) {
		kivfs_help();
		return EXIT_FAILURE;
	}

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	struct fuse_opt kivfs_opts[] = {
	     FUSE_OPT_KEY("-h",		KEY_HELP),
	     FUSE_OPT_KEY("--help",	KEY_HELP),
	     FUSE_OPT_KEY("--ip=%s",	KEY_IP),
	     FUSE_OPT_KEY("--host=%s",	KEY_IP),
	     FUSE_OPT_KEY("--port=%i",	KEY_PORT),
	     FUSE_OPT_KEY("--policy=%s", KEY_POLICY),
	     FUSE_OPT_KEY("--size=%s", KEY_SIZE),
	     FUSE_OPT_END
	};


	fuse_opt_parse(&args, NULL, kivfs_opts, kivfs_opt_proc);
	fuse_opt_add_arg(&args, "-obig_writes");

	if ( init() ) {
		fprintf(stderr, "Initialisation failed!\n");
		return EXIT_FAILURE;
	}

	return fuse_main(args.argc, args.argv, &kivfs_operations, NULL);
}
