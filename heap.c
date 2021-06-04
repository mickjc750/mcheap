/*
 	Dynamic memory allocator.
	Fully tested.

	Not intended to be high performance, low overhead, or have a low memory footprint.
	It is intended to be a safe alternative to malloc() and free(),
	 by vigilant run time testing of the heap space, and catching errors with the ERROR_x macros.

	There are performance improvements that could be made at the cost of 
*/

	#include "includes.h"

//********************************************************************************************************
// Configurable defines
//********************************************************************************************************

//	in bytes
	#define ALIGNMENT 	(sizeof(void*))

//	If defined, each call to heap_allocate() or heap_free() will
//	track the largest free section available, and the heap headroom
	#define TRACK_STATS

//	Test entire heap before allocating or freeing (walks all sections)
//	Each free operation will test that the given address is actually a used section
//	To test the heap without allocating or freeing call heap_free(NULL), or reallocate an existing allocation.
	#define TEST_HEAP

//	If defined, insert key values at the start of each section (both free and allocated) for integrity checking.
//	Key value will be .size ^ KEY_USED/KEY_FREE
//	The keys will only be tested if TEST_HEAP is defined
	#define USE_KEYS

//	Heap size
	#define	HEAP_SIZE	1000

/*
	If HEAP_ID_SECTIONS is defined, heap_id_file and heap_id_line (caller ID) will be available in all the below macros
	and can be passed to an assert handler to identify the caller.

	Define these to be ((void)0) if not used

	executed if call to heap_allocate fails
	ERROR_ALLOCATION_FAIL()

	executed if heap re-allocate fails
	ERROR_REALLOC_FAIL()

	executed if heap_free() is called with an address outside the heap
	ERROR_FREE_STATIC()

	executed if heap_reallocate() is called with an address outside the heap
	ERROR_REALLOC_STATIC()	

	executed if heap_free() is called with an address inside the heap, but not an allocation (requires TEST_HEAP)
	ERROR_FALSE_FREE()		
	
	executed if heap is broken (requires TEST_HEAP)
	ERROR_BROKEN()
*/

	#ifdef PLATFORM_AVR
		#define SLMEM(arg)	PSTR(arg)
	#else
		#define SLMEM(arg)	(arg)
	#endif

	#ifdef HEAP_ID_SECTIONS
		#define ERROR_ALLOCATION_FAIL()	assert_handle(heap_id_file, SLMEM("heap-1"), heap_id_line)
		#define ERROR_REALLOC_FAIL()	assert_handle(heap_id_file, SLMEM("heap-2"), heap_id_line)
		#define ERROR_FREE_STATIC()		assert_handle(heap_id_file, SLMEM("heap-3"), heap_id_line)
		#define ERROR_REALLOC_STATIC()	assert_handle(heap_id_file, SLMEM("heap-4"), heap_id_line)
		#define ERROR_FALSE_FREE()		assert_handle(heap_id_file, SLMEM("heap-5"), heap_id_line)
		#define ERROR_BROKEN()			assert_handle(heap_id_file, SLMEM("heap-6"), heap_id_line)
	#else
		#define ERROR_ALLOCATION_FAIL()	ASSERT(false)
		#define ERROR_REALLOC_FAIL()	ASSERT(false)
		#define ERROR_FREE_STATIC()		ASSERT(false)
		#define ERROR_REALLOC_STATIC()	ASSERT(false)
		#define ERROR_FALSE_FREE()		ASSERT(false)
		#define ERROR_BROKEN()			ASSERT(false)
	#endif

