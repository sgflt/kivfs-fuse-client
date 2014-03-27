/*----------------------------------------------------------------------------
 * heap.h
 *
 *  Created on: 12. 2. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#ifndef HEAP_H_
#define HEAP_H_

#include <stdint.h>

/*---------------------------- Structures ----------------------------------*/

typedef struct heap_node_st {
		void *data;
} heap_node_t;

typedef enum heap_type_e
{
	HEAP_MIN,
	HEAP_MAX
} heap_type_t;

typedef struct heap_st {
		int (*compare)(void *, void *);
		void (*print)(void *);
		void (*free_func)(void *);
		struct heap_node_st **root;
		size_t size; /* count of elements in array */
		int index; /* free element index */
		heap_type_t type;
} heap_t;

/*---------------------------- CONSTANTS -----------------------------------*/

/*---------------------------- Variables -----------------------------------*/

/*------------------- Functions: ANSI C prototypes -------------------------*/
heap_t * heap_create(int (*comparator)(void *, void *), void (*printer)(void *),
		void (*free_func)(void *), size_t heap_size, heap_type_t type);
void heap_add(heap_t *heap, void *data);
void heap_print(heap_t *heap);
void * heap_poll(heap_t *heap);
void heap_destroy(heap_t *heap);

/*----------------------------- Macros -------------------------------------*/


#endif /* HEAP_H_ */
