/*
*/
	#include <string.h>
	#include <stdint.h>
	#include <stdbool.h>
	#include <stddef.h>
	
//********************************************************************************************************
// Local defines
//********************************************************************************************************

	#ifndef MCHEAP_SIZE
		#define MCHEAP_SIZE 1000
		#warning "MCHEAP_SIZE not defined, using default size of 1000. Add -DMCHEAP_SIZE=<size in bytes> to compiler options."
	#endif

	#ifndef MCHEAP_ALIGNMENT
		#define MCHEAP_ALIGNMENT 	(sizeof(void*))
	#endif

	struct free_struct
	{
		size_t				size;		// size of empty content[] following this structure &content[size] will address the next used_struct/free_struct
		struct free_struct*	next_ptr;	// next free
		// addresses memory after the structure & aligns the size of the structure
		uint8_t		content[0] __attribute__((aligned(MCHEAP_ALIGNMENT)));
	};

	struct used_struct
	{
		size_t		size;				// size of content[] following this structure &content[size] will address the next used_struct/free_struct
		// addresses memory after the structure & aligns the size of the structure
		uint8_t		content[0] __attribute__((aligned(MCHEAP_ALIGNMENT)));
	};

//	evaluate the total size of a used or free section (including it's meta data) pointed to by arg1
//	arg1 must have correct type, used_struct* or free_struct*, not void*
	#define SECTION_SIZE(arg1)	(sizeof(*(arg1))+(arg1)->size)

//	address the next section, or, the first byte past heap space if there is no next section
//	arg1 must have correct type (not void*)
	#define SECTION_AFTER(arg1)	((void*)(&(arg1)->content[(arg1)->size]))

	#define END_OF_HEAP (&heap_space[MCHEAP_SIZE])

//	pointer casts
	#define USEDCAST(arg1)	((struct used_struct*)(arg1))
	#define FREECAST(arg1)	((struct free_struct*)(arg1))

//	used to access a structure instance by one of it's members
//	used to get the start of a section from it's .content[] member
	#define container_of(ptr, type, member)				\
	({													\
		void *__mptr = (void *)(ptr);					\
		((type *)(__mptr - offsetof(type, member)));	\
	})

	#define SMALLEST_OF(x,y) ((x)<(y) ? (x):(y))

//********************************************************************************************************
// Public variables
//********************************************************************************************************

//********************************************************************************************************
// Private variables
//********************************************************************************************************

	#ifdef MCHEAP_ADDRESS
		static uint8_t* heap_space = (uint8_t*)MCHEAP_ADDRESS;
	#else
		static uint8_t	heap_space[MCHEAP_SIZE] __attribute__((aligned(MCHEAP_ALIGNMENT)));
	#endif

	static bool	initialized = false;

	static struct free_struct* 	first_free;

//********************************************************************************************************
// Private prototypes
//********************************************************************************************************

	static void initialize(void);

// 	Internal allocate/reallocate/free functions 
	static void* allocate(size_t size);
	static void* reallocate(void* section, size_t new_size);
	static void* internal_free(void* section);

// relocate of realloc
// dest_ptr must be a suitable free section capable of allocating new_size bytes.
// removes dest_ptr from the free list, moves src_ptr to dest_ptr, and adds src_ptr to the free list
// preserves at most new_size bytes
// returns the new used section at dest_ptr
	static struct used_struct* relocate(struct free_struct* dest_ptr, struct used_struct* src_ptr, size_t new_size);

// 	Return true if section is in the free list
	static bool in_free_list(struct free_struct *x);

// 	Shrink used section so that it's content is reduced to the new_size.
// 	This will only happen if doing so allows a new free section to be created.
// 	new_size should be pre-aligned by the caller
// 	If created, the new free section will be inserted into the free list, and merged if possible
	static void used_shrink(struct used_struct *used_ptr, size_t new_size);

// 	Convert a used section to a free section, does not insert into the free list
// 	Returns the result
	static struct free_struct* used_to_free(struct used_struct *used_ptr);

