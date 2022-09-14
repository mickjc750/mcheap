/*

 	MCHEAP Dynamic memory allocator.
	********************************

	An alternative to malloc() and free(), which provides a number of runtime integrity checking options, and some diagnostic features.
	With minimal options enabled, the memory overhead is no larger than malloc/free.



Configuration
*************

 The following symbols may be defined to configure heap features:


MCHEAP_PROVIDE_PRNF
 	Allows formatted printing to the heap using prnf.h.
 	Also handles string literals in AVR's program memory, by using heap_prnf_SL()  (see usage below)


MCHEAP_ID_SECTIONS
	Includes __FILE__ and __LINE__ information to each call of heap_allocate() heap_free() heap_reallocate() and even heap_prnf_SL() 
	This can be used for detecting memory leaks with heap_find_leak()
	If using mcassert.h (-DUSE_MCASSERT), in the event of an error it will also pass the __FILE__ and __LINE__ info directly to the assert handler.
	The memory overhead is typically 6-8 bytes on a 32bit system, or 4 bytes on an 8bit system.


MCHEAP_TRACK_STATS  
	If defined, after each heap operation the largest free section will be found, and the heap headroom will be tracked.
	These are available in public variables heap_largest_free and heap_head_room


MCHEAP_TEST
	Test the entire heap integrity before any heap operation.
	Additionally, each free or reallocate operation, will test that the given address is actually an allocation.
	To test the heap without allocating or freeing, call heap_free(NULL).
	See 'Error handling' below for a description of the types errors caught.


MCHEAP_USE_KEYS
  	Insert key values at the start of each section (both free and allocated) for integrity checking.
 	The keys will only be tested if MCHEAP_TEST is defined.
	Adds a sizeof(size_t) overhead to each allocation.


MCHEAP_SIZE
 	The heap size in bytes. If this is not defined the default value of 1000 will be used


MCHEAP_ALIGNMENT
	Ensure all allocations are aligned to the specified byte boundary.
	If this is not defined, the default is sizeof(void*)


MCHEAP_ADDR
	Specify a fixed memory address for the heap. This is useful for parts which may have external RAM not covered by the linker script.
 	If this is not defined, the heap space will simply be a static uint8_t[] within the BSS section.
 	**CAUTION** If this is used, the address provided MUST respect the MCHEAP_ALIGNMENT provided, or an alignment of sizeof(void*).


MCHEAP_PRNF_GROW_STEP
	When providing formatted printing using prnf.h, a dynamic buffer is used. The buffer will be expanded as needed by this many bytes
	at a time. If this is not defined, the default value is 30 bytes.


MCHEAP_NO_ASSERT
	Do not use mcassert.h or assert.h for error handling. Errors will simply hang in an infinite loop.


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


	bool heap_contains(void* address)

		Returns true if an address is within the heap space.
		An application *could* use this to determine if data should be freed from the heap. Although this practice is not recommended.
 

	struct heap_leakid_struct heap_find_leak(void)

		Returns a structure containing file, line, and count, of the call which currently has the largest number of allocations in the heap.
		This requires MCHEAP_ID_SECTIONS.


	(If MCHEAP_PROVIDE_PRNF is defined)
	char* heap_prnf_SL(const char* format, ...)

		Provide formatted printing to memory allocated on the heap.
		If cross platform compatibility with AVR is not required heap_prnf() may be used directly.

		This is done via a simple implementation of a dynamic buffer, which starts as the same size as the format string +30 bytes.
		Each character which exceeds the buffer grows the buffer by a further 30 bytes. When the operation is complete,
		the buffer is shrunk to the size required. The execution overhead of this should be considered if printed output significantly
		exceeds the length of the format string.
		If it is desired to tweak the grow step of 30 bytes to some other value, MCHEAP_PRNF_GROW_STEP may be defined.
		Smaller values will reduce the heap memory used during the operation (but not the end result),
		 but will increase the number of resizing operations.
		Conversely, larger values will require more heap space during the operation, but will reduce the number of resizing operations.


*********************************
Status variables:
 	The following status variables are available
	
	size_t heap_head_room
		The minimum free space which has occurred  (requires MCHEAP_TRACK_STATS)

	size_t heap_largest_free
		The current largest allocatable size  (requires MCHEAP_TRACK_STATS)

	uint32_t heap_allocations
		The current number of allocations
	
	uint32_t heap_allocations_max
		The maximum number of allocations that has occurred


*********************************
Error handling:

	The following situations will be caught and passed to the assertion handler (mcassert.h) or assert.h
	If it is desired NOT to use an error handler, then MCHEAP_NO_ASSERT may be defined.
	In this case the below errors will simply hang.

	Allocation failure.
		heap_allocate() fails.
	
	Re-allocation failure.
		heap_reallocate() fails.

	Free external.
		An address was passed to heap_free() which is not inside the heap space.

	Reallocate external.
		An address was passed to heap_reallocate() which is not inside the heap space.

	False free (requires HEAP_TEST)
		An address is passed to heap_free(), which is not an allocation.
	
	False reallocate (requires HEAP_TEST)
		An address is passed to heap_reallocate(), which is not an allocation.

	Heap broken (requires HEAP_TEST)
		The heap integrity test failed. The application has written outside of it's allocation.

 */

