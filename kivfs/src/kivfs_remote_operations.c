/*----------------------------------------------------------------------------
 * kivfs_remote_operations.c
 *
 *  Created on: 7. 1. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <kivfs.h>
#include <inttypes.h>

#include "main.h"
#include "config.h"
#include "kivfs_operations.h"
#include "kivfs_remote_operations.h"
#include "connection.h"

#define KIVFS_USR(mode) (((mode) >> 6) & 07)
#define KIVFS_GRP(mode) (((mode) >> 3) & 07)
#define KIVFS_OTH(mode) ((mode) & 07)

extern kivfs_connection_t connection;
extern int sessid;

static int cmd_1arg(const char *path, KIVFS_VFS_COMMAND action){

	int res;
	kivfs_msg_t *response;

	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(sessid, action, KIVFS_STRING_FORMAT, path),
			&response
			);

	if( !res ){
		res = response->head->return_code;
	}

	kivfs_free_msg( response );
	return res;
}


int kivfs_remote_file_info(const char * path, kivfs_file_t **file)
{

	int res;
	kivfs_msg_t *response = NULL;

	fprintf(stderr,"\033[33;1mDownloading file info %s\033[0m\n", path);

	if ( !is_connected() )
	{
		return -ENOTCONN;
	}

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

	if ( !res )
	{
		fprintf(stderr,"\033[33;1mFile info request sent %s\033[0m\n", path);

		res = response->head->return_code;

		if ( !res )
		{
			res = kivfs_unpack(
					response->data,
					response->head->data_size,
					KIVFS_UPCK_FILE_FORMAT,
					file
					);

			if ( !res )
			{
				fprintf(stderr,"\033[33;1mFile info complete %s\033[0m\n", path);
				//kivfs_print_file( file );
			}
			else
			{
				printf("\033[33;1mFile not unpacked!\033[0m");
			}
		}
	}

	if ( res == KIVFS_ERC_CONNECTION_TERMINATED || res == KIVFS_ERC_NETWORK_ERROR )
	{
		kivfs_restore_connnection();
	}

	kivfs_free_msg( response );
	return res;
}


int kivfs_get_to_cache(const char *path)
{
//TODO
	int res;
	kivfs_msg_t *response;
	kivfs_ofile_t file;
	kivfs_file_t *file_info;


	res = kivfs_remote_file_info(path, &file_info);
	fprintf(stderr,"\033[33;1mDownloading whole file %s %" PRIu64 "\033[0m\n", path, file_info->size);

	if ( res )
	{
		fprintf(stderr,"\033[34;1mFile info"
				" failed %s\033[0m\n", path);
		return res;
	}


	res = kivfs_remote_open(path, KIVFS_FILE_MODE_READ, &file);

	if ( res )
	{
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
				file_info->size,
				0
				),
			&response
			);

	if ( !res )
	{
		res = response->head->return_code;

		/* Check to errors on server side */
		if ( !res )
		{
			char buf[BUFFER_1K];
			ssize_t bytes_received_now;
			char *full_path = get_full_path( path );
			FILE *cache_file = fopen(full_path, "wb");
			free( full_path );

			if ( !cache_file )
			{
				return -errno;
			}

			for (ssize_t bytes_received = 0LL;
					bytes_received < file_info->size;
					bytes_received += bytes_received_now)
			{
				if ( file_info->size - bytes_received < BUFFER_1K)
				{
					bytes_received_now = read(file.connection.socket, buf, BUFFER_1K);
				}
				else
				{
					bytes_received_now = read(file.connection.socket, buf, file_info->size - bytes_received);
				}

				if ( bytes_received_now == -1 )
				{
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


int kivfs_put_from_cache(const char *path)
{
	int res;
	kivfs_msg_t *response = NULL;
	kivfs_ofile_t file;
	char * full_path = get_full_path( path );

	fprintf(stderr,"\t%s\n", full_path);
	res = stat(full_path, &file.stbuf);


	/* local file is gone, we can't upload anything */
	if ( res == -1 )
	{
		free( full_path );
		return errno;
	}

	res = kivfs_remote_open(path, KIVFS_FILE_MODE_WRITE, &file);

	if ( res )
	{
		fprintf(stderr,"\033[31;1mFile open failed %s\033[0m\n", path);
		return res;
	}

	res = kivfs_send_and_receive(
			&file.connection,
			kivfs_request(
				sessid,
				KIVFS_WRITE,
				KIVFS_WRITE_FORMAT,
				file.r_fd,
				file.stbuf.st_size,
				0
				),
			&response
			);

	if ( !res )
	{
		res = response->head->return_code;

		/* Check to errors on server side */
		if ( !res )
		{
			char buf[BUFFER_128K];
			ssize_t bytes_sent_now;
			FILE *cache_file = fopen(full_path, "rb");

			fprintf(stderr,"FOR::0 -> %lu\n", file.stbuf.st_size);
			for (ssize_t bytes_sent = 0L; bytes_sent < file.stbuf.st_size; bytes_sent += bytes_sent_now) {
				size_t in_buffer_size = fread(buf, sizeof(char), BUFFER_128K, cache_file);

				bytes_sent_now = kivfs_send_data(file.connection.socket, (void *)buf, in_buffer_size);
				fprintf(stderr, "Sent: %ld B\nSent total: %ld\n Buff: %lu\n", bytes_sent_now, bytes_sent, in_buffer_size);

				if( bytes_sent_now <= 0 ){
					perror("WRIET");
					break;
				}
			}

			fclose( cache_file );
		}
	}


	free( full_path );
	kivfs_free_msg( response );
	kivfs_recv_v2(&file.connection, &response);


	res = kivfs_remote_close( &file );

	if( res ){
		fprintf(stderr,"\033[31;1mFile close failed %s\033[0m\n", path);
		return res;
	}

	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_readdir(const char *path, kivfs_list_t **files){

	int res;
	kivfs_msg_t *response;

	if ( !is_connected() )
	{
		return -ENOTCONN;
	}

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

int kivfs_remote_sync(const char *path, const void *path_or_mode, KIVFS_VFS_COMMAND action){

	switch( action ){
		case KIVFS_MKDIR:
			return kivfs_remote_mkdir( path );

		case KIVFS_RMDIR:
			return kivfs_remote_rmdir( path );

		case KIVFS_TOUCH:
			return kivfs_remote_touch( path );

		case KIVFS_CHMOD:
			return kivfs_remote_chmod(path, (mode_t)path_or_mode);

		case KIVFS_UNLINK:
			return kivfs_remote_unlink( path );

		case KIVFS_MOVE:
			return kivfs_remote_rename(path, path_or_mode);

		default:
			return -ENOSYS;
	}
}

// unix to kivfs :-)
kivfs_file_mode_t mode_utok(mode_t mode){

	if( mode & O_RDWR ){
		fprintf(stderr,"KIVFS_FILE_MODE_READ_WRITE\n");
		return KIVFS_FILE_MODE_READ_WRITE;
	}

	if( mode & O_WRONLY ){
		fprintf(stderr,"KIVFS_FILE_MODE_WRITE\n");
		return KIVFS_FILE_MODE_WRITE;
	}

	if( mode & O_APPEND ){
		fprintf(stderr,"KIVFS_FILE_MODE_APPEND\n");
		return KIVFS_FILE_MODE_APPEND;
	}

	fprintf(stderr,"KIVFS_FILE_MODE_READ\n");
	return KIVFS_FILE_MODE_READ;
}

int kivfs_remote_open(const char *path, mode_t mode, kivfs_ofile_t *file){

	int res;
	kivfs_msg_t *response = NULL;

	fprintf(stderr, VT_ACTION "remote open:\n" VT_NORMAL);

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

		fprintf(stderr, VT_OK "remote open: REQUEST OK\n" VT_NORMAL);
		/* Check to errors on server side */
		res = response->head->return_code;
		if( !res ){

			/* Initialize file connection */
			res = kivfs_unpack(
					response->data,
					response->head->data_size,
					KIVFS_UPCK_SRV_FORMAT,
					&file->r_fd,
					&file->connection.ip,
					&file->connection.port
					);

			if( !res ){
				struct timeval timeout; // 10 msec
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
				kivfs_print_connection(&file->connection);

				setsockopt(file->connection.socket, SOL_SOCKET, SO_RCVTIMEO, (const void *) &timeout, sizeof(timeout));
				res = kivfs_connect_to_v2(&file->connection);

				fprintf(stderr, VT_OK "remote open: FILE OK\n" VT_NORMAL);

			}
		}
		else{
			fprintf(stderr, VT_ERROR "remote open: FAIL\n" VT_NORMAL);
		}
	}

	pthread_mutex_init(&file->mutex, NULL);
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
		fprintf(stderr, VT_OK "File FLUSH OK\n" VT_NORMAL);
	}
	else{
		fprintf(stderr, VT_ERROR "File FLUSH FAIL\n" VT_NORMAL);
	}


	kivfs_free_msg( response );
	return res;

}

int kivfs_remote_close(kivfs_ofile_t *file){

	int res;
	kivfs_msg_t *response;

	/* disconnect file first, because fs want's it! */
	kivfs_disconnect_v2( &file->connection );

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
		fprintf(stderr, VT_OK "File close OK\n" VT_NORMAL);
	}
	else{
		fprintf(stderr, VT_ERROR "File close FAIL\n" VT_NORMAL);
	}

	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_read( kivfs_ofile_t *file, char *buf, size_t size,
		off_t offset){

	int res;
	kivfs_msg_t *response = NULL;
	ssize_t bytes_received;

	fprintf(stderr, VT_ACTION "SOCK [%d] port %u read request:\n" VT_NORMAL, file->connection.socket, file->connection.port);
	pthread_mutex_lock( &file->mutex );

	/* Send READ request */
	res = kivfs_send_and_receive(
			&file->connection,
			kivfs_request(
					sessid,
					KIVFS_READ,
					KIVFS_READ_FORMAT,
					file->r_fd,
					size,
					0
					),
				&response
			);

	if ( !res )
	{
		fprintf(stderr, VT_OK "read request OK\n" VT_NORMAL);
		res = response->head->return_code;

		/* Check to errors on server side */
		if ( !res )
		{
			fprintf(stderr, VT_OK "read request remote OK; size %ld\n" VT_NORMAL, size);

			bytes_received = kivfs_recv_data_v2(&file->connection, buf, size);
			if ( bytes_received >= 0 )
			{
				kivfs_free_msg( response );
				res = kivfs_recv_v2(&file->connection, &response);

				if ( !res )
				{
					res = response->head->return_code;
					fprintf(stderr, VT_ACTION "read request complete \n" VT_NORMAL);
				}
			}
		}
		else
		{
			fprintf(stderr, VT_ERROR "read request receive FAILED\n" VT_NORMAL);
		}

	}

	fprintf(stderr, VT_INFO " read %lu B\n" VT_NORMAL, size);
	pthread_mutex_unlock( &file->mutex );
	kivfs_free_msg( response );
	return res ? res : bytes_received;

}

int kivfs_remote_mkdir(const char *path){
	return cmd_1arg(path, KIVFS_MKDIR);
}

int kivfs_remote_rmdir(const char *path){
	return cmd_1arg(path, KIVFS_RMDIR);
}

int kivfs_remote_touch(const char *path){
	return cmd_1arg(path, KIVFS_TOUCH);
}

int kivfs_remote_unlink(const char *path){
	return cmd_1arg(path, KIVFS_UNLINK);
}

int kivfs_remote_create(const char *path, mode_t mode, kivfs_ofile_t *file){

	int res;

	if ( !is_connected() )
	{
		return -ENOTCONN;
	}

	res = kivfs_remote_touch( path );

	if ( !res && file )
	{
		res = kivfs_remote_open(path, O_RDWR, file);
	}

	if ( !res )
	{
		fprintf(stderr, "\033[35;1mFile CREATE OK.\033[0m\n");
	}
	else
	{
		fprintf(stderr, "\033[31;1mFile CREATE FAIL.\033[0m");
	}

	return res;
}

int kivfs_remote_fseek(kivfs_ofile_t *file, off_t offset){

	int res;
	kivfs_msg_t *response;

	pthread_mutex_lock( &file->mutex );

	/* Send WRITE request */
	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(
					sessid,
					KIVFS_FSEEK,
					KIVFS_FSEEK_FORMAT,
					file->r_fd,
					offset
					),
				&response
			);

	if ( !res )
	{
		res = response->head->return_code;
	}

	if( !res )
	{
		fprintf(stderr, "\033[35;1mFile fseek OK\033[0m\n");
	}
	else
	{
		fprintf(stderr, "\033[31;1mFile fseek FAIL\033[0m\n");
	}

	pthread_mutex_unlock( &file->mutex );

	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_write(kivfs_ofile_t *file, const char *buf, size_t size,
		off_t offset){

	int res;
	kivfs_msg_t *response;
	ssize_t bytes_sent;

	if ( !is_connected() )
	{
		return -ENOTCONN;
	}

	fprintf(stderr,"file: %p r_fd: %lu\n size: %lu\n", file, file->r_fd, size);
	pthread_mutex_lock( &file->mutex );

	/* Send WRITE request */
	res = kivfs_send_and_receive(
			&file->connection,
			kivfs_request(
					sessid,
					KIVFS_WRITE,
					KIVFS_WRITE_FORMAT,
					file->r_fd,
					size,
					0 /* unused */
					),
				&response
			);

	if ( !res )
	{
		res = response->head->return_code;

		/* Check to errors on server side */
		if ( !res )
		{
			bytes_sent = kivfs_send_data_v2(&file->connection,(void *)buf, size);

			if ( bytes_sent >= 0 )
			{
				kivfs_free_msg( response );
				res = kivfs_recv_v2(&file->connection, &response);

				if ( !res )
				{
					fprintf(stderr,"\033[31;1m File write result: %d | rc: %d\n",res, response->head->return_code);
					res =  response->head->return_code;
				}
			}
		}
	}



	if ( res >= 0)
	{
		fprintf(stderr, "\033[35;1mFile write OK rc: %d | size: %ld \033[0m\n", res, bytes_sent);
	}
	else
	{
		fprintf(stderr, "\033[31;1mFile write FAIL %d | size: %ld \033[0m\n", res, bytes_sent);
	}

	pthread_mutex_unlock( &file->mutex );
	kivfs_free_msg( response );
	return res ? res : bytes_sent;
}

int kivfs_remote_rename(const char *old_path, const char *new_path)
{
	int res;
	kivfs_msg_t *response;

	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(sessid, KIVFS_MOVE, KIVFS_MOVE_FORMAT, old_path, new_path),
			&response
			);

	if( !res ){
		res = response->head->return_code;
	}

	kivfs_free_msg( response );
	return res;
}

int kivfs_remote_chmod(const char *path, mode_t mode)
{
	int res;
	kivfs_msg_t *response;

	res = kivfs_send_and_receive(
			&connection,
			kivfs_request(
					sessid,
					KIVFS_CHMOD,
					KIVFS_CHMOD_FORMAT,
					KIVFS_USR( mode ),
					KIVFS_GRP( mode ),
					KIVFS_OTH( mode ),
					path
					),
			&response
			);

	if( !res ){
		res = response->head->return_code;
	}

	kivfs_free_msg( response );
	return res;
}
