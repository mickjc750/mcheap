
// Wrappers for access to this sandboxed implementation of mcheap built with MCHEAP_REALLOC_POLICY_RESIZE
void*	mcheap_resize_allocate(size_t size) {return mcheap_allocate(size);}
void*	mcheap_resize_reallocate(void* ptr, size_t size) {return mcheap_reallocate(ptr, size);}
void*	mcheap_resize_free(void* ptr) {return mcheap_free(ptr);}
size_t  mcheap_resize_largest_free(void) {return mcheap_largest_free();}
bool	mcheap_resize_is_intact(void) {return mcheap_is_intact();}
void	mcheap_resize_reinit(void) {return mcheap_reinit();}

#define MCHEAP_SANDBOX
#define MCHEAP_REALLOC_POLICY_RESIZE
#define MCHEAP_IMPLEMENTATION
#include "../mcheap.h"
