/*----------------------------------------------------------------------------
 * cache-algo-wlfuss.h
 *
 *  Created on: 1. 4. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CACHE_ALGO_WLFUSS_H_
#define CACHE_ALGO_WLFUSS_H_


/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
double wlfuss_read_hits(const void *data);
int wlfuss(const size_t size);
/*----------------------------- Macros -------------------------------------*/


#endif /* CACHE_ALGO_WLFUSS_H_ */
