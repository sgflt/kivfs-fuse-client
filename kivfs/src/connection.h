/*----------------------------------------------------------------------------
 * connection.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CONNECTION_H_
#define CONNECTION_H_

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

#define KIVFS_NO_SESSION		0LL

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/


/**
 * Initialises online session - SSL encryption and recovery thread.
 */
void kivfs_session_init(void);

/**
 * Proceed log in with given username and password.
 * @param username username
 * @param password password
 */
void kivfs_login(const char *username, const char *password);

/**
 * Free all resources at the end of session.
 */
void kivfs_session_destroy(void);

/**
 * Send a signal to the recovery thread, which is probably sleeping.
 */
void kivfs_restore_connnection(void);

/*----------------------------- Macros -------------------------------------*/
#define kivfs_send_and_receive kivfs_send_and_receive_v2

#endif /* CONNECTION_H_ */
