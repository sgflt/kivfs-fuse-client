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
#include <math.h>

#include "kivfs_operations.h"
#include "kivfs_remote_operations.h"
#include "connection.h"
#include "config.h"
#include "cache.h"

static const int MAX_DELAY = 30;

/**
 * Conditional variable for monitor. Signal on this variable wakes up a rescuer thread.
 */
static pthread_cond_t try_connect = PTHREAD_COND_INITIALIZER;

/**
 * Global connection variable is intended for storing connection to the VFS layer.
 */
kivfs_connection_t connection;

/**
 * Unique session identificator.
 */
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

			sleep( delay++ % MAX_DELAY  );
		}
		else
		{
			struct timeval timeout;
			timeout.tv_sec = 10;
			timeout.tv_usec = 0;

			setsockopt(connection.socket, SOL_SOCKET, SO_RCVTIMEO, (const void *) &timeout, sizeof(timeout));
			setsockopt(connection.socket, SOL_SOCKET, SO_SNDTIMEO, (const void *) &timeout, sizeof(timeout));

			kivfs_login("root", "\0");
			delay = 1;

			fprintf(stderr, "LOGGED as root");
			set_is_connected( 1 );
			cache_sync();

			pthread_mutex_lock( get_mutex() );
			pthread_cond_wait(&try_connect, get_mutex());
			pthread_mutex_unlock( get_mutex() );
		}
	}

	return NULL;
}

void kivfs_login(const char *username, const char *password){

	int res;
	kivfs_msg_t *response = NULL;
	kivfs_client_t *client = NULL;

	pthread_mutex_lock( get_mutex() );
	{
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
	}
	pthread_mutex_unlock( get_mutex() );

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
			}
		}
	}

	kivfs_free_msg( response );
	kivfs_free_client( client );
}


void kivfs_session_init()
{
	static pthread_t rescuer_thr;

    /*--- 1st step: Initializing the SSL Library */
    SSL_library_init(); /* load encryption & hash algorithms for SSL */
    SSL_load_error_strings(); /* load the error strings for good error reporting */

	kivfs_set_connection(&connection, get_server()->ip, get_server()->port);
	kivfs_print_connection( &connection );

	pthread_create(&rescuer_thr, NULL, kivfs_reconnect, NULL);
}

void kivfs_session_destroy()
{
	kivfs_disconnect_v2(&connection);
	//TODO close all conections on files
}

