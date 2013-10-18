/*----------------------------------------------------------------------------
 * connection.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#include <fuse.h>
#include <kivfs.h>

#include "kivfs_operations.h"
#include "connection.h"
#include "config.h"


kivfs_connection_t connection;
int sessid = 0;

int cmd_1arg(kivfs_connection_t *connection, const char *path, KIVFS_VFS_COMMAND action){

	int res;
	kivfs_msg_t *response;

	res = kivfs_send_and_receive(
			connection,
			kivfs_request(sessid, action, KIVFS_STRING_FORMAT, path),
			&response
			);

	if( !res ){
		res = response->head->return_code;
	}

	kivfs_free_msg( response );
	return res;
}

/* should be in libkivfs.so */
int kivfs_connect(kivfs_connection_t *connection, int attempts){
	return kivfs_connect_to(&connection->socket, connection->ip, connection->port, attempts);
}

int kivfs_login(const char *username, const char *password){

	int res;
	kivfs_msg_t *response;
	kivfs_client_t *client;

	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(
					sessid,
					KIVFS_ID,
					KIVFS_STRING_FORMAT,
					username
					),
				&response
			);

	/* If received some data */
	if( !res ){

		res = response->head->return_code;

		/* If no error occured on server side */
		if( !res ){

			res = kivfs_unpack(
					response->data,
					response->head->data_size,
					KIVFS_UPCK_CLNT_FORMAT,
					&client
					);

			/* If succesfully unpacked */
			if( !res ){

				sessid = client->user->id;
				fprintf(stderr, "Connection established.");
				return KIVFS_OK;
			}
		}
	}

	kivfs_free_msg( response );
	kivfs_free_client( client );
	return res;
}


int kivfs_session_init(){
//147.228.63.46		147.228.63.47	 147.228.63.48
	kivfs_set_connection(&connection, "147.228.63.48", 30003);
	kivfs_print_connection( &connection );

	kivfs_connect(&connection, 1);
	kivfs_login("root", "\0");

	return 0;
}

void kivfs_session_disconnect(){
	kivfs_disconnect(connection.socket);
	//TODO close all conections on files
}

int kivfs_remote_file_info(const char * path, kivfs_file_t *file){

	int res;
	kivfs_msg_t *response;

	//kivfs_connect(&connection, 1);

	fprintf(stderr,"\033[33;1mDownloading file info %s\033[0m\n", path);


	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(
				sessid,
				KIVFS_FILE_INFO,
				KIVFS_FILE_INFO_FORMAT,
				path
				),
			&response
			);

	if( !res ){
		fprintf(stderr,"\033[33;1mFile info request sent %s\033[0m\n", path);

		res = response->head->return_code;

		if( !res ){
			res = kivfs_unpack(
					response->data,
					response->head->data_size,
					KIVFS_UPCK_FILE_FORMAT,
					file
					);

			if( !res ){
				fprintf(stderr,"\033[33;1mFile info complete %s\033[0m\n", path);
				//kivfs_print_file( file );
			}
			else{
				printf("\033[33;1mFile not unpacked!\033[0m");
			}
		}
	}

	kivfs_free_msg( response );
	return res;
}


