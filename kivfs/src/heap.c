/*----------------------------------------------------------------------------
 * heap.c
 *
 *  Created on: 12. 2. 2014
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "heap.h"
#include "cleanup.h"
#include "main.h"

/**
 * Create instance of heap
 * @param comparator comapre function for your datatype
 * @param printer print function, can be NULL
 * @param heap_size initial
 * @param type HEAP_MIN or HEAP_MAX
 * @return instance of heap
 */
heap_t * heap_create(int (*comparator)(void *, void *), void (*printer)(void *), void (*free_func)(void *),
		size_t heap_size, heap_type_t type)
{
	heap_t *heap = malloc( sizeof( heap_t ) );
	heap->root = malloc(heap_size * sizeof( heap_node_t * ));
	heap->compare = comparator;
	heap->print = printer;
	heap->free_func = free_func;
	heap->type = type;
	heap->size = heap_size;
	heap->index = 1;

	return heap;
}

/**
 * Compares two items at i1 and i2
 * @param heap instance of heap
 * @param i1 first index
 * @param i2 second index
 * @return positive value if left item is greater than second
 */
static int heap_compare(heap_t *heap, int i1, int i2)
{
	return (heap->type == HEAP_MIN
			? -heap->compare(heap->root[i1]->data, heap->root[i2]->data)
			: heap->compare(heap->root[i1]->data, heap->root[i2]->data));
}

/**
 * Repairs heap after poll.
 * @param heap instance of heap
 */
static void repair_from_top(heap_t *heap)
{
	int k = 1;
	while ( 2 * k < heap->index)
	{
		heap_node_t *tmp;
		int j = 2 * k;

		if ( j < heap->index - 1 &&	 heap_compare(heap, j, j + 1) < 0)
		{
			j++;
		}

		if ( heap_compare(heap, k, j) >= 0)
		{
			break;
		}

		tmp = heap->root[k];
		heap->root[k] = heap->root[j];
		heap->root[j] = tmp;
		k = j;
	}
}

/**
 * Repairs heap after add.
 * @param heap instance of heap
 */
static void repair_from_bottom(heap_t *heap)
{
	int index = heap->index - 1;
	heap_node_t *last = heap->root[index];

	while ( index > 1 &&
			(heap->type == HEAP_MIN
				? -heap->compare(last->data, heap->root[index / 2]->data)
				: heap->compare(last->data, heap->root[index / 2]->data)) > 0
		)
	{
		heap->root[index] = heap->root[index / 2];
		index /= 2;
	}

	heap->root[index] = last;
}

/**
 * Inserts new item pointed by data.
 * @param heap instance of heap
 * @param data pointer to data which have to be stored
 */
void heap_add(heap_t *heap, void *data)
{
	assert( heap != NULL );
	assert( data != NULL );

	heap_node_t *node = malloc( sizeof( heap_node_t ) );

	if ( !node )
	{
		exit( EXIT_FAILURE );
	}

	node->data = data;
	heap->root[heap->index++] = node;

	if ( heap->size == heap->index )
	{
		heap->size *= 2;
		heap->root = realloc(heap->root, heap->size * sizeof(heap_node_t *));

		if ( !heap->root )
		{
			exit( EXIT_FAILURE );
		}
	}

	repair_from_bottom( heap );
}


/**
 * Returns item at the peek of heap.
 * @param heap instance of heap.
 */
void * heap_peek(heap_t *heap)
{
	if ( heap->index == 1 )
	{
		return NULL;
	}

	return heap->root[1]->data;
}

/**
 * Removes and returns item at the peek of heap.
 * @param heap instance of heap
 */
void * heap_poll(heap_t *heap)
{
	void *data = heap->root[1]->data;

	if ( heap->index == 1 )
	{
		return NULL;
	}

	heap->root[1] = heap->root[--heap->index];

	repair_from_top( heap );

	return data;
}

/**
 * Prints heap.
 * @param heap instance of heap
 */
void heap_print(heap_t *heap)
{

	if ( !heap->print )
		return;

	for(int index = 1; index < heap->index; index++)
	{
		heap->print( heap->root[index]->data );
	}
	printf("\n");
}

/**
 * Frees all resources used by heap.
 * @param heap instance of heap
 */
void heap_destroy(heap_t *heap)
{
	if ( !heap->free_func )
		return;

	for(int i = 1; i < heap->index; i++)
	{
		heap->free_func(heap->root[i]->data);
	}

	free( heap->root );
	free( heap );
}
