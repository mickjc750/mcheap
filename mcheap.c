/*
*/
	#include <string.h>
	#include "mcheap.h"

	#ifdef MCHEAP_PROVIDE_PRNF
		#include "prnf.h"
	#endif

	#ifndef USE_MCHEAP
		#warning "mcheap.c is being compiled, but USE_MCHEAP is not defined. Add -DUSE_MCHEAP to compiler options, to indicate the availability of mcheap to other modules."
	#endif

//********************************************************************************************************
// Local defines
//********************************************************************************************************

	#ifndef MCHEAP_SIZE
		#define MCHEAP_SIZE 1000
		#warning "MCHEAP_SIZE not defined, using default size of 1000. Add -DMCHEAP_SIZE=<size in bytes> to compiler options."
	#endif

	#ifdef MCHEAP_USE_KEYS
	#ifndef MCHEAP_TEST
		#warning "MCHEAP_USE_KEYS should only be used in addition to MCHEAP_TEST. Add -DMCHEAP_TEST to compiler options, or remove MCHEAP_USE_KEYS."
	#endif
	#endif

	#ifndef MCHEAP_ALIGNMENT
		#define MCHEAP_ALIGNMENT 	(sizeof(void*))
	#endif

	#ifdef MCHEAP_USE_POSIX_MUTEX_LOCK
		#define APPIF_ENTER()	pthread_mutex_lock(&mutex);
		#define APPIF_EXIT()	pthread_mutex_unlock(&mutex);
	#else
		#define APPIF_ENTER()	(void)0
		#define APPIF_EXIT()	(void)0
	#endif

	#ifdef PLATFORM_AVR
		#define SLMEM(arg)	PSTR(arg)
	#else
		#define SLMEM(arg)	(arg)
	#endif

	#ifdef MCHEAP_NO_ASSERT
		#define ERROR_ALLOCATION_FAIL()		while(true)
		#define ERROR_REALLOC_FAIL()		while(true)
		#define ERROR_FREE_EXTERNAL()		while(true)
		#define ERROR_REALLOC_EXTERNAL()	while(true)
		#define ERROR_FALSE_FREE()			while(true)
		#define ERROR_BROKEN()				while(true)
	#else
		#ifdef USE_MCASSERT
			#include "mcassert.h"
			#ifdef MCHEAP_ID_SECTIONS
				#define ERROR_ALLOCATION_FAIL()		assert_handle(heap_id_file, heap_id_line, SLMEM("heap-fail-alloc"))
				#define ERROR_REALLOC_FAIL()		assert_handle(heap_id_file, heap_id_line, SLMEM("heap-fail-realloc"))
				#define ERROR_FREE_EXTERNAL()		assert_handle(heap_id_file, heap_id_line, SLMEM("heap-free-external"))
				#define ERROR_REALLOC_EXTERNAL()	assert_handle(heap_id_file, heap_id_line, SLMEM("heap-realloc-external"))
				#define ERROR_FALSE_FREE()			assert_handle(heap_id_file, heap_id_line, SLMEM("heap-false-free"))
				#define ERROR_FALSE_REALLOC()		assert_handle(heap_id_file, heap_id_line, SLMEM("heap-false-realloc"))
				#define ERROR_BROKEN()				assert_handle(heap_id_file, heap_id_line, SLMEM("heap-broken"))
				#define ERROR_NO_INIT()				assert_handle(heap_id_file, heap_id_line, SLMEM("heap-no-init"))
			#else
				#define ERROR_ALLOCATION_FAIL()		ASSERT_MSG_SL(false, "heap-fail-alloc")
				#define ERROR_REALLOC_FAIL()		ASSERT_MSG_SL(false, "heap-fail-realloc")
				#define ERROR_FREE_EXTERNAL()		ASSERT_MSG_SL(false, "heap-free-external")
				#define ERROR_REALLOC_EXTERNAL()	ASSERT_MSG_SL(false, "heap-realloc-external")
				#define ERROR_FALSE_FREE()			ASSERT_MSG_SL(false, "heap-false-free")
				#define ERROR_FALSE_REALLOC()		ASSERT_MSG_SL(false, "heap-false-realloc")
				#define ERROR_BROKEN()				ASSERT_MSG_SL(false, "heap-broken")
				#define ERROR_NO_INIT()				ASSERT_MSG_SL(false, "heap-no-init")
			#endif
		#else
			#include <assert.h>
			#define ERROR_ALLOCATION_FAIL()		assert(!"heap-fail-alloc")
			#define ERROR_REALLOC_FAIL()		assert(!"heap-fail-realloc")
			#define ERROR_FREE_EXTERNAL()		assert(!"heap-free-external")
			#define ERROR_REALLOC_EXTERNAL()	assert(!"heap-realloc-external")
			#define ERROR_FALSE_FREE()			assert(!"heap-false-free")
			#define ERROR_FALSE_REALLOC()		assert(!"heap-false-realloc")
			#define ERROR_BROKEN()				assert(!"heap-broken")
			#define ERROR_NO_INIT()				assert(!"heap-no-init")
		#endif
	#endif

	#ifdef MCHEAP_USE_KEYS
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
	#ifdef MCHEAP_USE_KEYS
		size_t				key;		// key ^ size == KEY_FREE
	#endif
		size_t				size;		// size of empty content[] following this structure &content[size] will address the next used_struct/free_struct
	#ifdef MCHEAP_ID_SECTIONS
		const char*			id_file;	// file that freed this allocation
		uint16_t			id_line;	// line number of file that freed this allocation
	#endif
		struct free_struct*	next_ptr;	// next free
		//(insert extras here if desired)
		// addresses memory after the structure & aligns the size of the structure
		uint8_t		content[0] __attribute__((aligned(MCHEAP_ALIGNMENT)));
	};

	struct used_struct
	{
	#ifdef MCHEAP_USE_KEYS
		size_t		key;				// key ^ size == KEY_USED
	#endif
		size_t		size;				// size of content[] following this structure &content[size] will address the next used_struct/free_struct
	#ifdef MCHEAP_ID_SECTIONS
		const char*	id_file;			// file that made this allocation
		uint16_t	id_line;			// line number of file that made this allocation
	#endif
		//(insert extras here if desired)
		// addresses memory after the structure & aligns the size of the structure
		uint8_t		content[0] __attribute__((aligned(MCHEAP_ALIGNMENT)));
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

//********************************************************************************************************
// Public variables
//********************************************************************************************************

#ifdef MCHEAP_TRACK_STATS
	//the minimum free space which has occurred since initialize() (requires MCHEAP_TRACK_STATS)
	size_t		heap_head_room = MCHEAP_SIZE - sizeof(struct used_struct);

	//the current largest allocation possible (requires MCHEAP_TRACK_STATS)
	size_t 		heap_largest_free = MCHEAP_SIZE - sizeof(struct used_struct);
#endif

	//the current number of allocations
	uint32_t	heap_allocations=0;

	//the maximum number of allocations that has occurred
	uint32_t	heap_allocations_max=0;

//********************************************************************************************************
// Private variables
//********************************************************************************************************

	#ifdef MCHEAP_ADDRESS
		static uint8_t* heap_space = (uint8_t*)MCHEAP_ADDRESS;
	#elif defined MCHEAP_RUNTIME_ADDRESS
		static uint8_t* heap_space = NULL;
	#else
		static uint8_t	heap_space[MCHEAP_SIZE] __attribute__((aligned(MCHEAP_ALIGNMENT)));
	#endif

	static bool					initialized = false;

	static struct free_struct* 	first_free;

	#ifdef MCHEAP_ID_SECTIONS
		static const char*		heap_id_file;
		static uint16_t			heap_id_line;
	#endif

	#ifdef MCHEAP_USE_POSIX_MUTEX_LOCK
		#include <pthread.h>
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	#endif

//********************************************************************************************************
// Private prototypes
//********************************************************************************************************

	static void initialize(void);

// 	Internal allocate/reallocate/free functions 
	static void* allocate(size_t size);
	static void* reallocate(void* section, size_t new_size);
	static void* internal_free(void* section);

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

// 	From any section, find the next used section, or the end of the heap.
// 	The next free section from the starting point must be known.
	static struct search_point_struct find_next_used(struct search_point_struct start);

// 	Find largest free block. Used for tracking heap headroom.
#ifdef MCHEAP_TRACK_STATS
	static void free_find_largest(void);
#endif

// 	Heap test, may be used before freeing memory, to see if the heap is intact,
// 	and also that the section about to be freed is actually a used section
#ifdef MCHEAP_TEST
	static bool heap_test(struct used_struct *used_ptr);
#endif


//********************************************************************************************************
// Public functions
//********************************************************************************************************

#ifdef MCHEAP_RUNTIME_ADDRESS
void heap_init(void* addr)
{
	APPIF_ENTER();
	if(!heap_space)
	{
		heap_space = addr;
		initialize();
	};
	APPIF_EXIT();
}
#endif

#ifdef MCHEAP_PROVIDE_STDLIB_FUNCTIONS
void* malloc(size_t size)
{
	void* retval;
	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = SLMEM("malloc");
		heap_id_line = 0;
	#endif

	#ifdef MCHEAP_TRACK_STATS
	if(size >= heap_largest_free)	//mimic malloc's null return if heap_largest_free is tracked
		retval = NULL;
	else
	#endif
		retval = allocate(size);

	APPIF_EXIT();
	return retval;
}

void* calloc(size_t n, size_t size)
{
	void* retval;

	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = SLMEM("calloc");
		heap_id_line = 0;
	#endif
	size *= n;

	#ifdef MCHEAP_TRACK_STATS
	if(size >= heap_largest_free)	//mimic malloc's null return if heap_largest_free is tracked
		retval = NULL;
	else
	#endif
		retval = allocate(size);

	if(retval)
		memset(retval, 0, size);

	APPIF_EXIT();
	return retval;
}

void *realloc(void *ptr, size_t size)
{
	void* retval;

	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = SLMEM("realloc");
		heap_id_line = 0;
	#endif

	#ifdef MCHEAP_TRACK_STATS
	if(size >= heap_largest_free)	//offer a null return if realloc *may* not be possible
		retval = NULL;
	else
	#endif
		retval = reallocate(ptr, size);

	APPIF_EXIT();
	return retval;
}

void free(void* ptr)
{
	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = SLMEM("free");
		heap_id_line = 0;
	#endif
	internal_free(ptr);
	APPIF_EXIT();
}
#endif


#ifdef MCHEAP_ID_SECTIONS
void* heap_allocate_id(size_t size, const char* id_file, uint16_t id_line)
#else
void* heap_allocate(size_t size)
#endif
{
	void* retval;
	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	retval = allocate(size);

	APPIF_EXIT();
	return retval;
}

#ifdef MCHEAP_ID_SECTIONS
void* heap_reallocate_id(void* section, size_t new_size, const char* id_file, uint16_t id_line)
#else
void* heap_reallocate(void* section, size_t new_size)
#endif
{
	void* retval;
	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	retval = reallocate(section, new_size);
	APPIF_EXIT();

	return retval;
}


#ifdef MCHEAP_ID_SECTIONS
void* heap_free_id(void* section, const char* id_file, uint16_t id_line)
#else
void* heap_free(void* section)
#endif
{
	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	internal_free(section);
	APPIF_EXIT();

	return NULL;
}

//return true if pointer is within the heap
bool heap_contains(void* section)
{
	bool retval;
	
	APPIF_ENTER();
	if(!initialized)
		initialize();

	retval = ( (heap_space < (uint8_t*)section) && ((uint8_t*)section < &heap_space[MCHEAP_SIZE]) );
	APPIF_EXIT();
	return retval;
}

#ifdef MCHEAP_ID_SECTIONS
struct heap_list_struct heap_list(unsigned int n)
{
	struct heap_list_struct retval = {.file_id = NULL, .line_id = 0, .size = 0, .content = NULL,};
	struct search_point_struct search_base;

	APPIF_ENTER();
	if(!initialized)
		initialize();

	search_base.section_ptr = heap_space;
	search_base.next_free_ptr = first_free;
	if(heap_space == (uint8_t*)first_free)
		search_base = find_next_used(search_base);

	while(n-- && search_base.section_ptr != END_OF_HEAP)
		search_base = find_next_used(search_base);

	if(search_base.section_ptr != END_OF_HEAP)
	{
		retval.file_id 	= USEDCAST(search_base.section_ptr)->id_file;
		retval.line_id 	= USEDCAST(search_base.section_ptr)->id_line;
		retval.size 	= USEDCAST(search_base.section_ptr)->size;
		retval.content 	= USEDCAST(search_base.section_ptr)->content;
	};
	APPIF_EXIT();
	return retval;
}

struct heap_leakid_struct heap_find_leak(void)
{
	struct heap_leakid_struct record = {.file_id = "", .line_id = 0, .cnt = 0};
	struct search_point_struct search_base;
	struct search_point_struct search_id;

	const char* fid;
	uint16_t lid;
	uint32_t cnt;
	bool found_next_id = true;

	APPIF_ENTER();
	if(!initialized)
		initialize();

	search_base.section_ptr = heap_space;
	search_base.next_free_ptr = first_free;

//	if(heap_space == first_free)  todo: should thi next line be conditional?? what if the first section is used? it should be counted
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

	APPIF_EXIT();
	return record;
}
#endif


#ifdef MCHEAP_PROVIDE_PRNF

#ifndef MCHEAP_PRNF_GROW_STEP
	#define MCHEAP_PRNF_GROW_STEP 30
#endif

struct dynbuf_struct
{
	size_t size;
	size_t pos;
	char* buf;
};

static void append_char(void* buf, char x);
static void append_char(void* buf, char x)
{
	struct dynbuf_struct* dynbuf = (struct dynbuf_struct*)buf;
	if(dynbuf->pos == dynbuf->size-1)
	{
		dynbuf->buf = reallocate(dynbuf->buf, dynbuf->size+MCHEAP_PRNF_GROW_STEP);
		dynbuf->size += MCHEAP_PRNF_GROW_STEP;
	};
	dynbuf->buf[dynbuf->pos++] = x;
	dynbuf->buf[dynbuf->pos] = 0;
}

#ifdef MCHEAP_ID_SECTIONS
char* heap_prnf_id(const char* id_file, uint16_t id_line, const char* fmt, ...)
#else
char* heap_prnf(const char* fmt, ...)
#endif
{
	char* retval;
	va_list va;
	va_start(va, fmt);

	#ifdef MCHEAP_ID_SECTIONS
	retval = heap_vprnf_id(id_file, id_line, fmt, va);
	#else
	retval = heap_vprnf(fmt, va);
	#endif

	va_end(va);
	return retval;
}

#ifdef MCHEAP_ID_SECTIONS
char* heap_vprnf_id(const char* id_file, uint16_t id_line, const char* fmt, va_list va)
#else
char* heap_vprnf(const char* fmt, va_list va)
#endif
{
	struct dynbuf_struct dynbuf;

	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	dynbuf.size = strlen(fmt)+MCHEAP_PRNF_GROW_STEP;
	dynbuf.pos = 0;
	dynbuf.buf = allocate(dynbuf.size);

	vfptrprnf(append_char, &dynbuf, fmt, va);
	append_char(&dynbuf, 0);	//terminate
	dynbuf.buf = reallocate(dynbuf.buf, dynbuf.pos+1);

	APPIF_EXIT();
	return dynbuf.buf;
};

#ifdef PLATFORM_AVR
#ifdef MCHEAP_ID_SECTIONS
char* heap_prnf_P_id(const char* id_file, uint16_t id_line, PGM_P fmt, ...)
#else
char* heap_prnf_P(PGM_P fmt, ...)
#endif
{
	char* retval;
	va_list va;
	va_start(va, fmt);

	#ifdef MCHEAP_ID_SECTIONS
	retval = heap_vprnf_P_id(id_file, id_line, fmt, va);
	#else
	retval = heap_vprnf_P(fmt, va);
	#endif
	
	va_end(va);
	return retval;
};

#ifdef MCHEAP_ID_SECTIONS
char* heap_vprnf_P_id(const char* id_file, uint16_t id_line, PGM_P fmt, va_list va)
#else
char* heap_vprnf_P(PGM_P fmt, va_list va)
#endif
{
	struct dynbuf_struct dynbuf;

	APPIF_ENTER();
	#ifdef MCHEAP_ID_SECTIONS
		heap_id_file = id_file;
		heap_id_line = id_line;
	#endif

	dynbuf.size = strlen_P(fmt)+MCHEAP_PRNF_GROW_STEP;
	dynbuf.pos = 0;
	dynbuf.buf = allocate(dynbuf.size);

	vfptrprnf_P(append_char, &dynbuf, fmt, va);
	append_char(&dynbuf, 0);	//terminate
	dynbuf.buf = reallocate(dynbuf.buf, dynbuf.pos+1);
	APPIF_EXIT();
	return dynbuf.buf;
}
#endif

#endif	//PLATFORM_AVR

//********************************************************************************************************
// Private functions
//********************************************************************************************************

static void initialize(void)
{
	#ifdef MCHEAP_RUNTIME_ADDRESS
	if(heap_space == NULL)
		ERROR_NO_INIT();
	#endif

	initialized=true;
	first_free = (void*)heap_space;		//init head of the free list

//	initialize free space
	first_free->size 	= MCHEAP_SIZE - sizeof(struct free_struct);
	#ifdef MCHEAP_USE_KEYS
		first_free->key 	= first_free->size ^ KEY_FREE;
	#endif

	first_free->next_ptr 	= NULL;
}

static void* allocate(size_t size)
{
	struct free_struct *free_ptr;
	struct used_struct *used_ptr;
	void* retval=NULL;

	if(!initialized)
		initialize();

//	align size
	if(size & (MCHEAP_ALIGNMENT-1))
		size += MCHEAP_ALIGNMENT;
	size &= ~(size_t)(MCHEAP_ALIGNMENT-1);

//	allocation must be large enough to return to the free list
	if(sizeof(struct used_struct) + size < sizeof(struct free_struct))
		size = sizeof(struct free_struct) - sizeof(struct used_struct);

	#ifdef MCHEAP_TEST
		heap_test(NULL);
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

	#ifdef MCHEAP_TRACK_STATS
		free_find_largest();
	#endif

	return retval;
}

static void* reallocate(void* section, size_t new_size)
{
	struct free_struct* free_ptr;
	struct free_struct* dest_ptr=NULL;
	struct used_struct* used_ptr;
	struct used_struct* new_used_ptr=NULL;
	void* retval = NULL;

	if(!initialized)
		initialize();

	if(section == NULL)
	{
		retval = allocate(new_size);					//if section == NULL just call allocate()
	}
	else if(new_size == 0)
	{
		retval = internal_free(section);
	}
	else
	{
		if(!heap_contains(section))
			ERROR_REALLOC_EXTERNAL();

		// align size
		if(new_size & (MCHEAP_ALIGNMENT-1))
			new_size += MCHEAP_ALIGNMENT;
		new_size &= ~(size_t)(MCHEAP_ALIGNMENT-1);

		// allocation must be large enough to return to the free list
		if(sizeof(struct used_struct) + new_size < sizeof(struct free_struct))
			new_size = sizeof(struct free_struct) - sizeof(struct used_struct);

		used_ptr = container_of(section, struct used_struct, content);

		#ifdef MCHEAP_TEST
		// fail if this is not a used section
		if(!heap_test(used_ptr))
			ERROR_FALSE_REALLOC();
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

		#ifdef MCHEAP_TRACK_STATS
			free_find_largest();
		#endif
	};
	return retval;
}

static void* internal_free(void* section)
{
	struct used_struct *used_ptr;
	struct free_struct *free_ptr;

	if(!initialized)
		initialize();

	if(section==NULL)
	{
		#ifdef MCHEAP_TEST
			heap_test(NULL);
		#endif
	}
	else
	{
		// not null, is it in the heap?
		if(!heap_contains(section))
			ERROR_FREE_EXTERNAL();
		else
		{
			//in the heap
			used_ptr = container_of(section, struct used_struct, content);
			#ifdef MCHEAP_TEST
			if(!heap_test(used_ptr))
				ERROR_FALSE_FREE();
			#endif
			
			free_ptr = used_to_free(used_ptr);	//convert to free section
			free_insert(free_ptr);				//insert into the free list
			free_merge(free_ptr);				//merge with adjacent free sections

			#ifdef MCHEAP_TRACK_STATS
				free_find_largest();
			#endif
			heap_allocations--;
		};
	};
	return NULL;
}

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
			#ifdef MCHEAP_USE_KEYS
				free_ptr->key = free_ptr->size ^ KEY_FREE;
			#endif
			#ifdef MCHEAP_ID_SECTIONS
				free_ptr->id_file = __FILE__;	//(internal ID)
				free_ptr->id_line = __LINE__;
			#endif

			//shrink used section
			used_ptr->size = new_size;

			//correct used sections key
			#ifdef MCHEAP_USE_KEYS
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
	#ifdef MCHEAP_USE_KEYS
		free_ptr->key = KEY_FREE ^ free_ptr->size;
	#endif
	#ifdef MCHEAP_ID_SECTIONS
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
	#ifdef MCHEAP_USE_KEYS
		used_ptr->key = KEY_USED ^ used_ptr->size;
	#endif
	#ifdef MCHEAP_ID_SECTIONS
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
	#ifdef MCHEAP_USE_KEYS
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

	#ifdef MCHEAP_USE_KEYS
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
			#ifdef MCHEAP_USE_KEYS
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
#ifdef MCHEAP_TRACK_STATS
static void free_find_largest(void)
{
	struct free_struct *free_ptr;
	size_t largest=0;

	heap_largest_free = 0;
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
			heap_largest_free = largest - sizeof(struct used_struct);
	};

	if(heap_largest_free < heap_head_room)
		heap_head_room = heap_largest_free;
}
#endif

//
// Heap test, may be used before freeing memory, to see if the heap is intact,
// and also that the section about to be freed is actually a used section
//
#ifdef MCHEAP_TEST
	static bool heap_test(struct used_struct *used_ptr)	
	#ifdef MCHEAP_USE_KEYS
{
	struct free_struct *next_free_ptr;
	void* section_ptr;
	bool used_found=false;

	next_free_ptr = first_free;
	section_ptr = heap_space;

	while(section_ptr != &heap_space[MCHEAP_SIZE])
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
	return !(used_ptr && !used_found);
}
	#else
{
	struct free_struct *next_free_ptr;
	void* section_ptr;
	bool used_found=false;

	next_free_ptr = first_free;
	section_ptr = heap_space;

	while(section_ptr != &heap_space[MCHEAP_SIZE])
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

		if((uint8_t*)section_ptr < heap_space || (uint8_t*)section_ptr > &heap_space[MCHEAP_SIZE])
			ERROR_BROKEN();
	};
	return !(used_ptr && !used_found);
}
	#endif
#endif