//********************************************************************************************************
// Local defines
//********************************************************************************************************

	#ifdef USE_KEYS
	//	Misc values for testing heap integrity
		#define	KEY_USED	((size_t)0x47B3D19C)
		#define KEY_FREE	((size_t)0x8BA1963F)
		#define KEY_MERGED	((size_t)0x19751975)
	//	(key merged overwrites a free sections key, when it is merged with the previous free section)
	//	(it is never tested, but may be of some interest when observed in a memory dump)
	#endif

	// meta data used for free and allocated sections
	// these can be extended with extra info if desired
	// the size & key members must be at the same offset in both
	// the content member must have the same name (content) in both
	struct free_struct
	{
	#ifdef USE_KEYS
		size_t				key;		// key ^ size == KEY_FREE
	#endif
		size_t				size;		// size of empty content[] following this structure &content[size] will address the next used_struct/free_struct
	#ifdef HEAP_ID_SECTIONS
		uint8_t				id_file;	// file that freed this allocation
		uint16_t			id_line;	// line number of file that freed this allocation
	#endif
		struct free_struct*	next_ptr;	// next free
		//(insert extras here if desired)
		// addresses memory after the structure & aligns the size of the structure
		uint8_t				content[0] __attribute__((aligned(ALIGNMENT)));
	};

	struct used_struct
	{
	#ifdef USE_KEYS
		size_t		key;				// key ^ size == KEY_USED
	#endif
		size_t		size;				// size of content[] following this structure &content[size] will address the next used_struct/free_struct
	#ifdef HEAP_ID_SECTIONS
		uint8_t		id_file;			// file that made this allocation
		uint16_t	id_line;			// line number of file that made this allocation
	#endif
		//(insert extras here if desired)
		// addresses memory after the structure & aligns the size of the structure
		uint8_t		content[0] __attribute__((aligned(ALIGNMENT)));
	};

//	Info used for searching the heap, contains a section pointer (to either type), and the next known free section
	struct search_point_struct
	{
		void* section_ptr;
		struct free_struct *next_free_ptr;	//next free section after (or at) section_ptr, or NULL if none
	};

//	evaluate the total size of a used or free section (including it's meta data) pointed to by arg1
//	arg1 must have correct type, used_struct* or free_struct*, not void*
	#define SECTION_SIZE(arg1)	(sizeof(*(arg1))+(arg1)->size)

//	address the next section, or, the first byte past heap space if there is no next section
//	arg1 must have correct type (not void*)
	#define SECTION_AFTER(arg1)	((void*)(&(arg1)->content[(arg1)->size]))

	#define END_OF_HEAP (&heap_space[HEAP_SIZE])

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

//********************************************************************************************************
// Public variables
//********************************************************************************************************

	//the minimum free space which has occurred since heap_init() (requires TRACK_STATS)
	size_t		heap_head_room=0;	

	//the current largest free section (requires TRACK_STATS)
	size_t 		heap_largest_free=0;

	//the current number of allocations
	uint32_t	heap_allocations=0;

	//the maximum number of allocations that has occurred
	uint32_t	heap_allocations_max=0;

//********************************************************************************************************
// Private variables
//********************************************************************************************************

	static uint8_t				heap_space[HEAP_SIZE] __attribute__((aligned(ALIGNMENT)));

	static bool					initialized = false;

	static struct free_struct* 	first_free = (void*)heap_space;	//head of the free list

	#ifdef HEAP_ID_SECTIONS
		static uint8_t			heap_id_file;
		static uint16_t			heap_id_line;
	#endif

//********************************************************************************************************
// Private prototypes
//********************************************************************************************************

//
// Return true if section is in the free list
//
static bool in_free_list(struct free_struct *x);

//
// Shrink used section so that it's content is reduced to the new_size.
// This will only happen if doing so allows a new free section to be created.
// new_size should be pre-aligned by the caller
// If created, the new free section will be inserted into the free list, and merged if possible
//
static void used_shrink(struct used_struct *used_ptr, size_t new_size);

//
// Convert a used section to a free section, does not insert into the free list
// Returns the result
// 
static struct free_struct* used_to_free(struct used_struct *used_ptr);

//
// Convert a free section into a used section, free section must be removed from the free list beforehand
// Returns the result
// 
static struct used_struct* free_to_used(struct free_struct *free_ptr);

