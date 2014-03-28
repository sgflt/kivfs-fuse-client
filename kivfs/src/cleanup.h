/*----------------------------------------------------------------------------
 * cleanup.h
 *
 *  Created on: 22. 12. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef CLEANUP_H_
#define CLEANUP_H_


/*---------------------------- Structures ----------------------------------*/

typedef struct kivfs_cfile_st {
	char *path;
	size_t size;

	union
	{
		double weight;
		struct
		{
			union {
					uint64_t read_hits; /**< server side read_hits are 64bit int 	*/
					double c_read_hits; /**< client side read_hits are double 		*/
			};
			uint64_t write_hits;
		};
	};
} kivfs_cfile_t;

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/

/**
 * Clean old files from cache.
 * @param size how many bytes have to be clean.
 * @return 0 if cache is clean, -1 otherwise
 */
int cleanup(const size_t size);

/*----------------------------- Macros -------------------------------------*/


#endif /* CLEANUP_H_ */

