/*
MCHEAP Dynamic memory allocator.


Configuration
*************

 The following symbols may be defined to configure heap features:

MCHEAP_SIZE
 	The heap size in bytes. If this is not defined the default value of 1000 will be used

MCHEAP_ALIGNMENT
	Ensure all allocations are aligned to the specified byte boundary.
	If this is not defined, the default is sizeof(void*)

MCHEAP_ADDRESS
	Specify a fixed memory address for the heap. This is useful for parts which may have external RAM not covered by the linker script.
 	If this is not defined, the heap space will simply be a static uint8_t[] within the BSS section.
 	**CAUTION** If this is used, the address provided MUST respect the MCHEAP_ALIGNMENT provided, or an alignment of sizeof(void*).

*/

#ifndef _MCHEAP_H_
#define _MCHEAP_H_

	#include <stdbool.h>
	#include <stddef.h>

//********************************************************************************************************
// Public defines
//********************************************************************************************************

//********************************************************************************************************
// Public variables
//********************************************************************************************************

//********************************************************************************************************
// Public prototypes
//********************************************************************************************************

//	Allocate memory and return it's address.
	void*	mcheap_allocate(size_t size);

/*	Reallocate ptr to be a new size.
	If ptr is NULL, attempt a new allocation.
	If size is 0, free the allocation and return NULL.
	Preferred reallocate methods from 1st to last are:
		* relocate to a lower address (this minimizes fragmentation)
		* shinrk in place
		* extened down
		* extend up
		* relocate to a higher address.
	If heap_reallocate() fails, it will return NULL.*/
	void*	mcheap_reallocate(void* ptr, size_t size);

//	Free the allocation, always returns NULL
	void*	mcheap_free(void* ptr);

//	Return largest possible allcoation that can currently be made.
	size_t  mcheap_largest_free(void);

//	Return true if all the heap meta data is valid and intact.
	bool	mcheap_is_intact(void);

#endif