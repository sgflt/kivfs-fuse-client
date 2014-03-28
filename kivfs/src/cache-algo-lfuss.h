/*----------------------------------------------------------------------------
 * cache-algo-lfuss.h
 *
 *  Created on: 28. 3. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CACHE_ALGO_LFUSS_H_
#define CACHE_ALGO_LFUSS_H_

#include "cleanup.h"

/*---------------------------- Structures ----------------------------------*/

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
double kivfs_lfuss_weight(kivfs_ofile_t *global_hits_client,
		kivfs_ofile_t *global_hits_server, kivfs_ofile_t *file_hits);

double lfuss_read_hits(const void *data);

int lfuss(const size_t size);
/*----------------------------- Macros -------------------------------------*/


#endif /* CACHE_ALGO_LFUSS_H_ */
