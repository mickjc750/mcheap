MCHEAP Dynamic memory allocator.

 A typical linked free list allocator.

 * Intended for use on embedded platforms.
 * Reallocate policy favoring defragmentation.
 * Integrity test.
 * Test suit using https://github.com/silentbicycle/greatest
 * Requires C99 + GCC extensions 

Configuration
*************

 The following symbols may be defined to configure heap features:

MCHEAP_SIZE
 	The heap size in bytes. If this is not defined the default value of 1024 will be used.

MCHEAP_ALIGNMENT
	Ensure all allocations are aligned to the specified byte boundary.
	If this is not defined, the default is __BIGGEST_ALIGNMENT__ (usually =8 on 32bit platforms)

MCHEAP_ADDRESS
	Specify a fixed memory address for the heap. This is useful for parts which may have external RAM not covered by the linker script.
 	If this is not defined, the heap space will simply be a static uint8_t[] within the BSS section.


 MCHEAP was originally authored to include a variety of diagnostic features, such as tracking allocations against source code locations, checking for bad addresses passed to free, testing heap integrity, detecting leaks, calling an error handler on allocation failure, and printing formatted text to heap allcoations. It became bloated with more features than a memory allocator should have. Most of the diagnostic features were re-implemented in a separate project called Heaps (https://github.com/mickjc750/heaps) which can be added to any allocator. MCHEAP was then cut back to be just an allocator.