#ifndef _MCHEAP_H_
#define _MCHEAP_H_

	#include <stdint.h>
	#include <stdbool.h>
	#include <stddef.h>

//********************************************************************************************************
// Public defines
//********************************************************************************************************

	#ifdef MCHEAP_ID_SECTIONS
		#define		heap_allocate(arg1)			heap_allocate_id((arg1), __FILE__, __LINE__)
		#define		heap_reallocate(arg1, arg2)	heap_reallocate_id((arg1), (arg2), __FILE__, __LINE__)
		#define		heap_free(arg1)				heap_free_id((arg1), __FILE__, __LINE__)
	#endif

	#ifdef MCHEAP_PROVIDE_PRNF
		#ifdef PLATFORM_AVR
			static inline void heap_fmttst(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
			static inline void heap_fmttst(const char* fmt, ...) {}
		#endif

		#ifdef MCHEAP_ID_SECTIONS
			#define heap_prnf(_fmtarg, ...) 	heap_prnf_id(__FILE__, __LINE__, _fmtarg ,##__VA_ARGS__)
			#ifdef PLATFORM_AVR
				#define heap_prnf_P(_fmtarg, ...) 	heap_prnf_P_id(PSTR(__FILE__), __LINE__, _fmtarg ,##__VA_ARGS__)
				#define heap_prnf_SL(_fmtarg, ...) 	({char* _prv; _prv = heap_prnf_P_id(PSTR(__FILE__), __LINE__, PSTR(_fmtarg) ,##__VA_ARGS__); while(0) heap_fmttst(_fmtarg ,##__VA_ARGS__); _prv;})
			#else
				#define heap_prnf_SL(_fmtarg, ...) 	heap_prnf_id(__FILE__, __LINE__, _fmtarg ,##__VA_ARGS__)
			#endif
		#else
			#ifdef PLATFORM_AVR
				#define heap_prnf_SL(_fmtarg, ...) 	({char* _prv; _prv = heap_prnf_P(PSTR(_fmtarg) ,##__VA_ARGS__); while(0) heap_fmttst(_fmtarg ,##__VA_ARGS__); _prv;})
			#else
				#define heap_prnf_SL(_fmtarg, ...) 	heap_prnf(_fmtarg ,##__VA_ARGS__)
			#endif
		#endif
	#endif

	#ifdef MCHEAP_ID_SECTIONS
	struct heap_leakid_struct
	{
		const char* file_id;
		uint16_t	line_id;
		uint32_t	cnt;
	};

	struct heap_list_struct
	{
		const char* file_id;
		uint16_t	line_id;
		size_t		size;
		void*		content;
	};
	#endif

//********************************************************************************************************
// Public variables
//********************************************************************************************************

	//the minimum free space which has occurred since heap_init() (requires TRACK_STATS)
	extern size_t	heap_head_room;	

	//the current largest free allocatable size (requires TRACK_STATS)
	extern size_t 	heap_largest_free;

	//the current number of allocations
	extern uint32_t	heap_allocations;

	//the maximum number of allocations that has occurred
	extern uint32_t	heap_allocations_max;

//********************************************************************************************************
// Public prototypes
//********************************************************************************************************

	#ifdef	MCHEAP_ID_SECTIONS
		void*	heap_allocate_id(size_t size, const char* id_file, uint16_t id_line);
		void*	heap_reallocate_id(void* org_section, size_t size, const char* id_file, uint16_t id_line);
		void*	heap_free_id(void* address, const char* id_file, uint16_t id_line);
	#else
		void*	heap_allocate(size_t size);
		void*	heap_reallocate(void* org_section, size_t size);
		void*	heap_free(void* address);
	#endif

	#ifdef MCHEAP_PROVIDE_PRNF
		#ifdef MCHEAP_ID_SECTIONS
			#ifdef PLATFORM_AVR
				char* heap_prnf_P_id(PGM_P id_file, uint16_t id_line, PGM_P fmt, ...);
			#endif
			char* heap_prnf_id(const char* id_file, uint16_t id_line, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
		#else
			#ifdef PLATFORM_AVR
				char* heap_prnf_P(PGM_P fmt, ...);
			#endif
			char* heap_prnf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
		#endif
	#endif

	//return true if address is within heap space
	bool	heap_contains(void* address);

	#ifdef MCHEAP_ID_SECTIONS

	//return id of the caller which currently has the most allocations in the heap
	//requires MCHEAP_ID_SECTIONS
	struct heap_leakid_struct heap_find_leak(void);

//	Can be used for listing current heap allocations
//	Return information about the n'th allocation in the heap
//	n should be from 0 to heap_allocations-1
//	If it is outside of this range, heap_list returns 0/NULL in all members
	struct heap_list_struct heap_list(unsigned int n);

	#endif

#endif