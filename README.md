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
