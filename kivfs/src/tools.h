/*----------------------------------------------------------------------------
 * tools.h
 *
 *  Created on: 12. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef TOOLS_H_
#define TOOLS_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

int mkdirs(const char *path);
int recreate_and_open(const char *path, mode_t mode);
void print_open_mode(int mode);
int kivfs2unix_err(int error);

/*----------------------------- Macros -------------------------------------*/


#endif /* TOOLS_H_ */