//
// Extend a used section into a lower free section, also moves content limited to 'preserve_size' bytes
// Free section must be removed from the free list before calling this function
// Returns the resulting used section
//
static struct used_struct* used_extend_down(struct free_struct *free_ptr, struct used_struct *used_ptr, size_t preserve_size);

//
// Extend a used section into a higher free section
// The higher free section must be removed from the free list before calling this function
//
static struct used_struct* used_extend_up(struct used_struct *used_ptr);

//
// Find free below
// Find the last free section before target section (either type), if there is one
// Otherwise return NULL
//
static struct free_struct* find_free_below(void* target);

// Walk the free list for allocation (or re-allocation)
// Find a free section capable of holding 'size' bytes as a used section
static struct free_struct* free_walk(size_t size);

//
// Insert a free section into the free list
// Walks the free list to find the insertion point
//
static void free_insert(struct free_struct *new_free);

//
// Remove a free section from the free list
// Walks the free list to find the link to modify
//
static void free_remove(struct free_struct *free_ptr);

//
// Merge free section with adjacent free sections
// All free sections must already be in the free list
//
static void free_merge(struct free_struct *free_ptr);

//
// Merge free section into the next free section if possible
// merge does not destroy id_ info for either section, but overwrites second sections key with KEY_MERGED
//
static void free_merge_up(struct free_struct *free_ptr);

//
// From any section, find the next used section, or the end of the heap.
// The next free section from the starting point must be known.
//
static struct search_point_struct find_next_used(struct search_point_struct start);

//
// Find largest free block. Used for tracking heap headroom.
//
#ifdef TRACK_STATS
	static void free_find_largest(void);
#endif

//
// Heap test, may be used before freeing memory, to see if the heap is intact,
// and also that the section about to be freed is actually a used section
//
#ifdef TEST_HEAP
	static void test_heap(struct used_struct *used_ptr);
#endif


//********************************************************************************************************
// Public functions
//********************************************************************************************************

void heap_init(void)
{
	initialized=true;
	static struct free_struct* free_ptr = (void*)heap_space;

//	initialize free space
	free_ptr->size 	= HEAP_SIZE - sizeof(struct free_struct);
	#ifdef USE_KEYS
		free_ptr->key 	= free_ptr->size ^ KEY_FREE;
	#endif

	free_ptr->next_ptr 	= NULL;

	heap_head_room		= free_ptr->size;
	heap_largest_free	= free_ptr->size;
}

#ifdef HEAP_ID_SECTIONS
void* heap_allocate_id(size_t size, uint8_t id_file, uint16_t id_line)
#else
void* heap_allocate(size_t size)
#endif
{
	struct free_struct *free_ptr;
	struct used_struct *used_ptr;
	void* retval=NULL;

	#ifdef HEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	if(!initialized)
		heap_init();

//	align size
	if(size & (ALIGNMENT-1))
		size += ALIGNMENT;
	size &= ~(size_t)(ALIGNMENT-1);

//	allocation must be large enough to return to the free list
	if(sizeof(struct used_struct) + size < sizeof(struct free_struct))
		size = sizeof(struct free_struct) - sizeof(struct used_struct);

	#ifdef TEST_HEAP
		test_heap(NULL);
	#endif

	free_ptr = free_walk(size);
	if(free_ptr)
	{
		free_remove(free_ptr);				//remove from the free list
		used_ptr = free_to_used(free_ptr);	//convert to used section
		used_shrink(used_ptr, size);		//shrink to required size
		retval = used_ptr->content;
	};

	if(!retval)
		ERROR_ALLOCATION_FAIL();
	else
	{
		heap_allocations++;
		if(heap_allocations > heap_allocations_max)
			heap_allocations_max = heap_allocations;
	};

	#ifdef TRACK_STATS
		free_find_largest();
	#endif

	return retval;
}

