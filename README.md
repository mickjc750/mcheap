MCHEAP Dynamic memory allocator.

 A typical linked free list allocator.

 * Single header (stb style)
 * Supports sandboxing, (multiple implementations in a build).
 * Intended for use on embedded platforms.
 * Configurable reallocate policy.
 * Integrity test.
 * Test suit using https://github.com/silentbicycle/greatest
 * Requires C99 + GCC extensions 
 * Configurable locking mechanism for thread safety

Configuration
*************

 The following symbols may be defined to configure heap features:

MCHEAP_SIZE
 	The heap size in bytes. If this is not defined the default value of 1024 will be used.
	This value must be a multiple of MCHEAP_ALIGNMENT.

MCHEAP_ALIGNMENT
	Ensure all allocations are aligned to the specified byte boundary.
	If this is not defined, the default is __BIGGEST_ALIGNMENT__

MCHEAP_ADDRESS
	Specify a fixed memory address for the heap. This is useful for parts which may have external RAM not covered by the linker script.
 	If this is not defined, the heap space will simply be a static uint8_t[] within the BSS section.
 	**CAUTION** If this is used, the address provided MUST respect the MCHEAP_ALIGNMENT provided, or an alignment of __BIGGEST_ALIGNMENT__

Then ONE of:
	MCHEAP_REALLOC_POLICY_DEFRAG
		This reallocation policy will always try to relocate to a lower address.
		This (on average) reduces fragmentation in the heap, which is beneficial to embedded platforms with limited ram.

	MCHEAP_REALLOC_POLICY_EVICT
		This option will always allocate-copy-free.
		It reduces the build size by simplifiying the operation.
		It also makes the performance more predictable, as a content copy happens on every reallocation.

	MCHEAP_REALLOC_POLICY_RESIZE
		This behaves like regular realloc(), and will attempt to avoid copying the allocations content.
		If the allocation can be extended in place, it will be.
		This improves allcoator performance by avoiding the data copy when possible.

MCHEAP_SANDBOX
	This makes the API static, restricting it to the implementation.
	This allows creating multiple implementations of the allocator in a single build.
	An example of this can be seen in the test, which uses this to test all configurations of MCHEAP_REALLOC_POLICY_xxx


Thread safety
*************

The following two macros may be defined to provide locking if needed, if not provided these default to ((void)0).
	#define mcheap_platform_lock()
	#define mcheap_platform_unlock()




 MCHEAP was originally authored to include a variety of diagnostic features, such as tracking allocations against source code locations, checking for bad addresses passed to free, testing heap integrity, detecting leaks, calling an error handler on allocation failure, and printing formatted text to heap allcoations. It became bloated with more features than a memory allocator should have. Most of the diagnostic features were re-implemented in a separate project called Heaps (https://github.com/mickjc750/heaps) which can be added to any allocator. MCHEAP was then cut back to be just an allocator.

