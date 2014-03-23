/*----------------------------------------------------------------------------
 * stats.h
 *
 *  Created on: 23. 3. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *		Jabber: segfault@dione.zcu.cz
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef STATS_H_
#define STATS_H_


/*---------------------------- Structures ----------------------------------*/

typedef enum {
	KIVFS_CACHE_HIT = 0,
	KIVFS_CACHE_MISS = 1,
	KIVFS_CACHE_TOO_LARGE = 2
} kivfs_cache_hit_t;

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
void stats_init(void);
void stats_close(void);
void stats_log(const char *path, kivfs_cache_hit_t type, int value);
void stats_print_all(void);
/*----------------------------- Macros -------------------------------------*/


#endif /* STATS_H_ */