int kivfs_get_to_cache(const char *path){
//TODO
	int res;
	kivfs_msg_t *response;
	kivfs_ofile_t file;
	kivfs_file_t file_info;


	res = kivfs_remote_file_info(path, &file_info);
	fprintf(stderr,"\033[33;1mDownloading whole file %s %llu\033[0m\n", path, file_info.size);

	if( res ){
		fprintf(stderr,"\033[34;1mFile info"
				" failed %s\033[0m\n", path);
		return res;
	}


	res = kivfs_remote_open(path, KIVFS_FILE_MODE_READ, &file);

	if( res ){
		fprintf(stderr,"\033[31;1mFile open failed %s\033[0m\n", path);
		return res;
	}

	//kivfs_connect(&file.connection, 1);

	res = kivfs_send_and_receive(
			&file.connection,
			kivfs_request(
				sessid,
				KIVFS_READ,
				KIVFS_READ_FORMAT,
				path,
				file_info.size,
				0
				),
			&response
			);

	if( !res ){

		res = response->head->return_code;

		/* Check to errors on server side */
		if( !res ){
			char buf[BUFFER_1K];
			ssize_t bytes_received_now;
			char *full_path = get_full_path( path );
			FILE *cache_file = fopen(full_path, "wb");
			free( full_path );

			if( !cache_file ){
				return -errno;
			}

			for(ssize_t bytes_received = 0LL;
					bytes_received < file_info.size;
					bytes_received += bytes_received_now){
				bytes_received_now = read(file.connection.socket, buf, 1);

				if( bytes_received_now == -1 ){
					fclose( cache_file );
					kivfs_free_msg( response );
					return KIVFS_ERROR;
				}
				fwrite(buf, sizeof(char), bytes_received_now, cache_file);
			}
			fclose( cache_file );
		}
	}

	//kivfs_disconnect(file.connection.socket);
	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_readdir(const char *path, kivfs_list_t **files){

	int res;
	kivfs_msg_t *response;


	/* Send READDIR request */
	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(
					sessid,
					KIVFS_READDIR,
					KIVFS_READDIR_FORMAT,
					path
					),
			&response
			);

	if( !res ){

		res = response->head->return_code;

		/* Check to errors on server side */
		if( !res ){

			/* Unpack data to files */
			res = kivfs_unpack(response->data, response->head->data_size, "%files", files);
		}
	}

	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_sync(const char *path, const char *new_path, KIVFS_VFS_COMMAND action){

	switch( action ){
		case KIVFS_MKDIR:
			return kivfs_remote_mkdir( path );

		case KIVFS_RMDIR:
			return kivfs_remote_rmdir( path );

		case KIVFS_TOUCH:
			return kivfs_remote_touch( path );

		//case KIVFS_WRITE:
		//	return kivfs_remote_write();

		//case KIVFS_CHMOD:
		//	return kivfs_remote_chmod();

		case KIVFS_UNLINK:
			return kivfs_remote_unlink( path );

		default:
			return -ENOSYS;
	}
}

// unix to kivfs :-)
kivfs_file_mode_t mode_utok(mode_t mode){

	if( mode & O_RDWR ){
		return KIVFS_FILE_MODE_READ_WRITE;
	}

	if( mode & O_RDONLY ){
		return KIVFS_FILE_MODE_READ;
	}

	if( mode & O_WRONLY ){
		return KIVFS_FILE_MODE_WRITE;
	}

	if( mode & O_APPEND ){
		return KIVFS_FILE_MODE_APPEND;
	}

	return KIVFS_FILE_MODE_READ_WRITE;
}

int kivfs_remote_open(const char *path, mode_t mode, kivfs_ofile_t *file){

	int res;
	kivfs_msg_t *response;

	fprintf(stderr,"\033[33;1;mremote open:\033[0m\n");

	/* Send OPEN request */
	res = kivfs_send_and_receive(
				&connection,
				kivfs_request(
						sessid,
						KIVFS_OPEN,
						KIVFS_OPEN_FORMAT,
						path,
						mode_utok( mode )
						),
					&response
				);

	if( !res ){

		fprintf(stderr,"\033[31;1;mremote open: REQUEST OK\033[0m\n");
		/* Check to errors on server side */
		res = response->head->return_code;
		if( !res ){

			/* Initialize file connection */
			res = kivfs_unpack(
					response->data,
					response->head->data_size,
					"%llu %s %su",
					&file->r_fd,
					&file->connection.ip,
					&file->connection.port
					);

			if( !res ){
				fprintf(stderr,"\033[31;1;mremote open: FILE OK\033[0m\n");
				kivfs_print_connection(&file->connection);
			}
		}
		else{
			fprintf(stderr,"\033[31;1;mremote open: FAIL\033[0m\n");
		}
	}

	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_flush(kivfs_ofile_t *file){

	int res;
	kivfs_msg_t *response;

	/* Send FLUSH request */
	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(
					sessid,
					KIVFS_FLUSH,
					KIVFS_FLUSH_FORMAT,
					file->r_fd
					),
				&response
			);


	if( !res ){
		res = response->head->return_code;
	}

	if( !res ){
		fprintf(stderr, "\033[35;1mFile FLUSH OK\033[0m");
	}
	else{
		fprintf(stderr, "\033[31;1mFile FLUSH FAIL\033[0m");
	}

	kivfs_free_msg( response );
	return res;

}

int kivfs_remote_close(kivfs_ofile_t *file){

	int res;
	kivfs_msg_t *response;

	kivfs_remote_flush( file );


	/* Send CLOSE request */
	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(
					sessid,
					KIVFS_CLOSE,
					KIVFS_CLOSE_FORMAT,
					file->r_fd
					),
				&response
			);

	if( !res ){
		res = response->head->return_code;
	}

	if( !res ){
		fprintf(stderr, "\033[35;1mFile close OK\033[0m\n");
	}
	else{
		fprintf(stderr, "\033[31;1mFile close FAIL\033[0m\n");
	}

	kivfs_disconnect( file->connection.socket );
	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi){

	int res;
	kivfs_msg_t *response;
	kivfs_ofile_t *file = (kivfs_ofile_t *)fi->fh;

	/* Send READ request */
	res = kivfs_send_and_receive(
			&file->connection,
			kivfs_request(
					sessid,
					KIVFS_READ,
					KIVFS_READ_FORMAT,
					path,
					file->r_fd,
					size,
					0
					),
				&response
			);

	if( !res ){

		res = response->head->return_code;

		/* Check to errors on server side */
		if( !res ){
			res = kivfs_recv_data(file->connection.socket, buf, size);
		}

	}

	kivfs_free_msg( response );
	return res;

}

int kivfs_remote_mkdir(const char *path){
	return cmd_1arg(&connection, path, KIVFS_MKDIR);
}

int kivfs_remote_rmdir(const char *path){
	return cmd_1arg(&connection, path, KIVFS_RMDIR);
}

int kivfs_remote_touch(const char *path){
	return cmd_1arg(&connection, path, KIVFS_TOUCH);
}

int kivfs_remote_unlink(const char *path){
	return cmd_1arg(&connection, path, KIVFS_UNLINK);
}

int kivfs_remote_create(const char *path, mode_t mode, kivfs_ofile_t *file){

	int res;

	res = kivfs_remote_touch( path );

	if( !res ){
		res = kivfs_remote_open(path, mode, file);
	}

	if( !res ){
		fprintf(stderr, "\033[35;1mFile CREATE OK.\033[0m\n");
	}
	else{
		fprintf(stderr, "\033[31;1mFile CREATE FAIL.\033[0m");
	}

	return res;
}


int kivfs_remote_write(){
 return -ENOSYS;
}