//
// 	Heap reallocate
//
#ifdef HEAP_ID_SECTIONS
void* heap_reallocate_id(void* section, size_t new_size, uint8_t id_file, uint16_t id_line)
#else
void* heap_reallocate(void* section, size_t new_size)
#endif
{
	struct free_struct* free_ptr;
	struct free_struct* dest_ptr=NULL;
	struct used_struct* used_ptr;
	struct used_struct* new_used_ptr=NULL;
	void* retval = NULL;

	#ifdef HEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	if(section == NULL)
	{
		#ifdef HEAP_ID_SECTIONS
			retval = heap_allocate_id(new_size, id_file, id_line);	//if section == NULL just call heap_allocate()
		#else
			retval = heap_allocate(new_size);					//if section == NULL just call heap_allocate()
		#endif
	}
	else
	{
		if(!heap_contains(section))
			ERROR_REALLOC_STATIC();

		// align size
		if(new_size & (ALIGNMENT-1))
			new_size += ALIGNMENT;
		new_size &= ~(size_t)(ALIGNMENT-1);

		// allocation must be large enough to return to the free list
		if(sizeof(struct used_struct) + new_size < sizeof(struct free_struct))
			new_size = sizeof(struct free_struct) - sizeof(struct used_struct);

		used_ptr = container_of(section, struct used_struct, content);

		#ifdef TEST_HEAP
		// fail if this is not a used section
			test_heap(used_ptr);
		#endif

		// find space for new allocation
		free_ptr = free_walk(new_size);

		// space below?
		if(free_ptr && (void*)free_ptr < (void*)used_ptr)
		{
			dest_ptr = free_ptr;	//destination below (optimal)
		}
		else
		{
			dest_ptr = free_ptr;	//destination above (or NULL)

			//Can we extend down?
			free_ptr = find_free_below(used_ptr);
			if(free_ptr)
			{
				if(SECTION_AFTER(free_ptr) == used_ptr)
				{
					//Yes, can we extend down enough?
					if(used_ptr->size + SECTION_SIZE(free_ptr) >= new_size)
					{
						free_remove(free_ptr);
						new_used_ptr = used_extend_down(free_ptr, used_ptr, new_size);
						dest_ptr = NULL;
					};
				};
			};
			
			//Can we extend up?
			if(!new_used_ptr && in_free_list(SECTION_AFTER(used_ptr)))
			{
				//Yes, can we extend up enough?
				free_ptr = SECTION_AFTER(used_ptr);
				if(used_ptr->size + SECTION_SIZE(free_ptr) >= new_size)
				{
					free_remove(free_ptr);
					new_used_ptr = used_extend_up(used_ptr);
					dest_ptr = NULL;
				};
			};
		};

		//full relocation needed? (we were unable to extend)
		if(dest_ptr)
		{
			//remove from free list
			free_remove(dest_ptr);

			//convert free section to used section
			new_used_ptr = free_to_used(dest_ptr);

			//copy content
			if(new_size < used_ptr->size)
				memcpy(new_used_ptr->content, used_ptr->content, new_size);
			else
				memcpy(new_used_ptr->content, used_ptr->content, used_ptr->size);

			//free the original used section
			free_ptr = used_to_free(used_ptr);

			free_insert(free_ptr);	// insert it into the free list
			free_merge(free_ptr);	// and merge with adjacent free sections
		};

		// Shrink the new used section if possible
		if(new_used_ptr)
		{
			used_shrink(new_used_ptr, new_size);
			retval = new_used_ptr->content;
		};

		if(!retval)
			ERROR_REALLOC_FAIL();

		#ifdef TRACK_STATS
			free_find_largest();
		#endif
	};
	return retval;
}

#ifdef HEAP_ID_SECTIONS
void* heap_free_id(void* section, uint8_t id_file, uint16_t id_line)
#else
void* heap_free(void* section)
#endif
{
	struct used_struct *used_ptr;
	struct free_struct *free_ptr;

	#ifdef HEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	if(section==NULL)
	{
		#ifdef TEST_HEAP
			test_heap(NULL);
		#endif
	}
	else
	{
		// not null, is it in the heap?
		if(!heap_contains(section))
			ERROR_FREE_STATIC();
		else
		{
			//in the heap
			used_ptr = container_of(section, struct used_struct, content);
			#ifdef TEST_HEAP
				//fail if this is not a used section
				test_heap(used_ptr);
			#endif
			
			free_ptr = used_to_free(used_ptr);	//convert to free section
			free_insert(free_ptr);				//insert into the free list
			free_merge(free_ptr);				//merge with adjacent free sections

			#ifdef TRACK_STATS
				free_find_largest();
			#endif
			heap_allocations--;
		};
	};
	return NULL;
}

