MCHEAP
======

 A dynamic memory allocator with runtime testing & error trapping, suited for microcontrollers.
 Uses a classical linked free list like malloc().

 In it's minimal configuration does not have a higher memory overhead per allocation than malloc().

Optionally provides the following features:

 * Caller __FILE__ and __LINE__ ID tracking with macros.
 * Leak detection, which __FILE__ and __LINE__ owns the maximum number of allcoations in the heap.
 * Provides printf-like formatted printing to allocations using prnf (see mickjc750/prnf).
 * Provides a function for listing all current allocations.
 * Tracks the maximum number of allocations which has occurred.
 * Tracks the heap head room (minimum size available).
 * Tests the heap integrity before each operation.
 * Heap space can be at a fixed address, an array in the .bss section (default), or an address determined at runtime.
 * Configurable alignment (defaults to sizeof void*).
 * Able to provide standard functions malloc() calloc() realloc() free()
 * On non uC targets, can offer thread safety using pthreads.h  
  
  
---
ERROR TRAPPING
--------------

The following errors are caught, and passed to assert() or mcassert() via macros. If an error handler is not available errors can simply hang (inf loop) for catching in a debugger.  
(or you could fork and easily edit these to suit your error handler).

* Allocation failure.
* Re-allocation failure.
* Free external (passed an address outside of heap space).
* Reallocate external.
* False free (within heap space, but not an allocation).
* False reallocate.
* Heap broken (failed integrity test).
  
  
---
Configuration
--------------

The following symbols may be defined to configure heap features. These should ideally be added to compiler options using -D (eg -DMCHEAP_ID_SECTIONS or -DMCHEAP_SIZE=4096)

* **MCHEAP_PROVIDE_PRNF**  
Allows formatted printing to the heap using prnf.h (see mickjc750/prnf).  
Also handles string literals in AVR's program memory, by using heap_prnf_SL()  (see usage below).


* **MCHEAP_ID_SECTIONS**  
Includes __FILE__ and __LINE__ information to each call of heap_allocate() heap_free() heap_reallocate() and even heap_prnf_SL().  
This can be used for detecting memory leaks with heap_find_leak().  
If using mcassert.h (-DUSE_MCASSERT), in the event of an error it will also pass the __FILE__ and __LINE__ of the caller directly to the assert handler.  
The memory overhead is typically 6-8 bytes on a 32bit system, or 4 bytes on an 8bit system.


* **MCHEAP_TRACK_STATS**  
After each heap operation the largest free section will be found, and the heap headroom will be tracked.  
These are available in public variables heap_largest_free and heap_head_room.  


* **MCHEAP_TEST**  
Test the entire heap integrity before any heap operation.  
Additionally, each free or reallocate operation, will test that the given address is actually an allocation.  
To test the heap without allocating or freeing, call heap_free(NULL).  
See 'Error handling' below for a description of the types errors caught.  


* **MCHEAP_USE_KEYS**  
Insert key values at the start of each section (both free and allocated) for integrity checking.  
The keys will only be tested if MCHEAP_TEST is defined.  
Adds a sizeof(size_t) overhead to each allocation.  


* **MCHEAP_SIZE**=< size in bytes >  
The heap size in bytes. If this is not defined the default value of 1000 will be used  


* **MCHEAP_ALIGNMENT**  
Ensure all allocations are aligned to the specified byte boundary.  
If this is not defined, the default is sizeof(void*)


* **MCHEAP_ADDRESS**  
Specify a fixed memory address for the heap. This is useful for parts which may have external RAM not covered by the linker script.   
If this is not defined, the heap space will simply be a static uint8_t[] within the BSS section.  
**CAUTION** If this is used, the address provided MUST respect the MCHEAP_ALIGNMENT provided, or an alignment of sizeof(void*).


* **MCHEAP_RUNTIME_ADDRESS**  
Initialize mcheap with an address determined at runtime. This can be used to run mcheap within another memory allocator such as malloc().  
The address must be passed to heap_init() once before any other heap_ call is made.
The address passed must have **MCHEAP_SIZE** bytes available, and respect **MCHEAP_ALIGNMENT**.


* **MCHEAP_PRNF_GROW_STEP**  
When providing formatted printing using prnf.h, a dynamic buffer is used. The buffer will be expanded as needed by this many bytes at a time.  
If this is not defined, the default value is 30 bytes.


* **MCHEAP_NO_ASSERT**  
Do not use mcassert.h or assert.h for error handling. Errors will simply hang in an infinite loop.  


* **MCHEAP_PROVIDE_STDLIB_FUNCTIONS**  
Provides **malloc()** **calloc()** **realloc()** and **free()**  
If **MCHEAP_TRACK_STATS** is enabled:
malloc() and calloc() will return NULL on failure, instead of calling the error handler.  
realloc() will return null if the remaining free space is less than the new size, even though reallocation may still be possible.

* **MCHEAP_USE_POSIX_MUTEX_LOCK**  
Top level functions will use a mutex from pthread.h to provide thread safety.


---
Usage
-----
 
    void* heap_allocate(size_t size)

Allocate memory and return it's address.
Unlike malloc(), failure to allocate memory is considered an error and will be caught by the assertion handler.  
<br />


    void* heap_reallocate(void* org_section, size_t size)

Reallocate org_section to be a new size.
Unlike realloc(), this will take the opportunity to de-fragment the heap by moving the allocation to a lower address if possible.
Also unlike malloc(), failure to re-allocate memory is considered an error and will be caught by the assertion handler.  
<br />


    void* heap_free(void* address)  

Free allocated memory.  
<br />


    bool heap_contains(void* address)

Returns true if an address is within the heap space.  
<br />


    struct heap_leakid_struct heap_find_leak(void)

Returns a structure containing file, line, and count, of the call which currently has the largest number of allocations in the heap.  
This requires **MCHEAP_ID_SECTIONS**.  
<br />

	
    char* heap_prnf_SL(const char* format, ...)

(Requires **MCHEAP_PROVIDE_PRNF**)  
Provide formatted printing to memory allocated on the heap.
If cross platform compatibility with AVR is not required **heap_prnf()** may be used.  
The non-variadic version is also available __char* heap_vprnf(const char* format, va_list va)__  
This is done via a dynamic buffer, which grows in steps of **MCHEAP_PRNF_GROW_STEP** (default 30 bytes)  
This can be changed by adding **-DMCHEAP_PRNF_GROW_STEP=**< size in bytes > to compiler options.  
<br />


    struct heap_list_struct heap_list(unsigned int n)

Return information about the n'th allocation in the heap (file, line, size, content).  
Used for listing current heap allocations.  
If n is out of range, heap_list() returns 0/NULL in all members.  
<br />

---
Status variables
----------------
The following status variables are available
	
* **size_t heap_head_room**  
	The minimum free space which has occurred  (requires **MCHEAP_TRACK_STATS**)  

* **size_t heap_largest_free**  
	The current largest allocatable size  (requires **MCHEAP_TRACK_STATS**)  

* **uint32_t heap_allocations**  
	The current number of allocations  

* **uint32_t heap_allocations_max**  
	The maximum number of allocations that has occurred  

