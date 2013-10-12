/*----------------------------------------------------------------------------
 * connection.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#include <kivfs.h>

#include "connection.h"

#define cmd_1arg(action,path) kivfs_send_and_receive(&connection, kivfs_request(KIVFS_NO_SESSION, action, path), &response)

kivfs_connection_t connection;

/* should be in libkivfs.so */
int kivfs_connect(kivfs_connection_t *connection, int attempts){
	return kivfs_connect_to(&connection->socket, connection->ip, connection->port, attempts);
}


int kivfs_session_init(){

	kivfs_set_connection(&connection, "127.0.0.1", 1337);
	kivfs_print_connection( &connection );

	return kivfs_connect(&connection, 1);
}



int kivfs_get_to_cache(const char *path){
//TODO
	kivfs_msg_t *response;
	kivfs_list_t *files;

	if( !cmd_1arg(KIVFS_READ, path) ){
		printf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	}

	return -ENOSYS;
}

int kivfs_remote_readdir(const char *path, kivfs_list_t **files){

	kivfs_msg_t *response;


	/* Send READDIR request */
	if( !cmd_1arg( KIVFS_READDIR, path) ){

		/* Check to errors on server side */
		if( !response->head->return_code ){

			/* Unpack data to files */
			if( !kivfs_unpack(response->data, response->head->data_size, "%files", files) ){
				return KIVFS_OK;
			}
		}

		return response->head->return_code;
	}

	return -ENOTCONN;
}

int kivfs_remote_sync(const char *path, const char *new_path, KIVFS_VFS_COMMAND cmd){
	//TODO

	return -ENOSYS;
}

int kivfs_remote_open(){
	return 0; //kivfs_disconnect();
}