//return true if pointer is within the heap
bool heap_contains(void* section)
{
	bool retval;
	
	retval = ( (heap_space < (uint8_t*)section) && ((uint8_t*)section < &heap_space[HEAP_SIZE]) );

	return retval;
}

#ifdef HEAP_ID_SECTIONS
struct heap_leakid_struct heap_find_leak(void)
{
	struct heap_leakid_struct record = {.file_id = 0, .line_id = 0, .cnt = 0};
	struct search_point_struct search_base;
	struct search_point_struct search_id;

	uint8_t fid;
	uint16_t lid;
	uint32_t cnt;
	bool found_next_id = true;

	search_base.section_ptr = heap_space;
	search_base.next_free_ptr = first_free;

	search_base = find_next_used(search_base);

	while(search_base.section_ptr != END_OF_HEAP && found_next_id)
	{
		fid = USEDCAST(search_base.section_ptr)->id_file;
		lid = USEDCAST(search_base.section_ptr)->id_line;
		search_id = search_base;
		found_next_id = false;
		cnt = 0;
		while(search_id.section_ptr != END_OF_HEAP)
		{
			if( (fid == USEDCAST(search_id.section_ptr)->id_file)
			&&  (lid == USEDCAST(search_id.section_ptr)->id_line) )
			{
				cnt++;
			}
			else if(!found_next_id)
			{
				search_base = search_id;
				found_next_id = true;
			};

			search_id = find_next_used(search_id);
		};

		if(cnt > record.cnt)
		{
			record.cnt = cnt;
			record.file_id = fid;
			record.line_id = lid;
		};
	};

	return record;
}
#endif


#ifdef HEAP_PROVIDE_PRNF
#ifdef HEAP_ID_SECTIONS
char* heap_prnf_id(uint8_t id_file, uint16_t id_line, const char* fmt, ...)
#else
char* heap_prnf(const char* fmt, ...)
#endif
{
	va_list va;
	va_start(va, fmt);
	int 	size;
	char* 	result;

	size = vsnprnf(NULL, 0, fmt, va)+1;

	#ifdef HEAP_ID_SECTIONS
		result = heap_allocate_id(size, id_file, id_line);
	#else
		result = heap_allocate(size);
	#endif

	if(result)
		vsprnf(result, fmt, va);

	va_end(va);
	return result;
};

#ifdef PLATFORM_AVR
#ifdef HEAP_ID_SECTIONS
char* heap_prnf_P_id(uint8_t id_file, uint16_t id_line, PGM_P fmt, ...)
#else
char* heap_prnf_P(PGM_P fmt, ...)
#endif
{
	va_list va;
	va_start(va, fmt);
	int 	size;
	char* 	result;

	size = vsnprnf_P(NULL, 0, fmt, va)+1;

	#ifdef HEAP_ID_SECTIONS
		result = heap_allocate_id(size, id_file, id_line);
	#else
		result = heap_allocate(size);
	#endif

	if(result)
		vsprnf_P(result, fmt, va);

	va_end(va);
	return result;
};
#endif
#endif

//********************************************************************************************************
// Private functions
//********************************************************************************************************

