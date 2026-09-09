
// Wrappers for access to this sandboxed implementation of mcheap built with MCHEAP_REALLOC_POLICY_EVICT
void*	mcheap_evict_allocate(size_t size) {return mcheap_allocate(size);}
void*	mcheap_evict_reallocate(void* ptr, size_t size) {return mcheap_reallocate(ptr, size);}
void*	mcheap_evict_free(void* ptr) {return mcheap_free(ptr);}
size_t  mcheap_evict_largest_free(void) {return mcheap_largest_free();}
bool	mcheap_evict_is_intact(void) {return mcheap_is_intact();}
void	mcheap_evict_reinit(void) {return mcheap_reinit();}

#define MCHEAP_SANDBOX
#define MCHEAP_REALLOC_POLICY_EVICT
#define MCHEAP_IMPLEMENTATION
#include "../mcheap.h"
