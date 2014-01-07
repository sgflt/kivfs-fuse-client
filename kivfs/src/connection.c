/*----------------------------------------------------------------------------
 * connection.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/
#include <fuse.h>
#include <kivfs.h>
#include <stdint.h>

#include "kivfs_operations.h"
#include "kivfs_remote_operations.h"
#include "connection.h"
#include "config.h"


kivfs_connection_t connection;
int sessid = 0;

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
	if ( !res )
	{
		res = response->head->return_code;

		/* If no error occured on server side */
		if ( !res )
		{
			res = kivfs_unpack(
					response->data,
					response->head->data_size,
					KIVFS_UPCK_CLNT_FORMAT,
					&client
					);

			/* If succesfully unpacked */
			if ( !res )
			{
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


int kivfs_session_init()
{
	kivfs_set_connection(&connection, get_server()->public_ip, 30003);
	kivfs_print_connection( &connection );

	kivfs_connect(&connection, 1);
	kivfs_login("root", "\0");

	return 0;
}

void kivfs_session_disconnect()
{
	kivfs_disconnect(connection.socket);
	//TODO close all conections on files
}

