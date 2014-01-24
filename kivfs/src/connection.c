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
#include <time.h>

#include "kivfs_operations.h"
#include "kivfs_remote_operations.h"
#include "connection.h"
#include "config.h"
#include "cache.h"


pthread_t rescuer_thr;
pthread_cond_t try_connect = PTHREAD_COND_INITIALIZER;

kivfs_connection_t connection;
int sessid = 0;

void kivfs_restore_connnection(void)
{
	pthread_mutex_lock( get_mutex() );
	pthread_cond_signal( &try_connect );
	pthread_mutex_unlock( get_mutex() );
}

void * kivfs_reconnect(void *args)
{
	int delay = 1;

	for (;;)
	{
		if ( kivfs_connect(&connection, 1) != KIVFS_OK )
		{
			set_retry_count( 9000 );
			set_is_connected( 0 );

			sleep( delay *= 2 );
		}
		else
		{
			kivfs_login("root", "\0");

			set_is_connected( 1 );
			cache_sync();
			pthread_cond_wait(&try_connect, get_mutex());
		}
	}

	return NULL;
}


/* should be in libkivfs.so */
int kivfs_connect(kivfs_connection_t *connection, int attempts)
{
	/*int opt;
	struct in_addr addr;
	struct sockaddr_in srv_addr;
	struct timeval delay;

	connection->socket = socket(AF_INET, SOCK_STREAM, 0);

	addr.s_addr = inet_addr( connection->ip );
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr = addr;
	srv_addr.sin_port = htons( connection->port );

	delay.tv_sec = 1;
	delay.tv_usec = 0;

	opt = 1;

	if ( !setsockopt( connection->socket, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) )
	{
		fprintf(stderr, "kivfs_connect: SO_KEEPALIVE FAILED\n");
	}

	if ( !setsockopt( connection->socket, SOL_SOCKET, SO_RCVTIMEO, &delay, sizeof(delay)) )
	{
		fprintf(stderr, "kivfs_connect: SO_RCVTIMEO FAILED\n");
	}

	fcntl(connection->socket, F_SETFL, O_NONBLOCK);

	if ( connect(connection->socket, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) )
	{
		perror("connection failed");
	}

	return 0;*/
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
				fprintf(stderr, "Connection established with SESSID %d\n", sessid);
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

	pthread_create(&rescuer_thr, NULL, kivfs_reconnect, NULL);

	return 0;
}

void kivfs_session_destroy()
{
	kivfs_disconnect(connection.socket);
	//TODO close all conections on files
}

