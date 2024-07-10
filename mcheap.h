/*

 	MCHEAP Dynamic memory allocator.
	********************************

	An alternative to malloc() and free(), which provides a number of runtime integrity checking options, and some diagnostic features.
	With minimal options enabled, the memory overhead is no larger than malloc/free.



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

 *********************************
 Usage:
 
	void* heap_allocate(size_t size)

		Allocate memory and return it's address.
		Unlike malloc(), failure to allocate memory is considered an error and will be caught by the assertion handler.

 
	void* heap_reallocate(void* org_section, size_t size)

		Reallocate org_section to be a new size.
		Unlike realloc(), this will take the opportunity to de-fragment the heap by moving the allocation to a lower address if possible.
		Also unlike malloc(), failure to re-allocate memory is considered an error and will be caught by the assertion handler.

 
	void* heap_free(void* address)
	
		Free allocated memory.
 */

#ifndef _MCHEAP_H_
#define _MCHEAP_H_

	#include <stdint.h>
	#include <stdbool.h>
	#include <stddef.h>
	#include <stdarg.h>

//********************************************************************************************************
// Public defines
//********************************************************************************************************

//********************************************************************************************************
// Public variables
//********************************************************************************************************

//********************************************************************************************************
// Public prototypes
//********************************************************************************************************

	void*	heap_allocate(size_t size);
	void*	heap_reallocate(void* org_section, size_t size);
	void*	heap_free(void* address);
	size_t  heap_largest_free(void);
	bool	heap_is_intact(void);
#endif