//
// Shrink used section so that it's content is reduced to the new_size.
// This will only happen if doing so allows a new free section to be created.
// new_size should be pre-aligned by the caller
// If created, the new free section will be inserted into the free list, and merged if possible
//
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
			#ifdef USE_KEYS
				free_ptr->key = free_ptr->size ^ KEY_FREE;
			#endif
			#ifdef HEAP_ID_SECTIONS
				free_ptr->id_file = FILE_ENUM;	//(internal ID)
				free_ptr->id_line = __LINE__;
			#endif

			//shrink used section
			used_ptr->size = new_size;

			//correct used sections key
			#ifdef USE_KEYS
				used_ptr->key 	= used_ptr->size ^ KEY_USED;
			#endif

			//insert new free section into the free list
			free_insert(free_ptr);

			//merge with the following free section if possible
			free_merge_up(free_ptr);
		};
	};
}

//
// Convert a used section to a free section, does not insert into the free list
// Returns the result
// 
static struct free_struct* used_to_free(struct used_struct *used_ptr)
{
	struct free_struct *free_ptr;

//	Build new free section
	free_ptr = (void*)used_ptr;
	free_ptr->size = SECTION_SIZE(used_ptr) - sizeof(struct free_struct);
	#ifdef USE_KEYS
		free_ptr->key = KEY_FREE ^ free_ptr->size;
	#endif
	#ifdef HEAP_ID_SECTIONS
		free_ptr->id_file = heap_id_file;
		free_ptr->id_line = heap_id_line;
	#endif

	return free_ptr;
}

//
// Convert a free section into a used section, free section must be removed from the free list beforehand
// Returns the result
// 
static struct used_struct* free_to_used(struct free_struct *free_ptr)
{
	struct used_struct *used_ptr;

//	Build new used section
	used_ptr = (void*)free_ptr;
	used_ptr->size = SECTION_SIZE(free_ptr) - sizeof(struct used_struct);
	#ifdef USE_KEYS
		used_ptr->key = KEY_USED ^ used_ptr->size;
	#endif
	#ifdef HEAP_ID_SECTIONS
		used_ptr->id_file = heap_id_file;
		used_ptr->id_line = heap_id_line;
	#endif
	return used_ptr;
}

//
// Extend a used section into a lower free section, also moves content limited to 'preserve_size' bytes
// Free section must be removed from the free list before calling this function
// Returns the resulting used section
//
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

//	correct used sections key
	#ifdef USE_KEYS
		used_ptr->key = used_ptr->size ^ KEY_USED;
	#endif

	return used_ptr;
}

//
// Extend a used section into a higher free section
// The higher free section must be removed from the free list before calling this function
//
static struct used_struct* used_extend_up(struct used_struct *used_ptr)
{
	struct free_struct *free_ptr;
	size_t ext_size;

	free_ptr = SECTION_AFTER(used_ptr);
	ext_size = SECTION_SIZE(free_ptr);

	used_ptr->size += ext_size;

	#ifdef USE_KEYS
		used_ptr->key = used_ptr->size ^ KEY_USED;
	#endif

	return used_ptr;
}

//
// Find free below
// Find the last free section before target section (either type), if there is one
// Otherwise return NULL
//
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

//
// Return true if section is in the free list
//
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

//
// Insert a free section into the free list
// Walks the free list to find the insertion point
//
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

//
// Remove a free section from the free list
// Walks the free list to find the link to modify
//
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

//
// Merge free section with adjacent free sections
// All free sections must already be in the free list
//
static void free_merge(struct free_struct *free_ptr)
{
	struct free_struct *below;

	free_merge_up(free_ptr);
	below = find_free_below(free_ptr);
	if(below)
		free_merge_up(below);
}

//
// Merge free section into the next free section if possible
// merge does not destroy id_ info for either section, but overwrites second sections key with KEY_MERGED
//
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

			//correct extended sections key, and destroy next free sections key
			#ifdef USE_KEYS
				free_ptr->key 	= free_ptr->size ^ KEY_FREE;
				free_ptr->next_ptr->key = KEY_MERGED;
			#endif

			//copy next free sections link to this section
			free_ptr->next_ptr = free_ptr->next_ptr->next_ptr;
		};
	};
}

//
// From any section, find the next used section, or the end of the heap.
// The next free section from the starting point must be known.
//
static struct search_point_struct find_next_used(struct search_point_struct start)
{
	struct search_point_struct search;

