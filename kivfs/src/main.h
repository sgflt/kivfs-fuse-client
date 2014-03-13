/*----------------------------------------------------------------------------
 * main.h
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef MAIN_H_
#define MAIN_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/
#define VT_BLACK	"\033[30;1m"
#define VT_RED		"\033[31;1m"
#define VT_GREEN	"\033[32;1m"
#define VT_YELLOW	"\033[33;1m"
#define VT_BLUE		"\033[34;1m"
#define VT_MAGENTA	"\033[35;1m"
#define VT_CYAN		"\033[36;1m"
#define VT_WHITE	"\033[37;1m"
#define VT_NORMAL 	"\033[0m"

#define VT_OK		VT_GREEN
#define VT_ERROR	VT_RED
#define VT_INFO		VT_WHITE
#define VT_WARNING	VT_YELLOW
#define VT_ACTION	VT_MAGENTA

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

/**
 * Print user manual.
 */
void kivfs_help();

/*----------------------------- Macros -------------------------------------*/


#endif /* MAIN_H_ */