// 	Convert a free section into a used section, free section must be removed from the free list beforehand
// 	Returns the result
	static struct used_struct* free_to_used(struct free_struct *free_ptr);

// 	Extend a used section into a lower free section, also moves content limited to 'preserve_size' bytes
// 	Free section must be removed from the free list before calling this function
// 	Returns the resulting used section
	static struct used_struct* used_extend_down(struct free_struct *free_ptr, struct used_struct *used_ptr, size_t preserve_size);

// 	Extend a used section into a higher free section
// 	The higher free section must be removed from the free list before calling this function
	static struct used_struct* used_extend_up(struct used_struct *used_ptr);

// 	Find free below
// 	Find the last free section before target section (either type), if there is one
// 	Otherwise return NULL
	static struct free_struct* find_free_below(void* target);

// 	Walk the free list for allocation (or re-allocation)
// 	Find a free section capable of holding 'size' bytes as a used section
	static struct free_struct* free_walk(size_t size);

// 	Insert a free section into the free list
// 	Walks the free list to find the insertion point
	static void free_insert(struct free_struct *new_free);

// 	Remove a free section from the free list
// 	Walks the free list to find the link to modify
	static void free_remove(struct free_struct *free_ptr);

// 	Merge free section with adjacent free sections
// 	All free sections must already be in the free list
	static void free_merge(struct free_struct *free_ptr);

// 	Merge free section into the next free section if possible
// 	merge does not destroy id_ info for either section, but overwrites second sections key with KEY_MERGED
	static void free_merge_up(struct free_struct *free_ptr);

// 	Find largest free block. Used for tracking heap headroom.
	static size_t free_find_largest(void);

// 	Heap test, return true if the heap is intact.
	static bool heap_test(void);

//	Round up size to a multiple of MCHEAP_ALIGNMENT
	static size_t align_size(size_t sz);

// Ensure that size is aligned, AND that the used section will be large enough to return to the free list
	static size_t enforce_minimum_allocation_size(size_t sz);

// 	Return true, if the used section can extend down into the free section to acheive the desired size
	static bool used_section_can_extend_down(struct free_struct* free_ptr, struct used_struct* used_ptr, size_t desired_size);

// Return true, if the used section can extend up into a free section to acheive the desired size
	static bool used_section_can_extend_up(struct used_struct* used_ptr, size_t desired_size);

//********************************************************************************************************
// Public functions
//********************************************************************************************************

void* mcheap_allocate(size_t size)
{
	return allocate(size);
}

void* mcheap_reallocate(void* section, size_t new_size)
{
	return reallocate(section, new_size);
}

void* mcheap_free(void* section)
{
	return internal_free(section);
}

size_t mcheap_largest_free(void)
{
	return free_find_largest();
}

bool mcheap_is_intact(void)
{
	return heap_test();
}

void mcheap_reinit(void)
{
	initialize();
}

//********************************************************************************************************
// Private functions
//********************************************************************************************************

static void initialize(void)
{
	initialized = true;
	first_free = (void*)heap_space;		//init head of the free list
	first_free->size = MCHEAP_SIZE - sizeof(struct free_struct);
	first_free->next_ptr = NULL;
}

static void* allocate(size_t size)
{
	struct free_struct *free_ptr;
	struct used_struct *used_ptr;
	void* retval=NULL;

	if(!initialized)
		initialize();

	size = enforce_minimum_allocation_size(size);

	free_ptr = free_walk(size);
	if(free_ptr)
	{
		free_remove(free_ptr);				//remove from the free list
		used_ptr = free_to_used(free_ptr);	//convert to used section
		used_shrink(used_ptr, size);		//shrink to required size
		retval = used_ptr->content;
	};

	return retval;
}

