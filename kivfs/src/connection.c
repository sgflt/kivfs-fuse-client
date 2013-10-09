/*----------------------------------------------------------------------------
 * connection.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#include <kivfs.h>
#include "connection.h"


kivfs_connection_t connection;

int kivfs_session_init(){

	//kivfs_file_mode_t;

	kivfs_set_connection(&connection, "127.0.0.1", 1337);
	//kivfs_set_connection(&connection, "127.0.0.1", 1337); segfault because of free static string
	kivfs_print_connection( &connection );

	kivfs_connect_to(&connection.socket, "127.0.0.1", 1337, 1);

	return 0;
}


/* should be in libkivfs.so */
int kivfs_connect(kivfs_connection_t *connection, int attempts){
	return kivfs_connect_to(&connection->socket, connection->ip, connection->port, attempts);
}

int kivfs_get_to_cache(const char *path){

	kivfs_msg_t *msg;
	kivfs_list_t *files;

	if( !kivfs_send_and_receive(&connection, kivfs_request(KIVFS_NO_SESSION, KIVFS_READ, path), &msg) ){
		printf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	}

	return -ENOSYS;
}
