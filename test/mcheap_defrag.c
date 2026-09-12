
#include <stddef.h>


#define MCHEAP_SANDBOX
#define MCHEAP_REALLOC_POLICY_DEFRAG
#define MCHEAP_IMPLEMENTATION
#include "../mcheap.h"

// Wrappers for access to this sandboxed implementation of mcheap built with MCHEAP_REALLOC_POLICY_DEFRAG
void*	mcheap_defrag_allocate(size_t size) {return mcheap_allocate(size);}
void*	mcheap_defrag_reallocate(void* ptr, size_t size) {return mcheap_reallocate(ptr, size);}
void*	mcheap_defrag_free(void* ptr) {return mcheap_free(ptr);}
size_t  mcheap_defrag_largest_free(void) {return mcheap_largest_free();}
bool	mcheap_defrag_is_intact(void) {return mcheap_is_intact();}
void	mcheap_defrag_reinit(void) {return mcheap_reinit();}