static void* reallocate(void* section, size_t new_size)
{
	struct free_struct* free_ptr;
	struct free_struct* relocation_ptr;
	struct used_struct* used_ptr;
	struct used_struct* new_used_ptr = NULL;
	void* retval = NULL;

	if(!initialized)
		initialize();

	if(section == NULL)
		retval = allocate(new_size);					//if section == NULL just call allocate()
	else if(new_size == 0)
		retval = internal_free(section);
	else
	{
		new_size = enforce_minimum_allocation_size(new_size);
		used_ptr = container_of(section, struct used_struct, content);

		// find space for new allocation
		relocation_ptr = free_walk(new_size);

		// relocate to a lower address? (1st preference to minimize fragmentation)
		if(relocation_ptr && (void*)relocation_ptr < (void*)used_ptr)
			new_used_ptr = relocate(relocation_ptr, used_ptr, new_size);

		else
		{
			free_ptr = find_free_below(used_ptr); 
			if(used_section_can_extend_down(free_ptr, used_ptr, new_size)) // 2nd preference
			{
				free_remove(free_ptr);
				new_used_ptr = used_extend_down(free_ptr, used_ptr, new_size);
			}
			else if(new_size <= used_ptr->size)	//shrink in place? 3rd preference
				new_used_ptr = used_ptr;
			else if(used_section_can_extend_up(used_ptr, new_size))	//4th preference
			{
				free_remove(SECTION_AFTER(used_ptr));
				new_used_ptr = used_extend_up(used_ptr);
			}
			else if(relocation_ptr)
				new_used_ptr = relocate(relocation_ptr, used_ptr, new_size);	// 5th preference, relocate to higher address
		};

		// Shrink the new used section if possible
		if(new_used_ptr)
		{
			used_shrink(new_used_ptr, new_size);
			retval = new_used_ptr->content;
		};
	};
	return retval;
}

// relocate of realloc
// dest_ptr must be a suitable free section capable of allocating new_size bytes.
// removes dest_ptr from the free list, moves src_ptr to dest_ptr, and adds src_ptr to the free list
// preserves at most new_size bytes
// returns the new used section at dest_ptr, does not shrink the destination.
static struct used_struct* relocate(struct free_struct* dest_ptr, struct used_struct* src_ptr, size_t new_size)
{
	struct used_struct* new_used_ptr;
	struct free_struct* new_free_ptr;
	free_remove(dest_ptr);
	new_used_ptr = free_to_used(dest_ptr);
	memcpy(new_used_ptr->content, src_ptr->content, SMALLEST_OF(new_size, src_ptr->size));
	new_free_ptr = used_to_free(src_ptr);
	free_insert(new_free_ptr);	// insert it into the free list
	free_merge(new_free_ptr);	// and merge with adjacent free sections
	return new_used_ptr;
}

static void* internal_free(void* section)
{
	struct used_struct *used_ptr;
	struct free_struct *free_ptr;

	if(!initialized)
		initialize();

	if(section != NULL)
	{
		used_ptr = container_of(section, struct used_struct, content);
			
		free_ptr = used_to_free(used_ptr);	//convert to free section
		free_insert(free_ptr);				//insert into the free list
		free_merge(free_ptr);				//merge with adjacent free sections
	};
	return NULL;
}

// Shrink used section so that it's content is reduced to the new_size.
// This will only happen if doing so allows a new free section to be created.
// new_size should be pre-aligned by the caller
// If created, the new free section will be inserted into the free list, and merged if possible
static void used_shrink(struct used_struct *used_ptr, size_t new_size)
{
	struct free_struct *free_ptr;

	if(new_size < used_ptr->size)
	{
		// If this section is large enough for used meta + new_size + free meta
		if(SECTION_SIZE(used_ptr) >= sizeof(struct used_struct) + new_size + sizeof(struct free_struct))
		{
			//remaining free section will start at the end of the shrunken used section
			free_ptr = (void*)&(used_ptr->content[new_size]);

			//construct remaining free section
			free_ptr->size = used_ptr->size - new_size - sizeof(struct free_struct);

			//shrink used section
			used_ptr->size = new_size;

			free_insert(free_ptr);
			free_merge_up(free_ptr);
		};
	};
}