	search = start;

	//If starting on a free section
	if(search.section_ptr == (void*)(search.next_free_ptr))
	{
		//jump until used section or end of heap
		while(search.section_ptr == (void*)(search.next_free_ptr))
		{
			search.section_ptr = SECTION_AFTER(search.next_free_ptr);
			search.next_free_ptr = (search.next_free_ptr)->next_ptr;
		};
	}
	//If starting on a used section
	else
	{
		//jump once
		search.section_ptr = SECTION_AFTER(USEDCAST(search.section_ptr));

		//jump until used section or end of heap
		while(search.section_ptr == (void*)(search.next_free_ptr))
		{
			search.section_ptr = SECTION_AFTER(search.next_free_ptr);
			search.next_free_ptr = (search.next_free_ptr)->next_ptr;
		};
	};

	return search;
}

//
// Find largest free block. Used for tracking heap headroom.
//
#ifdef TRACK_STATS
static void free_find_largest(void)
{
	struct free_struct *free_ptr;
	size_t largest=0;

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
		heap_largest_free = largest - sizeof(struct used_struct);
	else
		heap_largest_free = 0;

	if(heap_largest_free < heap_head_room)
		heap_head_room = heap_largest_free;
}
#endif

//
// Heap test, may be used before freeing memory, to see if the heap is intact,
// and also that the section about to be freed is actually a used section
//
#ifdef TEST_HEAP
	static void test_heap(struct used_struct *used_ptr)	
	#ifdef USE_KEYS
{
	struct free_struct *next_free_ptr;
	void* section_ptr;
	bool used_found=false;

	next_free_ptr = first_free;
	section_ptr = heap_space;

	while(section_ptr != &heap_space[HEAP_SIZE])
	{
		//if this is a free section
		if(FREECAST(section_ptr)->key == (KEY_FREE ^ FREECAST(section_ptr)->size))
		{
			//if this does not equal the last free link, heap is broken
			if(section_ptr != (void*)next_free_ptr)
				ERROR_BROKEN();
			//next free section == this free link
			next_free_ptr = FREECAST(section_ptr)->next_ptr;
			//section_ptr = &(FREECAST(section_ptr)->content[FREECAST(section_ptr)->size]);
			section_ptr += SECTION_SIZE(FREECAST(section_ptr));
		}
		//if this is a used section
		else if(USEDCAST(section_ptr)->key == (KEY_USED ^ USEDCAST(section_ptr)->size))
		{
			//is this the used section we are looking for?
			if(section_ptr == (void*)used_ptr)
				used_found = true;
			section_ptr += SECTION_SIZE(USEDCAST(section_ptr));
		}
		else
			ERROR_BROKEN();
	};
	if(used_ptr && !used_found)
		ERROR_FALSE_FREE();
}
	#else
{
	struct free_struct *next_free_ptr;
	void* section_ptr;
	bool used_found=false;

	next_free_ptr = first_free;
	section_ptr = heap_space;

	while(section_ptr != &heap_space[HEAP_SIZE])
	{
		//free section?
		if(section_ptr == (void*)next_free_ptr)
		{
			//next free section == this free link
			next_free_ptr = FREECAST(section_ptr)->next_ptr;
			//section_ptr = &(FREECAST(section_ptr)->content[FREECAST(section_ptr)->size]);
			section_ptr += SECTION_SIZE(FREECAST(section_ptr));
		}
		//else this is a used section
		else
		{
			//is this the used section we are looking for?
			if(section_ptr == (void*)used_ptr)
				used_found = true;
			section_ptr += SECTION_SIZE(USEDCAST(section_ptr));
		};

		if((uint8_t*)section_ptr < heap_space || (uint8_t*)section_ptr > &heap_space[HEAP_SIZE])
			ERROR_BROKEN();
	};
	if(used_ptr && !used_found)
		ERROR_FALSE_FREE();
}
	#endif
#endif