// Convert a used section to a free section, does not insert into the free list
// Returns the result
static struct free_struct* used_to_free(struct used_struct *used_ptr)
{
	struct free_struct *free_ptr;

//	Build new free section
	free_ptr = (void*)used_ptr;
	free_ptr->size = SECTION_SIZE(used_ptr) - sizeof(struct free_struct);

	return free_ptr;
}

// Convert a free section into a used section, free section must be removed from the free list beforehand
// Returns the result
static struct used_struct* free_to_used(struct free_struct *free_ptr)
{
	struct used_struct *used_ptr;

//	Build new used section
	used_ptr = (void*)free_ptr;
	used_ptr->size = SECTION_SIZE(free_ptr) - sizeof(struct used_struct);
	return used_ptr;
}

// Return true, if the used section can extend down into the free section to acheive the desired size
static bool used_section_can_extend_down(struct free_struct* free_ptr, struct used_struct* used_ptr, size_t desired_size)
{
	return (free_ptr
		&& (SECTION_AFTER(free_ptr) == used_ptr)
		&& (used_ptr->size + SECTION_SIZE(free_ptr) >= desired_size));
}

// Return true, if the used section can extend up into a free section to acheive the desired size
static bool used_section_can_extend_up(struct used_struct* used_ptr, size_t desired_size)
{
	struct free_struct* free_ptr = SECTION_AFTER(used_ptr);

	return (in_free_list(free_ptr)
		&& (used_ptr->size + SECTION_SIZE(free_ptr) >= desired_size) );
}

// Extend a used section into a lower free section, also moves content limited to 'preserve_size' bytes
// Free section must be removed from the free list before calling this function
// Returns the resulting used section
static struct used_struct* used_extend_down(struct free_struct *free_ptr, struct used_struct *used_ptr, size_t preserve_size)
{
	size_t extra_size;
	size_t move_size;

//	extra size
	extra_size = SECTION_SIZE(free_ptr);

	if( preserve_size + sizeof(struct used_struct) < SECTION_SIZE(used_ptr) )
		move_size = preserve_size + sizeof(struct used_struct);
	else
		move_size = SECTION_SIZE(used_ptr);

//	move used section down, including limited content
	memmove(free_ptr, used_ptr, move_size);
	used_ptr = (void*)free_ptr;

//	extend used section
	used_ptr->size += extra_size;

	return used_ptr;
}

// Extend a used section into a higher free section
// The higher free section must be removed from the free list before calling this function
static struct used_struct* used_extend_up(struct used_struct *used_ptr)
{
	struct free_struct *free_ptr;
	size_t ext_size;

	free_ptr = SECTION_AFTER(used_ptr);
	ext_size = SECTION_SIZE(free_ptr);

	used_ptr->size += ext_size;

	return used_ptr;
}

// Find free below
// Find the last free section before target section (either type), if there is one
// Otherwise return NULL
static struct free_struct* find_free_below(void* target)
{
	struct free_struct *free_ptr;
	struct free_struct *retval=NULL;

	free_ptr = first_free;
	while(free_ptr && ((void*)free_ptr < target))
	{
		retval = free_ptr;
		free_ptr = free_ptr->next_ptr;
	};

	return retval;	
}

// Walk the free list for allocation (or re-allocation)
// Find a free section capable of holding 'size' bytes as a used section
static struct free_struct* free_walk(size_t size)
{
	struct free_struct *free_ptr;

	free_ptr = first_free;	
	while(free_ptr && SECTION_SIZE(free_ptr) < sizeof(struct used_struct)+size)
		free_ptr = free_ptr->next_ptr;

	return free_ptr;
}

// Return true if section is in the free list
static bool in_free_list(struct free_struct *section)
{
	struct 	free_struct *free_ptr;
	bool retval = false;

	free_ptr = first_free;
	while(free_ptr && !retval)
	{
		retval = (free_ptr == section);
		free_ptr = free_ptr->next_ptr;
	};
	return retval;
}

// Insert a free section into the free list
// Walks the free list to find the insertion point
static void free_insert(struct free_struct *new_free)
{
	struct free_struct **link_ptr;

	link_ptr = &first_free;

	//walk the links, until we find a link which points past the new_free section, or we find the end of the list
	while(*link_ptr && *link_ptr < new_free)
		link_ptr = &(*link_ptr)->next_ptr;	//link_ptr == the address of the next link

	//the new link points to what the previous link pointed to
	new_free->next_ptr = (*link_ptr);

	//the previous link points to the new free section
	(*link_ptr) = new_free;
}

// Remove a free section from the free list
// Walks the free list to find the link to modify
static void free_remove(struct free_struct *free_ptr)
{
	struct free_struct **link_ptr;
	link_ptr = &first_free;

	// Find the link that points to this section
	while(*link_ptr != free_ptr)
		link_ptr = &(*link_ptr)->next_ptr;	//link_ptr == the address of the next link

	// Remove it
	(*link_ptr) = free_ptr->next_ptr;
}

// Merge free section with adjacent free sections
// All free sections must already be in the free list
static void free_merge(struct free_struct *free_ptr)
{
	struct free_struct *below;

	free_merge_up(free_ptr);
	below = find_free_below(free_ptr);
	if(below)
		free_merge_up(below);
}

// Merge free section into the next free section if possible
static void free_merge_up(struct free_struct *free_ptr)
{
	//if there is a free section after this one
	if(free_ptr->next_ptr)
	{
		//if the next free section is at the end of this free section
		if(free_ptr->next_ptr == SECTION_AFTER(free_ptr))
		{
			//increase size of this free section, by total size of next section
			free_ptr->size += SECTION_SIZE(free_ptr->next_ptr);

			//copy next free sections link to this section
			free_ptr->next_ptr = free_ptr->next_ptr->next_ptr;
		};
	};
}

// Find largest free block. Used for tracking heap headroom.
static size_t free_find_largest(void)
{
	struct free_struct *free_ptr;
	size_t largest=0;
	if(first_free)
	{
		free_ptr = first_free;
		while(free_ptr)
		{
			if(free_ptr->size > largest)
				largest = free_ptr->size;
			free_ptr = free_ptr->next_ptr;
		};

	//	convert to allocatable content size
		largest += sizeof(struct free_struct);
		if(largest >= sizeof(struct used_struct))
			largest -= sizeof(struct used_struct);
	};

	return largest;
}

// Heap test, may be used before freeing memory, to see if the heap is intact,
static bool heap_test(void)	
{
	struct free_struct *next_free_ptr;
	void* section_ptr;
	bool intact = true;

	next_free_ptr = first_free;
	section_ptr = heap_space;

	while(intact && section_ptr != END_OF_HEAP)
	{
		if(section_ptr == (void*)next_free_ptr)
		{
			next_free_ptr = FREECAST(section_ptr)->next_ptr;
			section_ptr += SECTION_SIZE(FREECAST(section_ptr));
		}
		else
			section_ptr += SECTION_SIZE(USEDCAST(section_ptr));

		if((uint8_t*)section_ptr < heap_space || (uint8_t*)section_ptr > END_OF_HEAP)
			intact = false;
	};
	return intact;
}

// Ensure that size is aligned, AND that the used section will be large enough to return to the free list
static size_t enforce_minimum_allocation_size(size_t sz)
{
	sz = align_size(sz);

	if(sizeof(struct used_struct) + sz < sizeof(struct free_struct))
		sz = sizeof(struct free_struct) - sizeof(struct used_struct);

	return sz;
}

static size_t align_size(size_t sz)
{
	if(sz % MCHEAP_ALIGNMENT)
		sz += MCHEAP_ALIGNMENT - (sz % MCHEAP_ALIGNMENT);
	return sz;
}
